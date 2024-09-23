// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/image_annotation/annotator.h"

#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_split.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/google/core/common/google_util.h"
#include "components/manta/anchovy/anchovy_requests.h"
#include "components/manta/manta_status.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/image_annotation/image_annotation_metrics.h"
#include "services/image_annotation/public/mojom/image_annotation.mojom-forward.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "ui/accessibility/accessibility_features.h"
#include "url/gurl.h"

namespace image_annotation {

namespace {

constexpr size_t kImageAnnotationMaxResponseSize = 1024 * 1024;  // 1MB.
constexpr size_t kServerLangsMaxResponseSize = 1024;             // 1KB.

const std::map<std::string, mojom::AnnotationType>& annotation_types() {
  static const base::NoDestructor<std::map<std::string, mojom::AnnotationType>>
      kAnnotationTypes({{"OCR", mojom::AnnotationType::kOcr},
                        {"CAPTION", mojom::AnnotationType::kCaption},
                        {"LABEL", mojom::AnnotationType::kLabel}});

  return *kAnnotationTypes;
}

net::NetworkTrafficAnnotationTag GetTrafficAnnotation() {
  return net::DefineNetworkTrafficAnnotation("image_annotation", R"(
        semantics {
          sender: "Get Image Descriptions from Google"
          description:
            "Chrome can provide image labels (which include detected objects, "
            "extracted text and generated captions) to screen readers (for "
            "visually-impaired users) by sending images to Google's servers. "
            "If image labeling is enabled for a page, Chrome will send the "
            "URLs and pixels of all images on the page to Google's servers, "
            "which will return labels for content identified inside the "
            "images. This content is made accessible to screen reading "
            "software. Chrome fetches the list of supported languages from "
            "the servers and uses that to determine what language to request "
            "descriptions in."
          trigger: "A page containing images is loaded for a user who has "
                   "automatic image labeling enabled. At most once per day, "
                   "Chrome fetches the list of supported languages as a "
                   "separate network request."
          data: "Image pixels and URLs. No user identifier is sent along with "
                "the data."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: NO
          setting:
            "You can enable or disable this feature via the context menu "
            "for images, or via 'Get Image Descriptions' in Chrome's "
            "settings under Accessibility. This feature is disabled by default."
          chrome_policy {
            AccessibilityImageLabelsEnabled {
              AccessibilityImageLabelsEnabled: false
            }
          }
        })");
}

// For a given source ID and requested description language, returns the unique
// image ID string that can be used to look up results from a server response.
std::string MakeImageId(const std::string& source_id,
                        const std::string& desc_lang_tag) {
  return source_id + (desc_lang_tag.empty() ? "" : " " + desc_lang_tag);
}

std::string NormalizeLanguageCode(std::string language) {
  // Remove anything after a comma, in case we got more than one language
  // like "de,de-DE".
  language = language.substr(0, language.find(','));

  // Split based on underscore or dash so that we catch both
  // "zh_CN" and "zh-CN".
  const std::vector<std::string> tokens = base::SplitString(
      language, "-_", base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY);

  if (tokens.size() == 0) {
    return "";
  }

  // Normalize the language portion to lowercase.
  const std::string language_only = base::ToLowerASCII(tokens[0]);

  // For every language other than "zh" (Chinese), return only the language
  // and strip the locale. Image descriptions don't changed based on locale,
  // but zh-CN and zh-TW use different character sets.
  if (tokens.size() == 1 || language_only != "zh") {
    // For the case of Hebrew, Chrome uses "he" but vss uses "iw" internally.
    // For Norwegian, vss uses "no" for all variants.
    if (language_only == "he") {
      return "iw";
    } else if (language_only == "nb" || language_only == "nn") {
      return "no";
    }
    return language_only;
  }

  // Normalize the locale to uppercase.
  std::string locale_only = base::ToUpperASCII(tokens[1]);

  // Map several Chinese locales to the two most common ones used for
  // Simplified and Traditional.
  if (locale_only == "CN" || locale_only == "HANS" || locale_only == "SG") {
    return "zh-CN";
  } else if (locale_only == "TW" || locale_only == "HANT" ||
             locale_only == "MO" || locale_only == "HK") {
    return "zh-TW";
  }

  return "zh";
}

// The server returns separate OCR results for each region of the image; we
// naively concatenate these into one response string.
//
// Returns a null pointer if there is any unexpected structure to the
// annotations message.
mojom::AnnotationPtr ParseJsonOcrAnnotation(const base::Value& ocr_engine,
                                            const double min_ocr_confidence) {
  const base::Value::Dict* const ocr_engine_dict = ocr_engine.GetIfDict();
  if (!ocr_engine_dict) {
    return mojom::AnnotationPtr(nullptr);
  }

  // No OCR regions is valid - it just means there is no text.
  const base::Value* const ocr_regions = ocr_engine_dict->Find("ocrRegions");
  if (!ocr_regions) {
    ReportOcrAnnotation(1.0 /* confidence */, true /* empty */);
    return mojom::Annotation::New(mojom::AnnotationType::kOcr, 1.0 /* score */,
                                  std::string() /* text */);
  }

  if (!ocr_regions->is_list()) {
    return mojom::AnnotationPtr(nullptr);
  }

  std::string all_ocr_text;
  int word_count = 0;
  double word_confidence_sum = 0.0;
  for (const base::Value& ocr_region : ocr_regions->GetList()) {
    if (!ocr_region.is_dict()) {
      continue;
    }

    const base::Value::List* const words =
        ocr_region.GetDict().FindList("words");
    if (!words) {
      continue;
    }

    std::string region_ocr_text;
    for (const base::Value& word : *words) {
      const base::Value::Dict* word_dict = word.GetIfDict();
      if (!word_dict) {
        continue;
      }

      const std::string* const detected_text =
          word_dict->FindString("detectedText");
      if (!detected_text) {
        continue;
      }

      // A confidence value of 0 or 1 is interpreted as an int and not a double.
      std::optional<double> confidence =
          word_dict->FindDouble("confidenceScore");
      if (!confidence.has_value() || *confidence < 0.0 || *confidence > 1.0) {
        continue;
      }

      if (*confidence < min_ocr_confidence) {
        continue;
      }

      if (detected_text->empty()) {
        continue;
      }

      if (!region_ocr_text.empty()) {
        region_ocr_text += " ";
      }

      region_ocr_text += *detected_text;
      ++word_count;
      word_confidence_sum += *confidence;
    }

    if (!all_ocr_text.empty() && !region_ocr_text.empty()) {
      all_ocr_text += "\n";
    }
    all_ocr_text += region_ocr_text;
  }

  const double all_ocr_confidence =
      word_count == 0 ? 1.0 : word_confidence_sum / word_count;
  ReportOcrAnnotation(all_ocr_confidence, all_ocr_text.empty());
  return mojom::Annotation::New(mojom::AnnotationType::kOcr, all_ocr_confidence,
                                all_ocr_text);
}

// Extracts annotations from the given description engine result into the second
// element of the return tuple.
//
// The first element of the return tuple will be true if the image was
// classified as containing adult content.
std::tuple<bool, std::vector<mojom::AnnotationPtr>> ParseJsonDescAnnotations(
    const base::Value& desc_engine) {
  bool adult = false;
  std::vector<mojom::AnnotationPtr> results;

  const base::Value::Dict* desc_engine_dict = desc_engine.GetIfDict();
  if (!desc_engine_dict) {
    return {adult, std::move(results)};
  }

  // If there is a failure reason, log it and track whether it is due to adult
  // content.
  const std::string* const failure_reason_value =
      desc_engine_dict->FindString("failureReason");
  if (failure_reason_value) {
    const DescFailureReason failure_reason =
        ParseDescFailureReason(*failure_reason_value);
    ReportDescFailure(failure_reason);
    adult = failure_reason == DescFailureReason::kAdult;
  }

  const base::Value::Dict* const desc_list_dict =
      desc_engine_dict->FindDict("descriptionList");
  if (!desc_list_dict) {
    return {adult, std::move(results)};
  }

  const base::Value::List* const desc_list =
      desc_list_dict->FindList("descriptions");
  if (!desc_list) {
    return {adult, std::move(results)};
  }

  for (const base::Value& desc : *desc_list) {
    const base::Value::Dict* const desc_dict = desc.GetIfDict();
    if (!desc_dict) {
      continue;
    }

    const std::string* const type = desc_dict->FindString("type");
    if (!type) {
      continue;
    }

    const auto type_lookup = annotation_types().find(*type);
    if (type_lookup == annotation_types().end()) {
      continue;
    }

    const std::optional<double> score = desc_dict->FindDouble("score");
    if (!score.has_value()) {
      continue;
    }

    const std::string* const text = desc_dict->FindString("text");
    if (!text) {
      continue;
    }

    ReportDescAnnotation(type_lookup->second, *score, text->empty());

    // For OCR, we allow empty text and unusual scores; at the time of writing,
    // a score of -1 is always returned for OCR.
    //
    // For other annotation types, we do not allow these cases.
    if (type_lookup->second != mojom::AnnotationType::kOcr &&
        (text->empty() || *score < 0.0 || *score > 1.0)) {
      continue;
    }

    results.push_back(
        mojom::Annotation::New(type_lookup->second, *score, *text));
  }

  return {adult, std::move(results)};
}

// Extracts annotations from the given icon engine result.
mojom::AnnotationPtr ParseJsonIconAnnotations(const base::Value& icon_engine) {
  mojom::AnnotationPtr result;
  const base::Value::Dict* icon_engine_dict = icon_engine.GetIfDict();
  if (!icon_engine_dict) {
    return {};
  }

  const base::Value::List* const icon_list = icon_engine_dict->FindList("icon");
  if (!icon_list) {
    return {};
  }

  for (const base::Value& icon : *icon_list) {
    const base::Value::Dict* icon_dict = icon.GetIfDict();
    if (!icon_dict) {
      continue;
    }

    const std::string* const icon_type = icon_dict->FindString("iconType");
    if (!icon_type) {
      continue;
    }

    std::string icon_type_value = *icon_type;

    const std::optional<double> score = icon_dict->FindDouble("score");
    if (!score.has_value()) {
      continue;
    }

    // Only return the first matching icon.
    auto type = mojom::AnnotationType::kIcon;
    return mojom::Annotation::New(type, *score, icon_type_value);
  }

  return {};
}

// Returns the integer status code for this engine, or -1 if no status can be
// extracted.
int ExtractStatusCode(const base::Value::Dict* const status_dict) {
  if (!status_dict) {
    return -1;
  }

  const base::Value* const code_value = status_dict->Find("code");

  // A missing code is the same as a default (i.e. OK) code.
  if (!code_value) {
    return 0;
  }

  if (!code_value->is_int()) {
    return -1;
  }
  const int code = code_value->GetInt();

#ifndef NDEBUG
  // Also log error status messages (which are helpful for debugging).
  const std::string* const message = status_dict->FindString("message");
  if (code != 0 && message) {
    DVLOG(1) << "Engine failed with status " << code << " and message '"
             << *message << "'";
  }
#endif

  return code;
}

// Attempts to extract annotation results from the server response, returning a
// map from each image ID to its annotations (if successfully extracted).
std::map<std::string, mojom::AnnotateImageResultPtr> UnpackJsonResponse(
    const base::Value& json_data,
    const double min_ocr_confidence) {
  const base::Value::Dict* json_dict = json_data.GetIfDict();
  if (!json_dict) {
    return {};
  }

  const base::Value::List* const results = json_dict->FindList("results");
  if (!results) {
    return {};
  }

  std::map<std::string, mojom::AnnotateImageResultPtr> out;
  for (const base::Value& result : *results) {
    const base::Value::Dict* result_dict = result.GetIfDict();
    if (!result_dict) {
      continue;
    }

    const std::string* const image_id = result_dict->FindString("imageId");
    if (!image_id) {
      continue;
    }

    const base::Value::List* const engine_results =
        result_dict->FindList("engineResults");
    if (!engine_results) {
      continue;
    }

    // We expect the engine result list to have exactly two results: one for OCR
    // and one for image descriptions. However, we "robustly" handle missing
    // engines, unknown engines (by skipping them) and repetitions (by
    // overwriting data).
    bool adult = false;
    std::vector<mojom::AnnotationPtr> annotations;
    mojom::AnnotationPtr ocr_annotation;
    mojom::AnnotationPtr icon_annotation;
    for (const base::Value& engine_result : *engine_results) {
      const base::Value::Dict* engine_result_dict = engine_result.GetIfDict();
      if (!engine_result_dict) {
        continue;
      }

      // A non-zero status code means the following:
      //  -1:                       The status dict could not be parsed. We
      //                            still try to parse an engine result in this
      //                            case to be robust.
      //  any other non-zero value: The status dict was parsed and contains a
      //                            known failure. We always report an error
      //                            in this case.
      const int status_code =
          ExtractStatusCode(engine_result_dict->FindDict("status"));

      const base::Value* const desc_engine =
          engine_result_dict->Find("descriptionEngine");
      const base::Value* const ocr_engine =
          engine_result_dict->Find("ocrEngine");
      const base::Value* const icon_engine =
          engine_result_dict->Find("iconEngine");

      if (desc_engine) {
        // Add description annotations and update the adult image flag.
        ReportDescStatus(status_code);

        if (status_code <= 0) {
          std::tie(adult, annotations) = ParseJsonDescAnnotations(*desc_engine);
        }
      } else if (ocr_engine) {
        // Update the specialized OCR annotations.
        ReportOcrStatus(status_code);

        if (status_code <= 0) {
          ocr_annotation =
              ParseJsonOcrAnnotation(*ocr_engine, min_ocr_confidence);
        }
      } else if (icon_engine) {
        if (status_code <= 0) {
          icon_annotation = ParseJsonIconAnnotations(*icon_engine);
        }
      }

      ReportEngineKnown(ocr_engine || desc_engine || icon_engine);
    }

    // Remove any description OCR data (which is lower quality) if we have
    // specialized OCR results.
    if (!ocr_annotation.is_null()) {
      std::erase_if(annotations, [](const mojom::AnnotationPtr& a) {
        return a->type == mojom::AnnotationType::kOcr;
      });
      annotations.push_back(std::move(ocr_annotation));
    }

    // Remove labels and captions if the image is detected
    // as an icon. Don't remove OCR as any text in the icon might
    // be useful.
    // TODO(accessibility): consider filtering some icon types here e.g.
    // information.
    if (!icon_annotation.is_null()) {
      std::erase_if(annotations, [](const mojom::AnnotationPtr& a) {
        return a->type == mojom::AnnotationType::kLabel ||
               a->type == mojom::AnnotationType::kCaption;
      });
      annotations.push_back(std::move(icon_annotation));
    }

    if (adult) {
      out[*image_id] = mojom::AnnotateImageResult::NewErrorCode(
          mojom::AnnotateImageError::kAdult);
    } else if (!annotations.empty()) {
      out[*image_id] =
          mojom::AnnotateImageResult::NewAnnotations(std::move(annotations));
    }
  }

  return out;
}

mojom::AnnotationPtr CreateAnnotationFromMantaResponse(
    const base::Value::Dict& result_data) {
  auto* text = result_data.FindString("text");
  CHECK(text);

  auto* type = result_data.FindString("type");
  CHECK(type);
  auto score = result_data.FindDouble("score");
  CHECK(score.has_value());

  std::optional<mojom::AnnotationType> annotation_type;

  if (auto itr = annotation_types().find(*type);
      itr != annotation_types().end()) {
    annotation_type = itr->second;
  }

  CHECK(annotation_type.has_value());

  auto annotation =
      mojom::Annotation::New(annotation_type.value(), score.value(), *text);

  return annotation;
}

}  // namespace

constexpr char Annotator::kGoogApiKeyHeader[];

static_assert(Annotator::kDescMinDimension > 0,
              "Description engine must accept images of some sizes.");
static_assert(Annotator::kDescMaxAspectRatio > 0.0,
              "Description engine must accept images of some aspect ratios.");
static_assert(Annotator::kIconMinDimension > 0,
              "Icon engine must accept images of some sizes.");

Annotator::ClientRequestInfo::ClientRequestInfo(
    mojo::PendingRemote<mojom::ImageProcessor> in_image_processor,
    AnnotateImageCallback in_callback)
    : image_processor(std::move(in_image_processor)),
      callback(std::move(in_callback)) {}

Annotator::ClientRequestInfo::~ClientRequestInfo() = default;

Annotator::ServerRequestInfo::ServerRequestInfo(
    const std::string& in_source_id,
    const bool in_desc_requested,
    const bool in_icon_requested,
    const std::string& in_desc_lang_tag,
    const std::vector<uint8_t>& in_image_bytes)
    : source_id(in_source_id),
      desc_requested(in_desc_requested),
      icon_requested(in_icon_requested),
      desc_lang_tag(in_desc_lang_tag),
      image_bytes(in_image_bytes) {}

Annotator::ServerRequestInfo& Annotator::ServerRequestInfo::operator=(
    ServerRequestInfo&& other) = default;

Annotator::ServerRequestInfo::~ServerRequestInfo() = default;

Annotator::Annotator(
    GURL pixels_server_url,
    GURL langs_server_url,
    std::string api_key,
    const base::TimeDelta throttle,
    const int batch_size,
    const double min_ocr_confidence,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    std::unique_ptr<manta::AnchovyProvider> anchovy_provider,
    std::unique_ptr<Client> client)
    : anchovy_provider_(std::move(anchovy_provider)),
      client_(std::move(client)),
      url_loader_factory_(std::move(url_loader_factory)),
      pixels_server_url_(std::move(pixels_server_url)),
      langs_server_url_(std::move(langs_server_url)),
      api_key_(std::move(api_key)),
      batch_size_(batch_size),
      min_ocr_confidence_(min_ocr_confidence),
      server_languages_({"cs", "de", "en", "es", "fi", "fr", "hi", "hr", "id",
                         "it", "iw", "nl", "no", "pt", "ru", "sv", "tr"}) {
  server_request_timer_ = std::make_unique<base::RepeatingTimer>(
      FROM_HERE, throttle,
      base::BindRepeating(&Annotator::SendRequestBatchToServer,
                          weak_factory_.GetWeakPtr()));
  FetchServerLanguages();
}

Annotator::~Annotator() {
  // Report any clients still connected at service shutdown.
  for (const auto& request_info_kv : request_infos_) {
    for ([[maybe_unused]] const auto& unused : request_info_kv.second) {
      ReportClientResult(ClientResult::kShutdown);
    }
  }
}

void Annotator::BindReceiver(mojo::PendingReceiver<mojom::Annotator> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void Annotator::AnnotateImage(
    const std::string& source_id,
    const std::string& page_language,
    mojo::PendingRemote<mojom::ImageProcessor> image_processor,
    AnnotateImageCallback callback) {
  // Compute the desired language for the description result, based on the
  // page language, the accept languages, the top languages, and the
  // server languages.
  const std::string preferred_language =
      ComputePreferredLanguage(page_language);
  client_->RecordLanguageMetrics(page_language, preferred_language);

  const RequestKey request_key(source_id, preferred_language);

  // Return cached results if they exist.
  const auto cache_lookup = cached_results_.find(request_key);
  ReportCacheHit(cache_lookup != cached_results_.end());
  if (cache_lookup != cached_results_.end()) {
    std::move(callback).Run(cache_lookup->second.Clone());
    return;
  }

  // Register the ImageProcessor and callback to be used for this request.
  std::list<ClientRequestInfo>& request_info_list = request_infos_[request_key];
  request_info_list.emplace_back(std::move(image_processor),
                                 std::move(callback));

  // If the image processor dies: automatically delete the request info and
  // reassign local processing (for other interested clients) if the dead image
  // processor was responsible for some ongoing work.
  request_info_list.back().image_processor.set_disconnect_handler(
      base::BindOnce(&Annotator::RemoveRequestInfo, weak_factory_.GetWeakPtr(),
                     request_key, --request_info_list.end(),
                     true /* canceled */));

  // Don't start local work if it would duplicate some already-ongoing work.
  if (base::Contains(local_processors_, request_key) ||
      base::Contains(pending_requests_, request_key)) {
    return;
  }

  local_processors_.insert(
      {request_key, &request_info_list.back().image_processor});

  // TODO(crbug.com/41432508): first query the public result cache by URL to
  // improve latency.

  request_info_list.back().image_processor->GetJpgImageData(base::BindOnce(
      &Annotator::OnJpgImageDataReceived, weak_factory_.GetWeakPtr(),
      request_key, --request_info_list.end()));
}

// static
bool Annotator::IsWithinDescPolicy(const int32_t width, const int32_t height) {
  if (width < kDescMinDimension || height < kDescMinDimension) {
    return false;
  }

  // Can't be 0 or inf because |kDescMinDimension| is guaranteed positive (via a
  // static_assert).
  const double aspect_ratio = static_cast<double>(width) / height;
  if (aspect_ratio < 1.0 / kDescMaxAspectRatio ||
      aspect_ratio > kDescMaxAspectRatio) {
    return false;
  }

  return true;
}

// static
bool Annotator::IsWithinIconPolicy(const int32_t width, const int32_t height) {
  if (width < kIconMinDimension || height < kIconMinDimension ||
      width > kIconMaxDimension || height > kIconMaxDimension) {
    return false;
  }

  // Can't be 0 or inf because |kIconMinDimension| is guaranteed positive (via a
  // static_assert).
  const double aspect_ratio = static_cast<double>(width) / height;
  if (aspect_ratio < 1.0 / kIconMaxAspectRatio ||
      aspect_ratio > kIconMaxAspectRatio) {
    return false;
  }

  return true;
}

// static
std::string Annotator::FormatJsonRequest(
    const std::deque<ServerRequestInfo>::iterator begin,
    const std::deque<ServerRequestInfo>::iterator end) {
  base::Value::List image_request_list;
  for (std::deque<ServerRequestInfo>::iterator it = begin; it != end; ++it) {
    // Re-encode image bytes into base64, which can be represented in JSON.
    std::string base64_data = base::Base64Encode(
        std::string_view(reinterpret_cast<const char*>(it->image_bytes.data()),
                         it->image_bytes.size()));

    // TODO(crbug.com/41432508): accept and propagate page language info to
    //                         improve OCR accuracy.
    base::Value::Dict ocr_engine_params;
    ocr_engine_params.Set("ocrParameters", base::Value::Dict());

    base::Value::List engine_params_list;
    engine_params_list.Append(std::move(ocr_engine_params));

    // Also add a description annotations request if the image is within model
    // policy.
    if (it->desc_requested) {
      base::Value::Dict desc_params;

      // Add preferred description language if it has been specified.
      if (!it->desc_lang_tag.empty()) {
        base::Value::List desc_lang_list;
        desc_lang_list.Append(it->desc_lang_tag);

        desc_params.Set("preferredLanguages", std::move(desc_lang_list));
      }

      base::Value::Dict engine_params;
      engine_params.Set("descriptionParameters", std::move(desc_params));

      engine_params_list.Append(std::move(engine_params));
    }
    ReportImageRequestIncludesDesc(it->desc_requested);

    // Request icon classification.
    // TODO(accessibility): Maybe only do this for certain
    // file sizes?
    if (it->icon_requested) {
      base::Value::Dict icon_params;
      base::Value::Dict engine_params;
      engine_params.Set("iconParameters", std::move(icon_params));
      engine_params_list.Append(std::move(engine_params));
    }
    ReportImageRequestIncludesIcon(it->icon_requested);

    base::Value::Dict image_request;
    image_request.Set("imageId", MakeImageId(it->source_id, it->desc_lang_tag));
    image_request.Set("imageBytes", std::move(base64_data));
    image_request.Set("engineParameters", std::move(engine_params_list));

    image_request_list.Append(std::move(image_request));
  }

  base::Value::Dict request;
  request.Set("imageRequests", std::move(image_request_list));

  std::string json_request;
  base::JSONWriter::Write(request, &json_request);

  ReportServerRequestSizeKB(json_request.size() / 1024);

  return json_request;
}

// static
std::unique_ptr<network::SimpleURLLoader> Annotator::MakeRequestLoader(
    const GURL& server_url,
    const std::string& api_key) {
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->method = "POST";

  resource_request->url = server_url;

  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;

  // Put API key in request's header if a key exists, and the endpoint is
  // trusted by Google.
  if (!api_key.empty() && server_url.SchemeIs(url::kHttpsScheme) &&
      google_util::IsGoogleAssociatedDomainUrl(server_url)) {
    resource_request->headers.SetHeader(kGoogApiKeyHeader, api_key);
  }

  return network::SimpleURLLoader::Create(std::move(resource_request),
                                          GetTrafficAnnotation());
}

void Annotator::OnJpgImageDataReceived(
    const RequestKey& request_key,
    const std::list<ClientRequestInfo>::iterator request_info_it,
    const std::vector<uint8_t>& image_bytes,
    const int32_t width,
    const int32_t height) {
  const std::string& source_id = request_key.first;
  const std::string& request_language = request_key.second;

  ReportPixelFetchSuccess(!image_bytes.empty());

  // Failed to retrieve bytes from local processor; remove dead processor and
  // reschedule processing.
  if (image_bytes.empty()) {
    RemoveRequestInfo(request_key, request_info_it, false /* canceled */);
    return;
  }

  // Local processing is no longer ongoing.
  local_processors_.erase(request_key);

  // Schedule a server request for this image.
  server_request_queue_.emplace_front(
      source_id, IsWithinDescPolicy(width, height),
      IsWithinIconPolicy(width, height), request_language, image_bytes);
  pending_requests_.insert(request_key);

  // Start sending batches to the server.
  if (!server_request_timer_->IsRunning()) {
    server_request_timer_->Reset();
  }
}

void Annotator::SendRequestBatchToServer() {
  if (server_request_queue_.empty()) {
    server_request_timer_->Stop();
    return;
  }

  // Take last n elements (or all elements if there are less than n).
  const auto begin =
      server_request_queue_.end() -
      std::min<size_t>(server_request_queue_.size(), batch_size_);
  const auto end = server_request_queue_.end();

  // Use anchovy if the flag is enabled.

  if (features::IsImageDescriptionsAlternateRoutingEnabled()) {
    CHECK(anchovy_provider_);
    // The set of (source ID, desc lang) pairs relevant for this request.
    std::set<RequestKey> request_keys;
    for (std::deque<ServerRequestInfo>::iterator it = begin; it != end; it++) {
      RequestKey key = {it->source_id, it->desc_lang_tag};
      request_keys.insert(key);
      manta::anchovy::ImageDescriptionRequest request = {
          it->source_id, it->desc_lang_tag, it->image_bytes};
      const auto now = base::Time::Now();
      anchovy_provider_->GetImageDescription(
          request, GetTrafficAnnotation(),
          base::BindOnce(&Annotator::OnMantaResponseReceived,
                         weak_factory_.GetWeakPtr(), key, now));
    }
  } else {
    // The set of (source ID, desc lang) pairs relevant for this request.
    std::set<RequestKey> request_keys;
    for (std::deque<ServerRequestInfo>::iterator it = begin; it != end; it++) {
      request_keys.insert({it->source_id, it->desc_lang_tag});
    }

    // Kick off server communication.
    std::unique_ptr<network::SimpleURLLoader> url_loader =
        MakeRequestLoader(pixels_server_url_, api_key_);
    url_loader->AttachStringForUpload(FormatJsonRequest(begin, end),
                                      "application/json");
    ongoing_server_requests_.push_back(std::move(url_loader));
    ongoing_server_requests_.back()->DownloadToString(
        url_loader_factory_.get(),
        base::BindOnce(&Annotator::OnServerResponseReceived,
                       weak_factory_.GetWeakPtr(), request_keys,
                       --ongoing_server_requests_.end()),
        kImageAnnotationMaxResponseSize);
  }

  server_request_queue_.erase(begin, end);
}

void Annotator::OnMantaResponseReceived(const RequestKey& request_key,
                                        const base::Time request_time,
                                        base::Value::Dict dict,
                                        manta::MantaStatus status) {
  const auto now = base::Time::Now();
  const auto delta = now - request_time;
  ReportServerLatency(delta);

  if (dict.empty()) {
    ProcessResult(request_key, {});
    return;
  }

  auto* results_list = dict.FindList("results");

  if (!results_list || results_list->size() == 0) {
    ProcessResult(request_key, {});
    return;
  }

  if (status.status_code == manta::MantaStatusCode::kOk) {
    // Find the result with the best score.
    base::Value::Dict* best = nullptr;
    std::optional<double> best_score;

    // Store OCR values separately.
    base::Value::Dict* best_ocr = nullptr;
    std::optional<double> best_ocr_score;

    for (auto& result : *results_list) {
      auto* result_dict = result.GetIfDict();
      CHECK(result_dict);
      auto score = result_dict->FindDouble("score");
      CHECK(score.has_value());
      auto* type = result_dict->FindString("type");
      CHECK(type);

      // The better score in image descriptions is the larger number.
      if (*type == "OCR") {
        ReportOcrAnnotation(*score, false);
        if (!best_ocr_score.has_value() || *score > *best_ocr_score) {
          best_ocr_score = score;
          best_ocr = result_dict;
        }
      } else {
        if (auto itr = annotation_types().find(*type);
            itr != annotation_types().end()) {
          ReportDescAnnotation(itr->second, *score, false);
        }
        if (!best_score.has_value() || *score > *best_score) {
          best_score = score;
          best = result_dict;
        }
      }
    }

    std::vector<mojom::AnnotationPtr> annotations;

    if (best) {
      annotations.push_back(CreateAnnotationFromMantaResponse(*best));
    }

    if (best_ocr) {
      annotations.push_back(CreateAnnotationFromMantaResponse(*best_ocr));
    }

    // Add all annotations for the image.
    std::string image_id = MakeImageId(request_key.first, request_key.second);
    std::map<std::string, mojom::AnnotateImageResultPtr> results;
    results[image_id] =
        mojom::AnnotateImageResult::NewAnnotations(std::move(annotations));
    // Process the result from manta.
    ProcessResult(request_key, std::move(results));
  } else {
    LOG(ERROR) << "\nRequest for: " << request_key.first << " Failed: \n"
               << base::ToString(status.status_code);
  }
}

void Annotator::OnServerResponseReceived(
    const std::set<RequestKey>& request_keys,
    const UrlLoaderList::iterator server_request_it,
    const std::unique_ptr<std::string> json_response) {
  ReportServerNetError(server_request_it->get()->NetError());

  if (const network::mojom::URLResponseHead* const response_info =
          server_request_it->get()->ResponseInfo()) {
    ReportServerResponseCode(response_info->headers->response_code());
    ReportServerLatency(response_info->response_time -
                        response_info->request_time);
  }

  ongoing_server_requests_.erase(server_request_it);

  if (!json_response) {
    DVLOG(1) << "HTTP request to image annotation server failed.";
    ProcessResults(request_keys, {});
    return;
  }

  ReportServerResponseSizeBytes(json_response->size());

  // Send JSON string to a dedicated service for safe parsing.
  GetJsonParser()->Parse(
      *json_response, base::JSON_PARSE_RFC,
      base::BindOnce(&Annotator::OnResponseJsonParsed,
                     weak_factory_.GetWeakPtr(), request_keys));
}

void Annotator::OnResponseJsonParsed(const std::set<RequestKey>& request_keys,
                                     const std::optional<base::Value> json_data,
                                     const std::optional<std::string>& error) {
  const bool success = json_data.has_value() && !error.has_value();
  ReportJsonParseSuccess(success);

  // Extract annotation results for each request key with valid results.
  if (success) {
    ProcessResults(request_keys,
                   UnpackJsonResponse(*json_data, min_ocr_confidence_));
  } else {
    DVLOG(1) << "Parsing server response JSON failed with error: "
             << error.value_or("No reason reported.");
    ProcessResults(request_keys, {});
  }
}

void Annotator::ProcessResult(
    const RequestKey& request_key,
    const std::map<std::string, mojom::AnnotateImageResultPtr>& results) {
  pending_requests_.erase(request_key);

  // The lookup will be successful if there is a valid result (i.e. not an
  // error and not a malformed result) for this (source ID, desc lang) pair.
  const auto result_lookup =
      results.find(MakeImageId(request_key.first, request_key.second));

  // Populate the result struct for this image and copy it into the cache if
  // necessary.
  if (result_lookup != results.end()) {
    cached_results_.insert(
        std::make_pair(request_key, result_lookup->second.Clone()));
  }

  // This should not happen, since only this method removes entries of
  // |request_infos_|, and this method should only execute once per request
  // key.
  const auto request_info_it = request_infos_.find(request_key);
  if (request_info_it == request_infos_.end()) {
    LOG(ERROR) << "Could not find request key in request_infos_: "
               << request_key.first << "," << request_key.second;
    return;
  }

  const auto image_result = result_lookup != results.end()
                                ? result_lookup->second.Clone()
                                : mojom::AnnotateImageResult::NewErrorCode(
                                      mojom::AnnotateImageError::kFailure);
  const auto client_result = result_lookup != results.end()
                                 ? ClientResult::kSucceeded
                                 : ClientResult::kFailed;

  // Notify clients of success or failure.
  // TODO(crbug.com/41432508): explore server retry strategies.
  for (auto& info : request_info_it->second) {
    std::move(info.callback).Run(image_result.Clone());
    ReportClientResult(client_result);
  }
  request_infos_.erase(request_info_it);
}

void Annotator::ProcessResults(
    const std::set<RequestKey>& request_keys,
    const std::map<std::string, mojom::AnnotateImageResultPtr>& results) {
  // Process each request key for which we expect to have results.
  for (const RequestKey& request_key : request_keys) {
    ProcessResult(request_key, results);
  }
}

data_decoder::mojom::JsonParser* Annotator::GetJsonParser() {
  if (!json_parser_) {
    client_->BindJsonParser(json_parser_.BindNewPipeAndPassReceiver());
    json_parser_.reset_on_disconnect();
  }

  return json_parser_.get();
}

void Annotator::RemoveRequestInfo(
    const RequestKey& request_key,
    const std::list<ClientRequestInfo>::iterator request_info_it,
    const bool canceled) {
  // Check whether we are deleting the ImageProcessor responsible for current
  // local processing.
  auto local_processor_lookup = local_processors_.find(request_key);
  const bool should_reassign =
      local_processor_lookup != local_processors_.end() &&
      local_processor_lookup->second == &request_info_it->image_processor;

  // Notify client of cancellation / failure.
  ReportClientResult(canceled ? ClientResult::kCanceled
                              : ClientResult::kFailed);
  std::move(request_info_it->callback)
      .Run(mojom::AnnotateImageResult::NewErrorCode(
          canceled ? mojom::AnnotateImageError::kCanceled
                   : mojom::AnnotateImageError::kFailure));

  // Delete the specified ImageProcessor.
  std::list<ClientRequestInfo>& request_info_list = request_infos_[request_key];
  request_info_list.erase(request_info_it);

  // If necessary, reassign local processing.
  if (should_reassign) {
    if (request_info_list.empty()) {
      local_processors_.erase(local_processor_lookup);
    } else {
      local_processor_lookup->second =
          &request_info_list.front().image_processor;

      request_info_list.front().image_processor->GetJpgImageData(base::BindOnce(
          &Annotator::OnJpgImageDataReceived, weak_factory_.GetWeakPtr(),
          request_key, request_info_list.begin()));
    }
  }
}

std::string Annotator::ComputePreferredLanguage(
    const std::string& in_page_language) const {
  DCHECK(!server_languages_.empty());
  if (in_page_language.empty()) {
    return "";
  }

  std::string page_language = NormalizeLanguageCode(in_page_language);
  std::vector<std::string> accept_languages = client_->GetAcceptLanguages();
  base::ranges::transform(accept_languages, accept_languages.begin(),
                          NormalizeLanguageCode);
  std::vector<std::string> top_languages = client_->GetTopLanguages();
  base::ranges::transform(top_languages, top_languages.begin(),
                          NormalizeLanguageCode);

  // If the page language is a server language and it's in the list of accept
  // languages or top languages for this user, return that.
  if (base::Contains(server_languages_, page_language) &&
      (base::Contains(accept_languages, page_language) ||
       base::Contains(top_languages, page_language))) {
    return page_language;
  }

  // Otherwise, ignore the page language and compute the best language
  // for this user. The accept languages are the ones the user can
  // explicitly choose, so pick the first accept language that's a
  // top language and a server language.
  if (!top_languages.empty()) {
    for (const std::string& accept_language : accept_languages) {
      if (base::Contains(server_languages_, accept_language) &&
          base::Contains(top_languages, accept_language)) {
        return accept_language;
      }
    }
  }

  // Sometimes the top languages are empty. Try any accept language that's
  // a server language.
  for (const std::string& accept_language : accept_languages) {
    if (base::Contains(server_languages_, accept_language)) {
      return accept_language;
    }
  }

  // If that still fails, try any top language that's a server language.
  for (const std::string& top_language : top_languages) {
    if (base::Contains(server_languages_, top_language)) {
      return top_language;
    }
  }

  // If all else fails, return the first accept language. The server can
  // still do OCR and it can log this language request.
  if (!accept_languages.empty()) {
    return accept_languages[0];
  }

  // If that fails, return the page language. The server can
  // still do OCR and it can log this language request.
  return page_language;
}

void Annotator::FetchServerLanguages() {
  if (langs_server_url_.is_empty()) {
    return;
  }

  langs_url_loader_ = MakeRequestLoader(langs_server_url_, api_key_);
  langs_url_loader_->AttachStringForUpload("", "application/json");
  langs_url_loader_->DownloadToString(
      url_loader_factory_.get(),
      base::BindOnce(&Annotator::OnServerLangsResponseReceived,
                     weak_factory_.GetWeakPtr()),
      kServerLangsMaxResponseSize);
}

void Annotator::OnServerLangsResponseReceived(
    const std::unique_ptr<std::string> json_response) {
  if (!json_response) {
    DVLOG(1) << "Failed to get languages from the server.";
    return;
  }

  GetJsonParser()->Parse(
      *json_response, base::JSON_PARSE_RFC,
      base::BindOnce(&Annotator::OnServerLangsResponseJsonParsed,
                     weak_factory_.GetWeakPtr()));
}

void Annotator::OnServerLangsResponseJsonParsed(
    std::optional<base::Value> json_data,
    const std::optional<std::string>& error) {
  if (!json_data.has_value() || error.has_value()) {
    DVLOG(1) << "Parsing server langs response JSON failed with error: "
             << error.value_or("No reason reported.");
    return;
  }

  const base::Value::List* const langs = json_data->GetDict().FindList("langs");
  if (!langs) {
    DVLOG(1) << "No langs in response JSON";
    return;
  }

  std::vector<std::string> new_server_languages;
  for (const base::Value& lang : *langs) {
    if (!lang.is_string()) {
      DVLOG(1) << "Lang in response JSON is not a string";
      return;
    }
    new_server_languages.push_back(lang.GetString());
  }

  if (!base::Contains(new_server_languages, "en")) {
    DVLOG(1) << "Server langs don't even include 'en', rejecting";
    return;
  }

  // Only swap in the new languages at the end, if all of the other
  // checks passed.
  server_languages_.swap(new_server_languages);
}

}  // namespace image_annotation
