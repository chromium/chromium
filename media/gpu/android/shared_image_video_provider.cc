// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/android/shared_image_video_provider.h"

namespace media {

SharedImageVideoProvider::ImageSpec::ImageSpec() = default;
SharedImageVideoProvider::ImageSpec::ImageSpec(const gfx::Size& our_size,
                                               uint64_t our_generation_id)
    : coded_size(our_size), generation_id(our_generation_id) {}
SharedImageVideoProvider::ImageSpec::ImageSpec(const ImageSpec&) = default;
SharedImageVideoProvider::ImageSpec::~ImageSpec() = default;

bool SharedImageVideoProvider::ImageSpec::operator==(
    const ImageSpec& rhs) const {
  return coded_size == rhs.coded_size && generation_id == rhs.generation_id &&
         color_space == rhs.color_space;
}

bool SharedImageVideoProvider::ImageSpec::operator!=(
    const ImageSpec& rhs) const {
  return !(*this == rhs);
}

SharedImageVideoProvider::ImageRecord::ImageRecord() = default;
SharedImageVideoProvider::ImageRecord::ImageRecord(ImageRecord&&) = default;
SharedImageVideoProvider::ImageRecord::~ImageRecord() = default;

}  // namespace media
