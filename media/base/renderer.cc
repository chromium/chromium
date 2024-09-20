// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/renderer.h"
#include "base/logging.h"

namespace media {

// WARNING: The returned names are used as part of UMA names. Do NOT change
// existing return names.
std::string GetRendererName(RendererType renderer_type) {
  switch (renderer_type) {
    case RendererType::kRendererImpl:
      return "RendererImpl";
    case RendererType::kMojo:
      return "MojoRenderer";
    case RendererType::kMediaPlayer:
      return "MediaPlayerRenderer";
    case RendererType::kCourier:
      return "CourierRenderer";
    case RendererType::kFlinging:
      return "FlingingRenderer";
    case RendererType::kCast:
      return "CastRenderer";
    case RendererType::kMediaFoundation:
      return "MediaFoundationRenderer";
    case RendererType::kRemoting:
      return "RemotingRenderer";  // media::remoting::Receiver
    case RendererType::kCastStreaming:
      return "CastStreamingRenderer";
    case RendererType::kContentEmbedderDefined:
      return "EmbedderDefined";
    case RendererType::kTest:
      return "Media Renderer Implementation For Testing";
  }
}

Renderer::Renderer() = default;

Renderer::~Renderer() = default;

void Renderer::SetCdm(CdmContext* cdm_context, CdmAttachedCB cdm_attached_cb) {
  DLOG(WARNING) << "CdmContext is not supported.";
  std::move(cdm_attached_cb).Run(false);
}

void Renderer::OnSelectedVideoTracksChanged(
    const std::vector<DemuxerStream*>& enabled_tracks,
    base::OnceClosure change_completed_cb) {
  DLOG(WARNING) << "Track changes are not supported.";
  std::move(change_completed_cb).Run();
}

void Renderer::OnEnabledAudioTracksChanged(
    const std::vector<DemuxerStream*>& enabled_tracks,
    base::OnceClosure change_completed_cb) {
  DLOG(WARNING) << "Track changes are not supported.";
  std::move(change_completed_cb).Run();
}

void Renderer::SetPreservesPitch(bool preserves_pitch) {
  // Not supported by most renderers.
}

void Renderer::SetWasPlayedWithUserActivationAndHighMediaEngagement(
    bool was_played_with_user_activation_and_high_media_engagement) {
  // Not supported by most renderers.
}

void Renderer::OnExternalVideoFrameRequest() {
  // Default implementation of OnExternalVideoFrameRequest is to no-op.
}

}  // namespace media
