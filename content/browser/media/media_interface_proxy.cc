// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/media_interface_proxy.h"

#include <map>
#include <memory>
#include <string>
#include <tuple>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/no_destructor.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "base/unguessable_token.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "content/browser/media/cdm_storage_common.h"
#include "content/browser/renderer_host/render_frame_host_delegate.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/browser/media_service.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/service_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/cdm_info.h"
#include "content/public/common/content_client.h"
#include "media/base/cdm_context.h"
#include "media/cdm/cdm_type.h"
#include "media/media_buildflags.h"
#include "media/mojo/buildflags.h"
#include "media/mojo/mojom/frame_interface_factory.mojom.h"
#include "media/mojo/mojom/media_service.mojom.h"
#include "media/mojo/mojom/stable/stable_video_decoder.mojom.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "mojo/public/cpp/bindings/remote_set.h"

#if BUILDFLAG(ENABLE_MOJO_CDM)
#include "content/public/browser/browser_context.h"
#include "content/public/browser/provision_fetcher_impl.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/storage_partition.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#endif

#if BUILDFLAG(ENABLE_LIBRARY_CDMS)
#include "base/feature_list.h"
#include "base/metrics/histogram_macros.h"
#include "content/browser/media/cdm_storage_manager.h"
#include "content/browser/media/media_license_manager.h"
#include "media/base/key_system_names.h"
#include "media/mojo/mojom/cdm_service.mojom.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#endif  // BUILDFLAG(ENABLE_LIBRARY_CDMS)

#if BUILDFLAG(IS_WIN) || BUILDFLAG(ENABLE_LIBRARY_CDMS)
#include "content/browser/media/cdm_registry_impl.h"
#include "content/browser/media/service_factory.h"
#include "media/base/media_switches.h"
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(ENABLE_LIBRARY_CDMS)

#if BUILDFLAG(IS_WIN)
#include "content/browser/media/dcomp_surface_registry_broker.h"
#include "media/base/win/mf_feature_checks.h"
#include "media/cdm/win/media_foundation_cdm.h"
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_ANDROID)
#include "content/browser/media/android/media_player_renderer.h"
#include "content/browser/media/flinging_renderer.h"
#include "media/mojo/services/mojo_renderer_service.h"  // nogncheck
#endif

#if BUILDFLAG(ALLOW_OOP_VIDEO_DECODER)
#include "content/public/browser/stable_video_decoder_factory.h"
#include "media/base/media_switches.h"
#include "mojo/public/cpp/bindings/message.h"
#endif  // BUILDFLAG(ALLOW_OOP_VIDEO_DECODER)

