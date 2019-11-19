// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_MEDIA_INTERFACE_PROXY_H_
#define CONTENT_BROWSER_MEDIA_MEDIA_INTERFACE_PROXY_H_

#include <map>
#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/threading/thread_checker.h"
#include "base/token.h"
#include "base/unguessable_token.h"
#include "build/build_config.h"
#include "content/browser/media/media_interface_factory_holder.h"
#include "media/media_buildflags.h"
#include "media/mojo/buildflags.h"
#include "media/mojo/mojom/content_decryption_module.mojom.h"
#include "media/mojo/mojom/interface_factory.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/service_manager/public/mojom/interface_provider.mojom.h"

namespace media {
class MediaInterfaceProvider;
}

namespace content {

class RenderFrameHost;

// This implements the media::mojom::InterfaceFactory interface for a
// RenderFrameHostImpl. Upon InterfaceFactory calls, it will
// figure out where to forward to the interface requests. For example,
// - When |enable_library_cdms| is true, forward CDM request to the CdmService
// rather than the general media service.
// - Forward CDM requests to different CdmService instances based on library
//   CDM types.
class MediaInterfaceProxy : public media::mojom::InterfaceFactory {
 public:
  // Constructs MediaInterfaceProxy and bind |this| to the |request|. When
  // connection error happens on the client interface, |error_handler| will be
  // called, which could destroy |this|.
  MediaInterfaceProxy(
      RenderFrameHost* render_frame_host,
      mojo::PendingReceiver<media::mojom::InterfaceFactory> receiver,
      base::OnceClosure error_handler);
  ~MediaInterfaceProxy() final;

  // media::mojom::InterfaceFactory implementation.
  void CreateAudioDecoder(
      mojo::PendingReceiver<media::mojom::AudioDecoder> receiver) final;
  void CreateVideoDecoder(
      mojo::PendingReceiver<media::mojom::VideoDecoder> receiver) final;
  void CreateDefaultRenderer(
      const std::string& audio_device_id,
      mojo::PendingReceiver<media::mojom::Renderer> receiver) final;
#if BUILDFLAG(ENABLE_CAST_RENDERER)
  void CreateCastRenderer(
      const base::UnguessableToken& overlay_plane_id,
      mojo::PendingReceiver<media::mojom::Renderer> receiver) final;
#endif
#if defined(OS_ANDROID)
  void CreateFlingingRenderer(
      const std::string& presentation_id,
      mojo::PendingRemote<media::mojom::FlingingRendererClientExtension>
          client_extension,
      mojo::PendingReceiver<media::mojom::Renderer> receiver) final;
  void CreateMediaPlayerRenderer(
      mojo::PendingRemote<media::mojom::MediaPlayerRendererClientExtension>
          client_extension_remote,
      mojo::PendingReceiver<media::mojom::Renderer> receiver,
      mojo::PendingReceiver<media::mojom::MediaPlayerRendererExtension>
          renderer_extension_request) final;
#endif  // defined(OS_ANDROID)
  void CreateCdm(const std::string& key_system,
                 mojo::PendingReceiver<media::mojom::ContentDecryptionModule>
                     receiver) final;
  void CreateDecryptor(
      int cdm_id,
      mojo::PendingReceiver<media::mojom::Decryptor> receiver) final;
#if BUILDFLAG(ENABLE_CDM_PROXY)
  void CreateCdmProxy(
      const base::Token& cdm_guid,
      mojo::PendingReceiver<media::mojom::CdmProxy> receiver) final;
#endif  // BUILDFLAG(ENABLE_CDM_PROXY)

 private:
  // Gets services provided by the browser (at RenderFrameHost level) to the
  // mojo media (or CDM) service running remotely. |cdm_file_system_id| is
  // used to register the appropriate CdmStorage interface needed by the CDM.
  mojo::PendingRemote<service_manager::mojom::InterfaceProvider>
  GetFrameServices(const base::Token& cdm_guid,
                   const std::string& cdm_file_system_id);

#if BUILDFLAG(ENABLE_LIBRARY_CDMS)
  // Gets a CdmFactory pointer for |key_system|. Returns null if unexpected
  // error happened.
  media::mojom::CdmFactory* GetCdmFactory(const std::string& key_system);

  // Connects to the CDM service associated with |cdm_guid|, adds the new
  // CdmFactoryPtr to the |cdm_factory_map_|, and returns the newly created
  // CdmFactory pointer. Returns nullptr if unexpected error happened.
  // |cdm_path| will be used to preload the CDM, if necessary.
  // |cdm_file_system_id| is used when creating the matching storage interface.
  media::mojom::CdmFactory* ConnectToCdmService(
      const base::Token& cdm_guid,
      const base::FilePath& cdm_path,
      const std::string& cdm_file_system_id);

  // Callback for connection error from the CdmFactoryPtr in the
  // |cdm_factory_map_| associated with |cdm_guid|.
  void OnCdmServiceConnectionError(const base::Token& cdm_guid);

#if BUILDFLAG(ENABLE_CDM_PROXY)
  // Creates a CdmProxy for the CDM in CdmService. Not implemented in
  // CreateCdmProxy() because we don't want any client to be able to create
  // a CdmProxy.
  void CreateCdmProxyInternal(
      const base::Token& cdm_guid,
      mojo::PendingReceiver<media::mojom::CdmProxy> receiver);
#endif  // BUILDFLAG(ENABLE_CDM_PROXY)
#endif  // BUILDFLAG(ENABLE_LIBRARY_CDMS)

  // Safe to hold a raw pointer since |this| is owned by RenderFrameHostImpl.
  RenderFrameHost* const render_frame_host_;

  // Receiver for incoming InterfaceFactoryRequest from the the RenderFrameImpl.
  mojo::Receiver<InterfaceFactory> receiver_;

  // TODO(xhwang): Replace InterfaceProvider with a dedicated host interface.
  // See http://crbug.com/660573
  std::vector<std::unique_ptr<media::MediaInterfaceProvider>> media_registries_;

  // InterfacePtr to the remote InterfaceFactory implementation
  // in the service named kMediaServiceName hosted in the process specified by
  // the "mojo_media_host" gn argument. Available options are browser, GPU and
  // utility processes.
  std::unique_ptr<MediaInterfaceFactoryHolder> media_interface_factory_ptr_;

#if BUILDFLAG(ENABLE_CAST_RENDERER)
  // InterfacePtr to the remote InterfaceFactory implementation
  // in the service named kMediaRendererServiceName hosted. It provides the
  // remote implementation of media::Renderer and
  // media::ContentDecryptionModule.
  std::unique_ptr<MediaInterfaceFactoryHolder>
      media_renderer_interface_factory_ptr_;
#endif  // BUILDFLAG(ENABLE_CAST_RENDERER)

#if BUILDFLAG(ENABLE_LIBRARY_CDMS)
  // CDM GUID to CDM InterfaceFactoryRemote mapping, where the InterfaceFactory
  // instances live in the standalone kCdmServiceName service instances.
  std::map<base::Token, mojo::Remote<media::mojom::CdmFactory>>
      cdm_factory_map_;
#endif  // BUILDFLAG(ENABLE_LIBRARY_CDMS)

  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(MediaInterfaceProxy);
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_MEDIA_INTERFACE_PROXY_H_
