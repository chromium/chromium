// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/vaapi/test/fake_libva_driver/fake_surface.h"

namespace media::internal {

FakeSurface::FakeSurface(FakeSurface::IdType id,
                         unsigned int format,
                         unsigned int width,
                         unsigned int height,
                         std::vector<VASurfaceAttrib> attrib_list)
    : id_(id),
      format_(format),
      width_(width),
      height_(height),
      attrib_list_(std::move(attrib_list)) {}
FakeSurface::~FakeSurface() = default;

FakeSurface::IdType FakeSurface::GetID() const {
  return id_;
}

unsigned int FakeSurface::GetFormat() const {
  return format_;
}

unsigned int FakeSurface::GetWidth() const {
  return width_;
}

unsigned int FakeSurface::GetHeight() const {
  return height_;
}

const std::vector<VASurfaceAttrib>& FakeSurface::GetSurfaceAttribs() const {
  return attrib_list_;
}

}  // namespace media::internal