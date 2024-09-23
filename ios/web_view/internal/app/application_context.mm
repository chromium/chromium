// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web_view/internal/app/application_context.h"

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/no_destructor.h"
#include "base/path_service.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/component_updater/component_updater_service.h"
#include "components/component_updater/installer_policies/autofill_states_component_installer.h"
#include "components/component_updater/timer_update_scheduler.h"
#include "components/flags_ui/pref_service_flags_storage.h"
#import "components/metrics/demographics/user_demographics.h"
#import "components/os_crypt/async/browser/os_crypt_async.h"
#include "components/prefs/json_pref_store.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/pref_service_factory.h"
#include "components/proxy_config/pref_proxy_config_tracker_impl.h"
#import "components/sessions/core/session_id_generator.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/translate/core/browser/translate_download_manager.h"
#include "components/update_client/update_client.h"
#include "components/variations/net/variations_http_headers.h"
#include "ios/components/security_interstitials/safe_browsing/safe_browsing_service_impl.h"
#include "ios/web/public/thread/web_task_traits.h"
#include "ios/web/public/thread/web_thread.h"
#include "ios/web_view/internal/app/web_view_io_thread.h"
#import "ios/web_view/internal/component_updater/web_view_component_updater_configurator.h"
#import "ios/web_view/internal/cwv_flags_internal.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "net/log/net_log.h"
#include "net/socket/client_socket_pool_manager.h"
#include "services/network/network_change_manager.h"
#include "services/network/public/cpp/network_connection_tracker.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "ui/base/device_form_factor.h"
#include "ui/base/l10n/l10n_util_mac.h"

namespace ios_web_view {
namespace {

// Passed to NetworkConnectionTracker to bind a NetworkChangeManager receiver.
void BindNetworkChangeManagerReceiver(
    network::NetworkChangeManager* network_change_manager,
    mojo::PendingReceiver<network::mojom::NetworkChangeManager> receiver) {
  network_change_manager->AddReceiver(std::move(receiver));
}

}  // namespace

ApplicationContext* ApplicationContext::GetInstance() {
  static base::NoDestructor<ApplicationContext> instance;
  return instance.get();
}

ApplicationContext::ApplicationContext() {
  SetApplicationLocale(l10n_util::GetLocaleOverride());
}

ApplicationContext::~ApplicationContext() = default;

void ApplicationContext::PreCreateThreads() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  web_view_io_thread_ =
      std::make_unique<WebViewIOThread>(GetLocalState(), GetNetLog());
}

void ApplicationContext::PostCreateThreads() {
  // Delegate all encryption calls to OSCrypt.
  os_crypt_async_ = std::make_unique<os_crypt_async::OSCryptAsync>(
      std::vector<std::pair<os_crypt_async::OSCryptAsync::Precedence,
                            std::unique_ptr<os_crypt_async::KeyProvider>>>());

  // Trigger an instance grab on a background thread if necessary.
  std::ignore = os_crypt_async_->GetInstance(base::DoNothing());

  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  web::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&WebViewIOThread::InitOnIO,
                                base::Unretained(web_view_io_thread_.get())));
}

void ApplicationContext::SaveState() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (local_state_) {
    local_state_->CommitPendingWrite();
    sessions::SessionIdGenerator::GetInstance()->Shutdown();
  }

  if (shared_url_loader_factory_)
    shared_url_loader_factory_->Detach();

  if (network_context_) {
    web::GetIOThreadTaskRunner({})->DeleteSoon(
        FROM_HERE, network_context_owner_.release());
  }
}

void ApplicationContext::PostDestroyThreads() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Resets associated state right after actual thread is stopped as
  // WebViewIOThread::Globals cleanup happens in CleanUp on the IO
  // thread, i.e. as the thread exits its message loop.
  //
  // This is important because in various places, the WebViewIOThread
  // object being null is considered synonymous with the IO thread
  // having stopped.
  web_view_io_thread_.reset();
}

