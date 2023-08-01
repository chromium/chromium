// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_SHAPE_DETECTION_DETECTION_UTILS_MAC_H_
#define SERVICES_SHAPE_DETECTION_DETECTION_UTILS_MAC_H_

#import <CoreImage/CoreImage.h>
#import <Foundation/Foundation.h>
#import <Vision/Vision.h>

#include <memory>

#include "base/functional/callback.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/geometry/rect_f.h"

namespace shape_detection {

// Takes a ScopedSharedBufferHandle with dimensions and returns a new CIImage
// with the same contents, or nil if something goes wrong.
CIImage* CIImageFromSkBitmap(const SkBitmap& bitmap);

gfx::RectF ConvertCGToGfxCoordinates(CGRect bounds, int height);

// This class submits an image analysis request for asynchronous execution on a
// dispatch queue with default priority.
class VisionAPIAsyncRequestMac {
 public:
  // A callback run when the asynchronous execution completes. The callback is
  // repeating for the instance.
  using Callback =
      base::RepeatingCallback<void(VNRequest* request, NSError* error)>;

  VisionAPIAsyncRequestMac(const VisionAPIAsyncRequestMac&) = delete;
  VisionAPIAsyncRequestMac& operator=(const VisionAPIAsyncRequestMac&) = delete;

  ~VisionAPIAsyncRequestMac();

  // Creates an VisionAPIAsyncRequestMac instance which sets |callback| to be
  // called when the asynchronous action completes.
  static std::unique_ptr<VisionAPIAsyncRequestMac> Create(
      Class request_class,
      Callback callback,
      NSArray<VNBarcodeSymbology>* symbology_hints = nullptr);

  // Processes asynchronously an image analysis request and returns results with
  // |callback_| when the asynchronous request completes, the callers should
  // only enqueue one request at a time.
  bool PerformRequest(const SkBitmap& bitmap);

 private:
  VisionAPIAsyncRequestMac(Callback callback,
                           Class request_class,
                           NSArray<VNBarcodeSymbology>* symbology_hints);

  VNRequest* __strong request_;
  const Callback callback_;
};

}  // namespace shape_detection

#endif  // SERVICES_SHAPE_DETECTION_DETECTION_UTILS_MAC_H_
