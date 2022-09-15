// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_SHAPE_DETECTION_BARCODE_DETECTION_IMPL_BARHOPPER_H_
#define SERVICES_SHAPE_DETECTION_BARCODE_DETECTION_IMPL_BARHOPPER_H_

#include "services/shape_detection/public/mojom/barcodedetection.mojom.h"
#include "services/shape_detection/public/mojom/barcodedetection_provider.mojom.h"
#include "third_party/barhopper/barhopper/barhopper.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace shape_detection {

class BarcodeDetectionImplBarhopper : public mojom::BarcodeDetection {
 public:
  explicit BarcodeDetectionImplBarhopper(
      mojom::BarcodeDetectorOptionsPtr options);
  BarcodeDetectionImplBarhopper(const BarcodeDetectionImplBarhopper&) = delete;
  BarcodeDetectionImplBarhopper& operator=(
      const BarcodeDetectionImplBarhopper&) = delete;
  ~BarcodeDetectionImplBarhopper() override;

  // mojom::BarcodeDetection:
  void Detect(const SkBitmap& bitmap,
              shape_detection::mojom::BarcodeDetection::DetectCallback callback)
      override;

  static std::vector<shape_detection::mojom::BarcodeFormat>
  GetSupportedFormats();

 private:
  barhopper::RecognitionOptions recognition_options_;
};

}  // namespace shape_detection

#endif  // SERVICES_SHAPE_DETECTION_BARCODE_DETECTION_IMPL_BARHOPPER_H_