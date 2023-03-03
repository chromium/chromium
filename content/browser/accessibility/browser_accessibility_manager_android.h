// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_MANAGER_ANDROID_H_
#define CONTENT_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_MANAGER_ANDROID_H_

#include <unordered_set>
#include <utility>

#include "content/browser/accessibility/browser_accessibility_manager.h"
#include "content/common/content_export.h"
#include "third_party/blink/public/mojom/render_accessibility.mojom-forward.h"

namespace ui {

class MotionEventAndroid;

}  // namespace ui

namespace content {

class WebAXPlatformTreeManagerDelegate;

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
      base::WeakPtr<WebContentsAccessibilityAndroid> web_contents_accessibility,
      WebAXPlatformTreeManagerDelegate* delegate);

  BrowserAccessibilityManagerAndroid(
      const BrowserAccessibilityManagerAndroid&) = delete;
  BrowserAccessibilityManagerAndroid& operator=(
      const BrowserAccessibilityManagerAndroid&) = delete;

  ~BrowserAccessibilityManagerAndroid() override;

  static ui::AXTreeUpdate GetEmptyDocument();

  void set_allow_image_descriptions_for_testing(bool is_allowed) {
    allow_image_descriptions_for_testing_ = is_allowed;
  }

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

  void set_web_contents_accessibility(
      base::WeakPtr<WebContentsAccessibilityAndroid> wcax) {
    web_contents_accessibility_ = std::move(wcax);
  }
  void ResetWebContentsAccessibility();

  // State properties defined from Java-side code.
  bool ShouldAllowImageDescriptions();
  bool ShouldRespectDisplayedPasswordText();
  bool ShouldExposePasswordText();

  // Consume hover event if necessary, and return true if it did.
  bool OnHoverEvent(const ui::MotionEventAndroid& event);

  // AXTreeManager overrides.
  void FireFocusEvent(ui::AXNode* node) override;

  // BrowserAccessibilityManager overrides.
  BrowserAccessibility* GetFocus() const override;
  void SendLocationChangeEvents(
      const std::vector<blink::mojom::LocationChangesPtr>& changes) override;
  ui::AXNode* RetargetForEvents(ui::AXNode* node,
                                RetargetEventType type) const override;
  void FireBlinkEvent(ax::mojom::Event event_type,
                      BrowserAccessibility* node,
                      int action_request_id) override;
  void FireGeneratedEvent(ui::AXEventGenerator::Event event_type,
                          const ui::AXNode* node) override;

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

  // Helper method to clear AccessibilityNodeInfo cache on given node
  void ClearNodeInfoCacheForGivenId(int32_t unique_id);

  // Only set on the root BrowserAccessibilityManager. Keeps track of if
  // any node uses touch passthrough in any frame - if so, any incoming
  // touch event needs to be processed for possible forwarding. This is
  // just an optimization; once touch passthrough is enabled it stays
  // on for this main frame until the page is reloaded. In the future if
  // there's a need to optimize for touch passthrough being enabled only
  // temporarily, this would need to be more sophisticated.
  void EnableTouchPassthrough() { touch_passthrough_enabled_ = true; }
  bool touch_passthrough_enabled() const { return touch_passthrough_enabled_; }

  std::u16string GenerateAccessibilityNodeInfoString(int32_t unique_id);

 private:
  // AXTreeObserver overrides.
  void OnAtomicUpdateFinished(
      ui::AXTree* tree,
      bool root_changed,
      const std::vector<ui::AXTreeObserver::Change>& changes) override;
  void OnNodeWillBeDeleted(ui::AXTree* tree, ui::AXNode* node) override;
  void OnNodeCreated(ui::AXTree* tree, ui::AXNode* node) override;
  void OnBoolAttributeChanged(ui::AXTree* tree,
                              ui::AXNode* node,
                              ax::mojom::BoolAttribute attr,
                              bool new_value) override;

  WebContentsAccessibilityAndroid* GetWebContentsAXFromRootManager();

  // This gives BrowserAccessibilityManager::Create access to the class
  // constructor.
  friend class BrowserAccessibilityManager;

  // Handle a hover event from the renderer process.
  void HandleHoverEvent(BrowserAccessibility* node);

  // A weak reference to WebContentsAccessibility for reaching Java layer.
  // Only the root manager has the reference. Should be accessed through
  // |GetWebContentsAXFromRootManager| rather than directly.
  base::WeakPtr<WebContentsAccessibilityAndroid> web_contents_accessibility_;

  // See docs for set_prune_tree_for_screen_reader, above.
  bool prune_tree_for_screen_reader_;

  // True if this instance should force enable the image descriptions feature
  // for testing. This allows us to mock generated image descriptions and test
  // tree dumps for nodes without creating web_contents_accessibility_android.
  bool allow_image_descriptions_for_testing_ = false;

  // Only set on the root BrowserAccessibilityManager. Keeps track of if
  // any node uses touch passthrough in any frame. See comment next to
  // any_node_uses_touch_passthrough(), above, for details.
  bool touch_passthrough_enabled_ = false;

  // An unordered_set of |unique_id| values for nodes cleared from the cache
  // with each atomic update to prevent superfluous cache clear calls.
  std::unordered_set<int32_t> nodes_already_cleared_ =
      std::unordered_set<int32_t>();
};

}  // namespace content

#endif  // CONTENT_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_MANAGER_ANDROID_H_
