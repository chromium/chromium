// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/image_annotation/annotator.h"

#include <cstring>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/optional.h"
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

// Example image URLs.

constexpr char kImage1Url[] = "https://www.example.com/image1.jpg";
constexpr char kImage2Url[] = "https://www.example.com/image2.jpg";
constexpr char kImage3Url[] = "https://www.example.com/image3.jpg";

// Example server requests / responses.

// Template for a request for a single image.
constexpr char kTemplateRequest[] = R"(
{
  "imageRequests": [{
    "imageId": "%s",
    "imageBytes": "%s",
    "engineParameters": [
      {"ocrParameters": {}},
      {"descriptionParameters": {}}
    ]
  }]
}
)";

// Batch request for |kImage1Url|, |kImage2Url| and |kImage3Url|.
constexpr char kBatchRequest[] = R"(
{
  "imageRequests": [
    {
      "imageId": "https://www.example.com/image3.jpg",
      "imageBytes": "BwgJ",
      "engineParameters": [
        {"ocrParameters": {}},
        {"descriptionParameters": {}}
      ]
    },
    {
      "imageId": "https://www.example.com/image2.jpg",
      "imageBytes": "BAUG",
      "engineParameters": [
        {"ocrParameters": {}},
        {"descriptionParameters": {}}
      ]
    },
    {
      "imageId": "https://www.example.com/image1.jpg",
      "imageBytes": "AQID",
      "engineParameters": [
        {"ocrParameters": {}},
        {"descriptionParameters": {}}
      ]
    }
  ]
})";