namespace content {

namespace {

#if BUILDFLAG(ENABLE_LIBRARY_CDMS)

// The CDM name will be displayed as the process name in the Task Manager.
// Put a length limit and restrict to ASCII. Empty name is allowed, in which
// case the process name will be "media::mojom::CdmService".
bool IsValidCdmDisplayName(const std::string& cdm_name) {
  constexpr size_t kMaxCdmNameSize = 256;
  return cdm_name.size() <= kMaxCdmNameSize && base::IsStringASCII(cdm_name);
}

#endif  // BUILDFLAG(ENABLE_LIBRARY_CDMS)

#if BUILDFLAG(IS_CHROMEOS)
// These are reported to UMA server. Do not renumber or reuse values.
enum class CrosCdmType {
  kChromeCdm = 0,
  kPlatformCdm = 1,
  // Note: Only add new values immediately before this line.
  kMaxValue = kPlatformCdm,
};

void ReportCdmTypeUMA(CrosCdmType cdm_type) {
  UMA_HISTOGRAM_ENUMERATION("Media.EME.CrosCdmType", cdm_type);
}
#endif  // BUILDFLAG(IS_CHROMEOS)

// The amount of time to allow the secondary Media Service instance to idle
// before tearing it down. Only used if the Content embedder defines how to
// launch a secondary Media Service instance.
constexpr base::TimeDelta kSecondaryInstanceIdleTimeout = base::Seconds(5);

void MaybeLaunchSecondaryMediaService(
    mojo::Remote<media::mojom::MediaService>* remote) {
  *remote = GetContentClient()->browser()->RunSecondaryMediaService();
  if (*remote) {
    // If the embedder provides a secondary Media Service instance, it may run
    // out-of-process. Make sure we reset on disconnect to allow restart of
    // crashed instances, and reset on idle to allow for release of resources
    // when the service instance goes unused for a while.
    remote->reset_on_disconnect();
    remote->reset_on_idle_timeout(kSecondaryInstanceIdleTimeout);
  } else {
    // The embedder doesn't provide a secondary Media Service instance. Bind
    // permanently to a disconnected pipe which discards all calls.
    std::ignore = remote->BindNewPipeAndPassReceiver();
  }
}

// Returns a remote handle to the secondary Media Service instance, if the
// Content embedder defines how to create one. If not, this returns a non-null
// but non-functioning MediaService reference which discards all calls.
media::mojom::MediaService& GetSecondaryMediaService() {
  static base::NoDestructor<mojo::Remote<media::mojom::MediaService>> remote;
  if (!*remote)
    MaybeLaunchSecondaryMediaService(remote.get());
  return *remote->get();
}

class FrameInterfaceFactoryImpl : public media::mojom::FrameInterfaceFactory,
                                  public WebContentsObserver {
 public:
  FrameInterfaceFactoryImpl(RenderFrameHost* render_frame_host,
                            const media::CdmType& cdm_type)
      : WebContentsObserver(
            WebContents::FromRenderFrameHost(render_frame_host)),
        render_frame_host_(render_frame_host),
        cdm_type_(cdm_type) {}

  // media::mojom::FrameInterfaceFactory implementation:

  void CreateProvisionFetcher(
      mojo::PendingReceiver<media::mojom::ProvisionFetcher> receiver) override {
#if BUILDFLAG(ENABLE_MOJO_CDM)
    ProvisionFetcherImpl::Create(render_frame_host_->GetBrowserContext()
                                     ->GetDefaultStoragePartition()
                                     ->GetURLLoaderFactoryForBrowserProcess(),
                                 std::move(receiver));
#endif
  }

  void CreateCdmStorage(
      mojo::PendingReceiver<media::mojom::CdmStorage> receiver) override {
#if BUILDFLAG(ENABLE_LIBRARY_CDMS)
    if (cdm_type_.is_zero())
      return;

    auto storage_key =
        static_cast<RenderFrameHostImpl*>(render_frame_host_)->GetStorageKey();

    CdmStorageManager* cdm_storage_manager = static_cast<CdmStorageManager*>(
        render_frame_host_->GetStoragePartition()->GetCdmStorageDataModel());

    cdm_storage_manager->OpenCdmStorage(
        CdmStorageBindingContext(storage_key, cdm_type_), std::move(receiver));
#endif
  }

#if BUILDFLAG(IS_WIN)
  void RegisterMuteStateObserver(
      mojo::PendingRemote<media::mojom::MuteStateObserver> observer) override {
    auto remote_id = site_mute_observers_.Add(std::move(observer));
    // Initial notification on mute stage.
    site_mute_observers_.Get(remote_id)->OnMuteStateChange(
        WebContents::FromRenderFrameHost(render_frame_host_)->IsAudioMuted());
  }

  void CreateDCOMPSurfaceRegistry(
      mojo::PendingReceiver<media::mojom::DCOMPSurfaceRegistry> receiver)
      override {
    if (media::SupportMediaFoundationPlayback()) {
      // TODO(crbug.com/40191522): Pass IO task runner and remove the PostTask()
      // in DCOMPSurfaceRegistryBroker after bug fixed.
      mojo::MakeSelfOwnedReceiver(
          std::make_unique<DCOMPSurfaceRegistryBroker>(), std::move(receiver));
    }
  }
#endif  // BUILDFLAG(IS_WIN)

  void GetCdmOrigin(GetCdmOriginCallback callback) override {
    return std::move(callback).Run(
        render_frame_host_->GetLastCommittedOrigin());
  }

  void BindEmbedderReceiver(mojo::GenericPendingReceiver receiver) override {
    GetContentClient()->browser()->BindMediaServiceReceiver(
        render_frame_host_, std::move(receiver));
  }

#if BUILDFLAG(IS_WIN)
  // WebContentsObserver implementation:
  void DidUpdateAudioMutingState(bool muted) override {
    for (const auto& observer : site_mute_observers_)
      observer->OnMuteStateChange(muted);
  }
#endif  // BUILDFLAG(IS_WIN)

 private:
  const raw_ptr<RenderFrameHost> render_frame_host_;
  const media::CdmType cdm_type_;

#if BUILDFLAG(IS_WIN)
  mojo::RemoteSet<media::mojom::MuteStateObserver> site_mute_observers_;
#endif  // BUILDFLAG(IS_WIN)
};

}  // namespace

MediaInterfaceProxy::MediaInterfaceProxy(RenderFrameHost* render_frame_host)
    : DocumentUserData(render_frame_host) {
  DVLOG(1) << __func__;

  media::CdmType cdm_type;
#if BUILDFLAG(IS_CHROMEOS)
  // The CdmType passed in here is only used by the CDM obtained through the
  // |media_interface_factory_ptr_|.
  cdm_type = kChromeOsCdmType;
#endif

  auto frame_factory_getter = base::BindRepeating(
      &MediaInterfaceProxy::GetFrameServices, base::Unretained(this), cdm_type);
  media_interface_factory_ptr_ = std::make_unique<MediaInterfaceFactoryHolder>(
      base::BindRepeating(&GetMediaService), frame_factory_getter);
  secondary_interface_factory_ = std::make_unique<MediaInterfaceFactoryHolder>(
      base::BindRepeating(&GetSecondaryMediaService), frame_factory_getter);

  // |cdm_factory_map_| will be lazily connected in GetCdmFactory().
}

MediaInterfaceProxy::~MediaInterfaceProxy() {
  DVLOG(1) << __func__;
  DCHECK(thread_checker_.CalledOnValidThread());
}

void MediaInterfaceProxy::Bind(
    mojo::PendingReceiver<media::mojom::InterfaceFactory> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void MediaInterfaceProxy::CreateAudioDecoder(
    mojo::PendingReceiver<media::mojom::AudioDecoder> receiver) {
  DCHECK(thread_checker_.CalledOnValidThread());
  InterfaceFactory* factory = media_interface_factory_ptr_->Get();
  if (factory)
    factory->CreateAudioDecoder(std::move(receiver));
}

void MediaInterfaceProxy::CreateVideoDecoder(
    mojo::PendingReceiver<media::mojom::VideoDecoder> receiver,
    mojo::PendingRemote<media::stable::mojom::StableVideoDecoder>
        dst_video_decoder) {
  DCHECK(thread_checker_.CalledOnValidThread());
  // The browser process cannot act as a proxy for video decoding and clients
  // should not attempt to use it that way.
  DCHECK(!dst_video_decoder);

  InterfaceFactory* factory = media_interface_factory_ptr_->Get();
  if (!factory)
    return;

  mojo::PendingRemote<media::stable::mojom::StableVideoDecoder>
      oop_video_decoder;
#if BUILDFLAG(ALLOW_OOP_VIDEO_DECODER)
  switch (media::GetOutOfProcessVideoDecodingMode()) {
    case media::OOPVDMode::kEnabledWithGpuProcessAsProxy:
      render_frame_host().GetProcess()->CreateStableVideoDecoder(
          oop_video_decoder.InitWithNewPipeAndPassReceiver());
      break;
    case media::OOPVDMode::kEnabledWithoutGpuProcessAsProxy:
      // Well-behaved clients shouldn't call CreateVideoDecoder() in this OOP-VD
      // mode and MediaInterfaceProxy::CreateVideoDecoder() should always be
      // called during a message dispatch.
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
void MediaInterfaceProxy::CreateStableVideoDecoder(
    mojo::PendingReceiver<media::stable::mojom::StableVideoDecoder>
        video_decoder) {
  DCHECK(thread_checker_.CalledOnValidThread());
  switch (media::GetOutOfProcessVideoDecodingMode()) {
    case media::OOPVDMode::kEnabledWithGpuProcessAsProxy:
    case media::OOPVDMode::kDisabled:
      // Well-behaved clients shouldn't call CreateStableVideoDecoder() in this
      // OOP-VD mode and MediaInterfaceProxy::CreateStableVideoDecoder() should
      // always be called during a message dispatch.
      CHECK(mojo::IsInMessageDispatch());
      mojo::ReportBadMessage("CreateStableVideoDecoder() called unexpectedly");
      return;
    case media::OOPVDMode::kEnabledWithoutGpuProcessAsProxy:
      render_frame_host().GetProcess()->CreateStableVideoDecoder(
          std::move(video_decoder));
      break;
  }
}
#endif  // BUILDFLAG(ALLOW_OOP_VIDEO_DECODER)

void MediaInterfaceProxy::CreateAudioEncoder(
    mojo::PendingReceiver<media::mojom::AudioEncoder> receiver) {
  DCHECK(thread_checker_.CalledOnValidThread());
  InterfaceFactory* factory = media_interface_factory_ptr_->Get();
  if (factory)
    factory->CreateAudioEncoder(std::move(receiver));
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

  // CastRenderer is always hosted in the secondary Media Service instance.
  // This may not be running in some test environments (e.g.
  // content_browsertests) even though renderers may still request to bind it.
  InterfaceFactory* factory = secondary_interface_factory_->Get();
  if (factory)
    factory->CreateCastRenderer(overlay_plane_id, std::move(receiver));
}
#endif

#if BUILDFLAG(IS_ANDROID)
void MediaInterfaceProxy::CreateFlingingRenderer(
    const std::string& presentation_id,
    mojo::PendingRemote<media::mojom::FlingingRendererClientExtension>
        client_extension,
    mojo::PendingReceiver<media::mojom::Renderer> receiver) {
  DCHECK(thread_checker_.CalledOnValidThread());

  std::unique_ptr<FlingingRenderer> flinging_renderer =
      FlingingRenderer::Create(&render_frame_host(), presentation_id,
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
          render_frame_host().GetProcess()->GetID(),
          render_frame_host().GetRoutingID(),
          WebContents::FromRenderFrameHost(&render_frame_host()),
          std::move(renderer_extension_receiver),
          std::move(client_extension_remote)),
      std::move(receiver));
}
#endif

#if BUILDFLAG(IS_WIN)
void MediaInterfaceProxy::CreateMediaFoundationRenderer(
    mojo::PendingRemote<media::mojom::MediaLog> media_log_remote,
    mojo::PendingReceiver<media::mojom::Renderer> receiver,
    mojo::PendingReceiver<media::mojom::MediaFoundationRendererExtension>
        renderer_extension_receiver,
    mojo::PendingRemote<media::mojom::MediaFoundationRendererClientExtension>
        client_extension_remote) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DVLOG(1) << __func__ << ": this=" << this;

  // For protected playback, the service should have already been initialized
  // with a CDM path in CreateCdm().
  auto* factory = GetMediaFoundationServiceInterfaceFactory(base::FilePath());
  if (factory) {
    factory->CreateMediaFoundationRenderer(
        std::move(media_log_remote), std::move(receiver),
        std::move(renderer_extension_receiver),
        std::move(client_extension_remote));
  }
}
#endif  // BUILDFLAG(IS_WIN)

void MediaInterfaceProxy::CreateCdm(const media::CdmConfig& cdm_config,
                                    CreateCdmCallback create_cdm_cb) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DVLOG(1) << __func__ << ": cdm_config=" << cdm_config;

  // The remote process may drop the callback (e.g. in case of crash, or CDM
  // loading/initialization failure). Doing it here instead of in the renderer
  // process because the browser is trusted.
  auto callback = mojo::WrapCallbackWithDefaultInvokeIfNotRun(
      std::move(create_cdm_cb), mojo::NullRemote(), nullptr,
      media::CreateCdmStatus::kDisconnectionError);

  // Handle `use_hw_secure_codecs` cases first.
#if BUILDFLAG(USE_CHROMEOS_PROTECTED_MEDIA)
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  bool enable_cdm_factory_daemon =
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kLacrosUseChromeosProtectedMedia);
#else   // BUILDFLAG(IS_CHROMEOS_LACROS)
  bool enable_cdm_factory_daemon = true;
#endif  // else BUILDFLAG(IS_CHROMEOS_LACROS)
#if defined(ARCH_CPU_ARM_FAMILY)
  if (!base::FeatureList::IsEnabled(media::kEnableArmHwdrm)) {
    enable_cdm_factory_daemon = false;
  }
#endif  // defined(ARCH_CPU_ARM_FAMILY)
  if (enable_cdm_factory_daemon && cdm_config.use_hw_secure_codecs &&
      cdm_config.allow_distinctive_identifier) {
    auto* factory = media_interface_factory_ptr_->Get();
    if (factory) {
      // We need to intercept the callback in this case so we can fallback to
      // the library CDM in case of failure.
      factory->CreateCdm(
          cdm_config, base::BindOnce(&MediaInterfaceProxy::OnChromeOsCdmCreated,
                                     weak_factory_.GetWeakPtr(), cdm_config,
                                     std::move(callback)));
      return;
    }
  }
  // Fallback to use library CDM below.
  ReportCdmTypeUMA(CrosCdmType::kChromeCdm);
#elif BUILDFLAG(IS_WIN)
  if (ShouldUseMediaFoundationServiceForCdm(cdm_config)) {
    if (!cdm_config.allow_distinctive_identifier ||
        !cdm_config.allow_persistent_state) {
      DVLOG(2) << "MediaFoundationService requires both distinctive identifier "
                  "and persistent state";
      std::move(callback).Run(mojo::NullRemote(), nullptr,
                              media::CreateCdmStatus::kInvalidCdmConfig);
      return;
    }

    auto cdm_info = CdmRegistryImpl::GetInstance()->GetCdmInfo(
        cdm_config.key_system, CdmInfo::Robustness::kHardwareSecure);
    if (cdm_info) {
      DVLOG(2) << "Get MediaFoundationService with CDM path " << cdm_info->path;
      auto* factory = GetMediaFoundationServiceInterfaceFactory(cdm_info->path);
      if (factory) {
        factory->CreateCdm(cdm_config, std::move(callback));
        return;
      }
    }
  }
  // Fallback to use library CDM below.
#endif  // BUILDFLAG(USE_CHROMEOS_PROTECTED_MEDIA)

#if BUILDFLAG(ENABLE_LIBRARY_CDMS)
  // Fallback to use CdmFactory even if `use_hw_secure_codecs` is true.
  auto* factory = GetCdmFactory(cdm_config.key_system);
#elif BUILDFLAG(ENABLE_CAST_RENDERER)
  // CDM service lives together with renderer service if cast renderer is
  // enabled, because cast renderer creates its own audio/video decoder. Note
  // that in content_browsertests (and Content Shell in general) we don't have
  // an a cast renderer and this interface will be unbound.
  auto* factory = secondary_interface_factory_->Get();
#else
  // CDM service lives together with audio/video decoder service.
  auto* factory = media_interface_factory_ptr_->Get();
#endif  // BUILDFLAG(ENABLE_LIBRARY_CDMS)

