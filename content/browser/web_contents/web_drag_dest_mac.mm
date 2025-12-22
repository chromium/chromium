// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#import "content/browser/web_contents/web_drag_dest_mac.h"

#include <AppKit/AppKit.h>
#import <Carbon/Carbon.h>

#include <algorithm>
#include <optional>

#include "base/apple/foundation_util.h"
#include "base/containers/span.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/sys_string_conversions.h"
#include "components/input/render_widget_host_input_event_router.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/browser/web_contents/web_contents_view_drag_security_info.h"
#include "content/common/web_contents_ns_view_bridge.mojom.h"
#include "content/public/browser/child_process_host.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents_view_delegate.h"
#include "content/public/browser/web_drag_dest_delegate.h"
#include "content/public/common/drop_data.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "ui/base/clipboard/clipboard_constants.h"
#include "ui/base/clipboard/clipboard_util_mac.h"
#include "ui/base/clipboard/custom_data_helper.h"
#include "ui/base/window_open_disposition.h"
#include "ui/gfx/geometry/point.h"

using blink::DragOperationsMask;
using content::DropData;
using content::OpenURLParams;
using content::Referrer;
using content::WebContentsImpl;
using remote_cocoa::mojom::DraggingInfo;

namespace content {

DropContext::DropContext(const DropData drop_data,
                         const gfx::PointF client_pt,
                         const gfx::PointF screen_pt,
                         int modifier_flags,
                         base::WeakPtr<RenderWidgetHostImpl> target_rwh)
    : drop_data(drop_data),
      client_pt(client_pt),
      screen_pt(screen_pt),
      modifier_flags(modifier_flags),
      target_rwh(target_rwh) {}

DropContext::DropContext(const DropContext& other) = default;
DropContext::DropContext(DropContext&& other) = default;

DropContext::~DropContext() = default;

}  // namespace content

namespace {

int GetModifierFlags() {
  int modifier_state = 0;
  UInt32 currentModifiers = GetCurrentKeyModifiers();
  if (currentModifiers & ::shiftKey)
    modifier_state |= blink::WebInputEvent::kShiftKey;
  if (currentModifiers & ::controlKey)
    modifier_state |= blink::WebInputEvent::kControlKey;
  if (currentModifiers & ::optionKey)
    modifier_state |= blink::WebInputEvent::kAltKey;
  if (currentModifiers & ::cmdKey)
    modifier_state |= blink::WebInputEvent::kMetaKey;

  // The return value of 1 << 0 corresponds to the left mouse button,
  // 1 << 1 corresponds to the right mouse button,
  // 1 << n, n >= 2 correspond to other mouse buttons.
  NSUInteger pressedButtons = [NSEvent pressedMouseButtons];

  if (pressedButtons & (1 << 0))
    modifier_state |= blink::WebInputEvent::kLeftButtonDown;
  if (pressedButtons & (1 << 1))
    modifier_state |= blink::WebInputEvent::kRightButtonDown;
  if (pressedButtons & (1 << 2))
    modifier_state |= blink::WebInputEvent::kMiddleButtonDown;

  return modifier_state;
}

void DropCompletionCallback(WebDragDest* drag_dest,
                            const content::DropContext context,
                            std::optional<content::DropData> drop_data) {
  // This is an async callback. Make sure RWH is still valid.
  if (!context.target_rwh) {
    [drag_dest handleAsyncDropAbortWithContext:context];
    return;
  }

  [drag_dest completeDropAsync:drop_data withContext:context];
}

}  // namespace

