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
#include "third_party/blink/renderer/core/geometry/dom_rect.h"
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

  // TODO(1052145): Move this further into the document lifecycle when layout
  // is up to date.
  PhysicalRect physical_rect_area = layout_box->LocalToAbsoluteRect(
      layout_box->PhysicalBorderBoxRect(), kTraverseDocumentBoundaries);
  gfx::RectF area(physical_rect_area.X().ToFloat(),
                  physical_rect_area.Y().ToFloat(),
                  physical_rect_area.Width().ToFloat(),
                  physical_rect_area.Height().ToFloat());

  TRACE_EVENT_INSTANT2("blink",
                       "DelegatedInkTrailPresenter::updateInkTrailStartPoint",
                       TRACE_EVENT_SCOPE_THREAD, "point", point.ToString(),
                       "area", area.ToString());

  const double diameter_in_physical_pixels = style->diameter() * effective_zoom;
  std::unique_ptr<viz::DelegatedInkMetadata> metadata =
      std::make_unique<viz::DelegatedInkMetadata>(
          point, diameter_in_physical_pixels, color.Rgb(),
          evt->PlatformTimeStamp(), area);

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