  if (!factory) {
    std::move(callback).Run(mojo::NullRemote(), nullptr,
                            media::CreateCdmStatus::kCdmFactoryCreationFailed);
    return;
  }

  factory->CreateCdm(cdm_config, std::move(callback));
}

mojo::PendingRemote<media::mojom::FrameInterfaceFactory>
MediaInterfaceProxy::GetFrameServices(const media::CdmType& cdm_type) {
  mojo::PendingRemote<media::mojom::FrameInterfaceFactory> factory;
  frame_factories_.Add(
      std::make_unique<FrameInterfaceFactoryImpl>(
          static_cast<RenderFrameHostImpl*>(&render_frame_host()), cdm_type),
      factory.InitWithNewPipeAndPassReceiver());
  return factory;
}

#if BUILDFLAG(IS_WIN)
media::mojom::InterfaceFactory*
MediaInterfaceProxy::GetMediaFoundationServiceInterfaceFactory(
    const base::FilePath& cdm_path) {
  DVLOG(3) << __func__ << ": this=" << this << ", cdm_path=" << cdm_path;
  DCHECK(thread_checker_.CalledOnValidThread());

  // TODO(xhwang): Also check protected media identifier content setting.
  if (!media::SupportMediaFoundationPlayback()) {
    DLOG(ERROR) << "Media foundation encrypted or clear playback not supported";
    return nullptr;
  }

  if (!mf_interface_factory_remote_)
    ConnectToMediaFoundationService(cdm_path);

  return mf_interface_factory_remote_.get();
}

