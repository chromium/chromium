// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_SHARED_IMPL_PPB_IMAGE_DATA_SHARED_H_
#define PPAPI_SHARED_IMPL_PPB_IMAGE_DATA_SHARED_H_

#include "ppapi/c/pp_bool.h"
#include "ppapi/c/ppb_image_data.h"
#include "ppapi/shared_impl/ppapi_shared_export.h"

namespace ppapi {

// Contains the implementation of some simple image data functions that are
// shared between the proxy and Chrome's implementation. Since these functions
// are just lists of what we support, it's much easier to just have the same
// code run in the plugin process than to proxy it.
//
// It's possible the implementation will get more complex. In this case, it's
// probably best to have some kind of "configuration" message that the renderer
// sends to the plugin process on startup that contains all of these kind of
// settings.
class PPAPI_SHARED_EXPORT PPB_ImageData_Shared {
 public:
  enum ImageDataType {
    // An ImageData backed by a PlatformCanvas. You must create this type if
    // you intend the ImageData to be usable in platform-specific APIs (like
    // font rendering or rendering widgets like scrollbars). This type is not
    // available in untrusted (NaCl) plugins.
    PLATFORM,
    // An ImageData that doesn't need access to the platform-specific canvas.
    // This is backed by a simple shared memory buffer. This is the only type
    // of ImageData that can be used by untrusted (NaCl) plugins.
    SIMPLE
  };

  static PP_ImageDataFormat GetNativeImageDataFormat();
  static PP_Bool IsImageDataFormatSupported(PP_ImageDataFormat format);
  static PP_Bool IsImageDataDescValid(const PP_ImageDataDesc& desc);
};

}  // namespace ppapi

#endif  // PPAPI_SHARED_IMPL_PPB_IMAGE_DATA_SHARED_H_
