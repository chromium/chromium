// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/lib/browser/headless_browser_impl.h"

#include <string>
#include <utility>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/ptr_util.h"
#include "base/task/single_thread_task_runner.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/common/user_agent.h"
#include "headless/lib/browser/headless_browser_context_impl.h"
#include "headless/lib/browser/headless_browser_main_parts.h"
#include "headless/lib/browser/headless_devtools_agent_host_client.h"
#include "headless/lib/browser/headless_web_contents_impl.h"
#include "headless/public/version.h"

namespace headless {

namespace {
// Product name for building the default user agent string.
const char kHeadlessProductName[] = "HeadlessChrome";
constexpr gfx::Size kDefaultWindowSize(800, 600);

constexpr gfx::FontRenderParams::Hinting kDefaultFontRenderHinting =
    gfx::FontRenderParams::Hinting::HINTING_FULL;

}  // namespace

using Options = HeadlessBrowser::Options;
using Builder = HeadlessBrowser::Options::Builder;

Options::Options()
    : user_agent(content::BuildUserAgentFromProduct(
          HeadlessBrowser::GetProductNameAndVersion())),
      window_size(kDefaultWindowSize),
      font_render_hinting(kDefaultFontRenderHinting) {}

Options::Options(Options&& options) = default;

Options::~Options() = default;

Options& Options::operator=(Options&& options) = default;

bool Options::DevtoolsServerEnabled() {
  return (devtools_pipe_enabled || !devtools_endpoint.IsEmpty());
}

Builder::Builder() = default;

Builder::~Builder() = default;

Builder& Builder::SetUserAgent(const std::string& agent) {
  options_.user_agent = agent;
  return *this;
}

Builder& Builder::SetEnableLazyLoading(bool enable) {
  options_.lazy_load_enabled = enable;
  return *this;
}

Builder& Builder::SetAcceptLanguage(const std::string& language) {
  options_.accept_language = language;
  return *this;
}

Builder& Builder::SetEnableBeginFrameControl(bool enable) {
  options_.enable_begin_frame_control = enable;
  return *this;
}

Builder& Builder::EnableDevToolsServer(const net::HostPortPair& endpoint) {
  options_.devtools_endpoint = endpoint;
  return *this;
}

Builder& Builder::EnableDevToolsPipe() {
  options_.devtools_pipe_enabled = true;
  return *this;
}

Builder& Builder::SetProxyConfig(std::unique_ptr<net::ProxyConfig> config) {
  options_.proxy_config = std::move(config);
  return *this;
}

Builder& Builder::SetUserDataDir(const base::FilePath& dir) {
  options_.user_data_dir = dir;
  return *this;
}

Builder& Builder::SetWindowSize(const gfx::Size& size) {
  options_.window_size = size;
  return *this;
}

Builder& Builder::SetIncognitoMode(bool incognito) {
  options_.incognito_mode = incognito;
  return *this;
}

Builder& Builder::SetBlockNewWebContents(bool block) {
  options_.block_new_web_contents = block;
  return *this;
}

Builder& Builder::SetFontRenderHinting(gfx::FontRenderParams::Hinting hinting) {
  options_.font_render_hinting = hinting;
  return *this;
}

Options Builder::Build() {
  return std::move(options_);
}

/// static
std::string HeadlessBrowser::GetProductNameAndVersion() {
  return std::string(kHeadlessProductName) + "/" + PRODUCT_VERSION;
}

HeadlessBrowserImpl::HeadlessBrowserImpl(
    base::OnceCallback<void(HeadlessBrowser*)> on_start_callback)
    : on_start_callback_(std::move(on_start_callback)) {}

HeadlessBrowserImpl::~HeadlessBrowserImpl() = default;

void HeadlessBrowserImpl::SetOptions(HeadlessBrowser::Options options) {
  options_ = std::move(options);
}

HeadlessBrowserContext::Builder
HeadlessBrowserImpl::CreateBrowserContextBuilder() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return HeadlessBrowserContext::Builder(this);
}

scoped_refptr<base::SingleThreadTaskRunner>
HeadlessBrowserImpl::BrowserMainThread() const {
  return content::GetUIThreadTaskRunner({});
}

void HeadlessBrowserImpl::Shutdown() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  weak_ptr_factory_.InvalidateWeakPtrs();
  // Make sure GetAllBrowserContexts is sane if called after this point.
  auto tmp = std::move(browser_contexts_);
  tmp.clear();
  if (system_request_context_manager_) {
    content::GetIOThreadTaskRunner({})->DeleteSoon(
        FROM_HERE, system_request_context_manager_.release());
  }
  browser_main_parts_->QuitMainMessageLoop();
}

std::vector<HeadlessBrowserContext*>
HeadlessBrowserImpl::GetAllBrowserContexts() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  std::vector<HeadlessBrowserContext*> result;
  result.reserve(browser_contexts_.size());

  for (const auto& browser_context_pair : browser_contexts_) {
    result.push_back(browser_context_pair.second.get());
  }

  return result;
}

