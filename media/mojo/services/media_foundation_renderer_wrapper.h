// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_SERVICES_MEDIA_FOUNDATION_RENDERER_WRAPPER_H_
#define MEDIA_MOJO_SERVICES_MEDIA_FOUNDATION_RENDERER_WRAPPER_H_

#include "base/callback_list.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "media/base/media_resource.h"
#include "media/base/pipeline_status.h"
#include "media/base/renderer.h"
#include "media/base/renderer_client.h"
#include "media/mojo/mojom/dcomp_surface_registry.mojom.h"
#include "media/mojo/mojom/frame_interface_factory.mojom.h"
#include "media/mojo/mojom/renderer_extensions.mojom.h"
#include "media/renderers/win/media_foundation_renderer.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace media {

// Wrap media::MediaFoundationRenderer to remove its dependence on
// media::mojom::MediaFoundationRendererExtension interface.
class MediaFoundationRendererWrapper final
    : public Renderer,
      public mojom::MediaFoundationRendererExtension,
      public mojom::MuteStateObserver {
 public:
  using RendererExtension = mojom::MediaFoundationRendererExtension;
  using ClientExtension = mojom::MediaFoundationRendererClientExtension;

  MediaFoundationRendererWrapper(
      scoped_refptr<base::SequencedTaskRunner> task_runner,
      mojom::FrameInterfaceFactory* frame_interfaces,
      mojo::PendingRemote<mojom::MediaLog> media_log_remote,
      mojo::PendingReceiver<RendererExtension> renderer_extension_receiver,
      mojo::PendingRemote<ClientExtension> client_extension_remote);
  MediaFoundationRendererWrapper(const MediaFoundationRendererWrapper&) =
      delete;
  MediaFoundationRendererWrapper operator=(
      const MediaFoundationRendererWrapper&) = delete;
  ~MediaFoundationRendererWrapper() override;

  // Renderer implementation.
  void Initialize(MediaResource* media_resource,
                  RendererClient* client,
                  PipelineStatusCallback init_cb) override;
  void SetCdm(CdmContext* cdm_context, CdmAttachedCB cdm_attached_cb) override;
  void SetLatencyHint(std::optional<base::TimeDelta> latency_hint) override;
  void Flush(base::OnceClosure flush_cb) override;
  void StartPlayingFrom(base::TimeDelta time) override;
  void SetPlaybackRate(double playback_rate) override;
  void SetVolume(float volume) override;
  base::TimeDelta GetMediaTime() override;
  RendererType GetRendererType() override;

  // mojom::MediaFoundationRendererExtension implementation.
  void GetDCOMPSurface(GetDCOMPSurfaceCallback callback) override;
  void SetVideoStreamEnabled(bool enabled) override;
  void SetOutputRect(const gfx::Rect& output_rect,
                     SetOutputRectCallback callback) override;
  void NotifyFrameReleased(const base::UnguessableToken& frame_token) override;
  void RequestNextFrame() override;
  void SetMediaFoundationRenderingMode(
      MediaFoundationRenderingMode mode) override;

  // mojom::MuteStateObserver implementation.
  void OnMuteStateChange(bool muted) override;

 private:
  void OnGpuLuidChange(const CHROME_LUID& adapter_luid);
  void OnReceiveDCOMPSurface(GetDCOMPSurfaceCallback callback,
                             base::win::ScopedHandle handle,
                             const std::string& error);
  void OnDCOMPSurfaceHandleRegistered(
      GetDCOMPSurfaceCallback callback,
      const std::optional<base::UnguessableToken>& token);
  void OnFrameGeneratedByMediaFoundation(
      const base::UnguessableToken& frame_token,
      const gfx::Size& frame_size,
      base::TimeDelta frame_timestamp);
  void OnFramePoolInitialized(
      std::vector<MediaFoundationFrameInfo> frame_textures,
      const gfx::Size& texture_size);

  raw_ptr<mojom::FrameInterfaceFactory, FlakyDanglingUntriaged>
      frame_interfaces_;
  std::unique_ptr<MediaFoundationRenderer> renderer_;
  mojo::Receiver<MediaFoundationRendererExtension> renderer_extension_receiver_;
  mojo::Remote<media::mojom::MediaFoundationRendererClientExtension>
      client_extension_remote_;
  mojo::Receiver<mojom::MuteStateObserver> site_mute_observer_;

  base::CallbackListSubscription luid_update_subscription_;

  float volume_ = 1.0;
  bool muted_ = false;  // Whether the site (WebContents) is muted.

  bool has_get_dcomp_surface_called_ = false;
  mojo::Remote<mojom::DCOMPSurfaceRegistry> dcomp_surface_registry_;
  base::UnguessableToken dcomp_surface_token_;

  base::WeakPtrFactory<MediaFoundationRendererWrapper> weak_factory_{this};
};

}  // namespace media

#endif  // MEDIA_MOJO_SERVICES_MEDIA_FOUNDATION_RENDERER_WRAPPER_H_
