// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/shape_detection/barcode_detection_provider_barhopper.h"

#include <memory>
#include <utility>

#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "services/shape_detection/barcode_detection_impl_barhopper.h"
#include "services/shape_detection/public/mojom/barcodedetection.mojom.h"
#include "services/shape_detection/public/mojom/barcodedetection_provider.mojom.h"

namespace shape_detection {

// static
void BarcodeDetectionProviderBarhopper::Create(
    mojo::PendingReceiver<mojom::BarcodeDetectionProvider> receiver) {
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<BarcodeDetectionProviderBarhopper>(),
      std::move(receiver));
}

void BarcodeDetectionProviderBarhopper::CreateBarcodeDetection(
    mojo::PendingReceiver<shape_detection::mojom::BarcodeDetection> receiver,
    shape_detection::mojom::BarcodeDetectorOptionsPtr options) {
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<BarcodeDetectionImplBarhopper>(std::move(options)),
      std::move(receiver));
}

void BarcodeDetectionProviderBarhopper::EnumerateSupportedFormats(
    EnumerateSupportedFormatsCallback callback) {
  std::move(callback).Run(BarcodeDetectionImplBarhopper::GetSupportedFormats());
}

}  // namespace shape_detection