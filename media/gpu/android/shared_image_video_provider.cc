// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/android/shared_image_video_provider.h"

namespace media {

SharedImageVideoProvider::ImageSpec::ImageSpec() = default;
SharedImageVideoProvider::ImageSpec::ImageSpec(const gfx::Size& our_size,
                                               uint64_t our_generation_id)
    : size(our_size), generation_id(our_generation_id) {}
SharedImageVideoProvider::ImageSpec::ImageSpec(const ImageSpec&) = default;
SharedImageVideoProvider::ImageSpec::~ImageSpec() = default;

bool SharedImageVideoProvider::ImageSpec::operator==(
    const ImageSpec& rhs) const {
  return size == rhs.size && generation_id == rhs.generation_id;
}

bool SharedImageVideoProvider::ImageSpec::operator!=(
    const ImageSpec& rhs) const {
  return !(*this == rhs);
}

SharedImageVideoProvider::ImageRecord::ImageRecord() = default;
SharedImageVideoProvider::ImageRecord::ImageRecord(ImageRecord&&) = default;
SharedImageVideoProvider::ImageRecord::~ImageRecord() = default;

}  // namespace media
