// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/inspector/inspector_audits_agent.h"

#include "base/numerics/safe_conversions.h"
#include "third_party/blink/public/mojom/devtools/inspector_issue.mojom-blink.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-blink.h"
#include "third_party/blink/public/platform/web_data.h"
#include "third_party/blink/public/web/web_autofill_client.h"
#include "third_party/blink/public/web/web_image.h"
#include "third_party/blink/renderer/core/dom/dom_node_ids.h"
#include "third_party/blink/renderer/core/dom/dom_token_list.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/html/html_anchor_element.h"
#include "third_party/blink/renderer/core/inspector/identifiers_factory.h"
#include "third_party/blink/renderer/core/inspector/inspector_audits_issue.h"
#include "third_party/blink/renderer/core/inspector/inspector_issue_storage.h"
#include "third_party/blink/renderer/core/inspector/inspector_network_agent.h"
#include "third_party/blink/renderer/core/inspector/protocol/audits.h"
#include "third_party/blink/renderer/platform/graphics/image_data_buffer.h"
#include "third_party/blink/renderer/platform/wtf/text/base64.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/skia/include/core/SkImage.h"
#include "ui/gfx/geometry/size.h"

namespace blink {

using protocol::Maybe;

namespace encoding_enum = protocol::Audits::GetEncodedResponse::EncodingEnum;

namespace {

static constexpr int kMaximumEncodeImageWidthInPixels = 10000;

static constexpr int kMaximumEncodeImageHeightInPixels = 10000;

static constexpr double kDefaultEncodeQuality = 1;

bool EncodeAsImage(char* body,
                   size_t size,
                   const String& encoding,
                   const double quality,
                   Vector<unsigned char>* output) {
  const gfx::Size maximum_size = gfx::Size(kMaximumEncodeImageWidthInPixels,
                                           kMaximumEncodeImageHeightInPixels);
  SkBitmap bitmap = WebImage::FromData(WebData(body, size), maximum_size);
  if (bitmap.isNull())
    return false;

  SkImageInfo info =
      SkImageInfo::Make(bitmap.width(), bitmap.height(), kRGBA_8888_SkColorType,
                        kUnpremul_SkAlphaType);
  uint32_t row_bytes = static_cast<uint32_t>(info.minRowBytes());
  Vector<unsigned char> pixel_storage(
      base::checked_cast<wtf_size_t>(info.computeByteSize(row_bytes)));
  SkPixmap pixmap(info, pixel_storage.data(), row_bytes);
  sk_sp<SkImage> image = SkImages::RasterFromBitmap(bitmap);

  if (!image || !image->readPixels(pixmap, 0, 0))
    return false;

  std::unique_ptr<ImageDataBuffer> image_to_encode =
      ImageDataBuffer::Create(pixmap);
  if (!image_to_encode)
    return false;

  String mime_type_name = StringView("image/") + encoding;
  ImageEncodingMimeType mime_type;
  bool valid_mime_type = ParseImageEncodingMimeType(mime_type_name, mime_type);
  DCHECK(valid_mime_type);
  return image_to_encode->EncodeImage(mime_type, quality, output);
}

std::unique_ptr<protocol::Audits::InspectorIssue> CreateLowTextContrastIssue(
    const ContrastInfo& info) {
  Element* element = info.element;

  StringBuilder sb;
  auto element_id = element->GetIdAttribute().LowerASCII();
  sb.Append(element->nodeName().LowerASCII());
  if (!element_id.empty()) {
    sb.Append("#");
    sb.Append(element_id);
  }
  for (unsigned i = 0; i < element->classList().length(); i++) {
    sb.Append(".");
    sb.Append(element->classList().item(i));
  }

  auto issue_details = protocol::Audits::InspectorIssueDetails::create();
  auto low_contrast_details =
      protocol::Audits::LowTextContrastIssueDetails::create()
          .setThresholdAA(info.threshold_aa)
          .setThresholdAAA(info.threshold_aaa)
          .setFontSize(info.font_size)
          .setFontWeight(info.font_weight)
          .setContrastRatio(info.contrast_ratio)
          .setViolatingNodeSelector(sb.ToString())
          .setViolatingNodeId(element->GetDomNodeId())
          .build();
  issue_details.setLowTextContrastIssueDetails(std::move(low_contrast_details));

  return protocol::Audits::InspectorIssue::create()
      .setCode(protocol::Audits::InspectorIssueCodeEnum::LowTextContrastIssue)
      .setDetails(issue_details.build())
      .build();
}

}  // namespace

void InspectorAuditsAgent::Trace(Visitor* visitor) const {
  visitor->Trace(network_agent_);
  visitor->Trace(inspected_frames_);
  InspectorBaseAgent::Trace(visitor);
}

InspectorAuditsAgent::InspectorAuditsAgent(
    InspectorNetworkAgent* network_agent,
    InspectorIssueStorage* storage,
    InspectedFrames* inspected_frames,
    WebAutofillClient* web_autofill_client)
    : inspector_issue_storage_(storage),
      enabled_(&agent_state_, false),
      network_agent_(network_agent),
      inspected_frames_(inspected_frames),
      web_autofill_client_(web_autofill_client) {
  DCHECK(network_agent);
}

InspectorAuditsAgent::~InspectorAuditsAgent() = default;

protocol::Response InspectorAuditsAgent::getEncodedResponse(
    const String& request_id,
    const String& encoding,
    Maybe<double> quality,
    Maybe<bool> size_only,
    Maybe<protocol::Binary>* out_body,
    int* out_original_size,
    int* out_encoded_size) {
  DCHECK(encoding == encoding_enum::Jpeg || encoding == encoding_enum::Png ||
         encoding == encoding_enum::Webp);

  String body;
  bool is_base64_encoded;
  protocol::Response response =
      network_agent_->GetResponseBody(request_id, &body, &is_base64_encoded);
  if (!response.IsSuccess())
    return response;

  Vector<char> base64_decoded_buffer;
  if (!is_base64_encoded || !Base64Decode(body, base64_decoded_buffer) ||
      base64_decoded_buffer.size() == 0) {
    return protocol::Response::ServerError("Failed to decode original image");
  }

  Vector<unsigned char> encoded_image;
  if (!EncodeAsImage(base64_decoded_buffer.data(), base64_decoded_buffer.size(),
                     encoding, quality.value_or(kDefaultEncodeQuality),
                     &encoded_image)) {
    return protocol::Response::ServerError(
        "Could not encode image with given settings");
  }

  *out_original_size = static_cast<int>(base64_decoded_buffer.size());
  *out_encoded_size = static_cast<int>(encoded_image.size());

  if (!size_only.value_or(false)) {
    *out_body = protocol::Binary::fromVector(std::move(encoded_image));
  }
  return protocol::Response::Success();
}

void InspectorAuditsAgent::CheckContrastForDocument(Document* document,
                                                    bool report_aaa) {
  InspectorContrast contrast(document);
  unsigned max_elements = 100;
  for (ContrastInfo info :
       contrast.GetElementsWithContrastIssues(report_aaa, max_elements)) {
    GetFrontend()->issueAdded(CreateLowTextContrastIssue(info));
  }
  GetFrontend()->flush();
}

protocol::Response InspectorAuditsAgent::checkContrast(
    protocol::Maybe<bool> report_aaa) {
  if (!inspected_frames_) {
    return protocol::Response::ServerError(
        "Inspected frames are not available");
  }

