/*
 * Copyright (C) 2007, 2009 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAGE_DRAG_CONTROLLER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAGE_DRAG_CONTROLLER_H_

#include "base/macros.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/events/event_target.h"
#include "third_party/blink/renderer/core/execution_context/context_lifecycle_observer.h"
#include "third_party/blink/renderer/core/page/drag_actions.h"
#include "third_party/blink/renderer/platform/geometry/int_point.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"

namespace blink {

class DataTransfer;
class Document;
class DragData;
class DragImage;
class DragState;
class LocalFrame;
class FloatRect;
class FrameSelection;
class HTMLInputElement;
class Node;
class Page;
class WebMouseEvent;

class CORE_EXPORT DragController final
    : public GarbageCollected<DragController>,
      public ContextLifecycleObserver {
  USING_GARBAGE_COLLECTED_MIXIN(DragController);

 public:
  explicit DragController(Page*);

  DragOperation DragEnteredOrUpdated(DragData*, LocalFrame& local_root);
  void DragExited(DragData*, LocalFrame& local_root);
  void PerformDrag(DragData*, LocalFrame& local_root);

  enum SelectionDragPolicy {
    kImmediateSelectionDragResolution,
    kDelayedSelectionDragResolution,
  };
  Node* DraggableNode(const LocalFrame*,
                      Node*,
                      const IntPoint&,
                      SelectionDragPolicy,
                      DragSourceAction&) const;
  void DragEnded();

  bool PopulateDragDataTransfer(LocalFrame* src,
                                const DragState&,
                                const IntPoint& drag_origin);
  bool StartDrag(LocalFrame* src,
                 const DragState&,
                 const WebMouseEvent& drag_event,
                 const IntPoint& drag_origin);

  DragState& GetDragState();

  static std::unique_ptr<DragImage> DragImageForSelection(LocalFrame&, float);

  // Return the selection bounds in absolute coordinates for the frame, clipped
  // to the visual viewport.
  static FloatRect ClippedSelection(const LocalFrame&);

  // ContextLifecycleObserver.
  void ContextDestroyed(ExecutionContext*) final;

  void Trace(blink::Visitor*) final;

 private:
  DispatchEventResult DispatchTextInputEventFor(LocalFrame*, DragData*);
  bool CanProcessDrag(DragData*, LocalFrame& local_root);
  bool ConcludeEditDrag(DragData*);
  DragOperation OperationForLoad(DragData*, LocalFrame& local_root);
  bool TryDocumentDrag(DragData*,
                       DragDestinationAction,
                       DragOperation&,
                       LocalFrame& local_root);
  bool TryDHTMLDrag(DragData*, DragOperation&, LocalFrame& local_root);
  DragOperation GetDragOperation(DragData*);
  // Clear the selection from the document this drag is exiting.
  void ClearDragCaret();
  bool DragIsMove(FrameSelection&, DragData*);
  bool IsCopyKeyDown(DragData*);

  void MouseMovedIntoDocument(Document*);

  // drag_location and drag_origin should be in the coordinate space of the
  // LocalFrame's contents.
  void DoSystemDrag(DragImage*,
                    const IntPoint& drag_location,
                    const IntPoint& drag_origin,
                    DataTransfer*,
                    LocalFrame*,
                    bool for_link);

  Member<Page> page_;

  // The document the mouse was last dragged over.
  Member<Document> document_under_mouse_;
  // The Document (if any) that initiated the drag.
  Member<Document> drag_initiator_;

  Member<DragState> drag_state_;

  Member<HTMLInputElement> file_input_element_under_mouse_;
  bool document_is_handling_drag_;

  DragDestinationAction drag_destination_action_;
  bool did_initiate_drag_;
  DISALLOW_COPY_AND_ASSIGN(DragController);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAGE_DRAG_CONTROLLER_H_
