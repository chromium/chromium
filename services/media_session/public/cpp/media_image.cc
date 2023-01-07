// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/media_session/public/cpp/media_image.h"

namespace media_session {

MediaImage::MediaImage() = default;

MediaImage::MediaImage(const MediaImage& other) = default;

MediaImage::~MediaImage() = default;

bool MediaImage::operator==(const MediaImage& other) const {
  return src == other.src && type == other.type && sizes == other.sizes;
}

}  // namespace media_session
