// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/memory/ptr_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "components/manta/anchovy/anchovy_provider.h"
#include "components/manta/manta_status.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "ui/accessibility/accessibility_features.h"
#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <cstring>
#include <optional>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/strings/stringprintf.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "net/base/net_errors.h"
#include "net/http/http_status_code.h"
#include "services/data_decoder/public/cpp/data_decoder.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "services/data_decoder/public/mojom/json_parser.mojom.h"
#include "services/image_annotation/annotator.h"
#include "services/image_annotation/image_annotation_metrics.h"
#include "services/image_annotation/public/mojom/image_annotation.mojom.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/url_loader.mojom-shared.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace image_annotation {

namespace {

using base::Bucket;
using testing::ElementsAre;
using testing::Eq;
using testing::IsEmpty;
using testing::SizeIs;
using testing::UnorderedElementsAre;

MATCHER_P3(AnnotatorEq, type, score, text, "") {
  return (arg.type == type && arg.score == score && arg.text == text);
}

constexpr char kTestServerUrl[] = "https://ia-pa.googleapis.com/v1/annotation";
constexpr char kLangsServerUrl[] = "https://ia-pa.googleapis.com/v1/langs";

// Example image URLs.

constexpr char kImage1Url[] = "https://www.example.com/image1.jpg";

constexpr base::TimeDelta kThrottle = base::Seconds(1);

// The minimum dimension required for description annotation.
constexpr int32_t kDescDim = Annotator::kDescMinDimension;

// The description language to use in tests that don't exercise
// language-handling logic.
constexpr char kDescLang[] = "";

// An image processor that holds and exposes the callbacks it is passed.
class TestImageProcessor : public mojom::ImageProcessor {
 public:
  TestImageProcessor() = default;

  TestImageProcessor(const TestImageProcessor&) = delete;
  TestImageProcessor& operator=(const TestImageProcessor&) = delete;

  mojo::PendingRemote<mojom::ImageProcessor> GetPendingRemote() {
    mojo::PendingRemote<mojom::ImageProcessor> remote;
    receivers_.Add(this, remote.InitWithNewPipeAndPassReceiver());
    return remote;
  }

  void Reset() {
    receivers_.Clear();
    callbacks_.clear();
  }

  void GetJpgImageData(GetJpgImageDataCallback callback) override {
    callbacks_.push_back(std::move(callback));
  }

  std::vector<GetJpgImageDataCallback>& callbacks() { return callbacks_; }

 private:
  std::vector<GetJpgImageDataCallback> callbacks_;

  mojo::ReceiverSet<mojom::ImageProcessor> receivers_;
};

// A class that supports test URL loading for the "server" use case: where
// all request URLs have the same prefix and differ only in suffix and body
// content.
class TestServerURLLoaderFactory {
 public:
  TestServerURLLoaderFactory(const std::string& server_url_prefix)
      : server_url_prefix_(server_url_prefix),
        shared_loader_factory_(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &loader_factory_)) {}

  TestServerURLLoaderFactory(const TestServerURLLoaderFactory&) = delete;
  TestServerURLLoaderFactory& operator=(const TestServerURLLoaderFactory&) =
      delete;

  const std::vector<network::TestURLLoaderFactory::PendingRequest>& requests() {
    return *loader_factory_.pending_requests();
  }

