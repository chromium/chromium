// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BLINK_WEBSOURCEBUFFER_IMPL_H_
#define MEDIA_BLINK_WEBSOURCEBUFFER_IMPL_H_

#include <stddef.h>

#include <memory>
#include <string>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/time/time.h"
#include "media/filters/source_buffer_parse_warnings.h"
#include "third_party/blink/public/platform/web_source_buffer.h"

namespace media {
class ChunkDemuxer;
class MediaTracks;

class WebSourceBufferImpl : public blink::WebSourceBuffer {
 public:
  WebSourceBufferImpl(const std::string& id, ChunkDemuxer* demuxer);
  ~WebSourceBufferImpl() override;

  // blink::WebSourceBuffer implementation.
  void SetClient(blink::WebSourceBufferClient* client) override;
  bool GetGenerateTimestampsFlag() override;
  bool SetMode(AppendMode mode) override;
  blink::WebTimeRanges Buffered() override;
  double HighestPresentationTimestamp() override;
  bool EvictCodedFrames(double currentPlaybackTime,
                        size_t newDataSize) override;
  bool Append(const unsigned char* data,
              unsigned length,
              double* timestamp_offset) override;
  void ResetParserState() override;
  void Remove(double start, double end) override;
  bool CanChangeType(const blink::WebString& content_type,
                     const blink::WebString& codecs) override;
  void ChangeType(const blink::WebString& content_type,
                  const blink::WebString& codecs) override;
  bool SetTimestampOffset(double offset) override;
  void SetAppendWindowStart(double start) override;
  void SetAppendWindowEnd(double end) override;
  void RemovedFromMediaSource() override;

 private:
  // Demuxer callback handler to process an initialization segment received
  // during an append() call.
  void InitSegmentReceived(std::unique_ptr<MediaTracks> tracks);

  // Demuxer callback handler to notify Blink of a non-fatal parse warning.
  void NotifyParseWarning(const SourceBufferParseWarning warning);

  std::string id_;
  ChunkDemuxer* demuxer_;  // Owned by WebMediaPlayerImpl.

  blink::WebSourceBufferClient* client_;

  // Controls the offset applied to timestamps when processing appended media
  // segments. It is initially 0, which indicates that no offset is being
  // applied. Both setTimestampOffset() and append() may update this value.
  base::TimeDelta timestamp_offset_;

  base::TimeDelta append_window_start_;
  base::TimeDelta append_window_end_;

  DISALLOW_COPY_AND_ASSIGN(WebSourceBufferImpl);
};

}  // namespace media

#endif  // MEDIA_BLINK_WEBSOURCEBUFFER_IMPL_H_