HeadlessBrowserMainParts* HeadlessBrowserImpl::browser_main_parts() const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return browser_main_parts_;
}

void HeadlessBrowserImpl::set_browser_main_parts(
    HeadlessBrowserMainParts* browser_main_parts) {
  DCHECK(!browser_main_parts_);
  browser_main_parts_ = browser_main_parts;
}

void HeadlessBrowserImpl::RunOnStartCallback() {
  // We don't support the tethering domain on this agent host.
  agent_host_ = content::DevToolsAgentHost::CreateForBrowser(
      nullptr, content::DevToolsAgentHost::CreateServerSocketCallback());

  PlatformStart();
  std::move(on_start_callback_).Run(this);
}

HeadlessBrowserContext* HeadlessBrowserImpl::CreateBrowserContext(
    HeadlessBrowserContext::Builder* builder) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  auto browser_context = HeadlessBrowserContextImpl::Create(builder);
  HeadlessBrowserContext* result = browser_context.get();
  browser_contexts_[browser_context->Id()] = std::move(browser_context);

  return result;
}

void HeadlessBrowserImpl::DestroyBrowserContext(
    HeadlessBrowserContextImpl* browser_context) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  int erased = browser_contexts_.erase(browser_context->Id());
  DCHECK(erased);
  if (default_browser_context_ == browser_context)
    SetDefaultBrowserContext(nullptr);
}

void HeadlessBrowserImpl::SetDefaultBrowserContext(
    HeadlessBrowserContext* browser_context) {
  DCHECK(!browser_context ||
         this == HeadlessBrowserContextImpl::From(browser_context)->browser());

  default_browser_context_ = browser_context;

  if (default_browser_context_ && !system_request_context_manager_) {
    system_request_context_manager_ =
        HeadlessRequestContextManager::CreateSystemContext(
            HeadlessBrowserContextImpl::From(browser_context)->options());
  }
}

HeadlessBrowserContext* HeadlessBrowserImpl::GetDefaultBrowserContext() {
  return default_browser_context_;
}

base::WeakPtr<HeadlessBrowserImpl> HeadlessBrowserImpl::GetWeakPtr() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return weak_ptr_factory_.GetWeakPtr();
}

HeadlessWebContents* HeadlessBrowserImpl::GetWebContentsForDevToolsAgentHostId(
    const std::string& devtools_agent_host_id) {
  for (HeadlessBrowserContext* context : GetAllBrowserContexts()) {
    HeadlessWebContents* web_contents =
        context->GetWebContentsForDevToolsAgentHostId(devtools_agent_host_id);
    if (web_contents)
      return web_contents;
  }
  return nullptr;
}

HeadlessWebContentsImpl* HeadlessBrowserImpl::GetWebContentsForWindowId(
    const int window_id) {
  for (HeadlessBrowserContext* context : GetAllBrowserContexts()) {
    for (HeadlessWebContents* web_contents : context->GetAllWebContents()) {
      auto* contents = HeadlessWebContentsImpl::From(web_contents);
      if (contents->window_id() == window_id) {
        return contents;
      }
    }
  }
  return nullptr;
}

HeadlessBrowserContext* HeadlessBrowserImpl::GetBrowserContextForId(
    const std::string& id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  auto find_it = browser_contexts_.find(id);
  if (find_it == browser_contexts_.end())
    return nullptr;
  return find_it->second.get();
}

HeadlessDevToolsTarget* HeadlessBrowserImpl::GetDevToolsTarget() {
  return agent_host_ ? this : nullptr;
}

std::unique_ptr<HeadlessDevToolsChannel>
HeadlessBrowserImpl::CreateDevToolsChannel() {
  DCHECK(agent_host_);
  return std::make_unique<HeadlessDevToolsAgentHostClient>(agent_host_);
}

#if defined(HEADLESS_USE_PREFS)
PrefService* HeadlessBrowserImpl::GetPrefs() {
  return browser_main_parts_ ? browser_main_parts_->GetPrefs() : nullptr;
}
#endif

#if defined(HEADLESS_USE_POLICY)
policy::PolicyService* HeadlessBrowserImpl::GetPolicyService() {
  return browser_main_parts_ ? browser_main_parts_->GetPolicyService()
                             : nullptr;
}
#endif

void HeadlessBrowserImpl::AttachClient(HeadlessDevToolsClient* client) {
  client->AttachToChannel(CreateDevToolsChannel());
}

void HeadlessBrowserImpl::DetachClient(HeadlessDevToolsClient* client) {
  client->DetachFromChannel();
}

bool HeadlessBrowserImpl::IsAttached() {
  DCHECK(agent_host_);
  return agent_host_->IsAttached();
}

}  // namespace headless
