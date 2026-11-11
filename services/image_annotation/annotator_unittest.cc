// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/image_annotation/annotator.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <memory>
#include <optional>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/manta/anchovy/anchovy_provider.h"
#include "components/manta/anchovy/anchovy_requests.h"
#include "components/manta/manta_status.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/net_errors.h"
#include "net/http/http_status_code.h"
#include "services/image_annotation/image_annotation_metrics.h"
#include "services/image_annotation/public/mojom/image_annotation.mojom.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/url_loader.mojom-shared.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/accessibility_features.h"
#include "url/gurl.h"
#include "url/origin.h"

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
constexpr char kImage2Url[] = "https://www.example.com/image2.jpg";
constexpr char kImage3Url[] = "https://www.example.com/image3.jpg";

// Example server requests / responses.

// Template for a request for a single image.
constexpr char kTemplateRequest[] = R"(
{
  "imageRequests": [{
    "imageId": "0  %s",
    "imageBytes": "%s",
    "engineParameters": [
      {"ocrParameters": {}},
      {"descriptionParameters": {}},
      {"iconParameters": {}}
    ]
  }]
}
)";

// Batch request for |kImage1Url|, |kImage2Url| and |kImage3Url|.
constexpr char kBatchRequest[] = R"(
{
  "imageRequests": [
    {
      "imageId": "0  https://www.example.com/image3.jpg",
      "imageBytes": "BwgJ",
      "engineParameters": [
        {"ocrParameters": {}},
        {"descriptionParameters": {}},
        {"iconParameters": {}}
      ]
    },
    {
      "imageId": "0  https://www.example.com/image2.jpg",
      "imageBytes": "BAUG",
      "engineParameters": [
        {"ocrParameters": {}},
        {"descriptionParameters": {}},
        {"iconParameters": {}}
      ]
    },
    {
      "imageId": "0  https://www.example.com/image1.jpg",
      "imageBytes": "AQID",
      "engineParameters": [
        {"ocrParameters": {}},
        {"descriptionParameters": {}},
        {"iconParameters": {}}
      ]
    }
  ]
})";

// Successful OCR text extraction for |kImage1Url| with no descriptions.
constexpr char kOcrSuccessResponse[] = R"(
{
  "results": [
    {
      "imageId": "0  https://www.example.com/image1.jpg",
      "engineResults": [
        {
          "status": {},
          "ocrEngine": {
            "ocrRegions": [
              {
                "words": [
                  {
                    "detectedText": "Region",
                    "confidenceScore": 1.0
                  },
                  {
                    "detectedText": "1",
                    "confidenceScore": 1.0
                  }
                ]
              },
              {
                "words": [
                  {
                    "detectedText": "Region",
                    "confidenceScore": 1.0
                  },
                  {
                    "detectedText": "2",
                    "confidenceScore": 1.0
                  }
                ]
              }
            ]
          }
        },
        {
          "status": {},
          "descriptionEngine": {
            "descriptionList": {}
          }
        }
      ]
    }
  ]
}
)";

// Batch response containing successful annotations for |kImage1Url| and
// |kImage2Url|, and a failure for |kImage3Url|.
//
// The results also appear "out of order" (i.e. image 2 comes before image 1).
constexpr char kBatchResponse[] = R"(
{
  "results": [
    {
      "imageId": "0  https://www.example.com/image2.jpg",
      "engineResults": [
        {
          "status": {},
          "ocrEngine": {
            "ocrRegions": [{
              "words": [{
                "detectedText": "2",
                "confidenceScore": 1.0
              }]
            }]
          }
        },
        {
          "status": {},
          "descriptionEngine": {
            "descriptionList": {}
          }
        }
      ]
    },
    {
      "imageId": "0  https://www.example.com/image1.jpg",
      "engineResults": [
        {
          "status": {},
          "ocrEngine": {
            "ocrRegions": [{
              "words": [{
                "detectedText": "1",
                "confidenceScore": 1.0
              }]
            }]
          }
        },
        {
          "status": {},
          "descriptionEngine": {
            "descriptionList": {}
          }
        }
      ]
    },
    {
      "imageId": "0  https://www.example.com/image3.jpg",
      "engineResults": [
        {
          "status": {
            "code": 8,
            "message": "Resource exhausted"
          },
          "ocrEngine": {}
        },
        {
          "status": {
            "code": 8,
            "message": "Resource exhausted"
          },
          "descriptionEngine": {}
        }
      ]
    }
  ]
})";

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
    on_get_jpg_image_data_.Reset();
  }

  void GetJpgImageData(GetJpgImageDataCallback callback) override {
    callbacks_.push_back(std::move(callback));
    if (on_get_jpg_image_data_) {
      std::move(on_get_jpg_image_data_).Run();
    }
  }

  void WaitForGetJpgImageData() {
    if (!callbacks_.empty()) {
      return;
    }
    base::RunLoop run_loop;
    on_get_jpg_image_data_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  std::vector<GetJpgImageDataCallback>& callbacks() { return callbacks_; }

 private:
  std::vector<GetJpgImageDataCallback> callbacks_;
  base::OnceClosure on_get_jpg_image_data_;

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
    std::string_view actual_body;
    if (request.request_body) {
      // We only support the simplest body structure.
      const std::vector<network::DataElement>& elements =
          *request.request_body->elements();
      if (elements.size() == 1 &&
          elements[0].type() ==
              network::mojom::DataElementDataView::Tag::kBytes) {
        actual_body =
            elements[0].As<network::DataElementBytes>().AsStringView();
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

// Returns a "canonically" formatted version of a JSON string by parsing and
// then rewriting it.
std::string ReformatJson(const std::string& in) {
  return base::WriteJson(
             base::JSONReader::Read(in, base::JSON_PARSE_CHROMIUM_EXTENSIONS)
                 .value())
      .value_or("");
}

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
  std::vector<std::string> GetAcceptLanguages() override {
    return accept_langs_;
  }
  std::vector<std::string> GetTopLanguages() override { return top_langs_; }
  void RecordLanguageMetrics(const std::string& page_language,
                             const std::string& requested_language) override {}

  std::vector<std::string> accept_langs_ = {"en", "it", "fr"};
  std::vector<std::string> top_langs_;
};

}  // namespace

// Test that annotation works for one client, and that the cache is populated.
TEST(AnnotatorTest, OcrSuccessAndCache) {
  base::test::TaskEnvironment test_task_env(
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);
  TestServerURLLoaderFactory test_url_factory(
      "https://ia-pa.googleapis.com/v1/");
  base::HistogramTester histogram_tester;

  Annotator annotator(
      GURL(kTestServerUrl), GURL(""), std::string() /* api_key */, kThrottle,
      1 /* batch_size */, 1.0 /* min_ocr_confidence */,
      test_url_factory.AsSharedURLLoaderFactory(), /*anchovy_provider=*/nullptr,
      std::make_unique<TestAnnotatorClient>());
  mojo::Remote<mojom::Annotator> remote_annotator;
  annotator.BindReceiverForTesting(
      remote_annotator.BindNewPipeAndPassReceiver());
  TestImageProcessor processor;

  // First call performs original image annotation.
  {
    std::optional<mojom::AnnotateImageError> error;
    std::vector<mojom::Annotation> annotations;

    remote_annotator->AnnotateImage(
        kImage1Url, kDescLang, processor.GetPendingRemote(),
        base::BindOnce(&ReportResult, &error, &annotations));
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

    // HTTP request should have been made.
    const std::string request =
        ReformatJson(base::StringPrintf(kTemplateRequest, kImage1Url, "AQID"));
    test_url_factory.ExpectRequestAndSimulateResponse(
        "annotation", {} /* expected_headers */, request, kOcrSuccessResponse,
        net::HTTP_OK);
    test_task_env.RunUntilIdle();

    // HTTP response should have completed and callback should have been called.
    ASSERT_THAT(error, Eq(std::nullopt));
    EXPECT_THAT(annotations,
                UnorderedElementsAre(AnnotatorEq(mojom::AnnotationType::kOcr,
                                                 1.0, "Region 1\nRegion 2")));

    // Metrics should have been logged for the major actions of the service.
    histogram_tester.ExpectUniqueSample(metrics_internal::kCacheHit, false, 1);
    histogram_tester.ExpectUniqueSample(metrics_internal::kPixelFetchSuccess,
                                        true, 1);
    histogram_tester.ExpectUniqueSample(metrics_internal::kServerRequestSize,
                                        request.size() / 1024, 1);
    histogram_tester.ExpectUniqueSample(metrics_internal::kServerNetError,
                                        net::Error::OK, 1);
    histogram_tester.ExpectUniqueSample(
        metrics_internal::kServerHttpResponseCode, net::HTTP_OK, 1);
    histogram_tester.ExpectUniqueSample(metrics_internal::kServerResponseSize,
                                        std::strlen(kOcrSuccessResponse), 1);
    histogram_tester.ExpectUniqueSample(metrics_internal::kJsonParseSuccess,
                                        true, 1);
    histogram_tester.ExpectUniqueSample(
        base::StringPrintf(metrics_internal::kAnnotationStatus, "Ocr"),
        0 /* OK RPC status */, 1);
    histogram_tester.ExpectUniqueSample(
        base::StringPrintf(metrics_internal::kAnnotationStatus, "Desc"),
        0 /* OK RPC status */, 1);
    histogram_tester.ExpectUniqueSample(
        base::StringPrintf(metrics_internal::kAnnotationConfidence, "Ocr"), 100,
        1);
    histogram_tester.ExpectUniqueSample(
        base::StringPrintf(metrics_internal::kAnnotationEmpty, "Ocr"), false,
        1);
  }

  // Second call uses cached results.
  {
    std::optional<mojom::AnnotateImageError> error;
    std::vector<mojom::Annotation> annotations;

    remote_annotator->AnnotateImage(
        kImage1Url, kDescLang, processor.GetPendingRemote(),
        base::BindOnce(&ReportResult, &error, &annotations));
    test_task_env.RunUntilIdle();

    // Pixels shouldn't be requested.
    ASSERT_THAT(processor.callbacks(), IsEmpty());

    // Results should have been directly returned without any server call.
    ASSERT_THAT(error, Eq(std::nullopt));
    EXPECT_THAT(annotations,
                UnorderedElementsAre(AnnotatorEq(mojom::AnnotationType::kOcr,
                                                 1.0, "Region 1\nRegion 2")));

    // Metrics should have been logged for a cache hit.
    EXPECT_THAT(histogram_tester.GetAllSamples(metrics_internal::kCacheHit),
                UnorderedElementsAre(Bucket(false, 1), Bucket(true, 1)));
  }
}

// Test that description annotations are successfully returned.
TEST(AnnotatorTest, DescriptionSuccess) {
  base::test::TaskEnvironment test_task_env(
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);
  TestServerURLLoaderFactory test_url_factory(
      "https://ia-pa.googleapis.com/v1/");
  base::HistogramTester histogram_tester;

  Annotator annotator(
      GURL(kTestServerUrl), GURL(""), std::string() /* api_key */, kThrottle,
      1 /* batch_size */, 1.0 /* min_ocr_confidence */,
      test_url_factory.AsSharedURLLoaderFactory(), /*anchovy_provider=*/nullptr,
      std::make_unique<TestAnnotatorClient>());
  mojo::Remote<mojom::Annotator> remote_annotator;
  annotator.BindReceiverForTesting(
      remote_annotator.BindNewPipeAndPassReceiver());
  TestImageProcessor processor;

  std::optional<mojom::AnnotateImageError> error;
  std::vector<mojom::Annotation> annotations;

  remote_annotator->AnnotateImage(
      kImage1Url, kDescLang, processor.GetPendingRemote(),
      base::BindOnce(&ReportResult, &error, &annotations));
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

  // HTTP request should have been made.
  test_url_factory.ExpectRequestAndSimulateResponse(
      "annotation", {} /* expected_headers */,
      ReformatJson(base::StringPrintf(kTemplateRequest, kImage1Url, "AQID")),
      R"({
           "results": [{
             "imageId": "0  https://www.example.com/image1.jpg",
             "engineResults": [
               {
                 "status": {},
                 "ocrEngine": {}
               },
               {
                 "status": {},
                 "descriptionEngine": {
                   "descriptionList": {
                     "descriptions": [
                       {
                         "type": "CAPTION",
                         "text": "This is an example image.",
                         "score": 0.9
                       },
                       {
                         "type": "LABEL",
                         "text": "Example image",
                         "score": 1.0
                       }
                     ]
                   }
                 }
               }
             ]
           }]
         })",
      net::HTTP_OK);
  test_task_env.RunUntilIdle();

  // HTTP response should have completed and callback should have been called.
  ASSERT_THAT(error, Eq(std::nullopt));
  EXPECT_THAT(
      annotations,
      UnorderedElementsAre(
          AnnotatorEq(mojom::AnnotationType::kOcr, 1.0, ""),
          AnnotatorEq(mojom::AnnotationType::kCaption, 0.9,
                      "This is an example image."),
          AnnotatorEq(mojom::AnnotationType::kLabel, 1.0, "Example image")));

  // Metrics about the description results should have been logged.
  histogram_tester.ExpectUniqueSample(
      metrics_internal::kImageRequestIncludesDesc, true, 1);
  histogram_tester.ExpectUniqueSample(
      base::StringPrintf(metrics_internal::kAnnotationStatus, "Ocr"),
      0 /* OK RPC status */, 1);
  histogram_tester.ExpectUniqueSample(
      base::StringPrintf(metrics_internal::kAnnotationStatus, "Desc"),
      0 /* OK RPC status */, 1);
  histogram_tester.ExpectUniqueSample(
      base::StringPrintf(metrics_internal::kAnnotationConfidence,
                         "DescCaption"),
      90, 1);
  histogram_tester.ExpectUniqueSample(
      base::StringPrintf(metrics_internal::kAnnotationConfidence, "DescLabel"),
      100, 1);
  histogram_tester.ExpectUniqueSample(
      base::StringPrintf(metrics_internal::kAnnotationEmpty, "DescCaption"),
      false, 1);
  histogram_tester.ExpectUniqueSample(
      base::StringPrintf(metrics_internal::kAnnotationEmpty, "DescLabel"),
      false, 1);
}

// Test that the specialized OCR result takes precedence.
TEST(AnnotatorTest, DoubleOcrResult) {
  base::test::TaskEnvironment test_task_env(
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);
  TestServerURLLoaderFactory test_url_factory(
      "https://ia-pa.googleapis.com/v1/");
  base::HistogramTester histogram_tester;

  Annotator annotator(
      GURL(kTestServerUrl), GURL(""), std::string() /* api_key */, kThrottle,
      1 /* batch_size */, 1.0 /* min_ocr_confidence */,
      test_url_factory.AsSharedURLLoaderFactory(), /*anchovy_provider=*/nullptr,
      std::make_unique<TestAnnotatorClient>());
  mojo::Remote<mojom::Annotator> remote_annotator;
  annotator.BindReceiverForTesting(
      remote_annotator.BindNewPipeAndPassReceiver());
  TestImageProcessor processor;

  std::optional<mojom::AnnotateImageError> error;
  std::vector<mojom::Annotation> annotations;

  remote_annotator->AnnotateImage(
      kImage1Url, kDescLang, processor.GetPendingRemote(),
      base::BindOnce(&ReportResult, &error, &annotations));
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

  // HTTP request should have been made.
  test_url_factory.ExpectRequestAndSimulateResponse(
      "annotation", {} /* expected_headers */,
      ReformatJson(base::StringPrintf(kTemplateRequest, kImage1Url, "AQID")),
      R"({
           "results": [{
             "imageId": "0  https://www.example.com/image1.jpg",
             "engineResults": [
               {
                 "status": {},
                 "ocrEngine": {
                   "ocrRegions": [{
                     "words": [{
                       "detectedText": "Region 1",
                       "confidenceScore": 1.0
                     }]
                   }]
                 }
               },
               {
                 "status": {},
                 "descriptionEngine": {
                   "descriptionList": {
                     "descriptions": [
                       {
                         "type": "CAPTION",
                         "text": "This is an example image.",
                         "score": 0.9
                       },
                       {
                         "type": "OCR",
                         "text": "R3gi0n I",
                         "score": 1.0
                       }
                     ]
                   }
                 }
               }
             ]
           }]
         })",
      net::HTTP_OK);
  test_task_env.RunUntilIdle();

  // HTTP response should have completed and callback should have been called.
  ASSERT_THAT(error, Eq(std::nullopt));
  EXPECT_THAT(annotations,
              UnorderedElementsAre(
                  AnnotatorEq(mojom::AnnotationType::kOcr, 1.0, "Region 1"),
                  AnnotatorEq(mojom::AnnotationType::kCaption, 0.9,
                              "This is an example image.")));

  // Metrics about the returned results should have been logged.
  histogram_tester.ExpectUniqueSample(
      base::StringPrintf(metrics_internal::kAnnotationStatus, "Ocr"),
      0 /* OK RPC status */, 1);
  histogram_tester.ExpectUniqueSample(
      base::StringPrintf(metrics_internal::kAnnotationStatus, "Desc"),
      0 /* OK RPC status */, 1);
  histogram_tester.ExpectUniqueSample(
      base::StringPrintf(metrics_internal::kAnnotationConfidence, "Ocr"), 100,
      1);
  histogram_tester.ExpectUniqueSample(
      base::StringPrintf(metrics_internal::kAnnotationConfidence,
                         "DescCaption"),
      90, 1);
  histogram_tester.ExpectUniqueSample(
      base::StringPrintf(metrics_internal::kAnnotationConfidence, "DescOcr"),
      100, 1);
  histogram_tester.ExpectUniqueSample(
      base::StringPrintf(metrics_internal::kAnnotationEmpty, "Ocr"), false, 1);
  histogram_tester.ExpectUniqueSample(
      base::StringPrintf(metrics_internal::kAnnotationEmpty, "DescCaption"),
      false, 1);
  histogram_tester.ExpectUniqueSample(
      base::StringPrintf(metrics_internal::kAnnotationEmpty, "DescOcr"), false,
      1);
}

