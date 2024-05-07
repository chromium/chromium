// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_CLIENTS_MOJO_RENDERER_H_
#define MEDIA_MOJO_CLIENTS_MOJO_RENDERER_H_

#include <stdint.h>

#include <memory>
#include <optional>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/default_tick_clock.h"
#include "base/unguessable_token.h"
#include "media/base/demuxer_stream.h"
#include "media/base/renderer.h"
#include "media/base/time_delta_interpolator.h"
#include "media/mojo/mojom/renderer.mojom.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace media {

class MediaResource;
class MojoDemuxerStreamImpl;
class VideoOverlayFactory;
class VideoRendererSink;

// A media::Renderer that proxies to a mojom::Renderer. That
// mojom::Renderer proxies back to the MojoRenderer via the
// mojom::RendererClient interface.
//
// This class can be created on any thread, where the |remote_renderer| is
// connected and passed in the constructor. Then Initialize() will be called on
// the |task_runner| and starting from that point this class is bound to the
// |task_runner|*. That means all Renderer and RendererClient methods will be
// called/dispatched on the |task_runner|. The only exception is GetMediaTime(),
// which can be called on any thread.
class MojoRenderer : public Renderer, public mojom::RendererClient {
 public:
  MojoRenderer(const scoped_refptr<base::SequencedTaskRunner>& task_runner,
               std::unique_ptr<VideoOverlayFactory> video_overlay_factory,
               VideoRendererSink* video_renderer_sink,
               mojo::PendingRemote<mojom::Renderer> remote_renderer);

  MojoRenderer(const MojoRenderer&) = delete;
  MojoRenderer& operator=(const MojoRenderer&) = delete;

  ~MojoRenderer() override;

  // Renderer implementation.
  void Initialize(MediaResource* media_resource,
                  media::RendererClient* client,
                  PipelineStatusCallback init_cb) override;
  void SetCdm(CdmContext* cdm_context, CdmAttachedCB cdm_attached_cb) override;
  void SetLatencyHint(std::optional<base::TimeDelta> latency_hint) override;
  void Flush(base::OnceClosure flush_cb) override;
  void StartPlayingFrom(base::TimeDelta time) override;
  void SetPlaybackRate(double playback_rate) override;
  void SetVolume(float volume) override;
  base::TimeDelta GetMediaTime() override;
  RendererType GetRendererType() override;

 private:
  // mojom::RendererClient implementation, dispatched on the |task_runner_|.
  void OnTimeUpdate(base::TimeDelta time,
                    base::TimeDelta max_time,
                    base::TimeTicks capture_time) override;
  void OnBufferingStateChange(BufferingState state,
                              BufferingStateChangeReason reason) override;
  void OnEnded() override;
  void OnError(const PipelineStatus& status) override;
  void OnAudioConfigChange(const AudioDecoderConfig& config) override;
  void OnVideoConfigChange(const VideoDecoderConfig& config) override;
  void OnVideoNaturalSizeChange(const gfx::Size& size) override;
  void OnVideoOpacityChange(bool opaque) override;
  void OnWaiting(WaitingReason reason) override;
  void OnStatisticsUpdate(const PipelineStatistics& stats) override;

  // Binds |remote_renderer_| to the mojo message pipe. Can be called multiple
  // times. If an error occurs during connection, OnConnectionError will be
  // called asynchronously.
  void BindRemoteRendererIfNeeded();

  // Initialize the remote renderer when |media_resource| is of type
  // MediaResource::Type::STREAM.
  void InitializeRendererFromStreams(media::RendererClient* client);

  // Initialize the remote renderer when |media_resource| is of type
  // MediaResource::Type::URL.
  void InitializeRendererFromUrl(media::RendererClient* client);

  // Callback for connection error on |remote_renderer_|.
  void OnConnectionError();

  // Callback for connection error on any of |streams_|. The |stream| parameter
  // indicates which stream the error happened on.
  void OnDemuxerStreamConnectionError(MojoDemuxerStreamImpl* stream);

  // Callbacks for |remote_renderer_| methods.
  void OnInitialized(media::RendererClient* client, bool success);
  void OnFlushed();
  void OnCdmAttached(bool success);

  void CancelPendingCallbacks();

  // |task_runner| on which all methods are invoked, except for GetMediaTime(),
  // which can be called on any thread.
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  // Overlay factory used to create overlays for video frames rendered
  // by the remote renderer.
  std::unique_ptr<VideoOverlayFactory> video_overlay_factory_;

  // Video frame overlays are rendered onto this sink.
  // Rendering of a new overlay is only needed when video natural size changes.
  // TODO(crbug.com/41490899) Investigate dangling pointer.
  raw_ptr<VideoRendererSink,
          FlakyDanglingUntriaged | AcrossTasksDanglingUntriaged>
      video_renderer_sink_ = nullptr;

  // Provider of audio/video DemuxerStreams. Must be valid throughout the
  // lifetime of |this|.
  raw_ptr<MediaResource> media_resource_ = nullptr;

  // Client of |this| renderer passed in Initialize.
  raw_ptr<media::RendererClient> client_ = nullptr;

  // Mojo demuxer streams.
  // Owned by MojoRenderer instead of remote mojom::Renderer
  // because these demuxer streams need to be destroyed as soon as |this| is
  // destroyed. The local demuxer streams returned by MediaResource cannot be
  // used after |this| is destroyed.
  // TODO(alokp): Add tests for MojoDemuxerStreamImpl.
  std::vector<std::unique_ptr<MojoDemuxerStreamImpl>> streams_;

  // This class is constructed on one thread and used exclusively on another
  // thread. This member is used to safely pass the PendingRemote from one
  // thread to another. It is set in the constructor and is consumed in
  // Initialize().
  mojo::PendingRemote<mojom::Renderer> remote_renderer_pending_remote_;

  // Remote Renderer, bound to |task_runner_| during Initialize().
  mojo::Remote<mojom::Renderer> remote_renderer_;

  // Receiver for RendererClient, bound to the |task_runner_|.
  mojo::AssociatedReceiver<RendererClient> client_receiver_{this};

  bool encountered_error_ = false;

  PipelineStatusCallback init_cb_;
  base::OnceClosure flush_cb_;
  CdmAttachedCB cdm_attached_cb_;

  float volume_ = 1.0f;

  // Lock used to serialize access for |time_interpolator_|.
  mutable base::Lock lock_;
  media::TimeDeltaInterpolator media_time_interpolator_;

  std::optional<PipelineStatistics> pending_stats_;
};

}  // namespace media

#endif  // MEDIA_MOJO_CLIENTS_MOJO_RENDERER_H_
