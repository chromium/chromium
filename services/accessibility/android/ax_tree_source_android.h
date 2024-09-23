// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef SERVICES_ACCESSIBILITY_ANDROID_AX_TREE_SOURCE_ANDROID_H_
#define SERVICES_ACCESSIBILITY_ANDROID_AX_TREE_SOURCE_ANDROID_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "extensions/browser/api/automation_internal/automation_event_router.h"
#include "services/accessibility/android/accessibility_info_data_wrapper.h"
#include "services/accessibility/android/public/mojom/accessibility_helper.mojom-forward.h"
#include "ui/accessibility/ax_action_handler.h"
#include "ui/accessibility/ax_node.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/ax_tree_data.h"
#include "ui/accessibility/ax_tree_serializer.h"
#include "ui/accessibility/ax_tree_source.h"

namespace aura {
class Window;
}

namespace ax::android {
class AXTreeSourceAndroidTest;

using AXTreeAndroidSerializer = ui::AXTreeSerializer<
    AccessibilityInfoDataWrapper*,
    std::vector<raw_ptr<AccessibilityInfoDataWrapper, VectorExperimental>>,
    ui::AXTreeUpdate*,
    ui::AXTreeData*,
    ui::AXNodeData>;

// This class represents the accessibility tree from the focused ARC window.
class AXTreeSourceAndroid
    : public ui::AXTreeSource<AccessibilityInfoDataWrapper*,
                              ui::AXTreeData*,
                              ui::AXNodeData>,
      public ui::AXActionHandler {
 public:
  class Delegate {
   public:
    virtual void OnAction(const ui::AXActionData& data) const = 0;
    virtual bool UseFullFocusMode() const = 0;
  };

  class SerializationDelegate {
   public:
    virtual ~SerializationDelegate() = default;
    // Populate bounds of a node which can be passed to AXNodeData.location.
    // Bounds are returned in the following coordinates depending on whether
    // it's root or not.
    // - Root node is relative to its container, i.e. focused window.
    // - Non-root node is relative to the root node of this tree.
    virtual void PopulateBounds(const AccessibilityInfoDataWrapper& node,
                                ui::AXNodeData& out_data) const = 0;

   protected:
    raw_ptr<AXTreeSourceAndroid> tree_source_;  // owner of this
   private:
    friend class AXTreeSourceAndroid;
    // Called on construction of tree_source only.
    void BindTree(AXTreeSourceAndroid* tree_source) {
      tree_source_ = tree_source;
    }
  };
  // The interface to hook the event handling and the node serialization.
  class Hook {
   public:
    Hook() = default;
    virtual ~Hook() = default;

    // Called prior to accessibility event dispatch.
    // Hook implementations can update the internal state if necessary so that
    // hooks can update the serialization state in PostSerializeNode().
    // Return true if re-serialization of attaching node is needed.
    virtual bool PreDispatchEvent(
        AXTreeSourceAndroid* tree_source,
        const mojom::AccessibilityEventData& event_data) = 0;

    // Called after the default serialization of the attaching node.
    // Hook implementations can modify the serialization of given |out_data|.
    // Note that serialization is executed only when ui::AXTreeSerializer calls
    // SerializeNode() from AXTreeSerializer.SerializeChanges().
    // To ensure the node re-serialized, the class must return |true| on
    // PreDispatchEvent() if the event is NOT coming from its ancestry.
    virtual void PostSerializeNode(ui::AXNodeData* out_data) const = 0;

    virtual bool ShouldDestroy(AXTreeSourceAndroid* tree_source) const = 0;
  };

  AXTreeSourceAndroid(
      Delegate* delegate,
      std::unique_ptr<SerializationDelegate> serialization_delegate,
      aura::Window* window);

  AXTreeSourceAndroid(const AXTreeSourceAndroid&) = delete;
  AXTreeSourceAndroid& operator=(const AXTreeSourceAndroid&) = delete;

  ~AXTreeSourceAndroid() override;

  // Notify automation of an accessibility event.
  void NotifyAccessibilityEvent(mojom::AccessibilityEventData* event_data);

  // Notify automation of a result to an action.
  void NotifyActionResult(const ui::AXActionData& data, bool result);

  // Notify automation of result to getTextLocation.
  void NotifyGetTextLocationDataResult(const ui::AXActionData& data,
                                       const std::optional<gfx::Rect>& rect);

  // Invalidates the tree serializer.
  void InvalidateTree();

  // When it is enabled, this class exposes an accessibility tree optimized for
  // screen readers such as ChromeVox and SwitchAccess. This intends to have the
  // navigation order and focusabilities similar to TalkBack.
  // Also, when it is enabled, the accessibility focus in Android is exposed as
  // the focus of this tree.
  bool UseFullFocusMode() const;

  // Returns true if the node id is the root of the node tree (which can have a
  // parent window).
  // virtual for testing.
  virtual bool IsRootOfNodeTree(int32_t id) const;

  // Sets a virtual node, i.e., node that doesn't exist in source Android tree.
  // This set is only effective on the current event serialization.
  // Usually setting a node is always needed by using a Hook.
  // Note that currently panret node should be an instance of
  // AccessibilityWindowInfoDataWrapper.
  void SetVirtualNode(int32_t parent_id,
                      std::unique_ptr<AccessibilityInfoDataWrapper> child);

  AccessibilityInfoDataWrapper* GetFirstImportantAncestor(
      AccessibilityInfoDataWrapper* info_data) const;

  AccessibilityInfoDataWrapper* GetFirstAccessibilityFocusableAncestor(
      AccessibilityInfoDataWrapper* info_data) const;

  SerializationDelegate& serialization_delegate() const {
    return *serialization_delegate_.get();
  }
  // AXTreeSource:
  bool GetTreeData(ui::AXTreeData* data) const override;
  AccessibilityInfoDataWrapper* GetRoot() const override;
  AccessibilityInfoDataWrapper* GetFromId(int32_t id) const override;
  AccessibilityInfoDataWrapper* GetParent(
      AccessibilityInfoDataWrapper* info_data) const override;
  void SerializeNode(AccessibilityInfoDataWrapper* info_data,
                     ui::AXNodeData* out_data) const override;

  aura::Window* window() { return window_; }
  void set_window(aura::Window* window) { window_ = window; }

  bool is_notification() { return is_notification_; }

  bool is_input_method_window() { return is_input_method_window_; }

  // The window id of this tree.
  std::optional<int32_t> window_id() const { return window_id_; }
  // The root id of this tree.
  std::optional<int32_t> root_id() const { return root_id_; }

  void set_automation_event_router_for_test(
      extensions::AutomationEventRouterInterface* router) {
    automation_event_router_for_test_ = router;
  }
  void set_window_id_for_test(int32_t window_id) { window_id_ = window_id; }

 private:
  friend class AXTreeSourceAndroidTest;

  // Builds the map that stores relationships between nodes.
  void BuildNodeMap(const mojom::AccessibilityEventData& event_data);

  // Actual implementation of NotifyAccessibilityEvent.
  void NotifyAccessibilityEventInternal(
      const mojom::AccessibilityEventData& event_data);

  // Returns AutomationEventRouter.
  extensions::AutomationEventRouterInterface* GetAutomationEventRouter() const;

  // Computes the smallest rect that encloses all of the descendants of
  // |info_data|.
  gfx::Rect ComputeEnclosingBounds(
      AccessibilityInfoDataWrapper* info_data) const;

  // Helper to recursively compute bounds for |info_data|. Returns true if
  // non-empty bounds were encountered.
  void ComputeEnclosingBoundsInternal(AccessibilityInfoDataWrapper* info_data,
                                      gfx::Rect* computed_bounds) const;

  // Find the most top-left focusable node under the given node in full focus
  // mode.
  AccessibilityInfoDataWrapper* FindFirstFocusableNodeInFullFocusMode(
      AccessibilityInfoDataWrapper* info_data) const;

  // Updates android_focused_id_ from given AccessibilityEventData.
  // Having this method, |android_focused_id_| is one of these:
  // - input focus in Android
  // - accessibility focus in Android
  // - the chrome automation client's internal focus (via set sequential focus
  //   action and replying accessibility focus event from Android).
  // This returns false if we don't want to dispatch the processing
  // event to chrome automation. Otherwise, this returns true.
  bool UpdateAndroidFocusedId(const mojom::AccessibilityEventData& event_data);

  // Processes implementations of Hooks and returns a list node id that needs
  // re-serialization.
  std::vector<int32_t> ProcessHooksOnEvent(
      const mojom::AccessibilityEventData& event_data);

  // Resets tree state.
  void Reset();

  // Returns true if we want to traversal |left| after |right|.
  // Note that this comparison is NOT transitive.
  bool NeedReorder(AccessibilityInfoDataWrapper* left,
                   AccessibilityInfoDataWrapper* right) const;

  // Returns true if we can traversal |left| before |right|.
  bool CompareBounds(const gfx::Rect& left, const gfx::Rect& right) const;

  // AXTreeSource:
  int32_t GetId(AccessibilityInfoDataWrapper* info_data) const override;
  void CacheChildrenIfNeeded(AccessibilityInfoDataWrapper*) override;
  size_t GetChildCount(AccessibilityInfoDataWrapper*) const override;
  AccessibilityInfoDataWrapper* ChildAt(AccessibilityInfoDataWrapper*,
                                        size_t) const override;
  void ClearChildCache(AccessibilityInfoDataWrapper*) override;

  bool IsIgnored(AccessibilityInfoDataWrapper* info_data) const override;
  bool IsEqual(AccessibilityInfoDataWrapper* info_data1,
               AccessibilityInfoDataWrapper* info_data2) const override;
  AccessibilityInfoDataWrapper* GetNull() const override;

  // AXActionHandlerBase:
  void PerformAction(const ui::AXActionData& data) override;

  std::vector<raw_ptr<AccessibilityInfoDataWrapper, VectorExperimental>>&
  GetChildren(AccessibilityInfoDataWrapper* info_data) const;

  void ComputeAndCacheChildren(AccessibilityInfoDataWrapper* info_data) const;

  // Maps an AccessibilityInfoDataWrapper ID to its tree data.
  std::map<int32_t, std::unique_ptr<AccessibilityInfoDataWrapper>> tree_map_;

  // Maps an AccessibilityInfoDataWrapper ID to its parent.
  std::map<int32_t, int32_t> parent_map_;

  std::unique_ptr<AXTreeAndroidSerializer> current_tree_serializer_;
  std::optional<int32_t> root_id_;
  std::optional<int32_t> window_id_;
  std::optional<int32_t> android_focused_id_;

  bool is_notification_;
  bool is_input_method_window_;

  std::optional<std::string> notification_key_;

  // Window corresponding this tree.
  raw_ptr<aura::Window, DanglingUntriaged> window_;

  // Cache of mapping from the *Android* window id to the last focused node id.
  std::map<int32_t, int32_t> window_id_to_last_focus_node_id_;

  // Mapping from Chrome node ID to its cached computed bounds.
  // This simplifies bounds calculations.
  std::map<int32_t, gfx::Rect> computed_bounds_;

  // Mapping from Chrome node ID to the attached hook implementations.
  base::flat_map<int32_t, std::unique_ptr<Hook>> hooks_;

  // A delegate that handles accessibility actions on behalf of this tree. The
  // delegate is valid during the lifetime of this tree.
  const raw_ptr<const Delegate> delegate_;
  // A delegate that handles unique serialization logic on behalf of this tree.
  // The delegate is valid during the lifetime of this tree.
  const std::unique_ptr<SerializationDelegate> serialization_delegate_;

  raw_ptr<extensions::AutomationEventRouterInterface>
      automation_event_router_for_test_ = nullptr;
};

}  // namespace ax::android

#endif  // SERVICES_ACCESSIBILITY_ANDROID_AX_TREE_SOURCE_ANDROID_H_
