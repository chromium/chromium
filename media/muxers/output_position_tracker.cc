// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/muxers/output_position_tracker.h"

#include "base/functional/callback.h"
#include "base/numerics/checked_math.h"

namespace media {

OutputPositionTracker::OutputPositionTracker(
    Muxer::WriteDataCB write_data_callback)
    : write_data_callback_(std::move(write_data_callback)) {
  CHECK(write_data_callback_);
}

OutputPositionTracker::~OutputPositionTracker() = default;

void OutputPositionTracker::WriteSpan(base::span<const uint8_t> data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  write_data_callback_.Run(data);
  CHECK(base::CheckAdd(current_pos_, data.size()).AssignIfValid(&current_pos_));
}

uint32_t OutputPositionTracker::GetCurrentPos() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return current_pos_;
}

}  // namespace media
