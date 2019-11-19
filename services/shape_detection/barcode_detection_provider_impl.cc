// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/shape_detection/barcode_detection_provider_impl.h"

#include "base/logging.h"

namespace shape_detection {

void BarcodeDetectionProviderImpl::CreateBarcodeDetection(
    mojo::PendingReceiver<shape_detection::mojom::BarcodeDetection> receiver,
    shape_detection::mojom::BarcodeDetectorOptionsPtr options) {
  DLOG(ERROR) << "Platform not supported for Barcode Detection Service.";
}

void BarcodeDetectionProviderImpl::EnumerateSupportedFormats(
    EnumerateSupportedFormatsCallback callback) {
  DLOG(ERROR) << "Platform not supported for Barcode Detection Service.";
  std::move(callback).Run({});
}

}  // namespace shape_detection
