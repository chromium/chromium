// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_SHAPE_DETECTION_BARCODE_DETECTION_IMPL_MAC_VISION_API_H_
#define SERVICES_SHAPE_DETECTION_BARCODE_DETECTION_IMPL_MAC_VISION_API_H_

#ifdef __OBJC__

#import <Foundation/Foundation.h>
#import <Vision/Vision.h>

#include <memory>

namespace shape_detection {

class VisionAPIInterface {
 public:
  VisionAPIInterface() = default;
  virtual ~VisionAPIInterface() = default;
  VisionAPIInterface(const VisionAPIInterface&) = delete;
  VisionAPIInterface& operator=(const VisionAPIInterface&) = delete;

  static std::unique_ptr<VisionAPIInterface> Create();

  virtual NSArray<VNBarcodeSymbology>* GetSupportedSymbologies() const = 0;
};

}  // namespace shape_detection

#endif  // __OBJC__

#endif  // SERVICES_SHAPE_DETECTION_BARCODE_DETECTION_IMPL_MAC_VISION_API_H_
