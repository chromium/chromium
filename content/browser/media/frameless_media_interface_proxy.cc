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

#if BUILDFLAG(ALLOW_OOP_VIDEO_DECODER)
#include "content/public/browser/oop_video_decoder_factory.h"
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
    mojo::PendingRemote<media::mojom::VideoDecoder> dst_video_decoder) {
  DVLOG(2) << __func__;
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // The browser process cannot act as a proxy for video decoding and clients
  // should not attempt to use it that way.
  DCHECK(!dst_video_decoder);

  InterfaceFactory* factory = GetMediaInterfaceFactory();
  if (!factory)
    return;

  mojo::PendingRemote<media::mojom::VideoDecoder> oop_video_decoder;
#if BUILDFLAG(ALLOW_OOP_VIDEO_DECODER)
  if (media::IsOutOfProcessVideoDecodingEnabled()) {
    if (!render_process_host_) {
      if (!vd_factory_remote_.is_bound()) {
        LaunchOOPVideoDecoderFactory(
            vd_factory_remote_.BindNewPipeAndPassReceiver(), /*gpu_remote=*/{});
        vd_factory_remote_.reset_on_disconnect();
      }

      CHECK(vd_factory_remote_.is_bound());

      vd_factory_remote_->CreateVideoDecoderWithTracker(
          oop_video_decoder.InitWithNewPipeAndPassReceiver(), /*tracker=*/{});
    } else {
      render_process_host_->CreateOOPVideoDecoder(
          oop_video_decoder.InitWithNewPipeAndPassReceiver());
    }
  }
#endif  // BUILDFLAG(ALLOW_OOP_VIDEO_DECODER)
  factory->CreateVideoDecoder(std::move(receiver),
                              std::move(oop_video_decoder));
}

#if BUILDFLAG(ALLOW_OOP_VIDEO_DECODER)
void FramelessMediaInterfaceProxy::CreateVideoDecoderWithTracker(
    mojo::PendingReceiver<media::mojom::VideoDecoder> receiver,
    mojo::PendingRemote<media::mojom::VideoDecoderTracker> tracker) {
  // mojo::ReportBadMessage() should be called directly within the stack frame
  // of a message dispatch, hence the CHECK().
  // CreateVideoDecoderWithTracker() should be called by the browser process
  // only. This implementation is exposed to the renderer. Well-behaved clients
  // (renderers) shouldn't call CreateVideoDecoderWithTracker().
  CHECK(mojo::IsInMessageDispatch());
  mojo::ReportBadMessage("CreateVideoDecoderWithTracker() called unexpectedly");
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
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_WIN)
// Unimplemented method as this requires CDM and media::Renderer services with
// frame context.
void FramelessMediaInterfaceProxy::CreateMediaFoundationRenderer(
    mojo::PendingRemote<media::mojom::MediaLog> media_log_remote,
    mojo::PendingReceiver<media::mojom::Renderer> receiver,
    mojo::PendingReceiver<media::mojom::MediaFoundationRendererExtension>
        renderer_extension_receiver) {}
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
