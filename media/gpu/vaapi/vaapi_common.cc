// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/vaapi/vaapi_common.h"

namespace media {

VaapiH264Picture::VaapiH264Picture(scoped_refptr<VASurface> va_surface)
    : va_surface_(va_surface) {}

VaapiH264Picture::~VaapiH264Picture() {}

VaapiH264Picture* VaapiH264Picture::AsVaapiH264Picture() {
  return this;
}

VaapiVP8Picture::VaapiVP8Picture(scoped_refptr<VASurface> va_surface)
    : va_surface_(va_surface) {}

VaapiVP8Picture::~VaapiVP8Picture() {}

VaapiVP8Picture* VaapiVP8Picture::AsVaapiVP8Picture() {
  return this;
}

VaapiVP9Picture::VaapiVP9Picture(scoped_refptr<VASurface> va_surface)
    : va_surface_(va_surface) {}

VaapiVP9Picture::~VaapiVP9Picture() {}

VaapiVP9Picture* VaapiVP9Picture::AsVaapiVP9Picture() {
  return this;
}

scoped_refptr<VP9Picture> VaapiVP9Picture::CreateDuplicate() {
  return new VaapiVP9Picture(va_surface_);
}

}  // namespace media
