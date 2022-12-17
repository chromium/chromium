// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/vaapi/test/fake_libva_driver/fake_context.h"

namespace media::internal {

FakeContext::FakeContext(FakeContext::IdType id,
                         VAConfigID config_id,
                         int picture_width,
                         int picture_height,
                         int flag,
                         std::vector<VASurfaceID> render_targets)
    : id_(id),
      config_id_(config_id),
      picture_width_(picture_width),
      picture_height_(picture_height),
      flag_(flag),
      render_targets_(std::move(render_targets)) {}
FakeContext::~FakeContext() = default;

FakeContext::IdType FakeContext::GetID() const {
  return id_;
}

VAConfigID FakeContext::GetConfigID() const {
  return config_id_;
}

int FakeContext::GetPictureWidth() const {
  return picture_width_;
}

int FakeContext::GetPictureHeight() const {
  return picture_height_;
}

int FakeContext::GetFlag() const {
  return flag_;
}

const std::vector<VASurfaceID>& FakeContext::GetRenderTargets() const {
  return render_targets_;
}

}  // namespace media::internal