// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/bitrate.h"
#include "base/check_op.h"

namespace media {

bool Bitrate::operator==(const Bitrate& right) const {
  return (this->mode_ == right.mode_) && (this->target_ == right.target_) &&
         (this->peak_ == right.peak_);
}

uint32_t Bitrate::peak() const {
  DCHECK_EQ(mode_ == Mode::kConstant, peak_ == 0u);
  return peak_;
}

}  // namespace media