@implementation WebDragDest {
  // Our associated WebContentsImpl. Weak reference.
  raw_ptr<content::WebContentsImpl, DanglingUntriaged> _webContents;

  // Delegate; weak.
  raw_ptr<content::WebDragDestDelegate, DanglingUntriaged> _delegate;

  // Tracks the current RenderWidgetHost we're dragging over.
  base::WeakPtr<content::RenderWidgetHostImpl> _currentRWHForDrag;

  // Keep track of the render view host we're dragging over.  If it changes
  // during a drag, we need to re-send the DragEnter message.
  RenderViewHostIdentifier _currentRVH;

  // Holds the security info for the current drag.
  content::WebContentsViewDragSecurityInfo _dragSecurityInfo;

  // The unfiltered data for the current drag, or nullptr if none is in
  // progress.
  std::unique_ptr<content::DropData> _dropDataUnfiltered;

  // The data for the current drag, filtered by |currentRWHForDrag_|.
  std::unique_ptr<content::DropData> _dropDataFiltered;

  // True if the drag has been canceled.
  bool _canceled;

  // True for as long as `OnPerformingDrop` is pending, false otherwise.
  // This is used to properly order "dragend" after "drop" if the drop operation
  // is delayed by that callback being delayed.
  bool _dropInProgress;

  // Used to store closures passed in `endDrag`. This is called by
  // `completeDropAsync` after the "drop" event has fired.
  base::ScopedClosureRunner _endDragRunner;

  // Store DraggingInfo for async callbacks.
  std::unique_ptr<remote_cocoa::mojom::DraggingInfo> _pendingDragEnteredInfo;
  std::unique_ptr<remote_cocoa::mojom::DraggingInfo> _pendingDragUpdatedInfo;
  std::unique_ptr<remote_cocoa::mojom::DraggingInfo> _pendingDropInfo;

  // Store webContentsViewDelegate for async drop
  raw_ptr<content::WebContentsViewDelegate> _pendingDropDelegate;
}

// |contents| is the WebContentsImpl representing this tab, used to communicate
// drag&drop messages to WebCore and handle navigation on a successful drop
// (if necessary).
- (id)initWithWebContentsImpl:(WebContentsImpl*)contents {
  if ((self = [super init])) {
    _webContents = contents;
    _canceled = false;
    _dropInProgress = false;
    _pendingDropDelegate = nullptr;
  }
  return self;
}

- (DropData*)currentDropData {
  return _dropDataFiltered.get();
}

- (void)setDragDelegate:(content::WebDragDestDelegate*)delegate {
  _delegate = delegate;
}

- (void)handleAsyncDropAbortWithContext:(const content::DropContext&)context {
  // Called when DropCompletionCallback detects invalid RWH.
  // This happens when the target RWH is destroyed between drop start and
  // completion.
  if (_delegate) {
    _delegate->OnDragLeave();
  }

  _currentRWHForDrag.reset();
  _dropDataUnfiltered.reset();
  _dropDataFiltered.reset();
  _pendingDropInfo.reset();
  _pendingDropDelegate = nullptr;
  _dropInProgress = false;

  base::ScopedClosureRunner end_drag_runner(std::move(_endDragRunner));
}

// Call to set whether or not we should allow the drop. Takes effect the
// next time |-draggingUpdated:| is called.
- (void)setCurrentOperation:(ui::mojom::DragOperation)operation
     documentIsHandlingDrag:(bool)documentIsHandlingDrag {
  if (_dropDataUnfiltered) {
    _dropDataUnfiltered->operation = operation;
    _dropDataUnfiltered->document_is_handling_drag = documentIsHandlingDrag;
  }
  if (_dropDataFiltered) {
    _dropDataFiltered->operation = operation;
    _dropDataFiltered->document_is_handling_drag = documentIsHandlingDrag;
  }
}

// Given a point in window coordinates and a view in that window, return a
// flipped point in the coordinate system of |view|.
- (NSPoint)flipWindowPointToView:(const NSPoint&)windowPoint
                            view:(NSView*)view {
  DCHECK(view);
  NSPoint viewPoint =  [view convertPoint:windowPoint fromView:nil];
  NSRect viewFrame = [view frame];
  viewPoint.y = viewFrame.size.height - viewPoint.y;
  return viewPoint;
}

// Given a point in window coordinates and a view in that window, return a
// flipped point in screen coordinates.
- (NSPoint)flipWindowPointToScreen:(const NSPoint&)windowPoint
                              view:(NSView*)view {
  DCHECK(view);
  NSPoint screenPoint = [view.window convertPointToScreen:windowPoint];
  NSRect screenFrame = view.window.screen.frame;
  screenPoint.y = screenFrame.size.height - screenPoint.y;
  return screenPoint;
}

// Messages to send during the tracking of a drag, usually upon receiving
// calls from the view system. Communicates the drag messages to WebCore.

- (void)setDropData:(const DropData&)dropData {
  _dropDataUnfiltered = std::make_unique<DropData>(dropData);
}

