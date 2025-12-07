// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/devtools_manager_delegate.h"
#include "base/values.h"
#include "content/public/browser/devtools_agent_host.h"

namespace content {

DevToolsManagerDelegate::DevToolsOptions::DevToolsOptions() = default;

DevToolsManagerDelegate::DevToolsOptions::DevToolsOptions(
    const DevToolsManagerDelegate::DevToolsOptions& other) = default;

DevToolsManagerDelegate::DevToolsOptions::DevToolsOptions(
    std::optional<std::string> panel_id)
    : panel_id(panel_id) {}

DevToolsManagerDelegate::DevToolsOptions::~DevToolsOptions() = default;

void DevToolsManagerDelegate::Inspect(DevToolsAgentHost* agent_host) {
}

scoped_refptr<DevToolsAgentHost> DevToolsManagerDelegate::GetDevToolsAgentHost(
    DevToolsAgentHost* agent_host) {
  return nullptr;
}

scoped_refptr<DevToolsAgentHost> DevToolsManagerDelegate::OpenDevTools(
    DevToolsAgentHost* agent_host,
    const DevToolsManagerDelegate::DevToolsOptions& devtools_options) {
  return nullptr;
}

void DevToolsManagerDelegate::Activate(DevToolsAgentHost* agent_host) {}

std::string DevToolsManagerDelegate::GetTargetType(WebContents* wc) {
  return std::string();
}

std::string DevToolsManagerDelegate::GetTargetTitle(WebContents* wc) {
  return std::string();
}

std::string DevToolsManagerDelegate::GetTargetDescription(WebContents* wc) {
  return std::string();
}

bool DevToolsManagerDelegate::AllowInspectingRenderFrameHost(
    RenderFrameHost* rfh) {
  return true;
}

std::optional<bool> DevToolsManagerDelegate::ShouldReportAsTabTarget(
    WebContents* web_contents) {
  return std::nullopt;
}

DevToolsAgentHost::List DevToolsManagerDelegate::RemoteDebuggingTargets(
    DevToolsManagerDelegate::TargetType target_type) {
  return DevToolsAgentHost::GetOrCreateAll();
}

scoped_refptr<DevToolsAgentHost> DevToolsManagerDelegate::CreateNewTarget(
    const GURL& url,
    DevToolsManagerDelegate::TargetType target_type,
    bool new_window) {
  return nullptr;
}

std::vector<BrowserContext*> DevToolsManagerDelegate::GetBrowserContexts() {
  return std::vector<BrowserContext*>();
}

BrowserContext* DevToolsManagerDelegate::GetDefaultBrowserContext() {
  return nullptr;
}

BrowserContext* DevToolsManagerDelegate::CreateBrowserContext() {
  return nullptr;
}

void DevToolsManagerDelegate::DisposeBrowserContext(BrowserContext*,
                                                    DisposeCallback callback) {
  std::move(callback).Run(false, "Browser Context disposal is not supported");
}

void DevToolsManagerDelegate::ClientAttached(
    DevToolsAgentHostClientChannel* channel) {}
void DevToolsManagerDelegate::ClientDetached(
    DevToolsAgentHostClientChannel* channel) {}

void DevToolsManagerDelegate::HandleCommand(
    DevToolsAgentHostClientChannel* channel,
    base::span<const uint8_t> message,
    NotHandledCallback callback) {
  std::move(callback).Run(message);
}

std::string DevToolsManagerDelegate::GetDiscoveryPageHTML() {
  return std::string();
}

bool DevToolsManagerDelegate::HasBundledFrontendResources() {
  return false;
}

bool DevToolsManagerDelegate::IsBrowserTargetDiscoverable() {
  return false;
}

void DevToolsManagerDelegate::AcceptDebugging(AcceptCallback callback) {
  std::move(callback).Run(
      content::DevToolsManagerDelegate::AcceptConnectionResult::kDeny);
}

void DevToolsManagerDelegate::SetActiveWebSocketConnections(size_t count) {}

DevToolsManagerDelegate::~DevToolsManagerDelegate() = default;
}  // namespace content
