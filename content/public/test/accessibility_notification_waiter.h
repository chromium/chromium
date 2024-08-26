// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_ACCESSIBILITY_NOTIFICATION_WAITER_H_
#define CONTENT_PUBLIC_TEST_ACCESSIBILITY_NOTIFICATION_WAITER_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/accessibility/ax_event_generator.h"
#include "ui/accessibility/ax_mode.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/ax_node_id_forward.h"
#include "ui/accessibility/ax_tree.h"

namespace base {
class RunLoop;
}

namespace ui {
class BrowserAccessibilityManager;
}

namespace content {

class RenderFrameHost;
class RenderFrameHostImpl;
class WebContents;

// Create an instance of this class *before* doing any operation that
// might generate an accessibility event (like a page navigation or
// clicking on a button). Then call WaitForNotification
// afterwards to block until the specified accessibility notification has been
// received.
class AccessibilityNotificationWaiter : public WebContentsObserver {
 public:
  // Will wait for any event across all ways including scroll or location
  // changes as well normal and generated events.
  explicit AccessibilityNotificationWaiter(WebContents* web_contents);

  // Wait for a specific Blink event.
  AccessibilityNotificationWaiter(WebContents* web_contents,
                                  ui::AXMode accessibility_mode,
                                  ax::mojom::Event event);

  // Wait for a specific AXEventGenerator event.
  AccessibilityNotificationWaiter(WebContents* web_contents,
                                  ui::AXMode accessibility_mode,
                                  ui::AXEventGenerator::Event event);

  AccessibilityNotificationWaiter(const AccessibilityNotificationWaiter&) =
      delete;
  AccessibilityNotificationWaiter& operator=(
      const AccessibilityNotificationWaiter&) = delete;

  ~AccessibilityNotificationWaiter() override;

  // Blocks until the specific accessibility notification registered in
  // AccessibilityNotificationWaiter is received. Returns true if an event was
  // received, false if waiting ended for some other reason.
  // Pass true for |all_frames| to wait for a notification on all frames
  // before returning, rather than waiting for only a single notification
  // from any frame.
  [[nodiscard]] bool WaitForNotification(bool all_frames = false);

  // Blocks until the notification is received, or the given timeout passes.
  // Returns true if an event was received, false if waiting ended for some
  // other reason.
  [[nodiscard]] bool WaitForNotificationWithTimeout(base::TimeDelta timeout);

  // After WaitForNotification has returned, this will retrieve
  // the tree of accessibility nodes received from the renderer process for
  // the observed WebContents (not including the trees of inner WebContents).
  const ui::AXTree& GetAXTree() const;

  // After WaitForNotification returns, use this to retrieve the id of the
  // node that was the target of the event.
  int event_target_id() const { return event_target_id_; }

  bool notification_received() const { return notification_received_; }

  // After WaitForNotification returns, use this to retrieve the
  // `BrowserAccessibilityManager` that was the target of the event.
  ui::BrowserAccessibilityManager* event_browser_accessibility_manager() const {
    return event_browser_accessibility_manager_;
  }

  // WebContentsObserver override:
  void RenderFrameHostChanged(RenderFrameHost* old_host,
                              RenderFrameHost* new_host) override;

  // Quits listening and unblocks WaitForNotification* calls.
  void Quit();

 private:
  // Listen to all frames within the frame tree of this WebContents.
  void ListenToAllFrames(WebContents* web_contents);

  // Bind either the OnAccessibilityEvent or OnGeneratedEvent callback
  // for a given frame within the WebContent's frame tree.
  void ListenToFrame(RenderFrameHostImpl* frame_host);

  // Helper to bind the OnAccessibilityEvent callback.
  void BindOnAccessibilityEvent(RenderFrameHostImpl* frame_host);

  // Helper to bind the OnGeneratedEvent callback.
  void BindOnGeneratedEvent(RenderFrameHostImpl* frame_host);

  // Helper to bind the OnLocationsChanged callback.
  void BindOnLocationsChanged(RenderFrameHostImpl* frame_host);

  // Callback from RenderViewHostImpl.
  void OnAccessibilityEvent(RenderFrameHostImpl* rfhi,
                            ax::mojom::Event event,
                            int event_target_id);

  // Callback from BrowserAccessibilityManager for all generated events.
  void OnGeneratedEvent(ui::BrowserAccessibilityManager* manager,
                        ui::AXEventGenerator::Event event,
                        ui::AXNodeID event_target_id);

  // Callback from BrowserAccessibilityManager when locations / bounding
  // boxes change.
  void OnLocationsChanged();

  // Callback from BrowserAccessibilityManager for the focus changed event.
  //
  // TODO(crbug.com/41470112): Remove this method once we migrate to using
  // AXEventGenerator for focus changed events.
  void OnFocusChanged();

  // Returns the tree of accessibility nodes received from renderer processes
  // for the WebContents that owns `render_frame`. This may not be the observed
  // WebContents, but rather an inner WebContents (e.g., for a guest view).
  const ui::AXTree& GetAXTreeForFrame(RenderFrameHostImpl* render_frame) const;

  std::optional<ax::mojom::Event> event_to_wait_for_;
  std::optional<ui::AXEventGenerator::Event> generated_event_to_wait_for_;
  std::unique_ptr<base::RunLoop> loop_runner_;
  base::RepeatingClosure loop_runner_quit_closure_;
  int event_target_id_ = 0;
  raw_ptr<ui::BrowserAccessibilityManager, AcrossTasksDanglingUntriaged>
      event_browser_accessibility_manager_ = nullptr;
  bool notification_received_ = false;
  int frame_count_ = 0;
  int notification_count_ = 0;
  bool wait_for_any_event_ = false;

  base::WeakPtrFactory<AccessibilityNotificationWaiter> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_ACCESSIBILITY_NOTIFICATION_WAITER_H_
