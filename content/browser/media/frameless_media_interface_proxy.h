// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_FRAMELESS_MEDIA_INTERFACE_PROXY_H_
#define CONTENT_BROWSER_MEDIA_FRAMELESS_MEDIA_INTERFACE_PROXY_H_

#include <string>

#include "base/threading/thread_checker.h"
#include "base/unguessable_token.h"
#include "build/build_config.h"
#include "media/media_buildflags.h"
#include "media/mojo/buildflags.h"
#include "media/mojo/mojom/interface_factory.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#include "media/mojo/mojom/stable/stable_video_decoder.mojom.h"
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

namespace content {

class RenderProcessHost;

// This implements the media::mojom::InterfaceFactory interface for a
// RenderProcessHostImpl. It does not support creating services that require a
// frame context (ie. CDMs and renderers).
//
// It is used in cases without a frame context, namely by WebRTC, WebCodecs
// (which may be operating in a worker context), and for early querying of
// supported codecs.
class FramelessMediaInterfaceProxy final
    : public media::mojom::InterfaceFactory {
 public:
  explicit FramelessMediaInterfaceProxy(RenderProcessHost* render_process_host);

  FramelessMediaInterfaceProxy(const FramelessMediaInterfaceProxy&) = delete;
  FramelessMediaInterfaceProxy& operator=(const FramelessMediaInterfaceProxy&) =
      delete;

  ~FramelessMediaInterfaceProxy() final;

  void Add(mojo::PendingReceiver<media::mojom::InterfaceFactory> receiver);

  // media::mojom::InterfaceFactory implementation.
  void CreateAudioDecoder(
      mojo::PendingReceiver<media::mojom::AudioDecoder> receiver) final;
  void CreateVideoDecoder(
      mojo::PendingReceiver<media::mojom::VideoDecoder> receiver,
      mojo::PendingRemote<media::stable::mojom::StableVideoDecoder>
          dst_video_decoder) final;
#if BUILDFLAG(ALLOW_OOP_VIDEO_DECODER)
  void CreateStableVideoDecoder(
      mojo::PendingReceiver<media::stable::mojom::StableVideoDecoder>
          video_decoder) final;
#endif  // BUILDFLAG(ALLOW_OOP_VIDEO_DECODER)
  void CreateAudioEncoder(
      mojo::PendingReceiver<media::mojom::AudioEncoder> receiver) final;
  void CreateDefaultRenderer(
      const std::string& audio_device_id,
      mojo::PendingReceiver<media::mojom::Renderer> receiver) final;
#if BUILDFLAG(ENABLE_CAST_RENDERER)
  void CreateCastRenderer(
      const base::UnguessableToken& overlay_plane_id,
      mojo::PendingReceiver<media::mojom::Renderer> receiver) final;
#endif
#if BUILDFLAG(IS_ANDROID)
  void CreateMediaPlayerRenderer(
      mojo::PendingRemote<media::mojom::MediaPlayerRendererClientExtension>
          client_extension_remote,
      mojo::PendingReceiver<media::mojom::Renderer> receiver,
      mojo::PendingReceiver<media::mojom::MediaPlayerRendererExtension>
          renderer_extension_receiver) final;
  void CreateFlingingRenderer(
      const std::string& presentation_id,
      mojo::PendingRemote<media::mojom::FlingingRendererClientExtension>
          client_extension,
      mojo::PendingReceiver<media::mojom::Renderer> receiver) final;
#endif  // BUILDFLAG(IS_ANDROID)
#if BUILDFLAG(IS_WIN)
  void CreateMediaFoundationRenderer(
      mojo::PendingRemote<media::mojom::MediaLog> media_log_remote,
      mojo::PendingReceiver<media::mojom::Renderer> receiver,
      mojo::PendingReceiver<media::mojom::MediaFoundationRendererExtension>
          renderer_extension_receiver,
      mojo::PendingRemote<media::mojom::MediaFoundationRendererClientExtension>
          client_extension_remote) final;
#endif  // BUILDFLAG(IS_WIN)
  void CreateCdm(const media::CdmConfig& cdm_config,
                 CreateCdmCallback callback) final;

 private:
  media::mojom::InterfaceFactory* GetMediaInterfaceFactory();
  void ConnectToMediaService();
  void OnMediaServiceConnectionError();

  // Connection to the remote media InterfaceFactory.
  mojo::Remote<media::mojom::InterfaceFactory> interface_factory_remote_;

  // Connections to the renderer.
  mojo::ReceiverSet<media::mojom::InterfaceFactory> receivers_;

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  // Connection to the StableVideoDecoderFactory that lives in a utility
  // process. This is only used for out-of-process video decoding and only when
  // the FramelessMediaInterfaceProxy is created without a RenderProcessHost
  // (e.g., to get the supported video decoder configurations). Note that we
  // make this a member instead of a local variable inside CreateVideoDecoder()
  // in order to keep the video decoder process alive for the lifetime of the
  // FramelessMediaInterfaceProxy.
  mojo::Remote<media::stable::mojom::StableVideoDecoderFactory>
      stable_vd_factory_remote_;
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

  // FramelessMediaInterfaceProxy is fully owned by the RenderProcessHostImpl,
  // and the latter never gives up that ownership. Therefore,
  // *|render_process_host_| will never be destroyed before it's used by
  // *|this|.
  const raw_ptr<RenderProcessHost> render_process_host_ = nullptr;

  THREAD_CHECKER(thread_checker_);
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_FRAMELESS_MEDIA_INTERFACE_PROXY_H_