// Successful OCR text extraction for |kImage1Url| with no descriptions.
constexpr char kOcrSuccessResponse[] = R"(
{
  "results": [
    {
      "imageId": "https://www.example.com/image1.jpg",
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
      "imageId": "https://www.example.com/image2.jpg",
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
      "imageId": "https://www.example.com/image1.jpg",
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
      "imageId": "https://www.example.com/image3.jpg",
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

constexpr base::TimeDelta kThrottle = base::TimeDelta::FromSeconds(1);

// The minimum dimension required for description annotation.
constexpr int32_t kDescDim = Annotator::kDescMinDimension;

// The description language to use in tests that don't exercise
// language-handling logic.
constexpr char kDescLang[] = "";

// An image processor that holds and exposes the callbacks it is passed.
class TestImageProcessor : public mojom::ImageProcessor {
 public:
  TestImageProcessor() = default;

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

  DISALLOW_COPY_AND_ASSIGN(TestImageProcessor);
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
      const std::map<std::string, base::Optional<std::string>>&
          expected_headers,
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
      if (kv.second.has_value()) {
        std::string actual_value;
        EXPECT_THAT(request.headers.GetHeader(kv.first, &actual_value),
                    Eq(true));
        EXPECT_THAT(actual_value, Eq(*kv.second));
      } else {
        EXPECT_THAT(request.headers.HasHeader(kv.first), Eq(false));
      }
    }

    // Extract request body.
    std::string actual_body;
    if (request.request_body) {
      const std::vector<network::DataElement>* const elements =
          request.request_body->elements();

      // We only support the simplest body structure.
      CHECK(elements && elements->size() == 1 &&
            (*elements)[0].type() == network::mojom::DataElementType::kBytes);

      actual_body =
          std::string((*elements)[0].bytes(), (*elements)[0].length());
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

  DISALLOW_COPY_AND_ASSIGN(TestServerURLLoaderFactory);
};

// Returns a "canonically" formatted version of a JSON string by parsing and
// then rewriting it.
std::string ReformatJson(const std::string& in) {
  const std::unique_ptr<base::Value> json =
      base::JSONReader::ReadDeprecated(in);
  CHECK(json);

  std::string out;
  base::JSONWriter::Write(*json, &out);

  return out;
}

// Receives the result of an annotation request and writes the result data into
// the given variables.
void ReportResult(base::Optional<mojom::AnnotateImageError>* const error,
                  std::vector<mojom::Annotation>* const annotations,
                  mojom::AnnotateImageResultPtr result) {
  if (result->which() == mojom::AnnotateImageResult::Tag::ERROR_CODE) {
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

  // Annotator::Client implementation:
  void BindJsonParser(mojo::PendingReceiver<data_decoder::mojom::JsonParser>
                          receiver) override {
    decoder_.GetService()->BindJsonParser(std::move(receiver));
  }

 private:
  data_decoder::DataDecoder decoder_;
};

}  // namespace

// Test that annotation works for one client, and that the cache is populated.
TEST(AnnotatorTest, OcrSuccessAndCache) {
  base::test::TaskEnvironment test_task_env(
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);
  TestServerURLLoaderFactory test_url_factory(
      "https://ia-pa.googleapis.com/v1/");
  data_decoder::test::InProcessDataDecoder in_process_data_decoder;
  base::HistogramTester histogram_tester;

  Annotator annotator(GURL(kTestServerUrl), std::string() /* api_key */,
                      kThrottle, 1 /* batch_size */,
                      1.0 /* min_ocr_confidence */,
                      test_url_factory.AsSharedURLLoaderFactory(),
                      std::make_unique<TestAnnotatorClient>());
  TestImageProcessor processor;

  // First call performs original image annotation.
  {
    base::Optional<mojom::AnnotateImageError> error;
    std::vector<mojom::Annotation> annotations;

    annotator.AnnotateImage(
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
    test_task_env.FastForwardBy(base::TimeDelta::FromSeconds(1));
    test_task_env.RunUntilIdle();

    // HTTP request should have been made.
    const std::string request =
        ReformatJson(base::StringPrintf(kTemplateRequest, kImage1Url, "AQID"));
    test_url_factory.ExpectRequestAndSimulateResponse(
        "annotation", {} /* expected_headers */, request, kOcrSuccessResponse,
        net::HTTP_OK);
    test_task_env.RunUntilIdle();

    // HTTP response should have completed and callback should have been called.
    ASSERT_THAT(error, Eq(base::nullopt));
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
    base::Optional<mojom::AnnotateImageError> error;
    std::vector<mojom::Annotation> annotations;

    annotator.AnnotateImage(
        kImage1Url, kDescLang, processor.GetPendingRemote(),
        base::BindOnce(&ReportResult, &error, &annotations));
    test_task_env.RunUntilIdle();

    // Pixels shouldn't be requested.
    ASSERT_THAT(processor.callbacks(), IsEmpty());

    // Results should have been directly returned without any server call.
    ASSERT_THAT(error, Eq(base::nullopt));
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
  data_decoder::test::InProcessDataDecoder in_process_data_decoder;
  base::HistogramTester histogram_tester;

  Annotator annotator(GURL(kTestServerUrl), std::string() /* api_key */,
                      kThrottle, 1 /* batch_size */,
                      1.0 /* min_ocr_confidence */,
                      test_url_factory.AsSharedURLLoaderFactory(),
                      std::make_unique<TestAnnotatorClient>());
  TestImageProcessor processor;

  base::Optional<mojom::AnnotateImageError> error;
  std::vector<mojom::Annotation> annotations;

  annotator.AnnotateImage(kImage1Url, kDescLang, processor.GetPendingRemote(),
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
  test_task_env.FastForwardBy(base::TimeDelta::FromSeconds(1));
  test_task_env.RunUntilIdle();

  // HTTP request should have been made.
  test_url_factory.ExpectRequestAndSimulateResponse(
      "annotation", {} /* expected_headers */,
      ReformatJson(base::StringPrintf(kTemplateRequest, kImage1Url, "AQID")),
      R"({
           "results": [{
             "imageId": "https://www.example.com/image1.jpg",
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
  ASSERT_THAT(error, Eq(base::nullopt));
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
  data_decoder::test::InProcessDataDecoder in_process_data_decoder;
  base::HistogramTester histogram_tester;

  Annotator annotator(GURL(kTestServerUrl), std::string() /* api_key */,
                      kThrottle, 1 /* batch_size */,
                      1.0 /* min_ocr_confidence */,
                      test_url_factory.AsSharedURLLoaderFactory(),
                      std::make_unique<TestAnnotatorClient>());
  TestImageProcessor processor;

  base::Optional<mojom::AnnotateImageError> error;
  std::vector<mojom::Annotation> annotations;

  annotator.AnnotateImage(kImage1Url, kDescLang, processor.GetPendingRemote(),
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
  test_task_env.FastForwardBy(base::TimeDelta::FromSeconds(1));
  test_task_env.RunUntilIdle();

  // HTTP request should have been made.
  test_url_factory.ExpectRequestAndSimulateResponse(
      "annotation", {} /* expected_headers */,
      ReformatJson(base::StringPrintf(kTemplateRequest, kImage1Url, "AQID")),
      R"({
           "results": [{
             "imageId": "https://www.example.com/image1.jpg",
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
  ASSERT_THAT(error, Eq(base::nullopt));
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
  data_decoder::test::InProcessDataDecoder in_process_data_decoder;
  base::HistogramTester histogram_tester;

  Annotator annotator(GURL(kTestServerUrl), std::string() /* api_key */,
                      kThrottle, 1 /* batch_size */,
                      1.0 /* min_ocr_confidence */,
                      test_url_factory.AsSharedURLLoaderFactory(),
                      std::make_unique<TestAnnotatorClient>());

  TestImageProcessor processor;
  base::Optional<mojom::AnnotateImageError> error;
  std::vector<mojom::Annotation> annotations;

  annotator.AnnotateImage(kImage1Url, kDescLang, processor.GetPendingRemote(),
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
  test_task_env.FastForwardBy(base::TimeDelta::FromSeconds(1));

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
  data_decoder::test::InProcessDataDecoder in_process_data_decoder;
  base::HistogramTester histogram_tester;

  Annotator annotator(GURL(kTestServerUrl), std::string() /* api_key */,
                      kThrottle, 1 /* batch_size */,
                      1.0 /* min_ocr_confidence */,
                      test_url_factory.AsSharedURLLoaderFactory(),
                      std::make_unique<TestAnnotatorClient>());

  TestImageProcessor processor;
  base::Optional<mojom::AnnotateImageError> error;
  std::vector<mojom::Annotation> annotations;

  annotator.AnnotateImage(kImage1Url, kDescLang, processor.GetPendingRemote(),
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
  test_task_env.FastForwardBy(base::TimeDelta::FromSeconds(1));

  // HTTP request should have been made.
  test_url_factory.ExpectRequestAndSimulateResponse(
      "annotation", {} /* expected_headers */,
      ReformatJson(base::StringPrintf(kTemplateRequest, kImage1Url, "AQID")),
      R"({
           "results": [{
             "imageId": "https://www.example.com/image1.jpg",
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
  data_decoder::test::InProcessDataDecoder in_process_data_decoder;
  base::HistogramTester histogram_tester;

  Annotator annotator(GURL(kTestServerUrl), std::string() /* api_key */,
                      kThrottle, 1 /* batch_size */,
                      1.0 /* min_ocr_confidence */,
                      test_url_factory.AsSharedURLLoaderFactory(),
                      std::make_unique<TestAnnotatorClient>());

  TestImageProcessor processor;
  base::Optional<mojom::AnnotateImageError> error;
  std::vector<mojom::Annotation> annotations;

  annotator.AnnotateImage(kImage1Url, kDescLang, processor.GetPendingRemote(),
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
  test_task_env.FastForwardBy(base::TimeDelta::FromSeconds(1));

  // HTTP request should have been made.
  test_url_factory.ExpectRequestAndSimulateResponse(
      "annotation", {} /* expected_headers */,
      ReformatJson(base::StringPrintf(kTemplateRequest, kImage1Url, "AQID")),
      R"({
           "results": [{
             "imageId": "https://www.example.com/image1.jpg",
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
  EXPECT_THAT(error, Eq(base::nullopt));
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
  data_decoder::test::InProcessDataDecoder in_process_data_decoder;
  base::HistogramTester histogram_tester;

  Annotator annotator(GURL(kTestServerUrl), std::string() /* api_key */,
                      kThrottle, 1 /* batch_size */,
                      1.0 /* min_ocr_confidence */,
                      test_url_factory.AsSharedURLLoaderFactory(),
                      std::make_unique<TestAnnotatorClient>());

  TestImageProcessor processor;
  base::Optional<mojom::AnnotateImageError> error;
  std::vector<mojom::Annotation> annotations;

  annotator.AnnotateImage(kImage1Url, kDescLang, processor.GetPendingRemote(),
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
  test_task_env.FastForwardBy(base::TimeDelta::FromSeconds(1));

  // HTTP request should have been made.
  test_url_factory.ExpectRequestAndSimulateResponse(
      "annotation", {} /* expected_headers */,
      ReformatJson(base::StringPrintf(kTemplateRequest, kImage1Url, "AQID")),
      R"({
           "results": [{
             "imageId": "https://www.example.com/image1.jpg",
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
  EXPECT_THAT(error, Eq(base::nullopt));
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
  data_decoder::test::InProcessDataDecoder in_process_data_decoder;
  base::HistogramTester histogram_tester;

  Annotator annotator(GURL(kTestServerUrl), std::string() /* api_key */,
                      kThrottle, 1 /* batch_size */,
                      1.0 /* min_ocr_confidence */,
                      test_url_factory.AsSharedURLLoaderFactory(),
                      std::make_unique<TestAnnotatorClient>());

  TestImageProcessor processor;
  base::Optional<mojom::AnnotateImageError> error;
  std::vector<mojom::Annotation> annotations;

  annotator.AnnotateImage(kImage1Url, kDescLang, processor.GetPendingRemote(),
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
  test_task_env.FastForwardBy(base::TimeDelta::FromSeconds(1));

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
  data_decoder::test::InProcessDataDecoder in_process_data_decoder;
  base::HistogramTester histogram_tester;

  Annotator annotator(GURL(kTestServerUrl), std::string() /* api_key */,
                      kThrottle, 1 /* batch_size */,
                      1.0 /* min_ocr_confidence */,
                      test_url_factory.AsSharedURLLoaderFactory(),
                      std::make_unique<TestAnnotatorClient>());

  TestImageProcessor processor;
  base::Optional<mojom::AnnotateImageError> error;
  std::vector<mojom::Annotation> annotations;

  annotator.AnnotateImage(kImage1Url, kDescLang, processor.GetPendingRemote(),
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
  test_task_env.FastForwardBy(base::TimeDelta::FromSeconds(1));

  // HTTP request should have been made.
  test_url_factory.ExpectRequestAndSimulateResponse(
      "annotation", {} /* expected headers */,
      ReformatJson(base::StringPrintf(kTemplateRequest, kImage1Url, "AQID")),
      R"({
           "results": [{
             "imageId": "https://www.example.com/image1.jpg",
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
  data_decoder::test::InProcessDataDecoder in_process_data_decoder;
  base::HistogramTester histogram_tester;

  Annotator annotator(GURL(kTestServerUrl), std::string() /* api_key */,
                      kThrottle, 1 /* batch_size */,
                      1.0 /* min_ocr_confidence */,
                      test_url_factory.AsSharedURLLoaderFactory(),
                      std::make_unique<TestAnnotatorClient>());

  TestImageProcessor processor[3];
  base::Optional<mojom::AnnotateImageError> error[3];
  std::vector<mojom::Annotation> annotations[3];

  for (int i = 0; i < 3; ++i) {
    annotator.AnnotateImage(
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
  test_task_env.FastForwardBy(base::TimeDelta::FromSeconds(1));

  // HTTP request for image 1 should have been made.
  test_url_factory.ExpectRequestAndSimulateResponse(
      "annotation", {} /* expected_headers */,
      ReformatJson(base::StringPrintf(kTemplateRequest, kImage1Url, "AQID")),
      kOcrSuccessResponse, net::HTTP_OK);
  test_task_env.RunUntilIdle();

  // Annotator should have called all callbacks, but request 1 received an error
  // when we returned empty bytes.
  ASSERT_THAT(error, ElementsAre(mojom::AnnotateImageError::kFailure,
                                 base::nullopt, base::nullopt));
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
  data_decoder::test::InProcessDataDecoder in_process_data_decoder;
  base::HistogramTester histogram_tester;

  Annotator annotator(GURL(kTestServerUrl), std::string() /* api_key */,
                      kThrottle, 1 /* batch_size */,
                      1.0 /* min_ocr_confidence */,
                      test_url_factory.AsSharedURLLoaderFactory(),
                      std::make_unique<TestAnnotatorClient>());

  TestImageProcessor processor[2];
  base::Optional<mojom::AnnotateImageError> error[2];
  std::vector<mojom::Annotation> annotations[2];

  // Processor 1 makes a request for annotation of a given image.
  annotator.AnnotateImage(
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
  annotator.AnnotateImage(
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
  test_task_env.FastForwardBy(base::TimeDelta::FromSeconds(1));

  // HTTP request for image 1 should have been made.
  test_url_factory.ExpectRequestAndSimulateResponse(
      "annotation", {} /* expected_headers */,
      ReformatJson(base::StringPrintf(kTemplateRequest, kImage1Url, "AQID")),
      kOcrSuccessResponse, net::HTTP_OK);
  test_task_env.RunUntilIdle();

  // Annotator should have called all callbacks, but request 1 received an error
  // when we returned empty bytes.
  ASSERT_THAT(error,
              ElementsAre(mojom::AnnotateImageError::kFailure, base::nullopt));
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
  data_decoder::test::InProcessDataDecoder in_process_data_decoder;
  base::HistogramTester histogram_tester;

  Annotator annotator(GURL(kTestServerUrl), std::string() /* api_key */,
                      kThrottle, 1 /* batch_size */,
                      1.0 /* min_ocr_confidence */,
                      test_url_factory.AsSharedURLLoaderFactory(),
                      std::make_unique<TestAnnotatorClient>());

  TestImageProcessor processor[3];
  base::Optional<mojom::AnnotateImageError> error[3];
  std::vector<mojom::Annotation> annotations[3];

  for (int i = 0; i < 3; ++i) {
    annotator.AnnotateImage(
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
  test_task_env.FastForwardBy(base::TimeDelta::FromSeconds(1));

  // HTTP request for image 1 should have been made.
  test_url_factory.ExpectRequestAndSimulateResponse(
      "annotation", {} /* expected_headers */,
      ReformatJson(base::StringPrintf(kTemplateRequest, kImage1Url, "AQID")),
      kOcrSuccessResponse, net::HTTP_OK);
  test_task_env.RunUntilIdle();

  // Annotator should have called all callbacks, but request 1 was canceled when
  // we reset processor 1.
  ASSERT_THAT(error, ElementsAre(mojom::AnnotateImageError::kCanceled,
                                 base::nullopt, base::nullopt));
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
  data_decoder::test::InProcessDataDecoder in_process_data_decoder;
  base::HistogramTester histogram_tester;

  Annotator annotator(GURL(kTestServerUrl), std::string() /* api_key */,
                      kThrottle, 3 /* batch_size */,
                      1.0 /* min_ocr_confidence */,
                      test_url_factory.AsSharedURLLoaderFactory(),
                      std::make_unique<TestAnnotatorClient>());

  TestImageProcessor processor[3];
  base::Optional<mojom::AnnotateImageError> error[3];
  std::vector<mojom::Annotation> annotations[3];

  // Request OCR for images 1, 2 and 3.
  annotator.AnnotateImage(
      kImage1Url, kDescLang, processor[0].GetPendingRemote(),
      base::BindOnce(&ReportResult, &error[0], &annotations[0]));
  annotator.AnnotateImage(
      kImage2Url, kDescLang, processor[1].GetPendingRemote(),
      base::BindOnce(&ReportResult, &error[1], &annotations[1]));
  annotator.AnnotateImage(
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
  test_task_env.FastForwardBy(base::TimeDelta::FromSeconds(1));

  // A single HTTP request for all images should have been sent.
  test_url_factory.ExpectRequestAndSimulateResponse(
      "annotation", {} /* expected_headers */, ReformatJson(kBatchRequest),
      kBatchResponse, net::HTTP_OK);
  test_task_env.RunUntilIdle();

  // Annotator should have called each callback with its corresponding text or
  // failure.
  ASSERT_THAT(error, ElementsAre(base::nullopt, base::nullopt,
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
  data_decoder::test::InProcessDataDecoder in_process_data_decoder;
  base::HistogramTester histogram_tester;

  Annotator annotator(GURL(kTestServerUrl), std::string() /* api_key */,
                      kThrottle, 3 /* batch_size */,
                      1.0 /* min_ocr_confidence */,
                      test_url_factory.AsSharedURLLoaderFactory(),
                      std::make_unique<TestAnnotatorClient>());

  TestImageProcessor processor[2];
  base::Optional<mojom::AnnotateImageError> error[2];
  std::vector<mojom::Annotation> annotations[2];

  // Request OCR for image 1.
  annotator.AnnotateImage(
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
  test_task_env.FastForwardBy(base::TimeDelta::FromSeconds(1));

  // Request OCR for image 2.
  annotator.AnnotateImage(
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
             "imageId": "https://www.example.com/image1.jpg",
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

  test_task_env.FastForwardBy(base::TimeDelta::FromSeconds(1));

  // Now the HTTP request for image 2 should have been made.
  test_url_factory.ExpectRequestAndSimulateResponse(
      "annotation", {} /* expected_headers */,
      ReformatJson(base::StringPrintf(kTemplateRequest, kImage2Url, "BAUG")),
      R"({
           "results": [{
             "imageId": "https://www.example.com/image2.jpg",
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
  ASSERT_THAT(error, ElementsAre(base::nullopt, base::nullopt));
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
  data_decoder::test::InProcessDataDecoder in_process_data_decoder;
  base::HistogramTester histogram_tester;

  Annotator annotator(GURL(kTestServerUrl), std::string() /* api_key */,
                      kThrottle, 1 /* batch_size */,
                      1.0 /* min_ocr_confidence */,
                      test_url_factory.AsSharedURLLoaderFactory(),
                      std::make_unique<TestAnnotatorClient>());

  TestImageProcessor processor[4];
  base::Optional<mojom::AnnotateImageError> error[4];
  std::vector<mojom::Annotation> annotations[4];

  // First request annotation of the image with processor 1.
  annotator.AnnotateImage(
      kImage1Url, kDescLang, processor[0].GetPendingRemote(),
      base::BindOnce(&ReportResult, &error[0], &annotations[0]));
  test_task_env.RunUntilIdle();

  // Annotator should have asked processor 1 for the image's pixels.
  ASSERT_THAT(processor[0].callbacks(), SizeIs(1));
  ASSERT_THAT(processor[1].callbacks(), IsEmpty());
  ASSERT_THAT(processor[2].callbacks(), IsEmpty());
  ASSERT_THAT(processor[3].callbacks(), IsEmpty());

  // Now request annotation of the image with processor 2.
  annotator.AnnotateImage(
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
  annotator.AnnotateImage(
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
  test_task_env.FastForwardBy(base::TimeDelta::FromSeconds(1));
  EXPECT_THAT(test_url_factory.requests(), SizeIs(1));
  annotator.AnnotateImage(
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
  ASSERT_THAT(error, ElementsAre(base::nullopt, base::nullopt, base::nullopt,
                                 base::nullopt));
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
  data_decoder::test::InProcessDataDecoder in_process_data_decoder;
  base::HistogramTester histogram_tester;

  Annotator annotator(GURL(kTestServerUrl), std::string() /* api_key */,
                      kThrottle, 3 /* batch_size */,
                      1.0 /* min_ocr_confidence */,
                      test_url_factory.AsSharedURLLoaderFactory(),
                      std::make_unique<TestAnnotatorClient>());

  TestImageProcessor processor[3];
  base::Optional<mojom::AnnotateImageError> error[3];
  std::vector<mojom::Annotation> annotations[3];

  // Request annotation for images 1, 2 and 3.
  annotator.AnnotateImage(
      kImage1Url, kDescLang, processor[0].GetPendingRemote(),
      base::BindOnce(&ReportResult, &error[0], &annotations[0]));
  annotator.AnnotateImage(
      kImage2Url, kDescLang, processor[1].GetPendingRemote(),
      base::BindOnce(&ReportResult, &error[1], &annotations[1]));
  annotator.AnnotateImage(
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
  test_task_env.FastForwardBy(base::TimeDelta::FromSeconds(1));

  // A single HTTP request for all images should have been sent.
  test_url_factory.ExpectRequestAndSimulateResponse(
      "annotation", {} /* expected_headers */,
      // Only image 1 includes a description request (as the other two violate
      // one of the policies).
      ReformatJson(R"(
        {
          "imageRequests": [
            {
              "imageId": "https://www.example.com/image3.jpg",
              "imageBytes": "BwgJ",
              "engineParameters": [
                {"ocrParameters": {}}
              ]
            },
            {
              "imageId": "https://www.example.com/image2.jpg",
              "imageBytes": "BAUG",
              "engineParameters": [
                {"ocrParameters": {}}
              ]
            },
            {
              "imageId": "https://www.example.com/image1.jpg",
              "imageBytes": "AQID",
              "engineParameters": [
                {"ocrParameters": {}},
                {"descriptionParameters": {}}
              ]
            }
          ]
        }
      )"),
      R"(
        {
          "results": [
            {
              "imageId": "https://www.example.com/image2.jpg",
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
              "imageId": "https://www.example.com/image1.jpg",
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
              "imageId": "https://www.example.com/image3.jpg",
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
  ASSERT_THAT(error, ElementsAre(base::nullopt, base::nullopt, base::nullopt));
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
  data_decoder::test::InProcessDataDecoder in_process_data_decoder;
  base::HistogramTester histogram_tester;

  Annotator annotator(GURL(kTestServerUrl), std::string() /* api_key */,
                      kThrottle, 3 /* batch_size */,
                      1.0 /* min_ocr_confidence */,
                      test_url_factory.AsSharedURLLoaderFactory(),
                      std::make_unique<TestAnnotatorClient>());

  TestImageProcessor processor[3];
  base::Optional<mojom::AnnotateImageError> error[3];
  std::vector<mojom::Annotation> annotations[3];

  // Request annotation for one image in two languages, and one other image in
  // one language.
  annotator.AnnotateImage(
      kImage1Url, "fr", processor[0].GetPendingRemote(),
      base::BindOnce(&ReportResult, &error[0], &annotations[0]));
  annotator.AnnotateImage(
      kImage1Url, "en-AU", processor[1].GetPendingRemote(),
      base::BindOnce(&ReportResult, &error[1], &annotations[1]));
  annotator.AnnotateImage(
      kImage2Url, "en-US", processor[2].GetPendingRemote(),
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
  test_task_env.FastForwardBy(base::TimeDelta::FromSeconds(1));

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
              "imageId": "https://www.example.com/image2.jpg en-US",
              "imageBytes": "BAUG",
              "engineParameters": [
                {"ocrParameters": {}}
              ]
            },
            {
              "imageId": "https://www.example.com/image1.jpg en-AU",
              "imageBytes": "AQID",
              "engineParameters": [
                {"ocrParameters": {}},
                {
                  "descriptionParameters": {
                    "preferredLanguages": ["en-AU"]
                  }
                }
              ]
            },
            {
              "imageId": "https://www.example.com/image1.jpg fr",
              "imageBytes": "AQID",
              "engineParameters": [
                {"ocrParameters": {}},
                {
                  "descriptionParameters": {
                    "preferredLanguages": ["fr"]
                  }
                }
              ]
            }
          ]
        }
      )"),
      R"(
        {
          "results": [
            {
              "imageId": "https://www.example.com/image1.jpg en-AU",
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
              "imageId": "https://www.example.com/image1.jpg fr",
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
              "imageId": "https://www.example.com/image2.jpg en-US",
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
  ASSERT_THAT(error, ElementsAre(base::nullopt, base::nullopt, base::nullopt));
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

// Test that the specified API key is sent, but only to Google-associated server
// domains.
TEST(AnnotatorTest, ApiKey) {
  base::test::TaskEnvironment test_task_env(
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);
  data_decoder::test::InProcessDataDecoder in_process_data_decoder;

  // A call to a secure Google-owner server URL should include the specified API
  // key.
  {
    TestServerURLLoaderFactory test_url_factory(
        "https://ia-pa.googleapis.com/v1/");

    Annotator annotator(GURL(kTestServerUrl), "my_api_key", kThrottle,
                        1 /* batch_size */, 1.0 /* min_ocr_confidence */,
                        test_url_factory.AsSharedURLLoaderFactory(),
                        std::make_unique<TestAnnotatorClient>());
    TestImageProcessor processor;

    annotator.AnnotateImage(kImage1Url, kDescLang, processor.GetPendingRemote(),
                            base::DoNothing());
    test_task_env.RunUntilIdle();

    // Annotator should have asked processor for pixels.
    ASSERT_THAT(processor.callbacks(), SizeIs(1));

    // Send back image data.
    std::move(processor.callbacks()[0]).Run({1, 2, 3}, kDescDim, kDescDim);
    processor.callbacks().pop_back();
    test_task_env.FastForwardBy(base::TimeDelta::FromSeconds(1));
    test_task_env.RunUntilIdle();

    // HTTP request should have been made with the API key included.
    test_url_factory.ExpectRequestAndSimulateResponse(
        "annotation", {{Annotator::kGoogApiKeyHeader, "my_api_key"}},
        ReformatJson(base::StringPrintf(kTemplateRequest, kImage1Url, "AQID")),
        kOcrSuccessResponse, net::HTTP_OK);
  }

  // A call to a Google-owned server URL should not include the API key if the
  // requests are made insecurely.
  {
    // Note: not HTTPS.
    TestServerURLLoaderFactory test_url_factory(
        "http://ia-pa.googleapis.com/v1/");

    Annotator annotator(GURL("http://ia-pa.googleapis.com/v1/annotation"),
                        "my_api_key", kThrottle, 1 /* batch_size */,
                        1.0 /* min_ocr_confidence */,
                        test_url_factory.AsSharedURLLoaderFactory(),
                        std::make_unique<TestAnnotatorClient>());
    TestImageProcessor processor;

    annotator.AnnotateImage(kImage1Url, kDescLang, processor.GetPendingRemote(),
                            base::DoNothing());
    test_task_env.RunUntilIdle();

    // Annotator should have asked processor for pixels.
    ASSERT_THAT(processor.callbacks(), SizeIs(1));

    // Send back image data.
    std::move(processor.callbacks()[0]).Run({1, 2, 3}, kDescDim, kDescDim);
    processor.callbacks().pop_back();
    test_task_env.FastForwardBy(base::TimeDelta::FromSeconds(1));
    test_task_env.RunUntilIdle();

    // HTTP request should have been made without the API key included.
    test_url_factory.ExpectRequestAndSimulateResponse(
        "annotation", {{Annotator::kGoogApiKeyHeader, base::nullopt}},
        ReformatJson(base::StringPrintf(kTemplateRequest, kImage1Url, "AQID")),
        kOcrSuccessResponse, net::HTTP_OK);
  }

  // A call to a non-Google-owned URL should not include the API key.
  {
    TestServerURLLoaderFactory test_url_factory("https://datascraper.com/");

    Annotator annotator(GURL("https://datascraper.com/annotation"),
                        "my_api_key", kThrottle, 1 /* batch_size */,
                        1.0 /* min_ocr_confidence */,
                        test_url_factory.AsSharedURLLoaderFactory(),
                        std::make_unique<TestAnnotatorClient>());
    TestImageProcessor processor;

    annotator.AnnotateImage(kImage1Url, kDescLang, processor.GetPendingRemote(),
                            base::DoNothing());
    test_task_env.RunUntilIdle();

    // Annotator should have asked processor for pixels.
    ASSERT_THAT(processor.callbacks(), SizeIs(1));

    // Send back image data.
    std::move(processor.callbacks()[0]).Run({1, 2, 3}, kDescDim, kDescDim);
    processor.callbacks().pop_back();
    test_task_env.FastForwardBy(base::TimeDelta::FromSeconds(1));
    test_task_env.RunUntilIdle();

    // HTTP request should have been made without the API key included.
    test_url_factory.ExpectRequestAndSimulateResponse(
        "annotation", {{Annotator::kGoogApiKeyHeader, base::nullopt}},
        ReformatJson(base::StringPrintf(kTemplateRequest, kImage1Url, "AQID")),
        kOcrSuccessResponse, net::HTTP_OK);
  }
}

}  // namespace image_annotation
