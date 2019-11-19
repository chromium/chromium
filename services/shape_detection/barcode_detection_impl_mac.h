// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_SHAPE_DETECTION_BARCODE_DETECTION_IMPL_MAC_H_
#define SERVICES_SHAPE_DETECTION_BARCODE_DETECTION_IMPL_MAC_H_

#include <vector>

#include "base/mac/availability.h"
#include "base/mac/scoped_nsobject.h"
#include "services/shape_detection/public/mojom/barcodedetection.mojom.h"
#include "services/shape_detection/public/mojom/barcodedetection_provider.mojom.h"
#include "third_party/skia/include/core/SkBitmap.h"

@class CIDetector;

namespace shape_detection {

class API_AVAILABLE(macosx(10.10)) BarcodeDetectionImplMac
    : public shape_detection::mojom::BarcodeDetection {
 public:
  BarcodeDetectionImplMac();
  ~BarcodeDetectionImplMac() override;

  void Detect(const SkBitmap& bitmap,
              shape_detection::mojom::BarcodeDetection::DetectCallback callback)
      override;

  static std::vector<shape_detection::mojom::BarcodeFormat>
  GetSupportedSymbologies();

 private:
  base::scoped_nsobject<CIDetector> detector_;

  DISALLOW_COPY_AND_ASSIGN(BarcodeDetectionImplMac);
};

}  // namespace shape_detection

#endif  // SERVICES_SHAPE_DETECTION_BARCODE_DETECTION_IMPL_MAC_H_
