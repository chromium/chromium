// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/shape_detection/detection_utils_mac.h"

#import <Vision/Vision.h>

#include <vector>

#include "base/apple/foundation_util.h"
#include "base/apple/scoped_cftyperef.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/numerics/checked_math.h"
#include "base/strings/sys_string_conversions.h"
#import "base/task/sequenced_task_runner.h"
#include "base/task/sequenced_task_runner.h"
#include "third_party/skia/include/utils/mac/SkCGUtils.h"

namespace shape_detection {

CIImage* CIImageFromSkBitmap(const SkBitmap& bitmap) {
  base::CheckedNumeric<uint32_t> num_pixels =
      base::CheckedNumeric<uint32_t>(bitmap.width()) * bitmap.height();
  base::CheckedNumeric<uint32_t> num_bytes = num_pixels * 4;
  if (!num_bytes.IsValid()) {
    DLOG(ERROR) << "Data overflow";
    return nil;
  }

  // First convert SkBitmap to CGImageRef.
  base::apple::ScopedCFTypeRef<CGImageRef> cg_image(
      SkCreateCGImageRefWithColorspace(bitmap, nullptr));
  if (!cg_image) {
    DLOG(ERROR) << "Failed to create CGImageRef";
    return nil;
  }

  CIImage* ci_image = [[CIImage alloc] initWithCGImage:cg_image.get()];
  if (!ci_image) {
    DLOG(ERROR) << "Failed to create CIImage";
    return nil;
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
    NSArray<VNBarcodeSymbology>* symbology_hints) {
  return base::WrapUnique(new VisionAPIAsyncRequestMac(
      std::move(callback), request_class, symbology_hints));
}

VisionAPIAsyncRequestMac::VisionAPIAsyncRequestMac(
    Callback callback,
    Class request_class,
    NSArray<VNBarcodeSymbology>* symbology_hints)
    : callback_(std::move(callback)) {
  DCHECK(callback_);

  scoped_refptr<base::SequencedTaskRunner> task_runner =
      base::SequencedTaskRunner::GetCurrentDefault();

  const auto handler = ^(VNRequest* request, NSError* error) {
    task_runner->PostTask(FROM_HERE, base::BindOnce(callback_, request, error));
  };

  request_ = [[request_class alloc] initWithCompletionHandler:handler];

  // Pass symbology hints to request. Only valid for VNDetectBarcodesRequest.
  if ([symbology_hints count] > 0) {
    VNDetectBarcodesRequest* barcode_request =
        base::apple::ObjCCastStrict<VNDetectBarcodesRequest>(request_);
    barcode_request.symbologies = symbology_hints;
  }
}

VisionAPIAsyncRequestMac::~VisionAPIAsyncRequestMac() = default;

// Processes asynchronously an image analysis request and returns results with
// |callback_| when the asynchronous request completes.
bool VisionAPIAsyncRequestMac::PerformRequest(const SkBitmap& bitmap) {
  CIImage* ci_image = CIImageFromSkBitmap(bitmap);
  if (!ci_image) {
    DLOG(ERROR) << "Failed to create image from SkBitmap";
    return false;
  }

  VNImageRequestHandler* image_handler =
      [[VNImageRequestHandler alloc] initWithCIImage:ci_image options:@{}];
  if (!image_handler) {
    DLOG(ERROR) << "Failed to create image request handler";
    return false;
  }

  dispatch_async(
      dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        NSError* ns_error = nil;
        if ([image_handler performRequests:@[ request_ ] error:&ns_error]) {
          return;
        }
        DLOG(ERROR) << base::SysNSStringToUTF8(ns_error.localizedDescription);
      });
  return true;
}

}  // namespace shape_detection