  // Expects that the earliest received request has the given URL, headers and
  // body, and replies with the given response.
  //
  // |expected_headers| is a map from header key string to either:
  //   a) a null optional, if the given header should not be present, or
  //   b) a non-null optional, if the given header should be present and match
  //      the optional value.
  //
  // Consumes the earliest received request (i.e. a subsequent call will apply
  // to the second-earliest received request and so on).
  void ExpectRequestAndSimulateResponse(
      const std::string& expected_url_suffix,
      const std::map<std::string, std::optional<std::string>>& expected_headers,
      const std::string& expected_body,
      const std::string& response,
      const net::HttpStatusCode response_code) {
    const std::string expected_url = server_url_prefix_ + expected_url_suffix;

    const std::vector<network::TestURLLoaderFactory::PendingRequest>&
        pending_requests = *loader_factory_.pending_requests();

    CHECK(!pending_requests.empty());
    const network::ResourceRequest& request = pending_requests.front().request;

    // Assert that the earliest request is for the given URL.
    CHECK_EQ(request.url, GURL(expected_url));

    // Expect that specified headers are accurate.
    for (const auto& kv : expected_headers) {
      EXPECT_THAT(request.headers.GetHeader(kv.first), kv.second);
    }

    // Extract request body.
    std::string actual_body;
    if (request.request_body) {
      const std::vector<network::DataElement>* const elements =
          request.request_body->elements();

      // We only support the simplest body structure.
      if (elements && elements->size() == 1 &&
          (*elements)[0].type() ==
              network::mojom::DataElementDataView::Tag::kBytes) {
        actual_body = std::string(
            (*elements)[0].As<network::DataElementBytes>().AsStringPiece());
      }
    }

    EXPECT_THAT(actual_body, Eq(expected_body));

    // Guaranteed to match the first request based on URL.
    loader_factory_.SimulateResponseForPendingRequest(expected_url, response,
                                                      response_code);
  }

  scoped_refptr<network::SharedURLLoaderFactory> AsSharedURLLoaderFactory() {
    return shared_loader_factory_;
  }

 private:
  const std::string server_url_prefix_;
  network::TestURLLoaderFactory loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> shared_loader_factory_;
};

// Receives the result of an annotation request and writes the result data into
// the given variables.
void ReportResult(std::optional<mojom::AnnotateImageError>* const error,
                  std::vector<mojom::Annotation>* const annotations,
                  mojom::AnnotateImageResultPtr result) {
  if (result->which() == mojom::AnnotateImageResult::Tag::kErrorCode) {
    *error = result->get_error_code();
  } else {
    // If annotations exists, then it is not empty.
    ASSERT_THAT(result->get_annotations(), Not(IsEmpty()));
    for (const auto& annotation_ptr : result->get_annotations()) {
      annotations->push_back(*annotation_ptr);
    }
  }
}

class TestAnnotatorClient : public Annotator::Client {
 public:
  explicit TestAnnotatorClient() = default;
  ~TestAnnotatorClient() override = default;

  // Set up tests.
  void SetAcceptLanguages(const std::vector<std::string> accept_langs) {
    accept_langs_ = accept_langs;
  }
  void SetTopLanguages(const std::vector<std::string> top_langs) {
    top_langs_ = top_langs;
  }

 private:
  // Annotator::Client implementation:
  void BindJsonParser(mojo::PendingReceiver<data_decoder::mojom::JsonParser>
                          receiver) override {
    decoder_.GetService()->BindJsonParser(std::move(receiver));
  }
  std::vector<std::string> GetAcceptLanguages() override {
    return accept_langs_;
  }
  std::vector<std::string> GetTopLanguages() override { return top_langs_; }
  void RecordLanguageMetrics(const std::string& page_language,
                             const std::string& requested_language) override {}

  data_decoder::DataDecoder decoder_;
  std::vector<std::string> accept_langs_ = {"en", "it", "fr"};
  std::vector<std::string> top_langs_;
};

}  // namespace

