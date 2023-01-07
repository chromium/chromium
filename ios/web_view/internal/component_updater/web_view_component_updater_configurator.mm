// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/component_updater/web_view_component_updater_configurator.h"

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/version.h"
#include "components/component_updater/component_updater_command_line_config_policy.h"
#include "components/component_updater/configurator_impl.h"
#include "components/services/patch/in_process_file_patcher.h"
#include "components/services/unzip/in_process_unzipper.h"
#include "components/update_client/activity_data_service.h"
#include "components/update_client/crx_downloader_factory.h"
#include "components/update_client/net/network_chromium.h"
#include "components/update_client/patch/patch_impl.h"
#include "components/update_client/patcher.h"
#include "components/update_client/protocol_handler.h"
#include "components/update_client/unzip/unzip_impl.h"
#include "components/update_client/unzipper.h"
#include "components/update_client/update_query_params.h"
#include "ios/web_view/internal/app/application_context.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace ios_web_view {

namespace {

// A //ios/web_view specific configurator.
// See similar implementation at
// //ios/chrome/browser/component_updater/ios_component_updater_configurator.mm
class WebViewConfigurator : public update_client::Configurator {
 public:
  explicit WebViewConfigurator(const base::CommandLine* cmdline);

  // update_client::Configurator overrides.
  double InitialDelay() const override;
  int NextCheckDelay() const override;
  int OnDemandDelay() const override;
  int UpdateDelay() const override;
  std::vector<GURL> UpdateUrl() const override;
  std::vector<GURL> PingUrl() const override;
  std::string GetProdId() const override;
  base::Version GetBrowserVersion() const override;
  std::string GetChannel() const override;
  std::string GetLang() const override;
  std::string GetOSLongName() const override;
  base::flat_map<std::string, std::string> ExtraRequestParams() const override;
  std::string GetDownloadPreference() const override;
  scoped_refptr<update_client::NetworkFetcherFactory> GetNetworkFetcherFactory()
      override;
  scoped_refptr<update_client::CrxDownloaderFactory> GetCrxDownloaderFactory()
      override;
  scoped_refptr<update_client::UnzipperFactory> GetUnzipperFactory() override;
  scoped_refptr<update_client::PatcherFactory> GetPatcherFactory() override;
  bool EnabledDeltas() const override;
  bool EnabledBackgroundDownloader() const override;
  bool EnabledCupSigning() const override;
  PrefService* GetPrefService() const override;
  update_client::ActivityDataService* GetActivityDataService() const override;
  bool IsPerUserInstall() const override;
  std::unique_ptr<update_client::ProtocolHandlerFactory>
  GetProtocolHandlerFactory() const override;
  absl::optional<bool> IsMachineExternallyManaged() const override;
  update_client::UpdaterStateProvider GetUpdaterStateProvider() const override;

 private:
  friend class base::RefCountedThreadSafe<WebViewConfigurator>;

  component_updater::ConfiguratorImpl configurator_impl_;
  scoped_refptr<update_client::NetworkFetcherFactory> network_fetcher_factory_;
  scoped_refptr<update_client::CrxDownloaderFactory> crx_downloader_factory_;
  scoped_refptr<update_client::UnzipperFactory> unzip_factory_;
  scoped_refptr<update_client::PatcherFactory> patch_factory_;

