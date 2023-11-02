// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/common/encoded_frame.h"

#include "base/logging.h"

namespace media {
namespace cast {

EncodedFrame::EncodedFrame() = default;
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