// Test that HTTP failure is gracefully handled.
TEST(AnnotatorTest, HttpError) {
  base::test::TaskEnvironment test_task_env(
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);
  TestServerURLLoaderFactory test_url_factory(
      "https://ia-pa.googleapis.com/v1/");
  base::HistogramTester histogram_tester;

  Annotator annotator(
      GURL(kTestServerUrl), GURL(""), std::string() /* api_key */, kThrottle,
      1 /* batch_size */, 1.0 /* min_ocr_confidence */,
      test_url_factory.AsSharedURLLoaderFactory(), /*anchovy_provider=*/nullptr,
      std::make_unique<TestAnnotatorClient>());
  mojo::Remote<mojom::Annotator> remote_annotator;
  annotator.BindReceiverForTesting(
      remote_annotator.BindNewPipeAndPassReceiver());

  TestImageProcessor processor;
  std::optional<mojom::AnnotateImageError> error;
  std::vector<mojom::Annotation> annotations;

  remote_annotator->AnnotateImage(
      kImage1Url, kDescLang, processor.GetPendingRemote(),
      base::BindOnce(&ReportResult, &error, &annotations));
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

  // HTTP request should have been made.
  test_url_factory.ExpectRequestAndSimulateResponse(
      "annotation", {} /* expected_headers */,
      ReformatJson(base::StringPrintf(kTemplateRequest, kImage1Url, "AQID")),
      "", net::HTTP_INTERNAL_SERVER_ERROR);
  test_task_env.RunUntilIdle();

  // HTTP response should have completed and callback should have been called.
  EXPECT_THAT(error, Eq(mojom::AnnotateImageError::kFailure));
  EXPECT_THAT(annotations, IsEmpty());

  // Metrics about the HTTP request failure should have been logged.
  histogram_tester.ExpectUniqueSample(
      metrics_internal::kServerNetError,
      net::Error::ERR_HTTP_RESPONSE_CODE_FAILURE, 1);
  histogram_tester.ExpectUniqueSample(metrics_internal::kServerHttpResponseCode,
                                      net::HTTP_INTERNAL_SERVER_ERROR, 1);
  histogram_tester.ExpectUniqueSample(metrics_internal::kClientResult,
                                      ClientResult::kFailed, 1);
}

// Test that backend failure is gracefully handled.
TEST(AnnotatorTest, BackendError) {
  base::test::TaskEnvironment test_task_env(
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);
  TestServerURLLoaderFactory test_url_factory(
      "https://ia-pa.googleapis.com/v1/");
  base::HistogramTester histogram_tester;

  Annotator annotator(
      GURL(kTestServerUrl), GURL(""), std::string() /* api_key */, kThrottle,
      1 /* batch_size */, 1.0 /* min_ocr_confidence */,
      test_url_factory.AsSharedURLLoaderFactory(), /*anchovy_provider=*/nullptr,
      std::make_unique<TestAnnotatorClient>());
  mojo::Remote<mojom::Annotator> remote_annotator;
  annotator.BindReceiverForTesting(
      remote_annotator.BindNewPipeAndPassReceiver());

  TestImageProcessor processor;
  std::optional<mojom::AnnotateImageError> error;
  std::vector<mojom::Annotation> annotations;

  remote_annotator->AnnotateImage(
      kImage1Url, kDescLang, processor.GetPendingRemote(),
      base::BindOnce(&ReportResult, &error, &annotations));
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

  // HTTP request should have been made.
  test_url_factory.ExpectRequestAndSimulateResponse(
      "annotation", {} /* expected_headers */,
      ReformatJson(base::StringPrintf(kTemplateRequest, kImage1Url, "AQID")),
      R"({
           "results": [{
             "imageId": "0  https://www.example.com/image1.jpg",
             "engineResults": [
               {
                 "status": {
                   "code": 8,
                   "message": "Resource exhausted"
                 },
                 "ocrEngine": {}
               },
               {
                 "status": {
                   "code": 8,
                   "messages": "Resource exhausted"
                 },
                 "descriptionEngine": {}
               }
             ]
           }]
         })",
      net::HTTP_OK);
  test_task_env.RunUntilIdle();

  // HTTP response should have completed and callback should have been called
  // with an error status.
  EXPECT_THAT(error, Eq(mojom::AnnotateImageError::kFailure));
  EXPECT_THAT(annotations, IsEmpty());

  // Metrics about the backend failure should have been logged.
  histogram_tester.ExpectUniqueSample(metrics_internal::kServerNetError,
                                      net::Error::OK, 1);
  histogram_tester.ExpectUniqueSample(metrics_internal::kServerHttpResponseCode,
                                      net::HTTP_OK, 1);
  histogram_tester.ExpectUniqueSample(
      base::StringPrintf(metrics_internal::kAnnotationStatus, "Ocr"),
      8 /* Failed RPC status */, 1);
  histogram_tester.ExpectUniqueSample(
      base::StringPrintf(metrics_internal::kAnnotationStatus, "Desc"),
      8 /* Failed RPC status */, 1);
  histogram_tester.ExpectUniqueSample(metrics_internal::kClientResult,
                                      ClientResult::kFailed, 1);
}

// Test that partial results are returned if the OCR backend fails.
TEST(AnnotatorTest, OcrBackendError) {
  base::test::TaskEnvironment test_task_env(
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);
  TestServerURLLoaderFactory test_url_factory(
      "https://ia-pa.googleapis.com/v1/");
  base::HistogramTester histogram_tester;

  Annotator annotator(
      GURL(kTestServerUrl), GURL(""), std::string() /* api_key */, kThrottle,
      1 /* batch_size */, 1.0 /* min_ocr_confidence */,
      test_url_factory.AsSharedURLLoaderFactory(), /*anchovy_provider=*/nullptr,
      std::make_unique<TestAnnotatorClient>());
  mojo::Remote<mojom::Annotator> remote_annotator;
  annotator.BindReceiverForTesting(
      remote_annotator.BindNewPipeAndPassReceiver());

  TestImageProcessor processor;
  std::optional<mojom::AnnotateImageError> error;
  std::vector<mojom::Annotation> annotations;

  remote_annotator->AnnotateImage(
      kImage1Url, kDescLang, processor.GetPendingRemote(),
      base::BindOnce(&ReportResult, &error, &annotations));
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

  // HTTP request should have been made.
  test_url_factory.ExpectRequestAndSimulateResponse(
      "annotation", {} /* expected_headers */,
      ReformatJson(base::StringPrintf(kTemplateRequest, kImage1Url, "AQID")),
      R"({
           "results": [{
             "imageId": "0  https://www.example.com/image1.jpg",
             "engineResults": [
               {
                 "status": {
                   "code": 8,
                   "message": "Resource exhausted"
                 },
                 "ocrEngine": {}
               },
               {
                 "status": {},
                 "descriptionEngine": {
                   "descriptionList": {
                     "descriptions": [{
                       "type": "CAPTION",
                       "text": "This is an example image.",
                       "score": 0.9
                     }]
                   }
                 }
               }
             ]
           }]
         })",
      net::HTTP_OK);
  test_task_env.RunUntilIdle();

  // HTTP response should have completed and callback should have been called.
  EXPECT_THAT(error, Eq(std::nullopt));
  EXPECT_THAT(annotations, UnorderedElementsAre(
                               AnnotatorEq(mojom::AnnotationType::kCaption, 0.9,
                                           "This is an example image.")));

  // Metrics about the partial results should have been logged.
  histogram_tester.ExpectUniqueSample(metrics_internal::kServerNetError,
                                      net::Error::OK, 1);
  histogram_tester.ExpectUniqueSample(metrics_internal::kServerHttpResponseCode,
                                      net::HTTP_OK, 1);
  histogram_tester.ExpectUniqueSample(
      base::StringPrintf(metrics_internal::kAnnotationStatus, "Ocr"),
      8 /* Failed RPC status */, 1);
  histogram_tester.ExpectUniqueSample(
      base::StringPrintf(metrics_internal::kAnnotationStatus, "Desc"),
      0 /* OK RPC status */, 1);
  histogram_tester.ExpectUniqueSample(
      base::StringPrintf(metrics_internal::kAnnotationConfidence,
                         "DescCaption"),
      90, 1);
  histogram_tester.ExpectUniqueSample(
      base::StringPrintf(metrics_internal::kAnnotationEmpty, "DescCaption"),
      false, 1);
}

// Test that partial results are returned if the description backend fails.
TEST(AnnotatorTest, DescriptionBackendError) {
  base::test::TaskEnvironment test_task_env(
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);
  TestServerURLLoaderFactory test_url_factory(
      "https://ia-pa.googleapis.com/v1/");
  base::HistogramTester histogram_tester;

  Annotator annotator(
      GURL(kTestServerUrl), GURL(""), std::string() /* api_key */, kThrottle,
      1 /* batch_size */, 1.0 /* min_ocr_confidence */,
      test_url_factory.AsSharedURLLoaderFactory(), /*anchovy_provider=*/nullptr,
      std::make_unique<TestAnnotatorClient>());
  mojo::Remote<mojom::Annotator> remote_annotator;
  annotator.BindReceiverForTesting(
      remote_annotator.BindNewPipeAndPassReceiver());

  TestImageProcessor processor;
  std::optional<mojom::AnnotateImageError> error;
  std::vector<mojom::Annotation> annotations;

  remote_annotator->AnnotateImage(
      kImage1Url, kDescLang, processor.GetPendingRemote(),
      base::BindOnce(&ReportResult, &error, &annotations));
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

  // HTTP request should have been made.
  test_url_factory.ExpectRequestAndSimulateResponse(
      "annotation", {} /* expected_headers */,
      ReformatJson(base::StringPrintf(kTemplateRequest, kImage1Url, "AQID")),
      R"({
           "results": [{
             "imageId": "0  https://www.example.com/image1.jpg",
             "engineResults": [
               {
                 "status": {},
                 "ocrEngine": {
                   "ocrRegions": [{
                     "words": [{
                       "detectedText": "1",
                       "confidenceScore": 1.0
                     }]
                   }]
                 }
               },
               {
                 "status": {
                   "code": 8,
                   "message": "Resource exhausted"
                 },
                 "descriptionEngine": {}
               }
             ]
           }]
         })",
      net::HTTP_OK);
  test_task_env.RunUntilIdle();

  // HTTP response should have completed and callback should have been called.
  EXPECT_THAT(error, Eq(std::nullopt));
  EXPECT_THAT(annotations, UnorderedElementsAre(AnnotatorEq(
                               mojom::AnnotationType::kOcr, 1.0, "1")));

  // Metrics about the partial results should have been logged.
  histogram_tester.ExpectUniqueSample(metrics_internal::kServerNetError,
                                      net::Error::OK, 1);
  histogram_tester.ExpectUniqueSample(metrics_internal::kServerHttpResponseCode,
                                      net::HTTP_OK, 1);
  histogram_tester.ExpectUniqueSample(
      base::StringPrintf(metrics_internal::kAnnotationStatus, "Ocr"),
      0 /* OK RPC status */, 1);
  histogram_tester.ExpectUniqueSample(
      base::StringPrintf(metrics_internal::kAnnotationStatus, "Desc"),
      8 /* Failed RPC status */, 1);
  histogram_tester.ExpectUniqueSample(
      base::StringPrintf(metrics_internal::kAnnotationConfidence, "Ocr"), 100,
      1);
  histogram_tester.ExpectUniqueSample(
      base::StringPrintf(metrics_internal::kAnnotationEmpty, "Ocr"), false, 1);
}

// Test that server failure (i.e. nonsense response) is gracefully handled.
TEST(AnnotatorTest, ServerError) {
  base::test::TaskEnvironment test_task_env(
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);
  TestServerURLLoaderFactory test_url_factory(
      "https://ia-pa.googleapis.com/v1/");
  base::HistogramTester histogram_tester;

  Annotator annotator(
      GURL(kTestServerUrl), GURL(""), std::string() /* api_key */, kThrottle,
      1 /* batch_size */, 1.0 /* min_ocr_confidence */,
      test_url_factory.AsSharedURLLoaderFactory(), /*anchovy_provider=*/nullptr,
      std::make_unique<TestAnnotatorClient>());
  mojo::Remote<mojom::Annotator> remote_annotator;
  annotator.BindReceiverForTesting(
      remote_annotator.BindNewPipeAndPassReceiver());

  TestImageProcessor processor;
  std::optional<mojom::AnnotateImageError> error;
  std::vector<mojom::Annotation> annotations;

  remote_annotator->AnnotateImage(
      kImage1Url, kDescLang, processor.GetPendingRemote(),
      base::BindOnce(&ReportResult, &error, &annotations));
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

  // HTTP request should have been made; respond with nonsense string.
  test_url_factory.ExpectRequestAndSimulateResponse(
      "annotation", {} /* expected_headers */,
      ReformatJson(base::StringPrintf(kTemplateRequest, kImage1Url, "AQID")),
      "Hello, world!", net::HTTP_OK);
  test_task_env.RunUntilIdle();

  // HTTP response should have completed and callback should have been called
  // with an error status.
  EXPECT_THAT(error, Eq(mojom::AnnotateImageError::kFailure));
  EXPECT_THAT(annotations, IsEmpty());

  // Metrics about the invalid response format should have been logged.
  histogram_tester.ExpectUniqueSample(metrics_internal::kServerNetError,
                                      net::Error::OK, 1);
  histogram_tester.ExpectUniqueSample(metrics_internal::kServerHttpResponseCode,
                                      net::HTTP_OK, 1);
  histogram_tester.ExpectUniqueSample(metrics_internal::kJsonParseSuccess,
                                      false, 1);
  histogram_tester.ExpectUniqueSample(metrics_internal::kClientResult,
                                      ClientResult::kFailed, 1);
}

// Test that adult content returns an error.
TEST(AnnotatorTest, AdultError) {
  base::test::TaskEnvironment test_task_env(
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);
  TestServerURLLoaderFactory test_url_factory(
      "https://ia-pa.googleapis.com/v1/");
  base::HistogramTester histogram_tester;

  Annotator annotator(
      GURL(kTestServerUrl), GURL(""), std::string() /* api_key */, kThrottle,
      1 /* batch_size */, 1.0 /* min_ocr_confidence */,
      test_url_factory.AsSharedURLLoaderFactory(), /*anchovy_provider=*/nullptr,
      std::make_unique<TestAnnotatorClient>());
  mojo::Remote<mojom::Annotator> remote_annotator;
  annotator.BindReceiverForTesting(
      remote_annotator.BindNewPipeAndPassReceiver());

  TestImageProcessor processor;
  std::optional<mojom::AnnotateImageError> error;
  std::vector<mojom::Annotation> annotations;

  remote_annotator->AnnotateImage(
      kImage1Url, kDescLang, processor.GetPendingRemote(),
      base::BindOnce(&ReportResult, &error, &annotations));
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

  // HTTP request should have been made.
  test_url_factory.ExpectRequestAndSimulateResponse(
      "annotation", {} /* expected headers */,
      ReformatJson(base::StringPrintf(kTemplateRequest, kImage1Url, "AQID")),
      R"({
           "results": [{
             "imageId": "0  https://www.example.com/image1.jpg",
             "engineResults": [
               {
                 "status": {},
                 "ocrEngine": {
                   "ocrRegions": []
                 }
               },
               {
                 "status": {},
                 "descriptionEngine": {
                   "failureReason": "ADULT"
                 }
               }
             ]
           }]
         })",
      net::HTTP_OK);
  test_task_env.RunUntilIdle();

  // HTTP response should have completed and callback should have been called
  // with an error status.
  EXPECT_THAT(error, Eq(mojom::AnnotateImageError::kAdult));
  EXPECT_THAT(annotations, IsEmpty());

  // Metrics about the adult error should have been logged.
  histogram_tester.ExpectUniqueSample(metrics_internal::kServerNetError,
                                      net::Error::OK, 1);
  histogram_tester.ExpectUniqueSample(metrics_internal::kServerHttpResponseCode,
                                      net::HTTP_OK, 1);
  histogram_tester.ExpectUniqueSample(metrics_internal::kDescFailure,
                                      DescFailureReason::kAdult, 1);
}

