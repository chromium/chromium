// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/lib/browser/headless_devtools_manager_delegate.h"

#include "base/containers/contains.h"
#include "base/not_fatal_until.h"
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
  CHECK(it != sessions_.end(), base::NotFatalUntil::M130);
  it->second->HandleCommand(message, std::move(callback));
}

scoped_refptr<content::DevToolsAgentHost>
HeadlessDevToolsManagerDelegate::CreateNewTarget(
    const GURL& url,
    content::DevToolsManagerDelegate::TargetType target_type) {
  if (!browser_)
    return nullptr;

  HeadlessBrowserContext* context = browser_->GetDefaultBrowserContext();
  HeadlessWebContentsImpl* web_contents_impl = HeadlessWebContentsImpl::From(
      context->CreateWebContentsBuilder()
          .SetInitialURL(url)
          .SetWindowSize(browser_->options()->window_size)
          .Build());
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
  DCHECK(!base::Contains(sessions_, channel));
  sessions_.emplace(
      channel,
      std::make_unique<protocol::HeadlessDevToolsSession>(browser_, channel));
}

void HeadlessDevToolsManagerDelegate::ClientDetached(
    content::DevToolsAgentHostClientChannel* channel) {
  sessions_.erase(channel);
}

std::vector<content::BrowserContext*>
HeadlessDevToolsManagerDelegate::GetBrowserContexts() {
  std::vector<content::BrowserContext*> contexts;
  if (!browser_)
    return contexts;
  for (auto* context : browser_->GetAllBrowserContexts()) {
    if (context != browser_->GetDefaultBrowserContext())
      contexts.push_back(HeadlessBrowserContextImpl::From(context));
  }
  return contexts;
}
content::BrowserContext*
HeadlessDevToolsManagerDelegate::GetDefaultBrowserContext() {
  return browser_ ? HeadlessBrowserContextImpl::From(
                        browser_->GetDefaultBrowserContext())
                  : nullptr;
}

content::BrowserContext*
HeadlessDevToolsManagerDelegate::CreateBrowserContext() {
  if (!browser_)
    return nullptr;
  auto builder = browser_->CreateBrowserContextBuilder();
  builder.SetIncognitoMode(true);
  HeadlessBrowserContext* browser_context = builder.Build();
  return HeadlessBrowserContextImpl::From(browser_context);
}

void HeadlessDevToolsManagerDelegate::DisposeBrowserContext(
    content::BrowserContext* browser_context,
    DisposeCallback callback) {
  HeadlessBrowserContextImpl* context =
      HeadlessBrowserContextImpl::From(browser_context);
  std::vector<HeadlessWebContents*> web_contents = context->GetAllWebContents();
  while (!web_contents.empty()) {
    for (auto* wc : web_contents)
      wc->Close();
    // Since HeadlessWebContents::Close spawns a nested run loop to await
    // closing, new web_contents could be opened. We need to re-query pages and
    // close them too.
    web_contents = context->GetAllWebContents();
  }
  context->Close();
  std::move(callback).Run(true, "");
}

}  // namespace headless