// Tests that the Annotator computes a reasonable preferred language
// based on the page language, top languages, accept languages, and
// server languages.
TEST(AnnotatorTest, ComputePreferredLanguage) {
  TestAnnotatorClient* annotator_client = new TestAnnotatorClient();
  TestServerURLLoaderFactory test_url_factory(
      "https://ia-pa.googleapis.com/v1/");
  Annotator annotator(
      GURL("https://datascraper.com/annotation"), GURL(""), "my_api_key",
      kThrottle, 1 /* batch_size */, 1.0 /* min_ocr_confidence */,
      test_url_factory.AsSharedURLLoaderFactory(), /*anchovy_provider=*/nullptr,
      base::WrapUnique(annotator_client));

  // Simplest case: the page language is in the list of top languages,
  // accept languages, and server languages.
  annotator.server_languages_ = {"fr", "ja"};
  annotator_client->SetTopLanguages({"fr", "hu"});
  annotator_client->SetAcceptLanguages({"fr", "es"});
  EXPECT_EQ("fr", annotator.ComputePreferredLanguage("fr"));

  // Case and locale are ignored (except for zh, see below).
  annotator.server_languages_ = {"fR-FR", "ja"};
  annotator_client->SetTopLanguages({"Fr-CA", "hu"});
  annotator_client->SetAcceptLanguages({"fr-BE", "es"});
  EXPECT_EQ("fr", annotator.ComputePreferredLanguage("FR-ch"));

  // The page language is respected if it appears in the list of accept
  // languages OR top languages, and it's also a supported server language.
  annotator.server_languages_ = {"fr", "en", "de", "pt", "ja"};
  annotator_client->SetTopLanguages({"fr", "de"});
  annotator_client->SetAcceptLanguages({"en", "pt"});
  EXPECT_EQ("pt", annotator.ComputePreferredLanguage("pt"));
  EXPECT_EQ("de", annotator.ComputePreferredLanguage("de"));
  EXPECT_EQ("en", annotator.ComputePreferredLanguage("en"));
  EXPECT_EQ("fr", annotator.ComputePreferredLanguage("fr"));

  // If the page language is not in the list of accept languages or top
  // languages, the first choice should be an accept language that's
  // also a top language and server language.
  annotator.server_languages_ = {"en", "es"};
  annotator_client->SetTopLanguages({"es"});
  annotator_client->SetAcceptLanguages({"en", "es"});
  EXPECT_EQ("es", annotator.ComputePreferredLanguage("hu"));

  // If the page language is not in the list of accept languages or top
  // languages, and no accept languages are top languages, return the
  // first accept language that's a server language.
  annotator.server_languages_ = {"en", "es"};
  annotator_client->SetTopLanguages({});
  annotator_client->SetAcceptLanguages({"en", "es"});
  EXPECT_EQ("en", annotator.ComputePreferredLanguage("ja"));

  // If the page language is not in the list of accept languages and none
  // of the accept languages are server languages either, return the first
  // top language that's a server language.
  annotator.server_languages_ = {"en", "de", "pt"};
  annotator_client->SetTopLanguages({"it", "hu", "de", "pt"});
  annotator_client->SetAcceptLanguages({"es"});
  EXPECT_EQ("de", annotator.ComputePreferredLanguage("ja"));

  // If nothing matches, just return the first accept language. The server can
  // still return OCR results, and it can log the request.
  annotator.server_languages_ = {"en", "de", "pt"};
  annotator_client->SetTopLanguages({"it", "hu"});
  annotator_client->SetAcceptLanguages({"zh-TW"});
  EXPECT_EQ("zh-TW", annotator.ComputePreferredLanguage("zh-CN"));
}

TEST(AnnotatorTest, FetchServerLanguages) {
  base::test::TaskEnvironment test_task_env(
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);
  TestServerURLLoaderFactory test_url_factory(
      "https://ia-pa.googleapis.com/v1/");
  data_decoder::test::InProcessDataDecoder in_process_data_decoder;

  Annotator annotator(
      GURL(kTestServerUrl), GURL(kLangsServerUrl), std::string() /* api_key */,
      kThrottle, 1 /* batch_size */, 1.0 /* min_ocr_confidence */,
      test_url_factory.AsSharedURLLoaderFactory(), /*anchovy_provider=*/nullptr,
      std::make_unique<TestAnnotatorClient>());

  // Assert that initially server_languages_ doesn't contain the made-up
  // language code zz.
  EXPECT_FALSE(base::Contains(annotator.server_languages_, "zz"));

  test_url_factory.ExpectRequestAndSimulateResponse(
      "langs", {} /* expected headers */, "" /* body */,
      R"({
           "status": {},
           "langs": [
             "de",
             "en",
             "hu",
             "zz"
           ]
         })",
      net::HTTP_OK);
  test_task_env.RunUntilIdle();

  EXPECT_TRUE(base::Contains(annotator.server_languages_, "zz"));
}