- (NSDragOperation)draggingEntered:(const DraggingInfo*)info {
  if (_webContents->ShouldIgnoreInputEvents())
    return NSDragOperationNone;

  // Save off the RVH so we can tell if it changes during a drag. If it does,
  // we need to send a new enter message in draggingUpdated:.
  _currentRVH = _webContents->GetRenderViewHost();

  if (!_webContents->GetRenderWidgetHostView()) {
    // TODO(ekaramad, paulmeyer): Find a better way than toggling |canceled_|.
    // This could happen when the renderer process for the top-level RWH crashes
    // (see https://crbug.com/670645).
    _canceled = true;
    return NSDragOperationNone;
  }

  // Initiate async hit test (callback will handle actual drag enter).
  // Store DraggingInfo copy for the async callback.
  _pendingDragEnteredInfo =
      std::make_unique<remote_cocoa::mojom::DraggingInfo>(*info);

  auto callback = base::BindOnce(
      [](WebDragDest* drag_dest, remote_cocoa::mojom::DraggingInfo* info,
         base::WeakPtr<content::RenderWidgetHostViewBase> target,
         std::optional<gfx::PointF> transformedPoint) {
        if (drag_dest && transformedPoint.has_value()) {
          [drag_dest dragEnterHitTestDidComplete:info
                                      targetView:target
                                transformedPoint:transformedPoint.value()];
        }
      },
      self, _pendingDragEnteredInfo.get());

  _webContents->GetRenderWidgetHostAtPointAsynchronously(
      static_cast<content::RenderWidgetHostViewBase*>(
          _webContents->GetRenderWidgetHostView()),
      info->location_in_view, std::move(callback));

  // Return optimistic operation immediately for Mac's synchronous protocol.
  // The async callback will send the actual DragTargetDragEnter to renderer.
  if (_dropDataUnfiltered) {
    _dropDataUnfiltered->operation = ui::mojom::DragOperation::kCopy;
    _dropDataUnfiltered->document_is_handling_drag = true;
  }

  return NSDragOperationCopy;
}

- (void)dragEnterHitTestDidComplete:
            (const remote_cocoa::mojom::DraggingInfo*)info
                         targetView:
                             (base::WeakPtr<content::RenderWidgetHostViewBase>)
                                 target_view
                   transformedPoint:(const gfx::PointF&)transformedPoint {
  // Check if this callback is still valid - drag may have exited already.
  if (!_pendingDragEnteredInfo || _pendingDragEnteredInfo.get() != info) {
    return;
  }

  if (!target_view) {
    return;
  }

  auto* view_base = target_view.get();
  content::RenderWidgetHostImpl* targetRWH =
      content::RenderWidgetHostImpl::From(view_base->GetRenderWidgetHost());

  if (!_dragSecurityInfo.IsValidDragTarget(targetRWH)) {
    return;
  }

  // Filter |dropDataUnfiltered_| by targetRWH to populate |dropDataFiltered_|.
  DCHECK(_dropDataUnfiltered);
  std::unique_ptr<DropData> dropData =
      std::make_unique<DropData>(*_dropDataUnfiltered);
  _currentRWHForDrag = targetRWH->GetWeakPtr();
  _currentRWHForDrag->FilterDropData(dropData.get());

  NSDragOperation mask = info->operation_mask;

  // Give the delegate an opportunity to cancel the drag.
  if (auto* delegate = _webContents->GetDelegate()) {
    _canceled = !delegate->CanDragEnter(_webContents, *dropData,
                                        static_cast<DragOperationsMask>(mask));
  }

  if (_canceled) {
    return;
  }

  if (_delegate) {
    _delegate->DragInitialize(_webContents);
    _delegate->OnDragEnter();
  }

  _dropDataFiltered.swap(dropData);

  _currentRWHForDrag->DragTargetDragEnter(
      *_dropDataFiltered, transformedPoint, info->location_in_screen,
      static_cast<DragOperationsMask>(mask), GetModifierFlags(),
      base::DoNothing());
}

- (void)draggingExited {
  if (_webContents->ShouldIgnoreInputEvents())
    return;

  _webContents->PreHandleDragExit();

  if (!_dropDataFiltered || !_dropDataUnfiltered)
    return;

  DCHECK(_currentRVH);
  if (_currentRVH != _webContents->GetRenderViewHost())
    return;

  if (_canceled)
    return;

  if (_delegate)
    _delegate->OnDragLeave();

  if (_currentRWHForDrag) {
    _currentRWHForDrag->DragTargetDragLeave(gfx::PointF(), gfx::PointF());
    _currentRWHForDrag.reset();
  }
  _dropDataUnfiltered.reset();
  _dropDataFiltered.reset();
}

