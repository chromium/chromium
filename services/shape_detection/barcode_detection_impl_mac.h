// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_SHAPE_DETECTION_BARCODE_DETECTION_IMPL_MAC_H_
#define SERVICES_SHAPE_DETECTION_BARCODE_DETECTION_IMPL_MAC_H_

#include <os/availability.h>

#include <vector>

#include "base/mac/scoped_nsobject.h"
#include "services/shape_detection/public/mojom/barcodedetection.mojom.h"
#include "services/shape_detection/public/mojom/barcodedetection_provider.mojom.h"
#include "third_party/skia/include/core/SkBitmap.h"

@class CIDetector;

namespace shape_detection {

// This class is the implementation of Barcode Detection based on Core Image.
// This is used in some cases on macOS 10.14 when Vision is broken. When macOS
// 10.14 is no longer supported by Chromium, remove.
class BarcodeDetectionImplMac
    : public shape_detection::mojom::BarcodeDetection {
 public:
  BarcodeDetectionImplMac();

  BarcodeDetectionImplMac(const BarcodeDetectionImplMac&) = delete;
  BarcodeDetectionImplMac& operator=(const BarcodeDetectionImplMac&) = delete;

  ~BarcodeDetectionImplMac() override;

  void Detect(const SkBitmap& bitmap,
              shape_detection::mojom::BarcodeDetection::DetectCallback callback)
      override;

  static std::vector<shape_detection::mojom::BarcodeFormat>
  GetSupportedSymbologies();

 private:
  base::scoped_nsobject<CIDetector> detector_;
};

}  // namespace shape_detection

#endif  // SERVICES_SHAPE_DETECTION_BARCODE_DETECTION_IMPL_MAC_H_
