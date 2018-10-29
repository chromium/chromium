// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/lib/browser/protocol/browser_handler.h"

#include "base/task/post_task.h"
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

BrowserHandler::BrowserHandler(base::WeakPtr<HeadlessBrowserImpl> browser)
    : DomainHandler(Browser::Metainfo::domainName, browser) {}

BrowserHandler::~BrowserHandler() = default;

void BrowserHandler::Wire(UberDispatcher* dispatcher) {
  Browser::Dispatcher::wire(dispatcher, this);
}

Response BrowserHandler::GetWindowForTarget(
    const std::string& target_id,
    int* out_window_id,
    std::unique_ptr<Browser::Bounds>* out_bounds) {
  HeadlessWebContentsImpl* web_contents = HeadlessWebContentsImpl::From(
      browser()->GetWebContentsForDevToolsAgentHostId(target_id));
  if (!web_contents)
    return Response::Error("No web contents for the given target id");

  auto result = std::make_unique<base::DictionaryValue>();
  *out_window_id = web_contents->window_id();
  *out_bounds = CreateBrowserBounds(web_contents);
  return Response::OK();
}

Response BrowserHandler::GetWindowBounds(
    int window_id,
    std::unique_ptr<Browser::Bounds>* out_bounds) {
  HeadlessWebContentsImpl* web_contents =
      browser()->GetWebContentsForWindowId(window_id);
  if (!web_contents)
    return Response::Error("Browser window not found");
  *out_bounds = CreateBrowserBounds(web_contents);
  return Response::OK();
}

Response BrowserHandler::Close() {
  base::PostTaskWithTraits(
      FROM_HERE, {content::BrowserThread::UI},
      base::BindOnce(&HeadlessBrowserImpl::Shutdown, browser()));
  return Response::OK();
}

Response BrowserHandler::SetWindowBounds(
    int window_id,
    std::unique_ptr<Browser::Bounds> window_bounds) {
  HeadlessWebContentsImpl* web_contents = web_contents =
      browser()->GetWebContentsForWindowId(window_id);
  if (!web_contents)
    return Response::Error("Browser window not found");

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
    return Response::Error(
        "The 'minimized', 'maximized' and 'fullscreen' states cannot be "
        "combined with 'left', 'top', 'width' or 'height'");
  }

  if (set_bounds && web_contents->window_state() != "normal") {
    return Response::Error(
        "To resize minimized/maximized/fullscreen window, restore it to normal "
        "state first.");
  }

  web_contents->set_window_state(window_state);
  web_contents->SetBounds(bounds);
  return Response::OK();
}

}  // namespace protocol
}  // namespace headless
