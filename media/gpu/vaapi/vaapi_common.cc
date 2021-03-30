// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/vaapi/vaapi_common.h"

#include "build/chromeos_buildflags.h"

namespace media {

VaapiH264Picture::VaapiH264Picture(scoped_refptr<VASurface> va_surface)
    : va_surface_(va_surface) {}

VaapiH264Picture::~VaapiH264Picture() = default;

VaapiH264Picture* VaapiH264Picture::AsVaapiH264Picture() {
  return this;
}

void VaapiH264Picture::SetDecodeSurface(
    scoped_refptr<VASurface> decode_va_surface) {
  decode_va_surface_ = std::move(decode_va_surface);
}

#if BUILDFLAG(ENABLE_PLATFORM_HEVC)
VaapiH265Picture::VaapiH265Picture(scoped_refptr<VASurface> va_surface)
    : va_surface_(va_surface) {}

VaapiH265Picture::~VaapiH265Picture() = default;

VaapiH265Picture* VaapiH265Picture::AsVaapiH265Picture() {
  return this;
}

void VaapiH265Picture::SetDecodeSurface(
    scoped_refptr<VASurface> decode_va_surface) {
  decode_va_surface_ = std::move(decode_va_surface);
}

#endif  // BUILDFLAG(ENABLE_PLATFORM_HEVC)

VaapiVP8Picture::VaapiVP8Picture(scoped_refptr<VASurface> va_surface)
    : va_surface_(va_surface) {}

VaapiVP8Picture::~VaapiVP8Picture() = default;

VaapiVP8Picture* VaapiVP8Picture::AsVaapiVP8Picture() {
  return this;
}

VaapiVP9Picture::VaapiVP9Picture(scoped_refptr<VASurface> va_surface)
    : va_surface_(va_surface) {}

VaapiVP9Picture::~VaapiVP9Picture() = default;

VaapiVP9Picture* VaapiVP9Picture::AsVaapiVP9Picture() {
  return this;
}

void VaapiVP9Picture::SetDecodeSurface(
    scoped_refptr<VASurface> decode_va_surface) {
  decode_va_surface_ = std::move(decode_va_surface);
}

scoped_refptr<VP9Picture> VaapiVP9Picture::CreateDuplicate() {
  return new VaapiVP9Picture(va_surface_);
}

VaapiAV1Picture::VaapiAV1Picture(
    scoped_refptr<VASurface> display_va_surface,
    scoped_refptr<VASurface> reconstruct_va_surface)
    : display_va_surface_(std::move(display_va_surface)),
      reconstruct_va_surface_(std::move(reconstruct_va_surface)) {}

VaapiAV1Picture::~VaapiAV1Picture() = default;

scoped_refptr<AV1Picture> VaapiAV1Picture::CreateDuplicate() {
  return base::MakeRefCounted<VaapiAV1Picture>(display_va_surface_,
                                               reconstruct_va_surface_);
}

}  // namespace media
