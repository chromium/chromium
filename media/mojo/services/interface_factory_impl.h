// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_SERVICES_INTERFACE_FACTORY_IMPL_H_
#define MEDIA_MOJO_SERVICES_INTERFACE_FACTORY_IMPL_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/unguessable_token.h"
#include "build/build_config.h"
#include "media/base/media_util.h"
#include "media/media_buildflags.h"
#include "media/mojo/buildflags.h"
#include "media/mojo/mojom/audio_decoder.mojom.h"
#include "media/mojo/mojom/audio_encoder.mojom.h"
#include "media/mojo/mojom/content_decryption_module.mojom.h"
#include "media/mojo/mojom/decryptor.mojom.h"
#include "media/mojo/mojom/frame_interface_factory.mojom.h"
#include "media/mojo/mojom/interface_factory.mojom.h"
#include "media/mojo/mojom/media_log.mojom.h"
#include "media/mojo/mojom/renderer.mojom.h"
#include "media/mojo/mojom/video_decoder.mojom.h"
#include "media/mojo/services/deferred_destroy_unique_receiver_set.h"
#include "media/mojo/services/mojo_cdm_service_context.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/unique_receiver_set.h"

namespace media {

class CdmFactory;
class MojoMediaClient;
class Renderer;

class InterfaceFactoryImpl final
    : public DeferredDestroy<mojom::InterfaceFactory> {
 public:
  InterfaceFactoryImpl(
      mojo::PendingRemote<mojom::FrameInterfaceFactory> frame_interfaces,
      MojoMediaClient* mojo_media_client);

  InterfaceFactoryImpl(const InterfaceFactoryImpl&) = delete;
  InterfaceFactoryImpl& operator=(const InterfaceFactoryImpl&) = delete;

  ~InterfaceFactoryImpl() final;

  // mojom::InterfaceFactory implementation.
  void CreateAudioDecoder(
      mojo::PendingReceiver<mojom::AudioDecoder> receiver) final;
  void CreateVideoDecoder(
      mojo::PendingReceiver<mojom::VideoDecoder> receiver,
      mojo::PendingRemote<media::stable::mojom::StableVideoDecoder>
          dst_video_decoder) final;
#if BUILDFLAG(ALLOW_OOP_VIDEO_DECODER)
  void CreateStableVideoDecoder(
      mojo::PendingReceiver<media::stable::mojom::StableVideoDecoder>
          video_decoder) final;
#endif  // BUILDFLAG(ALLOW_OOP_VIDEO_DECODER)

  void CreateAudioEncoder(
      mojo::PendingReceiver<mojom::AudioEncoder> receiver) final;

  void CreateDefaultRenderer(
      const std::string& audio_device_id,
      mojo::PendingReceiver<mojom::Renderer> receiver) final;
#if BUILDFLAG(ENABLE_CAST_RENDERER)
  void CreateCastRenderer(
      const base::UnguessableToken& overlay_plane_id,
      mojo::PendingReceiver<mojom::Renderer> receiver) final;
#endif
#if BUILDFLAG(IS_ANDROID)
  void CreateMediaPlayerRenderer(
      mojo::PendingRemote<mojom::MediaPlayerRendererClientExtension>
          client_extension_remote,
      mojo::PendingReceiver<mojom::Renderer> receiver,
      mojo::PendingReceiver<mojom::MediaPlayerRendererExtension>
          renderer_extension_receiver) final;
  void CreateFlingingRenderer(
      const std::string& presentation_id,
      mojo::PendingRemote<mojom::FlingingRendererClientExtension>
          client_extension,
      mojo::PendingReceiver<mojom::Renderer> receiver) final;
#endif  // BUILDFLAG(IS_ANDROID)
#if BUILDFLAG(IS_WIN)
  void CreateMediaFoundationRenderer(
      mojo::PendingRemote<mojom::MediaLog> media_log_remote,
      mojo::PendingReceiver<mojom::Renderer> receiver,
      mojo::PendingReceiver<mojom::MediaFoundationRendererExtension>
          renderer_extension_receiver,
      mojo::PendingRemote<media::mojom::MediaFoundationRendererClientExtension>
          client_extension_remote) final;
#endif  // BUILDFLAG(IS_WIN)

  void CreateCdm(const CdmConfig& cdm_config, CreateCdmCallback callback) final;

  // DeferredDestroy<mojom::InterfaceFactory> implementation.
  void OnDestroyPending(base::OnceClosure destroy_cb) final;

 private:
  // Returns true when there is no media component (audio/video decoder,
  // renderer, cdm and cdm proxy) receivers exist.
  bool IsEmpty();

  void SetReceiverDisconnectHandler();
  void OnReceiverDisconnect();

#if BUILDFLAG(ENABLE_MOJO_RENDERER) || BUILDFLAG(ENABLE_CAST_RENDERER) || \
    BUILDFLAG(IS_WIN)
  // Creates MojoRendererService for `renderer`, bind it to `receiver` and add
  // them to `renderer_receivers_`.
  void AddRenderer(std::unique_ptr<media::Renderer> renderer,
                   mojo::PendingReceiver<mojom::Renderer> receiver);
#endif

#if BUILDFLAG(ENABLE_MOJO_CDM)
  CdmFactory* GetCdmFactory();
  void OnCdmServiceInitialized(MojoCdmService* raw_mojo_cdm_service,
                               CreateCdmCallback callback,
                               mojom::CdmContextPtr cdm_context,
                               CreateCdmStatus status);
#endif  // BUILDFLAG(ENABLE_MOJO_CDM)

#if BUILDFLAG(ALLOW_OOP_VIDEO_DECODER)
  void FinishCreatingVideoDecoder(
      mojo::PendingReceiver<mojom::VideoDecoder> receiver,
      mojo::PendingRemote<media::stable::mojom::StableVideoDecoder>
          dst_video_decoder);
#endif  // BUILDFLAG(ALLOW_OOP_VIDEO_DECODER)

  // Must be declared before the receivers below because the bound objects might
  // take a raw pointer of |cdm_service_context_| and assume it's always
  // available.
  MojoCdmServiceContext cdm_service_context_;

#if BUILDFLAG(ENABLE_MOJO_AUDIO_DECODER)
  class AudioDecoderReceivers;
  std::unique_ptr<AudioDecoderReceivers> audio_decoder_receivers_;
#endif  // BUILDFLAG(ENABLE_MOJO_AUDIO_DECODER)

#if BUILDFLAG(ENABLE_MOJO_VIDEO_DECODER)
  mojo::UniqueReceiverSet<mojom::VideoDecoder> video_decoder_receivers_;
#endif  // BUILDFLAG(ENABLE_MOJO_VIDEO_DECODER)

#if BUILDFLAG(ENABLE_MOJO_AUDIO_ENCODER)
  mojo::UniqueReceiverSet<mojom::AudioEncoder> audio_encoder_receivers_;
#endif  // BUILDFLAG(ENABLE_MOJO_VIDEO_ENCODER)

#if BUILDFLAG(ENABLE_MOJO_RENDERER) || BUILDFLAG(ENABLE_CAST_RENDERER) || \
    BUILDFLAG(IS_WIN)
  // TODO(xhwang): Use MojoMediaLog for Renderer.
  NullMediaLog media_log_;
  mojo::UniqueReceiverSet<mojom::Renderer> renderer_receivers_;
#endif

#if BUILDFLAG(ENABLE_MOJO_CDM)
  std::unique_ptr<CdmFactory> cdm_factory_;
  mojo::UniqueReceiverSet<mojom::ContentDecryptionModule> cdm_receivers_;

  // MojoCdmServices pending initialization.
  std::map<MojoCdmService*, std::unique_ptr<MojoCdmService>>
      pending_mojo_cdm_services_;
#endif  // BUILDFLAG(ENABLE_MOJO_CDM)

  mojo::Remote<mojom::FrameInterfaceFactory> frame_interfaces_;

  mojo::UniqueReceiverSet<mojom::Decryptor> decryptor_receivers_;

  raw_ptr<MojoMediaClient> mojo_media_client_;
  base::OnceClosure destroy_cb_;

  // NOTE: Weak pointers must be invalidated before all other member variables.
  base::WeakPtrFactory<InterfaceFactoryImpl> weak_ptr_factory_{this};
};

}  // namespace media

#endif  // MEDIA_MOJO_SERVICES_INTERFACE_FACTORY_IMPL_H_