// Test that work is reassigned if a processor fails.
TEST(AnnotatorTest, ProcessorFails) {
  base::test::TaskEnvironment test_task_env(
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);
  TestServerURLLoaderFactory test_url_factory(
      "https://ia-pa.googleapis.com/v1/");
  base::HistogramTester histogram_tester;

  Annotator annotator(
      GURL(kTestServerUrl), GURL(""), std::string() /* api_key */, kThrottle,
      1 /* batch_size */, 1.0 /* min_ocr_confidence */,
      test_url_factory.AsSharedURLLoaderFactory(), /*anchovy_provider=*/nullptr,
      std::make_unique<TestAnnotatorClient>());
  mojo::Remote<mojom::Annotator> remote_annotator;
  annotator.BindReceiverForTesting(
      remote_annotator.BindNewPipeAndPassReceiver());

  std::array<TestImageProcessor, 3> processor;
  std::array<std::optional<mojom::AnnotateImageError>, 3> error;
  std::array<std::vector<mojom::Annotation>, 3> annotations;

  for (int i = 0; i < 3; ++i) {
    remote_annotator->AnnotateImage(
        kImage1Url, kDescLang, processor[i].GetPendingRemote(),
        base::BindOnce(&ReportResult, &error[i], &annotations[i]));
  }
  test_task_env.RunUntilIdle();

  // Annotator should have asked processor 1 for image 1's pixels.
  ASSERT_THAT(processor[0].callbacks(), SizeIs(1));
  ASSERT_THAT(processor[1].callbacks(), IsEmpty());
  ASSERT_THAT(processor[2].callbacks(), IsEmpty());

  // Make processor 1 fail by returning empty bytes.
  std::move(processor[0].callbacks()[0]).Run({}, 0, 0);
  processor[0].callbacks().pop_back();
  test_task_env.RunUntilIdle();

  // Annotator should have asked processor 2 for image 1's pixels.
  ASSERT_THAT(processor[0].callbacks(), IsEmpty());
  ASSERT_THAT(processor[1].callbacks(), SizeIs(1));
  ASSERT_THAT(processor[2].callbacks(), IsEmpty());

  // Send back image data.
  std::move(processor[1].callbacks()[0]).Run({1, 2, 3}, kDescDim, kDescDim);
  processor[1].callbacks().pop_back();
  test_task_env.RunUntilIdle();

  // No request should be sent yet (because service is waiting to batch up
  // multiple requests).
  EXPECT_THAT(test_url_factory.requests(), IsEmpty());
  test_task_env.FastForwardBy(base::Seconds(1));

  // HTTP request for image 1 should have been made.
  test_url_factory.ExpectRequestAndSimulateResponse(
      "annotation", {} /* expected_headers */,
      ReformatJson(base::StringPrintf(kTemplateRequest, kImage1Url, "AQID")),
      kOcrSuccessResponse, net::HTTP_OK);
  test_task_env.RunUntilIdle();

  // Annotator should have called all callbacks, but request 1 received an error
  // when we returned empty bytes.
  ASSERT_THAT(error, ElementsAre(mojom::AnnotateImageError::kFailure,
                                 std::nullopt, std::nullopt));
  EXPECT_THAT(annotations[0], IsEmpty());
  EXPECT_THAT(annotations[1],
              UnorderedElementsAre(AnnotatorEq(mojom::AnnotationType::kOcr, 1.0,
                                               "Region 1\nRegion 2")));
  EXPECT_THAT(annotations[2],
              UnorderedElementsAre(AnnotatorEq(mojom::AnnotationType::kOcr, 1.0,
                                               "Region 1\nRegion 2")));

  // Metrics about the pixel fetch failure should have been logged.
  EXPECT_THAT(
      histogram_tester.GetAllSamples(metrics_internal::kPixelFetchSuccess),
      UnorderedElementsAre(Bucket(false, 1), Bucket(true, 1)));
  EXPECT_THAT(histogram_tester.GetAllSamples(metrics_internal::kClientResult),
              UnorderedElementsAre(
                  Bucket(static_cast<int32_t>(ClientResult::kFailed), 1),
                  Bucket(static_cast<int32_t>(ClientResult::kSucceeded), 2)));
}

// Test a case that was previously buggy: when one client requests annotations,
// then fails local processing, then another client makes the same request.
TEST(AnnotatorTest, ProcessorFailedPreviously) {
  base::test::TaskEnvironment test_task_env(
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);
  TestServerURLLoaderFactory test_url_factory(
      "https://ia-pa.googleapis.com/v1/");
  base::HistogramTester histogram_tester;

  Annotator annotator(
      GURL(kTestServerUrl), GURL(""), std::string() /* api_key */, kThrottle,
      1 /* batch_size */, 1.0 /* min_ocr_confidence */,
      test_url_factory.AsSharedURLLoaderFactory(), /*anchovy_provider=*/nullptr,
      std::make_unique<TestAnnotatorClient>());
  mojo::Remote<mojom::Annotator> remote_annotator;
  annotator.BindReceiverForTesting(
      remote_annotator.BindNewPipeAndPassReceiver());

  std::array<TestImageProcessor, 2> processor;
  std::array<std::optional<mojom::AnnotateImageError>, 2> error;
  std::array<std::vector<mojom::Annotation>, 2> annotations;

  // Processor 1 makes a request for annotation of a given image.
  remote_annotator->AnnotateImage(
      kImage1Url, kDescLang, processor[0].GetPendingRemote(),
      base::BindOnce(&ReportResult, &error[0], &annotations[0]));
  test_task_env.RunUntilIdle();

  // Annotator should have asked processor 1 for the image's pixels.
  ASSERT_THAT(processor[0].callbacks(), SizeIs(1));
  ASSERT_THAT(processor[1].callbacks(), IsEmpty());

  // Make processor 1 fail by returning empty bytes.
  std::move(processor[0].callbacks()[0]).Run({}, 0, 0);
  processor[0].callbacks().pop_back();
  test_task_env.RunUntilIdle();

  // Processor 2 makes a request for annotation of the same image.
  remote_annotator->AnnotateImage(
      kImage1Url, kDescLang, processor[1].GetPendingRemote(),
      base::BindOnce(&ReportResult, &error[1], &annotations[1]));
  test_task_env.RunUntilIdle();

  // Annotator should have asked processor 2 for the image's pixels.
  ASSERT_THAT(processor[1].callbacks(), SizeIs(1));

  // Send back image data.
  std::move(processor[1].callbacks()[0]).Run({1, 2, 3}, kDescDim, kDescDim);
  processor[1].callbacks().pop_back();
  test_task_env.RunUntilIdle();

  // No request should be sent yet (because service is waiting to batch up
  // multiple requests).
  EXPECT_THAT(test_url_factory.requests(), IsEmpty());
  test_task_env.FastForwardBy(base::Seconds(1));

  // HTTP request for image 1 should have been made.
  test_url_factory.ExpectRequestAndSimulateResponse(
      "annotation", {} /* expected_headers */,
      ReformatJson(base::StringPrintf(kTemplateRequest, kImage1Url, "AQID")),
      kOcrSuccessResponse, net::HTTP_OK);
  test_task_env.RunUntilIdle();

  // Annotator should have called all callbacks, but request 1 received an error
  // when we returned empty bytes.
  ASSERT_THAT(error,
              ElementsAre(mojom::AnnotateImageError::kFailure, std::nullopt));
  EXPECT_THAT(annotations[0], IsEmpty());
  EXPECT_THAT(annotations[1],
              UnorderedElementsAre(AnnotatorEq(mojom::AnnotationType::kOcr, 1.0,
                                               "Region 1\nRegion 2")));
}

// Test that work is reassigned if processor dies.
TEST(AnnotatorTest, ProcessorDies) {
  base::test::TaskEnvironment test_task_env(
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);
  TestServerURLLoaderFactory test_url_factory(
      "https://ia-pa.googleapis.com/v1/");
  base::HistogramTester histogram_tester;

  Annotator annotator(
      GURL(kTestServerUrl), GURL(""), std::string() /* api_key */, kThrottle,
      1 /* batch_size */, 1.0 /* min_ocr_confidence */,
      test_url_factory.AsSharedURLLoaderFactory(), /*anchovy_provider=*/nullptr,
      std::make_unique<TestAnnotatorClient>());
  mojo::Remote<mojom::Annotator> remote_annotator;
  annotator.BindReceiverForTesting(
      remote_annotator.BindNewPipeAndPassReceiver());

  std::array<TestImageProcessor, 3> processor;
  std::array<std::optional<mojom::AnnotateImageError>, 3> error;
  std::array<std::vector<mojom::Annotation>, 3> annotations;

  for (int i = 0; i < 3; ++i) {
    remote_annotator->AnnotateImage(
        kImage1Url, kDescLang, processor[i].GetPendingRemote(),
        base::BindOnce(&ReportResult, &error[i], &annotations[i]));
  }
  test_task_env.RunUntilIdle();

  // Annotator should have asked processor 1 for image 1's pixels.
  ASSERT_THAT(processor[0].callbacks(), SizeIs(1));
  ASSERT_THAT(processor[1].callbacks(), IsEmpty());
  ASSERT_THAT(processor[2].callbacks(), IsEmpty());

  // Kill processor 1.
  processor[0].Reset();
  test_task_env.RunUntilIdle();

  // Annotator should have asked processor 2 for image 1's pixels.
  ASSERT_THAT(processor[0].callbacks(), IsEmpty());
  ASSERT_THAT(processor[1].callbacks(), SizeIs(1));
  ASSERT_THAT(processor[2].callbacks(), IsEmpty());

  // Send back image data.
  std::move(processor[1].callbacks()[0]).Run({1, 2, 3}, kDescDim, kDescDim);
  processor[1].callbacks().pop_back();
  test_task_env.RunUntilIdle();

  // No request should be sent yet (because service is waiting to batch up
  // multiple requests).
  EXPECT_THAT(test_url_factory.requests(), IsEmpty());
  test_task_env.FastForwardBy(base::Seconds(1));

  // HTTP request for image 1 should have been made.
  test_url_factory.ExpectRequestAndSimulateResponse(
      "annotation", {} /* expected_headers */,
      ReformatJson(base::StringPrintf(kTemplateRequest, kImage1Url, "AQID")),
      kOcrSuccessResponse, net::HTTP_OK);
  test_task_env.RunUntilIdle();

  // Annotator should have called all callbacks, but request 1 was canceled when
  // we reset processor 1.
  ASSERT_THAT(error, ElementsAre(mojom::AnnotateImageError::kCanceled,
                                 std::nullopt, std::nullopt));
  EXPECT_THAT(annotations[0], IsEmpty());
  EXPECT_THAT(annotations[1],
              UnorderedElementsAre(AnnotatorEq(mojom::AnnotationType::kOcr, 1.0,
                                               "Region 1\nRegion 2")));
  EXPECT_THAT(annotations[2],
              UnorderedElementsAre(AnnotatorEq(mojom::AnnotationType::kOcr, 1.0,
                                               "Region 1\nRegion 2")));

  // Metrics about the client cancelation should have been logged.
  EXPECT_THAT(histogram_tester.GetAllSamples(metrics_internal::kClientResult),
              UnorderedElementsAre(
                  Bucket(static_cast<int32_t>(ClientResult::kCanceled), 1),
                  Bucket(static_cast<int32_t>(ClientResult::kSucceeded), 2)));
}

// Test that multiple concurrent requests are handled in the same batch.
TEST(AnnotatorTest, ConcurrentSameBatch) {
  base::test::TaskEnvironment test_task_env(
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);
  TestServerURLLoaderFactory test_url_factory(
      "https://ia-pa.googleapis.com/v1/");
  base::HistogramTester histogram_tester;

  Annotator annotator(
      GURL(kTestServerUrl), GURL(""), std::string() /* api_key */, kThrottle,
      3 /* batch_size */, 1.0 /* min_ocr_confidence */,
      test_url_factory.AsSharedURLLoaderFactory(), /*anchovy_provider=*/nullptr,
      std::make_unique<TestAnnotatorClient>());
  mojo::Remote<mojom::Annotator> remote_annotator;
  annotator.BindReceiverForTesting(
      remote_annotator.BindNewPipeAndPassReceiver());

  std::array<TestImageProcessor, 3> processor;
  std::array<std::optional<mojom::AnnotateImageError>, 3> error;
  std::array<std::vector<mojom::Annotation>, 3> annotations;

  // Request OCR for images 1, 2 and 3.
  remote_annotator->AnnotateImage(
      kImage1Url, kDescLang, processor[0].GetPendingRemote(),
      base::BindOnce(&ReportResult, &error[0], &annotations[0]));
  remote_annotator->AnnotateImage(
      kImage2Url, kDescLang, processor[1].GetPendingRemote(),
      base::BindOnce(&ReportResult, &error[1], &annotations[1]));
  remote_annotator->AnnotateImage(
      kImage3Url, kDescLang, processor[2].GetPendingRemote(),
      base::BindOnce(&ReportResult, &error[2], &annotations[2]));
  test_task_env.RunUntilIdle();

  // Annotator should have asked processor 1 for image 1's pixels, processor
  // 2 for image 2's pixels and processor 3 for image 3's pixels.
  ASSERT_THAT(processor[0].callbacks(), SizeIs(1));
  ASSERT_THAT(processor[1].callbacks(), SizeIs(1));
  ASSERT_THAT(processor[2].callbacks(), SizeIs(1));

  // Send back image data.
  std::move(processor[0].callbacks()[0]).Run({1, 2, 3}, kDescDim, kDescDim);
  processor[0].callbacks().pop_back();
  std::move(processor[1].callbacks()[0]).Run({4, 5, 6}, kDescDim, kDescDim);
  processor[1].callbacks().pop_back();
  std::move(processor[2].callbacks()[0]).Run({7, 8, 9}, kDescDim, kDescDim);
  processor[2].callbacks().pop_back();
  test_task_env.RunUntilIdle();

  // No request should be sent yet (because service is waiting to batch up
  // multiple requests).
  EXPECT_THAT(test_url_factory.requests(), IsEmpty());
  test_task_env.FastForwardBy(base::Seconds(1));

  // A single HTTP request for all images should have been sent.
  test_url_factory.ExpectRequestAndSimulateResponse(
      "annotation", {} /* expected_headers */, ReformatJson(kBatchRequest),
      kBatchResponse, net::HTTP_OK);
  test_task_env.RunUntilIdle();

  // Annotator should have called each callback with its corresponding text or
  // failure.
  ASSERT_THAT(error, ElementsAre(std::nullopt, std::nullopt,
                                 mojom::AnnotateImageError::kFailure));
  EXPECT_THAT(annotations[0], UnorderedElementsAre(AnnotatorEq(
                                  mojom::AnnotationType::kOcr, 1.0, "1")));
  EXPECT_THAT(annotations[1], UnorderedElementsAre(AnnotatorEq(
                                  mojom::AnnotationType::kOcr, 1.0, "2")));
  EXPECT_THAT(annotations[2], IsEmpty());

  // Metrics should have been logged for a single server response with multiple
  // results included.
  histogram_tester.ExpectUniqueSample(metrics_internal::kServerNetError,
                                      net::Error::OK, 1);
  histogram_tester.ExpectUniqueSample(metrics_internal::kServerHttpResponseCode,
                                      net::HTTP_OK, 1);
  EXPECT_THAT(histogram_tester.GetAllSamples(base::StringPrintf(
                  metrics_internal::kAnnotationStatus, "Ocr")),
              UnorderedElementsAre(Bucket(8 /* Failed RPC status */, 1),
                                   Bucket(0 /* OK RPC status */, 2)));
  histogram_tester.ExpectUniqueSample(
      base::StringPrintf(metrics_internal::kAnnotationConfidence, "Ocr"), 100,
      2);
  histogram_tester.ExpectUniqueSample(
      base::StringPrintf(metrics_internal::kAnnotationEmpty, "Ocr"), false, 2);
  EXPECT_THAT(histogram_tester.GetAllSamples(metrics_internal::kClientResult),
              UnorderedElementsAre(
                  Bucket(static_cast<int32_t>(ClientResult::kFailed), 1),
                  Bucket(static_cast<int32_t>(ClientResult::kSucceeded), 2)));
}

