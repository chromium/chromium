// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/lib/browser/headless_browser_context_impl.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/memory/ptr_util.h"
#include "base/not_fatal_until.h"
#include "base/path_service.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/core/simple_key_map.h"
#include "components/origin_trials/browser/leveldb_persistence_provider.h"
#include "components/origin_trials/browser/origin_trials.h"
#include "components/origin_trials/common/features.h"
#include "components/profile_metrics/browser_profile_type.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/storage_partition.h"
#include "headless/lib/browser/headless_browser_context_options.h"
#include "headless/lib/browser/headless_browser_impl.h"
#include "headless/lib/browser/headless_browser_main_parts.h"
#include "headless/lib/browser/headless_client_hints_controller_delegate.h"
#include "headless/lib/browser/headless_permission_manager.h"
#include "headless/lib/browser/headless_web_contents_impl.h"
#include "third_party/blink/public/common/origin_trials/trial_token_validator.h"
#include "ui/base/resource/resource_bundle.h"

#if defined(HEADLESS_USE_POLICY)
#include "components/user_prefs/user_prefs.h"  // nogncheck
#endif                                         // defined(HEADLESS_USE_POLICY)

namespace headless {

namespace {

base::FilePath MakeAbsolutePath(const base::FilePath& path) {
#if BUILDFLAG(IS_WIN)
  // On Windows it's common to omit drive specification assuming the current
  // drive, which makes the path specification not absolute, but relative to
  // the current drive. Handle this case by prepending the current drive to
  // the "\path" specification.
  std::vector<base::FilePath::StringType> components = path.GetComponents();
  if (components.size() > 0 && components[0].length() == 1 &&
      base::FilePath::IsSeparator(components[0].front())) {
    components =
        base::PathService::CheckedGet(base::DIR_CURRENT).GetComponents();
    return base::FilePath(components[0]).Append(path);
  }
#endif  // BUILDFLAG(IS_WIN)

  return base::PathService::CheckedGet(base::DIR_CURRENT).Append(path);
}

}  // namespace

HeadlessBrowserContextImpl::HeadlessBrowserContextImpl(
    HeadlessBrowserImpl* browser,
    std::unique_ptr<HeadlessBrowserContextOptions> context_options)
    : browser_(browser),
      context_options_(std::move(context_options)),
      permission_controller_delegate_(
          std::make_unique<HeadlessPermissionManager>(this)),
      hints_delegate_(
          std::make_unique<HeadlessClientHintsControllerDelegate>()) {
  BrowserContextDependencyManager::GetInstance()->MarkBrowserContextLive(this);
  InitWhileIOAllowed();
  simple_factory_key_ =
      std::make_unique<SimpleFactoryKey>(GetPath(), IsOffTheRecord());
  SimpleKeyMap::GetInstance()->Associate(this, simple_factory_key_.get());
  base::FilePath user_data_path =
      IsOffTheRecord() || context_options_->user_data_dir().empty()
          ? base::FilePath()
          : path_;
  request_context_manager_ = std::make_unique<HeadlessRequestContextManager>(
      context_options_.get(), user_data_path);
  profile_metrics::SetBrowserProfileType(
      this, IsOffTheRecord() ? profile_metrics::BrowserProfileType::kIncognito
                             : profile_metrics::BrowserProfileType::kRegular);
#if defined(HEADLESS_USE_POLICY)
  if (PrefService* pref_service = browser->GetPrefs())
    user_prefs::UserPrefs::Set(this, pref_service);
#endif  // defined(HEADLESS_USE_POLICY)

  // Ensure the delegate is initialized early to give it time to load its
  // persistence.
  GetOriginTrialsControllerDelegate();
}

HeadlessBrowserContextImpl::~HeadlessBrowserContextImpl() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  SimpleKeyMap::GetInstance()->Dissociate(this);
  NotifyWillBeDestroyed();

  // Destroy all web contents before shutting down in process renderer and
  // storage partitions.
  web_contents_map_.clear();

  // In single process mode we can only have one browser context, so it's
  // safe to shutdown the in-process renderer here.
  if (content::RenderProcessHost::run_renderer_in_process())
    content::RenderProcessHost::ShutDownInProcessRenderer();

  if (request_context_manager_) {
    content::GetIOThreadTaskRunner({})->DeleteSoon(
        FROM_HERE, request_context_manager_.release());
  }

  ShutdownStoragePartitions();

  BrowserContextDependencyManager::GetInstance()->DestroyBrowserContextServices(
      this);
}

// static
HeadlessBrowserContextImpl* HeadlessBrowserContextImpl::From(
    HeadlessBrowserContext* browser_context) {
  return static_cast<HeadlessBrowserContextImpl*>(browser_context);
}

