// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_MANAGER_ANDROID_H_
#define CONTENT_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_MANAGER_ANDROID_H_

#include "content/browser/accessibility/browser_accessibility_manager.h"

namespace ui {
class MotionEventAndroid;
}

namespace content {

// A Java counterpart will be generated for this enum.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.content.browser.accessibility
enum ScrollDirection { FORWARD, BACKWARD, UP, DOWN, LEFT, RIGHT };

// From android.view.accessibility.AccessibilityNodeInfo in Java:
enum AndroidMovementGranularity {
  ANDROID_ACCESSIBILITY_NODE_INFO_MOVEMENT_GRANULARITY_CHARACTER = 1,
  ANDROID_ACCESSIBILITY_NODE_INFO_MOVEMENT_GRANULARITY_WORD = 2,
  ANDROID_ACCESSIBILITY_NODE_INFO_MOVEMENT_GRANULARITY_LINE = 4
};

// From android.view.accessibility.AccessibilityEvent in Java:
enum {
  ANDROID_ACCESSIBILITY_EVENT_TEXT_CHANGED = 16,
  ANDROID_ACCESSIBILITY_EVENT_TEXT_SELECTION_CHANGED = 8192,
  ANDROID_ACCESSIBILITY_EVENT_TEXT_TRAVERSED_AT_MOVEMENT_GRANULARITY = 131072
};

class BrowserAccessibilityAndroid;
class WebContentsAccessibilityAndroid;

class CONTENT_EXPORT BrowserAccessibilityManagerAndroid
    : public BrowserAccessibilityManager {
 public:
  BrowserAccessibilityManagerAndroid(
      const ui::AXTreeUpdate& initial_tree,
      WebContentsAccessibilityAndroid* web_contents_accessibility,
      BrowserAccessibilityDelegate* delegate,
      BrowserAccessibilityFactory* factory = new BrowserAccessibilityFactory());

  ~BrowserAccessibilityManagerAndroid() override;

  static ui::AXTreeUpdate GetEmptyDocument();

  // By default, the tree is pruned for a better screen reading experience,
  // including:
  //   * If the node has only static text children
  //   * If the node is focusable and has no focusable children
  //   * If the node is a heading
  // This can be turned off to generate a tree that more accurately reflects
  // the DOM and includes style changes within these nodes.
  void set_prune_tree_for_screen_reader(bool prune) {
    prune_tree_for_screen_reader_ = prune;
  }
  bool prune_tree_for_screen_reader() { return prune_tree_for_screen_reader_; }

  void set_web_contents_accessibility(WebContentsAccessibilityAndroid* wcax) {
    web_contents_accessibility_ = wcax;
  }

  bool ShouldRespectDisplayedPasswordText();
  bool ShouldExposePasswordText();

  // Consume hover event if necessary, and return true if it did.
  bool OnHoverEvent(const ui::MotionEventAndroid& event);

  // BrowserAccessibilityManager overrides.
  BrowserAccessibility* GetFocus() const override;
  void SendLocationChangeEvents(
      const std::vector<AccessibilityHostMsg_LocationChangeParams>& params)
      override;
  void FireFocusEvent(BrowserAccessibility* node) override;
  void FireBlinkEvent(ax::mojom::Event event_type,
                      BrowserAccessibility* node) override;
  void FireGeneratedEvent(ui::AXEventGenerator::Event event_type,
                          BrowserAccessibility* node) override;
  gfx::Rect GetViewBounds() override;

  void FireLocationChanged(BrowserAccessibility* node);

  // Helper functions to compute the next start and end index when moving
  // forwards or backwards by character, word, or line. This part is
  // unit-tested; the Java interfaces above are just wrappers. Both of these
  // take a single cursor index as input and return the boundaries surrounding
  // the next word or line. If moving by character, the output start and
  // end index will be the same.
  bool NextAtGranularity(int32_t granularity,
                         int cursor_index,
                         BrowserAccessibilityAndroid* node,
                         int32_t* start_index,
                         int32_t* end_index);
  bool PreviousAtGranularity(int32_t granularity,
                             int cursor_index,
                             BrowserAccessibilityAndroid* node,
                             int32_t* start_index,
                             int32_t* end_index);

 private:
  // AXTreeObserver overrides.
  void OnAtomicUpdateFinished(
      ui::AXTree* tree,
      bool root_changed,
      const std::vector<ui::AXTreeObserver::Change>& changes) override;

  bool UseRootScrollOffsetsWhenComputingBounds() override;

  WebContentsAccessibilityAndroid* GetWebContentsAXFromRootManager();

  // This gives BrowserAccessibilityManager::Create access to the class
  // constructor.
  friend class BrowserAccessibilityManager;

  // Handle a hover event from the renderer process.
  void HandleHoverEvent(BrowserAccessibility* node);

  // Pointer to WebContentsAccessibility for reaching Java layer.
  // Only the root manager has the reference. Should be accessed through
  // |GetWebContentsAXFromRootManager| rather than directly.
  WebContentsAccessibilityAndroid* web_contents_accessibility_;

  // See docs for set_prune_tree_for_screen_reader, above.
  bool prune_tree_for_screen_reader_;

  DISALLOW_COPY_AND_ASSIGN(BrowserAccessibilityManagerAndroid);
};

}  // namespace content

#endif  // CONTENT_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_MANAGER_ANDROID_H_
