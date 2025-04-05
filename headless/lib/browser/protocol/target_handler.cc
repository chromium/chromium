// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/lib/browser/protocol/target_handler.h"

#include <ranges>
#include <string_view>

#include "build/build_config.h"
#include "headless/lib/browser/headless_browser_context_impl.h"
#include "headless/lib/browser/headless_browser_impl.h"
#include "headless/lib/browser/headless_web_contents_impl.h"
#include "headless/public/headless_window_state.h"
#include "ui/gfx/geometry/rect.h"

namespace headless {
namespace protocol {

TargetHandler::TargetHandler(HeadlessBrowserImpl* browser)
    : browser_(browser) {}

TargetHandler::~TargetHandler() = default;

void TargetHandler::Wire(UberDispatcher* dispatcher) {
  Target::Dispatcher::wire(dispatcher, this);
}

Response TargetHandler::Disable() {
  while (!hidden_web_contents_.empty()) {
    // Destroy all existing hidden targets when session is closed. Some of them
    // can be already closed.
    auto target_id = *hidden_web_contents_.begin();
    auto agent_host = content::DevToolsAgentHost::GetForId(target_id);
    if (agent_host) {
      // The target is still alive, so destroy it.
      agent_host->Close();
    }
    hidden_web_contents_.erase(hidden_web_contents_.begin());
  }
  return Response::Success();
}

Response TargetHandler::CreateTarget(
    const std::string& url,
    std::optional<int> left,
    std::optional<int> top,
    std::optional<int> width,
    std::optional<int> height,
    std::optional<std::string> window_state,
    std::optional<std::string> context_id,
    std::optional<bool> enable_begin_frame_control,
    std::optional<bool> new_window,
    std::optional<bool> background,
    std::optional<bool> for_tab,
    std::optional<bool> hidden,
    std::string* out_target_id) {
#if BUILDFLAG(IS_MAC)
  if (enable_begin_frame_control.value_or(false)) {
    return Response::ServerError(
        "BeginFrameControl is not supported on MacOS yet");
  }
#endif

  std::optional<HeadlessWindowState> headless_window_state;
  if (window_state) {
    headless_window_state = GetWindowStateFromProtocol(*window_state);
    if (!headless_window_state) {
      return Response::InvalidParams("Invalid target window state: " +
                                     *window_state);
    }
  }

  HeadlessBrowserContext* context;
  if (context_id.has_value()) {
    context = browser_->GetBrowserContextForId(context_id.value());
    if (!context)
      return Response::InvalidParams("browserContextId");
  } else {
    context = browser_->GetDefaultBrowserContext();
    if (!context) {
      return Response::ServerError(
          "You specified no |browserContextId|, but "
          "there is no default browser context set on "
          "HeadlessBrowser");
    }
  }

  GURL gurl(url);
  if (gurl.is_empty()) {
    gurl = GURL(url::kAboutBlankURL);
  }

  if (hidden.value_or(false)) {
    if (for_tab.value_or(false)) {
      return protocol::Response::InvalidParams(
          "Hidden target cannot be created for tab");
    }
    if (new_window) {
      return protocol::Response::InvalidParams(
          "Hidden target cannot be created in a new window");
    }
    if (!background.value_or(true)) {
      return protocol::Response::InvalidParams(
          "Hidden target can be created only in background");
    }

    // Create a hidden target.
    HeadlessWebContentsImpl* web_contents_impl = HeadlessWebContentsImpl::From(
        context->CreateWebContentsBuilder().SetInitialURL(gurl).Build());

    *out_target_id = content::DevToolsAgentHost::GetOrCreateFor(
                         web_contents_impl->web_contents())
                         ->GetId();
    // Keep hidden target's ID in the hidden_web_contents_ to close it when the
    // session is closed.
    hidden_web_contents_.insert(*out_target_id);
    return Response::Success();
  }

  const gfx::Rect target_window_bounds(
      left.value_or(0), top.value_or(0),
      width.value_or(browser_->options()->window_size.width()),
      height.value_or(browser_->options()->window_size.height()));

  const HeadlessWindowState target_window_state =
      headless_window_state.value_or(HeadlessWindowState::kNormal);

  HeadlessWebContentsImpl* web_contents_impl = HeadlessWebContentsImpl::From(
      context->CreateWebContentsBuilder()
          .SetInitialURL(gurl)
          .SetWindowBounds(target_window_bounds)
          .SetWindowState(target_window_state)
          .SetEnableBeginFrameControl(
              enable_begin_frame_control.value_or(false))
          .Build());

  content::WebContents* wc = web_contents_impl->web_contents();
  auto devtools_agent_host =
      for_tab.value_or(false)
          ? content::DevToolsAgentHost::GetOrCreateForTab(wc)
          : content::DevToolsAgentHost::GetOrCreateFor(wc);
  *out_target_id = devtools_agent_host->GetId();
  return Response::Success();
}

Response TargetHandler::CloseTarget(const std::string& target_id,
                                    bool* out_success) {
  auto agent_host = content::DevToolsAgentHost::GetForId(target_id);
  if (!agent_host) {
    return Response::InvalidParams("No target found for targetId");
  }
  *out_success = agent_host->Close();
  return Response::Success();
}

}  // namespace protocol
}  // namespace headless
