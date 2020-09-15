// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/delegated_ink/delegated_ink_trail_presenter.h"

#include "components/viz/common/delegated_ink_metadata.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ink_trail_style.h"
#include "third_party/blink/renderer/core/css/parser/css_parser.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/events/pointer_event.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/visual_viewport.h"
#include "third_party/blink/renderer/core/geometry/dom_rect.h"
#include "third_party/blink/renderer/core/layout/layout_embedded_content.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

DelegatedInkTrailPresenter* DelegatedInkTrailPresenter::CreatePresenter(
    Element* element,
    LocalFrame* frame) {
  DCHECK(!element || element->GetDocument() == frame->GetDocument());

  return MakeGarbageCollected<DelegatedInkTrailPresenter>(element, frame);
}

DelegatedInkTrailPresenter::DelegatedInkTrailPresenter(Element* element,
                                                       LocalFrame* frame)
    : presentation_area_(element), local_frame_(frame) {}

void ThrowException(v8::Isolate* isolate,
                    ExceptionCode code,
                    const String& error_message) {
  ExceptionState exception_state(isolate, ExceptionState::kExecutionContext,
                                 "DelegatedInkTrailPresenter",
                                 "updateInkTrailStatePoint");
  exception_state.ThrowException(code, error_message);
}

void DelegatedInkTrailPresenter::updateInkTrailStartPoint(
    ScriptState* state,
    PointerEvent* evt,
    InkTrailStyle* style) {
  DCHECK(RuntimeEnabledFeatures::DelegatedInkTrailsEnabled());

  if (!state->ContextIsValid()) {
    ThrowException(state->GetIsolate(),
                   ToExceptionCode(DOMExceptionCode::kInvalidStateError),
                   "The object is no longer associated with a window.");
    return;
  }

  if (!evt->isTrusted()) {
    ThrowException(state->GetIsolate(),
                   ToExceptionCode(DOMExceptionCode::kNotAllowedError),
                   "Only trusted pointerevents are accepted.");
    return;
  }

  // If diameter is less than or equal to 0, then nothing is going to be
  // displayed anyway, so just bail early and save the effort.
  if (style->diameter() <= 0) {
    ThrowException(state->GetIsolate(),
                   ToExceptionCode(DOMExceptionCode::kNotSupportedError),
                   "Delegated ink trail diameter must be greater than 0.");
    return;
  }

  Color color;
  if (!CSSParser::ParseColor(color, style->color(), true /*strict*/)) {
    ThrowException(state->GetIsolate(),
                   ToExceptionCode(ESErrorType::kTypeError), "Unknown color.");
    return;
  }

  LayoutView* layout_view = local_frame_->ContentLayoutObject();
  DCHECK(layout_view);
  const float effective_zoom = layout_view->StyleRef().EffectiveZoom();

  PhysicalOffset physical_point(LayoutUnit(evt->x()), LayoutUnit(evt->y()));
  physical_point.Scale(effective_zoom);
  physical_point = layout_view->LocalToAbsolutePoint(
      physical_point, kTraverseDocumentBoundaries);
  gfx::PointF point(physical_point.left.ToFloat(),
                    physical_point.top.ToFloat());

  LayoutBox* layout_box = nullptr;
  if (presentation_area_) {
    layout_box = presentation_area_->GetLayoutBox();
    DCHECK(layout_box);
  } else {
    // If presentation_area_ wasn't provided, then default to the layout
    // viewport.
    layout_box = layout_view;
  }

  // Intersect with the visible viewport so that the presentation area can't
  // extend beyond the edges of the window or over the scrollbars. The frame
  // visual viewport loop accounts for all iframe viewports, and the page visual
  // viewport accounts for the full window. Convert everything to root frame
  // coordinates in order to make sure offsets aren't lost along the way.
  //
  // TODO(1052145): Overflow and clip-path clips are ignored here, which results
  // in delegated ink trails ignoring the clips and appearing incorrectly in
  // some situations. This could also occur due to transformations, as the
  // |presenation_area| is currently always a rectilinear bounding box. Ideally
  // both of these situations are handled correctly, or the trail doesn't appear
  // if we are unable to accurately render it.
  PhysicalRect border_box_rect_absolute = layout_box->LocalToAbsoluteRect(
      layout_box->PhysicalBorderBoxRect(), kTraverseDocumentBoundaries);

  while (layout_view->GetFrame()->OwnerLayoutObject()) {
    PhysicalRect frame_visual_viewport_absolute =
        layout_view->LocalToAbsoluteRect(
            PhysicalRect(
                layout_view->GetScrollableArea()->VisibleContentRect()),
            kTraverseDocumentBoundaries);
    border_box_rect_absolute.Intersect(frame_visual_viewport_absolute);

    layout_view = layout_view->GetFrame()->OwnerLayoutObject()->View();
  }

  border_box_rect_absolute.Intersect(PhysicalRect(
      local_frame_->GetPage()->GetVisualViewport().VisibleContentRect()));

  gfx::RectF area(border_box_rect_absolute.X().ToFloat(),
                  border_box_rect_absolute.Y().ToFloat(),
                  border_box_rect_absolute.Width().ToFloat(),
                  border_box_rect_absolute.Height().ToFloat());

  const double diameter_in_physical_pixels = style->diameter() * effective_zoom;
  std::unique_ptr<viz::DelegatedInkMetadata> metadata =
      std::make_unique<viz::DelegatedInkMetadata>(
          point, diameter_in_physical_pixels, color.Rgb(),
          evt->PlatformTimeStamp(), area);

  TRACE_EVENT_INSTANT1(
      "blink", "DelegatedInkTrailPresenter::updateInkTrailStartPoint",
      TRACE_EVENT_SCOPE_THREAD, "ink metadata", metadata->ToString());

  Page* page = local_frame_->GetPage();
  page->GetChromeClient().SetDelegatedInkMetadata(local_frame_,
                                                  std::move(metadata));
}

void DelegatedInkTrailPresenter::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
  visitor->Trace(presentation_area_);
  visitor->Trace(local_frame_);
}

}  // namespace blink
