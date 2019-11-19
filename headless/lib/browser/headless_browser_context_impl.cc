// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/lib/browser/headless_browser_context_impl.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/guid.h"
#include "base/path_service.h"
#include "base/task/post_task.h"
#include "components/keyed_service/core/simple_key_map.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "headless/grit/headless_lib_resources.h"
#include "headless/lib/browser/headless_browser_context_options.h"
#include "headless/lib/browser/headless_browser_impl.h"
#include "headless/lib/browser/headless_browser_main_parts.h"
#include "headless/lib/browser/headless_permission_manager.h"
#include "headless/lib/browser/headless_web_contents_impl.h"
#include "ui/base/resource/resource_bundle.h"

namespace headless {

HeadlessBrowserContextImpl::HeadlessBrowserContextImpl(
    HeadlessBrowserImpl* browser,
    std::unique_ptr<HeadlessBrowserContextOptions> context_options)
    : browser_(browser),
      context_options_(std::move(context_options)),
      permission_controller_delegate_(
          std::make_unique<HeadlessPermissionManager>(this)) {
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
}

HeadlessBrowserContextImpl::~HeadlessBrowserContextImpl() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  SimpleKeyMap::GetInstance()->Dissociate(this);
  NotifyWillBeDestroyed(this);

  // Destroy all web contents before shutting down storage partitions.
  web_contents_map_.clear();

  if (request_context_manager_) {
    base::DeleteSoon(FROM_HERE, {content::BrowserThread::IO},
                     request_context_manager_.release());
  }

  ShutdownStoragePartitions();
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

void HeadlessBrowserContextImpl::SetDevToolsFrameToken(
    int render_process_id,
    int render_frame_routing_id,
    const base::UnguessableToken& devtools_frame_token,
    int frame_tree_node_id) {
  base::AutoLock lock(devtools_frame_token_map_lock_);
  devtools_frame_token_map_[content::GlobalFrameRoutingId(
      render_process_id, render_frame_routing_id)] = devtools_frame_token;
  frame_tree_node_id_to_devtools_frame_token_map_[frame_tree_node_id] =
      devtools_frame_token;
}

void HeadlessBrowserContextImpl::RemoveDevToolsFrameToken(
    int render_process_id,
    int render_frame_routing_id,
    int frame_tree_node_id) {
  base::AutoLock lock(devtools_frame_token_map_lock_);
  devtools_frame_token_map_.erase(content::GlobalFrameRoutingId(
      render_process_id, render_frame_routing_id));
  frame_tree_node_id_to_devtools_frame_token_map_.erase(frame_tree_node_id);
}

const base::UnguessableToken* HeadlessBrowserContextImpl::GetDevToolsFrameToken(
    int render_process_id,
    int render_frame_id) const {
  base::AutoLock lock(devtools_frame_token_map_lock_);
  const auto& find_it = devtools_frame_token_map_.find(
      content::GlobalFrameRoutingId(render_process_id, render_frame_id));
  if (find_it == devtools_frame_token_map_.end())
    return nullptr;
  return &find_it->second;
}

const base::UnguessableToken*
HeadlessBrowserContextImpl::GetDevToolsFrameTokenForFrameTreeNodeId(
    int frame_tree_node_id) const {
  base::AutoLock lock(devtools_frame_token_map_lock_);
  const auto& find_it =
      frame_tree_node_id_to_devtools_frame_token_map_.find(frame_tree_node_id);
  if (find_it == frame_tree_node_id_to_devtools_frame_token_map_.end())
    return nullptr;
  return &find_it->second;
}

void HeadlessBrowserContextImpl::Close() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  browser_->DestroyBrowserContext(this);
}

void HeadlessBrowserContextImpl::InitWhileIOAllowed() {
  if (!context_options_->user_data_dir().empty()) {
    path_ = context_options_->user_data_dir().Append(kDefaultProfileName);
  } else {
    base::PathService::Get(base::DIR_EXE, &path_);
  }
  BrowserContext::Initialize(this, path_);
}

std::unique_ptr<content::ZoomLevelDelegate>
HeadlessBrowserContextImpl::CreateZoomLevelDelegate(
    const base::FilePath& partition_path) {
  return std::unique_ptr<content::ZoomLevelDelegate>();
}

base::FilePath HeadlessBrowserContextImpl::GetPath() {
  return path_;
}

bool HeadlessBrowserContextImpl::IsOffTheRecord() {
  return context_options_->incognito_mode();
}

content::ResourceContext* HeadlessBrowserContextImpl::GetResourceContext() {
  return request_context_manager_->GetResourceContext();
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
  return nullptr;
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
  DCHECK(it != web_contents_map_.end());
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

mojo::Remote<::network::mojom::NetworkContext>
HeadlessBrowserContextImpl::CreateNetworkContext(
    bool in_memory,
    const base::FilePath& relative_partition_path) {
  return request_context_manager_->CreateNetworkContext(
      in_memory, relative_partition_path);
}

HeadlessBrowserContext::Builder::Builder(HeadlessBrowserImpl* browser)
    : browser_(browser),
      options_(new HeadlessBrowserContextOptions(browser->options())) {}

HeadlessBrowserContext::Builder::~Builder() = default;

HeadlessBrowserContext::Builder::Builder(Builder&&) = default;

HeadlessBrowserContext::Builder&
HeadlessBrowserContext::Builder::SetProductNameAndVersion(
    const std::string& product_name_and_version) {
  options_->product_name_and_version_ = product_name_and_version;
  return *this;
}

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

HeadlessBrowserContext::Builder&
HeadlessBrowserContext::Builder::SetOverrideWebPreferencesCallback(
    base::RepeatingCallback<void(WebPreferences*)> callback) {
  options_->override_web_preferences_callback_ = std::move(callback);
  return *this;
}

HeadlessBrowserContext* HeadlessBrowserContext::Builder::Build() {
  return browser_->CreateBrowserContext(this);
}

HeadlessBrowserContext::Builder::MojoBindings::MojoBindings() = default;

HeadlessBrowserContext::Builder::MojoBindings::MojoBindings(
    const std::string& mojom_name,
    const std::string& js_bindings)
    : mojom_name(mojom_name), js_bindings(js_bindings) {}

HeadlessBrowserContext::Builder::MojoBindings::~MojoBindings() = default;

}  // namespace headless