// Test that multiple concurrent requests are handled in separate batches.
TEST(AnnotatorTest, ConcurrentSeparateBatches) {
  base::test::TaskEnvironment test_task_env(
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);
  TestServerURLLoaderFactory test_url_factory(
      "https://ia-pa.googleapis.com/v1/");
  base::HistogramTester histogram_tester;

  Annotator annotator(
      GURL(kTestServerUrl), GURL(""), std::string() /* api_key */, kThrottle,
      3 /* batch_size */, 1.0 /* min_ocr_confidence */,
      test_url_factory.AsSharedURLLoaderFactory(), /*anchovy_provider=*/nullptr,
      std::make_unique<TestAnnotatorClient>());
  mojo::Remote<mojom::Annotator> remote_annotator;
  annotator.BindReceiverForTesting(
      remote_annotator.BindNewPipeAndPassReceiver());

  std::array<TestImageProcessor, 2> processor;
  std::array<std::optional<mojom::AnnotateImageError>, 2> error;
  std::array<std::vector<mojom::Annotation>, 2> annotations;

  // Request OCR for image 1.
  remote_annotator->AnnotateImage(
      kImage1Url, kDescLang, processor[0].GetPendingRemote(),
      base::BindOnce(&ReportResult, &error[0], &annotations[0]));
  test_task_env.RunUntilIdle();

  // Annotator should have asked processor 1 for image 1's pixels.
  ASSERT_THAT(processor[0].callbacks(), SizeIs(1));
  ASSERT_THAT(processor[1].callbacks(), IsEmpty());

  // Send back image 1 data.
  std::move(processor[0].callbacks()[0]).Run({1, 2, 3}, kDescDim, kDescDim);
  processor[0].callbacks().pop_back();
  test_task_env.RunUntilIdle();

  // No request should be sent yet (because service is waiting to batch up
  // multiple requests).
  EXPECT_THAT(test_url_factory.requests(), IsEmpty());
  test_task_env.FastForwardBy(base::Seconds(1));

  // Request OCR for image 2.
  remote_annotator->AnnotateImage(
      kImage2Url, kDescLang, processor[1].GetPendingRemote(),
      base::BindOnce(&ReportResult, &error[1], &annotations[1]));
  test_task_env.RunUntilIdle();

  // Annotator should have asked processor 2 for image 2's pixels.
  ASSERT_THAT(processor[0].callbacks(), IsEmpty());
  ASSERT_THAT(processor[1].callbacks(), SizeIs(1));

  // Send back image 2 data.
  std::move(processor[1].callbacks()[0]).Run({4, 5, 6}, kDescDim, kDescDim);
  processor[1].callbacks().pop_back();
  test_task_env.RunUntilIdle();

  // Only the HTTP request for image 1 should have been made (the service is
  // still waiting to make the batch that will include the request for image
  // 2).
  test_url_factory.ExpectRequestAndSimulateResponse(
      "annotation", {} /* expected_headers */,
      ReformatJson(base::StringPrintf(kTemplateRequest, kImage1Url, "AQID")),
      R"({
           "results": [{
             "imageId": "0  https://www.example.com/image1.jpg",
             "engineResults": [
               {
                 "status": {},
                 "ocrEngine": {
                   "ocrRegions": [{
                     "words": [{
                       "detectedText": "1",
                       "confidenceScore": 1.0
                     }]
                   }]
                 }
               },
               {
                 "status": {},
                 "descriptionEngine": {
                   "descriptionList": {}
                 }
               }
             ]
           }]
         })",
      net::HTTP_OK);
  EXPECT_THAT(test_url_factory.requests(), IsEmpty());

  test_task_env.FastForwardBy(base::Seconds(1));

  // Now the HTTP request for image 2 should have been made.
  test_url_factory.ExpectRequestAndSimulateResponse(
      "annotation", {} /* expected_headers */,
      ReformatJson(base::StringPrintf(kTemplateRequest, kImage2Url, "BAUG")),
      R"({
           "results": [{
             "imageId": "0  https://www.example.com/image2.jpg",
             "engineResults": [
               {
                 "status": {},
                 "ocrEngine": {
                   "ocrRegions": [{
                     "words": [{
                       "detectedText": "2",
                       "confidenceScore": 1.0
                     }]
                   }]
                 }
               },
               {
                 "status": {},
                 "descriptionEngine": {
                   "descriptionList": {}
                 }
               }
             ]
           }]
         })",
      net::HTTP_OK);

  test_task_env.RunUntilIdle();

  // Annotator should have called each callback with its corresponding text.
  ASSERT_THAT(error, ElementsAre(std::nullopt, std::nullopt));
  EXPECT_THAT(annotations[0], UnorderedElementsAre(AnnotatorEq(
                                  mojom::AnnotationType::kOcr, 1.0, "1")));
  EXPECT_THAT(annotations[1], UnorderedElementsAre(AnnotatorEq(
                                  mojom::AnnotationType::kOcr, 1.0, "2")));

  // Metrics should have been logged for two server responses.
  histogram_tester.ExpectUniqueSample(metrics_internal::kServerNetError,
                                      net::Error::OK, 2);
  histogram_tester.ExpectUniqueSample(metrics_internal::kServerHttpResponseCode,
                                      net::HTTP_OK, 2);
  histogram_tester.ExpectUniqueSample(
      base::StringPrintf(metrics_internal::kAnnotationStatus, "Ocr"),
      0 /* OK RPC status */, 2);
  histogram_tester.ExpectUniqueSample(
      base::StringPrintf(metrics_internal::kAnnotationConfidence, "Ocr"), 100,
      2);
  histogram_tester.ExpectUniqueSample(
      base::StringPrintf(metrics_internal::kAnnotationEmpty, "Ocr"), false, 2);
  histogram_tester.ExpectUniqueSample(metrics_internal::kClientResult,
                                      ClientResult::kSucceeded, 2);
}

// Test that work is not duplicated if it is already ongoing.
TEST(AnnotatorTest, DuplicateWork) {
  base::test::TaskEnvironment test_task_env(
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);
  TestServerURLLoaderFactory test_url_factory(
      "https://ia-pa.googleapis.com/v1/");
  base::HistogramTester histogram_tester;

  Annotator annotator(
      GURL(kTestServerUrl), GURL(""), std::string() /* api_key */, kThrottle,
      1 /* batch_size */, 1.0 /* min_ocr_confidence */,
      test_url_factory.AsSharedURLLoaderFactory(), /*anchovy_provider=*/nullptr,
      std::make_unique<TestAnnotatorClient>());
  mojo::Remote<mojom::Annotator> remote_annotator;
  annotator.BindReceiverForTesting(
      remote_annotator.BindNewPipeAndPassReceiver());

  std::array<TestImageProcessor, 4> processor;
  std::array<std::optional<mojom::AnnotateImageError>, 4> error;
  std::array<std::vector<mojom::Annotation>, 4> annotations;

  // First request annotation of the image with processor 1.
  remote_annotator->AnnotateImage(
      kImage1Url, kDescLang, processor[0].GetPendingRemote(),
      base::BindOnce(&ReportResult, &error[0], &annotations[0]));
  test_task_env.RunUntilIdle();

  // Annotator should have asked processor 1 for the image's pixels.
  ASSERT_THAT(processor[0].callbacks(), SizeIs(1));
  ASSERT_THAT(processor[1].callbacks(), IsEmpty());
  ASSERT_THAT(processor[2].callbacks(), IsEmpty());
  ASSERT_THAT(processor[3].callbacks(), IsEmpty());

  // Now request annotation of the image with processor 2.
  remote_annotator->AnnotateImage(
      kImage1Url, kDescLang, processor[1].GetPendingRemote(),
      base::BindOnce(&ReportResult, &error[1], &annotations[1]));
  test_task_env.RunUntilIdle();

  // Annotator *should not* have asked processor 2 for the image's pixels (since
  // processor 1 is already handling that).
  ASSERT_THAT(processor[0].callbacks(), SizeIs(1));
  ASSERT_THAT(processor[1].callbacks(), IsEmpty());
  ASSERT_THAT(processor[2].callbacks(), IsEmpty());
  ASSERT_THAT(processor[3].callbacks(), IsEmpty());

  // Get processor 1 to reply with bytes for the image.
  std::move(processor[0].callbacks()[0]).Run({1, 2, 3}, kDescDim, kDescDim);
  processor[0].callbacks().pop_back();
  test_task_env.RunUntilIdle();

  // Now request annotation of the image with processor 3.
  remote_annotator->AnnotateImage(
      kImage1Url, kDescLang, processor[2].GetPendingRemote(),
      base::BindOnce(&ReportResult, &error[2], &annotations[2]));
  test_task_env.RunUntilIdle();

  // Annotator *should not* have asked processor 3 for the image's pixels (since
  // it has already has the pixels in the HTTP request queue).
  ASSERT_THAT(processor[0].callbacks(), IsEmpty());
  ASSERT_THAT(processor[1].callbacks(), IsEmpty());
  ASSERT_THAT(processor[2].callbacks(), IsEmpty());
  ASSERT_THAT(processor[3].callbacks(), IsEmpty());
  EXPECT_THAT(test_url_factory.requests(), IsEmpty());

  // Allow batch HTTP request to be sent off and then request annotation of the
  // image with processor 4.
  test_task_env.FastForwardBy(base::Seconds(1));
  EXPECT_THAT(test_url_factory.requests(), SizeIs(1));
  remote_annotator->AnnotateImage(
      kImage1Url, kDescLang, processor[3].GetPendingRemote(),
      base::BindOnce(&ReportResult, &error[3], &annotations[3]));
  test_task_env.RunUntilIdle();

  // Annotator *should not* have asked processor 4 for the image's pixels (since
  // an HTTP request for the image is already in process).
  ASSERT_THAT(processor[0].callbacks(), IsEmpty());
  ASSERT_THAT(processor[1].callbacks(), IsEmpty());
  ASSERT_THAT(processor[2].callbacks(), IsEmpty());
  ASSERT_THAT(processor[3].callbacks(), IsEmpty());

  // HTTP request for the image should have been made (with bytes obtained from
  // processor 1).
  test_url_factory.ExpectRequestAndSimulateResponse(
      "annotation", {} /* expected_headers */,
      ReformatJson(base::StringPrintf(kTemplateRequest, kImage1Url, "AQID")),
      kOcrSuccessResponse, net::HTTP_OK);
  test_task_env.RunUntilIdle();

  // Annotator should have called all callbacks with annotation results.
  ASSERT_THAT(error, ElementsAre(std::nullopt, std::nullopt, std::nullopt,
                                 std::nullopt));
  EXPECT_THAT(annotations[0],
              UnorderedElementsAre(AnnotatorEq(mojom::AnnotationType::kOcr, 1.0,
                                               "Region 1\nRegion 2")));
  EXPECT_THAT(annotations[1],
              UnorderedElementsAre(AnnotatorEq(mojom::AnnotationType::kOcr, 1.0,
                                               "Region 1\nRegion 2")));
  EXPECT_THAT(annotations[2],
              UnorderedElementsAre(AnnotatorEq(mojom::AnnotationType::kOcr, 1.0,
                                               "Region 1\nRegion 2")));
  EXPECT_THAT(annotations[3],
              UnorderedElementsAre(AnnotatorEq(mojom::AnnotationType::kOcr, 1.0,
                                               "Region 1\nRegion 2")));

  // Metrics should have been logged for a single pixel fetch.
  histogram_tester.ExpectUniqueSample(metrics_internal::kPixelFetchSuccess,
                                      true, 1);
}

// Test that the description engine is not requested for images that violate
// model policy (i.e. are too small or have too-high an aspect ratio).
TEST(AnnotatorTest, DescPolicy) {
  base::test::TaskEnvironment test_task_env(
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);
  TestServerURLLoaderFactory test_url_factory(
      "https://ia-pa.googleapis.com/v1/");
  base::HistogramTester histogram_tester;

  Annotator annotator(
      GURL(kTestServerUrl), GURL(""), std::string() /* api_key */, kThrottle,
      3 /* batch_size */, 1.0 /* min_ocr_confidence */,
      test_url_factory.AsSharedURLLoaderFactory(), /*anchovy_provider=*/nullptr,
      std::make_unique<TestAnnotatorClient>());
  mojo::Remote<mojom::Annotator> remote_annotator;
  annotator.BindReceiverForTesting(
      remote_annotator.BindNewPipeAndPassReceiver());

  std::array<TestImageProcessor, 3> processor;
  std::array<std::optional<mojom::AnnotateImageError>, 3> error;
  std::array<std::vector<mojom::Annotation>, 3> annotations;

  // Request annotation for images 1, 2 and 3.
  remote_annotator->AnnotateImage(
      kImage1Url, kDescLang, processor[0].GetPendingRemote(),
      base::BindOnce(&ReportResult, &error[0], &annotations[0]));
  remote_annotator->AnnotateImage(
      kImage2Url, kDescLang, processor[1].GetPendingRemote(),
      base::BindOnce(&ReportResult, &error[1], &annotations[1]));
  remote_annotator->AnnotateImage(
      kImage3Url, kDescLang, processor[2].GetPendingRemote(),
      base::BindOnce(&ReportResult, &error[2], &annotations[2]));
  test_task_env.RunUntilIdle();

  // Annotator should have asked processor 1 for image 1's pixels, processor
  // 2 for image 2's pixels and processor 3 for image 3's pixels.
  ASSERT_THAT(processor[0].callbacks(), SizeIs(1));
  ASSERT_THAT(processor[1].callbacks(), SizeIs(1));
  ASSERT_THAT(processor[2].callbacks(), SizeIs(1));

  // Send back image data.
  //
  // Image 1 is (just) within policy. Image 2 violates policy because it is too
  // small. Image 3 is large enough, but violates policy because of its aspect
  // ratio.
  std::move(processor[0].callbacks()[0])
      .Run({1, 2, 3}, Annotator::kDescMinDimension,
           Annotator::kDescMinDimension);
  processor[0].callbacks().pop_back();
  std::move(processor[1].callbacks()[0])
      .Run({4, 5, 6}, Annotator::kDescMinDimension,
           Annotator::kDescMinDimension - 1);
  processor[1].callbacks().pop_back();
  std::move(processor[2].callbacks()[0])
      .Run({7, 8, 9},
           static_cast<int32_t>(Annotator::kDescMinDimension *
                                Annotator::kDescMaxAspectRatio) +
               1,
           Annotator::kDescMinDimension);
  processor[2].callbacks().pop_back();
  test_task_env.RunUntilIdle();

  // No request should be sent yet (because service is waiting to batch up
  // multiple requests).
  EXPECT_THAT(test_url_factory.requests(), IsEmpty());
  test_task_env.FastForwardBy(base::Seconds(1));

  // A single HTTP request for all images should have been sent.
  test_url_factory.ExpectRequestAndSimulateResponse(
      "annotation", {} /* expected_headers */,
      // Only image 1 includes a description request (as the other two violate
      // one of the policies).
      ReformatJson(R"(
        {
          "imageRequests": [
            {
              "imageId": "0  https://www.example.com/image3.jpg",
              "imageBytes": "BwgJ",
              "engineParameters": [
                {"ocrParameters": {}}
              ]
            },
            {
              "imageId": "0  https://www.example.com/image2.jpg",
              "imageBytes": "BAUG",
              "engineParameters": [
                {"ocrParameters": {}},
                {"iconParameters": {}}
              ]
            },
            {
              "imageId": "0  https://www.example.com/image1.jpg",
              "imageBytes": "AQID",
              "engineParameters": [
                {"ocrParameters": {}},
                {"descriptionParameters": {}},
                {"iconParameters": {}}
              ]
            }
          ]
        }
      )"),
      R"(
        {
          "results": [
            {
              "imageId": "0  https://www.example.com/image2.jpg",
              "engineResults": [
                {
                  "status": {},
                  "ocrEngine": {
                    "ocrRegions": [{
                      "words": [{
                        "detectedText": "2",
                        "confidenceScore": 1.0
                      }]
                    }]
                  }
                }
              ]
            },
            {
              "imageId": "0  https://www.example.com/image1.jpg",
              "engineResults": [
                {
                  "status": {},
                  "ocrEngine": {
                    "ocrRegions": [{
                      "words": [{
                        "detectedText": "1",
                        "confidenceScore": 1.0
                      }]
                    }]
                  }
                },
                {
                  "status": {},
                  "descriptionEngine": {
                    "descriptionList": {
                      "descriptions": [{
                        "type": "CAPTION",
                        "text": "This is an example image.",
                        "score": 1.0
                      }]
                    }
                  }
                }
              ]
            },
            {
              "imageId": "0  https://www.example.com/image3.jpg",
              "engineResults": [
                {
                  "status": {},
                  "ocrEngine": {
                    "ocrRegions": [{
                      "words": [{
                        "detectedText": "3",
                        "confidenceScore": 1.0
                      }]
                    }]
                  }
                }
              ]
            }
          ]
        }
      )",
      net::HTTP_OK);
  test_task_env.RunUntilIdle();

  // Annotator should have called each callback with its corresponding results.
  ASSERT_THAT(error, ElementsAre(std::nullopt, std::nullopt, std::nullopt));
  EXPECT_THAT(
      annotations[0],
      UnorderedElementsAre(AnnotatorEq(mojom::AnnotationType::kOcr, 1.0, "1"),
                           AnnotatorEq(mojom::AnnotationType::kCaption, 1.0,
                                       "This is an example image.")));
  EXPECT_THAT(annotations[1], UnorderedElementsAre(AnnotatorEq(
                                  mojom::AnnotationType::kOcr, 1.0, "2")));
  EXPECT_THAT(annotations[2], UnorderedElementsAre(AnnotatorEq(
                                  mojom::AnnotationType::kOcr, 1.0, "3")));

  // Metrics should have been logged for the 3 OCR results and 1 description
  // result.
  EXPECT_THAT(histogram_tester.GetAllSamples(
                  metrics_internal::kImageRequestIncludesDesc),
              UnorderedElementsAre(Bucket(false, 2), Bucket(true, 1)));
  histogram_tester.ExpectUniqueSample(
      base::StringPrintf(metrics_internal::kAnnotationStatus, "Ocr"),
      0 /* OK RPC status */, 3);
  histogram_tester.ExpectUniqueSample(
      base::StringPrintf(metrics_internal::kAnnotationStatus, "Desc"),
      0 /* OK RPC status */, 1);
  histogram_tester.ExpectUniqueSample(
      base::StringPrintf(metrics_internal::kAnnotationConfidence, "Ocr"), 100,
      3);
  histogram_tester.ExpectUniqueSample(
      base::StringPrintf(metrics_internal::kAnnotationConfidence,
                         "DescCaption"),
      100, 1);
  histogram_tester.ExpectUniqueSample(
      base::StringPrintf(metrics_internal::kAnnotationEmpty, "Ocr"), false, 3);
  histogram_tester.ExpectUniqueSample(
      base::StringPrintf(metrics_internal::kAnnotationEmpty, "DescCaption"),
      false, 1);
}