  auto* main_window = inspected_frames_->Root()->DomWindow();
  if (!main_window)
    return protocol::Response::ServerError("Document is not available");

  CheckContrastForDocument(main_window->document(), report_aaa.value_or(false));

  return protocol::Response::Success();
}

protocol::Response InspectorAuditsAgent::enable() {
  if (enabled_.Get()) {
    return protocol::Response::Success();
  }

  enabled_.Set(true);
  InnerEnable();
  return protocol::Response::Success();
}

protocol::Response InspectorAuditsAgent::checkFormsIssues(
    std::unique_ptr<protocol::Array<protocol::Audits::GenericIssueDetails>>*
        out_formIssues) {
  *out_formIssues = std::make_unique<
      protocol::Array<protocol::Audits::GenericIssueDetails>>();
  if (web_autofill_client_) {
    web_autofill_client_->EmitFormIssuesToDevtools();
  }
  return protocol::Response::Success();
}

protocol::Response InspectorAuditsAgent::disable() {
  if (!enabled_.Get()) {
    return protocol::Response::Success();
  }

  enabled_.Clear();
  instrumenting_agents_->RemoveInspectorAuditsAgent(this);
  return protocol::Response::Success();
}

void InspectorAuditsAgent::Restore() {
  if (!enabled_.Get())
    return;
  InnerEnable();
}

void InspectorAuditsAgent::InnerEnable() {
  instrumenting_agents_->AddInspectorAuditsAgent(this);
  for (wtf_size_t i = 0; i < inspector_issue_storage_->size(); ++i)
    InspectorIssueAdded(inspector_issue_storage_->at(i));
}

void InspectorAuditsAgent::InspectorIssueAdded(
    protocol::Audits::InspectorIssue* issue) {
  GetFrontend()->issueAdded(issue->Clone());
  GetFrontend()->flush();
}

}  // namespace blink