void MediaInterfaceProxy::ConnectToMediaFoundationService(
    const base::FilePath& cdm_path) {
  DVLOG(1) << __func__ << ": this=" << this << ", cdm_path=" << cdm_path;
  DCHECK(!mf_interface_factory_remote_);

  auto& mf_service = GetMediaFoundationService(
      render_frame_host().GetBrowserContext(),
      render_frame_host().GetSiteInstance()->GetSiteURL(), cdm_path);

  // Passing an empty CdmType as MediaFoundation-based CDMs don't use CdmStorage
  // currently.
  // TODO(crbug.com/40779490): This works but is a bit hacky. CdmType is used
  // for both CDM-process-isolation and storage isolation. We probably still
  // want to have the information on whether we want to use CdmStorage in CDM
  // registration and populate that info here.
  mf_service.CreateInterfaceFactory(
      mf_interface_factory_remote_.BindNewPipeAndPassReceiver(),
      GetFrameServices(media::CdmType()));
  // Handle unexpected mojo pipe disconnection such as MediaFoundationService
  // process crashed or killed in the browser task manager.
  mf_interface_factory_remote_.reset_on_disconnect();
}

bool MediaInterfaceProxy::ShouldUseMediaFoundationServiceForCdm(
    const media::CdmConfig& cdm_config) {
  DVLOG(1) << __func__ << ": this=" << this << ", cdm_config=" << cdm_config;

  // TODO(xhwang): Refine this after we populate support info during EME
  // requestMediaKeySystemAccess() query, e.g. to check the `key_system` in
  // `cdm_config`.
  return cdm_config.use_hw_secure_codecs;
}
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(ENABLE_LIBRARY_CDMS)