// Test that description language preferences are sent to the server.
TEST(AnnotatorTest, DescLanguage) {
  base::test::TaskEnvironment test_task_env(
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);
  TestServerURLLoaderFactory test_url_factory(
      "https://ia-pa.googleapis.com/v1/");
  base::HistogramTester histogram_tester;

  Annotator annotator(
      GURL(kTestServerUrl), GURL(""), std::string() /* api_key */, kThrottle,
      3 /* batch_size */, 1.0 /* min_ocr_confidence */,
      test_url_factory.AsSharedURLLoaderFactory(), /*anchovy_provider=*/nullptr,
      std::make_unique<TestAnnotatorClient>());
  annotator.server_languages_ = {"en", "it", "fr"};
  mojo::Remote<mojom::Annotator> remote_annotator;
  annotator.BindReceiverForTesting(
      remote_annotator.BindNewPipeAndPassReceiver());

  std::array<TestImageProcessor, 3> processor;
  std::array<std::optional<mojom::AnnotateImageError>, 3> error;
  std::array<std::vector<mojom::Annotation>, 3> annotations;

  // Request annotation for one image in two languages, and one other image in
  // one language.
  remote_annotator->AnnotateImage(
      kImage1Url, "fr", processor[0].GetPendingRemote(),
      base::BindOnce(&ReportResult, &error[0], &annotations[0]));
  remote_annotator->AnnotateImage(
      kImage1Url, "it", processor[1].GetPendingRemote(),
      base::BindOnce(&ReportResult, &error[1], &annotations[1]));
  remote_annotator->AnnotateImage(
      kImage2Url, "en", processor[2].GetPendingRemote(),
      base::BindOnce(&ReportResult, &error[2], &annotations[2]));
  test_task_env.RunUntilIdle();

  // Annotator should have asked processor 1 and 2 for image 1's pixels and
  // processor 3 for image 2's pixels.
  ASSERT_THAT(processor[0].callbacks(), SizeIs(1));
  ASSERT_THAT(processor[1].callbacks(), SizeIs(1));
  ASSERT_THAT(processor[2].callbacks(), SizeIs(1));

  // Send back image data. Image 2 is out of policy.
  std::move(processor[0].callbacks()[0]).Run({1, 2, 3}, kDescDim, kDescDim);
  processor[0].callbacks().pop_back();
  std::move(processor[1].callbacks()[0]).Run({1, 2, 3}, kDescDim, kDescDim);
  processor[1].callbacks().pop_back();
  std::move(processor[2].callbacks()[0])
      .Run({4, 5, 6}, Annotator::kDescMinDimension - 1,
           Annotator::kDescMinDimension);
  processor[2].callbacks().pop_back();
  test_task_env.RunUntilIdle();

  // No request should be sent yet (because service is waiting to batch up
  // multiple requests).
  EXPECT_THAT(test_url_factory.requests(), IsEmpty());
  test_task_env.FastForwardBy(base::Seconds(1));

  // A single HTTP request for all images should have been sent.
  test_url_factory.ExpectRequestAndSimulateResponse(
      "annotation", {} /* expected_headers */,
      // Image requests should include the preferred language in their image IDs
      // and description parameters (except for image 2 which should not include
      // description parameters).
      ReformatJson(R"(
        {
          "imageRequests": [
            {
              "imageId": "0 en https://www.example.com/image2.jpg",
              "imageBytes": "BAUG",
              "engineParameters": [
                {"ocrParameters": {}},
                {"iconParameters": {}}
              ]
            },
            {
              "imageId": "0 it https://www.example.com/image1.jpg",
              "imageBytes": "AQID",
              "engineParameters": [
                {"ocrParameters": {}},
                {
                  "descriptionParameters": {
                    "preferredLanguages": ["it"]
                  }
                },
                {"iconParameters": {}}
              ]
            },
            {
              "imageId": "0 fr https://www.example.com/image1.jpg",
              "imageBytes": "AQID",
              "engineParameters": [
                {"ocrParameters": {}},
                {
                  "descriptionParameters": {
                    "preferredLanguages": ["fr"]
                  }
                },
                {"iconParameters": {}}
              ]
            }
          ]
        }
      )"),
      R"(
        {
          "results": [
            {
              "imageId": "0 it https://www.example.com/image1.jpg",
              "engineResults": [
                {
                  "status": {},
                  "ocrEngine": {
                    "ocrRegions": [{
                      "words": [{
                        "detectedText": "1",
                        "confidenceScore": 1.0
                      }]
                    }]
                  }
                },
                {
                  "status": {},
                  "descriptionEngine": {
                    "descriptionList": {
                      "descriptions": [{
                        "type": "CAPTION",
                        "text": "This is an example image.",
                        "score": 1.0
                      }]
                    }
                  }
                }
              ]
            },
            {
              "imageId": "0 fr https://www.example.com/image1.jpg",
              "engineResults": [
                {
                  "status": {},
                  "ocrEngine": {
                    "ocrRegions": [{
                      "words": [{
                        "detectedText": "1",
                        "confidenceScore": 1.0
                      }]
                    }]
                  }
                },
                {
                  "status": {},
                  "descriptionEngine": {
                    "descriptionList": {
                      "descriptions": [{
                        "type": "CAPTION",
                        "text": "Ceci est un exemple d'image.",
                        "score": 1.0
                      }]
                    }
                  }
                }
              ]
            },
            {
              "imageId": "0 en https://www.example.com/image2.jpg",
              "engineResults": [
                {
                  "status": {},
                  "ocrEngine": {
                    "ocrRegions": [{
                      "words": [{
                        "detectedText": "2",
                        "confidenceScore": 1.0
                      }]
                    }]
                  }
                }
              ]
            }
          ]
        }
      )",
      net::HTTP_OK);
  test_task_env.RunUntilIdle();

  // Annotator should have called each callback with its corresponding results.
  ASSERT_THAT(error, ElementsAre(std::nullopt, std::nullopt, std::nullopt));
  EXPECT_THAT(
      annotations[0],
      UnorderedElementsAre(AnnotatorEq(mojom::AnnotationType::kOcr, 1.0, "1"),
                           AnnotatorEq(mojom::AnnotationType::kCaption, 1.0,
                                       "Ceci est un exemple d'image.")));
  EXPECT_THAT(
      annotations[1],
      UnorderedElementsAre(AnnotatorEq(mojom::AnnotationType::kOcr, 1.0, "1"),
                           AnnotatorEq(mojom::AnnotationType::kCaption, 1.0,
                                       "This is an example image.")));
  EXPECT_THAT(annotations[2], UnorderedElementsAre(AnnotatorEq(
                                  mojom::AnnotationType::kOcr, 1.0, "2")));
}

// Test that annotation works properly when we need to fall back on a
// different language because the page language isn't available.
TEST(AnnotatorTest, LanguageFallback) {
  base::test::TaskEnvironment test_task_env(
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);
  TestServerURLLoaderFactory test_url_factory(
      "https://ia-pa.googleapis.com/v1/");
  base::HistogramTester histogram_tester;

  Annotator annotator(
      GURL(kTestServerUrl), GURL(""), std::string() /* api_key */, kThrottle,
      1 /* batch_size */, 1.0 /* min_ocr_confidence */,
      test_url_factory.AsSharedURLLoaderFactory(), /*anchovy_provider=*/nullptr,
      std::make_unique<TestAnnotatorClient>());
  annotator.server_languages_ = {"en", "it", "fr"};
  mojo::Remote<mojom::Annotator> remote_annotator;
  annotator.BindReceiverForTesting(
      remote_annotator.BindNewPipeAndPassReceiver());

  TestImageProcessor processor;
  std::optional<mojom::AnnotateImageError> error;
  std::vector<mojom::Annotation> annotations;

  // Send a request in an unsupported language.
  remote_annotator->AnnotateImage(
      kImage1Url, "hu", processor.GetPendingRemote(),
      base::BindOnce(&ReportResult, &error, &annotations));
  test_task_env.RunUntilIdle();

  // Send back image data.
  std::move(processor.callbacks()[0]).Run({1, 2, 3}, kDescDim, kDescDim);
  processor.callbacks().pop_back();
  test_task_env.RunUntilIdle();

  // Fast-forward time so that server sends batch.
  EXPECT_THAT(test_url_factory.requests(), IsEmpty());
  test_task_env.FastForwardBy(base::Seconds(1));

  // A single HTTP request for all images should have been sent.
  test_url_factory.ExpectRequestAndSimulateResponse(
      "annotation", {} /* expected_headers */, ReformatJson(R"(
        {
          "imageRequests": [
            {
              "imageId": "0 en https://www.example.com/image1.jpg",
              "imageBytes": "AQID",
              "engineParameters": [
                {"ocrParameters": {}},
                {
                  "descriptionParameters": {
                    "preferredLanguages": ["en"]
                  }
                },
                {"iconParameters": {}}
              ]
            }
          ]
        }
      )"),
      R"(
        {
          "results": [
            {
              "imageId": "0 en https://www.example.com/image1.jpg",
              "engineResults": [
                {
                  "status": {},
                  "ocrEngine": {
                    "ocrRegions": [{
                      "words": [{
                        "detectedText": "1",
                        "confidenceScore": 1.0
                      }]
                    }]
                  }
                },
                {
                  "status": {},
                  "descriptionEngine": {
                    "descriptionList": {
                      "descriptions": [{
                        "type": "CAPTION",
                        "text": "Result in fallback language.",
                        "score": 1.0
                      }]
                    }
                  }
                }
              ]
            }
          ]
        }
      )",
      net::HTTP_OK);
  test_task_env.RunUntilIdle();

  // Annotator should have called each callback with its corresponding results.
  ASSERT_EQ(error, std::nullopt);
  EXPECT_THAT(
      annotations,
      UnorderedElementsAre(AnnotatorEq(mojom::AnnotationType::kOcr, 1.0, "1"),
                           AnnotatorEq(mojom::AnnotationType::kCaption, 1.0,
                                       "Result in fallback language.")));
}

// Test that the specified API key is sent, but only to Google-associated server
// domains.
TEST(AnnotatorTest, ApiKey) {
  base::test::TaskEnvironment test_task_env(
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);

  // A call to a secure Google-owner server URL should include the specified API
  // key.
  {
    TestServerURLLoaderFactory test_url_factory(
        "https://ia-pa.googleapis.com/v1/");

    Annotator annotator(GURL(kTestServerUrl), GURL(""), "my_api_key", kThrottle,
                        1 /* batch_size */, 1.0 /* min_ocr_confidence */,
                        test_url_factory.AsSharedURLLoaderFactory(),
                        /*anchovy_provider=*/nullptr,
                        std::make_unique<TestAnnotatorClient>());
    mojo::Remote<mojom::Annotator> remote_annotator;
    annotator.BindReceiverForTesting(
        remote_annotator.BindNewPipeAndPassReceiver());
    TestImageProcessor processor;

    remote_annotator->AnnotateImage(
        kImage1Url, kDescLang, processor.GetPendingRemote(), base::DoNothing());
    test_task_env.RunUntilIdle();

    // Annotator should have asked processor for pixels.
    ASSERT_THAT(processor.callbacks(), SizeIs(1));

    // Send back image data.
    std::move(processor.callbacks()[0]).Run({1, 2, 3}, kDescDim, kDescDim);
    processor.callbacks().pop_back();
    test_task_env.FastForwardBy(base::Seconds(1));
    test_task_env.RunUntilIdle();

    // HTTP request should have been made with the API key included.
    test_url_factory.ExpectRequestAndSimulateResponse(
        "annotation", {{Annotator::kGoogApiKeyHeader, "my_api_key"}},
        ReformatJson(base::StringPrintf(kTemplateRequest, kImage1Url, "AQID")),
        kOcrSuccessResponse, net::HTTP_OK);
    test_task_env.RunUntilIdle();
  }

  // A call to a Google-owned server URL should not include the API key if the
  // requests are made insecurely.
  {
    // Note: not HTTPS.
    TestServerURLLoaderFactory test_url_factory(
        "http://ia-pa.googleapis.com/v1/");

    Annotator annotator(GURL("http://ia-pa.googleapis.com/v1/annotation"),
                        GURL(""), "my_api_key", kThrottle, 1 /* batch_size */,
                        1.0 /* min_ocr_confidence */,
                        test_url_factory.AsSharedURLLoaderFactory(),
                        /*anchovy_provider=*/nullptr,
                        std::make_unique<TestAnnotatorClient>());
    mojo::Remote<mojom::Annotator> remote_annotator;
    annotator.BindReceiverForTesting(
        remote_annotator.BindNewPipeAndPassReceiver());
    TestImageProcessor processor;

    remote_annotator->AnnotateImage(
        kImage1Url, kDescLang, processor.GetPendingRemote(), base::DoNothing());
    test_task_env.RunUntilIdle();

    // Annotator should have asked processor for pixels.
    ASSERT_THAT(processor.callbacks(), SizeIs(1));

    // Send back image data.
    std::move(processor.callbacks()[0]).Run({1, 2, 3}, kDescDim, kDescDim);
    processor.callbacks().pop_back();
    test_task_env.FastForwardBy(base::Seconds(1));
    test_task_env.RunUntilIdle();

    // HTTP request should have been made without the API key included.
    test_url_factory.ExpectRequestAndSimulateResponse(
        "annotation", {{Annotator::kGoogApiKeyHeader, std::nullopt}},
        ReformatJson(base::StringPrintf(kTemplateRequest, kImage1Url, "AQID")),
        kOcrSuccessResponse, net::HTTP_OK);
    test_task_env.RunUntilIdle();
  }

  // A call to a non-Google-owned URL should not include the API key.
  {
    TestServerURLLoaderFactory test_url_factory("https://datascraper.com/");

    Annotator annotator(
        GURL("https://datascraper.com/annotation"), GURL(""), "my_api_key",
        kThrottle, 1 /* batch_size */, 1.0 /* min_ocr_confidence */,
        test_url_factory.AsSharedURLLoaderFactory(),
        /*anchovy_provider=*/nullptr, std::make_unique<TestAnnotatorClient>());
    mojo::Remote<mojom::Annotator> remote_annotator;
    annotator.BindReceiverForTesting(
        remote_annotator.BindNewPipeAndPassReceiver());
    TestImageProcessor processor;

    remote_annotator->AnnotateImage(
        kImage1Url, kDescLang, processor.GetPendingRemote(), base::DoNothing());
    test_task_env.RunUntilIdle();

    // Annotator should have asked processor for pixels.
    ASSERT_THAT(processor.callbacks(), SizeIs(1));

    // Send back image data.
    std::move(processor.callbacks()[0]).Run({1, 2, 3}, kDescDim, kDescDim);
    processor.callbacks().pop_back();
    test_task_env.FastForwardBy(base::Seconds(1));
    test_task_env.RunUntilIdle();

    // HTTP request should have been made without the API key included.
    test_url_factory.ExpectRequestAndSimulateResponse(
        "annotation", {{Annotator::kGoogApiKeyHeader, std::nullopt}},
        ReformatJson(base::StringPrintf(kTemplateRequest, kImage1Url, "AQID")),
        kOcrSuccessResponse, net::HTTP_OK);
    test_task_env.RunUntilIdle();
  }
}

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

  Annotator annotator(
      GURL(kTestServerUrl), GURL(kLangsServerUrl), std::string() /* api_key */,
      kThrottle, 1 /* batch_size */, 1.0 /* min_ocr_confidence */,
      test_url_factory.AsSharedURLLoaderFactory(), /*anchovy_provider=*/nullptr,
      std::make_unique<TestAnnotatorClient>());

  // Assert that initially server_languages_ doesn't contain the made-up
  // language code zz.
  EXPECT_FALSE(std::ranges::contains(annotator.server_languages_, "zz"));

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

  EXPECT_TRUE(std::ranges::contains(annotator.server_languages_, "zz"));
}

