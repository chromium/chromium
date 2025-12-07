// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/mac/color_space_util_mac.h"

#include <CoreFoundation/CoreFoundation.h>
#include <CoreVideo/CoreVideo.h>
#include <simd/simd.h>
#include <vector>

#include "base/apple/foundation_util.h"
#include "base/apple/scoped_cftyperef.h"
#include "base/memory/scoped_policy.h"
#include "base/no_destructor.h"
#include "third_party/skia/modules/skcms/skcms.h"
#include "ui/gfx/mac/color_space_util.h"

namespace media {

gfx::ColorSpace GetImageBufferColorSpace(CVImageBufferRef image_buffer) {
  base::apple::ScopedCFTypeRef<CFTypeRef> color_primaries(
      CVBufferCopyAttachment(image_buffer, kCVImageBufferColorPrimariesKey,
                             /*attachmentMode=*/nullptr));
  base::apple::ScopedCFTypeRef<CFTypeRef> transfer_function(
      CVBufferCopyAttachment(image_buffer, kCVImageBufferTransferFunctionKey,
                             /*attachmentMode=*/nullptr));
  base::apple::ScopedCFTypeRef<CFTypeRef> gamma_level(CVBufferCopyAttachment(
      image_buffer, kCVImageBufferGammaLevelKey, /*attachmentMode=*/nullptr));
  base::apple::ScopedCFTypeRef<CFTypeRef> ycbcr_matrix(CVBufferCopyAttachment(
      image_buffer, kCVImageBufferYCbCrMatrixKey, /*attachmentMode=*/nullptr));

  return gfx::ColorSpaceFromCVImageBufferKeys(
      color_primaries.get(), transfer_function.get(), gamma_level.get(),
      ycbcr_matrix.get());
}

gfx::ColorSpace GetFormatDescriptionColorSpace(
    CMFormatDescriptionRef format_description) {
  return gfx::ColorSpaceFromCVImageBufferKeys(
      CMFormatDescriptionGetExtension(
          format_description, kCMFormatDescriptionExtension_ColorPrimaries),
      CMFormatDescriptionGetExtension(
          format_description, kCMFormatDescriptionExtension_TransferFunction),
      CMFormatDescriptionGetExtension(format_description,
                                      kCMFormatDescriptionExtension_GammaLevel),
      CMFormatDescriptionGetExtension(
          format_description, kCMFormatDescriptionExtension_YCbCrMatrix));
}

// Converts a gfx::ColorSpace to individual kCVImageBuffer* keys.
bool GetImageBufferColorValues(const gfx::ColorSpace& color_space,
                               CFStringRef* out_primaries,
                               CFStringRef* out_transfer,
                               CFStringRef* out_matrix) {
  return gfx::ColorSpaceToCVImageBufferKeys(
      color_space,
      /*prefer_srgb_trfn=*/false, out_primaries, out_transfer, out_matrix);
}

}  // namespace media
