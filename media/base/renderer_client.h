// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_RENDERER_CLIENT_H_
#define MEDIA_BASE_RENDERER_CLIENT_H_

#include <optional>

#include "media/base/audio_decoder_config.h"
#include "media/base/buffering_state.h"
#include "media/base/media_export.h"
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

  // Executed if there is a non-fatal fallback that should be reported
  virtual void OnFallback(PipelineStatus status) = 0;

  // Executed when rendering has reached the end of stream.
  virtual void OnEnded() = 0;

  // Executed periodically with rendering statistics. Fields *_decoded*,
  // *_dropped and *memory_usage should be the delta since the last
  // OnStatisticsUpdate() call.
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
  // TODO(crbug.com/40638012): Used by AudioRendererImpl.  This can be removed
  // when the bug is resolved.
  virtual bool IsVideoStreamAvailable();

  // Called when the bucketed frames per second has changed.  |fps| will be
  // unset if the frame rate is unstable.  The duration used for the frame rate
  // is based on the wall clock time, not the media time.
  virtual void OnVideoFrameRateChange(std::optional<int> fps) = 0;
};

}  // namespace media

#endif  // MEDIA_BASE_RENDERER_CLIENT_H_