// static
HeadlessBrowserContextImpl* HeadlessBrowserContextImpl::From(
    content::BrowserContext* browser_context) {
  return static_cast<HeadlessBrowserContextImpl*>(browser_context);
}

// static
std::unique_ptr<HeadlessBrowserContextImpl> HeadlessBrowserContextImpl::Create(
    HeadlessBrowserContext::Builder* builder) {
  return base::WrapUnique(new HeadlessBrowserContextImpl(
      builder->browser_, std::move(builder->options_)));
}

HeadlessWebContents::Builder
HeadlessBrowserContextImpl::CreateWebContentsBuilder() {
  DCHECK(browser_->BrowserMainThread()->BelongsToCurrentThread());
  return HeadlessWebContents::Builder(this);
}

std::vector<HeadlessWebContents*>
HeadlessBrowserContextImpl::GetAllWebContents() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  std::vector<HeadlessWebContents*> result;
  result.reserve(web_contents_map_.size());

  for (const auto& web_contents_pair : web_contents_map_) {
    result.push_back(web_contents_pair.second.get());
  }

  return result;
}

void HeadlessBrowserContextImpl::Close() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  browser_->DestroyBrowserContext(this);
}

void HeadlessBrowserContextImpl::InitWhileIOAllowed() {
  if (!context_options_->user_data_dir().empty()) {
    base::FilePath path =
        context_options_->user_data_dir().Append(kDefaultProfileName);
    if (!path.IsAbsolute())
      path = MakeAbsolutePath(path);

    path_ = std::move(path);
  } else {
    base::PathService::Get(base::DIR_EXE, &path_);
  }
  DCHECK(path_.IsAbsolute());
}

std::unique_ptr<content::ZoomLevelDelegate>
HeadlessBrowserContextImpl::CreateZoomLevelDelegate(
    const base::FilePath& partition_path) {
  return nullptr;
}

base::FilePath HeadlessBrowserContextImpl::GetPath() {
  return path_;
}

bool HeadlessBrowserContextImpl::IsOffTheRecord() {
  return context_options_->incognito_mode();
}

content::DownloadManagerDelegate*
HeadlessBrowserContextImpl::GetDownloadManagerDelegate() {
  return nullptr;
}

content::BrowserPluginGuestManager*
HeadlessBrowserContextImpl::GetGuestManager() {
  // TODO(altimin): Should be non-null? (is null in content/shell).
  return nullptr;
}

storage::SpecialStoragePolicy*
HeadlessBrowserContextImpl::GetSpecialStoragePolicy() {
  return nullptr;
}

content::PlatformNotificationService*
HeadlessBrowserContextImpl::GetPlatformNotificationService() {
  return nullptr;
}

content::PushMessagingService*
HeadlessBrowserContextImpl::GetPushMessagingService() {
  return nullptr;
}

content::StorageNotificationService*
HeadlessBrowserContextImpl::GetStorageNotificationService() {
  return nullptr;
}
content::SSLHostStateDelegate*
HeadlessBrowserContextImpl::GetSSLHostStateDelegate() {
  return nullptr;
}

content::PermissionControllerDelegate*
HeadlessBrowserContextImpl::GetPermissionControllerDelegate() {
  return permission_controller_delegate_.get();
}

content::ClientHintsControllerDelegate*
HeadlessBrowserContextImpl::GetClientHintsControllerDelegate() {
  return hints_delegate_.get();
}

content::BackgroundFetchDelegate*
HeadlessBrowserContextImpl::GetBackgroundFetchDelegate() {
  return nullptr;
}

content::BackgroundSyncController*
HeadlessBrowserContextImpl::GetBackgroundSyncController() {
  return nullptr;
}

content::BrowsingDataRemoverDelegate*
HeadlessBrowserContextImpl::GetBrowsingDataRemoverDelegate() {
  return nullptr;
}

content::ReduceAcceptLanguageControllerDelegate*
HeadlessBrowserContextImpl::GetReduceAcceptLanguageControllerDelegate() {
  return nullptr;
}

content::OriginTrialsControllerDelegate*
HeadlessBrowserContextImpl::GetOriginTrialsControllerDelegate() {
  if (!origin_trials::features::IsPersistentOriginTrialsEnabled())
    return nullptr;

  if (!origin_trials_controller_delegate_) {
    origin_trials_controller_delegate_ =
        std::make_unique<origin_trials::OriginTrials>(
            std::make_unique<origin_trials::LevelDbPersistenceProvider>(
                GetPath(),
                GetDefaultStoragePartition()->GetProtoDatabaseProvider()),
            std::make_unique<blink::TrialTokenValidator>());
  }
  return origin_trials_controller_delegate_.get();
}

