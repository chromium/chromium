// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/bitrate.h"
#include "base/check_op.h"
#include "base/strings/stringprintf.h"

namespace media {

bool Bitrate::operator==(const Bitrate& right) const {
  return (this->mode_ == right.mode_) && (this->target_ == right.target_) &&
         (this->peak_ == right.peak_);
}

bool Bitrate::operator!=(const Bitrate& right) const {
  return !(*this == right);
}

uint32_t Bitrate::peak() const {
  DCHECK_EQ(mode_ == Mode::kConstant, peak_ == 0u);
  return peak_;
}

std::string Bitrate::ToString() const {
  switch (mode_) {
    case Mode::kConstant:
      return base::StringPrintf("CBR: %d bps", target_);
    case Mode::kVariable:
      return base::StringPrintf("VBR: target %d bps, peak %d bps", target_,
                                peak_);
  }
}

}  // namespace media
