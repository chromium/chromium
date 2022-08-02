/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Simon Hausmann (hausmann@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2006, 2009, 2010 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "third_party/blink/renderer/core/html/html_frame_set_element.h"

#include "third_party/blink/renderer/bindings/core/v8/js_event_handler_for_content_attribute.h"
#include "third_party/blink/renderer/core/css/css_property_names.h"
#include "third_party/blink/renderer/core/css/style_change_reason.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/events/mouse_event.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/html/frame_edge_info.h"
#include "third_party/blink/renderer/core/html/html_collection.h"
#include "third_party/blink/renderer/core/html/html_frame_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/input/event_handler.h"
#include "third_party/blink/renderer/core/layout/layout_frame_set.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"

namespace blink {

HTMLFrameSetElement::HTMLFrameSetElement(Document& document)
    : HTMLElement(html_names::kFramesetTag, document),
      border_(6),
      border_set_(false),
      border_color_set_(false),
      frameborder_(true),
      frameborder_set_(false),
      noresize_(false) {
  SetHasCustomStyleCallbacks();
  UseCounter::Count(document, WebFeature::kHTMLFrameSetElement);
}

bool HTMLFrameSetElement::IsPresentationAttribute(
    const QualifiedName& name) const {
  if (name == html_names::kBordercolorAttr)
    return true;
  return HTMLElement::IsPresentationAttribute(name);
}

void HTMLFrameSetElement::CollectStyleForPresentationAttribute(
    const QualifiedName& name,
    const AtomicString& value,
    MutableCSSPropertyValueSet* style) {
  if (name == html_names::kBordercolorAttr)
    AddHTMLColorToStyle(style, CSSPropertyID::kBorderColor, value);
  else
    HTMLElement::CollectStyleForPresentationAttribute(name, value, style);
}

void HTMLFrameSetElement::ParseAttribute(
    const AttributeModificationParams& params) {
  const QualifiedName& name = params.name;
  const AtomicString& value = params.new_value;
  if (name == html_names::kRowsAttr) {
    if (!value.IsNull()) {
      row_lengths_ = ParseListOfDimensions(value.GetString());
      SetNeedsStyleRecalc(kSubtreeStyleChange,
                          StyleChangeReasonForTracing::FromAttribute(name));
      if (GetLayoutObject() && TotalRows() != resize_rows_.deltas_.size()) {
        resize_rows_.Resize(TotalRows());
        resize_cols_.Resize(TotalCols());
      }
    }
  } else if (name == html_names::kColsAttr) {
    if (!value.IsNull()) {
      col_lengths_ = ParseListOfDimensions(value.GetString());
      SetNeedsStyleRecalc(kSubtreeStyleChange,
                          StyleChangeReasonForTracing::FromAttribute(name));
      if (GetLayoutObject() && TotalCols() != resize_cols_.deltas_.size()) {
        resize_rows_.Resize(TotalRows());
        resize_cols_.Resize(TotalCols());
      }
    }
  } else if (name == html_names::kFrameborderAttr) {
    if (!value.IsNull()) {
      if (EqualIgnoringASCIICase(value, "no") ||
          EqualIgnoringASCIICase(value, "0")) {
        frameborder_ = false;
        frameborder_set_ = true;
      } else if (EqualIgnoringASCIICase(value, "yes") ||
                 EqualIgnoringASCIICase(value, "1")) {
        frameborder_set_ = true;
      }
    } else {
      frameborder_ = false;
      frameborder_set_ = false;
    }
  } else if (name == html_names::kNoresizeAttr) {
    noresize_ = true;
  } else if (name == html_names::kBorderAttr) {
    if (!value.IsNull()) {
      border_ = value.ToInt();
      border_set_ = true;
    } else {
      border_set_ = false;
    }
  } else if (name == html_names::kBordercolorAttr) {
    border_color_set_ = !value.IsEmpty();
  } else if (name == html_names::kOnafterprintAttr) {
    GetDocument().SetWindowAttributeEventListener(
        event_type_names::kAfterprint,
        JSEventHandlerForContentAttribute::Create(GetExecutionContext(), name,
                                                  value));
  } else if (name == html_names::kOnbeforeprintAttr) {
    GetDocument().SetWindowAttributeEventListener(
        event_type_names::kBeforeprint,
        JSEventHandlerForContentAttribute::Create(GetExecutionContext(), name,
                                                  value));
  } else if (name == html_names::kOnloadAttr) {
    GetDocument().SetWindowAttributeEventListener(
        event_type_names::kLoad, JSEventHandlerForContentAttribute::Create(
                                     GetExecutionContext(), name, value));
  } else if (name == html_names::kOnbeforeunloadAttr) {
    GetDocument().SetWindowAttributeEventListener(
        event_type_names::kBeforeunload,
        JSEventHandlerForContentAttribute::Create(
            GetExecutionContext(), name, value,
            JSEventHandler::HandlerType::kOnBeforeUnloadEventHandler));
  } else if (name == html_names::kOnunloadAttr) {
    GetDocument().SetWindowAttributeEventListener(
        event_type_names::kUnload, JSEventHandlerForContentAttribute::Create(
                                       GetExecutionContext(), name, value));
  } else if (name == html_names::kOnpagehideAttr) {
    GetDocument().SetWindowAttributeEventListener(
        event_type_names::kPagehide, JSEventHandlerForContentAttribute::Create(
                                         GetExecutionContext(), name, value));
  } else if (name == html_names::kOnpageshowAttr) {
    GetDocument().SetWindowAttributeEventListener(
        event_type_names::kPageshow, JSEventHandlerForContentAttribute::Create(
                                         GetExecutionContext(), name, value));
  } else if (name == html_names::kOnblurAttr) {
    GetDocument().SetWindowAttributeEventListener(
        event_type_names::kBlur, JSEventHandlerForContentAttribute::Create(
                                     GetExecutionContext(), name, value));
  } else if (name == html_names::kOnerrorAttr) {
    GetDocument().SetWindowAttributeEventListener(
        event_type_names::kError,
        JSEventHandlerForContentAttribute::Create(
            GetExecutionContext(), name, value,
            JSEventHandler::HandlerType::kOnErrorEventHandler));
  } else if (name == html_names::kOnfocusAttr) {
    GetDocument().SetWindowAttributeEventListener(
        event_type_names::kFocus, JSEventHandlerForContentAttribute::Create(
                                      GetExecutionContext(), name, value));
  } else if (name == html_names::kOnfocusinAttr) {
    GetDocument().SetWindowAttributeEventListener(
        event_type_names::kFocusin, JSEventHandlerForContentAttribute::Create(
                                        GetExecutionContext(), name, value));
  } else if (name == html_names::kOnfocusoutAttr) {
    GetDocument().SetWindowAttributeEventListener(
        event_type_names::kFocusout, JSEventHandlerForContentAttribute::Create(
                                         GetExecutionContext(), name, value));
  } else if (RuntimeEnabledFeatures::OrientationEventEnabled() &&
             name == html_names::kOnorientationchangeAttr) {
    GetDocument().SetWindowAttributeEventListener(
        event_type_names::kOrientationchange,
        JSEventHandlerForContentAttribute::Create(GetExecutionContext(), name,
                                                  value));
  } else if (name == html_names::kOnhashchangeAttr) {
    GetDocument().SetWindowAttributeEventListener(
        event_type_names::kHashchange,
        JSEventHandlerForContentAttribute::Create(GetExecutionContext(), name,
                                                  value));
  } else if (name == html_names::kOnmessageAttr) {
    GetDocument().SetWindowAttributeEventListener(
        event_type_names::kMessage, JSEventHandlerForContentAttribute::Create(
                                        GetExecutionContext(), name, value));
  } else if (name == html_names::kOnresizeAttr) {
    GetDocument().SetWindowAttributeEventListener(
        event_type_names::kResize, JSEventHandlerForContentAttribute::Create(
                                       GetExecutionContext(), name, value));
  } else if (name == html_names::kOnscrollAttr) {
    GetDocument().SetWindowAttributeEventListener(
        event_type_names::kScroll, JSEventHandlerForContentAttribute::Create(
                                       GetExecutionContext(), name, value));
  } else if (name == html_names::kOnstorageAttr) {
    GetDocument().SetWindowAttributeEventListener(
        event_type_names::kStorage, JSEventHandlerForContentAttribute::Create(
                                        GetExecutionContext(), name, value));
  } else if (name == html_names::kOnonlineAttr) {
    GetDocument().SetWindowAttributeEventListener(
        event_type_names::kOnline, JSEventHandlerForContentAttribute::Create(
                                       GetExecutionContext(), name, value));
  } else if (name == html_names::kOnofflineAttr) {
    GetDocument().SetWindowAttributeEventListener(
        event_type_names::kOffline, JSEventHandlerForContentAttribute::Create(
                                        GetExecutionContext(), name, value));
  } else if (name == html_names::kOnpopstateAttr) {
    GetDocument().SetWindowAttributeEventListener(
        event_type_names::kPopstate, JSEventHandlerForContentAttribute::Create(
                                         GetExecutionContext(), name, value));
  } else if (name == html_names::kOnlanguagechangeAttr) {
    GetDocument().SetWindowAttributeEventListener(
        event_type_names::kLanguagechange,
        JSEventHandlerForContentAttribute::Create(GetExecutionContext(), name,
                                                  value));
  } else if (RuntimeEnabledFeatures::PortalsEnabled(GetExecutionContext()) &&
             name == html_names::kOnportalactivateAttr) {
    GetDocument().SetWindowAttributeEventListener(
        event_type_names::kPortalactivate,
        JSEventHandlerForContentAttribute::Create(GetExecutionContext(), name,
                                                  value));
  } else if (RuntimeEnabledFeatures::TimeZoneChangeEventEnabled() &&
             name == html_names::kOntimezonechangeAttr) {
    GetDocument().SetWindowAttributeEventListener(
        event_type_names::kTimezonechange,
        JSEventHandlerForContentAttribute::Create(GetExecutionContext(), name,
                                                  value));
  } else {
    HTMLElement::ParseAttribute(params);
  }
}

FrameEdgeInfo HTMLFrameSetElement::EdgeInfo() const {
  FrameEdgeInfo result(NoResize(), true);

  wtf_size_t rows_count = TotalRows();
  wtf_size_t cols_count = TotalCols();
  DCHECK_GT(rows_count, 0u);
  DCHECK_GT(cols_count, 0u);
  const auto* object = To<LayoutFrameSet>(GetLayoutObject());
  DCHECK(object) << "This must be called while LayoutFrameSet is attached.";
  const LayoutFrameSet::GridAxis& cols = object->Columns();
  result.SetPreventResize(kLeftFrameEdge, cols.prevent_resize_[0]);
  result.SetAllowBorder(kLeftFrameEdge, cols.allow_border_[0]);
  result.SetPreventResize(kRightFrameEdge, cols.prevent_resize_[cols_count]);
  result.SetAllowBorder(kRightFrameEdge, cols.allow_border_[cols_count]);
  const LayoutFrameSet::GridAxis& rows = object->Rows();
  result.SetPreventResize(kTopFrameEdge, rows.prevent_resize_[0]);
  result.SetAllowBorder(kTopFrameEdge, rows.allow_border_[0]);
  result.SetPreventResize(kBottomFrameEdge, rows.prevent_resize_[rows_count]);
  result.SetAllowBorder(kBottomFrameEdge, rows.allow_border_[rows_count]);
  return result;
}

void HTMLFrameSetElement::FillFromEdgeInfo(const FrameEdgeInfo& edge_info,
                                           wtf_size_t r,
                                           wtf_size_t c) {
  auto* object = To<LayoutFrameSet>(GetLayoutObject());
  LayoutFrameSet::GridAxis& cols = object->cols_;
  if (edge_info.AllowBorder(kLeftFrameEdge))
    cols.allow_border_[c] = true;
  if (edge_info.AllowBorder(kRightFrameEdge))
    cols.allow_border_[c + 1] = true;
  if (edge_info.PreventResize(kLeftFrameEdge))
    cols.prevent_resize_[c] = true;
  if (edge_info.PreventResize(kRightFrameEdge))
    cols.prevent_resize_[c + 1] = true;

  LayoutFrameSet::GridAxis& rows = object->rows_;
  if (edge_info.AllowBorder(kTopFrameEdge))
    rows.allow_border_[r] = true;
  if (edge_info.AllowBorder(kBottomFrameEdge))
    rows.allow_border_[r + 1] = true;
  if (edge_info.PreventResize(kTopFrameEdge))
    rows.prevent_resize_[r] = true;
  if (edge_info.PreventResize(kBottomFrameEdge))
    rows.prevent_resize_[r + 1] = true;
}

void HTMLFrameSetElement::CollectEdgeInfo() {
  auto* object = To<LayoutFrameSet>(GetLayoutObject());
  LayoutFrameSet::GridAxis& cols = object->cols_;
  LayoutFrameSet::GridAxis& rows = object->rows_;
  cols.prevent_resize_.Fill(NoResize());
  cols.allow_border_.Fill(false);
  rows.prevent_resize_.Fill(NoResize());
  rows.allow_border_.Fill(false);

  LayoutObject* child = object->FirstChild();
  if (!child)
    return;

  wtf_size_t rows_count = TotalRows();
  wtf_size_t cols_count = TotalCols();
  for (wtf_size_t r = 0; r < rows_count; ++r) {
    for (wtf_size_t c = 0; c < cols_count; ++c) {
      const auto* node = child->GetNode();
      if (const auto* frame_set = DynamicTo<HTMLFrameSetElement>(node))
        FillFromEdgeInfo(frame_set->EdgeInfo(), r, c);
      else
        FillFromEdgeInfo(To<HTMLFrameElement>(node)->EdgeInfo(), r, c);
      child = child->NextSibling();
      if (!child)
        return;
    }
  }
}

bool HTMLFrameSetElement::LayoutObjectIsNeeded(
    const ComputedStyle& style) const {
  // For compatibility, frames layoutObject even when display: none is set.
  return true;
}

LayoutObject* HTMLFrameSetElement::CreateLayoutObject(
    const ComputedStyle& style,
    LegacyLayout legacy) {
  if (style.ContentBehavesAsNormal())
    return MakeGarbageCollected<LayoutFrameSet>(this);
  return LayoutObject::CreateObject(this, style, legacy);
}

void HTMLFrameSetElement::AttachLayoutTree(AttachContext& context) {
  // Inherit default settings from parent frameset
  // FIXME: This is not dynamic.
  if (HTMLFrameSetElement* frameset =
          Traversal<HTMLFrameSetElement>::FirstAncestor(*this)) {
    if (!frameborder_set_)
      frameborder_ = frameset->HasFrameBorder();
    if (frameborder_) {
      if (!border_set_)
        border_ = frameset->Border();
      if (!border_color_set_)
        border_color_set_ = frameset->HasBorderColor();
    }
    if (!noresize_)
      noresize_ = frameset->NoResize();
  }

  HTMLElement::AttachLayoutTree(context);
  is_resizing_ = false;
  resize_rows_.Resize(TotalRows());
  resize_cols_.Resize(TotalCols());
}

void HTMLFrameSetElement::DefaultEventHandler(Event& evt) {
  auto* mouse_event = DynamicTo<MouseEvent>(evt);
  if (mouse_event && !noresize_ && GetLayoutObject() &&
      GetLayoutObject()->IsFrameSet()) {
    if (UserResize(*mouse_event)) {
      evt.SetDefaultHandled();
      return;
    }
  }
  HTMLElement::DefaultEventHandler(evt);
}

Node::InsertionNotificationRequest HTMLFrameSetElement::InsertedInto(
    ContainerNode& insertion_point) {
  if (insertion_point.isConnected() && GetDocument().GetFrame()) {
    // A document using <frameset> likely won't literally have a body, but as
    // far as the client is concerned, the frameset is effectively the body.
    GetDocument().WillInsertBody();
  }
  return HTMLElement::InsertedInto(insertion_point);
}
void HTMLFrameSetElement::WillRecalcStyle(const StyleRecalcChange) {
  if (NeedsStyleRecalc() && GetLayoutObject()) {
    if (GetForceReattachLayoutTree()) {
      // Adding a frameset to the top layer for fullscreen forces a reattach.
      SetNeedsReattachLayoutTree();
    } else {
      GetLayoutObject()->SetNeedsLayoutAndFullPaintInvalidation(
          layout_invalidation_reason::kStyleChange);
    }
    ClearNeedsStyleRecalc();
  }
}

bool HTMLFrameSetElement::UserResize(const MouseEvent& event) {
  auto& layout_frame_set = *To<LayoutFrameSet>(GetLayoutObject());
  if (!is_resizing_) {
    if (layout_frame_set.NeedsLayout())
      return false;
    if (event.type() == event_type_names::kMousedown && event.IsLeftButton()) {
      gfx::PointF local_pos =
          layout_frame_set.AbsoluteToLocalPoint(event.AbsoluteLocation());
      StartResizing(layout_frame_set.cols_, local_pos.x(), resize_cols_);
      StartResizing(layout_frame_set.rows_, local_pos.y(), resize_rows_);
      if (resize_cols_.IsResizingSplit() || resize_rows_.IsResizingSplit()) {
        SetIsResizing(true);
        return true;
      }
    }
  } else {
    if (event.type() == event_type_names::kMousemove ||
        (event.type() == event_type_names::kMouseup && event.IsLeftButton())) {
      gfx::PointF local_pos =
          layout_frame_set.AbsoluteToLocalPoint(event.AbsoluteLocation());
      ContinueResizing(layout_frame_set.cols_, local_pos.x(), resize_cols_);
      ContinueResizing(layout_frame_set.rows_, local_pos.y(), resize_rows_);
      if (event.type() == event_type_names::kMouseup && event.IsLeftButton()) {
        SetIsResizing(false);
        return true;
      }
    }
  }

  return false;
}

void HTMLFrameSetElement::SetIsResizing(bool is_resizing) {
  is_resizing_ = is_resizing;
  if (LocalFrame* frame = GetDocument().GetFrame())
    frame->GetEventHandler().SetResizingFrameSet(is_resizing ? this : nullptr);
}

void HTMLFrameSetElement::StartResizing(LayoutFrameSet::GridAxis& axis,
                                        int position,
                                        ResizeAxis& resize_axis) {
  int split = HitTestSplit(axis, position);
  if (!axis.CanResizeSplitAt(split)) {
    resize_axis.split_being_resized_ = ResizeAxis::kNoSplit;
    return;
  }
  resize_axis.split_being_resized_ = split;
  resize_axis.split_resize_offset_ = position - SplitPosition(axis, split);
}

void HTMLFrameSetElement::ContinueResizing(LayoutFrameSet::GridAxis& axis,
                                           int position,
                                           ResizeAxis& resize_axis) {
  if (GetLayoutObject()->NeedsLayout())
    return;
  if (!resize_axis.IsResizingSplit())
    return;
  int current_split_position =
      SplitPosition(axis, resize_axis.split_being_resized_);
  int delta =
      (position - current_split_position) - resize_axis.split_resize_offset_;
  if (!delta)
    return;
  resize_axis.deltas_[resize_axis.split_being_resized_ - 1] += delta;
  resize_axis.deltas_[resize_axis.split_being_resized_] -= delta;
  GetLayoutObject()->SetNeedsLayoutAndFullPaintInvalidation(
      layout_invalidation_reason::kSizeChanged);
}

int HTMLFrameSetElement::SplitPosition(const LayoutFrameSet::GridAxis& axis,
                                       int split) const {
  if (GetLayoutObject()->NeedsLayout())
    return 0;

  int border_thickness = Border();

  int size = axis.sizes_.size();
  if (!size)
    return 0;

  int position = 0;
  for (int i = 0; i < split && i < size; ++i)
    position += axis.sizes_[i] + border_thickness;
  return position - border_thickness;
}

bool HTMLFrameSetElement::CanResizeRow(const gfx::Point& p) const {
  const LayoutFrameSet::GridAxis& axis =
      To<LayoutFrameSet>(GetLayoutObject())->rows_;
  return axis.CanResizeSplitAt(HitTestSplit(axis, p.y()));
}

bool HTMLFrameSetElement::CanResizeColumn(const gfx::Point& p) const {
  const LayoutFrameSet::GridAxis& axis =
      To<LayoutFrameSet>(GetLayoutObject())->cols_;
  return axis.CanResizeSplitAt(HitTestSplit(axis, p.x()));
}

int HTMLFrameSetElement::HitTestSplit(const LayoutFrameSet::GridAxis& axis,
                                      int position) const {
  if (GetLayoutObject()->NeedsLayout())
    return ResizeAxis::kNoSplit;

  int border_thickness = Border();
  if (border_thickness <= 0)
    return ResizeAxis::kNoSplit;

  wtf_size_t size = axis.sizes_.size();
  if (!size)
    return ResizeAxis::kNoSplit;

  int split_position = axis.sizes_[0];
  for (wtf_size_t i = 1; i < size; ++i) {
    if (position >= split_position &&
        position < split_position + border_thickness)
      return static_cast<int>(i);
    split_position += border_thickness + axis.sizes_[i];
  }
  return ResizeAxis::kNoSplit;
}

void HTMLFrameSetElement::ResizeAxis::Resize(wtf_size_t number_of_frames) {
  deltas_.resize(number_of_frames);
  deltas_.Fill(0);
  split_being_resized_ = kNoSplit;
}

}  // namespace blink