HeadlessWebContents* HeadlessBrowserContextImpl::CreateWebContents(
    HeadlessWebContents::Builder* builder) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  std::unique_ptr<HeadlessWebContentsImpl> headless_web_contents =
      HeadlessWebContentsImpl::Create(builder);

  if (!headless_web_contents) {
    return nullptr;
  }

  HeadlessWebContents* result = headless_web_contents.get();

  RegisterWebContents(std::move(headless_web_contents));

  return result;
}

void HeadlessBrowserContextImpl::RegisterWebContents(
    std::unique_ptr<HeadlessWebContentsImpl> web_contents) {
  DCHECK(web_contents);
  web_contents_map_[web_contents->GetDevToolsAgentHostId()] =
      std::move(web_contents);
}

void HeadlessBrowserContextImpl::DestroyWebContents(
    HeadlessWebContentsImpl* web_contents) {
  auto it = web_contents_map_.find(web_contents->GetDevToolsAgentHostId());
  CHECK(it != web_contents_map_.end(), base::NotFatalUntil::M130);
  web_contents_map_.erase(it);
}

HeadlessWebContents*
HeadlessBrowserContextImpl::GetWebContentsForDevToolsAgentHostId(
    const std::string& devtools_agent_host_id) {
  auto find_it = web_contents_map_.find(devtools_agent_host_id);
  if (find_it == web_contents_map_.end())
    return nullptr;
  return find_it->second.get();
}

HeadlessBrowserImpl* HeadlessBrowserContextImpl::browser() const {
  return browser_;
}

const HeadlessBrowserContextOptions* HeadlessBrowserContextImpl::options()
    const {
  return context_options_.get();
}

const std::string& HeadlessBrowserContextImpl::Id() {
  return UniqueId();
}

void HeadlessBrowserContextImpl::ConfigureNetworkContextParams(
    bool in_memory,
    const base::FilePath& relative_partition_path,
    ::network::mojom::NetworkContextParams* network_context_params,
    ::cert_verifier::mojom::CertVerifierCreationParams*
        cert_verifier_creation_params) {
  request_context_manager_->ConfigureNetworkContextParams(
      in_memory, relative_partition_path, network_context_params,
      cert_verifier_creation_params);
}

HeadlessBrowserContext::Builder::Builder(HeadlessBrowserImpl* browser)
    : browser_(browser),
      options_(new HeadlessBrowserContextOptions(browser->options())) {}

HeadlessBrowserContext::Builder::~Builder() = default;

HeadlessBrowserContext::Builder::Builder(Builder&&) = default;

HeadlessBrowserContext::Builder& HeadlessBrowserContext::Builder::SetUserAgent(
    const std::string& user_agent) {
  options_->user_agent_ = user_agent;
  return *this;
}

HeadlessBrowserContext::Builder&
HeadlessBrowserContext::Builder::SetAcceptLanguage(
    const std::string& accept_language) {
  options_->accept_language_ = accept_language;
  return *this;
}

HeadlessBrowserContext::Builder&
HeadlessBrowserContext::Builder::SetProxyConfig(
    std::unique_ptr<net::ProxyConfig> proxy_config) {
  options_->proxy_config_ = std::move(proxy_config);
  return *this;
}

HeadlessBrowserContext::Builder& HeadlessBrowserContext::Builder::SetWindowSize(
    const gfx::Size& window_size) {
  options_->window_size_ = window_size;
  return *this;
}

HeadlessBrowserContext::Builder&
HeadlessBrowserContext::Builder::SetUserDataDir(
    const base::FilePath& user_data_dir) {
  options_->user_data_dir_ = user_data_dir;
  return *this;
}

HeadlessBrowserContext::Builder&
HeadlessBrowserContext::Builder::SetDiskCacheDir(
    const base::FilePath& disk_cache_dir) {
  options_->disk_cache_dir_ = disk_cache_dir;
  return *this;
}

HeadlessBrowserContext::Builder&
HeadlessBrowserContext::Builder::SetIncognitoMode(bool incognito_mode) {
  options_->incognito_mode_ = incognito_mode;
  return *this;
}

HeadlessBrowserContext::Builder&
HeadlessBrowserContext::Builder::SetBlockNewWebContents(
    bool block_new_web_contents) {
  options_->block_new_web_contents_ = block_new_web_contents;
  return *this;
}

HeadlessBrowserContext* HeadlessBrowserContext::Builder::Build() {
  return browser_->CreateBrowserContext(this);
}

}  // namespace headless
