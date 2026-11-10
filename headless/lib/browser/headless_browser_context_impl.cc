// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/lib/browser/headless_browser_context_impl.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/not_fatal_until.h"
#include "base/path_service.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/core/simple_key_map.h"
#include "components/origin_trials/browser/leveldb_persistence_provider.h"
#include "components/origin_trials/browser/origin_trials.h"
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
          std::make_unique<HeadlessPermissionManager>()),
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
      context_options_.get(), user_data_path, browser->os_crypt_async());
  profile_metrics::SetBrowserProfileType(
      this, IsOffTheRecord() ? profile_metrics::BrowserProfileType::kIncognito
                             : profile_metrics::BrowserProfileType::kRegular);

  // Ensure the delegate is initialized early to give it time to load its
  // persistence.
  GetOriginTrialsControllerDelegate();
}

HeadlessBrowserContextImpl::~HeadlessBrowserContextImpl() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  SimpleKeyMap::GetInstance()->Dissociate(this);
  NotifyWillBeDestroyed();

  // Destroy all web contents before shutting down in process renderer and
  // storage partitions. Note that web_contents_ container can't be "just"
  // cleared as deleting one element may lead to other elements being deleted.
  while (!web_contents_map_.empty()) {
    web_contents_map_.erase(web_contents_map_.begin());
  }

  // In single process mode we can only have one browser context, so it's
  // safe to shutdown the in-process renderer here.
  if (content::RenderProcessHost::run_renderer_in_process())
    content::RenderProcessHost::ShutDownInProcessRenderer();

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
    HeadlessBrowserImpl* browser,
    HeadlessBrowserContext::CreateParams params) {
  return std::make_unique<HeadlessBrowserContextImpl>(
      browser, std::make_unique<HeadlessBrowserContextOptions>(
                   browser->options(), std::move(params)));
}

HeadlessWebContents* HeadlessBrowserContextImpl::CreateWebContents(
    const HeadlessWebContents::CreateParams& params) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  std::unique_ptr<HeadlessWebContentsImpl> headless_web_contents =
      HeadlessWebContentsImpl::Create(params);

  if (!headless_web_contents) {
    return nullptr;
  }

  HeadlessWebContents* result = headless_web_contents.get();

  RegisterWebContents(std::move(headless_web_contents));

  return result;
}

HeadlessWebContents* HeadlessBrowserContextImpl::CreateWebContents(
    const GURL& initial_url) {
  return CreateWebContents(
      HeadlessWebContents::CreateParams(this, initial_url));
}

HeadlessWebContents* HeadlessBrowserContextImpl::CreateWebContents() {
  return CreateWebContents(HeadlessWebContents::CreateParams(this));
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
  while (!web_contents_map_.empty()) {
    auto it = web_contents_map_.begin();
    it->second->Close();
  }
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

base::FilePath HeadlessBrowserContextImpl::GetPath() const {
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

content::OriginTrialsControllerDelegate*
HeadlessBrowserContextImpl::GetOriginTrialsControllerDelegate() {
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

void HeadlessBrowserContextImpl::RegisterWebContents(
    std::unique_ptr<HeadlessWebContentsImpl> web_contents) {
  CHECK(web_contents);
  const uintptr_t key =
      reinterpret_cast<uintptr_t>(web_contents->web_contents());
  CHECK(key);
  const bool inserted =
      web_contents_map_.insert({key, std::move(web_contents)}).second;
  CHECK(inserted);
}

void HeadlessBrowserContextImpl::DestroyWebContents(
    HeadlessWebContentsImpl* web_contents) {
  CHECK(web_contents);
  const uintptr_t key =
      reinterpret_cast<uintptr_t>(web_contents->web_contents());
  CHECK(key);
  size_t erased = web_contents_map_.erase(key);
  CHECK(erased);
}

HeadlessWebContentsImpl* HeadlessBrowserContextImpl::GetHeadlessWebContents(
    const content::WebContents* web_contents) {
  const uintptr_t key = reinterpret_cast<uintptr_t>(web_contents);
  auto find_it = web_contents_map_.find(key);
  return find_it == web_contents_map_.end() ? nullptr : find_it->second.get();
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

HeadlessBrowserContext::CreateParams::CreateParams() = default;

HeadlessBrowserContext::CreateParams::~CreateParams() = default;

HeadlessBrowserContext::CreateParams::CreateParams(CreateParams&&) = default;

HeadlessBrowserContext::CreateParams&
HeadlessBrowserContext::CreateParams::operator=(CreateParams&&) = default;

}  // namespace headless
