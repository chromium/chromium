// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/accessibility_browser_test_utils.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/test_utils.h"
#include "ui/accessibility/ax_node.h"

namespace content {

AccessibilityNotificationWaiter::AccessibilityNotificationWaiter(
    WebContents* web_contents)
    : event_to_wait_for_(ax::mojom::Event::kNone),
      loop_runner_(new MessageLoopRunner()),
      event_target_id_(0),
      event_render_frame_host_(nullptr),
      weak_factory_(this) {
  frame_host_ = static_cast<RenderFrameHostImpl*>(
      web_contents->GetMainFrame());
  frame_host_->SetAccessibilityCallbackForTesting(
      base::Bind(&AccessibilityNotificationWaiter::OnAccessibilityEvent,
                 weak_factory_.GetWeakPtr()));
}

AccessibilityNotificationWaiter::AccessibilityNotificationWaiter(
    WebContents* web_contents,
    ui::AXMode accessibility_mode,
    ax::mojom::Event event_type)
    : event_to_wait_for_(event_type),
      loop_runner_(new MessageLoopRunner()),
      event_target_id_(0),
      weak_factory_(this) {
  WebContentsImpl* web_contents_impl = static_cast<WebContentsImpl*>(
      web_contents);
  frame_host_ = static_cast<RenderFrameHostImpl*>(
      web_contents_impl->GetMainFrame());
  frame_host_->SetAccessibilityCallbackForTesting(
      base::Bind(&AccessibilityNotificationWaiter::OnAccessibilityEvent,
                 weak_factory_.GetWeakPtr()));
  web_contents_impl->AddAccessibilityMode(accessibility_mode);
}

AccessibilityNotificationWaiter::AccessibilityNotificationWaiter(
    RenderFrameHostImpl* frame_host,
    ax::mojom::Event event_type)
    : frame_host_(frame_host),
      event_to_wait_for_(event_type),
      loop_runner_(new MessageLoopRunner()),
      event_target_id_(0),
      weak_factory_(this) {
  frame_host_->SetAccessibilityCallbackForTesting(
      base::Bind(&AccessibilityNotificationWaiter::OnAccessibilityEvent,
                 weak_factory_.GetWeakPtr()));
}

AccessibilityNotificationWaiter::~AccessibilityNotificationWaiter() {
}

void AccessibilityNotificationWaiter::ListenToAdditionalFrame(
    RenderFrameHostImpl* frame_host) {
  frame_host->SetAccessibilityCallbackForTesting(
      base::Bind(&AccessibilityNotificationWaiter::OnAccessibilityEvent,
                 weak_factory_.GetWeakPtr()));
}

void AccessibilityNotificationWaiter::WaitForNotification() {
  loop_runner_->Run();

  // Each loop runner can only be called once. Create a new one in case
  // the caller wants to call this again to wait for the next notification.
  loop_runner_ = new MessageLoopRunner();
}

const ui::AXTree& AccessibilityNotificationWaiter::GetAXTree() const {
  static base::NoDestructor<ui::AXTree> empty_tree;
  const ui::AXTree* tree = frame_host_->GetAXTreeForTesting();
  if (tree)
    return *tree;
  return *empty_tree;
}

void AccessibilityNotificationWaiter::OnAccessibilityEvent(
    RenderFrameHostImpl* rfhi,
    ax::mojom::Event event_type,
    int event_target_id) {
  if (!IsAboutBlank() && (event_to_wait_for_ == ax::mojom::Event::kNone ||
                          event_to_wait_for_ == event_type)) {
    event_target_id_ = event_target_id;
    event_render_frame_host_ = rfhi;
    loop_runner_->Quit();
  }
}

bool AccessibilityNotificationWaiter::IsAboutBlank() {
  // Skip any accessibility notifications related to "about:blank",
  // to avoid a possible race condition between the test beginning
  // listening for accessibility events and "about:blank" loading.
  return GetAXTree().data().url == url::kAboutBlankURL;
}

}  // namespace content