// If the server langs don't contain English, they're ignored.
TEST(AnnotatorTest, ServerLanguagesMustContainEnglish) {
  base::test::TaskEnvironment test_task_env(
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);
  TestServerURLLoaderFactory test_url_factory(
      "https://ia-pa.googleapis.com/v1/");
  data_decoder::test::InProcessDataDecoder in_process_data_decoder;

  Annotator annotator(
      GURL(kTestServerUrl), GURL(kLangsServerUrl), std::string() /* api_key */,
      kThrottle, 1 /* batch_size */, 1.0 /* min_ocr_confidence */,
      test_url_factory.AsSharedURLLoaderFactory(), /*anchovy_provider=*/nullptr,
      std::make_unique<TestAnnotatorClient>());

  // Assert that initially server_languages_ does contain "en" but
  // doesn't contain the made-up language code zz.
  EXPECT_FALSE(base::Contains(annotator.server_languages_, "zz"));

  // The server response doesn't include "en", so we should ignore it.
  test_url_factory.ExpectRequestAndSimulateResponse(
      "langs", {} /* expected headers */, "" /* body */,
      R"({
           "status": {},
           "langs": [
             "de",
             "zz"
           ]
         })",
      net::HTTP_OK);
  test_task_env.RunUntilIdle();

  // We shouldn't have updated our languages because the response didn't
  // include "en".
  EXPECT_TRUE(base::Contains(annotator.server_languages_, "en"));
  EXPECT_FALSE(base::Contains(annotator.server_languages_, "zz"));
}

// Alternative Routing Tests.

class FakeAnchovyProvider : public manta::AnchovyProvider {
 public:
  explicit FakeAnchovyProvider(base::Value::Dict fake_result)
      : manta::AnchovyProvider(nullptr, nullptr, {}),
        fake_result_(std::move(fake_result)) {}

  FakeAnchovyProvider(const FakeAnchovyProvider&) = delete;
  FakeAnchovyProvider& operator=(const FakeAnchovyProvider&) = delete;

  void GetImageDescription(manta::anchovy::ImageDescriptionRequest& request,
                           net::NetworkTrafficAnnotationTag traffic_annotation,
                           manta::MantaGenericCallback callback) override {
    manta::MantaStatus status;
    status.status_code = manta::MantaStatusCode::kOk;
    status.locale = "en";
    status.message = "ok";
    std::move(callback).Run(std::move(fake_result_), status);
  }

 private:
  base::Value::Dict fake_result_;
};

void RunAnchovyAnnotatorTest(
    std::unique_ptr<manta::AnchovyProvider> fake_provider,
    std::vector<mojom::Annotation>* annotations) {
  base::test::TaskEnvironment test_task_env(
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);
  TestServerURLLoaderFactory test_url_factory(
      "https://ia-pa.googleapis.com/v1/");

  Annotator annotator(
      GURL(kTestServerUrl), GURL(""), std::string() /* api_key */, kThrottle,
      1 /* batch_size */, 1.0 /* min_ocr_confidence */,
      test_url_factory.AsSharedURLLoaderFactory(), std::move(fake_provider),
      std::make_unique<TestAnnotatorClient>());
  TestImageProcessor processor;

  std::optional<mojom::AnnotateImageError> error;

  annotator.AnnotateImage(kImage1Url, kDescLang, processor.GetPendingRemote(),
                          base::BindOnce(&ReportResult, &error, annotations));
  test_task_env.RunUntilIdle();

  // Annotator should have asked processor for pixels.
  ASSERT_THAT(processor.callbacks(), SizeIs(1));

  // Send back image data.
  std::move(processor.callbacks()[0]).Run({1, 2, 3}, kDescDim, kDescDim);
  processor.callbacks().pop_back();
  test_task_env.RunUntilIdle();

  // No request should be sent yet (because service is waiting to batch up
  // multiple requests).
  EXPECT_THAT(test_url_factory.requests(), IsEmpty());
  test_task_env.FastForwardBy(base::Seconds(1));
  test_task_env.RunUntilIdle();
}

