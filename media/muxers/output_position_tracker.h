// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MUXERS_OUTPUT_POSITION_TRACKER_H_
#define MEDIA_MUXERS_OUTPUT_POSITION_TRACKER_H_

#include <string_view>

#include "base/sequence_checker.h"
#include "media/base/media_export.h"
#include "media/muxers/muxer.h"

namespace media {

// The purpose of this class is to provide the current offset
// information of the target buffer, as the target buffer lacks
// offset data in its callback. This offset information is necessary
// for the MOOF and TFHD boxes.
class MEDIA_EXPORT OutputPositionTracker {
 public:
  explicit OutputPositionTracker(Muxer::WriteDataCB write_data_callback);
  ~OutputPositionTracker();
  OutputPositionTracker(const OutputPositionTracker&) = delete;
  OutputPositionTracker& operator=(const OutputPositionTracker&) = delete;

  void WriteSpan(base::span<const uint8_t> data);
  uint32_t GetCurrentPos() const;

 private:
  uint32_t current_pos_ = 0;

  SEQUENCE_CHECKER(sequence_checker_);

  const Muxer::WriteDataCB write_data_callback_
      GUARDED_BY_CONTEXT(sequence_checker_);
};  // class OutputPositionTracker.

}  // namespace media

#endif  // MEDIA_MUXERS_OUTPUT_POSITION_TRACKER_H_
