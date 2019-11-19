// Copyright 2018 The Chromium Authors. All rights reserved.
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

TargetHandler::TargetHandler(base::WeakPtr<HeadlessBrowserImpl> browser)
    : DomainHandler(Target::Metainfo::domainName, browser) {}

TargetHandler::~TargetHandler() = default;

void TargetHandler::Wire(UberDispatcher* dispatcher) {
  Target::Dispatcher::wire(dispatcher, this);
}

Response TargetHandler::CreateTarget(const std::string& url,
                                     Maybe<int> width,
                                     Maybe<int> height,
                                     Maybe<std::string> context_id,
                                     Maybe<bool> enable_begin_frame_control,
                                     Maybe<bool> new_window,
                                     Maybe<bool> background,
                                     std::string* out_target_id) {
#if defined(OS_MACOSX)
  if (enable_begin_frame_control.fromMaybe(false))
    return Response::Error("BeginFrameControl is not supported on MacOS yet");
#endif

  HeadlessBrowserContext* context;
  if (context_id.isJust()) {
    context = browser()->GetBrowserContextForId(context_id.fromJust());
    if (!context)
      return Response::InvalidParams("browserContextId");
  } else {
    context = browser()->GetDefaultBrowserContext();
    if (!context) {
      return Response::Error(
          "You specified no |browserContextId|, but "
          "there is no default browser context set on "
          "HeadlessBrowser");
    }
  }

  HeadlessWebContentsImpl* web_contents_impl = HeadlessWebContentsImpl::From(
      context->CreateWebContentsBuilder()
          .SetInitialURL(GURL(url))
          .SetWindowSize(gfx::Size(
              width.fromMaybe(browser()->options()->window_size.width()),
              height.fromMaybe(browser()->options()->window_size.height())))
          .SetEnableBeginFrameControl(
              enable_begin_frame_control.fromMaybe(false))
          .Build());

  *out_target_id = web_contents_impl->GetDevToolsAgentHostId();
  return Response::OK();
}

Response TargetHandler::CloseTarget(const std::string& target_id,
                                    bool* out_success) {
  HeadlessWebContents* web_contents =
      browser()->GetWebContentsForDevToolsAgentHostId(target_id);
  *out_success = false;
  if (web_contents) {
    web_contents->Close();
    *out_success = true;
  }
  return Response::OK();
}
}  // namespace protocol
}  // namespace headless
