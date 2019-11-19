// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/shape_detection/barcode_detection_impl_mac_vision_api.h"
#include "base/logging.h"
#include "base/mac/sdk_forward_declarations.h"

namespace shape_detection {

namespace {

class VisionAPI : public VisionAPIInterface {
 public:
  VisionAPI() = default;

  ~VisionAPI() override = default;

  NSArray* GetSupportedSymbologies() const override {
    Class request_class = NSClassFromString(@"VNDetectBarcodesRequest");
    if (!request_class) {
      DPLOG(ERROR) << "Failed to load VNDetectBarcodesRequest class";
      return [NSArray array];
    }

    SEL sel = NSSelectorFromString(@"supportedSymbologies");
    id symbologies = [request_class performSelector:sel];
    if (![symbologies isKindOfClass:[NSArray class]]) {
      DLOG(ERROR)
          << "Failed to get NSArray of supportedSymbologies (wrong type)";
      return [NSArray array];
    }
    return symbologies;
  }
};

}  // anonymous namespace

// static
std::unique_ptr<VisionAPIInterface> VisionAPIInterface::Create() {
  return std::make_unique<VisionAPI>();
}

}  // namespace shape_detection