void SimpleAnchovySuccessTest(std::string str_type,
                              mojom::AnnotationType expected_type) {
  const std::string best_text = "best";
  const std::string type = str_type;
  const double best_score = 0.9;
  const std::string other_text = "other";
  const double other_score = 0.8;

  base::Value::List results;
  results.Append(base::Value::Dict()
                     .Set("type", type)
                     .Set("score", best_score)
                     .Set("text", best_text));
  results.Append(base::Value::Dict()
                     .Set("type", type)
                     .Set("score", other_score)
                     .Set("text", other_text));

  std::vector<mojom::Annotation> annotations;
  RunAnchovyAnnotatorTest(
      std::make_unique<FakeAnchovyProvider>(
          base::Value::Dict().Set("results", std::move(results))),
      &annotations);

  EXPECT_FALSE(annotations.empty());
  EXPECT_EQ(1, (int)annotations.size());
  auto annotation = annotations[0];
  EXPECT_EQ(annotation.text, best_text);
  EXPECT_EQ(annotation.score, best_score);
  EXPECT_EQ(annotation.type, expected_type);
}

TEST(AnnotatorTest, EmptyResultIfDictIsEmpty) {
  std::vector<mojom::Annotation> annotations;
  RunAnchovyAnnotatorTest(
      std::make_unique<FakeAnchovyProvider>(base::Value::Dict()), &annotations);
  EXPECT_TRUE(annotations.empty());
}

TEST(AnnotatorTest, EmptyResultIfListIsEmpty) {
  std::vector<mojom::Annotation> annotations;
  RunAnchovyAnnotatorTest(
      std::make_unique<FakeAnchovyProvider>(
          base::Value::Dict().Set("results", base::Value::List())),
      &annotations);
  EXPECT_TRUE(annotations.empty());
}

TEST(AnnotatorTest, AnchovySuccessMultiple) {
  const std::string text_ocr = "ocr";
  const std::string type_ocr = "OCR";
  const double score = 0.9;
  const std::string text_caption = "caption";
  const std::string type_caption = "CAPTION";

  base::Value::List results;
  results.Append(base::Value::Dict()
                     .Set("type", type_ocr)
                     .Set("score", score)
                     .Set("text", text_ocr));
  results.Append(base::Value::Dict()
                     .Set("type", type_caption)
                     .Set("score", score)
                     .Set("text", text_caption));

  std::vector<mojom::Annotation> annotations;
  RunAnchovyAnnotatorTest(
      std::make_unique<FakeAnchovyProvider>(
          base::Value::Dict().Set("results", std::move(results))),
      &annotations);

  EXPECT_FALSE(annotations.empty());
  EXPECT_EQ(2, (int)annotations.size());
  auto annotation_caption = annotations[0];
  EXPECT_EQ(annotation_caption.text, text_caption);
  EXPECT_EQ(annotation_caption.score, score);
  EXPECT_EQ(annotation_caption.type, mojom::AnnotationType::kCaption);
  auto annotation_ocr = annotations[1];
  EXPECT_EQ(annotation_ocr.text, text_ocr);
  EXPECT_EQ(annotation_ocr.score, score);
  EXPECT_EQ(annotation_ocr.type, mojom::AnnotationType::kOcr);
}

TEST(AnnotatorTest, AnchovySuccessOCR) {
  SimpleAnchovySuccessTest("OCR", mojom::AnnotationType::kOcr);
}

TEST(AnnotatorTest, AnchovySuccessCaption) {
  SimpleAnchovySuccessTest("CAPTION", mojom::AnnotationType::kCaption);
}

TEST(AnnotatorTest, AnchovySuccessLabel) {
  SimpleAnchovySuccessTest("LABEL", mojom::AnnotationType::kLabel);
}

TEST(AnnotatorTest, CrashIfNoText) {
  base::Value::List results;
  results.Append(base::Value::Dict().Set("type", "OCR").Set("score", 12));

  std::unique_ptr<manta::AnchovyProvider> fake_provider_ptr =
      std::make_unique<FakeAnchovyProvider>(
          base::Value::Dict().Set("results", std::move(results)));
  EXPECT_DEATH_IF_SUPPORTED(
      RunAnchovyAnnotatorTest(std::move(fake_provider_ptr), {}), "");
}

TEST(AnnotatorTest, CrashIfNoAnchovyProvider) {
  EXPECT_DEATH_IF_SUPPORTED(RunAnchovyAnnotatorTest(nullptr, {}), "");
}

}  // namespace image_annotation
