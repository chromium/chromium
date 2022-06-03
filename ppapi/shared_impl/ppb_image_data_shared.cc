// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/shared_impl/ppb_image_data_shared.h"

#include "base/notreached.h"
#include "build/build_config.h"

#if !defined(OS_NACL) && !defined(NACL_WIN64)
#include "third_party/skia/include/core/SkTypes.h"
#endif

namespace ppapi {

// static
PP_ImageDataFormat PPB_ImageData_Shared::GetNativeImageDataFormat() {
#if defined(OS_NACL)
  // In NaCl, just default to something. If we're wrong, it will be converted
  // later.
  // TODO(dmichael): Really proxy this.
  return PP_IMAGEDATAFORMAT_BGRA_PREMUL;
#elif defined(NACL_WIN64)
  // In the NaCl Win64 helper, this shouldn't be called. If we start building
  // Chrome on Windows 64 for realz, we should really implement this.
  NOTIMPLEMENTED();
  return PP_IMAGEDATAFORMAT_BGRA_PREMUL;
#else
  if (SK_B32_SHIFT == 0)
    return PP_IMAGEDATAFORMAT_BGRA_PREMUL;
  else if (SK_R32_SHIFT == 0)
    return PP_IMAGEDATAFORMAT_RGBA_PREMUL;
  else
    return PP_IMAGEDATAFORMAT_BGRA_PREMUL;  // Default to something on failure
#endif
}

// static
PP_Bool PPB_ImageData_Shared::IsImageDataFormatSupported(
    PP_ImageDataFormat format) {
  return PP_FromBool(format == PP_IMAGEDATAFORMAT_BGRA_PREMUL ||
                     format == PP_IMAGEDATAFORMAT_RGBA_PREMUL);
}

// static
PP_Bool PPB_ImageData_Shared::IsImageDataDescValid(
    const PP_ImageDataDesc& desc) {
  return PP_FromBool(IsImageDataFormatSupported(desc.format) &&
                     desc.size.width > 0 && desc.size.height > 0 &&
                     desc.stride > 0);
}

}  // namespace ppapi
