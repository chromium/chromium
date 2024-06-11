// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/frameless_media_interface_proxy.h"

#include <tuple>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "build/build_config.h"
#include "content/public/browser/media_service.h"
#include "content/public/browser/render_process_host.h"
#include "media/base/cdm_context.h"
#include "media/mojo/mojom/media_service.mojom.h"
#include "media/mojo/mojom/renderer_extensions.mojom.h"
#include "media/mojo/mojom/stable/stable_video_decoder.mojom.h"

#if BUILDFLAG(ALLOW_OOP_VIDEO_DECODER)
#include "content/public/browser/stable_video_decoder_factory.h"
#include "media/base/media_switches.h"
#include "mojo/public/cpp/bindings/message.h"
#endif  // BUILDFLAG(ALLOW_OOP_VIDEO_DECODER)

namespace content {

FramelessMediaInterfaceProxy::FramelessMediaInterfaceProxy(
    RenderProcessHost* render_process_host)
    : render_process_host_(render_process_host) {
  DVLOG(1) << __func__;
}

FramelessMediaInterfaceProxy::~FramelessMediaInterfaceProxy() {
  DVLOG(1) << __func__;
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

void FramelessMediaInterfaceProxy::Add(
    mojo::PendingReceiver<media::mojom::InterfaceFactory> receiver) {
  DVLOG(1) << __func__;
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  receivers_.Add(this, std::move(receiver));
}

void FramelessMediaInterfaceProxy::CreateAudioDecoder(
    mojo::PendingReceiver<media::mojom::AudioDecoder> receiver) {
  DVLOG(2) << __func__;
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  InterfaceFactory* factory = GetMediaInterfaceFactory();
  if (factory)
    factory->CreateAudioDecoder(std::move(receiver));
}

void FramelessMediaInterfaceProxy::CreateVideoDecoder(
    mojo::PendingReceiver<media::mojom::VideoDecoder> receiver,
    mojo::PendingRemote<media::stable::mojom::StableVideoDecoder>
        dst_video_decoder) {
  DVLOG(2) << __func__;
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // The browser process cannot act as a proxy for video decoding and clients
  // should not attempt to use it that way.
  DCHECK(!dst_video_decoder);

  InterfaceFactory* factory = GetMediaInterfaceFactory();
  if (!factory)
    return;

  mojo::PendingRemote<media::stable::mojom::StableVideoDecoder>
      oop_video_decoder;
#if BUILDFLAG(ALLOW_OOP_VIDEO_DECODER)
  switch (media::GetOutOfProcessVideoDecodingMode()) {
    case media::OOPVDMode::kEnabledWithGpuProcessAsProxy:
      if (!render_process_host_) {
        if (!stable_vd_factory_remote_.is_bound()) {
          LaunchStableVideoDecoderFactory(
              stable_vd_factory_remote_.BindNewPipeAndPassReceiver());
          stable_vd_factory_remote_.reset_on_disconnect();
        }

        CHECK(stable_vd_factory_remote_.is_bound());

        stable_vd_factory_remote_->CreateStableVideoDecoder(
            oop_video_decoder.InitWithNewPipeAndPassReceiver(), /*tracker=*/{});
      } else {
        render_process_host_->CreateStableVideoDecoder(
            oop_video_decoder.InitWithNewPipeAndPassReceiver());
      }
      break;
    case media::OOPVDMode::kEnabledWithoutGpuProcessAsProxy:
      // Well-behaved clients shouldn't call CreateVideoDecoder() in this OOP-VD
      // mode.
      //
      // Note: FramelessMediaInterfaceProxy::CreateVideoDecoder() might be
      // called outside of a message dispatch, e.g., by
      // GpuDataManagerImplPrivate::RequestMojoMediaVideoCapabilities().
      // However, these calls should only occur inside of the browser process
      // which we can trust not to reach this point, hence the CHECK().
      CHECK(mojo::IsInMessageDispatch());
      mojo::ReportBadMessage("CreateVideoDecoder() called unexpectedly");
      return;
    case media::OOPVDMode::kDisabled:
      break;
  }
#endif  // BUILDFLAG(ALLOW_OOP_VIDEO_DECODER)
  factory->CreateVideoDecoder(std::move(receiver),
                              std::move(oop_video_decoder));
}

#if BUILDFLAG(ALLOW_OOP_VIDEO_DECODER)
void FramelessMediaInterfaceProxy::CreateStableVideoDecoder(
    mojo::PendingReceiver<media::stable::mojom::StableVideoDecoder>
        video_decoder) {
  DVLOG(2) << __func__;
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  switch (media::GetOutOfProcessVideoDecodingMode()) {
    case media::OOPVDMode::kEnabledWithGpuProcessAsProxy:
    case media::OOPVDMode::kDisabled:
      // Well-behaved clients shouldn't call CreateStableVideoDecoder() in this
      // OOP-VD mode.
      //
      // Note: FramelessMediaInterfaceProxy::CreateStableVideoDecoder() might be
      // called outside of a message dispatch, e.g., by
      // GpuDataManagerImplPrivate::RequestMojoMediaVideoCapabilities().
      // However, these calls should only occur inside of the browser process
      // which we can trust not to reach this point, hence the CHECK().
      CHECK(mojo::IsInMessageDispatch());
      mojo::ReportBadMessage("CreateStableVideoDecoder() called unexpectedly");
      return;
    case media::OOPVDMode::kEnabledWithoutGpuProcessAsProxy:
      if (!render_process_host_) {
        if (!stable_vd_factory_remote_.is_bound()) {
          LaunchStableVideoDecoderFactory(
              stable_vd_factory_remote_.BindNewPipeAndPassReceiver());
          stable_vd_factory_remote_.reset_on_disconnect();
        }

        CHECK(stable_vd_factory_remote_.is_bound());

        stable_vd_factory_remote_->CreateStableVideoDecoder(
            std::move(video_decoder), /*tracker=*/{});
      } else {
        render_process_host_->CreateStableVideoDecoder(
            std::move(video_decoder));
      }
      break;
  }
}
#endif  // BUILDFLAG(ALLOW_OOP_VIDEO_DECODER)

void FramelessMediaInterfaceProxy::CreateAudioEncoder(
    mojo::PendingReceiver<media::mojom::AudioEncoder> receiver) {
  DVLOG(2) << __func__;
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  InterfaceFactory* factory = GetMediaInterfaceFactory();
  if (factory)
    factory->CreateAudioEncoder(std::move(receiver));
}

void FramelessMediaInterfaceProxy::CreateDefaultRenderer(
    const std::string& audio_device_id,
    mojo::PendingReceiver<media::mojom::Renderer> receiver) {}

#if BUILDFLAG(ENABLE_CAST_RENDERER)
void FramelessMediaInterfaceProxy::CreateCastRenderer(
    const base::UnguessableToken& overlay_plane_id,
    mojo::PendingReceiver<media::mojom::Renderer> receiver) {}
#endif  // BUILDFLAG(ENABLE_CAST_RENDERER)

#if BUILDFLAG(IS_ANDROID)
void FramelessMediaInterfaceProxy::CreateFlingingRenderer(
    const std::string& audio_device_id,
    mojo::PendingRemote<media::mojom::FlingingRendererClientExtension>
        client_extenion,
    mojo::PendingReceiver<media::mojom::Renderer> receiver) {}

void FramelessMediaInterfaceProxy::CreateMediaPlayerRenderer(
    mojo::PendingRemote<media::mojom::MediaPlayerRendererClientExtension>
        client_extension_remote,
    mojo::PendingReceiver<media::mojom::Renderer> receiver,
    mojo::PendingReceiver<media::mojom::MediaPlayerRendererExtension>
        renderer_extension_receiver) {}
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_WIN)
// Unimplemented method as this requires CDM and media::Renderer services with
// frame context.
void FramelessMediaInterfaceProxy::CreateMediaFoundationRenderer(
    mojo::PendingRemote<media::mojom::MediaLog> media_log_remote,
    mojo::PendingReceiver<media::mojom::Renderer> receiver,
    mojo::PendingReceiver<media::mojom::MediaFoundationRendererExtension>
        renderer_extension_receiver,
    mojo::PendingRemote<media::mojom::MediaFoundationRendererClientExtension>
        client_extension_remote) {}
#endif  // BUILDFLAG(IS_WIN)

void FramelessMediaInterfaceProxy::CreateCdm(const media::CdmConfig& cdm_config,
                                             CreateCdmCallback callback) {
  std::move(callback).Run(mojo::NullRemote(), nullptr,
                          media::CreateCdmStatus::kCdmNotSupported);
}

media::mojom::InterfaceFactory*
FramelessMediaInterfaceProxy::GetMediaInterfaceFactory() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (!interface_factory_remote_)
    ConnectToMediaService();

  return interface_factory_remote_.get();
}

void FramelessMediaInterfaceProxy::ConnectToMediaService() {
  DVLOG(1) << __func__;
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!interface_factory_remote_);

  mojo::PendingRemote<media::mojom::FrameInterfaceFactory> interfaces;
  std::ignore = interfaces.InitWithNewPipeAndPassReceiver();

  GetMediaService().CreateInterfaceFactory(
      interface_factory_remote_.BindNewPipeAndPassReceiver(),
      std::move(interfaces));
  interface_factory_remote_.set_disconnect_handler(base::BindOnce(
      &FramelessMediaInterfaceProxy::OnMediaServiceConnectionError,
      base::Unretained(this)));
}

void FramelessMediaInterfaceProxy::OnMediaServiceConnectionError() {
  DVLOG(1) << __func__;
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  interface_factory_remote_.reset();
}

}  // namespace content
