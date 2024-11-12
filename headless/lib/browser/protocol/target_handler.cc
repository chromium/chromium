// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/lib/browser/protocol/target_handler.h"

#include "build/build_config.h"
#include "headless/lib/browser/headless_browser_context_impl.h"
#include "headless/lib/browser/headless_browser_impl.h"
#include "headless/lib/browser/headless_web_contents_impl.h"
#include "ui/gfx/geometry/size.h"

namespace headless {
namespace protocol {

TargetHandler::TargetHandler(HeadlessBrowserImpl* browser)
    : browser_(browser) {}

TargetHandler::~TargetHandler() = default;

void TargetHandler::Wire(UberDispatcher* dispatcher) {
  Target::Dispatcher::wire(dispatcher, this);
}

Response TargetHandler::Disable() {
  return Response::Success();
}

Response TargetHandler::CreateTarget(const std::string& url,
                                     Maybe<int> width,
                                     Maybe<int> height,
                                     Maybe<std::string> context_id,
                                     Maybe<bool> enable_begin_frame_control,
                                     Maybe<bool> new_window,
                                     Maybe<bool> background,
                                     Maybe<bool> for_tab,
                                     std::string* out_target_id) {
#if BUILDFLAG(IS_MAC)
  if (enable_begin_frame_control.value_or(false)) {
    return Response::ServerError(
        "BeginFrameControl is not supported on MacOS yet");
  }
#endif

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

  HeadlessWebContentsImpl* web_contents_impl = HeadlessWebContentsImpl::From(
      context->CreateWebContentsBuilder()
          .SetInitialURL(gurl)
          .SetWindowSize(gfx::Size(
              width.value_or(browser_->options()->window_size.width()),
              height.value_or(browser_->options()->window_size.height())))
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
