// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/vaapi/vaapi_common.h"

#include "build/chromeos_buildflags.h"

namespace media {

VaapiH264Picture::VaapiH264Picture(std::unique_ptr<VASurfaceHandle> va_surface)
    : va_surface_(std::move(va_surface)) {}

VaapiH264Picture::~VaapiH264Picture() = default;

VaapiH264Picture* VaapiH264Picture::AsVaapiH264Picture() {
  return this;
}

#if BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
VaapiH265Picture::VaapiH265Picture(std::unique_ptr<VASurfaceHandle> va_surface)
    : va_surface_(std::move(va_surface)) {}

VaapiH265Picture::~VaapiH265Picture() = default;

VaapiH265Picture* VaapiH265Picture::AsVaapiH265Picture() {
  return this;
}

#endif  // BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)

VaapiVP8Picture::VaapiVP8Picture(std::unique_ptr<VASurfaceHandle> va_surface)
    : va_surface_(std::move(va_surface)) {}

VaapiVP8Picture::~VaapiVP8Picture() = default;

VaapiVP8Picture* VaapiVP8Picture::AsVaapiVP8Picture() {
  return this;
}

VaapiVP9Picture::VaapiVP9Picture(std::unique_ptr<VASurfaceHandle> va_surface)
    : va_surface_(std::move(va_surface)) {}

VaapiVP9Picture::~VaapiVP9Picture() = default;

VaapiVP9Picture* VaapiVP9Picture::AsVaapiVP9Picture() {
  return this;
}

scoped_refptr<VP9Picture> VaapiVP9Picture::CreateDuplicate() {
  return this;
}

VaapiAV1Picture::VaapiAV1Picture(
    std::unique_ptr<VASurfaceHandle> display_va_surface,
    std::unique_ptr<VASurfaceHandle> reconstruct_va_surface)
    : display_va_surface_(std::move(display_va_surface)),
      reconstruct_va_surface_(std::move(reconstruct_va_surface)) {}

VaapiAV1Picture::~VaapiAV1Picture() = default;

scoped_refptr<AV1Picture> VaapiAV1Picture::CreateDuplicate() {
  return this;
}

}  // namespace media
