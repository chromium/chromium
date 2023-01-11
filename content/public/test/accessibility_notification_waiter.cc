// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/accessibility_notification_waiter.h"

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "content/browser/accessibility/browser_accessibility.h"
#include "content/browser/accessibility/browser_accessibility_manager.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/browser_accessibility_state.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_plugin_guest_manager.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/test_utils.h"
#include "ui/accessibility/ax_node.h"

namespace content {

AccessibilityNotificationWaiter::AccessibilityNotificationWaiter(
    WebContents* web_contents)
    : WebContentsObserver(web_contents),
      event_to_wait_for_(ax::mojom::Event::kNone),
      generated_event_to_wait_for_(absl::nullopt),
      loop_runner_(std::make_unique<base::RunLoop>()),
      loop_runner_quit_closure_(loop_runner_->QuitClosure()) {
  ListenToAllFrames(web_contents);
}

AccessibilityNotificationWaiter::AccessibilityNotificationWaiter(
    WebContents* web_contents,
    ui::AXMode accessibility_mode,
    ax::mojom::Event event_type)
    : WebContentsObserver(web_contents),
      event_to_wait_for_(event_type),
      generated_event_to_wait_for_(absl::nullopt),
      loop_runner_(std::make_unique<base::RunLoop>()),
      loop_runner_quit_closure_(loop_runner_->QuitClosure()) {
  ListenToAllFrames(web_contents);
  static_cast<WebContentsImpl*>(web_contents)
      ->AddAccessibilityMode(accessibility_mode);
  // Add the the accessibility mode on BrowserAccessibilityState so it can be
  // also be added to AXPlatformNode, auralinux uses this to determine if it
  // should enable accessibility or not.
  BrowserAccessibilityState::GetInstance()->AddAccessibilityModeFlags(
      accessibility_mode);
}

AccessibilityNotificationWaiter::AccessibilityNotificationWaiter(
    WebContents* web_contents,
    ui::AXMode accessibility_mode,
    ui::AXEventGenerator::Event event_type)
    : WebContentsObserver(web_contents),
      event_to_wait_for_(absl::nullopt),
      generated_event_to_wait_for_(event_type),
      loop_runner_(std::make_unique<base::RunLoop>()),
      loop_runner_quit_closure_(loop_runner_->QuitClosure()) {
  ListenToAllFrames(web_contents);
  static_cast<WebContentsImpl*>(web_contents)
      ->AddAccessibilityMode(accessibility_mode);
  // Add the the accessibility mode on BrowserAccessibilityState so it can be
  // also be added to AXPlatformNode, auralinux uses this to determine if it
  // should enable accessibility or not.
  BrowserAccessibilityState::GetInstance()->AddAccessibilityModeFlags(
      accessibility_mode);
}

AccessibilityNotificationWaiter::~AccessibilityNotificationWaiter() = default;

void AccessibilityNotificationWaiter::ListenToAllFrames(
    WebContents* web_contents) {
  if (event_to_wait_for_)
    VLOG(1) << "Waiting for AccessibilityEvent " << *event_to_wait_for_;
  WebContentsImpl* web_contents_impl =
      static_cast<WebContentsImpl*>(web_contents);
  for (FrameTreeNode* node : web_contents_impl->GetPrimaryFrameTree().Nodes())
    ListenToFrame(node->current_frame_host());

  BrowserPluginGuestManager* guest_manager =
      web_contents_impl->GetBrowserContext()->GetGuestManager();
  if (guest_manager) {
    guest_manager->ForEachGuest(
        web_contents_impl,
        base::BindRepeating(
            &AccessibilityNotificationWaiter::ListenToGuestWebContents,
            base::Unretained(this)));
  }
}

bool AccessibilityNotificationWaiter::ListenToGuestWebContents(
    WebContents* web_contents) {
  ListenToAllFrames(web_contents);
  return true;
}

void AccessibilityNotificationWaiter::ListenToFrame(
    RenderFrameHostImpl* frame_host) {
  if (event_to_wait_for_)
    BindOnAccessibilityEvent(frame_host);
  if (generated_event_to_wait_for_)
    BindOnGeneratedEvent(frame_host);

  if (event_to_wait_for_ == ax::mojom::Event::kNone ||
      event_to_wait_for_ == ax::mojom::Event::kLocationChanged) {
    BindOnLocationsChanged(frame_host);
  }
}

bool AccessibilityNotificationWaiter::WaitForNotification() {
  loop_runner_->Run();

  bool notification_received = notification_received_;
  // Reset everything to allow reuse.
  // Each loop runner can only be called once. Create a new one in case
  // the caller wants to call this again to wait for the next notification.
  loop_runner_ = std::make_unique<base::RunLoop>();
  loop_runner_quit_closure_ = loop_runner_->QuitClosure();
  notification_received_ = false;

  return notification_received;
}

bool AccessibilityNotificationWaiter::WaitForNotificationWithTimeout(
    base::TimeDelta timeout) {
  base::OneShotTimer quit_timer;
  quit_timer.Start(FROM_HERE, timeout, loop_runner_->QuitWhenIdleClosure());

  return WaitForNotification();
}

const ui::AXTree& AccessibilityNotificationWaiter::GetAXTree() const {
  static base::NoDestructor<ui::AXTree> empty_tree;
  WebContentsImpl* web_contents_impl =
      static_cast<WebContentsImpl*>(web_contents());
  BrowserAccessibilityManager* manager =
      web_contents_impl->GetRootBrowserAccessibilityManager();
  return manager && manager->ax_tree() ? *manager->ax_tree() : *empty_tree;
}

void AccessibilityNotificationWaiter::BindOnAccessibilityEvent(
    RenderFrameHostImpl* frame_host) {
  frame_host->SetAccessibilityCallbackForTesting(base::BindRepeating(
      &AccessibilityNotificationWaiter::OnAccessibilityEvent,
      weak_factory_.GetWeakPtr()));
}

void AccessibilityNotificationWaiter::OnAccessibilityEvent(
    RenderFrameHostImpl* rfhi,
    ax::mojom::Event event_type,
    int event_target_id) {
  if (IsAboutBlank())
    return;

  VLOG(1) << "OnAccessibilityEvent " << event_type;

  if (event_to_wait_for_ == ax::mojom::Event::kNone ||
      event_to_wait_for_ == event_type) {
    event_target_id_ = event_target_id;
    event_render_frame_host_ = rfhi;
    notification_received_ = true;

    loop_runner_quit_closure_.Run();
  }
}

void AccessibilityNotificationWaiter::BindOnGeneratedEvent(
    RenderFrameHostImpl* frame_host) {
  if (auto* manager = frame_host->GetOrCreateBrowserAccessibilityManager()) {
    manager->SetGeneratedEventCallbackForTesting(
        base::BindRepeating(&AccessibilityNotificationWaiter::OnGeneratedEvent,
                            weak_factory_.GetWeakPtr()));
    manager->SetFocusChangeCallbackForTesting(
        base::BindRepeating(&AccessibilityNotificationWaiter::OnFocusChanged,
                            weak_factory_.GetWeakPtr()));
  }
}

void AccessibilityNotificationWaiter::BindOnLocationsChanged(
    RenderFrameHostImpl* frame_host) {
  if (auto* manager = frame_host->browser_accessibility_manager()) {
    manager->SetLocationChangeCallbackForTesting(base::BindRepeating(
        &AccessibilityNotificationWaiter::OnLocationsChanged,
        weak_factory_.GetWeakPtr()));
  }
}

void AccessibilityNotificationWaiter::OnGeneratedEvent(
    RenderFrameHostImpl* render_frame_host,
    ui::AXEventGenerator::Event event,
    ui::AXNodeID event_target_id) {
  DCHECK(render_frame_host);
  DCHECK_NE(event_target_id, ui::kInvalidAXNodeID);

  if (IsAboutBlank())
    return;

  if (generated_event_to_wait_for_ == event) {
    event_target_id_ = event_target_id;
    event_render_frame_host_ = render_frame_host;
    notification_received_ = true;
    loop_runner_quit_closure_.Run();
  }
}

void AccessibilityNotificationWaiter::OnLocationsChanged() {
  if (IsAboutBlank())
    return;

  notification_received_ = true;
  loop_runner_quit_closure_.Run();
}

// TODO(982776): Remove this method once we migrate to using AXEventGenerator
// for focus changed events.
void AccessibilityNotificationWaiter::OnFocusChanged() {
  WebContentsImpl* web_contents_impl =
      static_cast<WebContentsImpl*>(web_contents());
  const BrowserAccessibilityManager* manager =
      web_contents_impl->GetRootBrowserAccessibilityManager();
  if (manager && manager->delegate() && manager->GetFocus()) {
    OnGeneratedEvent(manager->delegate()->AccessibilityRenderFrameHost(),
                     ui::AXEventGenerator::Event::FOCUS_CHANGED,
                     manager->GetFocus()->GetId());
  }
}

bool AccessibilityNotificationWaiter::IsAboutBlank() {
  // Skip any accessibility notifications related to "about:blank",
  // to avoid a possible race condition between the test beginning
  // listening for accessibility events and "about:blank" loading.
  return GetAXTree().data().url == url::kAboutBlankURL;
}

// WebContentsObserver override:
void AccessibilityNotificationWaiter::RenderFrameHostChanged(
    RenderFrameHost* old_host,
    RenderFrameHost* new_host) {
  ListenToFrame(static_cast<RenderFrameHostImpl*>(new_host));
}

void AccessibilityNotificationWaiter::Quit() {
  loop_runner_quit_closure_.Run();
}

}  // namespace content
