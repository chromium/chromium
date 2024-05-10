// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_SERVICES_MOJO_RENDERER_SERVICE_H_
#define MEDIA_MOJO_SERVICES_MOJO_RENDERER_SERVICE_H_

#include <stdint.h>

#include <memory>
#include <optional>

#include "base/compiler_specific.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/unguessable_token.h"
#include "media/base/buffering_state.h"
#include "media/base/media_resource.h"
#include "media/base/pipeline_status.h"
#include "media/base/renderer_client.h"
#include "media/mojo/mojom/renderer.mojom.h"
#include "media/mojo/services/media_mojo_export.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

namespace media {

class CdmContextRef;
class MediaResourceShim;
class MojoCdmServiceContext;
class Renderer;

// A mojom::Renderer implementation that use a media::Renderer to render
// media streams.
class MEDIA_MOJO_EXPORT MojoRendererService final : public mojom::Renderer,
                                                    public RendererClient {
 public:
  // Helper function to bind MojoRendererService with a SelfOwendReceiver,
  // which is safely accessible via the returned SelfOwnedReceiverRef.
  static mojo::SelfOwnedReceiverRef<mojom::Renderer> Create(
      MojoCdmServiceContext* mojo_cdm_service_context,
      std::unique_ptr<media::Renderer> renderer,
      mojo::PendingReceiver<mojom::Renderer> receiver);

  // |mojo_cdm_service_context| can be used to find the CDM to support
  // encrypted media. If null, encrypted media is not supported.
  MojoRendererService(MojoCdmServiceContext* mojo_cdm_service_context,
                      std::unique_ptr<media::Renderer> renderer);

  MojoRendererService(const MojoRendererService&) = delete;
  MojoRendererService& operator=(const MojoRendererService&) = delete;

  ~MojoRendererService() final;

  // mojom::Renderer implementation.
  void Initialize(
      mojo::PendingAssociatedRemote<mojom::RendererClient> client,
      std::optional<std::vector<mojo::PendingRemote<mojom::DemuxerStream>>>
          streams,
      mojom::MediaUrlParamsPtr media_url_params,
      InitializeCallback callback) final;
  void Flush(FlushCallback callback) final;
  void StartPlayingFrom(base::TimeDelta time_delta) final;
  void SetPlaybackRate(double playback_rate) final;
  void SetVolume(float volume) final;
  void SetCdm(const std::optional<base::UnguessableToken>& cdm_id,
              SetCdmCallback callback) final;
  void SetLatencyHint(std::optional<base::TimeDelta> latency_hint) final;

 private:
  enum State {
    STATE_UNINITIALIZED,
    STATE_INITIALIZING,
    STATE_FLUSHING,
    STATE_PLAYING,
    STATE_ERROR
  };

  // RendererClient implementation.
  void OnError(PipelineStatus status) final;
  void OnFallback(PipelineStatus status) final;
  void OnEnded() final;
  void OnStatisticsUpdate(const PipelineStatistics& stats) final;
  void OnBufferingStateChange(BufferingState state,
                              BufferingStateChangeReason reason) final;
  void OnWaiting(WaitingReason reason) final;
  void OnAudioConfigChange(const AudioDecoderConfig& config) final;
  void OnVideoConfigChange(const VideoDecoderConfig& config) final;
  void OnVideoNaturalSizeChange(const gfx::Size& size) final;
  void OnVideoOpacityChange(bool opaque) final;
  void OnVideoFrameRateChange(std::optional<int> fps) final;

  // Called when the MediaResourceShim is ready to go (has a config,
  // pipe handle, etc) and can be handed off to a renderer for use.
  void OnAllStreamsReady(base::OnceCallback<void(bool)> callback);

  // Called when |audio_renderer_| initialization has completed.
  void OnRendererInitializeDone(base::OnceCallback<void(bool)> callback,
                                PipelineStatus status);

  // Periodically polls the media time from the renderer and notifies the client
  // if the media time has changed since the last update.
  // If |force| is true, the client is notified even if the time is unchanged.
  // If |range| is true, an interpolation time range is reported.
  void UpdateMediaTime(bool force);
  void CancelPeriodicMediaTimeUpdates();
  void SchedulePeriodicMediaTimeUpdates();

  // Callback executed once Flush() completes.
  void OnFlushCompleted(FlushCallback callback);

  // Callback executed once SetCdm() completes.
  void OnCdmAttached(base::OnceCallback<void(bool)> callback, bool success);

  const raw_ptr<MojoCdmServiceContext> mojo_cdm_service_context_ = nullptr;

  State state_;
  double playback_rate_;

  std::unique_ptr<MediaResource> media_resource_;

  base::RepeatingTimer time_update_timer_;
  base::TimeDelta last_media_time_;

  mojo::AssociatedRemote<mojom::RendererClient> client_;

  // Holds the CdmContextRef to keep the CdmContext alive for the lifetime of
  // the |renderer_|.
  std::unique_ptr<CdmContextRef> cdm_context_ref_;

  // Note: Destroy |renderer_| first to avoid access violation into other
  // members, e.g. |media_resource_| and |cdm_|.
  // Must use "media::" because "Renderer" is ambiguous.
  std::unique_ptr<media::Renderer> renderer_;

  base::WeakPtr<MojoRendererService> weak_this_;
  base::WeakPtrFactory<MojoRendererService> weak_factory_{this};
};

}  // namespace media

#endif  // MEDIA_MOJO_SERVICES_MOJO_RENDERER_SERVICE_H_
