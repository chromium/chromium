// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/browser/protocol/browser_handler.h"

#include "base/functional/bind.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/web_contents.h"

namespace content::shell::protocol {

namespace {

std::unique_ptr<Browser::Bounds> CreateBrowserBounds(
    WebContents* web_contents) {
  gfx::Rect bounds = web_contents->GetContainerBounds();
  return Browser::Bounds::Create()
      .SetLeft(bounds.x())
      .SetTop(bounds.y())
      .SetWidth(bounds.width())
      .SetHeight(bounds.height())
      .SetWindowState("normal")
      .Build();
}

}  // namespace

BrowserHandler::BrowserHandler(
    const raw_ref<const BrowserContext> browser_context,
    std::string target_id)
    : browser_context_(browser_context), target_id_(std::move(target_id)) {}

BrowserHandler::~BrowserHandler() = default;

void BrowserHandler::Wire(UberDispatcher* dispatcher) {
  Browser::Dispatcher::wire(dispatcher, this);
}

Response BrowserHandler::Disable() {
  return Response::Success();
}

Response BrowserHandler::GetWindowForTarget(
    Maybe<std::string> target_id,
    int* out_window_id,
    std::unique_ptr<Browser::Bounds>* out_bounds) {
  scoped_refptr<DevToolsAgentHost> agent_host =
      DevToolsAgentHost::GetForId(target_id.value_or(target_id_));
  if (!agent_host) {
    return Response::InvalidParams("No target with given id found");
  }

  WebContents* web_contents = agent_host->GetWebContents();
  if (!web_contents) {
    return Response::ServerError("No web contents for the given target id");
  }

  *out_window_id = 1;
  *out_bounds = CreateBrowserBounds(web_contents);
  return Response::Success();
}

}  // namespace content::shell::protocol