PrefService* ApplicationContext::GetLocalState() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!local_state_) {
    // Register local state preferences.
    scoped_refptr<PrefRegistrySimple> pref_registry(new PrefRegistrySimple);
    flags_ui::PrefServiceFlagsStorage::RegisterPrefs(pref_registry.get());
    PrefProxyConfigTrackerImpl::RegisterPrefs(pref_registry.get());
    signin::IdentityManager::RegisterLocalStatePrefs(pref_registry.get());
    component_updater::RegisterComponentUpdateServicePrefs(pref_registry.get());
    update_client::RegisterPrefs(pref_registry.get());
    component_updater::AutofillStatesComponentInstallerPolicy::RegisterPrefs(
        pref_registry.get());
    metrics::RegisterDemographicsLocalStatePrefs(pref_registry.get());
    sessions::SessionIdGenerator::RegisterPrefs(pref_registry.get());

    base::FilePath local_state_path;
    base::PathService::Get(base::DIR_APP_DATA, &local_state_path);
    local_state_path =
        local_state_path.Append(FILE_PATH_LITERAL("ChromeWebViewLocalState"));

    scoped_refptr<PersistentPrefStore> user_pref_store =
        new JsonPrefStore(std::move(local_state_path));

    PrefServiceFactory factory;
    factory.set_user_prefs(user_pref_store);
    local_state_ = factory.Create(pref_registry.get());

    sessions::SessionIdGenerator::GetInstance()->Init(local_state_.get());

    int max_normal_socket_pool_count =
        net::ClientSocketPoolManager::max_sockets_per_group(
            net::HttpNetworkSession::NORMAL_SOCKET_POOL);
    int socket_count = std::max<int>(net::kDefaultMaxSocketsPerProxyChain,
                                     max_normal_socket_pool_count);
    net::ClientSocketPoolManager::set_max_sockets_per_proxy_chain(
        net::HttpNetworkSession::NORMAL_SOCKET_POOL, socket_count);
  }
  return local_state_.get();
}

net::URLRequestContextGetter* ApplicationContext::GetSystemURLRequestContext() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return web_view_io_thread_->system_url_request_context_getter();
}

scoped_refptr<network::SharedURLLoaderFactory>
ApplicationContext::GetSharedURLLoaderFactory() {
  if (!url_loader_factory_) {
    auto url_loader_factory_params =
        network::mojom::URLLoaderFactoryParams::New();
    url_loader_factory_params->process_id = network::mojom::kBrowserProcessId;
    url_loader_factory_params->is_orb_enabled = false;
    GetSystemNetworkContext()->CreateURLLoaderFactory(
        url_loader_factory_.BindNewPipeAndPassReceiver(),
        std::move(url_loader_factory_params));
    shared_url_loader_factory_ =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            url_loader_factory_.get());
  }
  return shared_url_loader_factory_;
}

network::mojom::NetworkContext* ApplicationContext::GetSystemNetworkContext() {
  if (!network_context_) {
    network::mojom::NetworkContextParamsPtr network_context_params =
        network::mojom::NetworkContextParams::New();
    variations::UpdateCorsExemptHeaderForVariations(
        network_context_params.get());
    network_context_owner_ = std::make_unique<web::NetworkContextOwner>(
        GetSystemURLRequestContext(),
        network_context_params->cors_exempt_header_list, &network_context_);
  }
  return network_context_.get();
}

network::NetworkConnectionTracker*
ApplicationContext::GetNetworkConnectionTracker() {
  if (!network_connection_tracker_) {
    if (!network_change_manager_) {
      network_change_manager_ =
          std::make_unique<network::NetworkChangeManager>(nullptr);
    }
    network_connection_tracker_ =
        std::make_unique<network::NetworkConnectionTracker>(base::BindRepeating(
            &BindNetworkChangeManagerReceiver,
            base::Unretained(network_change_manager_.get())));
  }
  return network_connection_tracker_.get();
}

const std::string& ApplicationContext::GetApplicationLocale() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!application_locale_.empty());
  return application_locale_;
}

net::NetLog* ApplicationContext::GetNetLog() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return net::NetLog::Get();
}

component_updater::ComponentUpdateService*
ApplicationContext::GetComponentUpdateService() {
  if (!component_updater_) {
    // TODO(crbug.com/40215633): Brand code should be configurable.
    std::string brand_code =
        ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET ? "APLB"
                                                                   : "APLA";
    component_updater_ = component_updater::ComponentUpdateServiceFactory(
        MakeComponentUpdaterConfigurator(
            base::CommandLine::ForCurrentProcess()),
        std::make_unique<component_updater::TimerUpdateScheduler>(),
        brand_code);
  }
  return component_updater_.get();
}

os_crypt_async::OSCryptAsync* ApplicationContext::GetOSCryptAsync() {
  return os_crypt_async_.get();
}

WebViewIOThread* ApplicationContext::GetWebViewIOThread() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(web_view_io_thread_.get());
  return web_view_io_thread_.get();
}

void ApplicationContext::SetApplicationLocale(const std::string& locale) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  application_locale_ = locale;
  translate::TranslateDownloadManager::GetInstance()->set_application_locale(
      application_locale_);
}

SafeBrowsingService* ApplicationContext::GetSafeBrowsingService() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!safe_browsing_service_) {
    safe_browsing_service_ = base::MakeRefCounted<SafeBrowsingServiceImpl>();
  }
  return safe_browsing_service_.get();
}

void ApplicationContext::ShutdownSafeBrowsingServiceIfNecessary() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (safe_browsing_service_) {
    safe_browsing_service_->ShutDown();
  }
}

}  // namespace ios_web_view
