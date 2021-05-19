// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BLINK_WEBMEDIASOURCE_IMPL_H_
#define MEDIA_BLINK_WEBMEDIASOURCE_IMPL_H_

#include <string>
#include <vector>

#include "base/macros.h"
#include "media/blink/media_blink_export.h"
#include "third_party/blink/public/platform/web_media_source.h"

namespace media {
class AudioDecoderConfig;
class ChunkDemuxer;
class VideoDecoderConfig;

class MEDIA_BLINK_EXPORT WebMediaSourceImpl : public blink::WebMediaSource {
 public:
  WebMediaSourceImpl(ChunkDemuxer* demuxer);
  ~WebMediaSourceImpl() override;

  // blink::WebMediaSource implementation.
  std::unique_ptr<blink::WebSourceBuffer> AddSourceBuffer(
      const blink::WebString& content_type,
      const blink::WebString& codecs,
      AddStatus& out_status /* out */) override;
  std::unique_ptr<blink::WebSourceBuffer> AddSourceBuffer(
      std::unique_ptr<AudioDecoderConfig> audio_config,
      AddStatus& out_status /* out */) override;
  std::unique_ptr<blink::WebSourceBuffer> AddSourceBuffer(
      std::unique_ptr<VideoDecoderConfig> video_config,
      AddStatus& out_status /* out */) override;
  double Duration() override;
  void SetDuration(double duration) override;
  void MarkEndOfStream(EndOfStreamStatus status) override;
  void UnmarkEndOfStream() override;

 private:
  ChunkDemuxer* demuxer_;  // Owned by WebMediaPlayerImpl.

  DISALLOW_COPY_AND_ASSIGN(WebMediaSourceImpl);
};

}  // namespace media

#endif  // MEDIA_BLINK_WEBMEDIASOURCE_IMPL_H_
