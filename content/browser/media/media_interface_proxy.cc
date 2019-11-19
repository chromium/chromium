// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/media_interface_proxy.h"

#include <memory>
#include <string>

#include "base/bind.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/strings/string_util.h"
#include "content/browser/frame_host/render_frame_host_delegate.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/system_connector.h"
#include "content/public/common/content_client.h"
#include "media/mojo/buildflags.h"
#include "media/mojo/mojom/constants.mojom.h"
#include "media/mojo/mojom/media_service.mojom.h"
#include "media/mojo/services/media_interface_provider.h"
#include "services/service_manager/public/cpp/connector.h"

#if BUILDFLAG(ENABLE_MOJO_CDM)
#include "content/public/browser/browser_context.h"
#include "content/public/browser/provision_fetcher_impl.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/storage_partition.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#endif

#if BUILDFLAG(ENABLE_LIBRARY_CDMS)
#include "content/browser/media/cdm_storage_impl.h"
#include "content/browser/media/key_system_support_impl.h"
#include "content/public/common/cdm_info.h"
#include "media/base/key_system_names.h"
#include "media/mojo/mojom/cdm_service.mojom.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#if defined(OS_MACOSX)
#include "sandbox/mac/seatbelt_extension.h"
#endif  // defined(OS_MACOSX)
#endif  // BUILDFLAG(ENABLE_LIBRARY_CDMS)

#if BUILDFLAG(ENABLE_CDM_PROXY)
#include "media/mojo/mojom/cdm_proxy.mojom.h"
#endif  // BUILDFLAG(ENABLE_CDM_PROXY)

#if defined(OS_ANDROID)
#include "content/browser/media/android/media_player_renderer.h"
#include "content/browser/media/flinging_renderer.h"
#include "media/mojo/services/mojo_renderer_service.h"  // nogncheck
#endif

