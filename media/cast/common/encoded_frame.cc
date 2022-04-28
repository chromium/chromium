// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/common/encoded_frame.h"

#include "base/logging.h"

namespace media {
namespace cast {

EncodedFrame::EncodedFrame()
    : dependency(UNKNOWN_DEPENDENCY), new_playout_delay_ms(0) {}

EncodedFrame::~EncodedFrame() = default;

void EncodedFrame::CopyMetadataTo(EncodedFrame* dest) const {
  DCHECK(dest);
  dest->dependency = this->dependency;
  dest->frame_id = this->frame_id;
  dest->referenced_frame_id = this->referenced_frame_id;
  dest->rtp_timestamp = this->rtp_timestamp;
  dest->reference_time = this->reference_time;
  dest->new_playout_delay_ms = this->new_playout_delay_ms;
}

}  // namespace cast
}  // namespace media
