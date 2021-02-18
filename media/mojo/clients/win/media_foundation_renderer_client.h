// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_CLIENTS_WIN_MEDIA_FOUNDATION_RENDERER_CLIENT_H_
#define MEDIA_MOJO_CLIENTS_WIN_MEDIA_FOUNDATION_RENDERER_CLIENT_H_

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "media/base/media_resource.h"
#include "media/base/renderer.h"
#include "media/base/renderer_client.h"
#include "media/base/video_renderer_sink.h"
#include "media/mojo/clients/mojo_renderer.h"
#include "media/mojo/mojom/renderer_extensions.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace media {

// MediaFoundationRendererClient lives in Renderer process and mirrors a
// MediaFoundationRenderer living in the MF_CDM LPAC Utility process.
//
// It is responsible for forwarding media::Renderer calls from WMPI to the
// MediaFoundationRenderer, using |mojo_renderer|. It also manages a
// DCOMPTexture, (via |dcomp_texture_wrapper_|) and notifies the
// VideoRendererSink when new frames are available.
//
// This class handles all calls on |media_task_runner_|, except for
// OnFrameAvailable(), which is called on |compositor_task_runner_|.
//
// N.B: This class implements media::RendererClient, in order to intercept
// OnVideoNaturalSizeChange() events, to update DCOMPTextureWrapper. All events
// (including OnVideoNaturalSizeChange()) are bubbled up to |client_|.
//
class MediaFoundationRendererClient
    : public media::Renderer,
      public media::RendererClient,
      public media::VideoRendererSink::RenderCallback {
 public:
  using RendererExtension = media::mojom::MediaFoundationRendererExtension;

  MediaFoundationRendererClient(
      mojo::PendingRemote<RendererExtension> renderer_extension_remote,
      scoped_refptr<base::SingleThreadTaskRunner> media_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> compositor_task_runner,
      std::unique_ptr<media::MojoRenderer> mojo_renderer,
      media::VideoRendererSink* sink);

  ~MediaFoundationRendererClient() override;

  // media::Renderer implementation.
  void Initialize(MediaResource* media_resource,
                  RendererClient* client,
                  PipelineStatusCallback init_cb) override;
  void SetCdm(CdmContext* cdm_context, CdmAttachedCB cdm_attached_cb) override;
  void SetLatencyHint(base::Optional<base::TimeDelta> latency_hint) override;
  void Flush(base::OnceClosure flush_cb) override;
  void StartPlayingFrom(base::TimeDelta time) override;
  void SetPlaybackRate(double playback_rate) override;
  void SetVolume(float volume) override;
  base::TimeDelta GetMediaTime() override;
  void OnSelectedVideoTracksChanged(
      const std::vector<media::DemuxerStream*>& enabled_tracks,
      base::OnceClosure change_completed_cb) override;

  // media::RendererClient implementation.
  void OnError(PipelineStatus status) override;
  void OnEnded() override;
  void OnStatisticsUpdate(const media::PipelineStatistics& stats) override;
  void OnBufferingStateChange(media::BufferingState state,
                              media::BufferingStateChangeReason) override;
  void OnWaiting(media::WaitingReason reason) override;
  void OnAudioConfigChange(const media::AudioDecoderConfig& config) override;
  void OnVideoConfigChange(const media::VideoDecoderConfig& config) override;
  void OnVideoNaturalSizeChange(const gfx::Size& size) override;
  void OnVideoOpacityChange(bool opaque) override;
  void OnVideoFrameRateChange(base::Optional<int>) override;

  // media::VideoRendererSink::RenderCallback implementation.
  scoped_refptr<media::VideoFrame> Render(base::TimeTicks deadline_min,
                                          base::TimeTicks deadline_max,
                                          bool background_rendering) override;
  void OnFrameDropped() override;
  base::TimeDelta GetPreferredRenderInterval() override;

 private:
  void OnConnectionError();
  void OnRemoteRendererInitialized(media::PipelineStatus status);
  void OnVideoFrameCreated(scoped_refptr<media::VideoFrame> video_frame);
  void OnDCOMPStreamTextureInitialized(bool success);
  void OnDCOMPSurfaceTextureReleased();
  void OnDCOMPSurfaceHandleCreated(bool success);
  void OnReceivedRemoteDCOMPSurface(mojo::ScopedHandle surface_handle);
  void OnDCOMPSurfaceRegisteredInGPUProcess(
      const base::UnguessableToken& token);
  void OnCompositionParamsReceived(gfx::Rect output_rect);

  void InitializeDCOMPRendering();
  void RegisterDCOMPSurfaceHandleInGPUProcess(
      base::win::ScopedHandle surface_handle);
  void OnCdmAttached(bool success);
  void InitializeMojoCdmTelemetryPtrServer();
  void OnCDMTelemetryPtrConnectionError();

  bool MojoSetDCOMPMode(bool enabled);
  void MojoGetDCOMPSurface();

  // Used to forward calls to the MediaFoundationRenderer living in the MF_CDM
  // LPAC Utility process.
  std::unique_ptr<media::MojoRenderer> mojo_renderer_;

  RendererClient* client_ = nullptr;

  VideoRendererSink* sink_;
  bool video_rendering_started_ = false;
  bool dcomp_rendering_initialized_ = false;
  // video's native size.
  gfx::Size natural_size_;

  scoped_refptr<base::SingleThreadTaskRunner> media_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> compositor_task_runner_;
  scoped_refptr<media::VideoFrame> dcomp_frame_;
  bool dcomp_surface_handle_bound_ = false;
  bool has_video_ = false;

  PipelineStatusCallback init_cb_;
  CdmContext* cdm_context_ = nullptr;
  CdmAttachedCB cdm_attached_cb_;

  // Used temporarily, to delay binding to |renderer_extension_remote_| until we
  // are on the right sequence, when Initialize() is called.
  mojo::PendingRemote<RendererExtension>
      delayed_bind_renderer_extension_remote_;

  // Used to call methods on the MediaFoundationRenderer in the MF_CMD LPAC
  // Utility process.
  mojo::Remote<RendererExtension> renderer_extension_remote_;

  bool waiting_for_dcomp_surface_handle_ = false;

  // NOTE: Weak pointers must be invalidated before all other member variables.
  base::WeakPtrFactory<MediaFoundationRendererClient> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(MediaFoundationRendererClient);
};

}  // namespace media

#endif  // MEDIA_MOJO_CLIENTS_WIN_MEDIA_FOUNDATION_RENDERER_CLIENT_H_