- (NSDragOperation)draggingUpdated:(const DraggingInfo*)info {
  if (_webContents->ShouldIgnoreInputEvents())
    return NSDragOperationNone;

  if (_canceled) {
    // TODO(ekaramad,paulmeyer): We probably shouldn't be checking for
    // |canceled_| twice in this method.
    return NSDragOperationNone;
  }

  if (_dropDataUnfiltered) {
    _webContents->PreHandleDragUpdate(*_dropDataUnfiltered,
                                      info->location_in_view);
  }

  if (!_dropDataFiltered || !_dropDataUnfiltered) {
    return NSDragOperationNone;
  }

  // Initiate async hit test (callback will handle target updates).
  // Store DraggingInfo copy for the async callback.
  _pendingDragUpdatedInfo =
      std::make_unique<remote_cocoa::mojom::DraggingInfo>(*info);

  auto callback = base::BindOnce(
      [](WebDragDest* drag_dest, remote_cocoa::mojom::DraggingInfo* info,
         base::WeakPtr<content::RenderWidgetHostViewBase> target,
         std::optional<gfx::PointF> transformedPoint) {
        if (drag_dest && transformedPoint.has_value()) {
          [drag_dest dragUpdateHitTestDidComplete:info
                                       targetView:target
                                 transformedPoint:transformedPoint.value()];
        }
      },
      self, _pendingDragUpdatedInfo.get());

  _webContents->GetRenderWidgetHostAtPointAsynchronously(
      static_cast<content::RenderWidgetHostViewBase*>(
          _webContents->GetRenderWidgetHostView()),
      info->location_in_view, std::move(callback));

  // Return optimistic operation immediately for Mac's synchronous protocol.
  // The async callback will send the actual DragTargetDragOver to renderer.
  return static_cast<NSDragOperation>(_dropDataUnfiltered->operation);
}

- (void)dragUpdateHitTestDidComplete:
            (const remote_cocoa::mojom::DraggingInfo*)info
                          targetView:
                              (base::WeakPtr<content::RenderWidgetHostViewBase>)
                                  target_view
                    transformedPoint:(const gfx::PointF&)transformedPoint {
  // Check if this callback is still valid - drag may have exited already.
  if (!_pendingDragUpdatedInfo || _pendingDragUpdatedInfo.get() != info) {
    return;
  }

  if (!target_view) {
    return;
  }

  if (_dropDataUnfiltered) {
    _webContents->PreHandleDragUpdate(*_dropDataUnfiltered, transformedPoint);
  }

  auto* view_base = target_view.get();
  content::RenderWidgetHostImpl* targetRWH =
      content::RenderWidgetHostImpl::From(view_base->GetRenderWidgetHost());

  if (!_dragSecurityInfo.IsValidDragTarget(targetRWH)) {
    return;
  }

  // TODO(paulmeyer): The dragging delegates may now by invoked multiple times
  // per drag, even without the drag ever leaving the window.
  if (targetRWH != _currentRWHForDrag.get()) {
    if (_currentRWHForDrag) {
      gfx::PointF transformedLeavePoint = info->location_in_view;
      gfx::PointF transformedScreenPoint = info->location_in_screen;
      content::RenderWidgetHostViewBase* rootView =
          static_cast<content::RenderWidgetHostViewBase*>(
              _webContents->GetRenderWidgetHostView());
      content::RenderWidgetHostViewBase* currentDragView =
          static_cast<content::RenderWidgetHostViewBase*>(
              _currentRWHForDrag->GetView());
      rootView->TransformPointToCoordSpaceForView(
          transformedLeavePoint, currentDragView, &transformedLeavePoint);
      rootView->TransformPointToCoordSpaceForView(
          transformedScreenPoint, currentDragView, &transformedScreenPoint);
      _currentRWHForDrag->DragTargetDragLeave(transformedLeavePoint,
                                              transformedScreenPoint);
    }

    // Re-enter with new target.
    [self dragEnterHitTestDidComplete:info
                           targetView:target_view
                     transformedPoint:transformedPoint];
    return;
  }

  if (!_dropDataFiltered) {
    return;
  }

  // Send drag over to renderer.
  NSDragOperation mask = info->operation_mask;
  targetRWH->DragTargetDragOver(transformedPoint, info->location_in_screen,
                                static_cast<DragOperationsMask>(mask),
                                GetModifierFlags(), base::DoNothing());

  if (_delegate) {
    _delegate->OnDragOver();
  }
}

