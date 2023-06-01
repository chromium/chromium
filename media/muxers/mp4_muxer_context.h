// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MUXERS_MP4_MUXER_CONTEXT_H_
#define MEDIA_MUXERS_MP4_MUXER_CONTEXT_H_

#include <map>
#include <memory>

#include "base/sequence_checker.h"
#include "media/base/media_export.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace media {

class OutputPositionTracker;

// The class provides any additional data that isn't part of box.
// Usually, the data will be set in the middle of box data creation, but
// can be also set during writing box.
class MEDIA_EXPORT Mp4MuxerContext {
 public:
  explicit Mp4MuxerContext(
      std::unique_ptr<OutputPositionTracker> output_position_tracker);
  ~Mp4MuxerContext();
  Mp4MuxerContext(const Mp4MuxerContext&) = delete;
  Mp4MuxerContext& operator=(const Mp4MuxerContext&) = delete;

  // Track will be created and inserted to vector by the order of arrival
  // on Muxer so video or audio index could be different on new stream
  // collection (e.g. after putRequest)
  absl::optional<size_t> GetVideoIndex() const;
  void SetVideoIndex(size_t index);

  absl::optional<size_t> GetAudioIndex() const;
  void SetAudioIndex(size_t index);

  OutputPositionTracker& GetOutputPositionTracker() const;

 private:
  absl::optional<size_t> video_index_;
  absl::optional<size_t> audio_index_;

  std::unique_ptr<OutputPositionTracker> output_position_tracker_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace media

#endif  // MEDIA_MUXERS_MP4_MUXER_CONTEXT_H_
