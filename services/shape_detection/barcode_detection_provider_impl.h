// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_SHAPE_DETECTION_BARCODE_DETECTION_PROVIDER_IMPL_H_
#define SERVICES_SHAPE_DETECTION_BARCODE_DETECTION_PROVIDER_IMPL_H_

#include <memory>
#include <utility>

#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "services/shape_detection/public/mojom/barcodedetection_provider.mojom.h"

namespace shape_detection {

class BarcodeDetectionProviderImpl
    : public shape_detection::mojom::BarcodeDetectionProvider {
 public:
  ~BarcodeDetectionProviderImpl() override = default;

  static void Create(
      mojo::PendingReceiver<shape_detection::mojom::BarcodeDetectionProvider>
          receiver) {
    mojo::MakeSelfOwnedReceiver(
        std::make_unique<BarcodeDetectionProviderImpl>(), std::move(receiver));
  }

  void CreateBarcodeDetection(
      mojo::PendingReceiver<shape_detection::mojom::BarcodeDetection> receiver,
      shape_detection::mojom::BarcodeDetectorOptionsPtr options) override;
  void EnumerateSupportedFormats(
      EnumerateSupportedFormatsCallback callback) override;
};

}  // namespace shape_detection

#endif  // SERVICES_SHAPE_DETECTION_BARCODE_DETECTION_PROVIDER_IMPL_H_
