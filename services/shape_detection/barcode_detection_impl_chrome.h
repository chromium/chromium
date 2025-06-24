// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_SHAPE_DETECTION_BARCODE_DETECTION_IMPL_CHROME_H_
#define SERVICES_SHAPE_DETECTION_BARCODE_DETECTION_IMPL_CHROME_H_

#include "services/shape_detection/chrome_shape_detection_api.h"
#include "services/shape_detection/public/mojom/barcodedetection.mojom.h"
#include "services/shape_detection/public/mojom/barcodedetection_provider.mojom.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace shape_detection {

class BarcodeDetectionImplChrome : public mojom::BarcodeDetection {
 public:
  explicit BarcodeDetectionImplChrome(mojom::BarcodeDetectorOptionsPtr options);
  BarcodeDetectionImplChrome(const BarcodeDetectionImplChrome&) = delete;
  BarcodeDetectionImplChrome& operator=(const BarcodeDetectionImplChrome&) =
      delete;
  ~BarcodeDetectionImplChrome() override;

  // mojom::BarcodeDetection:
  void Detect(const SkBitmap& bitmap,
              shape_detection::mojom::BarcodeDetection::DetectCallback callback)
      override;

  static std::vector<shape_detection::mojom::BarcodeFormat>
  GetSupportedFormats();

 private:
  const ChromeBarcodeFormat expected_formats_;
};

}  // namespace shape_detection

#endif  // SERVICES_SHAPE_DETECTION_BARCODE_DETECTION_IMPL_CHROME_H_
