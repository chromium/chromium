// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/shape_detection/barcode_detection_impl_mac_vision_api.h"

#include "base/logging.h"

namespace shape_detection {

namespace {

class VisionAPI : public VisionAPIInterface {
 public:
  VisionAPI() = default;

  ~VisionAPI() override = default;

  NSArray<VNBarcodeSymbology>* GetSupportedSymbologies() const override {
    return [VNDetectBarcodesRequest supportedSymbologies];
  }
};

}  // namespace

// static
std::unique_ptr<VisionAPIInterface> VisionAPIInterface::Create() {
  return std::make_unique<VisionAPI>();
}

}  // namespace shape_detection
