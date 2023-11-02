// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_SHAPE_DETECTION_BARCODE_DETECTION_PROVIDER_BARHOPPER_H_
#define SERVICES_SHAPE_DETECTION_BARCODE_DETECTION_PROVIDER_BARHOPPER_H_

#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "services/shape_detection/public/mojom/barcodedetection_provider.mojom.h"

namespace shape_detection {

// BarcodeDetectionProviderBarhopper class is a provider that binds to
// a BarcodeDetection implementation based on the barhopper library.
class BarcodeDetectionProviderBarhopper
    : public mojom::BarcodeDetectionProvider {
 public:
  BarcodeDetectionProviderBarhopper() = default;
  BarcodeDetectionProviderBarhopper(const BarcodeDetectionProviderBarhopper&) =
      delete;
  BarcodeDetectionProviderBarhopper& operator=(
      const BarcodeDetectionProviderBarhopper&) = delete;
  ~BarcodeDetectionProviderBarhopper() override = default;

  static void Create(
      mojo::PendingReceiver<mojom::BarcodeDetectionProvider> receiver);

  // mojom::BarcodeDetectionProvider:
  void CreateBarcodeDetection(
      mojo::PendingReceiver<shape_detection::mojom::BarcodeDetection> receiver,
      shape_detection::mojom::BarcodeDetectorOptionsPtr options) override;
  void EnumerateSupportedFormats(
      EnumerateSupportedFormatsCallback callback) override;
};

}  // namespace shape_detection
#endif  // SERVICES_SHAPE_DETECTION_BARCODE_DETECTION_PROVIDER_BARHOPPER_H_