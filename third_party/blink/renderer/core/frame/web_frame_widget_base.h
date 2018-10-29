// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_WEB_FRAME_WIDGET_BASE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_WEB_FRAME_WIDGET_BASE_H_

#include "base/single_thread_task_runner.h"
#include "services/network/public/mojom/referrer_policy.mojom-blink.h"
#include "third_party/blink/public/platform/web_coalesced_input_event.h"
#include "third_party/blink/public/platform/web_drag_data.h"
#include "third_party/blink/public/platform/web_gesture_device.h"
#include "third_party/blink/public/web/web_frame_widget.h"
#include "third_party/blink/renderer/core/clipboard/data_object.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/user_gesture_indicator.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_image.h"
#include "third_party/blink/renderer/platform/heap/member.h"

namespace cc {
class Layer;
}

namespace blink {

class AnimationWorkletMutatorDispatcherImpl;
class CompositorAnimationHost;
class GraphicsLayer;
class HitTestResult;
class PageWidgetEventHandler;
class WebLayerTreeView;
class WebLocalFrameImpl;
class WebViewImpl;
struct IntrinsicSizingInfo;
struct WebFloatPoint;

class CORE_EXPORT WebFrameWidgetBase
    : public GarbageCollectedFinalized<WebFrameWidgetBase>,
      public WebFrameWidget {
 public:
  explicit WebFrameWidgetBase(WebWidgetClient&);
  virtual ~WebFrameWidgetBase();

  WebWidgetClient* Client() const { return client_; }
  WebLocalFrameImpl* LocalRootImpl() const { return local_root_; }

  // Returns the bounding box of the block type node touched by the WebPoint.
  WebRect ComputeBlockBound(const WebPoint& point_in_root_frame,
                            bool ignore_clipping) const;

  void BindLocalRoot(WebLocalFrame&);

  // Called once the local root is bound via |BindLocalRoot()|.
  virtual void Initialize() = 0;
  virtual bool ForSubframe() const = 0;
  virtual void ScheduleAnimation() = 0;
  virtual void IntrinsicSizingInfoChanged(const IntrinsicSizingInfo&) {}
  virtual base::WeakPtr<AnimationWorkletMutatorDispatcherImpl>
  EnsureCompositorMutatorDispatcher(
      scoped_refptr<base::SingleThreadTaskRunner>* mutator_task_runner) = 0;

  // Sets the root graphics layer. |GraphicsLayer| can be null when detaching
  // the root layer.
  virtual void SetRootGraphicsLayer(GraphicsLayer*) = 0;
  virtual GraphicsLayer* RootGraphicsLayer() const = 0;

  // Sets the root layer. The |layer| can be null when detaching the root layer.
  virtual void SetRootLayer(scoped_refptr<cc::Layer> layer) = 0;

  virtual WebLayerTreeView* GetLayerTreeView() const = 0;
  virtual CompositorAnimationHost* AnimationHost() const = 0;

  virtual HitTestResult CoreHitTestResultAt(const WebPoint&) = 0;

  // WebFrameWidget implementation.
  void Close() override;
  WebLocalFrame* LocalRoot() const override;
  void UpdateAllLifecyclePhasesAndCompositeForTesting(bool do_raster) override;
  WebDragOperation DragTargetDragEnter(const WebDragData&,
                                       const WebFloatPoint& point_in_viewport,
                                       const WebFloatPoint& screen_point,
                                       WebDragOperationsMask operations_allowed,
                                       int modifiers) override;
  WebDragOperation DragTargetDragOver(const WebFloatPoint& point_in_viewport,
                                      const WebFloatPoint& screen_point,
                                      WebDragOperationsMask operations_allowed,
                                      int modifiers) override;
  void DragTargetDragLeave(const WebFloatPoint& point_in_viewport,
                           const WebFloatPoint& screen_point) override;
  void DragTargetDrop(const WebDragData&,
                      const WebFloatPoint& point_in_viewport,
                      const WebFloatPoint& screen_point,
                      int modifiers) override;
  void DragSourceEndedAt(const WebFloatPoint& point_in_viewport,
                         const WebFloatPoint& screen_point,
                         WebDragOperation) override;
  void DragSourceSystemDragEnded() override;

  WebLocalFrame* FocusedWebLocalFrameInWidget() const override;

  // Called when a drag-n-drop operation should begin.
  void StartDragging(network::mojom::ReferrerPolicy,
                     const WebDragData&,
                     WebDragOperationsMask,
                     const SkBitmap& drag_image,
                     const WebPoint& drag_image_offset);

  bool DoingDragAndDrop() { return doing_drag_and_drop_; }
  static void SetIgnoreInputEvents(bool value) { ignore_input_events_ = value; }
  static bool IgnoreInputEvents() { return ignore_input_events_; }

  // WebWidget methods.
  void DidAcquirePointerLock() override;
  void DidNotAcquirePointerLock() override;
  void DidLosePointerLock() override;
  void ShowContextMenu(WebMenuSourceType) override;

  // Image decode functionality.
  void RequestDecode(const PaintImage&, base::OnceCallback<void(bool)>);

  // Called when the FrameView for this Widget's local root is created.
  virtual void DidCreateLocalRootView() {}

  // This method returns the focused frame belonging to this WebWidget, that
  // is, a focused frame with the same local root as the one corresponding
  // to this widget. It will return nullptr if no frame is focused or, the
  // focused frame has a different local root.
  LocalFrame* FocusedLocalFrameInWidget() const;

  virtual void Trace(blink::Visitor*);

 protected:
  enum DragAction { kDragEnter, kDragOver };

  // Consolidate some common code between starting a drag over a target and
  // updating a drag over a target. If we're starting a drag, |isEntering|
  // should be true.
  WebDragOperation DragTargetDragEnterOrOver(
      const WebFloatPoint& point_in_viewport,
      const WebFloatPoint& screen_point,
      DragAction,
      int modifiers);

  // Helper function to call VisualViewport::viewportToRootFrame().
  WebFloatPoint ViewportToRootFrame(
      const WebFloatPoint& point_in_viewport) const;

  WebViewImpl* View() const;

  // Returns the page object associated with this widget. This may be null when
  // the page is shutting down, but will be valid at all other times.
  Page* GetPage() const;

  // Helper function to process events while pointer locked.
  void PointerLockMouseEvent(const WebCoalescedInputEvent&);

  virtual PageWidgetEventHandler* GetPageWidgetEventHandler() = 0;

  // A copy of the web drop data object we received from the browser.
  Member<DataObject> current_drag_data_;

  bool doing_drag_and_drop_ = false;

  // The available drag operations (copy, move link...) allowed by the source.
  WebDragOperation operations_allowed_ = kWebDragOperationNone;

  // The current drag operation as negotiated by the source and destination.
  // When not equal to DragOperationNone, the drag data can be dropped onto the
  // current drop target in this WebView (the drop target can accept the drop).
  WebDragOperation drag_operation_ = kWebDragOperationNone;

 private:
  void CancelDrag();

  WebWidgetClient* client_;

  // WebFrameWidget is associated with a subtree of the frame tree,
  // corresponding to a maximal connected tree of LocalFrames. This member
  // points to the root of that subtree.
  Member<WebLocalFrameImpl> local_root_;

  static bool ignore_input_events_;
  scoped_refptr<UserGestureToken> pointer_lock_gesture_token_;

  friend class WebViewImpl;
};

DEFINE_TYPE_CASTS(WebFrameWidgetBase, WebFrameWidget, widget, true, true);

}  // namespace blink

#endif
