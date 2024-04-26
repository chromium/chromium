// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/component_updater/web_view_component_updater_configurator.h"

#import <stdint.h>

#import <memory>
#import <optional>
#import <string>
#import <vector>

#import "base/containers/flat_map.h"
#import "base/files/file_path.h"
#import "base/path_service.h"
#import "base/time/time.h"
#import "base/version.h"
#import "components/component_updater/component_updater_command_line_config_policy.h"
#import "components/component_updater/configurator_impl.h"
#import "components/services/patch/in_process_file_patcher.h"
#import "components/services/unzip/in_process_unzipper.h"
#import "components/update_client/activity_data_service.h"
#import "components/update_client/crx_downloader_factory.h"
#import "components/update_client/net/network_chromium.h"
#import "components/update_client/patch/patch_impl.h"
#import "components/update_client/patcher.h"
#import "components/update_client/persisted_data.h"
#import "components/update_client/protocol_handler.h"
#import "components/update_client/unzip/unzip_impl.h"
#import "components/update_client/unzipper.h"
#import "components/update_client/update_query_params.h"
#import "ios/web_view/internal/app/application_context.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"

namespace ios_web_view {

namespace {

// A //ios/web_view specific configurator.
// See similar implementation at
// //ios/chrome/browser/component_updater/model/ios_component_updater_configurator.mm
class WebViewConfigurator : public update_client::Configurator {
 public:
  explicit WebViewConfigurator(const base::CommandLine* cmdline);

  // update_client::Configurator overrides.
  base::TimeDelta InitialDelay() const override;
  base::TimeDelta NextCheckDelay() const override;
  base::TimeDelta OnDemandDelay() const override;
  base::TimeDelta UpdateDelay() const override;
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
  update_client::PersistedData* GetPersistedData() const override;
  bool IsPerUserInstall() const override;
  std::unique_ptr<update_client::ProtocolHandlerFactory>
  GetProtocolHandlerFactory() const override;
  std::optional<bool> IsMachineExternallyManaged() const override;
  update_client::UpdaterStateProvider GetUpdaterStateProvider() const override;
  std::optional<base::FilePath> GetCrxCachePath() const override;
  bool IsConnectionMetered() const override;

 private:
  friend class base::RefCountedThreadSafe<WebViewConfigurator>;

  component_updater::ConfiguratorImpl configurator_impl_;
  std::unique_ptr<update_client::PersistedData> persisted_data_;
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
          /*require_encryption=*/false),
      persisted_data_(update_client::CreatePersistedData(
          ApplicationContext::GetInstance()->GetLocalState(),
          nullptr)) {}

base::TimeDelta WebViewConfigurator::InitialDelay() const {
  return configurator_impl_.InitialDelay();
}

base::TimeDelta WebViewConfigurator::NextCheckDelay() const {
  return configurator_impl_.NextCheckDelay();
}

base::TimeDelta WebViewConfigurator::OnDemandDelay() const {
  return configurator_impl_.OnDemandDelay();
}

base::TimeDelta WebViewConfigurator::UpdateDelay() const {
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
  // TODO(crbug.com/40216038): Pass proper channel depending on build type.
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

update_client::PersistedData* WebViewConfigurator::GetPersistedData() const {
  return persisted_data_.get();
}

bool WebViewConfigurator::IsPerUserInstall() const {
  return true;
}

std::unique_ptr<update_client::ProtocolHandlerFactory>
WebViewConfigurator::GetProtocolHandlerFactory() const {
  return configurator_impl_.GetProtocolHandlerFactory();
}

std::optional<bool> WebViewConfigurator::IsMachineExternallyManaged() const {
  return configurator_impl_.IsMachineExternallyManaged();
}

update_client::UpdaterStateProvider
WebViewConfigurator::GetUpdaterStateProvider() const {
  return configurator_impl_.GetUpdaterStateProvider();
}

std::optional<base::FilePath> WebViewConfigurator::GetCrxCachePath() const {
  base::FilePath path;
  if (!base::PathService::Get(base::DIR_CACHE, &path)) {
    return std::nullopt;
  }
  return path.Append(FILE_PATH_LITERAL("ios_webview_crx_cache"));
}

bool WebViewConfigurator::IsConnectionMetered() const {
  return configurator_impl_.IsConnectionMetered();
}

}  // namespace

scoped_refptr<update_client::Configurator> MakeComponentUpdaterConfigurator(
    const base::CommandLine* cmdline) {
  return base::MakeRefCounted<WebViewConfigurator>(cmdline);
}

}  // namespace ios_web_view
