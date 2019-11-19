// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_SHAPE_DETECTION_BARCODE_DETECTION_PROVIDER_MAC_H_
#define SERVICES_SHAPE_DETECTION_BARCODE_DETECTION_PROVIDER_MAC_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "services/shape_detection/barcode_detection_impl_mac_vision_api.h"
#include "services/shape_detection/public/mojom/barcodedetection.mojom.h"
#include "services/shape_detection/public/mojom/barcodedetection_provider.mojom.h"

class VisionAPIInterface;

namespace shape_detection {

// The BarcodeDetectionProviderMac class is a provider that binds an
// implementation of mojom::BarcodeDetection with Core Image or Vision
// Framework.
class BarcodeDetectionProviderMac
    : public shape_detection::mojom::BarcodeDetectionProvider {
 public:
  BarcodeDetectionProviderMac();
  explicit BarcodeDetectionProviderMac(std::unique_ptr<VisionAPIInterface>);
  ~BarcodeDetectionProviderMac() override;

  // Binds BarcodeDetection provider receiver to the implementation of
  // mojom::BarcodeDetectionProvider.
  static void Create(
      mojo::PendingReceiver<mojom::BarcodeDetectionProvider> receiver);

  void CreateBarcodeDetection(
      mojo::PendingReceiver<mojom::BarcodeDetection> receiver,
      mojom::BarcodeDetectorOptionsPtr options) override;
  void EnumerateSupportedFormats(
      EnumerateSupportedFormatsCallback callback) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(BarcodeDetectionProviderMac);

  base::Optional<std::vector<mojom::BarcodeFormat>> supported_formats_;
  std::unique_ptr<VisionAPIInterface> vision_api_;
};

}  // namespace shape_detection

#endif  // SERVICES_SHAPE_DETECTION_BARCODE_DETECTION_PROVIDER_MAC_H_