- (void)AbortDropFromExternalEvent {
  // Called when drag is aborted from external event (e.g., draggingExited).
  // NOT used during async callback error paths.
  if (_delegate) {
    _delegate->OnDragLeave();
  }

  if (_currentRWHForDrag) {
    _currentRWHForDrag->DragTargetDragLeave(gfx::PointF(), gfx::PointF());
    _currentRWHForDrag.reset();
  }

  _dropDataUnfiltered.reset();
  _dropDataFiltered.reset();
  _pendingDropInfo.reset();
  _pendingDropDelegate = nullptr;
  _dropInProgress = false;

  base::ScopedClosureRunner end_drag_runner(std::move(_endDragRunner));
}

- (BOOL)performDragOperation:(const DraggingInfo*)info
    withWebContentsViewDelegate:
        (content::WebContentsViewDelegate*)webContentsViewDelegate {
  if (_webContents->ShouldIgnoreInputEvents()) {
    return NO;
  }

  // Store drop info and delegate for async callback.
  _pendingDropInfo = std::make_unique<DraggingInfo>(*info);
  _pendingDropDelegate = webContentsViewDelegate;

  // Set drop in progress to prevent endDrag from clearing state.
  _dropInProgress = true;

  // Create callback for async hit test.
  auto callback = base::BindOnce(
      [](WebDragDest* drag_dest, remote_cocoa::mojom::DraggingInfo* info,
         base::WeakPtr<content::RenderWidgetHostViewBase> target,
         std::optional<gfx::PointF> transformedPoint) {
        if (drag_dest && transformedPoint.has_value()) {
          [drag_dest dropHitTestDidComplete:info
                                 targetView:target
                           transformedPoint:transformedPoint.value()];
        }
      },
      self, _pendingDropInfo.get());

  // Trigger async hit test.
  _webContents->GetRenderWidgetHostAtPointAsynchronously(
      static_cast<content::RenderWidgetHostViewBase*>(
          _webContents->GetRenderWidgetHostView()),
      info->location_in_view, std::move(callback));

  // Return YES to indicate we're handling the drop (async).
  return YES;
}