media::mojom::CdmFactory* MediaInterfaceProxy::GetCdmFactory(
    const std::string& key_system) {
  DCHECK(thread_checker_.CalledOnValidThread());

  // CdmService only supports software secure codecs.
  auto cdm_info = CdmRegistryImpl::GetInstance()->GetCdmInfo(
      key_system, CdmInfo::Robustness::kSoftwareSecure);
  if (!cdm_info) {
    NOTREACHED_IN_MIGRATION() << "No valid CdmInfo for " << key_system;
    return nullptr;
  }
  if (cdm_info->path.empty()) {
    NOTREACHED_IN_MIGRATION() << "CDM path for " << key_system << " is empty";
    return nullptr;
  }
  if (!IsValidCdmDisplayName(cdm_info->name)) {
    NOTREACHED_IN_MIGRATION() << "Invalid CDM display name " << cdm_info->name;
    return nullptr;
  }

  auto& cdm_type = cdm_info->type;

  auto found = cdm_factory_map_.find(cdm_type);
  if (found != cdm_factory_map_.end())
    return found->second.get();

  return ConnectToCdmService(*cdm_info);
}

media::mojom::CdmFactory* MediaInterfaceProxy::ConnectToCdmService(
    const CdmInfo& cdm_info) {
  DVLOG(1) << __func__ << ": cdm_name = " << cdm_info.name;

  DCHECK(!cdm_factory_map_.count(cdm_info.type));

  auto* browser_context = render_frame_host().GetBrowserContext();
  auto& site = render_frame_host().GetSiteInstance()->GetSiteURL();
  auto& cdm_service = GetCdmService(browser_context, site, cdm_info);

  mojo::Remote<media::mojom::CdmFactory> cdm_factory_remote;
  cdm_service.CreateCdmFactory(cdm_factory_remote.BindNewPipeAndPassReceiver(),
                               GetFrameServices(cdm_info.type));
  cdm_factory_remote.set_disconnect_handler(
      base::BindOnce(&MediaInterfaceProxy::OnCdmServiceConnectionError,
                     base::Unretained(this), cdm_info.type));

  auto* cdm_factory = cdm_factory_remote.get();
  cdm_factory_map_.emplace(cdm_info.type, std::move(cdm_factory_remote));
  return cdm_factory;
}

