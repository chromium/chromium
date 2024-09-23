// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/proxy_resolution/proxy_config_service.h"

#include <memory>

#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "net/proxy_resolution/proxy_config_with_annotation.h"

#if BUILDFLAG(IS_WIN)
#include "net/proxy_resolution/win/proxy_config_service_win.h"
#elif BUILDFLAG(IS_IOS)
#include "net/proxy_resolution/proxy_config_service_ios.h"
#elif BUILDFLAG(IS_MAC)
#include "net/proxy_resolution/proxy_config_service_mac.h"
#elif BUILDFLAG(IS_LINUX)
#include "net/proxy_resolution/proxy_config_service_linux.h"
#elif BUILDFLAG(IS_ANDROID)
#include "net/proxy_resolution/proxy_config_service_android.h"
#endif

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_LINUX)
#include "net/traffic_annotation/network_traffic_annotation.h"
#endif

namespace net {

namespace {
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_LINUX)
constexpr net::NetworkTrafficAnnotationTag kSystemProxyConfigTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("proxy_config_system", R"(
      semantics {
        sender: "Proxy Config"
        description:
          "Establishing a connection through a proxy server using system proxy "
          "settings."
        trigger:
          "Whenever a network request is made when the system proxy settings "
          "are used, and they indicate to use a proxy server."
        data:
          "Proxy configuration."
        destination: OTHER
        destination_other:
          "The proxy server specified in the configuration."
      }
      policy {
        cookies_allowed: NO
        setting:
          "User cannot override system proxy settings, but can change them "
          "through 'Advanced/System/Open proxy settings'."
        policy_exception_justification:
          "Using 'ProxySettings' policy can set Chrome to use specific "
          "proxy settings and avoid system proxy."
      })");
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
class UnsetProxyConfigService : public ProxyConfigService {
 public:
  UnsetProxyConfigService() = default;
  ~UnsetProxyConfigService() override = default;

  void AddObserver(Observer* observer) override {}
  void RemoveObserver(Observer* observer) override {}
  ConfigAvailability GetLatestProxyConfig(
      ProxyConfigWithAnnotation* config) override {
    return CONFIG_UNSET;
  }
};
#endif

// Config getter that always returns direct settings.
class ProxyConfigServiceDirect : public ProxyConfigService {
 public:
  // ProxyConfigService implementation:
  void AddObserver(Observer* observer) override {}
  void RemoveObserver(Observer* observer) override {}
  ConfigAvailability GetLatestProxyConfig(
      ProxyConfigWithAnnotation* config) override {
    *config = ProxyConfigWithAnnotation::CreateDirect();
    return CONFIG_VALID;
  }
};

}  // namespace

// static
std::unique_ptr<ProxyConfigService>
ProxyConfigService::CreateSystemProxyConfigService(
    scoped_refptr<base::SequencedTaskRunner> main_task_runner) {
#if BUILDFLAG(IS_WIN)
  return std::make_unique<ProxyConfigServiceWin>(
      kSystemProxyConfigTrafficAnnotation);
#elif BUILDFLAG(IS_IOS)
  return std::make_unique<ProxyConfigServiceIOS>(
      kSystemProxyConfigTrafficAnnotation);
#elif BUILDFLAG(IS_MAC)
  return std::make_unique<ProxyConfigServiceMac>(
      std::move(main_task_runner), kSystemProxyConfigTrafficAnnotation);
#elif BUILDFLAG(IS_CHROMEOS_ASH)
  LOG(ERROR) << "ProxyConfigService for ChromeOS should be created in "
             << "profile_io_data.cc::CreateProxyConfigService and this should "
             << "be used only for examples.";
  return std::make_unique<UnsetProxyConfigService>();
#elif BUILDFLAG(IS_LINUX)
  std::unique_ptr<ProxyConfigServiceLinux> linux_config_service(
      std::make_unique<ProxyConfigServiceLinux>());

  // Assume we got called on the thread that runs the default glib
  // main loop, so the current thread is where we should be running
  // gsettings calls from.
  scoped_refptr<base::SingleThreadTaskRunner> glib_thread_task_runner =
      base::SingleThreadTaskRunner::GetCurrentDefault();

  // Synchronously fetch the current proxy config (since we are running on
  // glib_default_loop). Additionally register for notifications (delivered in
  // either |glib_default_loop| or an internal sequenced task runner) to
  // keep us updated when the proxy config changes.
  linux_config_service->SetupAndFetchInitialConfig(
      glib_thread_task_runner, std::move(main_task_runner),
      kSystemProxyConfigTrafficAnnotation);

  return std::move(linux_config_service);
#elif BUILDFLAG(IS_ANDROID)
  return std::make_unique<ProxyConfigServiceAndroid>(
      std::move(main_task_runner),
      base::SingleThreadTaskRunner::GetCurrentDefault());
#elif BUILDFLAG(IS_FUCHSIA)
  // TODO(crbug.com/42050626): Implement a system proxy service for Fuchsia.
  return std::make_unique<ProxyConfigServiceDirect>();
#else
  LOG(WARNING) << "Failed to choose a system proxy settings fetcher "
                  "for this platform.";
  return std::make_unique<ProxyConfigServiceDirect>();
#endif
}

bool ProxyConfigService::UsesPolling() {
  return false;
}

}  // namespace net
