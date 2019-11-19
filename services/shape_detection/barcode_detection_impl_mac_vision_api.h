// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_SHAPE_DETECTION_BARCODE_DETECTION_IMPL_MAC_VISION_API_H_
#define SERVICES_SHAPE_DETECTION_BARCODE_DETECTION_IMPL_MAC_VISION_API_H_

#include <memory>

#ifdef __OBJC__

#include <CoreFoundation/CoreFoundation.h>

#include "base/mac/scoped_nsobject.h"

namespace shape_detection {

class VisionAPIInterface {
 public:
  VisionAPIInterface() {}
  virtual ~VisionAPIInterface() {}

  static std::unique_ptr<VisionAPIInterface> Create();

  virtual NSArray* GetSupportedSymbologies() const = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(VisionAPIInterface);
};

}  // namespace shape_detection

#endif  // __OBJC__

#endif  // SERVICES_SHAPE_DETECTION_BARCODE_DETECTION_IMPL_MAC_VISION_API_H_
