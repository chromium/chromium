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
#include "third_party/blink/renderer/core/dom/element_traversal.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/events/mouse_event.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/html/frame_edge_info.h"
#include "third_party/blink/renderer/core/html/html_collection.h"
#include "third_party/blink/renderer/core/html/html_frame_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/input/event_handler.h"
#include "third_party/blink/renderer/core/layout/frame_set_layout_data.h"
#include "third_party/blink/renderer/core/layout/layout_frame_set.h"
#include "third_party/blink/renderer/core/layout/physical_box_fragment.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"

namespace blink {

namespace {

constexpr int kDefaultBorderThicknessPx = 6;

const Vector<LayoutUnit>& ColumnSizes(const LayoutBox& box) {
  DCHECK(IsA<LayoutFrameSet>(box));
  // |object| should have only 1 physical fragment because <frameset> is
  // monolithic.
  const auto* data = box.GetPhysicalFragment(0)->GetFrameSetLayoutData();
  DCHECK(data);
  return data->col_sizes;
}

const Vector<LayoutUnit>& RowSizes(const LayoutBox& box) {
  DCHECK(IsA<LayoutFrameSet>(box));
  // |object| should have only 1 physical fragment because <frameset> is
  // monolithic.
  const auto* data = box.GetPhysicalFragment(0)->GetFrameSetLayoutData();
  DCHECK(data);
  return data->row_sizes;
}

}  // namespace

HTMLFrameSetElement::HTMLFrameSetElement(Document& document)
    : HTMLElement(html_names::kFramesetTag, document) {
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
      if (GetLayoutObject() && TotalRows() != resize_rows_.deltas_.size())
        ResizeChildrenData();
    }
    DirtyEdgeInfo();
  } else if (name == html_names::kColsAttr) {
    if (!value.IsNull()) {
      col_lengths_ = ParseListOfDimensions(value.GetString());
      SetNeedsStyleRecalc(kSubtreeStyleChange,
                          StyleChangeReasonForTracing::FromAttribute(name));
      if (GetLayoutObject() && TotalCols() != resize_cols_.deltas_.size())
        ResizeChildrenData();
    }
    DirtyEdgeInfo();
  } else if (name == html_names::kFrameborderAttr) {
    if (!value.IsNull()) {
      if (EqualIgnoringASCIICase(value, "no") ||
          EqualIgnoringASCIICase(value, "0")) {
        frameborder_ = false;
      } else if (EqualIgnoringASCIICase(value, "yes") ||
                 EqualIgnoringASCIICase(value, "1")) {
        frameborder_ = true;
      }
    } else {
      frameborder_.reset();
    }
    DirtyEdgeInfoAndFullPaintInvalidation();
    for (auto& frame_set :
         Traversal<HTMLFrameSetElement>::DescendantsOf(*this)) {
      frame_set.DirtyEdgeInfoAndFullPaintInvalidation();
    }
  } else if (name == html_names::kNoresizeAttr) {
    DirtyEdgeInfo();
  } else if (name == html_names::kBorderAttr) {
    if (!value.IsNull()) {
      border_ = value.ToInt();
    } else {
      border_.reset();
    }
    if (auto* box = GetLayoutBox()) {
      box->SetNeedsLayoutAndFullPaintInvalidation(
          layout_invalidation_reason::kAttributeChanged);
    }
  } else if (name == html_names::kBordercolorAttr) {
    if (GetLayoutBox()) {
      for (const auto& frame_set :
           Traversal<HTMLFrameSetElement>::DescendantsOf(*this)) {
        if (auto* box = frame_set.GetLayoutBox()) {
          box->SetNeedsLayoutAndFullPaintInvalidation(
              layout_invalidation_reason::kAttributeChanged);
        }
      }
    }
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

bool HTMLFrameSetElement::HasFrameBorder() const {
  if (frameborder_.has_value())
    return *frameborder_;
  if (const auto* frame_set = DynamicTo<HTMLFrameSetElement>(parentNode()))
    return frame_set->HasFrameBorder();
  return true;
}

bool HTMLFrameSetElement::NoResize() const {
  if (FastHasAttribute(html_names::kNoresizeAttr))
    return true;
  if (const auto* frame_set = DynamicTo<HTMLFrameSetElement>(parentNode()))
    return frame_set->NoResize();
  return false;
}

int HTMLFrameSetElement::Border(const ComputedStyle& style) const {
  if (!HasFrameBorder())
    return 0;
  if (border_.has_value()) {
    return *border_ == 0
               ? 0
               : std::max(ClampTo<int>(*border_ * style.EffectiveZoom()), 1);
  }
  if (const auto* frame_set = DynamicTo<HTMLFrameSetElement>(parentNode()))
    return frame_set->Border(style);
  return ClampTo<int>(kDefaultBorderThicknessPx * style.EffectiveZoom());
}

bool HTMLFrameSetElement::HasBorderColor() const {
  if (FastHasAttribute(html_names::kBordercolorAttr))
    return true;
  if (const auto* frame_set = DynamicTo<HTMLFrameSetElement>(parentNode()))
    return frame_set->HasBorderColor();
  return false;
}

FrameEdgeInfo HTMLFrameSetElement::EdgeInfo() const {
  const_cast<HTMLFrameSetElement*>(this)->CollectEdgeInfoIfDirty();
  FrameEdgeInfo result(NoResize(), true);

  wtf_size_t rows_count = TotalRows();
  wtf_size_t cols_count = TotalCols();
  DCHECK_GT(rows_count, 0u);
  DCHECK_GT(cols_count, 0u);
  result.SetPreventResize(kLeftFrameEdge, resize_cols_.prevent_resize_[0]);
  result.SetAllowBorder(kLeftFrameEdge, allow_border_cols_[0]);
  result.SetPreventResize(kRightFrameEdge,
                          resize_cols_.prevent_resize_[cols_count]);
  result.SetAllowBorder(kRightFrameEdge, allow_border_cols_[cols_count]);
  result.SetPreventResize(kTopFrameEdge, resize_rows_.prevent_resize_[0]);
  result.SetAllowBorder(kTopFrameEdge, allow_border_rows_[0]);
  result.SetPreventResize(kBottomFrameEdge,
                          resize_rows_.prevent_resize_[rows_count]);
  result.SetAllowBorder(kBottomFrameEdge, allow_border_rows_[rows_count]);
  return result;
}

void HTMLFrameSetElement::FillFromEdgeInfo(const FrameEdgeInfo& edge_info,
                                           wtf_size_t r,
                                           wtf_size_t c) {
  if (edge_info.AllowBorder(kLeftFrameEdge))
    allow_border_cols_[c] = true;
  if (edge_info.AllowBorder(kRightFrameEdge))
    allow_border_cols_[c + 1] = true;
  if (edge_info.PreventResize(kLeftFrameEdge))
    resize_cols_.prevent_resize_[c] = true;
  if (edge_info.PreventResize(kRightFrameEdge))
    resize_cols_.prevent_resize_[c + 1] = true;

  if (edge_info.AllowBorder(kTopFrameEdge))
    allow_border_rows_[r] = true;
  if (edge_info.AllowBorder(kBottomFrameEdge))
    allow_border_rows_[r + 1] = true;
  if (edge_info.PreventResize(kTopFrameEdge))
    resize_rows_.prevent_resize_[r] = true;
  if (edge_info.PreventResize(kBottomFrameEdge))
    resize_rows_.prevent_resize_[r + 1] = true;
}

void HTMLFrameSetElement::CollectEdgeInfoIfDirty() {
  if (!is_edge_info_dirty_)
    return;
  is_edge_info_dirty_ = false;
  resize_cols_.prevent_resize_.Fill(NoResize());
  allow_border_cols_.Fill(false);
  resize_rows_.prevent_resize_.Fill(NoResize());
  allow_border_rows_.Fill(false);

  LayoutObject* child = GetLayoutObject()->SlowFirstChild();
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

void HTMLFrameSetElement::DirtyEdgeInfo() {
  is_edge_info_dirty_ = true;
  if (auto* parent_frame_set = DynamicTo<HTMLFrameSetElement>(parentNode()))
    parent_frame_set->DirtyEdgeInfo();
}

void HTMLFrameSetElement::DirtyEdgeInfoAndFullPaintInvalidation() {
  is_edge_info_dirty_ = true;
  if (auto* box = GetLayoutBox()) {
    box->SetNeedsLayoutAndFullPaintInvalidation(
        layout_invalidation_reason::kAttributeChanged);
  }
  if (auto* parent_frame_set = DynamicTo<HTMLFrameSetElement>(parentNode()))
    parent_frame_set->DirtyEdgeInfoAndFullPaintInvalidation();
}

const Vector<bool>& HTMLFrameSetElement::AllowBorderRows() const {
  const_cast<HTMLFrameSetElement*>(this)->CollectEdgeInfoIfDirty();
  return allow_border_rows_;
}
const Vector<bool>& HTMLFrameSetElement::AllowBorderColumns() const {
  const_cast<HTMLFrameSetElement*>(this)->CollectEdgeInfoIfDirty();
  return allow_border_cols_;
}

bool HTMLFrameSetElement::LayoutObjectIsNeeded(
    const DisplayStyle& style) const {
  // For compatibility, frames layoutObject even when display: none is set.
  return true;
}

LayoutObject* HTMLFrameSetElement::CreateLayoutObject(
    const ComputedStyle& style) {
  if (style.ContentBehavesAsNormal())
    return MakeGarbageCollected<LayoutFrameSet>(this);
  return LayoutObject::CreateObject(this, style);
}

void HTMLFrameSetElement::AttachLayoutTree(AttachContext& context) {
  HTMLElement::AttachLayoutTree(context);
  is_resizing_ = false;
  ResizeChildrenData();
}

void HTMLFrameSetElement::DefaultEventHandler(Event& evt) {
  auto* mouse_event = DynamicTo<MouseEvent>(evt);
  if (mouse_event && !NoResize() && GetLayoutObject() &&
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
  const auto& box = *GetLayoutBox();
  if (!is_resizing_) {
    if (box.NeedsLayout())
      return false;
    if (event.type() == event_type_names::kMousedown && event.IsLeftButton()) {
      gfx::PointF local_pos =
          box.AbsoluteToLocalPoint(event.AbsoluteLocation());
      StartResizing(ColumnSizes(box), local_pos.x(), resize_cols_);
      StartResizing(RowSizes(box), local_pos.y(), resize_rows_);
      if (resize_cols_.IsResizingSplit() || resize_rows_.IsResizingSplit()) {
        SetIsResizing(true);
        return true;
      }
    }
  } else {
    if (event.type() == event_type_names::kMousemove ||
        (event.type() == event_type_names::kMouseup && event.IsLeftButton())) {
      gfx::PointF local_pos =
          box.AbsoluteToLocalPoint(event.AbsoluteLocation());
      ContinueResizing(ColumnSizes(box), local_pos.x(), resize_cols_);
      ContinueResizing(RowSizes(box), local_pos.y(), resize_rows_);
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

void HTMLFrameSetElement::StartResizing(const Vector<LayoutUnit>& sizes,
                                        int position,
                                        ResizeAxis& resize_axis) {
  int split = HitTestSplit(sizes, position);
  CollectEdgeInfoIfDirty();
  if (!resize_axis.CanResizeSplitAt(split)) {
    resize_axis.split_being_resized_ = ResizeAxis::kNoSplit;
    return;
  }
  resize_axis.split_being_resized_ = split;
  resize_axis.split_resize_offset_ = position - SplitPosition(sizes, split);
}

void HTMLFrameSetElement::ContinueResizing(const Vector<LayoutUnit>& sizes,
                                           int position,
                                           ResizeAxis& resize_axis) {
  if (GetLayoutObject()->NeedsLayout())
    return;
  if (!resize_axis.IsResizingSplit())
    return;
  const int split_index = resize_axis.split_being_resized_;
  int current_split_position = SplitPosition(sizes, split_index);
  int delta =
      (position - current_split_position) - resize_axis.split_resize_offset_;
  if (!delta)
    return;
  const LayoutUnit original_size_prev =
      sizes[split_index - 1] - resize_axis.deltas_[split_index - 1];
  const LayoutUnit original_size_next =
      sizes[split_index] - resize_axis.deltas_[split_index];
  if ((original_size_prev != 0 && sizes[split_index - 1] + delta <= 0) ||
      (original_size_next != 0 && sizes[split_index] - delta <= 0)) {
    resize_axis.deltas_.Fill(0);
  } else {
    resize_axis.deltas_[split_index - 1] += delta;
    resize_axis.deltas_[split_index] -= delta;
  }
  GetLayoutObject()->SetNeedsLayoutAndFullPaintInvalidation(
      layout_invalidation_reason::kSizeChanged);
}

int HTMLFrameSetElement::SplitPosition(const Vector<LayoutUnit>& sizes,
                                       int split) const {
  if (GetLayoutObject()->NeedsLayout())
    return 0;

  int border_thickness = Border(GetLayoutObject()->StyleRef());

  int size = sizes.size();
  if (!size)
    return 0;

  int position = 0;
  for (int i = 0; i < split && i < size; ++i)
    position += sizes[i].ToInt() + border_thickness;
  return position - border_thickness;
}

bool HTMLFrameSetElement::CanResizeRow(const gfx::Point& p) const {
  const_cast<HTMLFrameSetElement*>(this)->CollectEdgeInfoIfDirty();
  return resize_rows_.CanResizeSplitAt(
      HitTestSplit(RowSizes(*GetLayoutBox()), p.y()));
}

bool HTMLFrameSetElement::CanResizeColumn(const gfx::Point& p) const {
  const_cast<HTMLFrameSetElement*>(this)->CollectEdgeInfoIfDirty();
  return resize_cols_.CanResizeSplitAt(
      HitTestSplit(ColumnSizes(*GetLayoutBox()), p.x()));
}

int HTMLFrameSetElement::HitTestSplit(const Vector<LayoutUnit>& sizes,
                                      int position) const {
  if (GetLayoutObject()->NeedsLayout())
    return ResizeAxis::kNoSplit;

  int border_thickness = Border(GetLayoutObject()->StyleRef());
  if (border_thickness <= 0)
    return ResizeAxis::kNoSplit;

  wtf_size_t size = sizes.size();
  if (!size)
    return ResizeAxis::kNoSplit;

  int split_position = sizes[0].ToInt();
  for (wtf_size_t i = 1; i < size; ++i) {
    if (position >= split_position &&
        position < split_position + border_thickness)
      return static_cast<int>(i);
    split_position += border_thickness + sizes[i].ToInt();
  }
  return ResizeAxis::kNoSplit;
}

void HTMLFrameSetElement::ResizeChildrenData() {
  resize_rows_.Resize(TotalRows());
  resize_cols_.Resize(TotalCols());

  // To track edges for borders, we need to be (size + 1). This is because a
  // parent frameset may ask us for information about our left/top/right/bottom
  // edges in order to make its own decisions about what to do. We are capable
  // of tainting that parent frameset's borders, so we have to cache this info.
  allow_border_rows_.resize(TotalRows() + 1);
  allow_border_cols_.resize(TotalCols() + 1);
}

void HTMLFrameSetElement::ResizeAxis::Resize(wtf_size_t number_of_frames) {
  deltas_.resize(number_of_frames);
  deltas_.Fill(0);
  split_being_resized_ = kNoSplit;

  // To track edges for resizability, we need to be (size + 1). This is because
  // a parent frameset may ask us for information about our left/top/right/
  // bottom edges in order to make its own decisions about what to do. We are
  // capable of tainting that parent frameset's borders, so we have to cache
  // this info.
  prevent_resize_.resize(number_of_frames + 1);
}

bool HTMLFrameSetElement::ResizeAxis::CanResizeSplitAt(int split_index) const {
  return split_index != kNoSplit && !prevent_resize_[split_index];
}

}  // namespace blink
