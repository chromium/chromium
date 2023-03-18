// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEB_CONTENTS_WEB_CONTENTS_VIEW_AURA_H_
#define CONTENT_BROWSER_WEB_CONTENTS_WEB_CONTENTS_VIEW_AURA_H_

#include <memory>
#include <utility>
#include <vector>

#include "base/functional/callback_helpers.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "build/build_config.h"
#include "content/browser/renderer_host/render_view_host_delegate_view.h"
#include "content/browser/site_instance_group.h"
#include "content/browser/web_contents/web_contents_view.h"
#include "content/common/buildflags.h"
#include "content/common/content_export.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/visibility.h"
#include "content/public/browser/web_contents_view_delegate.h"
#include "content/public/common/drop_data.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/mojom/choosers/popup_menu.mojom.h"
#include "ui/aura/client/drag_drop_delegate.h"
#include "ui/aura/window.h"
#include "ui/aura/window_delegate.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/dragdrop/drop_target_event.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom-forward.h"
#include "ui/compositor/layer_tree_owner.h"

namespace ui {
class DropTargetEvent;
class TouchSelectionController;
}

namespace content {
class GestureNavSimple;
class RenderWidgetHostImpl;
class RenderWidgetHostViewAura;
class TouchSelectionControllerClientAura;
class WebContentsImpl;
class WebDragDestDelegate;

class CONTENT_EXPORT WebContentsViewAura
    : public WebContentsView,
      public RenderViewHostDelegateView,
      public aura::WindowDelegate,
      public aura::client::DragDropDelegate {
 public:
  WebContentsViewAura(WebContentsImpl* web_contents,
                      std::unique_ptr<WebContentsViewDelegate> delegate);
  ~WebContentsViewAura() override;

  WebContentsViewAura(const WebContentsViewAura&) = delete;
  WebContentsViewAura& operator=(const WebContentsViewAura&) = delete;

  // Allow the WebContentsViewDelegate to be set explicitly.
  void SetDelegateForTesting(std::unique_ptr<WebContentsViewDelegate> delegate);

  // Set a flag to pass nullptr as the parent_view argument to
  // RenderWidgetHostViewAura::InitAsChild().
  void set_init_rwhv_with_null_parent_for_testing(bool set) {
    init_rwhv_with_null_parent_for_testing_ = set;
  }

  using RenderWidgetHostViewCreateFunction =
      RenderWidgetHostViewAura* (*)(RenderWidgetHost*);

  // Used to override the creation of RenderWidgetHostViews in tests.
  static void InstallCreateHookForTests(
      RenderWidgetHostViewCreateFunction create_render_widget_host_view);

 private:
  // Just the metadata from DropTargetEvent that's safe and cheap to copy to
  // help locate drop events in the callback.
  struct DropMetadata {
    explicit DropMetadata(const ui::DropTargetEvent& event);

    // Location local to WebContentsViewAura.
    gfx::PointF localized_location;

    // Root location of the drop target event.
    gfx::PointF root_location;

    // The supported DnD operation of the source. A bitmask of
    // ui::mojom::DragOperations.
    int source_operations;
    // Flags from ui::Event. Usually represents modifier keys used at drop time.
    int flags;
  };

  // A structure used to keep drop context for asynchronously finishing a
  // drop operation.  This is required because some drop event data gets
  // cleared out once PerformDropCallback() returns.
  struct CONTENT_EXPORT OnPerformDropContext {
    OnPerformDropContext(RenderWidgetHostImpl* target_rwh,
                         DropMetadata drop_metadata,
                         std::unique_ptr<ui::OSExchangeData> data,
                         base::ScopedClosureRunner end_drag_runner,
                         absl::optional<gfx::PointF> transformed_pt,
                         gfx::PointF screen_pt);
    OnPerformDropContext(OnPerformDropContext&& other);
    ~OnPerformDropContext();

    base::WeakPtr<RenderWidgetHostImpl> target_rwh;
    DropMetadata drop_metadata;
    std::unique_ptr<ui::OSExchangeData> data;
    base::ScopedClosureRunner end_drag_runner;
    absl::optional<gfx::PointF> transformed_pt;
    gfx::PointF screen_pt;
  };

  friend class WebContentsViewAuraTest;
  FRIEND_TEST_ALL_PREFIXES(WebContentsViewAuraTest, EnableDisableOverscroll);
  FRIEND_TEST_ALL_PREFIXES(WebContentsViewAuraTest, DragDropFiles);
  FRIEND_TEST_ALL_PREFIXES(WebContentsViewAuraTest,
                           DragDropFilesOriginateFromRenderer);
  FRIEND_TEST_ALL_PREFIXES(WebContentsViewAuraTest, DragDropImageFromRenderer);
  FRIEND_TEST_ALL_PREFIXES(WebContentsViewAuraTest, DragDropVirtualFiles);
  FRIEND_TEST_ALL_PREFIXES(WebContentsViewAuraTest,
                           DragDropVirtualFilesOriginateFromRenderer);
  FRIEND_TEST_ALL_PREFIXES(WebContentsViewAuraTest, DragDropUrlData);
  FRIEND_TEST_ALL_PREFIXES(WebContentsViewAuraTest, DragDropOnOopif);
  FRIEND_TEST_ALL_PREFIXES(WebContentsViewAuraTest, Drop_DeepScanOK);
  FRIEND_TEST_ALL_PREFIXES(WebContentsViewAuraTest, Drop_DeepScanBad);
  FRIEND_TEST_ALL_PREFIXES(WebContentsViewAuraTest, StartDragging);
  FRIEND_TEST_ALL_PREFIXES(WebContentsViewAuraTest, GetDropCallback_Run);
  FRIEND_TEST_ALL_PREFIXES(WebContentsViewAuraTest, GetDropCallback_Cancelled);
  FRIEND_TEST_ALL_PREFIXES(
      WebContentsViewAuraTest,
      RejectDragFromPrivilegedWebContentsToNonPrivilegedWebContents);
  FRIEND_TEST_ALL_PREFIXES(
      WebContentsViewAuraTest,
      AcceptDragFromPrivilegedWebContentsToPrivilegedWebContents);
  FRIEND_TEST_ALL_PREFIXES(
      WebContentsViewAuraTest,
      RejectDragFromNonPrivilegedWebContentsToPrivilegedWebContents);
  FRIEND_TEST_ALL_PREFIXES(WebContentsViewAuraTest,
                           StartDragFromPrivilegedWebContents);

  class WindowObserver;

  // Utility to fill a DropData object from ui::OSExchangeData.
  void PrepareDropData(DropData* drop_data,
                       const ui::OSExchangeData& data) const;

  void EndDrag(base::WeakPtr<RenderWidgetHostImpl> source_rwh_weak_ptr,
               ui::mojom::DragOperation op);

  void InstallOverscrollControllerDelegate(RenderWidgetHostViewAura* view);

  ui::TouchSelectionController* GetSelectionController() const;
  TouchSelectionControllerClientAura* GetSelectionControllerClient() const;

  // Returns GetNativeView unless overridden for testing.
  gfx::NativeView GetRenderWidgetHostViewParent() const;

  // Returns whether |target_rwh| is a valid RenderWidgetHost to be dragging
  // over. This enforces that same-page, cross-site drags are not allowed. See
  // crbug.com/666858.
  bool IsValidDragTarget(RenderWidgetHostImpl* target_rwh) const;

  // Called from CreateView() to create |window_|.
  void CreateAuraWindow(aura::Window* context);

  // Computes the view's visibility updates the WebContents accordingly.
  void UpdateWebContentsVisibility();

  // Computes the view's visibility.
  Visibility GetVisibility() const;

  // Overridden from WebContentsView:
  gfx::NativeView GetNativeView() const override;
  gfx::NativeView GetContentNativeView() const override;
  gfx::NativeWindow GetTopLevelNativeWindow() const override;
  gfx::Rect GetContainerBounds() const override;
  void Focus() override;
  void SetInitialFocus() override;
  void StoreFocus() override;
  void RestoreFocus() override;
  void FocusThroughTabTraversal(bool reverse) override;
  DropData* GetDropData() const override;
  gfx::Rect GetViewBounds() const override;
  void CreateView(gfx::NativeView context) override;
  RenderWidgetHostViewBase* CreateViewForWidget(
      RenderWidgetHost* render_widget_host) override;
  RenderWidgetHostViewBase* CreateViewForChildWidget(
      RenderWidgetHost* render_widget_host) override;
  void SetPageTitle(const std::u16string& title) override;
  void RenderViewReady() override;
  void RenderViewHostChanged(RenderViewHost* old_host,
                             RenderViewHost* new_host) override;
  void SetOverscrollControllerEnabled(bool enabled) override;
  void OnCapturerCountChanged() override;
  void FullscreenStateChanged(bool is_fullscreen) override;
  void UpdateWindowControlsOverlay(const gfx::Rect& bounding_rect) override;

  // Overridden from RenderViewHostDelegateView:
  void ShowContextMenu(RenderFrameHost& render_frame_host,
                       const ContextMenuParams& params) override;
  void StartDragging(const DropData& drop_data,
                     blink::DragOperationsMask operations,
                     const gfx::ImageSkia& image,
                     const gfx::Vector2d& cursor_offset,
                     const gfx::Rect& drag_obj_rect,
                     const blink::mojom::DragEventSourceInfo& event_info,
                     RenderWidgetHostImpl* source_rwh) override;
  void UpdateDragCursor(ui::mojom::DragOperation operation) override;
  void GotFocus(RenderWidgetHostImpl* render_widget_host) override;
  void LostFocus(RenderWidgetHostImpl* render_widget_host) override;
  void TakeFocus(bool reverse) override;
  int GetTopControlsHeight() const override;
  int GetBottomControlsHeight() const override;
  bool DoBrowserControlsShrinkRendererSize() const override;
#if BUILDFLAG(USE_EXTERNAL_POPUP_MENU)
  void ShowPopupMenu(
      RenderFrameHost* render_frame_host,
      mojo::PendingRemote<blink::mojom::PopupMenuClient> popup_client,
      const gfx::Rect& bounds,
      int item_height,
      double item_font_size,
      int selected_item,
      std::vector<blink::mojom::MenuItemPtr> menu_items,
      bool right_aligned,
      bool allow_multiple_selection) override;
#endif

  // Overridden from aura::WindowDelegate:
  gfx::Size GetMinimumSize() const override;
  gfx::Size GetMaximumSize() const override;
  void OnBoundsChanged(const gfx::Rect& old_bounds,
                       const gfx::Rect& new_bounds) override;
  gfx::NativeCursor GetCursor(const gfx::Point& point) override;
  int GetNonClientComponent(const gfx::Point& point) const override;
  bool ShouldDescendIntoChildForEventHandling(
      aura::Window* child,
      const gfx::Point& location) override;
  bool CanFocus() override;
  void OnCaptureLost() override;
  void OnPaint(const ui::PaintContext& context) override;
  void OnDeviceScaleFactorChanged(float old_device_scale_factor,
                                  float new_device_scale_factor) override;
  void OnWindowDestroying(aura::Window* window) override;
  void OnWindowDestroyed(aura::Window* window) override;
  void OnWindowTargetVisibilityChanged(bool visible) override;
  void OnWindowOcclusionChanged(
      aura::Window::OcclusionState occlusion_state) override;
  bool HasHitTestMask() const override;
  void GetHitTestMask(SkPath* mask) const override;

  // Overridden from ui::EventHandler:
  void OnKeyEvent(ui::KeyEvent* event) override;
  void OnMouseEvent(ui::MouseEvent* event) override;

  // Overridden from aura::client::DragDropDelegate:
  void OnDragEntered(const ui::DropTargetEvent& event) override;
  aura::client::DragUpdateInfo OnDragUpdated(
      const ui::DropTargetEvent& event) override;
  void OnDragExited() override;
  aura::client::DragDropDelegate::DropCallback GetDropCallback(
      const ui::DropTargetEvent& event) override;

  void DragEnteredCallback(DropMetadata flags,
                           std::unique_ptr<DropData> drop_data,
                           base::WeakPtr<RenderWidgetHostViewBase> target,
                           absl::optional<gfx::PointF> transformed_pt);
  void DragUpdatedCallback(DropMetadata drop_metadata,
                           std::unique_ptr<DropData> drop_data,
                           base::WeakPtr<RenderWidgetHostViewBase> target,
                           absl::optional<gfx::PointF> transformed_pt);
  void PerformDropCallback(DropMetadata drop_metadata,
                           std::unique_ptr<ui::OSExchangeData> data,
                           base::WeakPtr<RenderWidgetHostViewBase> target,
                           absl::optional<gfx::PointF> transformed_pt);

  // Completes a drag exit operation by communicating with the renderer process.
  void CompleteDragExit();

  // Called from PerformDropCallback() to finish processing the drop.
  // The override with `drop_data` updates `current_drop_data_` before
  // completing the drop.
  void FinishOnPerformDrop(OnPerformDropContext context);
  void FinishOnPerformDropCallback(OnPerformDropContext context,
                                   absl::optional<DropData> drop_data);

  // Completes a drop operation by communicating the drop data to the renderer
  // process.
  void CompleteDrop(RenderWidgetHostImpl* target_rwh,
                    const DropData& drop_data,
                    const gfx::PointF& client_pt,
                    const gfx::PointF& screen_pt,
                    int key_modifiers);

  // Performs drop if it's run. Otherwise, it exits the drag. Returned by
  // GetDropCallback.
  void PerformDropOrExitDrag(
      base::ScopedClosureRunner exit_drag,
      DropMetadata drop_metadata,
      std::unique_ptr<ui::OSExchangeData> data,
      ui::mojom::DragOperation& output_drag_op,
      std::unique_ptr<ui::LayerTreeOwner> drag_image_layer_owner);

  // For unit testing, registers a callback for when a drop operation
  // completes.
  using DropCallbackForTesting =
      base::OnceCallback<void(RenderWidgetHostImpl* target_rwh,
                              const DropData& drop_data,
                              const gfx::PointF& client_pt,
                              const gfx::PointF& screen_pt,
                              int key_modifiers,
                              bool drop_allowed)>;
  void RegisterDropCallbackForTesting(DropCallbackForTesting callback);

  void SetDragDestDelegateForTesting(WebDragDestDelegate* delegate) {
    drag_dest_delegate_ = delegate;
  }

#if BUILDFLAG(IS_WIN)
  // Callback for asynchronous retrieval of virtual files.
  void OnGotVirtualFilesAsTempFiles(
      const std::vector<std::pair</*temp path*/ base::FilePath,
                                  /*display name*/ base::FilePath>>&
          filepaths_and_names);

  class AsyncDropNavigationObserver;
  std::unique_ptr<AsyncDropNavigationObserver> async_drop_navigation_observer_;

  class AsyncDropTempFileDeleter;
  std::unique_ptr<AsyncDropTempFileDeleter> async_drop_temp_file_deleter_;
#endif
  DropCallbackForTesting drop_callback_for_testing_;

  // If this callback is initialized it must be run after the drop operation is
  // done to send dragend event in EndDrag function.
  base::ScopedClosureRunner end_drag_runner_;

  std::unique_ptr<aura::Window> window_;

  std::unique_ptr<WindowObserver> window_observer_;

  // The WebContentsImpl whose contents we display.
  raw_ptr<WebContentsImpl> web_contents_;

  std::unique_ptr<WebContentsViewDelegate> delegate_;

  ui::mojom::DragOperation current_drag_op_;

  std::unique_ptr<DropData> current_drop_data_;

  raw_ptr<WebDragDestDelegate> drag_dest_delegate_;

  // We keep track of the RenderWidgetHost we're dragging over. If it changes
  // during a drag, we need to re-send the DragEnter message.
  base::WeakPtr<RenderWidgetHostImpl> current_rwh_for_drag_;

  // We also keep track of the ID of the RenderViewHost we're dragging over to
  // avoid sending the drag exited message after leaving the current view.
  GlobalRoutingID current_rvh_for_drag_;

  // We track the IDs of the source RenderProcessHost and RenderViewHost from
  // which the current drag originated. These are used to ensure that drag
  // events do not fire over a cross-site frame (with respect to the source
  // frame) in the same page (see crbug.com/666858). Specifically, the
  // RenderViewHost is used to check the "same page" property, while the
  // RenderProcessHost is used to check the "cross-site" property. Note that the
  // reason the RenderProcessHost is tracked instead of the RenderWidgetHost is
  // so that we still allow drags between non-contiguous same-site frames (such
  // frames will have the same process, but different widgets). Note also that
  // the RenderViewHost may not be in the same process as the RenderProcessHost,
  // since the view corresponds to the page, while the process is specific to
  // the frame from which the drag started.
  // We also track whether a dragged image is accessible from its frame, so we
  // can disallow tainted-cross-origin same-page drag-drop.
  struct DragStart {
    DragStart(SiteInstanceGroupId site_instance_group_id,
              GlobalRoutingID view_id,
              bool image_accessible_from_frame)
        : site_instance_group_id(site_instance_group_id),
          view_id(view_id),
          image_accessible_from_frame(image_accessible_from_frame) {}
    ~DragStart() = default;

    SiteInstanceGroupId site_instance_group_id;
    GlobalRoutingID view_id;
    bool image_accessible_from_frame;
  };
  absl::optional<DragStart> drag_start_;

  // Responsible for handling gesture-nav and pull-to-refresh UI.
  std::unique_ptr<GestureNavSimple> gesture_nav_simple_;

  // This is true when the drag is in process from the perspective of this
  // class. It means it gets true when drag enters and gets reset when either
  // drop happens or drag exits.
  bool drag_in_progress_;

  bool init_rwhv_with_null_parent_for_testing_;

  // Non-null when the WebContents is being captured for video.
  std::unique_ptr<aura::WindowTreeHost::VideoCaptureLock> video_capture_lock_;

  base::WeakPtrFactory<WebContentsViewAura> weak_ptr_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEB_CONTENTS_WEB_CONTENTS_VIEW_AURA_H_
