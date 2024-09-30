/*
 * Copyright (C) 2007, 2009, 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Google Inc.
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

#include "third_party/blink/renderer/core/page/drag_controller.h"

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "build/build_config.h"
#include "third_party/blink/public/common/page/drag_operation.h"
#include "third_party/blink/public/mojom/frame/user_activation_notification_type.mojom-blink.h"
#include "third_party/blink/public/platform/web_common.h"
#include "third_party/blink/public/platform/web_drag_data.h"
#include "third_party/blink/renderer/core/clipboard/data_object.h"
#include "third_party/blink/renderer/core/clipboard/data_transfer.h"
#include "third_party/blink/renderer/core/clipboard/data_transfer_access_policy.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/document_fragment.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/editing/commands/drag_and_drop_command.h"
#include "third_party/blink/renderer/core/editing/drag_caret.h"
#include "third_party/blink/renderer/core/editing/editing_utilities.h"
#include "third_party/blink/renderer/core/editing/editor.h"
#include "third_party/blink/renderer/core/editing/ephemeral_range.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/editing/selection_template.h"
#include "third_party/blink/renderer/core/editing/serializers/serialization.h"
#include "third_party/blink/renderer/core/events/text_event.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/frame/visual_viewport.h"
#include "third_party/blink/renderer/core/html/forms/html_form_element.h"
#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
#include "third_party/blink/renderer/core/html/html_anchor_element.h"
#include "third_party/blink/renderer/core/html/html_plugin_element.h"
#include "third_party/blink/renderer/core/html/plugin_document.h"
#include "third_party/blink/renderer/core/input/event_handler.h"
#include "third_party/blink/renderer/core/input_type_names.h"
#include "third_party/blink/renderer/core/layout/hit_test_request.h"
#include "third_party/blink/renderer/core/layout/hit_test_result.h"
#include "third_party/blink/renderer/core/layout/layout_image.h"
#include "third_party/blink/renderer/core/layout/layout_theme.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/loader/frame_load_request.h"
#include "third_party/blink/renderer/core/loader/frame_loader.h"
#include "third_party/blink/renderer/core/loader/resource/image_resource_content.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/drag_data.h"
#include "third_party/blink/renderer/core/page/drag_image.h"
#include "third_party/blink/renderer/core/page/drag_state.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/graphics/image.h"
#include "third_party/blink/renderer/platform/graphics/image_orientation.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_record_builder.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_request.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/shared_buffer.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom-blink.h"
#include "ui/display/screen_info.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/vector2d_conversions.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>
#endif

namespace blink {

using mojom::blink::FormControlType;
using ui::mojom::blink::DragOperation;

static const int kMaxOriginalImageArea = 1500 * 1500;
static const int kLinkDragBorderInset = 2;
#if BUILDFLAG(IS_ANDROID)
// Android handles drag image transparency at the browser level
static const float kDragImageAlpha = 1.00f;
#else
static const float kDragImageAlpha = 0.75f;
#endif

#if DCHECK_IS_ON()
static bool DragTypeIsValid(DragSourceAction action) {
  switch (action) {
    case kDragSourceActionDHTML:
    case kDragSourceActionImage:
    case kDragSourceActionLink:
    case kDragSourceActionSelection:
      return true;
    case kDragSourceActionNone:
      return false;
  }
  // Make sure MSVC doesn't complain that not all control paths return a value.
  NOTREACHED_IN_MIGRATION();
  return false;
}
#endif  // DCHECK_IS_ON()

static WebMouseEvent CreateMouseEvent(DragData* drag_data) {
  WebMouseEvent result(
      WebInputEvent::Type::kMouseMove, drag_data->ClientPosition(),
      drag_data->GlobalPosition(), WebPointerProperties::Button::kLeft, 0,
      static_cast<WebInputEvent::Modifiers>(drag_data->GetModifiers()),
      base::TimeTicks::Now());
  result.SetFrameScale(1);
  return result;
}

static DataTransfer* CreateDraggingDataTransfer(DataTransferAccessPolicy policy,
                                                DragData* drag_data) {
  return DataTransfer::Create(DataTransfer::kDragAndDrop, policy,
                              drag_data->PlatformData());
}

DragController::DragController(Page* page)
    : ExecutionContextLifecycleObserver(
          static_cast<ExecutionContext*>(nullptr)),
      page_(page),
      document_under_mouse_(nullptr),
      drag_initiator_(nullptr),
      file_input_element_under_mouse_(nullptr),
      document_is_handling_drag_(false),
      drag_destination_action_(kDragDestinationActionNone),
      did_initiate_drag_(false) {}

static DocumentFragment* DocumentFragmentFromDragData(
    DragData* drag_data,
    LocalFrame* frame,
    Range* context,
    bool allow_plain_text,
    DragSourceType& drag_source_type,
    bool is_richly_editable_position) {
  DCHECK(drag_data);
  CHECK(is_richly_editable_position ||
        RuntimeEnabledFeatures::
            DropUrlAsPlainTextInPlainTextOnlyEditablePositionEnabled());
  drag_source_type = DragSourceType::kHTMLSource;

  Document& document = context->OwnerDocument();
  if (drag_data->ContainsCompatibleContent()) {
    if (DocumentFragment* fragment = drag_data->AsFragment(frame))
      return fragment;

    if (is_richly_editable_position &&
        drag_data->ContainsURL(DragData::kDoNotConvertFilenames)) {
      String title;
      String url = drag_data->AsURL(DragData::kDoNotConvertFilenames, &title);
      if (!url.empty()) {
        auto* anchor = MakeGarbageCollected<HTMLAnchorElement>(document);
        anchor->SetHref(AtomicString(url));
        if (title.empty()) {
          // Try the plain text first because the url might be normalized or
          // escaped.
          if (drag_data->ContainsPlainText())
            title = drag_data->AsPlainText();
          if (title.empty())
            title = url;
        }
        Node* anchor_text = document.createTextNode(title);
        anchor->AppendChild(anchor_text);
        DocumentFragment* fragment = document.createDocumentFragment();
        fragment->AppendChild(anchor);
        return fragment;
      }
    }
  }
  if (allow_plain_text && drag_data->ContainsPlainText()) {
    drag_source_type = DragSourceType::kPlainTextSource;
    return CreateFragmentFromText(EphemeralRange(context),
                                  drag_data->AsPlainText());
  }

  return nullptr;
}

bool DragController::DragIsMove(FrameSelection& selection,
                                DragData* drag_data) {
  return document_under_mouse_ ==
             (drag_initiator_ ? drag_initiator_->document() : nullptr) &&
         selection.SelectionHasFocus() &&
         selection.ComputeVisibleSelectionInDOMTreeDeprecated()
             .IsContentEditable() &&
         selection.ComputeVisibleSelectionInDOMTreeDeprecated().IsRange() &&
         !IsCopyKeyDown(drag_data);
}

void DragController::ClearDragCaret() {
  page_->GetDragCaret().Clear();
}

void DragController::DragEnded() {
  drag_initiator_ = nullptr;
  did_initiate_drag_ = false;
  page_->GetDragCaret().Clear();
}

void DragController::DragExited(DragData* drag_data, LocalFrame& local_root) {
  DCHECK(drag_data);

  LocalFrameView* frame_view(local_root.View());
  if (frame_view) {
    DataTransferAccessPolicy policy = DataTransferAccessPolicy::kTypesReadable;
    DataTransfer* data_transfer = CreateDraggingDataTransfer(policy, drag_data);
    data_transfer->SetSourceOperation(drag_data->DraggingSourceOperationMask());
    local_root.GetEventHandler().CancelDragAndDrop(CreateMouseEvent(drag_data),
                                                   data_transfer);
    data_transfer->SetAccessPolicy(
        DataTransferAccessPolicy::kNumb);  // invalidate clipboard here for
                                           // security
  }
  MouseMovedIntoDocument(nullptr);
  if (file_input_element_under_mouse_)
    file_input_element_under_mouse_->SetCanReceiveDroppedFiles(false);
  file_input_element_under_mouse_ = nullptr;
}

void DragController::PerformDrag(DragData* drag_data, LocalFrame& local_root) {
  DCHECK(drag_data);
  document_under_mouse_ = local_root.DocumentAtPoint(
      PhysicalOffset::FromPointFRound(drag_data->ClientPosition()));
  LocalFrame::NotifyUserActivation(
      document_under_mouse_ ? document_under_mouse_->GetFrame() : nullptr,
      mojom::blink::UserActivationNotificationType::kInteraction);
  if ((drag_destination_action_ & kDragDestinationActionDHTML) &&
      document_is_handling_drag_) {
    bool prevented_default = false;
    if (drag_data->ForceDefaultAction()) {
      // Tell the document that the drag has left the building.
      DragExited(drag_data, local_root);
    } else if (local_root.View()) {
      // Sending an event can result in the destruction of the view and part.
      DataTransfer* data_transfer = CreateDraggingDataTransfer(
          DataTransferAccessPolicy::kReadable, drag_data);
      data_transfer->SetSourceOperation(
          drag_data->DraggingSourceOperationMask());
      EventHandler& event_handler = local_root.GetEventHandler();
      prevented_default = event_handler.PerformDragAndDrop(
                              CreateMouseEvent(drag_data), data_transfer) !=
                          WebInputEventResult::kNotHandled;
      if (!prevented_default && document_under_mouse_) {
        // When drop target is plugin element and it can process drag, we
        // should prevent default behavior.
        const HitTestLocation location(local_root.View()->ConvertFromRootFrame(
            PhysicalOffset::FromPointFRound(drag_data->ClientPosition())));
        const HitTestResult result =
            event_handler.HitTestResultAtLocation(location);
        auto* html_plugin_element =
            DynamicTo<HTMLPlugInElement>(result.InnerNode());
        prevented_default |=
            html_plugin_element && html_plugin_element->CanProcessDrag();
      }

      // Invalidate clipboard here for security.
      data_transfer->SetAccessPolicy(DataTransferAccessPolicy::kNumb);
    }
    if (prevented_default) {
      document_under_mouse_ = nullptr;
      ClearDragCaret();
      return;
    }
  }

  if ((drag_destination_action_ & kDragDestinationActionEdit) &&
      ConcludeEditDrag(drag_data)) {
    document_under_mouse_ = nullptr;
    return;
  }

  if (OperationForLoad(drag_data, local_root) != DragOperation::kNone) {
    ResourceRequest resource_request(drag_data->AsURL());
    resource_request.SetHasUserGesture(LocalFrame::HasTransientUserActivation(
        document_under_mouse_ ? document_under_mouse_->GetFrame() : nullptr));

    // Use a unique origin to match other navigations that are initiated
    // outside of a renderer process (e.g. omnibox navigations).  Here, the
    // initiator of the navigation is a user dragging files from *outside* of
    // the current page.  See also https://crbug.com/930049.
    //
    // TODO(crbug.com/331733543): Once supported, use the source of the drag as
    // the initiator of the navigation below.
    resource_request.SetRequestorOrigin(SecurityOrigin::CreateUniqueOpaque());

    FrameLoadRequest request(nullptr, resource_request);

    // Open the dropped URL in a new tab to avoid potential data-loss in the
    // current tab. See https://crbug.com/451659.
    request.SetNavigationPolicy(
        NavigationPolicy::kNavigationPolicyNewForegroundTab);
    local_root.Navigate(request, WebFrameLoadType::kStandard);
  }

  document_under_mouse_ = nullptr;
}

void DragController::MouseMovedIntoDocument(Document* new_document) {
  if (document_under_mouse_ == new_document)
    return;

  // If we were over another document clear the selection
  if (document_under_mouse_)
    ClearDragCaret();
  document_under_mouse_ = new_document;
}

DragController::Operation DragController::DragEnteredOrUpdated(
    DragData* drag_data,
    LocalFrame& local_root) {
  DCHECK(drag_data);

  MouseMovedIntoDocument(local_root.DocumentAtPoint(
      PhysicalOffset::FromPointFRound(drag_data->ClientPosition())));

  // TODO(crbug.com/331682039): Replace `AcceptsLoadDrops` with a Setting used
  // in core.
  drag_destination_action_ =
      page_->GetChromeClient().AcceptsLoadDrops()
          ? kDragDestinationActionAny
          : static_cast<DragDestinationAction>(kDragDestinationActionDHTML |
                                               kDragDestinationActionEdit);

  Operation drag_operation;
  document_is_handling_drag_ =
      TryDocumentDrag(drag_data, drag_destination_action_,
                      drag_operation.operation, local_root);
  if (!document_is_handling_drag_ &&
      (drag_destination_action_ & kDragDestinationActionLoad)) {
    drag_operation.operation = OperationForLoad(drag_data, local_root);
  }

  drag_operation.document_is_handling_drag = document_is_handling_drag_;
  return drag_operation;
}

static HTMLInputElement* AsFileInput(Node* node) {
  DCHECK(node);
  for (; node; node = node->OwnerShadowHost()) {
    auto* html_input_element = DynamicTo<HTMLInputElement>(node);
    if (html_input_element &&
        html_input_element->FormControlType() == FormControlType::kInputFile) {
      return html_input_element;
    }
  }
  return nullptr;
}

// This can return null if an empty document is loaded.
static Element* ElementUnderMouse(Document* document_under_mouse,
                                  const PhysicalOffset& point) {
  HitTestRequest request(HitTestRequest::kReadOnly | HitTestRequest::kActive);
  HitTestLocation location(point);
  HitTestResult result(request, location);
  document_under_mouse->GetLayoutView()->HitTest(location, result);

  Node* n = result.InnerNode();
  while (n && !n->IsElementNode())
    n = n->ParentOrShadowHostNode();
  if (n && n->IsInShadowTree())
    n = n->OwnerShadowHost();

  return To<Element>(n);
}

bool DragController::TryDocumentDrag(DragData* drag_data,
                                     DragDestinationAction action_mask,
                                     DragOperation& drag_operation,
                                     LocalFrame& local_root) {
  DCHECK(drag_data);

  if (!document_under_mouse_)
    return false;

  // This is the renderer-side check for https://crbug.com/59081 to prevent
  // drags between cross-origin frames within the same page. This logic relies
  // on the browser process to have already filtered out any drags that might
  // span distinct `blink::Page` objects but still be part of the same logical
  // page. Otherwise, `drag_initiator_` will be null here and the drag will
  // incorrectly be allowed to proceed.
  //
  // Note: One example where the drag start frame and the drop target frame can
  // be part of the same logical page, but belong to different `blink::Page`
  // instances is if the two frames are hosted in different renderer processes.
  auto* under_mouse_origin =
      document_under_mouse_->GetExecutionContext()->GetSecurityOrigin();
  if (drag_initiator_ &&
      !under_mouse_origin->CanAccess(drag_initiator_->GetSecurityOrigin())) {
    return false;
  }

  bool is_handling_drag = false;
  if (action_mask & kDragDestinationActionDHTML) {
    is_handling_drag = TryDHTMLDrag(drag_data, drag_operation, local_root);
    // Do not continue if m_documentUnderMouse has been reset by tryDHTMLDrag.
    // tryDHTMLDrag fires dragenter event. The event listener that listens
    // to this event may create a nested run loop (open a modal dialog),
    // which could process dragleave event and reset m_documentUnderMouse in
    // dragExited.
    if (!document_under_mouse_)
      return false;
  }

  // It's unclear why this check is after tryDHTMLDrag.
  // We send drag events in tryDHTMLDrag and that may be the reason.
  LocalFrameView* frame_view = document_under_mouse_->View();
  if (!frame_view)
    return false;

  if (is_handling_drag) {
    page_->GetDragCaret().Clear();
    return true;
  }

  if ((action_mask & kDragDestinationActionEdit) &&
      CanProcessDrag(drag_data, local_root)) {
    PhysicalOffset point = frame_view->ConvertFromRootFrame(
        PhysicalOffset::FromPointFRound(drag_data->ClientPosition()));
    Element* element = ElementUnderMouse(document_under_mouse_.Get(), point);
    if (!element)
      return false;

    HTMLInputElement* element_as_file_input = AsFileInput(element);
    if (file_input_element_under_mouse_ != element_as_file_input) {
      if (file_input_element_under_mouse_)
        file_input_element_under_mouse_->SetCanReceiveDroppedFiles(false);
      file_input_element_under_mouse_ = element_as_file_input;
    }

    if (!file_input_element_under_mouse_) {
      page_->GetDragCaret().SetCaretPosition(
          document_under_mouse_->GetFrame()->PositionForPoint(point));
    }

    LocalFrame* inner_frame = element->GetDocument().GetFrame();
    drag_operation = DragIsMove(inner_frame->Selection(), drag_data)
                         ? DragOperation::kMove
                         : DragOperation::kCopy;
    if (file_input_element_under_mouse_) {
      bool can_receive_dropped_files = false;
      if (!file_input_element_under_mouse_->IsDisabledFormControl()) {
        can_receive_dropped_files = file_input_element_under_mouse_->Multiple()
                                        ? drag_data->NumberOfFiles() > 0
                                        : drag_data->NumberOfFiles() == 1;
      }
      if (!can_receive_dropped_files)
        drag_operation = DragOperation::kNone;
      file_input_element_under_mouse_->SetCanReceiveDroppedFiles(
          can_receive_dropped_files);
    }

    return true;
  }

  // We are not over an editable region. Make sure we're clearing any prior drag
  // cursor.
  page_->GetDragCaret().Clear();
  if (file_input_element_under_mouse_)
    file_input_element_under_mouse_->SetCanReceiveDroppedFiles(false);
  file_input_element_under_mouse_ = nullptr;
  return false;
}

DragOperation DragController::OperationForLoad(DragData* drag_data,
                                               LocalFrame& local_root) {
  DCHECK(drag_data);
  Document* doc = local_root.DocumentAtPoint(
      PhysicalOffset::FromPointFRound(drag_data->ClientPosition()));

  if (doc &&
      (did_initiate_drag_ || IsA<PluginDocument>(doc) || IsEditable(*doc)))
    return DragOperation::kNone;
  return GetDragOperation(drag_data);
}

// Returns true if node at |point| is editable with populating |dragCaret| and
// |range|, otherwise returns false.
static bool SetSelectionToDragCaret(LocalFrame* frame,
                                    const SelectionInDOMTree& drag_caret,
                                    Range*& range,
                                    const PhysicalOffset& point) {
  frame->Selection().SetSelection(drag_caret, SetSelectionOptions());
  // TODO(crbug.com/40458806): Audit the usage of `UpdateStyleAndLayout`.
  frame->GetDocument()->UpdateStyleAndLayout(DocumentUpdateReason::kEditing);
  if (!frame->Selection().ComputeVisibleSelectionInDOMTree().IsNone()) {
    return frame->Selection()
        .ComputeVisibleSelectionInDOMTree()
        .IsContentEditable();
  }

  const PositionWithAffinity& position = frame->PositionForPoint(point);
  if (!position.IsConnected())
    return false;

  frame->Selection().SetSelection(
      SelectionInDOMTree::Builder().Collapse(position).Build(),
      SetSelectionOptions());
  // TODO(crbug.com/40458806): Audit the usage of `UpdateStyleAndLayout`.
  frame->GetDocument()->UpdateStyleAndLayout(DocumentUpdateReason::kEditing);
  const VisibleSelection& visible_selection =
      frame->Selection().ComputeVisibleSelectionInDOMTree();
  range = CreateRange(visible_selection.ToNormalizedEphemeralRange());
  return !visible_selection.IsNone() && visible_selection.IsContentEditable();
}

DispatchEventResult DragController::DispatchTextInputEventFor(
    LocalFrame* inner_frame,
    DragData* drag_data) {
  // Layout should be clean due to a hit test performed in |elementUnderMouse|.
  DCHECK(!inner_frame->GetDocument()->NeedsLayoutTreeUpdate());
  DCHECK(page_->GetDragCaret().HasCaret());
  String text = page_->GetDragCaret().IsContentRichlyEditable()
                    ? ""
                    : drag_data->AsPlainText();
  const PositionWithAffinity& caret_position =
      page_->GetDragCaret().CaretPosition();
  DCHECK(caret_position.IsConnected()) << caret_position;
  Element* target = FindEventTargetFrom(
      *inner_frame,
      CreateVisibleSelection(
          SelectionInDOMTree::Builder().Collapse(caret_position).Build()));
  if (!target)
    return DispatchEventResult::kNotCanceled;
  return target->DispatchEvent(
      *TextEvent::CreateForDrop(inner_frame->DomWindow(), text));
}

bool DragController::ConcludeEditDrag(DragData* drag_data) {
  DCHECK(drag_data);

  HTMLInputElement* file_input = file_input_element_under_mouse_;
  if (file_input_element_under_mouse_) {
    file_input_element_under_mouse_->SetCanReceiveDroppedFiles(false);
    file_input_element_under_mouse_ = nullptr;
  }

  if (!document_under_mouse_)
    return false;

  PhysicalOffset point = document_under_mouse_->View()->ConvertFromRootFrame(
      PhysicalOffset::FromPointFRound(drag_data->ClientPosition()));
  Element* element = ElementUnderMouse(document_under_mouse_.Get(), point);
  if (!element)
    return false;
  LocalFrame* inner_frame = element->ownerDocument()->GetFrame();
  DCHECK(inner_frame);

  if (page_->GetDragCaret().HasCaret() &&
      DispatchTextInputEventFor(inner_frame, drag_data) !=
          DispatchEventResult::kNotCanceled)
    return true;

  if (drag_data->ContainsFiles() && file_input) {
    // fileInput should be the element we hit tested for, unless it was made
    // display:none in a drop event handler.
    if (file_input->GetLayoutObject())
      DCHECK_EQ(file_input, element);
    if (file_input->IsDisabledFormControl())
      return false;

    return file_input->ReceiveDroppedFiles(drag_data);
  }

  if (!page_->GetDragController().CanProcessDrag(
          drag_data, inner_frame->LocalFrameRoot())) {
    page_->GetDragCaret().Clear();
    return false;
  }

  if (page_->GetDragCaret().HasCaret()) {
    // TODO(crbug.com/40458806): Audit the usage of` UpdateStyleAndLayout`.
    page_->GetDragCaret()
        .CaretPosition()
        .GetPosition()
        .GetDocument()
        ->UpdateStyleAndLayout(DocumentUpdateReason::kEditing);
  }

  const PositionWithAffinity& caret_position =
      page_->GetDragCaret().CaretPosition();
  if (!caret_position.IsConnected()) {
    // "editing/pasteboard/drop-text-events-sideeffect-crash.html" and
    // "editing/pasteboard/drop-text-events-sideeffect.html" reach here.
    page_->GetDragCaret().Clear();
    return false;
  }
  VisibleSelection drag_caret = CreateVisibleSelection(
      SelectionInDOMTree::Builder().Collapse(caret_position).Build());
  page_->GetDragCaret().Clear();
  // |innerFrame| can be removed by event handler called by
  // |dispatchTextInputEventFor()|.
  if (!inner_frame->Selection().IsAvailable()) {
    // "editing/pasteboard/drop-text-events-sideeffect-crash.html" reaches
    // here.
    return false;
  }
  Range* range = CreateRange(drag_caret.ToNormalizedEphemeralRange());
  Element* root_editable_element =
      inner_frame->Selection()
          .ComputeVisibleSelectionInDOMTreeDeprecated()
          .RootEditableElement();

  // For range to be null a WebKit client must have done something bad while
  // manually controlling drag behaviour
  if (!range)
    return false;
  ResourceFetcher* fetcher = range->OwnerDocument().Fetcher();
  ResourceCacheValidationSuppressor validation_suppressor(fetcher);

  // Start new Drag&Drop command group, invalidate previous command group.
  // Assume no other places is firing |DeleteByDrag| and |InsertFromDrop|.
  inner_frame->GetEditor().RegisterCommandGroup(
      MakeGarbageCollected<DragAndDropCommand>(*inner_frame->GetDocument()));

  bool drag_is_move = DragIsMove(inner_frame->Selection(), drag_data);
  bool is_richly_editable_position =
      IsRichlyEditablePosition(drag_caret.Anchor());

  if (drag_is_move || is_richly_editable_position) {
    DragSourceType drag_source_type = DragSourceType::kHTMLSource;
    if (!RuntimeEnabledFeatures::
            DropUrlAsPlainTextInPlainTextOnlyEditablePositionEnabled()) {
      is_richly_editable_position = true;
    }
    DocumentFragment* fragment = DocumentFragmentFromDragData(
        drag_data, inner_frame, range, true, drag_source_type,
        is_richly_editable_position);
    if (!fragment)
      return false;

    if (drag_is_move) {
      // NSTextView behavior is to always smart delete on moving a selection,
      // but only to smart insert if the selection granularity is word
      // granularity.
      const DeleteMode delete_mode =
          inner_frame->GetEditor().SmartInsertDeleteEnabled()
              ? DeleteMode::kSmart
              : DeleteMode::kSimple;
      const InsertMode insert_mode =
          (delete_mode == DeleteMode::kSmart &&
           inner_frame->Selection().Granularity() == TextGranularity::kWord &&
           drag_data->CanSmartReplace())
              ? InsertMode::kSmart
              : InsertMode::kSimple;

      if (!inner_frame->GetEditor().DeleteSelectionAfterDraggingWithEvents(
              FindEventTargetFrom(
                  *inner_frame,
                  inner_frame->Selection()
                      .ComputeVisibleSelectionInDOMTreeDeprecated()),
              delete_mode, drag_caret.Anchor())) {
        return false;
      }

      inner_frame->Selection().SetSelection(
          SelectionInDOMTree::Builder()
              .SetBaseAndExtent(EphemeralRange(range))
              .Build(),
          SetSelectionOptions());
      if (inner_frame->Selection().IsAvailable()) {
        DCHECK(document_under_mouse_);
        if (!inner_frame->GetEditor().ReplaceSelectionAfterDraggingWithEvents(
                element, drag_data, fragment, range, insert_mode,
                drag_source_type))
          return false;
      }
    } else {
      if (SetSelectionToDragCaret(inner_frame, drag_caret.AsSelection(), range,
                                  point)) {
        DCHECK(document_under_mouse_);
        if (!inner_frame->GetEditor().ReplaceSelectionAfterDraggingWithEvents(
                element, drag_data, fragment, range,
                drag_data->CanSmartReplace() ? InsertMode::kSmart
                                             : InsertMode::kSimple,
                drag_source_type))
          return false;
      }
    }
  } else {
    String text = drag_data->AsPlainText();
    if (text.empty())
      return false;

    if (SetSelectionToDragCaret(inner_frame, drag_caret.AsSelection(), range,
                                point)) {
      DCHECK(document_under_mouse_);
      if (!inner_frame->GetEditor().ReplaceSelectionAfterDraggingWithEvents(
              element, drag_data,
              CreateFragmentFromText(EphemeralRange(range), text), range,
              InsertMode::kSimple, DragSourceType::kPlainTextSource))
        return false;
    }
  }

  if (root_editable_element) {
    if (LocalFrame* frame = root_editable_element->GetDocument().GetFrame()) {
      frame->GetEventHandler().UpdateDragStateAfterEditDragIfNeeded(
          root_editable_element);
    }
  }

  return true;
}

bool DragController::CanProcessDrag(DragData* drag_data,
                                    LocalFrame& local_root) {
  DCHECK(drag_data);

  if (!drag_data->ContainsCompatibleContent())
    return false;

  if (!local_root.ContentLayoutObject())
    return false;

  const PhysicalOffset point_in_local_root =
      local_root.View()->ConvertFromRootFrame(
          PhysicalOffset::FromPointFRound(drag_data->ClientPosition()));

  const HitTestResult result =
      local_root.GetEventHandler().HitTestResultAtLocation(
          HitTestLocation(point_in_local_root));

  if (!result.InnerNode())
    return false;

  if (drag_data->ContainsFiles() && AsFileInput(result.InnerNode()))
    return true;

  if (auto* plugin = DynamicTo<HTMLPlugInElement>(result.InnerNode())) {
    if (!plugin->CanProcessDrag() && !IsEditable(*result.InnerNode()))
      return false;
  } else if (!IsEditable(*result.InnerNode())) {
    return false;
  }

  if (did_initiate_drag_ &&
      document_under_mouse_ ==
          (drag_initiator_ ? drag_initiator_->document() : nullptr)) {
    const PhysicalOffset point_in_frame =
        result.InnerNode()
            ->GetDocument()
            .GetFrame()
            ->View()
            ->ConvertFromRootFrame(
                PhysicalOffset::FromPointFRound(drag_data->ClientPosition()));
    return !result.IsSelected(HitTestLocation(point_in_frame));
  }

  return true;
}

static DragOperation DefaultOperationForDrag(DragOperationsMask src_op_mask) {
  // This is designed to match IE's operation fallback for the case where
  // the page calls preventDefault() in a drag event but doesn't set dropEffect.
  if (src_op_mask == kDragOperationEvery)
    return DragOperation::kCopy;
  if (src_op_mask == kDragOperationNone)
    return DragOperation::kNone;
  if (src_op_mask & kDragOperationMove)
    return DragOperation::kMove;
  if (src_op_mask & kDragOperationCopy)
    return DragOperation::kCopy;
  if (src_op_mask & kDragOperationLink)
    return DragOperation::kLink;

  return DragOperation::kNone;
}

bool DragController::TryDHTMLDrag(DragData* drag_data,
                                  DragOperation& operation,
                                  LocalFrame& local_root) {
  DCHECK(drag_data);
  DCHECK(document_under_mouse_);
  if (!local_root.View())
    return false;

  DataTransferAccessPolicy policy = DataTransferAccessPolicy::kTypesReadable;
  DataTransfer* data_transfer = CreateDraggingDataTransfer(policy, drag_data);
  DragOperationsMask src_op_mask = drag_data->DraggingSourceOperationMask();
  data_transfer->SetSourceOperation(src_op_mask);

  WebMouseEvent event = CreateMouseEvent(drag_data);
  if (local_root.GetEventHandler().UpdateDragAndDrop(event, data_transfer) ==
      WebInputEventResult::kNotHandled) {
    data_transfer->SetAccessPolicy(
        DataTransferAccessPolicy::kNumb);  // invalidate clipboard here for
                                           // security
    return false;
  }

  if (!data_transfer->DropEffectIsInitialized()) {
    operation = DefaultOperationForDrag(src_op_mask);
  } else {
    operation = data_transfer->DestinationOperation();
    if (!(src_op_mask & static_cast<int>(operation))) {
      // The element picked an operation which is not supported by the source.
      operation = DragOperation::kNone;
    }
  }

  data_transfer->SetAccessPolicy(
      DataTransferAccessPolicy::kNumb);  // invalidate clipboard here for
                                         // security
  return true;
}

bool SelectTextInsteadOfDrag(const Node& node) {
  if (!node.IsTextNode())
    return false;

  // Editable elements loose their draggability,
  // see https://github.com/whatwg/html/issues/3114.
  if (IsEditable(node))
    return true;

  for (Node& ancestor_node : NodeTraversal::InclusiveAncestorsOf(node)) {
    auto* html_element = DynamicTo<HTMLElement>(ancestor_node);
    if (html_element && html_element->draggable())
      return false;
  }

  return node.CanStartSelection();
}

Node* DragController::DraggableNode(const LocalFrame* src,
                                    Node* start_node,
                                    const gfx::Point& drag_origin,
                                    SelectionDragPolicy selection_drag_policy,
                                    DragSourceAction& drag_type) const {
  if (src->Selection().Contains(PhysicalOffset(drag_origin))) {
    drag_type = kDragSourceActionSelection;
    if (selection_drag_policy == kImmediateSelectionDragResolution)
      return start_node;
  } else {
    drag_type = kDragSourceActionNone;
  }

  Node* node = nullptr;
  DragSourceAction candidate_drag_type = kDragSourceActionNone;
  for (const LayoutObject* layout_object = start_node->GetLayoutObject();
       layout_object; layout_object = layout_object->Parent()) {
    node = layout_object->NonPseudoNode();
    if (!node) {
      // Anonymous layout blocks don't correspond to actual DOM nodes, so we
      // skip over them for the purposes of finding a draggable node.
      continue;
    }
    if (drag_type != kDragSourceActionSelection &&
        SelectTextInsteadOfDrag(*node)) {
      // We have a click in an unselected, selectable text that is not
      // draggable... so we want to start the selection process instead
      // of looking for a parent to try to drag.
      return nullptr;
    }
    if (node->IsElementNode()) {
      EUserDrag drag_mode = layout_object->Style()->UserDrag();
      if (drag_mode == EUserDrag::kNone)
        continue;
      // Even if the image is part of a selection, we always only drag the image
      // in this case.
      if (layout_object->IsImage() && src->GetSettings() &&
          src->GetSettings()->GetLoadsImagesAutomatically()) {
        drag_type = kDragSourceActionImage;
        return node;
      }
      // Other draggable elements are considered unselectable.
      if (drag_mode == EUserDrag::kElement) {
        candidate_drag_type = kDragSourceActionDHTML;
        break;
      }
      // TODO(crbug.com/369219144): Should this be
      // DynamicTo<HTMLAnchorElementBase>?
      auto* html_anchor_element = DynamicTo<HTMLAnchorElement>(node);
      if (html_anchor_element && html_anchor_element->IsLiveLink()) {
        candidate_drag_type = kDragSourceActionLink;
        break;
      }
    }
  }

  if (candidate_drag_type == kDragSourceActionNone) {
    // Either:
    // 1) Nothing under the cursor is considered draggable, so we bail out.
    // 2) There was a selection under the cursor but selectionDragPolicy is set
    //    to DelayedSelectionDragResolution and no other draggable element could
    //    be found, so bail out and allow text selection to start at the cursor
    //    instead.
    return nullptr;
  }

  DCHECK(node);
  if (drag_type == kDragSourceActionSelection) {
    // Dragging unselectable elements in a selection has special behavior if
    // selectionDragPolicy is DelayedSelectionDragResolution and this drag was
    // flagged as a potential selection drag. In that case, don't allow
    // selection and just drag the entire selection instead.
    DCHECK_EQ(selection_drag_policy, kDelayedSelectionDragResolution);
    node = start_node;
  } else {
    // If the cursor isn't over a selection, then just drag the node we found
    // earlier.
    DCHECK_EQ(drag_type, kDragSourceActionNone);
    drag_type = candidate_drag_type;
  }
  return node;
}

static void PrepareDataTransferForImageDrag(LocalFrame* source,
                                            DataTransfer* data_transfer,
                                            Element* node,
                                            const KURL& link_url,
                                            const KURL& image_url,
                                            const String& label) {
  node->GetDocument().UpdateStyleAndLayoutTree();
  if (IsRichlyEditable(*node)) {
    // TODO(crbug.com/331666850): Replace `EphemeralRange` usage with `Range`.
    Range* range = source->GetDocument()->createRange();
    range->selectNode(node, ASSERT_NO_EXCEPTION);
    source->Selection().SetSelection(
        SelectionInDOMTree::Builder()
            .SetBaseAndExtent(EphemeralRange(range))
            .Build(),
        SetSelectionOptions());
  }
  data_transfer->DeclareAndWriteDragImage(node, link_url, image_url, label);
}

bool DragController::PopulateDragDataTransfer(LocalFrame* src,
                                              const DragState& state,
                                              const gfx::Point& drag_origin) {
#if DCHECK_IS_ON()
  DCHECK(DragTypeIsValid(state.drag_type_));
#endif
  DCHECK(src);
  if (!src->View() || !src->ContentLayoutObject())
    return false;

  HitTestLocation location(drag_origin);
  HitTestResult hit_test_result =
      src->GetEventHandler().HitTestResultAtLocation(location);
  // FIXME: Can this even happen? I guess it's possible, but should verify
  // with a web test.
  Node* hit_inner_node = hit_test_result.InnerNode();
  if (!hit_inner_node ||
      !state.drag_src_->IsShadowIncludingInclusiveAncestorOf(*hit_inner_node)) {
    // The original node being dragged isn't under the drag origin anymore...
    // maybe it was hidden or moved out from under the cursor. Regardless, we
    // don't want to start a drag on something that's not actually under the
    // drag origin.
    return false;
  }
  const KURL& link_url = hit_test_result.AbsoluteLinkURL();
  const KURL& image_url = hit_test_result.AbsoluteImageURL();

  DataTransfer* data_transfer = state.drag_data_transfer_.Get();
  Node* node = state.drag_src_.Get();

  // TODO(crbug.com/369219144): Should this be DynamicTo<HTMLAnchorElementBase>?
  auto* html_anchor_element = DynamicTo<HTMLAnchorElement>(node);
  if (html_anchor_element && html_anchor_element->IsLiveLink() &&
      !link_url.IsEmpty()) {
    // Simplify whitespace so the title put on the clipboard resembles what
    // the user sees on the web page. This includes replacing newlines with
    // spaces.
    data_transfer->WriteURL(node, link_url,
                            hit_test_result.TextContent().SimplifyWhiteSpace());
  }

  if (state.drag_type_ == kDragSourceActionSelection) {
    data_transfer->WriteSelection(src->Selection());
  } else if (state.drag_type_ == kDragSourceActionImage) {
    auto* element = DynamicTo<Element>(node);
    if (image_url.IsEmpty() || !element)
      return false;
    PrepareDataTransferForImageDrag(src, data_transfer, element, link_url,
                                    image_url,
                                    hit_test_result.AltDisplayString());
  } else if (state.drag_type_ == kDragSourceActionLink) {
    if (link_url.IsEmpty())
      return false;
  } else if (state.drag_type_ == kDragSourceActionDHTML) {
    LayoutObject* layout_object = node->GetLayoutObject();
    if (!layout_object) {
      // The layoutObject has disappeared, this can happen if the onStartDrag
      // handler has hidden the element in some way. In this case we just kill
      // the drag.
      return false;
    }

    gfx::Rect bounding_including_descendants =
        layout_object->AbsoluteBoundingBoxRectIncludingDescendants();
    gfx::Point drag_element_location =
        drag_origin - bounding_including_descendants.OffsetFromOrigin();
    data_transfer->SetDragImageElement(node, drag_element_location);

    // FIXME: For DHTML/draggable element drags, write element markup to
    // clipboard.
  }

  // Observe context related to source to allow dropping drag_state_ when the
  // Document goes away.
  SetExecutionContext(src->DomWindow());

  return true;
}

namespace {

gfx::Point DragLocationForDHTMLDrag(const gfx::Point& mouse_dragged_point,
                                    const gfx::Point& drag_initiation_location,
                                    const gfx::Point& drag_image_offset,
                                    bool is_link_image) {
  if (is_link_image) {
    return gfx::Point(mouse_dragged_point.x() - drag_image_offset.x(),
                      mouse_dragged_point.y() - drag_image_offset.y());
  }

  return gfx::Point(drag_initiation_location.x() - drag_image_offset.x(),
                    drag_initiation_location.y() - drag_image_offset.y());
}

gfx::Rect DragRectForSelectionDrag(const LocalFrame& frame) {
  frame.View()->UpdateLifecycleToLayoutClean(DocumentUpdateReason::kSelection);
  gfx::Rect dragging_rect =
      gfx::ToEnclosingRect(DragController::ClippedSelection(frame));
  int x1 = dragging_rect.x();
  int y1 = dragging_rect.y();
  int x2 = dragging_rect.right();
  int y2 = dragging_rect.bottom();
  gfx::Point origin(std::min(x1, x2), std::min(y1, y2));
  gfx::Size size(std::abs(x2 - x1), std::abs(y2 - y1));
  return gfx::Rect(origin, size);
}

const gfx::Size MaxDragImageSize(float device_scale_factor) {
#if BUILDFLAG(IS_MAC)
  // Match Safari's drag image size.
  static const gfx::Size kMaxDragImageSize(400, 400);
#else
  static const gfx::Size kMaxDragImageSize(200, 200);
#endif
  return gfx::ScaleToFlooredSize(kMaxDragImageSize, device_scale_factor);
}

bool CanDragImage(const Element& element) {
  auto* layout_image = DynamicTo<LayoutImage>(element.GetLayoutObject());
  if (!layout_image)
    return false;
  const ImageResourceContent* image_content = layout_image->CachedImage();
  if (!image_content || image_content->ErrorOccurred() ||
      image_content->GetImage()->IsNull())
    return false;
  scoped_refptr<const SharedBuffer> buffer = image_content->ResourceBuffer();
  if (!buffer || !buffer->size())
    return false;
  // We shouldn't be starting a drag for an image that can't provide an
  // extension.
  // This is an early detection for problems encountered later upon drop.
  DCHECK(!image_content->GetImage()->FilenameExtension().empty());
  return true;
}

std::unique_ptr<DragImage> DragImageForImage(
    const Element& element,
    float device_scale_factor,
    const gfx::Size& image_element_size_in_pixels) {
  auto* layout_image = To<LayoutImage>(element.GetLayoutObject());
  const LayoutImageResource& image_resource = *layout_image->ImageResource();
  scoped_refptr<Image> image =
      image_resource.GetImage(image_element_size_in_pixels);
  RespectImageOrientationEnum respect_orientation =
      image_resource.ImageOrientation();

  gfx::Size image_size = image->Size(respect_orientation);
  if (image_size.Area64() > kMaxOriginalImageArea)
    return nullptr;

  InterpolationQuality interpolation_quality = kInterpolationDefault;
  if (layout_image->StyleRef().ImageRendering() == EImageRendering::kPixelated)
    interpolation_quality = kInterpolationNone;

  gfx::Vector2dF image_scale =
      DragImage::ClampedImageScale(image_size, image_element_size_in_pixels,
                                   MaxDragImageSize(device_scale_factor));

  return DragImage::Create(image.get(), respect_orientation,
                           interpolation_quality, kDragImageAlpha, image_scale);
}

gfx::Rect DragRectForImage(const DragImage* drag_image,
                           const gfx::Point& drag_initiation_location,
                           const gfx::Point& image_element_location,
                           const gfx::Size& image_element_size_in_pixels) {
  if (!drag_image)
    return gfx::Rect(drag_initiation_location, gfx::Size());

  gfx::Size original_size = image_element_size_in_pixels;
  gfx::Size new_size = drag_image->Size();

  // Properly orient the drag image and orient it differently if it's smaller
  // than the original
  float scale = new_size.width() / static_cast<float>(original_size.width());
  gfx::Vector2dF offset = image_element_location - drag_initiation_location;
  gfx::Point origin = drag_initiation_location +
                      gfx::ToRoundedVector2d(gfx::ScaleVector2d(offset, scale));
  return gfx::Rect(origin, new_size);
}

std::unique_ptr<DragImage> DragImageForLink(const KURL& link_url,
                                            const String& link_text,
                                            float device_scale_factor) {
  return DragImage::Create(link_url, link_text, device_scale_factor);
}

gfx::Rect DragRectForLink(const DragImage* link_image,
                          const gfx::Point& origin,
                          float device_scale_factor,
                          float page_scale_factor) {
  if (!link_image)
    return gfx::Rect(origin, gfx::Size());

  gfx::Size image_size = link_image->Size();
  // Offset the image so that the cursor is horizontally centered.
  gfx::PointF image_offset(-image_size.width() / 2.f, -kLinkDragBorderInset);
  // |origin| is in the coordinate space of the frame's contents whereas the
  // size of |link_image| is in physical pixels. Adjust the image offset to be
  // scaled in the frame's contents.
  // TODO(crbug.com/331670940): Unify this calculation with the
  // `DragImageForImage` scaling code.
  float scale = 1.f / (device_scale_factor * page_scale_factor);
  image_offset.Scale(scale);
  image_offset += origin.OffsetFromOrigin();
  return gfx::Rect(gfx::ToRoundedPoint(image_offset), image_size);
}

}  // namespace

// static
gfx::RectF DragController::ClippedSelection(const LocalFrame& frame) {
  DCHECK(frame.View());
  return DataTransfer::ClipByVisualViewport(
      gfx::RectF(frame.Selection().AbsoluteUnclippedBounds()), frame);
}

// static
std::unique_ptr<DragImage> DragController::DragImageForSelection(
    LocalFrame& frame,
    float opacity) {
  if (!frame.Selection().ComputeVisibleSelectionInDOMTreeDeprecated().IsRange())
    return nullptr;

  frame.View()->UpdateAllLifecyclePhasesExceptPaint(
      DocumentUpdateReason::kDragImage);
  DCHECK(frame.GetDocument()->IsActive());

  gfx::RectF painting_rect = DragController::ClippedSelection(frame);
  PaintFlags paint_flags =
      PaintFlag::kSelectionDragImageOnly | PaintFlag::kOmitCompositingInfo;

  PaintRecordBuilder builder;
  frame.View()->PaintOutsideOfLifecycle(
      builder.Context(), paint_flags,
      CullRect(gfx::ToEnclosingRect(painting_rect)));

  auto property_tree_state = frame.View()
                                 ->GetLayoutView()
                                 ->FirstFragment()
                                 .LocalBorderBoxProperties()
                                 .Unalias();
  return DataTransfer::CreateDragImageForFrame(
      frame, opacity, painting_rect.size(), painting_rect.OffsetFromOrigin(),
      builder, property_tree_state);
}

namespace {

void SelectEnclosingAnchorIfContentEditable(LocalFrame* frame) {
  if (frame->Selection()
          .ComputeVisibleSelectionInDOMTreeDeprecated()
          .IsCaret() &&
      frame->Selection()
          .ComputeVisibleSelectionInDOMTreeDeprecated()
          .IsContentEditable()) {
    // A user can initiate a drag on a link without having any text
    // selected.  In this case, we should expand the selection to
    // the enclosing anchor element.
    if (Node* anchor = EnclosingAnchorElement(
            frame->Selection()
                .ComputeVisibleSelectionInDOMTreeDeprecated()
                .Anchor())) {
      frame->Selection().SetSelection(
          SelectionInDOMTree::Builder().SelectAllChildren(*anchor).Build(),
          SetSelectionOptions());
    }
  }
}

std::unique_ptr<DragImage> DetermineDragImageAndRect(
    gfx::Rect& drag_obj_rect,
    gfx::Point& effective_drag_initiation_location,
    LocalFrame* frame,
    const DragState& state,
    const HitTestResult& hit_test_result,
    const gfx::Point& drag_initiation_location,
    const gfx::Point& mouse_dragged_point) {
  DataTransfer* data_transfer = state.drag_data_transfer_.Get();
  const KURL& link_url = hit_test_result.AbsoluteLinkURL();
  float device_scale_factor =
      frame->GetChromeClient().GetScreenInfo(*frame).device_scale_factor;

  gfx::Point drag_offset;

  // HTML DnD spec allows setting the drag image, even if it is a link, image or
  // text we are dragging.
  std::unique_ptr<DragImage> drag_image =
      data_transfer->CreateDragImage(drag_offset, device_scale_factor, frame);
  if (drag_image) {
    drag_obj_rect.set_origin(
        DragLocationForDHTMLDrag(mouse_dragged_point, drag_initiation_location,
                                 drag_offset, !link_url.IsEmpty()));
    drag_obj_rect.set_size(drag_image.get()->Size());
  } else {
    drag_obj_rect = gfx::Rect();
  }

  effective_drag_initiation_location = drag_initiation_location;

  // If |drag_image| is not provided, try to determine a drag-source-specific
  // image and location.
  if (state.drag_type_ == kDragSourceActionSelection) {
    if (!drag_image) {
      drag_image =
          DragController::DragImageForSelection(*frame, kDragImageAlpha);
      drag_obj_rect = DragRectForSelectionDrag(*frame);
    }
  } else if (state.drag_type_ == kDragSourceActionImage) {
    if (!drag_image) {
      auto* element = DynamicTo<Element>(state.drag_src_.Get());
      const gfx::Rect& image_rect = hit_test_result.ImageRect();
      // TODO(crbug.com/331670941): Remove this scaling and simply pass
      // `imageRect`to `dragImageForImage` once all platforms are migrated
      // to use zoom for dsf.
      gfx::Size image_size_in_pixels = gfx::ScaleToFlooredSize(
          image_rect.size(), frame->GetPage()->GetVisualViewport().Scale());

      // Pass the selected image size in DIP becasue dragImageForImage clips the
      // image in DIP.  The coordinates of the locations are in Viewport
      // coordinates, and they're converted in the Blink client.
      // TODO(crbug.com/331753419): Consider clipping screen coordinates to
      // use a high resolution image on high DPI screens.
      drag_image = DragImageForImage(*element, device_scale_factor,
                                     image_size_in_pixels);
      drag_obj_rect =
          DragRectForImage(drag_image.get(), effective_drag_initiation_location,
                           image_rect.origin(), image_size_in_pixels);
    }
  } else if (state.drag_type_ == kDragSourceActionLink) {
    if (!drag_image) {
      DCHECK(frame->GetPage());
      drag_image = DragImageForLink(link_url, hit_test_result.TextContent(),
                                    device_scale_factor);
      drag_obj_rect = DragRectForLink(drag_image.get(), mouse_dragged_point,
                                      device_scale_factor,
                                      frame->GetPage()->PageScaleFactor());
    }
    // Why is the initiation location different only for link-drags?
    effective_drag_initiation_location = mouse_dragged_point;
  }

  return drag_image;
}

}  // namespace

bool DragController::StartDrag(LocalFrame* frame,
                               const DragState& state,
                               const WebMouseEvent& drag_event,
                               const gfx::Point& drag_initiation_location) {
  DCHECK(frame);
  if (!frame->View() || !frame->ContentLayoutObject())
    return false;

  HitTestLocation location(drag_initiation_location);
  HitTestResult hit_test_result =
      frame->GetEventHandler().HitTestResultAtLocation(location);
  Node* hit_inner_node = hit_test_result.InnerNode();
  if (!hit_inner_node ||
      !state.drag_src_->IsShadowIncludingInclusiveAncestorOf(*hit_inner_node)) {
    // The original node being dragged isn't under the drag origin anymore...
    // maybe it was hidden or moved out from under the cursor. Regardless, we
    // don't want to start a drag on something that's not actually under the
    // drag origin.
    return false;
  }

  // Note that drag_origin is different from event position.
  gfx::Point mouse_dragged_point = frame->View()->ConvertFromRootFrame(
      gfx::ToFlooredPoint(drag_event.PositionInRootFrame()));

  // Check early return conditions.
  if (state.drag_type_ == kDragSourceActionImage) {
    const KURL& image_url = hit_test_result.AbsoluteImageURL();
    auto* element = DynamicTo<Element>(state.drag_src_.Get());
    if (image_url.IsEmpty() || !element || !CanDragImage(*element))
      return false;
  } else if (state.drag_type_ == kDragSourceActionLink) {
    const KURL& link_url = hit_test_result.AbsoluteLinkURL();
    if (link_url.IsEmpty())
      return false;
  } else if (state.drag_type_ != kDragSourceActionSelection &&
             state.drag_type_ != kDragSourceActionDHTML) {
    NOTREACHED_IN_MIGRATION();
    return false;
  }

  if (state.drag_type_ == kDragSourceActionLink)
    SelectEnclosingAnchorIfContentEditable(frame);

  gfx::Rect drag_obj_rect;
  gfx::Point effective_drag_initiation_location;

  std::unique_ptr<DragImage> drag_image = DetermineDragImageAndRect(
      drag_obj_rect, effective_drag_initiation_location, frame, state,
      hit_test_result, drag_initiation_location, mouse_dragged_point);

  DoSystemDrag(drag_image.get(), drag_obj_rect,
               effective_drag_initiation_location,
               state.drag_data_transfer_.Get(), frame);
  return true;
}

void DragController::DoSystemDrag(DragImage* image,
                                  const gfx::Rect& drag_obj_rect,
                                  const gfx::Point& drag_initiation_location,
                                  DataTransfer* data_transfer,
                                  LocalFrame* frame) {
  did_initiate_drag_ = true;
  drag_initiator_ = frame->DomWindow();
  SetExecutionContext(frame->DomWindow());

  // TODO(crbug.com/331753420): `drag_obj_rect` and `drag_initiation_location`
  // should be passed in as `gfx::RectF` and `gfx::PointF` respectively to
  // avoid unnecessary rounding.
  gfx::Point adjusted_drag_obj_location =
      frame->View()->FrameToViewport(drag_obj_rect.origin());
  gfx::Point adjusted_event_pos =
      frame->View()->FrameToViewport(drag_initiation_location);
  gfx::Vector2d cursor_offset = adjusted_event_pos - adjusted_drag_obj_location;
  WebDragData drag_data = data_transfer->GetDataObject()->ToWebDragData();
  drag_data.SetReferrerPolicy(drag_initiator_->GetReferrerPolicy());
  DragOperationsMask drag_operation_mask = data_transfer->SourceOperation();

  SkBitmap drag_image = image ? image->Bitmap() : SkBitmap();
  page_->GetChromeClient().StartDragging(frame, drag_data, drag_operation_mask,
                                         std::move(drag_image), cursor_offset,
                                         drag_obj_rect);
}

DragOperation DragController::GetDragOperation(DragData* drag_data) {
  // FIXME: To match the MacOS behaviour we should return DragOperation::kNone
  // if we are a modal window, we are the drag source, or the window is an
  // attached sheet If this can be determined from within WebCore
  // operationForDrag can be pulled into WebCore itself
  DCHECK(drag_data);
  return drag_data->ContainsURL() && !did_initiate_drag_ ? DragOperation::kCopy
                                                         : DragOperation::kNone;
}

bool DragController::IsCopyKeyDown(DragData* drag_data) {
  int modifiers = drag_data->GetModifiers();

#if BUILDFLAG(IS_MAC)
  return modifiers & WebInputEvent::kAltKey;
#else
  return modifiers & WebInputEvent::kControlKey;
#endif
}

DragState& DragController::GetDragState() {
  if (!drag_state_)
    drag_state_ = MakeGarbageCollected<DragState>();
  return *drag_state_;
}

void DragController::ContextDestroyed() {
  drag_state_ = nullptr;
}

void DragController::Trace(Visitor* visitor) const {
  visitor->Trace(page_);
  visitor->Trace(document_under_mouse_);
  visitor->Trace(drag_initiator_);
  visitor->Trace(drag_state_);
  visitor->Trace(file_input_element_under_mouse_);
  ExecutionContextLifecycleObserver::Trace(visitor);
}

}  // namespace blink
