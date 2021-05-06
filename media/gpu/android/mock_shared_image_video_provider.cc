// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/android/mock_shared_image_video_provider.h"

namespace media {

MockSharedImageVideoProvider::RequestImageArgs::RequestImageArgs(
    ImageReadyCB cb,
    ImageSpec spec,
    scoped_refptr<gpu::TextureOwner> texture_owner)
    : cb_(std::move(cb)),
      spec_(std::move(spec)),
      texture_owner_(std::move(texture_owner)) {}

MockSharedImageVideoProvider::RequestImageArgs::~RequestImageArgs() = default;

MockSharedImageVideoProvider::MockSharedImageVideoProvider()
    : weak_factory_(this) {}

MockSharedImageVideoProvider::~MockSharedImageVideoProvider() = default;

}  // namespace media