namespace content {

#if BUILDFLAG(ENABLE_LIBRARY_CDMS) && defined(OS_MACOSX)

namespace {

#if BUILDFLAG(ENABLE_CDM_HOST_VERIFICATION)
// TODO(xhwang): Move this to a common place.
const base::FilePath::CharType kSignatureFileExtension[] =
    FILE_PATH_LITERAL(".sig");

// Returns the signature file path given the |file_path|. This function should
// only be used when the signature file and the file are located in the same
// directory, which is the case for the CDM and CDM adapter.
base::FilePath GetSigFilePath(const base::FilePath& file_path) {
  return file_path.AddExtension(kSignatureFileExtension);
}
#endif  // BUILDFLAG(ENABLE_CDM_HOST_VERIFICATION)

class SeatbeltExtensionTokenProviderImpl
    : public media::mojom::SeatbeltExtensionTokenProvider {
 public:
  explicit SeatbeltExtensionTokenProviderImpl(const base::FilePath& cdm_path)
      : cdm_path_(cdm_path) {}
  void GetTokens(GetTokensCallback callback) final {
    std::vector<sandbox::SeatbeltExtensionToken> tokens;

    // Allow the CDM to be loaded in the CDM service process.
    auto cdm_token = sandbox::SeatbeltExtension::Issue(
        sandbox::SeatbeltExtension::FILE_READ, cdm_path_.value());
    if (cdm_token) {
      tokens.push_back(std::move(*cdm_token));
    } else {
      std::move(callback).Run({});
      return;
    }

#if BUILDFLAG(ENABLE_CDM_HOST_VERIFICATION)
    // If CDM host verification is enabled, also allow to open the CDM signature
    // file.
    auto cdm_sig_token =
        sandbox::SeatbeltExtension::Issue(sandbox::SeatbeltExtension::FILE_READ,
                                          GetSigFilePath(cdm_path_).value());
    if (cdm_sig_token) {
      tokens.push_back(std::move(*cdm_sig_token));
    } else {
      std::move(callback).Run({});
      return;
    }
#endif  // BUILDFLAG(ENABLE_CDM_HOST_VERIFICATION)

    std::move(callback).Run(std::move(tokens));
  }

 private:
  base::FilePath cdm_path_;

  DISALLOW_COPY_AND_ASSIGN(SeatbeltExtensionTokenProviderImpl);
};

}  // namespace

#endif  // BUILDFLAG(ENABLE_LIBRARY_CDMS) && defined(OS_MACOSX)

MediaInterfaceProxy::MediaInterfaceProxy(
    RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<media::mojom::InterfaceFactory> receiver,
    base::OnceClosure error_handler)
    : render_frame_host_(render_frame_host),
      receiver_(this, std::move(receiver)) {
  DVLOG(1) << __func__;
  DCHECK(render_frame_host_);
  DCHECK(!error_handler.is_null());

  auto create_interface_provider_cb =
      base::BindRepeating(&MediaInterfaceProxy::GetFrameServices,
                          base::Unretained(this), base::Token(), std::string());
  media_interface_factory_ptr_ = std::make_unique<MediaInterfaceFactoryHolder>(
      media::mojom::kMediaServiceName, create_interface_provider_cb);

#if BUILDFLAG(ENABLE_CAST_RENDERER)
  media_renderer_interface_factory_ptr_ =
      std::make_unique<MediaInterfaceFactoryHolder>(
          media::mojom::kMediaRendererServiceName,
          std::move(create_interface_provider_cb));
#endif  // BUILDFLAG(ENABLE_CAST_RENDERER)

  receiver_.set_disconnect_handler(std::move(error_handler));

  // |cdm_factory_map_| will be lazily connected in GetCdmFactory().
}

MediaInterfaceProxy::~MediaInterfaceProxy() {
  DVLOG(1) << __func__;
  DCHECK(thread_checker_.CalledOnValidThread());
}

void MediaInterfaceProxy::CreateAudioDecoder(
    mojo::PendingReceiver<media::mojom::AudioDecoder> receiver) {
  DCHECK(thread_checker_.CalledOnValidThread());
  InterfaceFactory* factory = media_interface_factory_ptr_->Get();
  if (factory)
    factory->CreateAudioDecoder(std::move(receiver));
}

void MediaInterfaceProxy::CreateVideoDecoder(
    mojo::PendingReceiver<media::mojom::VideoDecoder> receiver) {
  DCHECK(thread_checker_.CalledOnValidThread());
  InterfaceFactory* factory = media_interface_factory_ptr_->Get();
  if (factory)
    factory->CreateVideoDecoder(std::move(receiver));
}

void MediaInterfaceProxy::CreateDefaultRenderer(
    const std::string& audio_device_id,
    mojo::PendingReceiver<media::mojom::Renderer> receiver) {
  DCHECK(thread_checker_.CalledOnValidThread());

  InterfaceFactory* factory = media_interface_factory_ptr_->Get();
  if (factory)
    factory->CreateDefaultRenderer(audio_device_id, std::move(receiver));
}

#if BUILDFLAG(ENABLE_CAST_RENDERER)
void MediaInterfaceProxy::CreateCastRenderer(
    const base::UnguessableToken& overlay_plane_id,
    mojo::PendingReceiver<media::mojom::Renderer> receiver) {
  DCHECK(thread_checker_.CalledOnValidThread());

  // CastRenderer is always hosted in "media_renderer" service.
  InterfaceFactory* factory = media_renderer_interface_factory_ptr_->Get();
  if (factory)
    factory->CreateCastRenderer(overlay_plane_id, std::move(receiver));
}
#endif

#if defined(OS_ANDROID)
void MediaInterfaceProxy::CreateFlingingRenderer(
    const std::string& presentation_id,
    mojo::PendingRemote<media::mojom::FlingingRendererClientExtension>
        client_extension,
    mojo::PendingReceiver<media::mojom::Renderer> receiver) {
  DCHECK(thread_checker_.CalledOnValidThread());

  std::unique_ptr<FlingingRenderer> flinging_renderer =
      FlingingRenderer::Create(render_frame_host_, presentation_id,
                               std::move(client_extension));

  if (!flinging_renderer)
    return;

  media::MojoRendererService::Create(nullptr, std::move(flinging_renderer),
                                     std::move(receiver));
}

void MediaInterfaceProxy::CreateMediaPlayerRenderer(
    mojo::PendingRemote<media::mojom::MediaPlayerRendererClientExtension>
        client_extension_remote,
    mojo::PendingReceiver<media::mojom::Renderer> receiver,
    mojo::PendingReceiver<media::mojom::MediaPlayerRendererExtension>
        renderer_extension_receiver) {
  DCHECK(thread_checker_.CalledOnValidThread());

  media::MojoRendererService::Create(
      nullptr,
      std::make_unique<MediaPlayerRenderer>(
          render_frame_host_->GetProcess()->GetID(),
          render_frame_host_->GetRoutingID(),
          static_cast<RenderFrameHostImpl*>(render_frame_host_)
              ->delegate()
              ->GetAsWebContents(),
          std::move(renderer_extension_receiver),
          std::move(client_extension_remote)),
      std::move(receiver));
}
#endif

void MediaInterfaceProxy::CreateCdm(
    const std::string& key_system,
    mojo::PendingReceiver<media::mojom::ContentDecryptionModule> receiver) {
  DCHECK(thread_checker_.CalledOnValidThread());
#if BUILDFLAG(ENABLE_LIBRARY_CDMS)
  auto* factory = GetCdmFactory(key_system);
#elif BUILDFLAG(ENABLE_CAST_RENDERER)
  // CDM service lives together with renderer service if cast renderer is
  // enabled, because cast renderer creates its own audio/video decoder.
  auto* factory = media_renderer_interface_factory_ptr_->Get();
#else
  // CDM service lives together with audio/video decoder service.
  auto* factory = media_interface_factory_ptr_->Get();
#endif

  if (factory)
    factory->CreateCdm(key_system, std::move(receiver));
}

void MediaInterfaceProxy::CreateDecryptor(
    int cdm_id,
    mojo::PendingReceiver<media::mojom::Decryptor> receiver) {
  InterfaceFactory* factory = media_interface_factory_ptr_->Get();
  if (factory)
    factory->CreateDecryptor(cdm_id, std::move(receiver));
}

#if BUILDFLAG(ENABLE_CDM_PROXY)
void MediaInterfaceProxy::CreateCdmProxy(
    const base::Token& cdm_guid,
    mojo::PendingReceiver<media::mojom::CdmProxy> receiver) {
  NOTREACHED() << "The CdmProxy should only be created by a CDM.";
}
#endif  // BUILDFLAG(ENABLE_CDM_PROXY)

mojo::PendingRemote<service_manager::mojom::InterfaceProvider>
MediaInterfaceProxy::GetFrameServices(const base::Token& cdm_guid,
                                      const std::string& cdm_file_system_id) {
  // Register frame services.
  mojo::PendingRemote<service_manager::mojom::InterfaceProvider> interfaces;

  // TODO(xhwang): Replace this InterfaceProvider with a dedicated media host
  // interface. See http://crbug.com/660573
  auto provider = std::make_unique<media::MediaInterfaceProvider>(
      interfaces.InitWithNewPipeAndPassReceiver());

#if BUILDFLAG(ENABLE_MOJO_CDM)
  // TODO(slan): Wrap these into a RenderFrame specific ProvisionFetcher impl.
  provider->registry()->AddInterface(base::BindRepeating(
      &ProvisionFetcherImpl::Create,
      base::RetainedRef(
          BrowserContext::GetDefaultStoragePartition(
              render_frame_host_->GetProcess()->GetBrowserContext())
              ->GetURLLoaderFactoryForBrowserProcess())));

#if BUILDFLAG(ENABLE_LIBRARY_CDMS)
  // Only provide CdmStorageImpl when we have a valid |cdm_file_system_id|,
  // which is currently only set for the CdmService (not the MediaService).
  if (!cdm_file_system_id.empty()) {
    provider->registry()->AddInterface(base::BindRepeating(
        &CdmStorageImpl::Create, render_frame_host_, cdm_file_system_id));
  }

#if BUILDFLAG(ENABLE_CDM_PROXY)
  provider->registry()->AddInterface(
      base::BindRepeating(&MediaInterfaceProxy::CreateCdmProxyInternal,
                          base::Unretained(this), cdm_guid));
#endif  // BUILDFLAG(ENABLE_CDM_PROXY)
#endif  // BUILDFLAG(ENABLE_LIBRARY_CDMS)
#endif  // BUILDFLAG(ENABLE_MOJO_CDM)

  GetContentClient()->browser()->ExposeInterfacesToMediaService(
      provider->registry(), render_frame_host_);

  media_registries_.push_back(std::move(provider));

  return interfaces;
}

#if BUILDFLAG(ENABLE_LIBRARY_CDMS)

media::mojom::CdmFactory* MediaInterfaceProxy::GetCdmFactory(
    const std::string& key_system) {
  DCHECK(thread_checker_.CalledOnValidThread());

  base::Token cdm_guid;
  base::FilePath cdm_path;
  std::string cdm_file_system_id;

  std::unique_ptr<CdmInfo> cdm_info =
      KeySystemSupportImpl::GetCdmInfoForKeySystem(key_system);
  if (!cdm_info) {
    NOTREACHED() << "No valid CdmInfo for " << key_system;
    return nullptr;
  }
  if (cdm_info->path.empty()) {
    NOTREACHED() << "CDM path for " << key_system << " is empty.";
    return nullptr;
  }
  if (!CdmStorageImpl::IsValidCdmFileSystemId(cdm_info->file_system_id)) {
    NOTREACHED() << "Invalid file system ID " << cdm_info->file_system_id;
    return nullptr;
  }
  cdm_guid = cdm_info->guid;
  cdm_path = cdm_info->path;
  cdm_file_system_id = cdm_info->file_system_id;

  auto found = cdm_factory_map_.find(cdm_guid);
  if (found != cdm_factory_map_.end())
    return found->second.get();

  return ConnectToCdmService(cdm_guid, cdm_path, cdm_file_system_id);
}

media::mojom::CdmFactory* MediaInterfaceProxy::ConnectToCdmService(
    const base::Token& cdm_guid,
    const base::FilePath& cdm_path,
    const std::string& cdm_file_system_id) {
  DVLOG(1) << __func__ << ": cdm_guid = " << cdm_guid.ToString();

  DCHECK(!cdm_factory_map_.count(cdm_guid));

  // TODO(slan): Use the BrowserContext Connector instead. See crbug.com/638950.
  mojo::Remote<media::mojom::CdmService> cdm_service;
  GetSystemConnector()->Connect(service_manager::ServiceFilter::ByNameWithId(
                                    media::mojom::kCdmServiceName, cdm_guid),
                                cdm_service.BindNewPipeAndPassReceiver());

#if defined(OS_MACOSX)
  // LoadCdm() should always be called before CreateInterfaceFactory().
  media::mojom::SeatbeltExtensionTokenProviderPtr token_provider_ptr;
  mojo::MakeStrongBinding(
      std::make_unique<SeatbeltExtensionTokenProviderImpl>(cdm_path),
      mojo::MakeRequest(&token_provider_ptr));

  cdm_service->LoadCdm(cdm_path, std::move(token_provider_ptr));
#else
  cdm_service->LoadCdm(cdm_path);
#endif  // defined(OS_MACOSX)

  mojo::Remote<media::mojom::CdmFactory> cdm_factory_remote;
  cdm_service->CreateCdmFactory(cdm_factory_remote.BindNewPipeAndPassReceiver(),
                                GetFrameServices(cdm_guid, cdm_file_system_id));
  cdm_factory_remote.set_disconnect_handler(
      base::BindOnce(&MediaInterfaceProxy::OnCdmServiceConnectionError,
                     base::Unretained(this), cdm_guid));

  auto* cdm_factory = cdm_factory_remote.get();
  cdm_factory_map_.emplace(cdm_guid, std::move(cdm_factory_remote));
  return cdm_factory;
}

void MediaInterfaceProxy::OnCdmServiceConnectionError(
    const base::Token& cdm_guid) {
  DVLOG(1) << __func__;
  DCHECK(thread_checker_.CalledOnValidThread());

  DCHECK(cdm_factory_map_.count(cdm_guid));
  cdm_factory_map_.erase(cdm_guid);
}

#if BUILDFLAG(ENABLE_CDM_PROXY)
void MediaInterfaceProxy::CreateCdmProxyInternal(
    const base::Token& cdm_guid,
    mojo::PendingReceiver<media::mojom::CdmProxy> receiver) {
  DVLOG(1) << __func__;
  DCHECK(thread_checker_.CalledOnValidThread());

  InterfaceFactory* factory = media_interface_factory_ptr_->Get();
  if (factory)
    factory->CreateCdmProxy(cdm_guid, std::move(receiver));
}
#endif  // BUILDFLAG(ENABLE_CDM_PROXY)

#endif  // BUILDFLAG(ENABLE_LIBRARY_CDMS)

}  // namespace content
