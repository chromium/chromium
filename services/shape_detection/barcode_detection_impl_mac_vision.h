// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_SHAPE_DETECTION_BARCODE_DETECTION_IMPL_MAC_VISION_H_
#define SERVICES_SHAPE_DETECTION_BARCODE_DETECTION_IMPL_MAC_VISION_H_

#include <memory>
#include <utility>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "services/shape_detection/barcode_detection_impl_mac_vision_api.h"
#include "services/shape_detection/detection_utils_mac.h"
#include "services/shape_detection/public/mojom/barcodedetection.mojom.h"
#include "services/shape_detection/public/mojom/barcodedetection_provider.mojom.h"

class SkBitmap;
class VisionAPIInterface;

namespace shape_detection {

// This class is the implementation of Barcode Detection based on Mac OS Vision
// framework (https://developer.apple.com/documentation/vision).
class BarcodeDetectionImplMacVision : public mojom::BarcodeDetection {
 public:
  explicit BarcodeDetectionImplMacVision(
      mojom::BarcodeDetectorOptionsPtr options);

  BarcodeDetectionImplMacVision(const BarcodeDetectionImplMacVision&) = delete;
  BarcodeDetectionImplMacVision& operator=(
      const BarcodeDetectionImplMacVision&) = delete;

  ~BarcodeDetectionImplMacVision() override;

  void Detect(const SkBitmap& bitmap,
              mojom::BarcodeDetection::DetectCallback callback) override;

  void SetReceiver(
      mojo::SelfOwnedReceiverRef<mojom::BarcodeDetection> receiver) {
    receiver_ = std::move(receiver);
  }

  static std::vector<shape_detection::mojom::BarcodeFormat>
  GetSupportedSymbologies(VisionAPIInterface* vision_api = nullptr);

  NSArray<VNBarcodeSymbology>* GetSymbologyHintsForTesting();

 private:
  void OnBarcodesDetected(VNRequest* request, NSError* error);

  CGSize image_size_;
  NSArray<VNBarcodeSymbology>* __strong symbology_hints_;
  std::unique_ptr<VisionAPIAsyncRequestMac> barcodes_async_request_;
  DetectCallback detected_callback_;
  mojo::SelfOwnedReceiverRef<mojom::BarcodeDetection> receiver_;
  base::WeakPtrFactory<BarcodeDetectionImplMacVision> weak_factory_;
};

}  // namespace shape_detection

#endif  // SERVICES_SHAPE_DETECTION_BARCODE_DETECTION_IMPL_MAC_VISION_H_
