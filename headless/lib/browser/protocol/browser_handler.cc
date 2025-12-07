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
#include "headless/lib/browser/protocol/target_handler.h"
#include "headless/public/headless_window_state.h"

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
      .SetWindowState(GetProtocolWindowState(web_contents->GetWindowState()))
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
    std::optional<std::string> target_id,
    int* out_window_id,
    std::unique_ptr<Browser::Bounds>* out_bounds) {
  auto agent_host =
      content::DevToolsAgentHost::GetForId(target_id.value_or(target_id_));
  HeadlessWebContentsImpl* web_contents =
      HeadlessWebContentsImpl::From(agent_host->GetWebContents());
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

  const bool set_bounds = window_bounds->HasLeft() || window_bounds->HasTop() ||
                          window_bounds->HasWidth() ||
                          window_bounds->HasHeight();

  std::optional<HeadlessWindowState> headless_window_state;
  if (window_bounds->HasWindowState()) {
    std::string protocol_window_state = window_bounds->GetWindowState().value();
    headless_window_state = GetWindowStateFromProtocol(protocol_window_state);
    if (!headless_window_state) {
      return Response::InvalidParams("Invalid window state: " +
                                     protocol_window_state);
    }

    if (set_bounds && headless_window_state != HeadlessWindowState::kNormal) {
      return Response::InvalidParams(
          "The 'minimized', 'maximized' and 'fullscreen' states cannot be "
          "combined with 'left', 'top', 'width' or 'height'");
    }
  }

  if (set_bounds &&
      web_contents->GetWindowState() != HeadlessWindowState::kNormal) {
    return Response::ServerError(
        "To resize minimized/maximized/fullscreen window, restore it to normal "
        "state first.");
  }

  // Set the requested or normal window state. Note that this will update window
  // position according to the requested window state if necessary.
  web_contents->SetWindowState(
      headless_window_state.value_or(HeadlessWindowState::kNormal));

  if (set_bounds) {
    gfx::Rect bounds = web_contents->web_contents()->GetContainerBounds();

    bounds.set_x(window_bounds->GetLeft(bounds.x()));
    bounds.set_y(window_bounds->GetTop(bounds.y()));
    bounds.set_width(window_bounds->GetWidth(bounds.width()));
    bounds.set_height(window_bounds->GetHeight(bounds.height()));

    web_contents->SetBounds(bounds);
  }

  return Response::Success();
}

Response BrowserHandler::SetContentsSize(int window_id,
                                         std::optional<int> width,
                                         std::optional<int> height) {
  HeadlessWebContentsImpl* web_contents =
      browser_->GetWebContentsForWindowId(window_id);
  if (!web_contents) {
    return Response::ServerError("Browser window not found");
  }

  if (web_contents->GetWindowState() != HeadlessWindowState::kNormal) {
    return Response::ServerError(
        "Restore window to normal state before setting content size");
  }

  if (!width && !height) {
    return Response::InvalidParams(
        "At least one of 'width' or 'height' must be specified");
  }

  if (width && width.value() <= 0) {
    return Response::InvalidParams("Contents 'width' must be a positive value");
  }

  if (height && height.value() <= 0) {
    return Response::InvalidParams(
        "Contents 'height' must be a positive value");
  }

  gfx::Rect bounds = web_contents->web_contents()->GetContainerBounds();
  bounds.set_width(width.value_or(bounds.width()));
  bounds.set_height(height.value_or(bounds.height()));

  web_contents->SetBounds(bounds);

  return Response::Success();
}

protocol::Response BrowserHandler::SetDockTile(
    std::optional<std::string> label,
    std::optional<protocol::Binary> image) {
  return Response::Success();
}

}  // namespace protocol
}  // namespace headless
