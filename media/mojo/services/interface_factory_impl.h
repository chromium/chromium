// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_SERVICES_INTERFACE_FACTORY_IMPL_H_
#define MEDIA_MOJO_SERVICES_INTERFACE_FACTORY_IMPL_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/unguessable_token.h"
#include "build/build_config.h"
#include "media/base/media_util.h"
#include "media/media_buildflags.h"
#include "media/mojo/buildflags.h"
#include "media/mojo/mojom/audio_decoder.mojom.h"
#include "media/mojo/mojom/content_decryption_module.mojom.h"
#include "media/mojo/mojom/decryptor.mojom.h"
#include "media/mojo/mojom/frame_interface_factory.mojom.h"
#include "media/mojo/mojom/interface_factory.mojom.h"
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

class InterfaceFactoryImpl final
    : public DeferredDestroy<mojom::InterfaceFactory> {
 public:
  InterfaceFactoryImpl(
      mojo::PendingRemote<mojom::FrameInterfaceFactory> frame_interfaces,
      MojoMediaClient* mojo_media_client);
  ~InterfaceFactoryImpl() final;

  // mojom::InterfaceFactory implementation.
  void CreateAudioDecoder(
      mojo::PendingReceiver<mojom::AudioDecoder> receiver) final;
  void CreateVideoDecoder(
      mojo::PendingReceiver<mojom::VideoDecoder> receiver) final;
  void CreateDefaultRenderer(
      const std::string& audio_device_id,
      mojo::PendingReceiver<mojom::Renderer> receiver) final;
#if BUILDFLAG(ENABLE_CAST_RENDERER)
  void CreateCastRenderer(
      const base::UnguessableToken& overlay_plane_id,
      mojo::PendingReceiver<media::mojom::Renderer> receiver) final;
#endif
#if defined(OS_ANDROID)
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
#endif  // defined(OS_ANDROID)
#if defined(OS_WIN)
  void CreateMediaFoundationRenderer(
      mojo::PendingReceiver<media::mojom::Renderer> receiver,
      mojo::PendingReceiver<media::mojom::MediaFoundationRendererExtension>
          renderer_extension_receiver) final;
#endif  // defined(OS_WIN)

  void CreateCdm(const std::string& key_system,
                 const CdmConfig& cdm_config,
                 CreateCdmCallback callback) final;

  // DeferredDestroy<mojom::InterfaceFactory> implemenation.
  void OnDestroyPending(base::OnceClosure destroy_cb) final;

 private:
  // Returns true when there is no media component (audio/video decoder,
  // renderer, cdm and cdm proxy) receivers exist.
  bool IsEmpty();

  void SetReceiverDisconnectHandler();
  void OnReceiverDisconnect();

#if BUILDFLAG(ENABLE_MOJO_CDM)
  CdmFactory* GetCdmFactory();
  void OnCdmServiceCreated(CreateCdmCallback callback,
                           std::unique_ptr<MojoCdmService> cdm_service,
                           mojo::PendingRemote<mojom::Decryptor> decryptor,
                           const std::string& error_message);
#endif  // BUILDFLAG(ENABLE_MOJO_CDM)

#if defined(OS_WIN)
  void CreateMediaFoundationRendererOnTaskRunner(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      mojo::PendingReceiver<media::mojom::Renderer> receiver,
      mojo::PendingReceiver<media::mojom::MediaFoundationRendererExtension>
          renderer_extension_receiver);
#endif  // defined(OS_WIN)

  // Must be declared before the receivers below because the bound objects might
  // take a raw pointer of |cdm_service_context_| and assume it's always
  // available.
  MojoCdmServiceContext cdm_service_context_;

#if BUILDFLAG(ENABLE_MOJO_AUDIO_DECODER)
  mojo::UniqueReceiverSet<mojom::AudioDecoder> audio_decoder_receivers_;
#endif  // BUILDFLAG(ENABLE_MOJO_AUDIO_DECODER)

#if BUILDFLAG(ENABLE_MOJO_VIDEO_DECODER)
  mojo::UniqueReceiverSet<mojom::VideoDecoder> video_decoder_receivers_;
#endif  // BUILDFLAG(ENABLE_MOJO_VIDEO_DECODER)

#if BUILDFLAG(ENABLE_MOJO_RENDERER) || BUILDFLAG(ENABLE_CAST_RENDERER)
  // TODO(xhwang): Use MojoMediaLog for Renderer.
  NullMediaLog media_log_;
  mojo::UniqueReceiverSet<mojom::Renderer> renderer_receivers_;
#endif  // BUILDFLAG(ENABLE_MOJO_RENDERER) || BUILDFLAG(ENABLE_CAST_RENDERER)

#if BUILDFLAG(ENABLE_MOJO_CDM)
  std::unique_ptr<CdmFactory> cdm_factory_;
  mojo::UniqueReceiverSet<mojom::ContentDecryptionModule> cdm_receivers_;
#endif  // BUILDFLAG(ENABLE_MOJO_CDM)

  mojo::Remote<mojom::FrameInterfaceFactory> frame_interfaces_;

  mojo::UniqueReceiverSet<mojom::Decryptor> decryptor_receivers_;

  MojoMediaClient* mojo_media_client_;
  base::OnceClosure destroy_cb_;

  // NOTE: Weak pointers must be invalidated before all other member variables.
  base::WeakPtrFactory<InterfaceFactoryImpl> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(InterfaceFactoryImpl);
};

}  // namespace media

#endif  // MEDIA_MOJO_SERVICES_INTERFACE_FACTORY_IMPL_H_
