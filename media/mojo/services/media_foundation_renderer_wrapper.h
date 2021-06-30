// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_SERVICES_MEDIA_FOUNDATION_RENDERER_WRAPPER_H_
#define MEDIA_MOJO_SERVICES_MEDIA_FOUNDATION_RENDERER_WRAPPER_H_

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "media/base/media_resource.h"
#include "media/base/pipeline_status.h"
#include "media/base/renderer.h"
#include "media/base/renderer_client.h"
#include "media/mojo/mojom/frame_interface_factory.mojom.h"
#include "media/mojo/mojom/renderer_extensions.mojom.h"
#include "media/renderers/win/media_foundation_renderer.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace media {

// Wrap media::MediaFoundationRenderer to remove its dependence on
// media::mojom::MediaFoundationRendererExtension interface.
class MediaFoundationRendererWrapper
    : public Renderer,
      public mojom::MediaFoundationRendererExtension,
      public mojom::MuteStateObserver {
 public:
  using RendererExtension = mojom::MediaFoundationRendererExtension;

  MediaFoundationRendererWrapper(
      scoped_refptr<base::SequencedTaskRunner> task_runner,
      mojom::FrameInterfaceFactory* frame_interfaces,
      mojo::PendingReceiver<RendererExtension> renderer_extension_receiver);
  MediaFoundationRendererWrapper(const MediaFoundationRendererWrapper&) =
      delete;
  MediaFoundationRendererWrapper operator=(
      const MediaFoundationRendererWrapper&) = delete;
  ~MediaFoundationRendererWrapper() final;

  // Renderer implementation.
  void Initialize(MediaResource* media_resource,
                  RendererClient* client,
                  PipelineStatusCallback init_cb) override;
  void SetCdm(CdmContext* cdm_context, CdmAttachedCB cdm_attached_cb) override;
  void SetLatencyHint(absl::optional<base::TimeDelta> latency_hint) override;
  void Flush(base::OnceClosure flush_cb) override;
  void StartPlayingFrom(base::TimeDelta time) override;
  void SetPlaybackRate(double playback_rate) override;
  void SetVolume(float volume) override;
  base::TimeDelta GetMediaTime() override;

  // mojom::MediaFoundationRendererExtension implementation.
  void SetDCOMPMode(bool enabled, SetDCOMPModeCallback callback) final;
  void GetDCOMPSurface(GetDCOMPSurfaceCallback callback) final;
  void SetVideoStreamEnabled(bool enabled) final;
  void SetOutputParams(const gfx::Rect& output_rect) final;

  // mojom::MuteStateObserver implementation.
  void OnMuteStateChange(bool muted) final;

 private:
  void OnReceiveDCOMPSurface(HANDLE handle);

  mojom::FrameInterfaceFactory* frame_interfaces_;
  std::unique_ptr<MediaFoundationRenderer> renderer_;
  mojo::Receiver<MediaFoundationRendererExtension> renderer_extension_receiver_;
  GetDCOMPSurfaceCallback get_decomp_surface_cb_;

  mojo::Receiver<mojom::MuteStateObserver> site_mute_observer_;
  float volume_ = 1.0;
  bool muted_ = false;  // Whether the site (WebContents) is muted.

  base::WeakPtrFactory<MediaFoundationRendererWrapper> weak_factory_{this};
};

}  // namespace media

#endif  // MEDIA_MOJO_SERVICES_MEDIA_FOUNDATION_RENDERER_WRAPPER_H_
