// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_MANAGER_H_
#define CONTENT_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_MANAGER_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/scoped_observer.h"
#include "build/build_config.h"
#include "content/browser/accessibility/accessibility_buildflags.h"
#include "content/browser/accessibility/browser_accessibility_position.h"
#include "content/common/content_export.h"
#include "content/common/render_accessibility.mojom-forward.h"
#include "content/public/browser/ax_event_notification_details.h"
#include "content/public/browser/web_contents_observer.h"
#include "third_party/blink/public/web/web_ax_enums.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/accessibility/ax_event_generator.h"
#include "ui/accessibility/ax_node.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/ax_range.h"
#include "ui/accessibility/ax_serializable_tree.h"
#include "ui/accessibility/ax_tree.h"
#include "ui/accessibility/ax_tree_id_registry.h"
#include "ui/accessibility/ax_tree_manager.h"
#include "ui/accessibility/ax_tree_observer.h"
#include "ui/accessibility/ax_tree_update.h"
#include "ui/accessibility/platform/ax_platform_node.h"
#include "ui/gfx/native_widget_types.h"

namespace content {
class BrowserAccessibility;
class BrowserAccessibilityDelegate;
class BrowserAccessibilityManager;
#if defined(OS_ANDROID)
class BrowserAccessibilityManagerAndroid;
#elif defined(OS_WIN)
class BrowserAccessibilityManagerWin;
#elif BUILDFLAG(USE_ATK)
class BrowserAccessibilityManagerAuraLinux;
#elif defined(OS_MAC)
class BrowserAccessibilityManagerMac;
#endif

// To be called when a BrowserAccessibilityManager fires a generated event.
// Provides the host, the event fired, and which node id the event was for.
typedef base::RepeatingCallback<
    void(BrowserAccessibilityDelegate*, ui::AXEventGenerator::Event, int)>
    GeneratedEventCallbackForTesting;

// For testing.
CONTENT_EXPORT ui::AXTreeUpdate MakeAXTreeUpdate(
    const ui::AXNodeData& node,
    const ui::AXNodeData& node2 = ui::AXNodeData(),
    const ui::AXNodeData& node3 = ui::AXNodeData(),
    const ui::AXNodeData& node4 = ui::AXNodeData(),
    const ui::AXNodeData& node5 = ui::AXNodeData(),
    const ui::AXNodeData& node6 = ui::AXNodeData(),
    const ui::AXNodeData& node7 = ui::AXNodeData(),
    const ui::AXNodeData& node8 = ui::AXNodeData(),
    const ui::AXNodeData& node9 = ui::AXNodeData(),
    const ui::AXNodeData& node10 = ui::AXNodeData(),
    const ui::AXNodeData& node11 = ui::AXNodeData(),
    const ui::AXNodeData& node12 = ui::AXNodeData());

// Class that can perform actions on behalf of the BrowserAccessibilityManager.
// Note: BrowserAccessibilityManager should never cache any of the return
// values from any of these interfaces, especially those that return pointers.
// They may only be valid within this call stack. That policy eliminates any
// concerns about ownership and lifecycle issues; none of these interfaces
// transfer ownership and no return values are guaranteed to be valid outside
// of the current call stack.
class CONTENT_EXPORT BrowserAccessibilityDelegate {
 public:
  virtual ~BrowserAccessibilityDelegate() {}

  virtual void AccessibilityPerformAction(const ui::AXActionData& data) = 0;
  virtual bool AccessibilityViewHasFocus() = 0;
  virtual void AccessibilityViewSetFocus() = 0;
  virtual gfx::Rect AccessibilityGetViewBounds() = 0;
  virtual float AccessibilityGetDeviceScaleFactor() = 0;
  virtual void AccessibilityFatalError() = 0;
  virtual gfx::AcceleratedWidget AccessibilityGetAcceleratedWidget() = 0;
  virtual gfx::NativeViewAccessible AccessibilityGetNativeViewAccessible() = 0;
  virtual gfx::NativeViewAccessible
  AccessibilityGetNativeViewAccessibleForWindow() = 0;
  virtual WebContents* AccessibilityWebContents() = 0;
  virtual void AccessibilityHitTest(
      const gfx::Point& point_in_frame_pixels,
      ax::mojom::Event opt_event_to_fire,
      int opt_request_id,
      base::OnceCallback<void(BrowserAccessibilityManager* hit_manager,
                              int hit_node_id)> opt_callback) = 0;

