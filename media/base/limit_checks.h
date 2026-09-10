// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_LIMIT_CHECKS_H_
#define MEDIA_BASE_LIMIT_CHECKS_H_

#include "media/base/limits.h"
#include "ui/gfx/geometry/size.h"

namespace media {

// Checks whether a gfx::Size is within the valid bounds of kMaxDimension and
// kMaxCanvas as defined in media::limits.
constexpr bool ValidMediaSize(const gfx::Size& size) {
  // We use uint64_t to prevent overflow when calculating the area.
  return size.width() >= 0 && size.width() <= limits::kMaxDimension &&
         size.height() >= 0 && size.height() <= limits::kMaxDimension &&
         (static_cast<uint64_t>(size.width()) *
          static_cast<uint64_t>(size.height())) <=
             static_cast<uint64_t>(limits::kMaxCanvas);
}

}  // namespace media

#endif  // MEDIA_BASE_LIMIT_CHECKS_H_
