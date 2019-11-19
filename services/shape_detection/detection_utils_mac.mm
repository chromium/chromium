// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/shape_detection/detection_utils_mac.h"

#include <vector>

#include "base/bind.h"
#include "base/logging.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/memory/ptr_util.h"
#include "base/numerics/checked_math.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/sys_string_conversions.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "third_party/skia/include/utils/mac/SkCGUtils.h"

namespace shape_detection {

base::scoped_nsobject<CIImage> CreateCIImageFromSkBitmap(
    const SkBitmap& bitmap) {
  base::CheckedNumeric<uint32_t> num_pixels =
      base::CheckedNumeric<uint32_t>(bitmap.width()) * bitmap.height();
  base::CheckedNumeric<uint32_t> num_bytes = num_pixels * 4;
  if (!num_bytes.IsValid()) {
    DLOG(ERROR) << "Data overflow";
    return base::scoped_nsobject<CIImage>();
  }

  // First convert SkBitmap to CGImageRef.
  base::ScopedCFTypeRef<CGImageRef> cg_image(
      SkCreateCGImageRefWithColorspace(bitmap, NULL));
  if (!cg_image) {
    DLOG(ERROR) << "Failed to create CGImageRef";
    return base::scoped_nsobject<CIImage>();
  }

  base::scoped_nsobject<CIImage> ci_image(
      [[CIImage alloc] initWithCGImage:cg_image]);
  if (!ci_image) {
    DLOG(ERROR) << "Failed to create CIImage";
    return base::scoped_nsobject<CIImage>();
  }
  return ci_image;
}

gfx::RectF ConvertCGToGfxCoordinates(CGRect bounds, int height) {
  // In the default Core Graphics coordinate space, the origin is located
  // in the lower-left corner, and thus |ci_image| is flipped vertically.
  // We need to adjust |y| coordinate of bounding box before sending it.
  return gfx::RectF(bounds.origin.x,
                    height - bounds.origin.y - bounds.size.height,
                    bounds.size.width, bounds.size.height);
}

// static
// Creates an VisionAPIAsyncRequestMac instance which sets |callback| to be
// called when the asynchronous action completes.
std::unique_ptr<VisionAPIAsyncRequestMac> VisionAPIAsyncRequestMac::Create(
    Class request_class,
    Callback callback,
    NSSet<NSString*>* symbology_hints) {
  return base::WrapUnique(new VisionAPIAsyncRequestMac(
      std::move(callback), request_class, symbology_hints));
}

VisionAPIAsyncRequestMac::VisionAPIAsyncRequestMac(
    Callback callback,
    Class request_class,
    NSSet<NSString*>* symbology_hints)
    : callback_(std::move(callback)) {
  DCHECK(callback_);

  scoped_refptr<base::SequencedTaskRunner> task_runner =
      base::SequencedTaskRunnerHandle::Get();

  const auto handler = ^(VNRequest* request, NSError* error) {
    task_runner->PostTask(FROM_HERE, base::BindOnce(callback_, request, error));
  };

  request_.reset([[request_class alloc] initWithCompletionHandler:handler]);

  // Pass symbology hints to request.
  SEL sel = NSSelectorFromString(@"setSymbologies:");
  if ([symbology_hints count] > 0)
    [request_ performSelector:sel withObject:symbology_hints];
}

VisionAPIAsyncRequestMac::~VisionAPIAsyncRequestMac() = default;

// Processes asynchronously an image analysis request and returns results with
// |callback_| when the asynchronous request completes.
bool VisionAPIAsyncRequestMac::PerformRequest(const SkBitmap& bitmap) {
  Class image_handler_class = NSClassFromString(@"VNImageRequestHandler");
  if (!image_handler_class) {
    DLOG(ERROR) << "Failed to load VNImageRequestHandler class";
    return false;
  }

  base::scoped_nsobject<CIImage> ci_image = CreateCIImageFromSkBitmap(bitmap);
  if (!ci_image) {
    DLOG(ERROR) << "Failed to create image from SkBitmap";
    return false;
  }

  base::scoped_nsobject<VNImageRequestHandler> image_handler(
      [[image_handler_class alloc] initWithCIImage:ci_image options:@{}]);
  if (!image_handler) {
    DLOG(ERROR) << "Failed to create image request handler";
    return false;
  }

  dispatch_async(
      dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        NSError* ns_error = nil;
        if ([image_handler performRequests:@[ request_ ] error:&ns_error])
          return;
        DLOG(ERROR) << base::SysNSStringToUTF8([ns_error localizedDescription]);
      });
  return true;
}

}  // namespace shape_detection