// If the server langs don't contain English, they're ignored.
TEST(AnnotatorTest, ServerLanguagesMustContainEnglish) {
  base::test::TaskEnvironment test_task_env(
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);
  TestServerURLLoaderFactory test_url_factory(
      "https://ia-pa.googleapis.com/v1/");

  Annotator annotator(
      GURL(kTestServerUrl), GURL(kLangsServerUrl), std::string() /* api_key */,
      kThrottle, 1 /* batch_size */, 1.0 /* min_ocr_confidence */,
      test_url_factory.AsSharedURLLoaderFactory(), /*anchovy_provider=*/nullptr,
      std::make_unique<TestAnnotatorClient>());

  // Assert that initially server_languages_ does contain "en" but
  // doesn't contain the made-up language code zz.
  EXPECT_FALSE(std::ranges::contains(annotator.server_languages_, "zz"));

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
  EXPECT_TRUE(std::ranges::contains(annotator.server_languages_, "en"));
  EXPECT_FALSE(std::ranges::contains(annotator.server_languages_, "zz"));
}

// Alternative Routing Tests.

class FakeAnchovyProvider : public manta::AnchovyProvider {
 public:
  explicit FakeAnchovyProvider(
      base::DictValue fake_result,
      manta::MantaStatusCode status_code = manta::MantaStatusCode::kOk)
      : manta::AnchovyProvider(nullptr, nullptr, {}),
        fake_result_(std::move(fake_result)),
        status_code_(status_code) {}

  FakeAnchovyProvider(const FakeAnchovyProvider&) = delete;
  FakeAnchovyProvider& operator=(const FakeAnchovyProvider&) = delete;

  void GetImageDescription(manta::anchovy::ImageDescriptionRequest& request,
                           net::NetworkTrafficAnnotationTag traffic_annotation,
                           manta::MantaGenericCallback callback) override {
    ++request_count_;
    manta::MantaStatus status;
    status.status_code = status_code_;
    status.locale = "en";
    status.message =
        status_code_ == manta::MantaStatusCode::kOk ? "ok" : "error";
    std::move(callback).Run(fake_result_.Clone(), status);
  }

  void SetFakeResult(base::DictValue fake_result,
                     manta::MantaStatusCode status_code) {
    fake_result_ = std::move(fake_result);
    status_code_ = status_code;
  }

  int request_count() const { return request_count_; }

 private:
  base::DictValue fake_result_;
  manta::MantaStatusCode status_code_;
  int request_count_ = 0;
};

void RunAnchovyAnnotatorTest(
    std::unique_ptr<FakeAnchovyProvider> fake_provider,
    std::vector<mojom::Annotation>* annotations,
    std::optional<mojom::AnnotateImageError>* out_error = nullptr,
    int32_t image_dim = kDescDim,
    int* out_request_count = nullptr) {
  base::test::ScopedFeatureList enabled_features;
  enabled_features.InitAndEnableFeature(
      features::kImageDescriptionsAlternateRouting);

  base::test::TaskEnvironment test_task_env(
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);
  TestServerURLLoaderFactory test_url_factory(
      "https://ia-pa.googleapis.com/v1/");

  FakeAnchovyProvider* provider_ptr = fake_provider.get();
  Annotator annotator(
      GURL(kTestServerUrl), GURL(""), std::string() /* api_key */, kThrottle,
      1 /* batch_size */, 0.7 /* min_ocr_confidence */,
      test_url_factory.AsSharedURLLoaderFactory(), std::move(fake_provider),
      std::make_unique<TestAnnotatorClient>());
  mojo::Remote<mojom::Annotator> remote_annotator;
  annotator.BindReceiverForTesting(
      remote_annotator.BindNewPipeAndPassReceiver());
  TestImageProcessor processor;

  std::optional<mojom::AnnotateImageError> local_error;
  std::optional<mojom::AnnotateImageError>* error_ptr =
      out_error ? out_error : &local_error;

  base::RunLoop run_loop;
  remote_annotator->AnnotateImage(
      kImage1Url, kDescLang, processor.GetPendingRemote(),
      base::BindOnce(&ReportResult, error_ptr, annotations)
          .Then(run_loop.QuitClosure()));

  // Annotator should have asked processor for pixels.
  processor.WaitForGetJpgImageData();
  ASSERT_THAT(processor.callbacks(), SizeIs(1));

  // Send back image data.
  std::move(processor.callbacks()[0]).Run({1, 2, 3}, image_dim, image_dim);
  processor.callbacks().pop_back();

  test_task_env.FastForwardBy(base::Seconds(1));
  run_loop.Run();
  // No HTTP request should be sent to the legacy endpoint.
  EXPECT_THAT(test_url_factory.requests(), IsEmpty());

  if (out_request_count && provider_ptr) {
    *out_request_count = provider_ptr->request_count();
  }
}

void SimpleAnchovySuccessTest(std::string str_type,
                              mojom::AnnotationType expected_type) {
  const std::string best_text = "best";
  const std::string type = str_type;
  const double best_score = 0.9;
  const std::string other_text = "other";
  const double other_score = 0.8;

  base::ListValue results;
  results.Append(base::DictValue()
                     .Set("type", type)
                     .Set("score", best_score)
                     .Set("text", best_text));
  results.Append(base::DictValue()
                     .Set("type", type)
                     .Set("score", other_score)
                     .Set("text", other_text));

  std::vector<mojom::Annotation> annotations;
  RunAnchovyAnnotatorTest(
      std::make_unique<FakeAnchovyProvider>(
          base::DictValue().Set("results", std::move(results))),
      &annotations);

  EXPECT_FALSE(annotations.empty());
  EXPECT_EQ(1u, annotations.size());
  auto annotation = annotations[0];
  EXPECT_EQ(annotation.text, best_text);
  EXPECT_EQ(annotation.score, best_score);
  EXPECT_EQ(annotation.type, expected_type);
}

TEST(AnnotatorTest, EmptyResultIfDictIsEmpty) {
  std::vector<mojom::Annotation> annotations;
  RunAnchovyAnnotatorTest(
      std::make_unique<FakeAnchovyProvider>(base::DictValue()), &annotations);
  EXPECT_TRUE(annotations.empty());
}

TEST(AnnotatorTest, EmptyResultIfListIsEmpty) {
  std::vector<mojom::Annotation> annotations;
  RunAnchovyAnnotatorTest(
      std::make_unique<FakeAnchovyProvider>(
          base::DictValue().Set("results", base::ListValue())),
      &annotations);
  EXPECT_TRUE(annotations.empty());
}

TEST(AnnotatorTest, AnchovySuccessMultiple) {
  const std::string text_ocr = "ocr";
  const std::string type_ocr(manta::anchovy::kTypeOcr);
  const double score = 0.9;
  const std::string text_caption = "caption";
  const std::string type_caption(manta::anchovy::kTypePrimaryCaption);

  base::ListValue results;
  results.Append(base::DictValue()
                     .Set("type", type_ocr)
                     .Set("score", score)
                     .Set("text", text_ocr));
  results.Append(base::DictValue()
                     .Set("type", type_caption)
                     .Set("score", score)
                     .Set("text", text_caption));

  std::vector<mojom::Annotation> annotations;
  RunAnchovyAnnotatorTest(
      std::make_unique<FakeAnchovyProvider>(
          base::DictValue().Set("results", std::move(results))),
      &annotations);

  EXPECT_FALSE(annotations.empty());
  EXPECT_EQ(2u, annotations.size());
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
  SimpleAnchovySuccessTest(manta::anchovy::kTypeOcr,
                           mojom::AnnotationType::kOcr);
}

TEST(AnnotatorTest, AnchovySuccessCaption) {
  SimpleAnchovySuccessTest(manta::anchovy::kTypeCaption,
                           mojom::AnnotationType::kCaption);
}

TEST(AnnotatorTest, AnchovySuccessPrimaryCaption) {
  SimpleAnchovySuccessTest(manta::anchovy::kTypePrimaryCaption,
                           mojom::AnnotationType::kCaption);
}

TEST(AnnotatorTest, AnchovySuccessLabel) {
  SimpleAnchovySuccessTest(manta::anchovy::kTypeLabel,
                           mojom::AnnotationType::kLabel);
}

TEST(AnnotatorTest, AnchovyClampsUncalibratedScoreAboveOne) {
  base::ListValue results;
  results.Append(base::DictValue()
                     .Set("type", manta::anchovy::kTypeCaption)
                     .Set("score", 1.2)
                     .Set("text", "A trail"));
  results.Append(base::DictValue()
                     .Set("type", manta::anchovy::kTypePrimaryCaption)
                     .Set("score", 1.45)
                     .Set("text", "A scenic mountain trail"));

  std::vector<mojom::Annotation> annotations;
  RunAnchovyAnnotatorTest(
      std::make_unique<FakeAnchovyProvider>(
          base::DictValue().Set("results", std::move(results))),
      &annotations);

  ASSERT_EQ(annotations.size(), 1u);
  EXPECT_EQ(annotations[0].type, mojom::AnnotationType::kCaption);
  EXPECT_EQ(annotations[0].text, "A scenic mountain trail");
  EXPECT_DOUBLE_EQ(annotations[0].score, 1.0);
}

TEST(AnnotatorTest, AnchovyFailsIfNoText) {
  base::ListValue results;
  results.Append(base::DictValue()
                     .Set("type", manta::anchovy::kTypeOcr)
                     .Set("score", 0.9));

  std::vector<mojom::Annotation> annotations;
  std::optional<mojom::AnnotateImageError> error;
  RunAnchovyAnnotatorTest(
      std::make_unique<FakeAnchovyProvider>(
          base::DictValue().Set("results", std::move(results))),
      &annotations, &error);
  EXPECT_TRUE(annotations.empty());
  EXPECT_EQ(error, mojom::AnnotateImageError::kFailure);
}

TEST(AnnotatorTest, CrashIfNoAnchovyProvider) {
  EXPECT_DEATH_IF_SUPPORTED(RunAnchovyAnnotatorTest(nullptr, {}), "");
}

// Verifies that a non-OK MantaStatusCode resolves pending_requests_ with
// kFailure (even if the response dict is non-empty) so a retry for the same
// image succeeds.
TEST(AnnotatorTest, AnchovyNonOkStatusResolvesPendingRequest) {
  base::test::ScopedFeatureList enabled_features;
  enabled_features.InitAndEnableFeature(
      features::kImageDescriptionsAlternateRouting);

  base::test::TaskEnvironment test_task_env(
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);
  TestServerURLLoaderFactory test_url_factory(
      "https://ia-pa.googleapis.com/v1/");

  base::ListValue valid_results;
  valid_results.Append(base::DictValue()
                           .Set("type", manta::anchovy::kTypePrimaryCaption)
                           .Set("score", 0.9)
                           .Set("text", "A dog in a park"));
  auto fake_provider = std::make_unique<FakeAnchovyProvider>(
      base::DictValue().Set("results", valid_results.Clone()),
      manta::MantaStatusCode::kBackendFailure);
  FakeAnchovyProvider* provider_ptr = fake_provider.get();

  Annotator annotator(
      GURL(kTestServerUrl), GURL(""), std::string() /* api_key */, kThrottle,
      1 /* batch_size */, 0.7 /* min_ocr_confidence */,
      test_url_factory.AsSharedURLLoaderFactory(), std::move(fake_provider),
      std::make_unique<TestAnnotatorClient>());
  mojo::Remote<mojom::Annotator> remote_annotator;
  annotator.BindReceiverForTesting(
      remote_annotator.BindNewPipeAndPassReceiver());

  // First request fails with kBackendFailure.
  TestImageProcessor processor1;
  std::optional<mojom::AnnotateImageError> error1;
  std::vector<mojom::Annotation> annotations1;
  base::RunLoop run_loop1;
  remote_annotator->AnnotateImage(
      kImage1Url, kDescLang, processor1.GetPendingRemote(),
      base::BindOnce(&ReportResult, &error1, &annotations1)
          .Then(run_loop1.QuitClosure()));
  processor1.WaitForGetJpgImageData();
  ASSERT_THAT(processor1.callbacks(), SizeIs(1));
  std::move(processor1.callbacks()[0]).Run({1, 2, 3}, kDescDim, kDescDim);
  processor1.callbacks().pop_back();
  test_task_env.FastForwardBy(base::Seconds(1));
  run_loop1.Run();

  EXPECT_EQ(provider_ptr->request_count(), 1);
  EXPECT_EQ(error1, mojom::AnnotateImageError::kFailure);
  EXPECT_TRUE(annotations1.empty());

  // Retry for the same image should not be stuck in pending_requests_ or served
  // from cached_results_.
  provider_ptr->SetFakeResult(
      base::DictValue().Set("results", std::move(valid_results)),
      manta::MantaStatusCode::kOk);
  TestImageProcessor processor2;
  std::optional<mojom::AnnotateImageError> error2;
  std::vector<mojom::Annotation> annotations2;
  base::RunLoop run_loop2;
  remote_annotator->AnnotateImage(
      kImage1Url, kDescLang, processor2.GetPendingRemote(),
      base::BindOnce(&ReportResult, &error2, &annotations2)
          .Then(run_loop2.QuitClosure()));
  processor2.WaitForGetJpgImageData();
  ASSERT_THAT(processor2.callbacks(), SizeIs(1));
  std::move(processor2.callbacks()[0]).Run({1, 2, 3}, kDescDim, kDescDim);
  processor2.callbacks().pop_back();
  test_task_env.FastForwardBy(base::Seconds(1));
  run_loop2.Run();

  EXPECT_EQ(provider_ptr->request_count(), 2);
  EXPECT_EQ(error2, std::nullopt);
  ASSERT_EQ(annotations2.size(), 1u);
  EXPECT_EQ(annotations2[0].text, "A dog in a park");
}

