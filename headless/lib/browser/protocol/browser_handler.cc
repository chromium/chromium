// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/lib/browser/protocol/browser_handler.h"

#include "base/functional/bind.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "headless/lib/browser/headless_browser_impl.h"
#include "headless/lib/browser/headless_web_contents_impl.h"

namespace headless {
namespace protocol {

namespace {

std::unique_ptr<Browser::Bounds> CreateBrowserBounds(
    const HeadlessWebContentsImpl* web_contents) {
  gfx::Rect bounds = web_contents->web_contents()->GetContainerBounds();
  return Browser::Bounds::Create()
      .SetLeft(bounds.x())
      .SetTop(bounds.y())
      .SetWidth(bounds.width())
      .SetHeight(bounds.height())
      .SetWindowState(web_contents->window_state())
      .Build();
}

}  // namespace

BrowserHandler::BrowserHandler(HeadlessBrowserImpl* browser,
                               const std::string& target_id)
    : browser_(browser), target_id_(target_id) {}

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
  HeadlessWebContentsImpl* web_contents = HeadlessWebContentsImpl::From(
      browser_->GetWebContentsForDevToolsAgentHostId(
          target_id.value_or(target_id_)));
  if (!web_contents)
    return Response::ServerError("No web contents for the given target id");

  *out_window_id = web_contents->window_id();
  *out_bounds = CreateBrowserBounds(web_contents);
  return Response::Success();
}

Response BrowserHandler::GetWindowBounds(
    int window_id,
    std::unique_ptr<Browser::Bounds>* out_bounds) {
  HeadlessWebContentsImpl* web_contents =
      browser_->GetWebContentsForWindowId(window_id);
  if (!web_contents)
    return Response::ServerError("Browser window not found");
  *out_bounds = CreateBrowserBounds(web_contents);
  return Response::Success();
}

Response BrowserHandler::Close() {
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&HeadlessBrowserImpl::Shutdown, browser_->GetWeakPtr()));
  return Response::Success();
}

Response BrowserHandler::SetWindowBounds(
    int window_id,
    std::unique_ptr<Browser::Bounds> window_bounds) {
  HeadlessWebContentsImpl* web_contents =
      browser_->GetWebContentsForWindowId(window_id);
  if (!web_contents)
    return Response::ServerError("Browser window not found");

  gfx::Rect bounds = web_contents->web_contents()->GetContainerBounds();
  const bool set_bounds = window_bounds->HasLeft() || window_bounds->HasTop() ||
                          window_bounds->HasWidth() ||
                          window_bounds->HasHeight();
  if (set_bounds) {
    bounds.set_x(window_bounds->GetLeft(bounds.x()));
    bounds.set_y(window_bounds->GetTop(bounds.y()));
    bounds.set_width(window_bounds->GetWidth(bounds.width()));
    bounds.set_height(window_bounds->GetHeight(bounds.height()));
  }

  const std::string window_state = window_bounds->GetWindowState("normal");
  if (set_bounds && window_state != "normal") {
    return Response::ServerError(
        "The 'minimized', 'maximized' and 'fullscreen' states cannot be "
        "combined with 'left', 'top', 'width' or 'height'");
  }

  if (set_bounds && web_contents->window_state() != "normal") {
    return Response::ServerError(
        "To resize minimized/maximized/fullscreen window, restore it to normal "
        "state first.");
  }

  web_contents->set_window_state(window_state);
  web_contents->SetBounds(bounds);
  return Response::Success();
}

protocol::Response BrowserHandler::SetDockTile(Maybe<std::string> label,
                                               Maybe<protocol::Binary> image) {
  return Response::Success();
}

}  // namespace protocol
}  // namespace headless
