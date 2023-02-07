// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/service_factory.h"

#include <map>
#include <string>
#include <utility>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/threading/sequence_local_storage_slot.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/service_process_host.h"
#include "content/public/common/content_client.h"
#include "media/base/cdm_context.h"
#include "media/base/media_switches.h"
#include "media/cdm/cdm_type.h"
#include "media/media_buildflags.h"
#include "media/mojo/mojom/cdm_service.mojom.h"
#include "mojo/public/cpp/bindings/remote_set.h"

#if BUILDFLAG(IS_MAC)
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "sandbox/mac/seatbelt_extension.h"
#endif  // BUILDFLAG(IS_MAC)

#if BUILDFLAG(IS_WIN)
#include "content/public/browser/gpu_data_manager.h"
#include "content/public/browser/gpu_data_manager_observer.h"
#include "media/mojo/mojom/media_foundation_service.mojom.h"
#endif  // BUILDFLAG(IS_WIN)

namespace content {

namespace {

#if BUILDFLAG(IS_MAC)
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

class SeatbeltExtensionTokenProviderImpl final
    : public media::mojom::SeatbeltExtensionTokenProvider {
 public:
  explicit SeatbeltExtensionTokenProviderImpl(const base::FilePath& cdm_path)
      : cdm_path_(cdm_path) {}
  SeatbeltExtensionTokenProviderImpl(
      const SeatbeltExtensionTokenProviderImpl&) = delete;
  SeatbeltExtensionTokenProviderImpl operator=(
      const SeatbeltExtensionTokenProviderImpl&) = delete;
  ~SeatbeltExtensionTokenProviderImpl() override = default;

  void GetTokens(GetTokensCallback callback) override {
    DVLOG(1) << __func__;

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
};
#endif  // BUILDFLAG(IS_MAC)

#if BUILDFLAG(IS_WIN)
// A singleton running in the browser process to notify (multiple) service
// processes on GpuInfo updates.
class GpuInfoMonitor : public GpuDataManagerObserver {
 public:
  static GpuInfoMonitor* GetInstance() {
    static GpuInfoMonitor* instance = new GpuInfoMonitor();
    return instance;
  }

  GpuInfoMonitor() { GpuDataManager::GetInstance()->AddObserver(this); }

  void RegisterGpuInfoObserver(
      mojo::PendingRemote<media::mojom::GpuInfoObserver> observer) {
    auto observer_id = gpu_info_observers_.Add(std::move(observer));
    // Notify upon registration in case there's a GPUInfo change between
    // `InitializeBroker()` and when this observer is registered.
    gpu_info_observers_.Get(observer_id)
        ->OnGpuInfoUpdate(GpuDataManager::GetInstance()->GetGPUInfo());
  }

  // GpuDataManagerObserver:
  void OnGpuInfoUpdate() override {
    for (const auto& observer : gpu_info_observers_) {
      observer->OnGpuInfoUpdate(GpuDataManager::GetInstance()->GetGPUInfo());
    }
  }

 private:
  mojo::RemoteSet<media::mojom::GpuInfoObserver> gpu_info_observers_;
};

void RegisterGpuInfoObserver(
    mojo::PendingRemote<media::mojom::GpuInfoObserver> observer) {
  GpuInfoMonitor::GetInstance()->RegisterGpuInfoObserver(std::move(observer));
}
#endif  // BUILDFLAG(IS_WIN)

// How long an instance of the service is allowed to sit idle before we
// disconnect and effectively kill it.
constexpr auto kServiceIdleTimeout = base::Seconds(5);

// Services are keyed on CDM type, user profile and site URL. Note that site
// is not normal URL nor origin. See chrome/browser/site_isolation for details.
using ServiceKey = std::tuple<media::CdmType, const BrowserContext*, GURL>;

std::ostream& operator<<(std::ostream& os, const ServiceKey& key) {
  return os << "{" << std::get<0>(key).ToString() << ", " << std::get<1>(key)
            << ", " << std::get<2>(key) << "}";
}

template <typename T>
struct ServiceTraits {};

template <typename BrokerRemoteType>
void InitializeBroker(BrokerRemoteType& broker_remote) {}

template <>
struct ServiceTraits<media::mojom::CdmService> {
  using BrokerType = media::mojom::CdmServiceBroker;
};

#if BUILDFLAG(IS_WIN)
template <>
struct ServiceTraits<media::mojom::MediaFoundationService> {
  using BrokerType = media::mojom::MediaFoundationServiceBroker;
};

template <>
void InitializeBroker(
    mojo::Remote<media::mojom::MediaFoundationServiceBroker>& broker_remote) {
  broker_remote->UpdateGpuInfo(GpuDataManager::GetInstance()->GetGPUInfo(),
                               base::BindOnce(&RegisterGpuInfoObserver));
}
#endif  // BUILDFLAG(IS_WIN)

// A map hosts all service remotes, each of which corresponds to one service
// process. There should be only one instance of this class stored in
// base::SequenceLocalStorageSlot. See below.
template <typename T>
class ServiceMap {
 public:
  ServiceMap() = default;
  ~ServiceMap() = default;

  // Gets or creates a service remote. The returned remote might not be bound,
  // e.g. if it's newly created.
  auto& GetOrCreateRemote(const ServiceKey& key) { return remotes_[key]; }

  void EraseRemote(const ServiceKey& key) {
    DCHECK(remotes_.count(key));
    remotes_.erase(key);
  }

 private:
  using BrokerType = typename ServiceTraits<T>::BrokerType;

  // Keep the broker remote to keep the process alive. Keep the service remote
  // for reuse and for monitoring idle state (see below).
  std::map<ServiceKey, std::pair<mojo::Remote<BrokerType>, mojo::Remote<T>>>
      remotes_;
};

template <typename T>
ServiceMap<T>& GetServiceMap() {
  // NOTE: Sequence-local storage is used to limit the lifetime of the Remote
  // objects to that of the UI-thread sequence. This ensures the Remotes are
  // destroyed when the task environment is torn down and reinitialized, e.g.,
  // between unit tests.
  static base::SequenceLocalStorageSlot<ServiceMap<T>> slot;
  return slot.GetOrCreateValue();
}

// Erases the service instance identified by `key`.
template <typename T>
void EraseCdmService(const ServiceKey& key) {
  DVLOG(2) << __func__ << ": key=" << key;
  GetServiceMap<T>().EraseRemote(key);
}

// Gets an instance of the service for `cdm_type`, `browser_context` and `site`.
// Instances are started lazily as needed.
template <typename T>
T& GetService(const media::CdmType& cdm_type,
              BrowserContext* browser_context,
              const GURL& site,
              const std::string& service_name,
              const base::FilePath& cdm_path) {
  ServiceKey key;
  std::string display_name = service_name;

  if (base::FeatureList::IsEnabled(media::kCdmProcessSiteIsolation)) {
    key = {cdm_type, browser_context, site};
    auto site_display_name =
        GetContentClient()->browser()->GetSiteDisplayNameForCdmProcess(
            browser_context, site);
    if (!site_display_name.empty())
      display_name += " (" + site_display_name + ")";
  } else {
    key = {cdm_type, nullptr, GURL()};
  }
  DVLOG(2) << __func__ << ": key=" << key;

  auto& broker_service_pair = GetServiceMap<T>().GetOrCreateRemote(key);
  auto& broker_remote = broker_service_pair.first;
  auto& remote = broker_service_pair.second;
  if (!remote) {
    ServiceProcessHost::Options options;
    options.WithDisplayName(display_name);
    options.WithSite(site);
    ServiceProcessHost::Launch(broker_remote.BindNewPipeAndPassReceiver(),
                               options.Pass());

    // Initialize the broker if necessary.
    InitializeBroker(broker_remote);

#if BUILDFLAG(IS_MAC)
    mojo::PendingRemote<media::mojom::SeatbeltExtensionTokenProvider>
        token_provider_remote;
    mojo::MakeSelfOwnedReceiver(
        std::make_unique<SeatbeltExtensionTokenProviderImpl>(cdm_path),
        token_provider_remote.InitWithNewPipeAndPassReceiver());
    broker_remote->GetService(cdm_path, std::move(token_provider_remote),
                              remote.BindNewPipeAndPassReceiver());
#else
    broker_remote->GetService(cdm_path, remote.BindNewPipeAndPassReceiver());
#endif  // BUILDFLAG(IS_MAC)

    // The idle handler must be set on the `remote` because the `broker_remote`
    // will never idle when the `remote` is bound.
    remote.set_disconnect_handler(base::BindOnce(&EraseCdmService<T>, key));
    remote.set_idle_handler(kServiceIdleTimeout,
                            base::BindRepeating(EraseCdmService<T>, key));
  }

  return *remote.get();
}

}  // namespace

media::mojom::CdmService& GetCdmService(BrowserContext* browser_context,
                                        const GURL& site,
                                        const CdmInfo& cdm_info) {
  return GetService<media::mojom::CdmService>(
      cdm_info.type, browser_context, site, cdm_info.name, cdm_info.path);
}

#if BUILDFLAG(IS_WIN)
media::mojom::MediaFoundationService& GetMediaFoundationService(
    BrowserContext* browser_context,
    const GURL& site,
    const base::FilePath& cdm_path) {
  return GetService<media::mojom::MediaFoundationService>(
      media::CdmType(), browser_context, site, "Media Foundation Service",
      cdm_path);
}
#endif  // BUILDFLAG(IS_WIN)

}  // namespace content