// Verifies that unknown description types, malformed dictionary entries, and
// low-confidence OCR results do not crash and are not cached as kSucceeded.
TEST(AnnotatorTest, AnchovyUnknownTypeAndFilteredEntriesNotCached) {
  base::test::ScopedFeatureList enabled_features;
  enabled_features.InitAndEnableFeature(
      features::kImageDescriptionsAlternateRouting);

  base::test::TaskEnvironment test_task_env(
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);
  TestServerURLLoaderFactory test_url_factory(
      "https://ia-pa.googleapis.com/v1/");

  base::ListValue invalid_results;
  invalid_results.Append(
      base::DictValue().Set("type", "").Set("score", 0.9).Set("text", "empty"));
  invalid_results.Append(base::DictValue()
                             .Set("type", "UNKNOWN")
                             .Set("score", 0.9)
                             .Set("text", "unknown"));
  invalid_results.Append(base::DictValue()
                             .Set("type", manta::anchovy::kTypePrimaryCaption)
                             .Set("score", 0.9)
                             .Set("text", ""));
  invalid_results.Append(base::DictValue()
                             .Set("type", manta::anchovy::kTypeOcr)
                             .Set("score", 0.5)
                             .Set("text", "low confidence ocr"));

  auto fake_provider = std::make_unique<FakeAnchovyProvider>(
      base::DictValue().Set("results", std::move(invalid_results)));
  FakeAnchovyProvider* provider_ptr = fake_provider.get();

  Annotator annotator(
      GURL(kTestServerUrl), GURL(""), std::string() /* api_key */, kThrottle,
      1 /* batch_size */, 0.7 /* min_ocr_confidence */,
      test_url_factory.AsSharedURLLoaderFactory(), std::move(fake_provider),
      std::make_unique<TestAnnotatorClient>());
  mojo::Remote<mojom::Annotator> remote_annotator;
  annotator.BindReceiverForTesting(
      remote_annotator.BindNewPipeAndPassReceiver());

  TestImageProcessor processor1;
  std::optional<mojom::AnnotateImageError> error1;
  std::vector<mojom::Annotation> annotations1;
  base::RunLoop run_loop1;
  remote_annotator->AnnotateImage(
      kImage1Url, kDescLang, processor1.GetPendingRemote(),
      base::BindOnce(&ReportResult, &error1, &annotations1)
          .Then(run_loop1.QuitClosure()));
  processor1.WaitForGetJpgImageData();
  ASSERT_THAT(processor1.callbacks(), SizeIs(1));
  std::move(processor1.callbacks()[0]).Run({1, 2, 3}, kDescDim, kDescDim);
  processor1.callbacks().pop_back();
  test_task_env.FastForwardBy(base::Seconds(1));
  run_loop1.Run();

  EXPECT_EQ(provider_ptr->request_count(), 1);
  EXPECT_EQ(error1, mojom::AnnotateImageError::kFailure);
  EXPECT_TRUE(annotations1.empty());

  // Because all entries were filtered out, the empty result must not be cached.
  base::ListValue valid_results;
  valid_results.Append(base::DictValue()
                           .Set("type", manta::anchovy::kTypePrimaryCaption)
                           .Set("score", 0.85)
                           .Set("text", "Valid caption on retry"));
  provider_ptr->SetFakeResult(
      base::DictValue().Set("results", std::move(valid_results)),
      manta::MantaStatusCode::kOk);

  TestImageProcessor processor2;
  std::optional<mojom::AnnotateImageError> error2;
  std::vector<mojom::Annotation> annotations2;
  base::RunLoop run_loop2;
  remote_annotator->AnnotateImage(
      kImage1Url, kDescLang, processor2.GetPendingRemote(),
      base::BindOnce(&ReportResult, &error2, &annotations2)
          .Then(run_loop2.QuitClosure()));
  processor2.WaitForGetJpgImageData();
  ASSERT_THAT(processor2.callbacks(), SizeIs(1));
  std::move(processor2.callbacks()[0]).Run({1, 2, 3}, kDescDim, kDescDim);
  processor2.callbacks().pop_back();
  test_task_env.FastForwardBy(base::Seconds(1));
  run_loop2.Run();

  EXPECT_EQ(provider_ptr->request_count(), 2);
  EXPECT_EQ(error2, std::nullopt);
  ASSERT_EQ(annotations2.size(), 1u);
  EXPECT_EQ(annotations2[0].text, "Valid caption on retry");
}

// Verifies that PRIMARY_CAPTION and SECONDARY_CAPTION are joined with "\n" and
// an averaged score, preferred over a higher-scoring LABEL, and logged once in
// UMA histograms.
TEST(AnnotatorTest,
     AnchovyPrimaryAndSecondaryCaptionJoinedAndPreferredOverLabel) {
  base::HistogramTester histogram_tester;
  base::ListValue results;
  results.Append(base::DictValue()
                     .Set("type", manta::anchovy::kTypeLabel)
                     .Set("score", 0.96)
                     .Set("text", "bicycle"));
  results.Append(base::DictValue()
                     .Set("type", manta::anchovy::kTypePrimaryCaption)
                     .Set("score", 0.84)
                     .Set("text", "A person riding a bicycle"));
  results.Append(base::DictValue()
                     .Set("type", manta::anchovy::kTypeSecondaryCaption)
                     .Set("score", 0.76)
                     .Set("text", "Golden Gate Park"));

  std::vector<mojom::Annotation> annotations;
  RunAnchovyAnnotatorTest(
      std::make_unique<FakeAnchovyProvider>(
          base::DictValue().Set("results", std::move(results))),
      &annotations);

  ASSERT_EQ(annotations.size(), 1u);
  EXPECT_EQ(annotations[0].type, mojom::AnnotationType::kCaption);
  EXPECT_EQ(annotations[0].text, "A person riding a bicycle\nGolden Gate Park");
  EXPECT_NEAR(annotations[0].score, 0.80, 1e-5);

  histogram_tester.ExpectUniqueSample(metrics_internal::kDescType,
                                      mojom::AnnotationType::kCaption, 1);
  histogram_tester.ExpectUniqueSample(
      base::StringPrintf(metrics_internal::kAnnotationConfidence,
                         "DescCaption"),
      80, 1);
  histogram_tester.ExpectUniqueSample(
      base::StringPrintf(metrics_internal::kAnnotationEmpty, "DescCaption"),
      false, 1);
  histogram_tester.ExpectTotalCount(
      base::StringPrintf(metrics_internal::kAnnotationConfidence, "DescLabel"),
      0);
}

// Verifies that SECONDARY_CAPTION preceding PRIMARY_CAPTION still joins in
// primary-then-secondary order and clamps uncalibrated scores > 1.0 before
// averaging.
TEST(AnnotatorTest,
     AnchovySecondaryBeforePrimaryCaptionClampedBeforeAveraging) {
  base::HistogramTester histogram_tester;
  base::ListValue results;
  results.Append(base::DictValue()
                     .Set("type", manta::anchovy::kTypeSecondaryCaption)
                     .Set("score", 0.20)
                     .Set("text", "Lower-scoring secondary"));
  results.Append(base::DictValue()
                     .Set("type", manta::anchovy::kTypeSecondaryCaption)
                     .Set("score", 0.50)
                     .Set("text", "Yosemite Valley"));
  results.Append(base::DictValue()
                     .Set("type", manta::anchovy::kTypePrimaryCaption)
                     .Set("score", 1.50)
                     .Set("text", "A scenic mountain trail"));

  std::vector<mojom::Annotation> annotations;
  RunAnchovyAnnotatorTest(
      std::make_unique<FakeAnchovyProvider>(
          base::DictValue().Set("results", std::move(results))),
      &annotations);

  ASSERT_EQ(annotations.size(), 1u);
  EXPECT_EQ(annotations[0].type, mojom::AnnotationType::kCaption);
  EXPECT_EQ(annotations[0].text, "A scenic mountain trail\nYosemite Valley");
  EXPECT_DOUBLE_EQ(annotations[0].score, 0.75);

  histogram_tester.ExpectUniqueSample(metrics_internal::kDescType,
                                      mojom::AnnotationType::kCaption, 1);
  histogram_tester.ExpectUniqueSample(
      base::StringPrintf(metrics_internal::kAnnotationConfidence,
                         "DescCaption"),
      75, 1);
}

// Verifies that an orphan SECONDARY_CAPTION without a PRIMARY_CAPTION is never
// selected as a standalone caption or logged as DescCaption, falling back to
// LABEL when present or failing when no PRIMARY_CAPTION or LABEL is present.
TEST(AnnotatorTest, AnchovyOrphanSecondaryCaptionRejected) {
  // Case 1: Orphan SECONDARY_CAPTION + LABEL -> falls back to LABEL.
  {
    base::HistogramTester histogram_tester;
    base::ListValue results;
    results.Append(base::DictValue()
                       .Set("type", manta::anchovy::kTypeSecondaryCaption)
                       .Set("score", 0.90)
                       .Set("text", "Los Angeles"));
    results.Append(base::DictValue()
                       .Set("type", manta::anchovy::kTypeLabel)
                       .Set("score", 0.80)
                       .Set("text", "city"));

    std::vector<mojom::Annotation> annotations;
    RunAnchovyAnnotatorTest(
        std::make_unique<FakeAnchovyProvider>(
            base::DictValue().Set("results", std::move(results))),
        &annotations);

    ASSERT_EQ(annotations.size(), 1u);
    EXPECT_EQ(annotations[0].type, mojom::AnnotationType::kLabel);
    EXPECT_EQ(annotations[0].text, "city");
    EXPECT_NEAR(annotations[0].score, 0.80, 1e-5);

    histogram_tester.ExpectUniqueSample(metrics_internal::kDescType,
                                        mojom::AnnotationType::kLabel, 1);
    histogram_tester.ExpectUniqueSample(
        base::StringPrintf(metrics_internal::kAnnotationConfidence,
                           "DescLabel"),
        80, 1);
    histogram_tester.ExpectTotalCount(
        base::StringPrintf(metrics_internal::kAnnotationConfidence,
                           "DescCaption"),
        0);
  }

  // Case 2: Orphan SECONDARY_CAPTION alone -> rejected without DescType
  // metrics.
  {
    base::HistogramTester histogram_tester;
    base::ListValue results;
    results.Append(base::DictValue()
                       .Set("type", manta::anchovy::kTypeSecondaryCaption)
                       .Set("score", 0.90)
                       .Set("text", "Los Angeles"));

    std::vector<mojom::Annotation> annotations;
    std::optional<mojom::AnnotateImageError> error;
    RunAnchovyAnnotatorTest(
        std::make_unique<FakeAnchovyProvider>(
            base::DictValue().Set("results", std::move(results))),
        &annotations, &error);

    EXPECT_TRUE(annotations.empty());
    EXPECT_EQ(error, mojom::AnnotateImageError::kFailure);

    histogram_tester.ExpectTotalCount(metrics_internal::kDescType, 0);
    histogram_tester.ExpectTotalCount(
        base::StringPrintf(metrics_internal::kAnnotationConfidence,
                           "DescCaption"),
        0);
    histogram_tester.ExpectTotalCount(
        base::StringPrintf(metrics_internal::kAnnotationConfidence,
                           "DescLabel"),
        0);
    histogram_tester.ExpectUniqueSample(metrics_internal::kClientResult,
                                        ClientResult::kFailed, 1);
  }
}

// Verifies that images smaller than kDescMinDimension (150x150) are filtered by
// IsWithinDescPolicy and not sent to AnchovyProvider.
TEST(AnnotatorTest, AnchovySkipsImagesOutsideDescPolicy) {
  base::ListValue results;
  results.Append(base::DictValue()
                     .Set("type", manta::anchovy::kTypePrimaryCaption)
                     .Set("score", 0.9)
                     .Set("text", "Should not be requested"));
  int request_count = -1;
  auto fake_provider = std::make_unique<FakeAnchovyProvider>(
      base::DictValue().Set("results", std::move(results)));

  std::vector<mojom::Annotation> annotations;
  std::optional<mojom::AnnotateImageError> error;
  RunAnchovyAnnotatorTest(std::move(fake_provider), &annotations, &error,
                          /*image_dim=*/50, &request_count);

  EXPECT_EQ(request_count, 0);
  EXPECT_TRUE(annotations.empty());
  EXPECT_EQ(error, mojom::AnnotateImageError::kFailure);
}

// Verifies that FetchServerLanguages() is not called when
// kImageDescriptionsAlternateRouting is enabled.
TEST(AnnotatorTest, AnchovyDoesNotFetchServerLanguages) {
  base::test::ScopedFeatureList enabled_features;
  enabled_features.InitAndEnableFeature(
      features::kImageDescriptionsAlternateRouting);

  base::test::TaskEnvironment test_task_env(
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);
  TestServerURLLoaderFactory test_url_factory(
      "https://ia-pa.googleapis.com/v1/");

  Annotator annotator(GURL(kTestServerUrl), GURL(kLangsServerUrl),
                      std::string() /* api_key */, kThrottle,
                      1 /* batch_size */, 0.7 /* min_ocr_confidence */,
                      test_url_factory.AsSharedURLLoaderFactory(),
                      std::make_unique<FakeAnchovyProvider>(base::DictValue()),
                      std::make_unique<TestAnnotatorClient>());
  test_task_env.FastForwardBy(base::Seconds(1));

  EXPECT_THAT(test_url_factory.requests(), IsEmpty());
}

// Test that two receivers bound with the same origin share cached results.
TEST(AnnotatorTest, SameOriginCacheHitAcrossReceivers) {
  base::test::TaskEnvironment test_task_env(
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);
  TestServerURLLoaderFactory test_url_factory(
      "https://ia-pa.googleapis.com/v1/");
  base::HistogramTester histogram_tester;

  Annotator annotator(
      GURL(kTestServerUrl), GURL(""), std::string() /* api_key */, kThrottle,
      1 /* batch_size */, 1.0 /* min_ocr_confidence */,
      test_url_factory.AsSharedURLLoaderFactory(), /*anchovy_provider=*/nullptr,
      std::make_unique<TestAnnotatorClient>());

  const url::Origin origin =
      url::Origin::Create(GURL("https://www.example.com"));
  mojo::Remote<mojom::Annotator> remote_annotator_1;
  annotator.BindReceiver(remote_annotator_1.BindNewPipeAndPassReceiver(),
                         origin);
  mojo::Remote<mojom::Annotator> remote_annotator_2;
  annotator.BindReceiver(remote_annotator_2.BindNewPipeAndPassReceiver(),
                         origin);

  TestImageProcessor processor_1;
  std::optional<mojom::AnnotateImageError> error_1;
  std::vector<mojom::Annotation> annotations_1;

  remote_annotator_1->AnnotateImage(
      kImage1Url, kDescLang, processor_1.GetPendingRemote(),
      base::BindOnce(&ReportResult, &error_1, &annotations_1));
  test_task_env.RunUntilIdle();

  ASSERT_THAT(processor_1.callbacks(), SizeIs(1));
  std::move(processor_1.callbacks()[0]).Run({1, 2, 3}, kDescDim, kDescDim);
  processor_1.callbacks().pop_back();
  test_task_env.FastForwardBy(base::Seconds(1));
  test_task_env.RunUntilIdle();

  test_url_factory.ExpectRequestAndSimulateResponse(
      "annotation", {} /* expected_headers */,
      ReformatJson(base::StringPrintf(kTemplateRequest, kImage1Url, "AQID")),
      kOcrSuccessResponse, net::HTTP_OK);
  test_task_env.RunUntilIdle();

  EXPECT_EQ(error_1, std::nullopt);
  EXPECT_THAT(annotations_1,
              ElementsAre(AnnotatorEq(mojom::AnnotationType::kOcr, 1.0,
                                      "Region 1\nRegion 2")));

  // Request the same image via the second receiver bound to the same origin.
  TestImageProcessor processor_2;
  std::optional<mojom::AnnotateImageError> error_2;
  std::vector<mojom::Annotation> annotations_2;

  remote_annotator_2->AnnotateImage(
      kImage1Url, kDescLang, processor_2.GetPendingRemote(),
      base::BindOnce(&ReportResult, &error_2, &annotations_2));
  test_task_env.RunUntilIdle();

  // Result should be served from cache without requesting pixels.
  EXPECT_THAT(processor_2.callbacks(), IsEmpty());
  EXPECT_EQ(error_2, std::nullopt);
  EXPECT_THAT(annotations_2,
              ElementsAre(AnnotatorEq(mojom::AnnotationType::kOcr, 1.0,
                                      "Region 1\nRegion 2")));
  EXPECT_THAT(histogram_tester.GetAllSamples(metrics_internal::kCacheHit),
              UnorderedElementsAre(Bucket(false, 1), Bucket(true, 1)));
}