- (void)dropHitTestDidComplete:(const remote_cocoa::mojom::DraggingInfo*)info
                    targetView:
                        (base::WeakPtr<content::RenderWidgetHostViewBase>)
                            target_view
              transformedPoint:(const gfx::PointF&)transformedPoint {
  // Check if this callback is still valid - drag may have been canceled.
  if (!_pendingDropInfo || _pendingDropInfo.get() != info) {
    // Reset flag if stale - the real drop may have already happened or been
    // canceled.
    _dropInProgress = false;
    return;
  }

  if (!target_view) {
    // Clean up and reset drop state.
    _pendingDropInfo.reset();
    _pendingDropDelegate = nullptr;
    _dropInProgress = false;
    // Send leave event to current target if any.
    if (_delegate) {
      _delegate->OnDragLeave();
    }
    if (_currentRWHForDrag) {
      _currentRWHForDrag->DragTargetDragLeave(gfx::PointF(), gfx::PointF());
      _currentRWHForDrag.reset();
    }
    // Run the end drag closure if pending.
    base::ScopedClosureRunner end_drag_runner(std::move(_endDragRunner));
    return;
  }

  auto* view_base = target_view.get();
  content::RenderWidgetHostImpl* targetRWH =
      content::RenderWidgetHostImpl::From(view_base->GetRenderWidgetHost());

  if (!_dragSecurityInfo.IsValidDragTarget(targetRWH)) {
    // Clean up and reset drop state.
    _pendingDropInfo.reset();
    _pendingDropDelegate = nullptr;
    _dropInProgress = false;
    // Send leave event.
    if (_delegate) {
      _delegate->OnDragLeave();
    }
    if (_currentRWHForDrag) {
      _currentRWHForDrag->DragTargetDragLeave(gfx::PointF(),
                                              info->location_in_screen);
      _currentRWHForDrag.reset();
    }
    // Run the end drag closure if pending.
    base::ScopedClosureRunner end_drag_runner(std::move(_endDragRunner));
    return;
  }

  // If target changed since last update, send leave to old and enter to new.
  if (targetRWH != _currentRWHForDrag.get()) {
    if (_currentRWHForDrag) {
      _currentRWHForDrag->DragTargetDragLeave(transformedPoint,
                                              info->location_in_screen);
    }

    // Filter drop data for new target.
    // For external drags, _dropDataUnfiltered might not be set, so check first.
    if (_dropDataUnfiltered) {
      std::unique_ptr<DropData> dropData =
          std::make_unique<DropData>(*_dropDataUnfiltered);
      _currentRWHForDrag = targetRWH->GetWeakPtr();
      _currentRWHForDrag->FilterDropData(dropData.get());
      _dropDataFiltered.swap(dropData);
    } else {
      // External drag without drop data - just update current target.
      _currentRWHForDrag = targetRWH->GetWeakPtr();
    }
  }

  _currentRVH = nullptr;
  _webContents->Focus();

  content::WebContentsViewDelegate* webContentsViewDelegate =
      _pendingDropDelegate;
  _pendingDropDelegate = nullptr;

  // If we don't have filtered data yet but have unfiltered data,
  // filter it now synchronously. This can happen if the async callbacks
  // from draggingEntered/draggingUpdated haven't completed yet.
  if (!_dropDataFiltered && _dropDataUnfiltered && targetRWH) {
    std::unique_ptr<DropData> dropData =
        std::make_unique<DropData>(*_dropDataUnfiltered);
    targetRWH->FilterDropData(dropData.get());
    _dropDataFiltered.swap(dropData);
  }

  if (webContentsViewDelegate) {
    // Need drop data for delegate callback.
    if (!_dropDataFiltered) {
      _dropInProgress = false;
      base::ScopedClosureRunner end_drag_runner(std::move(_endDragRunner));
      return;
    }
    content::DropContext context(/*drop_data=*/*_dropDataFiltered,
                                 /*client_pt=*/transformedPoint,
                                 /*screen_pt=*/info->location_in_screen,
                                 /*modifier_flags=*/GetModifierFlags(),
                                 /*target_rwh=*/targetRWH->GetWeakPtr());
    content::DropData drop_data = context.drop_data;
    // _dropInProgress already set in performDragOperation.
    webContentsViewDelegate->OnPerformingDrop(
        std::move(drop_data),
        base::BindOnce(&DropCompletionCallback, self, std::move(context)));
  } else {
    // No delegate - drop completes synchronously.
    if (_delegate)
      _delegate->OnDrop();
    // For drops with data, send to renderer.
    if (_dropDataFiltered) {
      targetRWH->DragTargetDrop(*_dropDataFiltered, transformedPoint,
                                info->location_in_screen, GetModifierFlags(),
                                base::DoNothing());
    }
    _dropInProgress = false;
  }

  _dropDataUnfiltered.reset();
  _dropDataFiltered.reset();
  _pendingDropInfo.reset();
}

- (void)completeDropAsync:(std::optional<content::DropData>)dropData
              withContext:(const content::DropContext)context {
  _dropInProgress = false;
  base::ScopedClosureRunner end_drag_runner(std::move(_endDragRunner));

  if (dropData.has_value()) {
    if (_delegate)
      _delegate->OnDrop();
    context.target_rwh->DragTargetDrop(
        dropData.value(), context.client_pt, context.screen_pt,
        context.modifier_flags, base::DoNothing());
  } else {
    if (_delegate)
      _delegate->OnDragLeave();
    context.target_rwh->DragTargetDragLeave(gfx::PointF(), gfx::PointF());
  }
}

- (content::RenderWidgetHostImpl*)
    GetRenderWidgetHostAtPoint:(const gfx::PointF&)viewPoint
                 transformedPt:(gfx::PointF*)transformedPt {
  auto* view =
      _webContents->GetInputEventRouter()->GetRenderWidgetHostViewInputAtPoint(
          _webContents->GetRenderViewHost()->GetWidget()->GetView(), viewPoint,
          transformedPt);
  if (!view) {
    return nullptr;
  }
  return content::RenderWidgetHostImpl::From(
      static_cast<content::RenderWidgetHostViewBase*>(view)
          ->GetRenderWidgetHost());
}

