// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_SHAPE_DETECTION_DETECTION_UTILS_MAC_H_
#define SERVICES_SHAPE_DETECTION_DETECTION_UTILS_MAC_H_

#import <CoreImage/CoreImage.h>
#include <memory>

#include "base/callback.h"
#include "base/mac/availability.h"
#include "base/mac/scoped_nsobject.h"
#include "base/mac/sdk_forward_declarations.h"
#include "base/macros.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/geometry/rect_f.h"

namespace shape_detection {

// Takes a ScopedSharedBufferHandle with dimensions and produces a new CIImage
// with the same contents, or a null scoped_nsobject is something goes wrong.
base::scoped_nsobject<CIImage> CreateCIImageFromSkBitmap(
    const SkBitmap& bitmap);

gfx::RectF ConvertCGToGfxCoordinates(CGRect bounds, int height);

// This class submits an image analysis request for asynchronous execution on a
// dispatch queue with default priority.
class API_AVAILABLE(macos(10.13)) VisionAPIAsyncRequestMac {
 public:
  // A callback run when the asynchronous execution completes. The callback is
  // repeating for the instance.
  using Callback =
      base::RepeatingCallback<void(VNRequest* request, NSError* error)>;

  ~VisionAPIAsyncRequestMac();

  // Creates an VisionAPIAsyncRequestMac instance which sets |callback| to be
  // called when the asynchronous action completes.
  static std::unique_ptr<VisionAPIAsyncRequestMac> Create(
      Class request_class,
      Callback callback,
      NSSet<NSString*>* symbology_hints = nullptr);

  // Processes asynchronously an image analysis request and returns results with
  // |callback_| when the asynchronous request completes, the callers should
  // only enqueue one request at a timer.
  bool PerformRequest(const SkBitmap& bitmap);

 private:
  VisionAPIAsyncRequestMac(Callback callback,
                           Class request_class,
                           NSSet<NSString*>* symbology_hints);

  base::scoped_nsobject<VNRequest> request_;
  const Callback callback_;

  DISALLOW_COPY_AND_ASSIGN(VisionAPIAsyncRequestMac);
};

}  // namespace shape_detection

#endif  // SERVICES_SHAPE_DETECTION_DETECTION_UTILS_MAC_H_
