// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MUXERS_FILE_WEBM_MUXER_DELEGATE_H_
#define MEDIA_MUXERS_FILE_WEBM_MUXER_DELEGATE_H_

#include "base/files/file.h"
#include "base/thread_annotations.h"
#include "media/base/media_export.h"
#include "media/muxers/webm_muxer.h"

namespace media {

// Defines a delegate for WebmMuxer that provides a seekable file-based
// implementation of the |mkvmuxer::IMkvWriter| interface. This allows a
// SeekHead element to be written to the given |webm_file| once the muxer is
// flushed and finalized. This allows video players to be able to seek through
// the video and jump to any arbitrary position.
class MEDIA_EXPORT FileWebmMuxerDelegate : public WebmMuxer::Delegate {
 public:
  // |webm_file| must be an already valid opened file ready for writing.
  explicit FileWebmMuxerDelegate(base::File webm_file);
  FileWebmMuxerDelegate(const FileWebmMuxerDelegate&) = delete;
  FileWebmMuxerDelegate& operator=(const FileWebmMuxerDelegate&) = delete;
  ~FileWebmMuxerDelegate() override;

  // WebmMuxer::Delegate:
  void InitSegment(mkvmuxer::Segment* segment) override;

  // mkvmuxer::IMkvWriter:
  mkvmuxer::int64 Position() const override;
  mkvmuxer::int32 Position(mkvmuxer::int64 position) override;
  bool Seekable() const override;
  void ElementStartNotify(mkvmuxer::uint64 element_id,
                          mkvmuxer::int64 position) override;

 protected:
  // WebmMuxerDelegate:
  mkvmuxer::int32 DoWrite(const void* buf, mkvmuxer::uint32 len) override;

 private:
  base::File webm_file_ GUARDED_BY_CONTEXT(sequence_checker_);
};

}  // namespace media

#endif  // MEDIA_MUXERS_FILE_WEBM_MUXER_DELEGATE_H_