- (void)initiateDragWithRenderWidgetHost:(content::RenderWidgetHostImpl*)rwhi
                                dropData:(const content::DropData&)dropData {
  _dragSecurityInfo.OnDragInitiated(rwhi, dropData);
}

- (void)endDrag:(base::OnceClosure)closure {
  _dragSecurityInfo.OnDragEnded();

  // Clear pending drag info (entered/updated callbacks).
  // These are no longer needed after drag ends.
  _pendingDragEnteredInfo.reset();
  _pendingDragUpdatedInfo.reset();

  // Only clear drop state if no drop is in progress.
  // If drop is in progress, dropHitTestDidComplete needs these and will clear
  // them.
  if (!_dropInProgress) {
    _dropDataUnfiltered.reset();
    _dropDataFiltered.reset();
    _pendingDropInfo.reset();
    _pendingDropDelegate = nullptr;
  }

  if (_dropInProgress) {
    _endDragRunner.ReplaceClosure(std::move(closure));
  } else {
    std::move(closure).Run();
  }
}

- (void)resetDragDropState {
  _dropInProgress = false;
  auto end_drag_closure = _endDragRunner.Release();
  if (end_drag_closure) {
    std::move(end_drag_closure).Run();
  }
}

- (bool)dropInProgressForTesting {
  return _dropInProgress;
}

- (void)setDropInProgressForTesting {
  _dropInProgress = true;
}

@end

namespace content {

DropData PopulateDropDataFromPasteboard(NSPasteboard* pboard) {
  DCHECK(pboard);
  DropData drop_data;

  // https://crbug.com/1016740#c21
  NSArray* types = [pboard types];

  drop_data.did_originate_from_renderer =
      [types containsObject:ui::kUTTypeChromiumRendererInitiatedDrag];
  drop_data.is_from_privileged =
      [types containsObject:ui::kUTTypeChromiumPrivilegedInitiatedDrag];

  // Get URL if possible. To avoid exposing file system paths to web content,
  // filenames in the drag are not converted to file URLs.
  NSArray<URLAndTitle*>* urls_and_titles =
      ui::clipboard_util::URLsAndTitlesFromPasteboard(pboard,
                                                      /*include_files=*/false);
  for (URLAndTitle* url_and_title in urls_and_titles) {
    drop_data.url_infos.push_back(
        ui::ClipboardUrlInfo{GURL(base::SysNSStringToUTF8(url_and_title.URL)),
                             base::SysNSStringToUTF16(url_and_title.title)});
  }

  // Get plain text.
  if ([types containsObject:NSPasteboardTypeString]) {
    drop_data.text =
        base::SysNSStringToUTF16([pboard stringForType:NSPasteboardTypeString]);
  }

  // Get HTML. If there's no HTML, try RTF.
  if ([types containsObject:NSPasteboardTypeHTML]) {
    NSString* html = [pboard stringForType:NSPasteboardTypeHTML];
    drop_data.html = base::SysNSStringToUTF16(html);
  } else if ([types containsObject:ui::kUTTypeChromiumImageAndHtml]) {
    NSString* html = [pboard stringForType:ui::kUTTypeChromiumImageAndHtml];
    drop_data.html = base::SysNSStringToUTF16(html);
  } else if ([types containsObject:NSPasteboardTypeRTF]) {
    NSString* html = ui::clipboard_util::GetHTMLFromRTFOnPasteboard(pboard);
    drop_data.html = base::SysNSStringToUTF16(html);
  }

  // Get files.
  drop_data.filenames = ui::clipboard_util::FilesFromPasteboard(pboard);

  // Get custom MIME data.
  if ([types containsObject:ui::kUTTypeChromiumDataTransferCustomData]) {
    NSData* customData =
        [pboard dataForType:ui::kUTTypeChromiumDataTransferCustomData];
    if (std::optional<std::unordered_map<std::u16string, std::u16string>>
            maybe_custom_data = ui::ReadCustomDataIntoMap(
                base::apple::NSDataToSpan(customData));
        maybe_custom_data) {
      drop_data.custom_data = std::move(*maybe_custom_data);
    }
  }

  return drop_data;
}

}  // namespace content