  ~WebViewConfigurator() override = default;
};

// Allows the component updater to use non-encrypted communication with the
// update backend. The security of the update checks is enforced using
// a custom message signing protocol and it does not depend on using HTTPS.
WebViewConfigurator::WebViewConfigurator(const base::CommandLine* cmdline)
    : configurator_impl_(
          component_updater::ComponentUpdaterCommandLineConfigPolicy(cmdline),
          /*require_encryption=*/false) {}

double WebViewConfigurator::InitialDelay() const {
  return configurator_impl_.InitialDelay();
}

int WebViewConfigurator::NextCheckDelay() const {
  return configurator_impl_.NextCheckDelay();
}

int WebViewConfigurator::OnDemandDelay() const {
  return configurator_impl_.OnDemandDelay();
}

int WebViewConfigurator::UpdateDelay() const {
  return configurator_impl_.UpdateDelay();
}

std::vector<GURL> WebViewConfigurator::UpdateUrl() const {
  return configurator_impl_.UpdateUrl();
}

std::vector<GURL> WebViewConfigurator::PingUrl() const {
  return configurator_impl_.PingUrl();
}

std::string WebViewConfigurator::GetProdId() const {
  return update_client::UpdateQueryParams::GetProdIdString(
      update_client::UpdateQueryParams::ProdId::WEBVIEW);
}

base::Version WebViewConfigurator::GetBrowserVersion() const {
  return configurator_impl_.GetBrowserVersion();
}

std::string WebViewConfigurator::GetChannel() const {
  // TODO(crbug.com/1299888): Pass proper channel depending on build type.
  return "stable";
}

std::string WebViewConfigurator::GetLang() const {
  return ApplicationContext::GetInstance()->GetApplicationLocale();
}

std::string WebViewConfigurator::GetOSLongName() const {
  return configurator_impl_.GetOSLongName();
}

base::flat_map<std::string, std::string>
WebViewConfigurator::ExtraRequestParams() const {
  return configurator_impl_.ExtraRequestParams();
}

std::string WebViewConfigurator::GetDownloadPreference() const {
  return configurator_impl_.GetDownloadPreference();
}

scoped_refptr<update_client::NetworkFetcherFactory>
WebViewConfigurator::GetNetworkFetcherFactory() {
  if (!network_fetcher_factory_) {
    network_fetcher_factory_ =
        base::MakeRefCounted<update_client::NetworkFetcherChromiumFactory>(
            ApplicationContext::GetInstance()->GetSharedURLLoaderFactory(),
            // Never send cookies for component update downloads.
            base::BindRepeating([](const GURL& url) { return false; }));
  }
  return network_fetcher_factory_;
}

scoped_refptr<update_client::CrxDownloaderFactory>
WebViewConfigurator::GetCrxDownloaderFactory() {
  if (!crx_downloader_factory_) {
    crx_downloader_factory_ =
        update_client::MakeCrxDownloaderFactory(GetNetworkFetcherFactory());
  }
  return crx_downloader_factory_;
}

scoped_refptr<update_client::UnzipperFactory>
WebViewConfigurator::GetUnzipperFactory() {
  if (!unzip_factory_) {
    unzip_factory_ = base::MakeRefCounted<update_client::UnzipChromiumFactory>(
        base::BindRepeating(&unzip::LaunchInProcessUnzipper));
  }
  return unzip_factory_;
}

scoped_refptr<update_client::PatcherFactory>
WebViewConfigurator::GetPatcherFactory() {
  if (!patch_factory_) {
    patch_factory_ = base::MakeRefCounted<update_client::PatchChromiumFactory>(
        base::BindRepeating(&patch::LaunchInProcessFilePatcher));
  }
  return patch_factory_;
}

bool WebViewConfigurator::EnabledDeltas() const {
  return configurator_impl_.EnabledDeltas();
}

bool WebViewConfigurator::EnabledBackgroundDownloader() const {
  return configurator_impl_.EnabledBackgroundDownloader();
}

bool WebViewConfigurator::EnabledCupSigning() const {
  return configurator_impl_.EnabledCupSigning();
}

PrefService* WebViewConfigurator::GetPrefService() const {
  return ApplicationContext::GetInstance()->GetLocalState();
}

update_client::ActivityDataService*
WebViewConfigurator::GetActivityDataService() const {
  return nullptr;
}

bool WebViewConfigurator::IsPerUserInstall() const {
  return true;
}

std::unique_ptr<update_client::ProtocolHandlerFactory>
WebViewConfigurator::GetProtocolHandlerFactory() const {
  return configurator_impl_.GetProtocolHandlerFactory();
}

absl::optional<bool> WebViewConfigurator::IsMachineExternallyManaged() const {
  return configurator_impl_.IsMachineExternallyManaged();
}

update_client::UpdaterStateProvider
WebViewConfigurator::GetUpdaterStateProvider() const {
  return configurator_impl_.GetUpdaterStateProvider();
}

}  // namespace

scoped_refptr<update_client::Configurator> MakeComponentUpdaterConfigurator(
    const base::CommandLine* cmdline) {
  return base::MakeRefCounted<WebViewConfigurator>(cmdline);
}

}  // namespace ios_web_view
