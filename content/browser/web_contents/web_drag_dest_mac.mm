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

void OnWebContentsViewDelegatePerformingDropComplete(
    WebDragDest* drag_dest,
    const content::DropContext context,
    std::optional<content::DropData> drop_data) {
  // This is an async callback. Make sure RWH is still valid.
  if (!context.target_rwh) {
    [drag_dest cleanupDragState];
    return;
  }

  [drag_dest finishDropWithData:drop_data context:context];
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

  // Sequence numbers to track validity of async callbacks.
  // Using uint64_t to avoid overflow in realistic usage.
  uint64_t _dragEnteredSequenceNumber;
  uint64_t _dragUpdatedSequenceNumber;
  uint64_t _dropSequenceNumber;

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
    _dragEnteredSequenceNumber = 0;
    _dragUpdatedSequenceNumber = 0;
    _dropSequenceNumber = 0;
  }
  return self;
}

- (DropData*)currentDropData {
  return _dropDataFiltered.get();
}

- (void)setDragDelegate:(content::WebDragDestDelegate*)delegate {
  _delegate = delegate;
}

// Helper method to cleanup drag state consistently across error paths.
// This ensures all state is properly reset when a drag operation fails or
// needs to abort, preventing state leaks and inconsistencies.
- (void)cleanupDragState {
  _currentRWHForDrag.reset();

  _dropDataUnfiltered.reset();
  _dropDataFiltered.reset();
  _pendingDragEnteredInfo.reset();
  _pendingDragUpdatedInfo.reset();
  _pendingDropInfo.reset();
  _pendingDropDelegate = nullptr;
  _dropInProgress = false;
  _canceled = false;

  // Invalidate pending async callbacks by incrementing sequence numbers.
  ++_dragEnteredSequenceNumber;
  ++_dragUpdatedSequenceNumber;
  ++_dropSequenceNumber;

  // Release any pending endDrag closure without running it.
  // This is an abort/error path - we don't want to fire dragend events.
  std::ignore = _endDragRunner.Release();
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
  // Increment sequence number to track this specific operation.
  uint64_t current_sequence_number = ++_dragEnteredSequenceNumber;

  __weak WebDragDest* weak_self = self;
  auto callback = base::BindOnce(
      [](WebDragDest* __weak weak_drag_dest, uint64_t sequence_no,
         base::WeakPtr<content::RenderWidgetHostViewBase> target,
         std::optional<gfx::PointF> transformedPoint) {
        WebDragDest* drag_dest = weak_drag_dest;
        if (drag_dest && transformedPoint.has_value()) {
          [drag_dest dragEnterHitTestDidCompleteForView:target
                                       transformedPoint:transformedPoint.value()
                                         sequenceNumber:sequence_no];
        }
      },
      weak_self, current_sequence_number);

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

- (void)dragEnterHitTestDidCompleteForView:
            (base::WeakPtr<content::RenderWidgetHostViewBase>)target_view
                          transformedPoint:(const gfx::PointF&)transformedPoint
                            sequenceNumber:(uint64_t)sequence_no {
  // Check if this callback is still valid - drag may have exited already.
  // Compare sequence number instead of pointer to avoid dangling pointer
  // issues.
  if (!_pendingDragEnteredInfo || sequence_no != _dragEnteredSequenceNumber) {
    return;
  }

  // Safe to access _pendingDragEnteredInfo now that sequence is validated.
  const remote_cocoa::mojom::DraggingInfo* info = _pendingDragEnteredInfo.get();

  if (!target_view) {
    // Clean up state on invalid target.
    [self cleanupDragState];
    return;
  }

  auto* view_base = target_view.get();
  auto* rwh = view_base->GetRenderWidgetHost();
  if (!rwh) {
    [self cleanupDragState];
    return;
  }
  content::RenderWidgetHostImpl* targetRWH =
      content::RenderWidgetHostImpl::From(rwh);

  if (!_dragSecurityInfo.IsValidDragTarget(targetRWH)) {
    // Clean up state on invalid security.
    [self cleanupDragState];
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

  // Invalidate all pending async callbacks by incrementing sequence numbers.
  // This prevents stale callbacks from executing after the drag has exited.
  // We increment instead of resetting to 0 to avoid collisions if a new drag
  // starts immediately (e.g., rapid enter/exit/enter would otherwise have
  // matching sequence numbers).
  ++_dragEnteredSequenceNumber;
  ++_dragUpdatedSequenceNumber;
  ++_dropSequenceNumber;

  // Clear pending drag info to prevent callbacks from accessing stale data.
  _pendingDragEnteredInfo.reset();
  _pendingDragUpdatedInfo.reset();

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
  // Increment sequence number to track this specific operation.
  uint64_t current_sequence_number = ++_dragUpdatedSequenceNumber;

  __weak WebDragDest* weak_self = self;
  auto callback = base::BindOnce(
      [](WebDragDest* __weak weak_drag_dest, uint64_t sequence_no,
         base::WeakPtr<content::RenderWidgetHostViewBase> target,
         std::optional<gfx::PointF> transformedPoint) {
        WebDragDest* drag_dest = weak_drag_dest;
        if (drag_dest && transformedPoint.has_value()) {
          [drag_dest
              dragUpdateHitTestDidCompleteForView:target
                                 transformedPoint:transformedPoint.value()
                                   sequenceNumber:sequence_no];
        }
      },
      weak_self, current_sequence_number);

  _webContents->GetRenderWidgetHostAtPointAsynchronously(
      static_cast<content::RenderWidgetHostViewBase*>(
          _webContents->GetRenderWidgetHostView()),
      info->location_in_view, std::move(callback));

  // Return optimistic operation immediately for Mac's synchronous protocol.
  // The async callback will send the actual DragTargetDragOver to renderer.
  return static_cast<NSDragOperation>(_dropDataUnfiltered->operation);
}

- (void)dragUpdateHitTestDidCompleteForView:
            (base::WeakPtr<content::RenderWidgetHostViewBase>)target_view
                           transformedPoint:(const gfx::PointF&)transformedPoint
                             sequenceNumber:(uint64_t)sequence_no {
  // Check if this callback is still valid - drag may have exited already.
  // Compare sequence number instead of pointer to avoid dangling pointer
  // issues.
  if (!_pendingDragUpdatedInfo || sequence_no != _dragUpdatedSequenceNumber) {
    return;
  }

  // Safe to access _pendingDragUpdatedInfo now that sequence is validated.
  const remote_cocoa::mojom::DraggingInfo* info = _pendingDragUpdatedInfo.get();

  if (!target_view) {
    // Clean up state on invalid target.
    [self cleanupDragState];
    return;
  }

  auto* view_base = target_view.get();
  auto* rwh = view_base->GetRenderWidgetHost();
  if (!rwh) {
    [self cleanupDragState];
    return;
  }
  content::RenderWidgetHostImpl* targetRWH =
      content::RenderWidgetHostImpl::From(rwh);

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

    // Re-enter with new target only if drag state is still valid.
    // If draggingExited was called, _dropDataUnfiltered will be null and
    // dragEnterHitTestDidComplete expects it to exist.
    if (!_dropDataUnfiltered) {
      return;
    }

    // Re-enter with new target. Set up _pendingDragEnteredInfo so that
    // dragEnterHitTestDidCompleteForView can validate and access it.
    _pendingDragEnteredInfo =
        std::make_unique<remote_cocoa::mojom::DraggingInfo>(
            *_pendingDragUpdatedInfo);
    [self dragEnterHitTestDidCompleteForView:target_view
                            transformedPoint:transformedPoint
                              sequenceNumber:++_dragEnteredSequenceNumber];
    // Continue to send DragOver to the new target after re-entry.
  }

  if (!_dropDataFiltered) {
    return;
  }

  if (_canceled) {
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

- (BOOL)performDragOperation:(const DraggingInfo*)info
    withWebContentsViewDelegate:
        (content::WebContentsViewDelegate*)webContentsViewDelegate {
  if (_webContents->ShouldIgnoreInputEvents()) {
    return NO;
  }

  // Store drop info and delegate for async callback.
  _pendingDropInfo = std::make_unique<DraggingInfo>(*info);
  _pendingDropDelegate = webContentsViewDelegate;
  // Increment sequence number to track this specific operation.
  uint64_t current_sequence_number = ++_dropSequenceNumber;

  // Set drop in progress to prevent endDrag from clearing state.
  _dropInProgress = true;

  // Create callback for async hit test.
  __weak WebDragDest* weak_self = self;
  auto callback = base::BindOnce(
      [](WebDragDest* __weak weak_drag_dest, uint64_t sequence_no,
         base::WeakPtr<content::RenderWidgetHostViewBase> target,
         std::optional<gfx::PointF> transformedPoint) {
        WebDragDest* drag_dest = weak_drag_dest;
        if (drag_dest && transformedPoint.has_value()) {
          [drag_dest dropHitTestDidCompleteForView:target
                                  transformedPoint:transformedPoint.value()
                                    sequenceNumber:sequence_no];
        }
      },
      weak_self, current_sequence_number);

  // Trigger async hit test.
  _webContents->GetRenderWidgetHostAtPointAsynchronously(
      static_cast<content::RenderWidgetHostViewBase*>(
          _webContents->GetRenderWidgetHostView()),
      info->location_in_view, std::move(callback));

  // Return YES to indicate we're handling the drop (async).
  return YES;
}

- (void)dropHitTestDidCompleteForView:
            (base::WeakPtr<content::RenderWidgetHostViewBase>)target_view
                     transformedPoint:(const gfx::PointF&)transformedPoint
                       sequenceNumber:(uint64_t)sequence_no {
  // Check if this callback is still valid - drag may have been canceled.
  // Compare sequence number instead of pointer to avoid dangling pointer
  // issues.
  if (!_pendingDropInfo || sequence_no != _dropSequenceNumber) {
    // Reset flag if stale - the real drop may have already happened or been
    // canceled.
    _dropInProgress = false;
    base::ScopedClosureRunner end_drag_runner(std::move(_endDragRunner));
    return;
  }

  // Safe to access _pendingDropInfo now that sequence is validated.
  const remote_cocoa::mojom::DraggingInfo* info = _pendingDropInfo.get();

  if (!target_view) {
    [self cleanupDragState];
    return;
  }

  auto* view_base = target_view.get();
  auto* rwh = view_base->GetRenderWidgetHost();
  if (!rwh) {
    [self cleanupDragState];
    return;
  }
  content::RenderWidgetHostImpl* targetRWH =
      content::RenderWidgetHostImpl::From(rwh);

  if (!_dragSecurityInfo.IsValidDragTarget(targetRWH)) {
    // Clean up and reset drop state using consistent helper method.
    [self cleanupDragState];
    return;
  }

  // If target changed since last update, send leave to old and enter to new.
  if (targetRWH != _currentRWHForDrag.get()) {
    if (_currentRWHForDrag) {
      _currentRWHForDrag->DragTargetDragLeave(transformedPoint,
                                              info->location_in_screen);
    }

    // Re-enter with new target. Set up state for
    // dragEnterHitTestDidCompleteForView. Copy drop info to drag entered info
    // so the method can validate it.
    if (_dropDataUnfiltered) {
      _pendingDragEnteredInfo =
          std::make_unique<remote_cocoa::mojom::DraggingInfo>(
              *_pendingDropInfo);
      [self dragEnterHitTestDidCompleteForView:target_view
                              transformedPoint:transformedPoint
                                sequenceNumber:++_dragEnteredSequenceNumber];
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
        base::BindOnce(&OnWebContentsViewDelegatePerformingDropComplete, self,
                       std::move(context)));
  } else {
    // No delegate - drop completes synchronously.
    _dropInProgress = false;
    base::ScopedClosureRunner end_drag_runner(std::move(_endDragRunner));

    if (_delegate)
      _delegate->OnDrop();
    // For drops with data, send to renderer.
    if (_dropDataFiltered) {
      targetRWH->DragTargetDrop(*_dropDataFiltered, transformedPoint,
                                info->location_in_screen, GetModifierFlags(),
                                base::DoNothing());
    }
  }

  _dropDataUnfiltered.reset();
  _dropDataFiltered.reset();
  _pendingDropInfo.reset();
}

- (void)finishDropWithData:(std::optional<content::DropData>)dropData
                   context:(const content::DropContext)context {
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

- (void)initiateDragWithRenderWidgetHost:(content::RenderWidgetHostImpl*)rwhi
                                dropData:(const content::DropData&)dropData {
  _dragSecurityInfo.OnDragInitiated(rwhi, dropData);
}

- (void)endDrag:(base::OnceClosure)closure {
  _dragSecurityInfo.OnDragEnded();

  // Invalidate pending enter/update async callbacks by incrementing sequences.
  // This prevents stale callbacks from executing after drag ends.
  ++_dragEnteredSequenceNumber;
  ++_dragUpdatedSequenceNumber;

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
