// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_ANDROID_FLINGING_RENDERER_CLIENT_FACTORY_H_
#define CONTENT_RENDERER_MEDIA_ANDROID_FLINGING_RENDERER_CLIENT_FACTORY_H_

#include <memory>
#include <string>

#include "base/task/sequenced_task_runner.h"
#include "content/common/content_export.h"
#include "media/base/media_status.h"
#include "media/base/renderer_factory.h"
#include "media/renderers/remote_playback_client_wrapper.h"

namespace media {
class MojoRendererFactory;
}

namespace content {

// Creates a renderer for media flinging.
// The FRCF uses a MojoRendererFactory to create a FlingingRenderer in the
// browser process.
class CONTENT_EXPORT FlingingRendererClientFactory
    : public media::RendererFactory {
 public:
  FlingingRendererClientFactory(
      std::unique_ptr<media::MojoRendererFactory> mojo_renderer_factory,
      std::unique_ptr<media::RemotePlaybackClientWrapper>
          remote_playback_client);

  FlingingRendererClientFactory(const FlingingRendererClientFactory&) = delete;
  FlingingRendererClientFactory& operator=(
      const FlingingRendererClientFactory&) = delete;

  ~FlingingRendererClientFactory() override;

  // Sets a callback that renderers created by |this| will use to propagate
  // Play/Pause state changes on remote devices.
  // NOTE: This must be called before CreateRenderer().
  void SetRemotePlayStateChangeCB(media::RemotePlayStateChangeCB callback);

  std::unique_ptr<media::Renderer> CreateRenderer(
      const scoped_refptr<base::SequencedTaskRunner>& media_task_runner,
      const scoped_refptr<base::TaskRunner>& worker_task_runner,
      media::AudioRendererSink* audio_renderer_sink,
      media::VideoRendererSink* video_renderer_sink,
      media::RequestOverlayInfoCB request_overlay_info_cb,
      const gfx::ColorSpace& target_color_space) override;

  // Returns whether media flinging has started, based off of whether the
  // |remote_playback_client_| has a presentation ID or not. Called by
  // RendererFactorySelector to determine when to create a FlingingRenderer.
  bool IsFlingingActive();

 private:
  std::string GetActivePresentationId();

  std::unique_ptr<media::MojoRendererFactory> mojo_flinging_factory_;
  std::unique_ptr<media::RemotePlaybackClientWrapper> remote_playback_client_;

  media::RemotePlayStateChangeCB remote_play_state_change_cb_;
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_ANDROID_FLINGING_RENDERER_CLIENT_FACTORY_H_
