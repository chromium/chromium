// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MUXERS_LIVE_WEBM_MUXER_DELEGATE_H_
#define MEDIA_MUXERS_LIVE_WEBM_MUXER_DELEGATE_H_

#include "base/functional/callback.h"
#include "base/thread_annotations.h"
#include "media/base/media_export.h"
#include "media/muxers/webm_muxer.h"

namespace media {

// Defines a delegate for WebmMuxer that provides a live-mode non-seekable
// implementation of the |mkvmuxer::IMkvWriter| interface. The output of the
// muxer writer will be provided to the client by repeated invoking the given
// |write_data_callback|.
class MEDIA_EXPORT LiveWebmMuxerDelegate : public WebmMuxer::Delegate {
 public:
  explicit LiveWebmMuxerDelegate(Muxer::WriteDataCB write_data_callback);
  LiveWebmMuxerDelegate(const LiveWebmMuxerDelegate&) = delete;
  LiveWebmMuxerDelegate& operator=(const LiveWebmMuxerDelegate&) = delete;
  ~LiveWebmMuxerDelegate() override;

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
  // Callback to dump written data as being called by libwebm.
  const Muxer::WriteDataCB write_data_callback_
      GUARDED_BY_CONTEXT(sequence_checker_);
};

}  // namespace media

#endif  // MEDIA_MUXERS_LIVE_WEBM_MUXER_DELEGATE_H_