void MediaInterfaceProxy::OnCdmServiceConnectionError(
    const media::CdmType& cdm_type) {
  DVLOG(1) << __func__;
  DCHECK(thread_checker_.CalledOnValidThread());

  DCHECK(cdm_factory_map_.count(cdm_type));
  cdm_factory_map_.erase(cdm_type);
}
#endif  // BUILDFLAG(ENABLE_LIBRARY_CDMS)

#if BUILDFLAG(IS_CHROMEOS)
void MediaInterfaceProxy::OnChromeOsCdmCreated(
    const media::CdmConfig& cdm_config,
    CreateCdmCallback callback,
    mojo::PendingRemote<media::mojom::ContentDecryptionModule> receiver,
    media::mojom::CdmContextPtr cdm_context,
    media::CreateCdmStatus status) {
  if (receiver) {
    ReportCdmTypeUMA(CrosCdmType::kPlatformCdm);
    // Success case, just pass it back through the callback.
    std::move(callback).Run(std::move(receiver), std::move(cdm_context),
                            status);
    return;
  }

  // We failed creating a CDM with the Chrome OS daemon, fallback to the library
  // CDM interface.
  VLOG(1) << "Failed creating Chrome OS CDM, will use library CDM";
  auto* factory = GetCdmFactory(cdm_config.key_system);
  if (!factory) {
    std::move(callback).Run(mojo::NullRemote(), nullptr,
                            media::CreateCdmStatus::kCdmFactoryCreationFailed);
    return;
  }
  ReportCdmTypeUMA(CrosCdmType::kChromeCdm);
  factory->CreateCdm(cdm_config, std::move(callback));
}
#endif  // BUILDFLAG(IS_CHROMEOS)

DOCUMENT_USER_DATA_KEY_IMPL(MediaInterfaceProxy);

}  // namespace content
