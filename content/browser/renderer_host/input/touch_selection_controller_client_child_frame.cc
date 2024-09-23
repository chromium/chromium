// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/input/touch_selection_controller_client_child_frame.h"

#include "base/check.h"
#include "base/notreached.h"
#include "content/browser/renderer_host/render_widget_host_delegate.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_child_frame.h"
#include "content/public/browser/touch_selection_controller_client_manager.h"
#include "third_party/blink/public/mojom/input/input_handler.mojom-shared.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/data_transfer_policy/data_transfer_endpoint.h"
#include "ui/base/pointer/touch_editing_controller.h"
#include "ui/base/ui_base_features.h"
#include "ui/base/ui_base_types.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/strings/grit/ui_strings.h"

namespace content {

TouchSelectionControllerClientChildFrame::
    TouchSelectionControllerClientChildFrame(
        RenderWidgetHostViewChildFrame* rwhv,
        TouchSelectionControllerClientManager* manager)
    : rwhv_(rwhv), manager_(manager) {
  DCHECK(rwhv);
  DCHECK(manager_);
}

TouchSelectionControllerClientChildFrame::
    ~TouchSelectionControllerClientChildFrame() {
  // If the manager doesn't outlive us, our owning view sill detach us.
  manager_->InvalidateClient(this);
}

void TouchSelectionControllerClientChildFrame::DidStopFlinging() {
  manager_->DidStopFlinging();
}

void TouchSelectionControllerClientChildFrame::OnSwipeToMoveCursorBegin() {
  manager_->OnSwipeToMoveCursorBegin();
}

void TouchSelectionControllerClientChildFrame::OnSwipeToMoveCursorEnd() {
  manager_->OnSwipeToMoveCursorEnd();
}

void TouchSelectionControllerClientChildFrame::OnHitTestRegionUpdated() {
  manager_->OnClientHitTestRegionUpdated(this);
}

void TouchSelectionControllerClientChildFrame::
    TransformSelectionBoundsAndUpdate() {
  gfx::SelectionBound transformed_selection_start(selection_start_);
  gfx::SelectionBound transformed_selection_end(selection_end_);

  // TODO(wjmaclean): Get the transform between the views to lower the
  // overhead here, instead of calling the transform functions four times.
  transformed_selection_start.SetEdge(
      rwhv_->TransformPointToRootCoordSpaceF(selection_start_.edge_start()),
      rwhv_->TransformPointToRootCoordSpaceF(selection_start_.edge_end()));
  transformed_selection_start.SetVisibleEdge(
      rwhv_->TransformPointToRootCoordSpaceF(
          selection_start_.visible_edge_start()),
      rwhv_->TransformPointToRootCoordSpaceF(
          selection_start_.visible_edge_end()));
  transformed_selection_end.SetEdge(
      rwhv_->TransformPointToRootCoordSpaceF(selection_end_.edge_start()),
      rwhv_->TransformPointToRootCoordSpaceF(selection_end_.edge_end()));
  transformed_selection_end.SetVisibleEdge(
      rwhv_->TransformPointToRootCoordSpaceF(
          selection_end_.visible_edge_start()),
      rwhv_->TransformPointToRootCoordSpaceF(
          selection_end_.visible_edge_end()));

  manager_->UpdateClientSelectionBounds(transformed_selection_start,
                                        transformed_selection_end, this, this);
}

void TouchSelectionControllerClientChildFrame::UpdateSelectionBoundsIfNeeded(
    const viz::Selection<gfx::SelectionBound>& selection,
    float device_scale_factor) {
  if (selection.start != selection_start_ || selection.end != selection_end_) {
    selection_start_ = selection.start;
    selection_end_ = selection.end;

    TransformSelectionBoundsAndUpdate();
  }
}

void TouchSelectionControllerClientChildFrame::ShowTouchSelectionContextMenu(
    const gfx::Point& location) {
  // |location| should be in root-view coordinates, and RenderWidgetHostImpl
  // will do the conversion to renderer coordinates.
  rwhv_->host()->ShowContextMenuAtPoint(location, ui::MENU_SOURCE_TOUCH_HANDLE);
}

// Since an active touch selection in a child frame can have its screen position
// changed by a scroll in a containing frame (and thus without the child frame
// sending a new compositor frame), we must manually recompute the screen
// position if requested to do so and it has changed.
void TouchSelectionControllerClientChildFrame::DidScroll() {
  TransformSelectionBoundsAndUpdate();
}

gfx::Point TouchSelectionControllerClientChildFrame::ConvertFromRoot(
    const gfx::PointF& point_f) const {
  gfx::PointF transformed_point(point_f);
  RenderWidgetHostViewBase* root_view = rwhv_->GetRootRenderWidgetHostView();
  if (root_view) {
    root_view->TransformPointToCoordSpaceForView(point_f, rwhv_,
                                                 &transformed_point);
  }

  return gfx::ToRoundedPoint(transformed_point);
}

bool TouchSelectionControllerClientChildFrame::SupportsAnimation() const {
  NOTREACHED_IN_MIGRATION();
  return false;
}

void TouchSelectionControllerClientChildFrame::SetNeedsAnimate() {
  NOTREACHED_IN_MIGRATION();
}

void TouchSelectionControllerClientChildFrame::MoveCaret(
    const gfx::PointF& position) {
  RenderWidgetHostDelegate* host_delegate = rwhv_->host()->delegate();
  if (host_delegate)
    host_delegate->MoveCaret(ConvertFromRoot(position));
}

void TouchSelectionControllerClientChildFrame::MoveRangeSelectionExtent(
    const gfx::PointF& extent) {
  RenderWidgetHostDelegate* host_delegate = rwhv_->host()->delegate();
  if (host_delegate)
    host_delegate->MoveRangeSelectionExtent(ConvertFromRoot(extent));
}

void TouchSelectionControllerClientChildFrame::SelectBetweenCoordinates(
    const gfx::PointF& base,
    const gfx::PointF& extent) {
  RenderWidgetHostDelegate* host_delegate = rwhv_->host()->delegate();
  if (host_delegate) {
    host_delegate->SelectRange(ConvertFromRoot(base), ConvertFromRoot(extent));
  }
}

void TouchSelectionControllerClientChildFrame::OnSelectionEvent(
    ui::SelectionEventType event) {
  NOTREACHED_IN_MIGRATION();
}

void TouchSelectionControllerClientChildFrame::OnDragUpdate(
    const ui::TouchSelectionDraggable::Type type,
    const gfx::PointF& position) {
  NOTREACHED_IN_MIGRATION();
}

std::unique_ptr<ui::TouchHandleDrawable>
TouchSelectionControllerClientChildFrame::CreateDrawable() {
  NOTREACHED_IN_MIGRATION();
  return nullptr;
}

bool TouchSelectionControllerClientChildFrame::IsCommandIdEnabled(
    int command_id) const {
  bool editable = rwhv_->GetTextInputType() != ui::TEXT_INPUT_TYPE_NONE;
  bool readable = rwhv_->GetTextInputType() != ui::TEXT_INPUT_TYPE_PASSWORD;
  bool has_selection = !rwhv_->GetSelectedText().empty();
  switch (command_id) {
    case ui::TouchEditable::kCut:
      return editable && readable && has_selection;
    case ui::TouchEditable::kCopy:
      return readable && has_selection;
    case ui::TouchEditable::kPaste: {
      std::u16string result;
      ui::DataTransferEndpoint data_dst = ui::DataTransferEndpoint(
          ui::EndpointType::kDefault, {.notify_if_restricted = false});
      ui::Clipboard::GetForCurrentThread()->ReadText(
          ui::ClipboardBuffer::kCopyPaste, &data_dst, &result);
      return editable && !result.empty();
    }
    case ui::TouchEditable::kSelectAll: {
      gfx::Range text_range;
      if (rwhv_->GetTextRange(&text_range)) {
        return text_range.length() > rwhv_->GetSelectedText().length();
      }
      return true;
    }
    case ui::TouchEditable::kSelectWord: {
      gfx::Range text_range;
      if (rwhv_->GetTextRange(&text_range)) {
        return readable && !has_selection && !text_range.is_empty();
      }
      return readable && !has_selection;
    }
    default:
      return false;
  }
}

void TouchSelectionControllerClientChildFrame::ExecuteCommand(int command_id,
                                                              int event_flags) {
  const bool should_dismiss_handles =
      command_id != ui::TouchEditable::kSelectAll &&
      command_id != ui::TouchEditable::kSelectWord;
  manager_->GetTouchSelectionController()->OnMenuCommand(
      should_dismiss_handles);

  RenderWidgetHostDelegate* host_delegate = rwhv_->host()->delegate();
  if (!host_delegate)
    return;

  switch (command_id) {
    case ui::TouchEditable::kCut:
      host_delegate->Cut();
      break;
    case ui::TouchEditable::kCopy:
      host_delegate->Copy();
      break;
    case ui::TouchEditable::kPaste:
      host_delegate->Paste();
      break;
    case ui::TouchEditable::kSelectAll:
      host_delegate->SelectAll();
      break;
    case ui::TouchEditable::kSelectWord:
      host_delegate->SelectAroundCaret(
          blink::mojom::SelectionGranularity::kWord,
          /*should_show_handle=*/true,
          /*should_show_context_menu=*/false);
      break;
    default:
      NOTREACHED_IN_MIGRATION();
      break;
  }
}

void TouchSelectionControllerClientChildFrame::RunContextMenu() {
  gfx::RectF anchor_rect =
      manager_->GetTouchSelectionController()->GetVisibleRectBetweenBounds();
  gfx::PointF anchor_point =
      gfx::PointF(anchor_rect.CenterPoint().x(), anchor_rect.y());
  gfx::PointF origin = rwhv_->TransformPointToRootCoordSpaceF(gfx::PointF());
  anchor_point.Offset(-origin.x(), -origin.y());
  RenderWidgetHostImpl* host = rwhv_->host();
  host->GetRenderInputRouter()->ShowContextMenuAtPoint(
      gfx::ToRoundedPoint(anchor_point), ui::MENU_SOURCE_TOUCH_EDIT_MENU);

  // Hide selection handles after getting rect-between-bounds from touch
  // selection controller; otherwise, rect would be empty and the above
  // calculations would be invalid.
  manager_->GetTouchSelectionController()
      ->HideAndDisallowShowingAutomatically();
}

bool TouchSelectionControllerClientChildFrame::ShouldShowQuickMenu() {
  return true;
}

std::u16string TouchSelectionControllerClientChildFrame::GetSelectedText() {
  return rwhv_->GetSelectedText();
}

}  // namespace content
