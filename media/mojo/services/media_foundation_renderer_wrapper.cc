// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/media_foundation_renderer_wrapper.h"

#include "base/functional/callback_helpers.h"
#include "base/task/sequenced_task_runner.h"
#include "media/base/win/mf_helpers.h"
#include "media/mojo/mojom/renderer_extensions.mojom.h"
#include "media/mojo/services/media_foundation_gpu_info_monitor.h"
#include "media/mojo/services/mojo_media_log.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "mojo/public/cpp/system/platform_handle.h"

namespace media {

namespace {

bool HasAudio(MediaResource* media_resource) {
  DCHECK(media_resource->GetType() == MediaResource::Type::kStream);

  const auto media_streams = media_resource->GetAllStreams();
  for (const media::DemuxerStream* stream : media_streams) {
    if (stream->type() == media::DemuxerStream::Type::AUDIO)
      return true;
  }

  return false;
}

LUID ChromeLuidToLuid(const CHROME_LUID& chrome_luid) {
  LUID luid;
  luid.LowPart = chrome_luid.LowPart;
  luid.HighPart = chrome_luid.HighPart;
  return luid;
}

}  // namespace

MediaFoundationRendererWrapper::MediaFoundationRendererWrapper(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    mojom::FrameInterfaceFactory* frame_interfaces,
    mojo::PendingRemote<mojom::MediaLog> media_log_remote,
    mojo::PendingReceiver<RendererExtension> renderer_extension_receiver,
    mojo::PendingRemote<ClientExtension> client_extension_remote)
    : frame_interfaces_(frame_interfaces),
      renderer_extension_receiver_(this,
                                   std::move(renderer_extension_receiver)),
      client_extension_remote_(std::move(client_extension_remote), task_runner),
      site_mute_observer_(this) {
  DVLOG_FUNC(1);
  DCHECK(frame_interfaces_);

  renderer_ = std::make_unique<MediaFoundationRenderer>(
      std::move(task_runner),
      std::make_unique<MojoMediaLog>(std::move(media_log_remote), task_runner),
      ChromeLuidToLuid(
          MediaFoundationGpuInfoMonitor::GetInstance()->gpu_luid()));

  luid_update_subscription_ =
      MediaFoundationGpuInfoMonitor::GetInstance()->AddLuidObserver(
          base::BindRepeating(&MediaFoundationRendererWrapper::OnGpuLuidChange,
                              weak_factory_.GetWeakPtr()));
}

MediaFoundationRendererWrapper::~MediaFoundationRendererWrapper() {
  DVLOG_FUNC(1);
  if (!dcomp_surface_token_.is_empty())
    dcomp_surface_registry_->UnregisterDCOMPSurfaceHandle(dcomp_surface_token_);
}

void MediaFoundationRendererWrapper::Initialize(
    MediaResource* media_resource,
    RendererClient* client,
    PipelineStatusCallback init_cb) {
  if (HasAudio(media_resource)) {
    frame_interfaces_->RegisterMuteStateObserver(
        site_mute_observer_.BindNewPipeAndPassRemote());
  }

  renderer_->SetFrameReturnCallbacks(
      base::BindRepeating(
          &MediaFoundationRendererWrapper::OnFrameGeneratedByMediaFoundation,
          weak_factory_.GetWeakPtr()),
      base::BindRepeating(
          &MediaFoundationRendererWrapper::OnFramePoolInitialized,
          weak_factory_.GetWeakPtr()));

  renderer_->Initialize(media_resource, client, std::move(init_cb));
}

void MediaFoundationRendererWrapper::SetCdm(CdmContext* cdm_context,
                                            CdmAttachedCB cdm_attached_cb) {
  renderer_->SetCdm(cdm_context, std::move(cdm_attached_cb));
}

void MediaFoundationRendererWrapper::SetLatencyHint(
    std::optional<base::TimeDelta> latency_hint) {
  renderer_->SetLatencyHint(latency_hint);
}

void MediaFoundationRendererWrapper::Flush(base::OnceClosure flush_cb) {
  renderer_->Flush(std::move(flush_cb));
}

void MediaFoundationRendererWrapper::StartPlayingFrom(base::TimeDelta time) {
  renderer_->StartPlayingFrom(time);
}

void MediaFoundationRendererWrapper::SetPlaybackRate(double playback_rate) {
  renderer_->SetPlaybackRate(playback_rate);
}

void MediaFoundationRendererWrapper::SetVolume(float volume) {
  volume_ = volume;
  renderer_->SetVolume(muted_ ? 0 : volume_);
}

base::TimeDelta MediaFoundationRendererWrapper::GetMediaTime() {
  return renderer_->GetMediaTime();
}

RendererType MediaFoundationRendererWrapper::GetRendererType() {
  return RendererType::kMediaFoundation;
}

void MediaFoundationRendererWrapper::GetDCOMPSurface(
    GetDCOMPSurfaceCallback callback) {
  if (has_get_dcomp_surface_called_) {
    renderer_extension_receiver_.ReportBadMessage(
        "GetDCOMPSurface should only be called once!");
    return;
  }

  has_get_dcomp_surface_called_ = true;
  renderer_->GetDCompSurface(
      base::BindOnce(&MediaFoundationRendererWrapper::OnReceiveDCOMPSurface,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void MediaFoundationRendererWrapper::SetVideoStreamEnabled(bool enabled) {
  renderer_->SetVideoStreamEnabled(enabled);
}

void MediaFoundationRendererWrapper::SetOutputRect(
    const gfx::Rect& output_rect,
    SetOutputRectCallback callback) {
  renderer_->SetOutputRect(output_rect, std::move(callback));
}

void MediaFoundationRendererWrapper::OnMuteStateChange(bool muted) {
  DVLOG_FUNC(2) << ": muted=" << muted;

  if (muted == muted_)
    return;

  muted_ = muted;
  renderer_->SetVolume(muted_ ? 0 : volume_);
}

void MediaFoundationRendererWrapper::OnGpuLuidChange(
    const CHROME_LUID& adapter_luid) {
  renderer_->SetGpuProcessAdapterLuid(ChromeLuidToLuid(adapter_luid));
}

void MediaFoundationRendererWrapper::OnReceiveDCOMPSurface(
    GetDCOMPSurfaceCallback callback,
    base::win::ScopedHandle handle,
    const std::string& error) {
  if (!handle.IsValid()) {
    std::move(callback).Run(std::nullopt, "invalid handle: " + error);
    return;
  }

  if (!dcomp_surface_registry_) {
    frame_interfaces_->CreateDCOMPSurfaceRegistry(
        dcomp_surface_registry_.BindNewPipeAndPassReceiver());
  }

  auto register_cb = base::BindOnce(
      &MediaFoundationRendererWrapper::OnDCOMPSurfaceHandleRegistered,
      weak_factory_.GetWeakPtr(), std::move(callback));

  dcomp_surface_registry_->RegisterDCOMPSurfaceHandle(
      mojo::PlatformHandle(std::move(handle)),
      mojo::WrapCallbackWithDefaultInvokeIfNotRun(std::move(register_cb),
                                                  std::nullopt));
}

void MediaFoundationRendererWrapper::OnDCOMPSurfaceHandleRegistered(
    GetDCOMPSurfaceCallback callback,
    const std::optional<base::UnguessableToken>& token) {
  std::string error;
  if (token) {
    DCHECK(dcomp_surface_token_.is_empty());
    dcomp_surface_token_ = token.value();
  } else {
    error = "dcomp surface handle registration failed";
  }

  std::move(callback).Run(token, error);
}

void MediaFoundationRendererWrapper::OnFramePoolInitialized(
    std::vector<MediaFoundationFrameInfo> frame_textures,
    const gfx::Size& texture_size) {
  auto pool_params = media::mojom::FramePoolInitializationParameters::New();
  for (auto& texture : frame_textures) {
    auto frame_info = media::mojom::FrameTextureInfo::New();
    gfx::GpuMemoryBufferHandle gpu_handle;

    gpu_handle.dxgi_handle = std::move(texture.dxgi_handle);
    gpu_handle.dxgi_token = gfx::DXGIHandleToken();
    gpu_handle.type = gfx::GpuMemoryBufferType::DXGI_SHARED_HANDLE;

    frame_info->token = texture.token;
    frame_info->texture_handle = std::move(gpu_handle);
    pool_params->frame_textures.emplace_back(std::move(frame_info));
  }

  pool_params->texture_size = texture_size;
  client_extension_remote_->InitializeFramePool(std::move(pool_params));
}

void MediaFoundationRendererWrapper::OnFrameGeneratedByMediaFoundation(
    const base::UnguessableToken& frame_token,
    const gfx::Size& frame_size,
    base::TimeDelta frame_timestamp) {
  client_extension_remote_->OnFrameAvailable(frame_token, frame_size,
                                             frame_timestamp);
}

void MediaFoundationRendererWrapper::NotifyFrameReleased(
    const base::UnguessableToken& frame_token) {
  renderer_->NotifyFrameReleased(frame_token);
}

void MediaFoundationRendererWrapper::RequestNextFrame() {
  renderer_->RequestNextFrame();
}

void MediaFoundationRendererWrapper::SetMediaFoundationRenderingMode(
    MediaFoundationRenderingMode mode) {
  renderer_->SetMediaFoundationRenderingMode(mode);
}
}  // namespace media
