// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/lib/browser/headless_devtools_manager_delegate.h"

#include "base/containers/span.h"
#include "build/build_config.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/devtools_agent_host_client_channel.h"
#include "content/public/browser/web_contents.h"
#include "headless/lib/browser/headless_browser_context_impl.h"
#include "headless/lib/browser/headless_browser_impl.h"
#include "headless/lib/browser/headless_web_contents_impl.h"
#include "headless/lib/browser/protocol/headless_devtools_session.h"
#include "ui/base/resource/resource_bundle.h"

namespace headless {

HeadlessDevToolsManagerDelegate::HeadlessDevToolsManagerDelegate(
    base::WeakPtr<HeadlessBrowserImpl> browser)
    : browser_(std::move(browser)) {}

HeadlessDevToolsManagerDelegate::~HeadlessDevToolsManagerDelegate() = default;

void HeadlessDevToolsManagerDelegate::HandleCommand(
    content::DevToolsAgentHostClientChannel* channel,
    base::span<const uint8_t> message,
    NotHandledCallback callback) {
  auto it = sessions_.find(channel);
  CHECK(it != sessions_.end());
  it->second->HandleCommand(message, std::move(callback));
}

scoped_refptr<content::DevToolsAgentHost>
HeadlessDevToolsManagerDelegate::CreateNewTarget(
    const GURL& url,
    content::DevToolsManagerDelegate::TargetType target_type,
    bool new_window) {
  if (!browser_)
    return nullptr;

  HeadlessBrowserContext* context = browser_->GetDefaultBrowserContext();
  HeadlessWebContents::CreateParams create_params(context, url);
  create_params.window_bounds = gfx::Rect(browser_->options()->window_size);
  HeadlessWebContentsImpl* web_contents_impl =
      HeadlessWebContentsImpl::From(context->CreateWebContents(create_params));
  return target_type == content::DevToolsManagerDelegate::kTab
             ? content::DevToolsAgentHost::GetOrCreateForTab(
                   web_contents_impl->web_contents())
             : content::DevToolsAgentHost::GetOrCreateFor(
                   web_contents_impl->web_contents());
}

bool HeadlessDevToolsManagerDelegate::HasBundledFrontendResources() {
  return true;
}

void HeadlessDevToolsManagerDelegate::ClientAttached(
    content::DevToolsAgentHostClientChannel* channel) {
  DCHECK(!sessions_.contains(channel));
  sessions_.emplace(
      channel,
      std::make_unique<protocol::HeadlessDevToolsSession>(browser_, channel));
}

void HeadlessDevToolsManagerDelegate::ClientDetached(
    content::DevToolsAgentHostClientChannel* channel) {
  sessions_.erase(channel);
}

std::vector<base::WeakPtr<content::BrowserContext>>
HeadlessDevToolsManagerDelegate::GetBrowserContexts() {
  std::vector<base::WeakPtr<content::BrowserContext>> contexts;
  if (!browser_)
    return contexts;
  for (auto* context : browser_->GetAllBrowserContexts()) {
    if (context != browser_->GetDefaultBrowserContext()) {
      contexts.push_back(
          HeadlessBrowserContextImpl::From(context)->GetWeakPtr());
    }
  }
  return contexts;
}

content::BrowserContext*
HeadlessDevToolsManagerDelegate::GetDefaultBrowserContext() {
  if (!browser_) {
    return nullptr;
  }
  return HeadlessBrowserContextImpl::From(browser_->GetDefaultBrowserContext());
}

content::BrowserContext* HeadlessDevToolsManagerDelegate::GetBrowserContext(
    const std::string& context_id) {
  if (!browser_) {
    return nullptr;
  }
  return HeadlessBrowserContextImpl::From(
      browser_->GetBrowserContextForId(context_id));
}

content::BrowserContext*
HeadlessDevToolsManagerDelegate::CreateBrowserContext() {
  if (!browser_)
    return nullptr;
  HeadlessBrowserContext::CreateParams params;
  params.incognito_mode = true;
  HeadlessBrowserContext* browser_context =
      browser_->CreateBrowserContext(std::move(params));
  CHECK(browser_context);
  return HeadlessBrowserContextImpl::From(browser_context);
}

void HeadlessDevToolsManagerDelegate::DisposeBrowserContext(
    content::BrowserContext* browser_context,
    DisposeCallback callback) {
  HeadlessBrowserContextImpl* context =
      HeadlessBrowserContextImpl::From(browser_context);
  context->Close();
  std::move(callback).Run(true, "");
}

}  // namespace headless