  // Returns true if this delegate represents the main (topmost) frame in a
  // tree of frames.
  virtual bool AccessibilityIsMainFrame() = 0;
};

// This is all of the information about the current find in page result,
// so we can activate it if requested.
struct BrowserAccessibilityFindInPageInfo {
  BrowserAccessibilityFindInPageInfo();

  // This data about find in text results is updated as the user types.
  int request_id;
  int match_index;
  int start_id;
  int start_offset;
  int end_id;
  int end_offset;

  // The active request id indicates that the user committed to a find query,
  // e.g. by pressing enter or pressing the next or previous buttons. If
  // |active_request_id| == |request_id|, we fire an accessibility event
  // to move screen reader focus to that event.
  int active_request_id;
};

// Manages a tree of BrowserAccessibility objects.
class CONTENT_EXPORT BrowserAccessibilityManager : public ui::AXTreeObserver,
                                                   public ui::AXTreeManager,
                                                   public WebContentsObserver {
 protected:
  using BrowserAccessibilityPositionInstance =
      BrowserAccessibilityPosition::AXPositionInstance;
  using BrowserAccessibilityRange =
      ui::AXRange<BrowserAccessibilityPositionInstance::element_type>;

 public:
  // Creates the platform-specific BrowserAccessibilityManager, but
  // with no parent window pointer. Only useful for unit tests.
  static BrowserAccessibilityManager* Create(
      const ui::AXTreeUpdate& initial_tree,
      BrowserAccessibilityDelegate* delegate);

  static BrowserAccessibilityManager* FromID(ui::AXTreeID ax_tree_id);

  ~BrowserAccessibilityManager() override;

  void Initialize(const ui::AXTreeUpdate& initial_tree);

  static ui::AXTreeUpdate GetEmptyDocument();

  enum RetargetEventType {
    RetargetEventTypeGenerated = 0,
    RetargetEventTypeBlinkGeneral,
    RetargetEventTypeBlinkHover,
  };

  // Return |node| by default, but some platforms want to update the target node
  // based on the event type.
  virtual BrowserAccessibility* RetargetForEvents(BrowserAccessibility* node,
                                                  RetargetEventType type) const;

  // Subclasses override these methods to send native event notifications.
  virtual void FireFocusEvent(BrowserAccessibility* node);
  virtual void FireBlinkEvent(ax::mojom::Event event_type,
                              BrowserAccessibility* node) {}
  virtual void FireGeneratedEvent(ui::AXEventGenerator::Event event_type,
                                  BrowserAccessibility* node);

  // Checks whether focus has changed since the last time it was checked,
  // taking into account whether the window has focus and which frame within
  // the frame tree has focus. If focus has changed, calls FireFocusEvent.
  void FireFocusEventsIfNeeded();

  // Return whether or not we are currently able to fire events.
  virtual bool CanFireEvents() const;

  // Return a pointer to the root of the tree.
  BrowserAccessibility* GetRoot() const;

  // Returns a pointer to the BrowserAccessibility object for a given AXNode.
  BrowserAccessibility* GetFromAXNode(const ui::AXNode* node) const;

  // Return a pointer to the object corresponding to the given id,
  // does not make a new reference.
  BrowserAccessibility* GetFromID(int32_t id) const;

  // If this tree has a parent tree, return the parent node in that tree.
  BrowserAccessibility* GetParentNodeFromParentTree() const;

  // In general, there is only a single node with the role of kRootWebArea,
  // but if a popup is opened, a second nested "root" is created in the same
  // tree as the "true" root. This will keep track of the nested root node.
  BrowserAccessibility* GetPopupRoot() const;

  // Get the AXTreeData for this frame.
  const ui::AXTreeData& GetTreeData() const;

  // Called to notify the accessibility manager that its associated native
  // view got focused.
  virtual void OnWindowFocused();

  // Called to notify the accessibility manager that its associated native
  // view lost focus.
  virtual void OnWindowBlurred();

  // Notify the accessibility manager about page navigation.
  // TODO(domfarolino, dmazzoni): Implement WebContentsObserver methods that
  // correspond to the ones we provide today, so we can stop being manually
  // notified of navigation events when they happen.
  void UserIsNavigatingAway();
  virtual void UserIsReloading();
  void NavigationSucceeded();
  void NavigationFailed();

  // WebContentsObserver overrides
  void DidStopLoading() override;
  void DidActivatePortal(WebContents* predecessor_contents,
                         base::TimeTicks activation_time) override;

  // Keep track of if this page is hidden by an interstitial, in which case
  // we need to suppress all events.
  void set_hidden_by_interstitial_page(bool hidden) {
    hidden_by_interstitial_page_ = hidden;
  }
  bool hidden_by_interstitial_page() const {
    return hidden_by_interstitial_page_;
  }

  // Pretend that the given node has focus, for testing only. Doesn't
  // communicate with the renderer and doesn't fire any events.
  void SetFocusLocallyForTesting(BrowserAccessibility* node);

  // For testing only, register a function to be called when focus changes
  // in any BrowserAccessibilityManager.
  static void SetFocusChangeCallbackForTesting(base::RepeatingClosure callback);

  // For testing only, register a function to be called when
  // a generated event is fired from this BrowserAccessibilityManager.
  void SetGeneratedEventCallbackForTesting(
      const GeneratedEventCallbackForTesting& callback);

  // Normally we avoid firing accessibility focus events when the containing
  // native window isn't focused, and we also delay some other events like
  // live region events to improve screen reader compatibility.
  // However, this can lead to test flakiness, so for testing, simplify
  // this behavior and just fire all events with no delay as if the window
  // had focus.
  static void NeverSuppressOrDelayEventsForTesting();

  // Accessibility actions. All of these are implemented asynchronously
  // by sending a message to the renderer to perform the respective action
  // on the given node.  See the definition of |ui::AXActionData| for more
  // information about each of these actions.
  void ClearAccessibilityFocus(const BrowserAccessibility& node);
  void Decrement(const BrowserAccessibility& node);
  void DoDefaultAction(const BrowserAccessibility& node);
  void GetImageData(const BrowserAccessibility& node,
                    const gfx::Size& max_size);
  // Per third_party/blink/renderer/core/layout/hit_test_location.h, Blink
  // expects hit test points in page coordinates. However, WebAXObject::HitTest
  // applies the visual viewport offset, so we want to pass that function a
  // point in frame coordinates.
  void HitTest(const gfx::Point& frame_point) const;
  void Increment(const BrowserAccessibility& node);
  void LoadInlineTextBoxes(const BrowserAccessibility& node);
  void ScrollToMakeVisible(
      const BrowserAccessibility& node,
      gfx::Rect subfocus,
      ax::mojom::ScrollAlignment horizontal_scroll_alignment =
          ax::mojom::ScrollAlignment::kScrollAlignmentCenter,
      ax::mojom::ScrollAlignment vertical_scroll_alignment =
          ax::mojom::ScrollAlignment::kScrollAlignmentCenter,
      ax::mojom::ScrollBehavior scroll_behavior =
          ax::mojom::ScrollBehavior::kDoNotScrollIfVisible);
  void ScrollToPoint(const BrowserAccessibility& node, gfx::Point point);
  void SetAccessibilityFocus(const BrowserAccessibility& node);
  void SetFocus(const BrowserAccessibility& node);
  void SetSequentialFocusNavigationStartingPoint(
      const BrowserAccessibility& node);
  void SetScrollOffset(const BrowserAccessibility& node, gfx::Point offset);
  void SetValue(const BrowserAccessibility& node, const std::string& value);
  void SetSelection(const ui::AXActionData& action_data);
  void SetSelection(const BrowserAccessibilityRange& range);
  void ShowContextMenu(const BrowserAccessibility& node);
  void SignalEndOfTest();

  // Retrieve the bounds of the parent View in screen coordinates.
  virtual gfx::Rect GetViewBoundsInScreenCoordinates() const;

  // Fire an event telling native assistive technology to move focus to the
  // given find in page result.
  void ActivateFindInPageResult(int request_id, int match_index);

  // Called when the renderer process has notified us of tree changes. Returns
  // false in fatal-error conditions, in which case the caller should destroy
  // the manager.
  virtual bool OnAccessibilityEvents(const AXEventNotificationDetails& details)
      WARN_UNUSED_RESULT;

  // Allows derived classes to do event pre-processing
  virtual void BeforeAccessibilityEvents();

  // Allows derived classes to do event post-processing.
  virtual void FinalizeAccessibilityEvents();

  // Called when the renderer process updates the location of accessibility
  // objects. Calls SendLocationChangeEvents(), which can be overridden.
  void OnLocationChanges(const std::vector<mojom::LocationChangesPtr>& changes);

  // Called when a new find in page result is received. We hold on to this
  // information and don't activate it until the user requests it.
  virtual void OnFindInPageResult(int request_id,
                                  int match_index,
                                  int start_id,
                                  int start_offset,
                                  int end_id,
                                  int end_offset);

  // This is called when the user has committed to a find in page query,
  // e.g. by pressing enter or tapping on the next / previous result buttons.
  // If a match has already been received for this request id,
  // activate the result now by firing an accessibility event. If a match
  // has not been received, we hold onto this request id and update it
  // when OnFindInPageResult is called.
  void ActivateFindInPageResult(int request_id);

  // This is called when the user finishes a find in page query and all
  // highlighted matches are deactivated.
  virtual void OnFindInPageTermination() {}

#if defined(OS_WIN)
  BrowserAccessibilityManagerWin* ToBrowserAccessibilityManagerWin();
#endif

#if defined(OS_ANDROID)
  BrowserAccessibilityManagerAndroid* ToBrowserAccessibilityManagerAndroid();
#endif

#if BUILDFLAG(USE_ATK)
  BrowserAccessibilityManagerAuraLinux*
  ToBrowserAccessibilityManagerAuraLinux();
#endif

#if defined(OS_MAC)
  BrowserAccessibilityManagerMac* ToBrowserAccessibilityManagerMac();
#endif

  // Returns the object that has focus, starting at the top of the frame tree,
  // or returns nullptr if this manager doesn't have access to the top document.
  virtual BrowserAccessibility* GetFocus() const;

  // Return the object that has focus, only considering this frame and
  // descendants.
  BrowserAccessibility* GetFocusFromThisOrDescendantFrame() const;

  // Given a focused node |focus|, returns a descendant of that node if it
  // has an active descendant, otherwise returns |focus|.
  BrowserAccessibility* GetActiveDescendant(BrowserAccessibility* focus) const;

  // Returns true if native focus is anywhere in this WebContents or not.
  bool NativeViewHasFocus();

  // True by default, but some platforms want to treat the root
  // scroll offsets separately.
  bool UseRootScrollOffsetsWhenComputingBounds();
  void SetUseRootScrollOffsetsWhenComputingBoundsForTesting(bool use);

  // Walk the tree using depth-first pre-order traversal.
  static BrowserAccessibility* NextInTreeOrder(
      const BrowserAccessibility* object);
  static BrowserAccessibility* NextNonDescendantInTreeOrder(
      const BrowserAccessibility* object);
  static BrowserAccessibility* PreviousInTreeOrder(
      const BrowserAccessibility* object,
      bool can_wrap_to_last_element);
  static BrowserAccessibility* NextTextOnlyObject(
      const BrowserAccessibility* object);
  static BrowserAccessibility* PreviousTextOnlyObject(
      const BrowserAccessibility* object);

  // If the two objects provided have a common ancestor returns both the
  // common ancestor and the child indices of the two subtrees in which the
  // objects are located.
  // Returns false if a common ancestor cannot be found.
  static bool FindIndicesInCommonParent(const BrowserAccessibility& object1,
                                        const BrowserAccessibility& object2,
                                        BrowserAccessibility** common_parent,
                                        int* child_index1,
                                        int* child_index2);

  // Sets |out_is_before| to true if |object1| comes before |object2|
  // in tree order (pre-order traversal), and false if the objects are the
  // same or not in the same tree.
  static ax::mojom::TreeOrder CompareNodes(const BrowserAccessibility& object1,
                                           const BrowserAccessibility& object2);

  static std::vector<const BrowserAccessibility*> FindTextOnlyObjectsInRange(
      const BrowserAccessibility& start_object,
      const BrowserAccessibility& end_object);

  static base::string16 GetTextForRange(
      const BrowserAccessibility& start_object,
      const BrowserAccessibility& end_object);

  // If start and end offsets are greater than the text's length, returns all
  // the text.
  static base::string16 GetTextForRange(
      const BrowserAccessibility& start_object,
      int start_offset,
      const BrowserAccessibility& end_object,
      int end_offset);

  // DEPRECATED: Prefer using AXPlatformNodeDelegate bounds interfaces when
  // writing new code.
  static gfx::Rect GetRootFrameInnerTextRangeBoundsRect(
      const BrowserAccessibility& start_object,
      int start_offset,
      const BrowserAccessibility& end_object,
      int end_offset);

  // Accessors.
  ui::AXTreeID ax_tree_id() const { return ax_tree_id_; }
  float device_scale_factor() const { return device_scale_factor_; }
  ui::AXTree* ax_tree() const { return tree_.get(); }

  // AXTreeObserver implementation.
  void OnNodeWillBeDeleted(ui::AXTree* tree, ui::AXNode* node) override;
  void OnSubtreeWillBeDeleted(ui::AXTree* tree, ui::AXNode* node) override;
  void OnNodeCreated(ui::AXTree* tree, ui::AXNode* node) override;
  void OnNodeDeleted(ui::AXTree* tree, int32_t node_id) override;
  void OnNodeReparented(ui::AXTree* tree, ui::AXNode* node) override;
  void OnRoleChanged(ui::AXTree* tree,
                     ui::AXNode* node,
                     ax::mojom::Role old_role,
                     ax::mojom::Role new_role) override;
  void OnAtomicUpdateFinished(
      ui::AXTree* tree,
      bool root_changed,
      const std::vector<ui::AXTreeObserver::Change>& changes) override;

  // AXTreeManager overrides.
  ui::AXNode* GetNodeFromTree(ui::AXTreeID tree_id,
                              ui::AXNode::AXID node_id) const override;
  ui::AXNode* GetNodeFromTree(ui::AXNode::AXID node_id) const override;
  AXTreeID GetTreeID() const override;
  AXTreeID GetParentTreeID() const override;
  ui::AXNode* GetRootAsAXNode() const override;
  ui::AXNode* GetParentNodeFromParentTreeAsAXNode() const override;

  BrowserAccessibilityDelegate* delegate() const { return delegate_; }

  // If this BrowserAccessibilityManager is a child frame or guest frame,
  // returns the BrowserAccessibilityManager from the top document in the frame
  // tree. If the current frame is not connected to its parent frame yet, or if
  // it got disconnected after being reparented, return nullptr to indicate that
  // we don't have access to the root manager yet.
  BrowserAccessibilityManager* GetRootManager() const;

  // Returns the BrowserAccessibilityDelegate from |GetRootManager| above, or
  // returns nullptr in case we don't have access to the root manager yet.
  BrowserAccessibilityDelegate* GetDelegateFromRootManager() const;

  // Returns whether this is the top document.
  bool IsRootTree() const;

  // Get a snapshot of the current tree as an AXTreeUpdate.
  ui::AXTreeUpdate SnapshotAXTreeForTesting();

  // Use a custom device scale factor for testing.
  void UseCustomDeviceScaleFactorForTesting(float device_scale_factor);

  // Given a point in physical pixel coordinates, trigger an asynchronous hit
  // test but return the best possible match instantly.
  BrowserAccessibility* CachingAsyncHitTest(
      const gfx::Point& physical_pixel_point) const;

  // Called in response to a hover event, caches the result for the next
  // call to CachingAsyncHitTest().
  void CacheHitTestResult(BrowserAccessibility* hit_test_result) const;

  // Updates the page scale factor for this frame.
  void SetPageScaleFactor(float page_scale_factor);

  // Returns the current page scale factor for this frame.
  float GetPageScaleFactor() const;

 protected:
  FRIEND_TEST_ALL_PREFIXES(BrowserAccessibilityManagerTest,
                           TestShouldFireEventForNode);
  explicit BrowserAccessibilityManager(BrowserAccessibilityDelegate* delegate);

  BrowserAccessibilityManager(const ui::AXTreeUpdate& initial_tree,
                              BrowserAccessibilityDelegate* delegate);

  // Send platform-specific notifications to each of these objects that
  // their location has changed. This is called by OnLocationChanges
  // after it's updated the internal data structure.
  virtual void SendLocationChangeEvents(
      const std::vector<mojom::LocationChangesPtr>& changes);

  // Given the data from an atomic update, collect the nodes that need updating
  // assuming that this platform is one where plain text node content is
  // directly included in parents' hypertext.
  void CollectChangedNodesAndParentsForAtomicUpdate(
      ui::AXTree* tree,
      const std::vector<ui::AXTreeObserver::Change>& changes,
      std::set<ui::AXPlatformNode*>* nodes_needing_update);

  bool ShouldFireEventForNode(BrowserAccessibility* node) const;

  static void SetLastFocusedNode(BrowserAccessibility* node);
  static BrowserAccessibility* GetLastFocusedNode();

  // The object that can perform actions on our behalf.
  BrowserAccessibilityDelegate* delegate_;

  // A mapping from a node id to its wrapper of type BrowserAccessibility.
  std::map<int32_t, BrowserAccessibility*> id_wrapper_map_;

  // True if the user has initiated a navigation to another page.
  bool user_is_navigating_away_;

  // Interstitial page, like an SSL warning.
  // If so we need to suppress any events.
  bool hidden_by_interstitial_page_ = false;

  BrowserAccessibilityFindInPageInfo find_in_page_info_;

  // These cache the AX tree ID, node ID, and global screen bounds of the
  // last object found by an asynchronous hit test. Subsequent hit test
  // requests that remain within this object's bounds will return the same
  // object, but will also trigger a new asynchronous hit test request.
  mutable ui::AXTreeID last_hover_ax_tree_id_;
  mutable int last_hover_node_id_;
  mutable gfx::Rect last_hover_bounds_;

  // True if the root's parent is in another accessibility tree and that
  // parent's child is the root. Ensures that the parent node is notified
  // once when this subtree is first connected.
  bool connected_to_parent_tree_node_;

  // The global ID of this accessibility tree.
  ui::AXTreeID ax_tree_id_;

  // The device scale factor for the view associated with this frame,
  // cached each time there's any update to the accessibility tree.
  float device_scale_factor_;

  // The page scale factor for the view associated with this frame,
  // cached when we get an update via SetPageScaleFactor().
  float page_scale_factor_ = 1.0f;

  // For testing only: If true, the manually-set device scale factor will be
  // used and it won't be updated from the delegate.
  bool use_custom_device_scale_factor_for_testing_;

  // Whether we should include or exclude the scroll offsets on the root
  // scroller when computing bounding rectangles. Usually true, but on
  // some platforms the root scroll offsets are handled separately.
  bool use_root_scroll_offsets_when_computing_bounds_ = true;

  // For testing only: A function to call when a generated event is fired.
  GeneratedEventCallbackForTesting generated_event_callback_for_testing_;

  // Keeps track of the nested popup root's id, if it exists. See GetPopupRoot()
  // for details.
  std::set<int32_t> popup_root_ids_;

  // Fire all events regardless of focus and with no delay, to avoid test
  // flakiness. See NeverSuppressOrDelayEventsForTesting() for details.
  static bool never_suppress_or_delay_events_for_testing_;

  const ui::AXEventGenerator& event_generator() const {
    return event_generator_;
  }
  ui::AXEventGenerator& event_generator() { return event_generator_; }

  // Stores the id of the last focused node across all frames, as well as the id
  // of the tree that contains it, so that when focus might have changed we can
  // figure out whether we need to fire a focus event.
  //
  // NOTE: Don't use or modify these properties directly, use the
  // SetLastFocusedNode and GetLastFocusedNode methods instead.
  static base::Optional<int32_t> last_focused_node_id_;
  static base::Optional<ui::AXTreeID> last_focused_node_tree_id_;

 private:
  // Helper that calls AXTree::Unserialize(). On failure it populates crash data
  // with error information.
  bool Unserialize(const ui::AXTreeUpdate& tree_update);

  // The underlying tree of accessibility objects.
  std::unique_ptr<ui::AXSerializableTree> tree_;

  ui::AXEventGenerator event_generator_;

  // Automatically stops observing notifications from the AXTree when this class
  // is destructed.
  //
  // This member needs to be destructed before any observed AXTrees. Since
  // destructors for non-static member fields are called in the reverse order of
  // declaration, do not move this member above other members.
  ScopedObserver<ui::AXTree, ui::AXTreeObserver> tree_observer_{this};

  DISALLOW_COPY_AND_ASSIGN(BrowserAccessibilityManager);
};

}  // namespace content

#endif  // CONTENT_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_MANAGER_H_
