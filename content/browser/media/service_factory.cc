// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/service_factory.h"

#include <map>
#include <string>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/no_destructor.h"
#include "base/threading/sequence_local_storage_slot.h"
#include "base/time/time.h"
#include "content/browser/service_sandbox_type.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/service_process_host.h"
#include "content/public/common/content_client.h"
#include "media/base/cdm_context.h"
#include "media/base/media_switches.h"
#include "media/media_buildflags.h"

namespace content {

namespace {

// How long an instance of the service is allowed to sit idle before we
// disconnect and effectively kill it.
constexpr auto kServiceIdleTimeout = base::TimeDelta::FromSeconds(5);

// Services are keyed on CDM type, user profile and site URL. Note that site
// is not normal URL nor origin. See chrome/browser/site_isolation for details.
using ServiceKey = std::tuple<base::Token, const BrowserContext*, GURL>;

std::ostream& operator<<(std::ostream& os, const ServiceKey& key) {
  return os << "{" << std::get<0>(key).ToString() << ", " << std::get<1>(key)
            << ", " << std::get<2>(key) << "}";
}

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
  std::map<ServiceKey, mojo::Remote<T>> remotes_;
};

template <typename T>
ServiceMap<T>& GetServiceMap() {
  // NOTE: Sequence-local storage is used to limit the lifetime of the Remote
  // objects to that of the UI-thread sequence. This ensures the Remotes are
  // destroyed when the task environment is torn down and reinitialized, e.g.,
  // between unit tests.
  static base::NoDestructor<base::SequenceLocalStorageSlot<ServiceMap<T>>> slot;
  return slot->GetOrCreateValue();
}

// Erases the service instance identified by `key`.
template <typename T>
void EraseCdmService(const ServiceKey& key) {
  DVLOG(2) << __func__ << ": key=" << key;
  GetServiceMap<T>().EraseRemote(key);
}

// Gets an instance of the service for `guid`, `browser_context` and `site`.
// Instances are started lazily as needed.
template <typename T>
T& GetService(const base::Token& guid,
              BrowserContext* browser_context,
              const GURL& site,
              const std::string& service_name) {
  ServiceKey key;
  std::string display_name = service_name;

  if (base::FeatureList::IsEnabled(media::kCdmProcessSiteIsolation)) {
    key = {guid, browser_context, site};
    auto site_display_name =
        GetContentClient()->browser()->GetSiteDisplayNameForCdmProcess(
            browser_context, site);
    if (!site_display_name.empty())
      display_name += " (" + site_display_name + ")";
  } else {
    key = {guid, nullptr, GURL()};
  }
  DVLOG(2) << __func__ << ": key=" << key;

  auto& remote = GetServiceMap<T>().GetOrCreateRemote(key);
  if (!remote) {
    ServiceProcessHost::Options options;
    options.WithDisplayName(display_name);
    ServiceProcessHost::Launch(remote.BindNewPipeAndPassReceiver(),
                               options.Pass());
    remote.set_disconnect_handler(base::BindOnce(&EraseCdmService<T>, key));
    remote.set_idle_handler(kServiceIdleTimeout,
                            base::BindRepeating(EraseCdmService<T>, key));
  }

  return *remote.get();
}

}  // namespace

media::mojom::CdmService& GetCdmService(const base::Token& guid,
                                        BrowserContext* browser_context,
                                        const GURL& site,
                                        const CdmInfo& cdm_info) {
  return GetService<media::mojom::CdmService>(guid, browser_context, site,
                                              cdm_info.name);
}

#if defined(OS_WIN)
media::mojom::MediaFoundationService& GetMediaFoundationService(
    BrowserContext* browser_context,
    const GURL& site) {
  return GetService<media::mojom::MediaFoundationService>(
      base::Token(), browser_context, site, "Media Foundation Service");
}
#endif  // defined(OS_WIN)

}  // namespace content
