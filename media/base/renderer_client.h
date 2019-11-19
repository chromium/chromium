// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_RENDERER_CLIENT_H_
#define MEDIA_BASE_RENDERER_CLIENT_H_

#include "base/time/time.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/buffering_state.h"
#include "media/base/media_status.h"
#include "media/base/pipeline_status.h"
#include "media/base/video_decoder_config.h"
#include "media/base/waiting.h"
#include "ui/gfx/geometry/size.h"

namespace media {

// Interface used by Renderer, AudioRenderer, VideoRenderer and
// MediaPlayerRenderer implementations to notify their clients.
class MEDIA_EXPORT RendererClient {
 public:
  // Executed if any error was encountered after Renderer initialization.
  virtual void OnError(PipelineStatus status) = 0;

  // Executed when rendering has reached the end of stream.
  virtual void OnEnded() = 0;

  // Executed periodically with rendering statistics.
  virtual void OnStatisticsUpdate(const PipelineStatistics& stats) = 0;

  // Executed when buffering state is changed. |reason| indicates the cause of
  // the state change, when known.
  virtual void OnBufferingStateChange(BufferingState state,
                                      BufferingStateChangeReason reason) = 0;

  // Executed whenever the Renderer is waiting because of |reason|.
  virtual void OnWaiting(WaitingReason reason) = 0;

  // Executed whenever DemuxerStream status returns kConfigChange. Initial
  // configs provided by OnMetadata.
  virtual void OnAudioConfigChange(const AudioDecoderConfig& config) = 0;
  virtual void OnVideoConfigChange(const VideoDecoderConfig& config) = 0;

  // Executed for the first video frame and whenever natural size changes.
  // Only used if media stream contains a video track.
  virtual void OnVideoNaturalSizeChange(const gfx::Size& size) = 0;

  // Executed for the first video frame and whenever opacity changes.
  // Only used if media stream contains a video track.
  virtual void OnVideoOpacityChange(bool opaque) = 0;

  // Returns true if video stream is available in the media resource.
  // TODO(crbug.com/988535): Used by AudioRendererImpl.  This can be removed
  // when the bug is resolved.
  virtual bool IsVideoStreamAvailable();
};

}  // namespace media

#endif  // MEDIA_BASE_RENDERER_CLIENT_H_
