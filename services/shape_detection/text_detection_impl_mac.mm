// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/shape_detection/text_detection_impl_mac.h"

#include "base/apple/scoped_cftyperef.h"
#include "base/strings/sys_string_conversions.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "services/shape_detection/detection_utils_mac.h"
#include "services/shape_detection/text_detection_impl.h"

namespace shape_detection {

// static
void TextDetectionImpl::Create(
    mojo::PendingReceiver<mojom::TextDetection> receiver) {
  mojo::MakeSelfOwnedReceiver(std::make_unique<TextDetectionImplMac>(),
                              std::move(receiver));
}

TextDetectionImplMac::TextDetectionImplMac() {
  NSDictionary* const opts = @{CIDetectorAccuracy : CIDetectorAccuracyHigh};
  detector_ = [CIDetector detectorOfType:CIDetectorTypeText
                                 context:nil
                                 options:opts];
}

TextDetectionImplMac::~TextDetectionImplMac() = default;

void TextDetectionImplMac::Detect(const SkBitmap& bitmap,
                                  DetectCallback callback) {
  CIImage* ci_image = CIImageFromSkBitmap(bitmap);
  if (!ci_image) {
    std::move(callback).Run({});
    return;
  }

  NSArray* const features = [detector_ featuresInImage:ci_image];

  const int height = bitmap.height();
  std::vector<mojom::TextDetectionResultPtr> results;
  for (CIRectangleFeature* const f in features) {
    // CIRectangleFeature only has bounding box information.
    auto result = mojom::TextDetectionResult::New();
    result->bounding_box = ConvertCGToGfxCoordinates(f.bounds, height);

    // Enumerate corner points starting from top-left in clockwise fashion:
    // https://wicg.github.io/shape-detection-api/text.html#dom-detectedtext-cornerpoints
    result->corner_points.emplace_back(f.topLeft.x, height - f.topLeft.y);
    result->corner_points.emplace_back(f.topRight.x, height - f.topRight.y);
    result->corner_points.emplace_back(f.bottomRight.x,
                                       height - f.bottomRight.y);
    result->corner_points.emplace_back(f.bottomLeft.x, height - f.bottomLeft.y);

    results.push_back(std::move(result));
  }
  std::move(callback).Run(std::move(results));
}

}  // namespace shape_detection
