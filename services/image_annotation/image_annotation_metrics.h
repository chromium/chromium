// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_IMAGE_ANNOTATION_IMAGE_ANNOTATION_METRICS_H_
#define SERVICES_IMAGE_ANNOTATION_IMAGE_ANNOTATION_METRICS_H_

#include "services/image_annotation/image_annotation_utils.h"
#include "services/image_annotation/public/mojom/image_annotation.mojom.h"

namespace image_annotation {

// Implementation details exposed only for testing. May change without warning.
namespace metrics_internal {

// TODO(crbug.com/916420): separate out client / annotation types when we have
//                         more use cases for the service.
constexpr char kCacheHit[] = "ImageAnnotationService.AccessibilityV1.CacheHit";
constexpr char kJsonParseSuccess[] =
    "ImageAnnotationService.AccessibilityV1.JsonParseSuccess";
constexpr char kPixelFetchSuccess[] =
    "ImageAnnotationService.AccessibilityV1.PixelFetchSuccess";
constexpr char kAnnotationConfidence[] =
    "ImageAnnotationService.AccessibilityV1.%s.Confidence";
constexpr char kAnnotationEmpty[] =
    "ImageAnnotationService.AccessibilityV1.%s.Empty";
constexpr char kAnnotationStatus[] =
    "ImageAnnotationService.AccessibilityV1.%s.Status";
constexpr char kDescType[] = "ImageAnnotationService.AccessibilityV1.DescType";
constexpr char kDescFailure[] =
    "ImageAnnotationService.AccessibilityV1.DescFailure";
constexpr char kEngineKnown[] =
    "ImageAnnotationService.AccessibilityV1.EngineKnown";
constexpr char kServerNetError[] =
    "ImageAnnotationService.AccessibilityV1.ServerNetError";
constexpr char kServerHttpResponseCode[] =
    "ImageAnnotationService.AccessibilityV1.ServerHttpResponseCode";
constexpr char kServerLatency[] =
    "ImageAnnotationService.AccessibilityV1.ServerLatencyMs";
constexpr char kImageRequestIncludesDesc[] =
    "ImageAnnotationService.AccessibilityV1.ImageRequestIncludesDesc";
constexpr char kServerRequestSize[] =
    "ImageAnnotationService.AccessibilityV1.ServerRequestSizeKB";
constexpr char kServerResponseSize[] =
    "ImageAnnotationService.AccessibilityV1.ServerResponseSizeBytes";
constexpr char kSourcePixelCount[] =
    "ImageAnnotationService.AccessibilityV1.SourcePixelCount";
constexpr char kEncodedJpegSize[] =
    "ImageAnnotationService.AccessibilityV1.EncodedJpegSizeKB";
constexpr char kClientResult[] =
    "ImageAnnotationService.AccessibilityV1.ClientResult";

}  // namespace metrics_internal

// An enum for reporting the end result of an image annotation request.
//
// Logged in metrics - do not reuse or reassign values.
enum class ClientResult {
  kUnknown = 0,
  kSucceeded = 1,
  kCanceled = 2,
  kFailed = 3,
  kShutdown = 4,
  kMaxValue = kShutdown,
};

// Report whether or not annotations for an image were already stored in the
// service cache.
void ReportCacheHit(bool cache_hit);

// Report whether or not JSON returned by the image annotation service was
// successfully parsed or not.
void ReportJsonParseSuccess(bool success);

// Report whether or not pixel data is successfully fetched from a client.
void ReportPixelFetchSuccess(bool success);

// Report metadata for a successful OCR annotation.
void ReportOcrAnnotation(double confidence, bool empty);

// Report metadata for a successful description annotation.
void ReportDescAnnotation(mojom::AnnotationType type, double score, bool empty);

// Report an unsuccessful description response.
void ReportDescFailure(DescFailureReason reason);

// Report the net error from the image annotation server request. This will be
// populated even if e.g. the server URL is incorrect.
void ReportServerNetError(int code);

// Report a HTTP response code from the image annotation server.
void ReportServerResponseCode(int code);

// Report the length of time taken for a response to be returned from the
// server.
void ReportServerLatency(base::TimeDelta latency);

// Report whether or not a request for image annotation includes parameters for
// the description engine; requests for images that violate the description
// engine policy (e.g. are too small) will not.
void ReportImageRequestIncludesDesc(bool includes_desc);

// Report the size of the request sent to the image annotation server.
void ReportServerRequestSizeKB(size_t size_kb);

// Report the size of the response returned by the image annotation server.
void ReportServerResponseSizeBytes(size_t size_bytes);

// Report the status code attached to the OCR engine response.
void ReportOcrStatus(int status);

// Report the status code attached to the description engine response.
void ReportDescStatus(int status);

// Report whether or not each engine is recognised as either OCR or description.
void ReportEngineKnown(bool known);

// Report the number of source (i.e. pre-scaling) pixels in an image sent to the
// image annotation service.
void ReportSourcePixelCount(size_t pixel_count);

// Report the size of a single encoded image to be sent to the image annotation
// service.
void ReportEncodedJpegSize(size_t size_kb);

// Report the result of the image annotation request for a client.
void ReportClientResult(ClientResult result);

}  // namespace image_annotation

#endif  // SERVICES_IMAGE_ANNOTATION_IMAGE_ANNOTATION_METRICS_H_
