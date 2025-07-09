// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MUXERS_MEMORY_WEBM_MUXER_DELEGATE_H_
#define MEDIA_MUXERS_MEMORY_WEBM_MUXER_DELEGATE_H_

#include <vector>

#include "base/thread_annotations.h"
#include "media/base/media_export.h"
#include "media/muxers/webm_muxer.h"

namespace media {

// Defines a delegate for WebmMuxer that provides a seekable memory-based
// implementation of the |mkvmuxer::IMkvWriter| interface. This allows a
// SeekHead element to be written once the muxer is flushed and finalized.
// This allows video players to be able to seek through the video.
class MEDIA_EXPORT MemoryWebmMuxerDelegate : public WebmMuxer::Delegate {
 public:
  MemoryWebmMuxerDelegate(Muxer::WriteDataCB write_data_callback,
                          base::OnceClosure started_callback);
  ~MemoryWebmMuxerDelegate() override;

  // WebmMuxer::Delegate:
  void InitSegment(mkvmuxer::Segment* segment) override;

  // mkvmuxer::IMkvWriter:
  mkvmuxer::int64 Position() const override;
  mkvmuxer::int32 Position(mkvmuxer::int64 position) override;
  bool Seekable() const override;
  void ElementStartNotify(mkvmuxer::uint64 element_id,
                          mkvmuxer::int64 position) override;

  // Flushes all data from `buffer_` and effectively turns this class into
  // LiveWebmMuxerDelegate. I.e., duration will no longer be written during
  // finalization and cues will be written at the end of the file instead of
  // at the beginning as required for seeking.
  void FlushAndDisableSeeking();

 protected:
  // WebmMuxerDelegate:
  mkvmuxer::int32 DoWrite(base::span<const uint8_t> buf) override;

 private:
  std::vector<uint8_t> buffer_ GUARDED_BY_CONTEXT(sequence_checker_);

  // Callback to dump written data as being called by libwebm.
  const Muxer::WriteDataCB write_data_callback_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Optional callback called once we initialize the first segment.
  base::OnceClosure started_callback_;

  bool seeking_allowed_ GUARDED_BY_CONTEXT(sequence_checker_) = true;
};

}  // namespace media

#endif  // MEDIA_MUXERS_MEMORY_WEBM_MUXER_DELEGATE_H_