// Test that two receivers bound with different origins do not share cached
// results.
TEST(AnnotatorTest, CrossOriginCacheMiss) {
  base::test::TaskEnvironment test_task_env(
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);
  TestServerURLLoaderFactory test_url_factory(
      "https://ia-pa.googleapis.com/v1/");
  base::HistogramTester histogram_tester;

  Annotator annotator(
      GURL(kTestServerUrl), GURL(""), std::string() /* api_key */, kThrottle,
      1 /* batch_size */, 1.0 /* min_ocr_confidence */,
      test_url_factory.AsSharedURLLoaderFactory(), /*anchovy_provider=*/nullptr,
      std::make_unique<TestAnnotatorClient>());

  const url::Origin origin_a =
      url::Origin::Create(GURL("https://a.example.com"));
  const url::Origin origin_b =
      url::Origin::Create(GURL("https://b.example.com"));
  mojo::Remote<mojom::Annotator> remote_annotator_a;
  annotator.BindReceiver(remote_annotator_a.BindNewPipeAndPassReceiver(),
                         origin_a);
  mojo::Remote<mojom::Annotator> remote_annotator_b;
  annotator.BindReceiver(remote_annotator_b.BindNewPipeAndPassReceiver(),
                         origin_b);

  TestImageProcessor processor_a;
  std::optional<mojom::AnnotateImageError> error_a;
  std::vector<mojom::Annotation> annotations_a;

  remote_annotator_a->AnnotateImage(
      kImage1Url, kDescLang, processor_a.GetPendingRemote(),
      base::BindOnce(&ReportResult, &error_a, &annotations_a));
  test_task_env.RunUntilIdle();

  ASSERT_THAT(processor_a.callbacks(), SizeIs(1));
  std::move(processor_a.callbacks()[0]).Run({1, 2, 3}, kDescDim, kDescDim);
  processor_a.callbacks().pop_back();
  test_task_env.FastForwardBy(base::Seconds(1));
  test_task_env.RunUntilIdle();

  test_url_factory.ExpectRequestAndSimulateResponse(
      "annotation", {} /* expected_headers */,
      ReformatJson(base::StringPrintf(kTemplateRequest, kImage1Url, "AQID")),
      kOcrSuccessResponse, net::HTTP_OK);
  test_task_env.RunUntilIdle();

  EXPECT_EQ(error_a, std::nullopt);
  EXPECT_THAT(annotations_a,
              ElementsAre(AnnotatorEq(mojom::AnnotationType::kOcr, 1.0,
                                      "Region 1\nRegion 2")));

  // Request the same image URL from a different origin: must miss cache and
  // fetch pixels + server response for origin B.
  TestImageProcessor processor_b;
  std::optional<mojom::AnnotateImageError> error_b;
  std::vector<mojom::Annotation> annotations_b;

  remote_annotator_b->AnnotateImage(
      kImage1Url, kDescLang, processor_b.GetPendingRemote(),
      base::BindOnce(&ReportResult, &error_b, &annotations_b));
  test_task_env.RunUntilIdle();

  ASSERT_THAT(processor_b.callbacks(), SizeIs(1));
  std::move(processor_b.callbacks()[0]).Run({4, 5, 6}, kDescDim, kDescDim);
  processor_b.callbacks().pop_back();
  test_task_env.FastForwardBy(base::Seconds(1));
  test_task_env.RunUntilIdle();

  test_url_factory.ExpectRequestAndSimulateResponse(
      "annotation", {} /* expected_headers */, ReformatJson(R"(
        {
          "imageRequests": [{
            "imageId": "1  https://www.example.com/image1.jpg",
            "imageBytes": "BAUG",
            "engineParameters": [
              {"ocrParameters": {}},
              {"descriptionParameters": {}},
              {"iconParameters": {}}
            ]
          }]
        }
      )"),
      R"(
        {
          "results": [
            {
              "imageId": "1  https://www.example.com/image1.jpg",
              "engineResults": [
                {
                  "status": {},
                  "ocrEngine": {
                    "ocrRegions": [{
                      "words": [{
                        "detectedText": "OriginB",
                        "confidenceScore": 1.0
                      }]
                    }]
                  }
                }
              ]
            }
          ]
        }
      )",
      net::HTTP_OK);
  test_task_env.RunUntilIdle();

  EXPECT_EQ(error_b, std::nullopt);
  EXPECT_THAT(annotations_b, ElementsAre(AnnotatorEq(
                                 mojom::AnnotationType::kOcr, 1.0, "OriginB")));
  EXPECT_THAT(histogram_tester.GetAllSamples(metrics_internal::kCacheHit),
              UnorderedElementsAre(Bucket(false, 2)));
}

// Test that two receivers bound with distinct opaque origins do not share
// cached results.
TEST(AnnotatorTest, OpaqueOriginCacheMiss) {
  base::test::TaskEnvironment test_task_env(
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);
  TestServerURLLoaderFactory test_url_factory(
      "https://ia-pa.googleapis.com/v1/");
  base::HistogramTester histogram_tester;

  Annotator annotator(
      GURL(kTestServerUrl), GURL(""), std::string() /* api_key */, kThrottle,
      1 /* batch_size */, 1.0 /* min_ocr_confidence */,
      test_url_factory.AsSharedURLLoaderFactory(), /*anchovy_provider=*/nullptr,
      std::make_unique<TestAnnotatorClient>());

  const url::Origin file_origin = url::Origin::Create(GURL("file:///a.html"));
  mojo::Remote<mojom::Annotator> remote_annotator_1;
  annotator.BindReceiver(remote_annotator_1.BindNewPipeAndPassReceiver(),
                         file_origin.DeriveNewOpaqueOrigin());
  mojo::Remote<mojom::Annotator> remote_annotator_2;
  annotator.BindReceiver(remote_annotator_2.BindNewPipeAndPassReceiver(),
                         file_origin.DeriveNewOpaqueOrigin());

  TestImageProcessor processor_1;
  std::optional<mojom::AnnotateImageError> error_1;
  std::vector<mojom::Annotation> annotations_1;

  remote_annotator_1->AnnotateImage(
      kImage1Url, kDescLang, processor_1.GetPendingRemote(),
      base::BindOnce(&ReportResult, &error_1, &annotations_1));
  test_task_env.RunUntilIdle();

  ASSERT_THAT(processor_1.callbacks(), SizeIs(1));
  std::move(processor_1.callbacks()[0]).Run({1, 2, 3}, kDescDim, kDescDim);
  processor_1.callbacks().pop_back();
  test_task_env.FastForwardBy(base::Seconds(1));
  test_task_env.RunUntilIdle();

  test_url_factory.ExpectRequestAndSimulateResponse(
      "annotation", {} /* expected_headers */,
      ReformatJson(base::StringPrintf(kTemplateRequest, kImage1Url, "AQID")),
      kOcrSuccessResponse, net::HTTP_OK);
  test_task_env.RunUntilIdle();

  EXPECT_EQ(error_1, std::nullopt);

  // Second opaque origin must miss the cache and trigger pixel fetch.
  TestImageProcessor processor_2;
  std::optional<mojom::AnnotateImageError> error_2;
  std::vector<mojom::Annotation> annotations_2;

  remote_annotator_2->AnnotateImage(
      kImage1Url, kDescLang, processor_2.GetPendingRemote(),
      base::BindOnce(&ReportResult, &error_2, &annotations_2));
  test_task_env.RunUntilIdle();

  ASSERT_THAT(processor_2.callbacks(), SizeIs(1));
  std::move(processor_2.callbacks()[0]).Run({4, 5, 6}, kDescDim, kDescDim);
  processor_2.callbacks().pop_back();
  test_task_env.FastForwardBy(base::Seconds(1));
  test_task_env.RunUntilIdle();

  test_url_factory.ExpectRequestAndSimulateResponse(
      "annotation", {} /* expected_headers */, ReformatJson(R"(
        {
          "imageRequests": [{
            "imageId": "1  https://www.example.com/image1.jpg",
            "imageBytes": "BAUG",
            "engineParameters": [
              {"ocrParameters": {}},
              {"descriptionParameters": {}},
              {"iconParameters": {}}
            ]
          }]
        }
      )"),
      R"(
        {
          "results": [
            {
              "imageId": "1  https://www.example.com/image1.jpg",
              "engineResults": [
                {
                  "status": {},
                  "ocrEngine": {
                    "ocrRegions": [{
                      "words": [{
                        "detectedText": "Opaque2",
                        "confidenceScore": 1.0
                      }]
                    }]
                  }
                }
              ]
            }
          ]
        }
      )",
      net::HTTP_OK);
  test_task_env.RunUntilIdle();

  EXPECT_EQ(error_2, std::nullopt);
  EXPECT_THAT(annotations_2, ElementsAre(AnnotatorEq(
                                 mojom::AnnotationType::kOcr, 1.0, "Opaque2")));
  EXPECT_THAT(histogram_tester.GetAllSamples(metrics_internal::kCacheHit),
              UnorderedElementsAre(Bucket(false, 2)));
}

// Test that concurrent requests for the same source ID from different origins
// in the same server batch use distinct image IDs and receive their respective
// results without collision.
TEST(AnnotatorTest, CrossOriginConcurrentSameBatch) {
  base::test::TaskEnvironment test_task_env(
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);
  TestServerURLLoaderFactory test_url_factory(
      "https://ia-pa.googleapis.com/v1/");

  Annotator annotator(
      GURL(kTestServerUrl), GURL(""), std::string() /* api_key */, kThrottle,
      10 /* batch_size */, 1.0 /* min_ocr_confidence */,
      test_url_factory.AsSharedURLLoaderFactory(), /*anchovy_provider=*/nullptr,
      std::make_unique<TestAnnotatorClient>());

  const url::Origin origin_a =
      url::Origin::Create(GURL("https://a.example.com"));
  const url::Origin origin_b =
      url::Origin::Create(GURL("https://b.example.com"));
  mojo::Remote<mojom::Annotator> remote_annotator_a;
  annotator.BindReceiver(remote_annotator_a.BindNewPipeAndPassReceiver(),
                         origin_a);
  mojo::Remote<mojom::Annotator> remote_annotator_b;
  annotator.BindReceiver(remote_annotator_b.BindNewPipeAndPassReceiver(),
                         origin_b);

  TestImageProcessor processor_a;
  std::optional<mojom::AnnotateImageError> error_a;
  std::vector<mojom::Annotation> annotations_a;

  TestImageProcessor processor_b;
  std::optional<mojom::AnnotateImageError> error_b;
  std::vector<mojom::Annotation> annotations_b;

  remote_annotator_a->AnnotateImage(
      kImage1Url, kDescLang, processor_a.GetPendingRemote(),
      base::BindOnce(&ReportResult, &error_a, &annotations_a));
  remote_annotator_b->AnnotateImage(
      kImage1Url, kDescLang, processor_b.GetPendingRemote(),
      base::BindOnce(&ReportResult, &error_b, &annotations_b));
  test_task_env.RunUntilIdle();

  ASSERT_THAT(processor_a.callbacks(), SizeIs(1));
  ASSERT_THAT(processor_b.callbacks(), SizeIs(1));

  std::move(processor_a.callbacks()[0]).Run({1, 2, 3}, kDescDim, kDescDim);
  processor_a.callbacks().pop_back();
  std::move(processor_b.callbacks()[0]).Run({4, 5, 6}, kDescDim, kDescDim);
  processor_b.callbacks().pop_back();
  test_task_env.FastForwardBy(base::Seconds(1));
  test_task_env.RunUntilIdle();

  test_url_factory.ExpectRequestAndSimulateResponse(
      "annotation", {} /* expected_headers */, ReformatJson(R"(
        {
          "imageRequests": [
            {
              "imageId": "1  https://www.example.com/image1.jpg",
              "imageBytes": "BAUG",
              "engineParameters": [
                {"ocrParameters": {}},
                {"descriptionParameters": {}},
                {"iconParameters": {}}
              ]
            },
            {
              "imageId": "0  https://www.example.com/image1.jpg",
              "imageBytes": "AQID",
              "engineParameters": [
                {"ocrParameters": {}},
                {"descriptionParameters": {}},
                {"iconParameters": {}}
              ]
            }
          ]
        }
      )"),
      R"(
        {
          "results": [
            {
              "imageId": "0  https://www.example.com/image1.jpg",
              "engineResults": [
                {
                  "status": {},
                  "ocrEngine": {
                    "ocrRegions": [{
                      "words": [{
                        "detectedText": "TextA",
                        "confidenceScore": 1.0
                      }]
                    }]
                  }
                }
              ]
            },
            {
              "imageId": "1  https://www.example.com/image1.jpg",
              "engineResults": [
                {
                  "status": {},
                  "ocrEngine": {
                    "ocrRegions": [{
                      "words": [{
                        "detectedText": "TextB",
                        "confidenceScore": 1.0
                      }]
                    }]
                  }
                }
              ]
            }
          ]
        }
      )",
      net::HTTP_OK);
  test_task_env.RunUntilIdle();

  EXPECT_EQ(error_a, std::nullopt);
  EXPECT_THAT(annotations_a, ElementsAre(AnnotatorEq(
                                 mojom::AnnotationType::kOcr, 1.0, "TextA")));
  EXPECT_EQ(error_b, std::nullopt);
  EXPECT_THAT(annotations_b, ElementsAre(AnnotatorEq(
                                 mojom::AnnotationType::kOcr, 1.0, "TextB")));

  // Subsequent requests on each origin should receive their respective cached
  // results without cross-contamination.
  std::optional<mojom::AnnotateImageError> cached_error_a;
  std::vector<mojom::Annotation> cached_annotations_a;
  std::optional<mojom::AnnotateImageError> cached_error_b;
  std::vector<mojom::Annotation> cached_annotations_b;

  remote_annotator_a->AnnotateImage(
      kImage1Url, kDescLang, processor_a.GetPendingRemote(),
      base::BindOnce(&ReportResult, &cached_error_a, &cached_annotations_a));
  remote_annotator_b->AnnotateImage(
      kImage1Url, kDescLang, processor_b.GetPendingRemote(),
      base::BindOnce(&ReportResult, &cached_error_b, &cached_annotations_b));
  test_task_env.RunUntilIdle();

  EXPECT_THAT(processor_a.callbacks(), IsEmpty());
  EXPECT_THAT(processor_b.callbacks(), IsEmpty());
  EXPECT_EQ(cached_error_a, std::nullopt);
  EXPECT_THAT(
      cached_annotations_a,
      ElementsAre(AnnotatorEq(mojom::AnnotationType::kOcr, 1.0, "TextA")));
  EXPECT_EQ(cached_error_b, std::nullopt);
  EXPECT_THAT(
      cached_annotations_b,
      ElementsAre(AnnotatorEq(mojom::AnnotationType::kOcr, 1.0, "TextB")));
}

// Test that local pixel processing is deduplicated within the same origin but
// isolated across different origins.
TEST(AnnotatorTest, CrossOriginLocalProcessingIsolated) {
  base::test::TaskEnvironment test_task_env(
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);
  TestServerURLLoaderFactory test_url_factory(
      "https://ia-pa.googleapis.com/v1/");

  Annotator annotator(
      GURL(kTestServerUrl), GURL(""), std::string() /* api_key */, kThrottle,
      10 /* batch_size */, 1.0 /* min_ocr_confidence */,
      test_url_factory.AsSharedURLLoaderFactory(), /*anchovy_provider=*/nullptr,
      std::make_unique<TestAnnotatorClient>());

  const url::Origin origin_a =
      url::Origin::Create(GURL("https://a.example.com"));
  const url::Origin origin_b =
      url::Origin::Create(GURL("https://b.example.com"));
  mojo::Remote<mojom::Annotator> remote_a1;
  annotator.BindReceiver(remote_a1.BindNewPipeAndPassReceiver(), origin_a);
  mojo::Remote<mojom::Annotator> remote_a2;
  annotator.BindReceiver(remote_a2.BindNewPipeAndPassReceiver(), origin_a);
  mojo::Remote<mojom::Annotator> remote_b1;
  annotator.BindReceiver(remote_b1.BindNewPipeAndPassReceiver(), origin_b);

  TestImageProcessor processor_a1;
  std::optional<mojom::AnnotateImageError> error_a1;
  std::vector<mojom::Annotation> annotations_a1;

  TestImageProcessor processor_a2;
  std::optional<mojom::AnnotateImageError> error_a2;
  std::vector<mojom::Annotation> annotations_a2;

  TestImageProcessor processor_b1;
  std::optional<mojom::AnnotateImageError> error_b1;
  std::vector<mojom::Annotation> annotations_b1;

  remote_a1->AnnotateImage(
      kImage1Url, kDescLang, processor_a1.GetPendingRemote(),
      base::BindOnce(&ReportResult, &error_a1, &annotations_a1));
  remote_a2->AnnotateImage(
      kImage1Url, kDescLang, processor_a2.GetPendingRemote(),
      base::BindOnce(&ReportResult, &error_a2, &annotations_a2));
  remote_b1->AnnotateImage(
      kImage1Url, kDescLang, processor_b1.GetPendingRemote(),
      base::BindOnce(&ReportResult, &error_b1, &annotations_b1));
  test_task_env.RunUntilIdle();

  // Within origin A, only processor_a1 should have been queried; origin B
  // should independently query processor_b1.
  EXPECT_THAT(processor_a1.callbacks(), SizeIs(1));
  EXPECT_THAT(processor_a2.callbacks(), IsEmpty());
  EXPECT_THAT(processor_b1.callbacks(), SizeIs(1));

  // If processor_a1 disconnects, origin A reassigns local work to processor_a2
  // without affecting origin B.
  processor_a1.Reset();
  test_task_env.RunUntilIdle();

  EXPECT_EQ(error_a1, mojom::AnnotateImageError::kCanceled);
  EXPECT_THAT(processor_a2.callbacks(), SizeIs(1));
  EXPECT_THAT(processor_b1.callbacks(), SizeIs(1));
}

}  // namespace image_annotation
