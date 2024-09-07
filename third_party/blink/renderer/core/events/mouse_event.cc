/*
 * Copyright (C) 2001 Peter Kelly (pmk@post.com)
 * Copyright (C) 2001 Tobias Anton (anton@stud.fbi.fh-darmstadt.de)
 * Copyright (C) 2006 Samuel Weinig (sam.weinig@gmail.com)
 * Copyright (C) 2003, 2005, 2006, 2008 Apple Inc. All rights reserved.
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

#include "third_party/blink/renderer/core/events/mouse_event.h"

#include "third_party/blink/public/common/input/web_pointer_properties.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_mouse_event_init.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/events/event_dispatcher.h"
#include "third_party/blink/renderer/core/dom/events/event_path.h"
#include "third_party/blink/renderer/core/event_interface_names.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/html/canvas/html_canvas_element.h"
#include "third_party/blink/renderer/core/html/html_frame_element_base.h"
#include "third_party/blink/renderer/core/html/media/html_media_element.h"
#include "third_party/blink/renderer/core/input/input_device_capabilities.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/page/pointer_lock_controller.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/svg/svg_element.h"
#include "third_party/blink/renderer/platform/bindings/dom_wrapper_world.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"

namespace blink {

namespace {

float LayoutZoomFactor(const LocalDOMWindow* local_dom_window) {
  if (!local_dom_window)
    return 1.f;
  LocalFrame* frame = local_dom_window->GetFrame();
  if (!frame)
    return 1.f;
  return frame->LayoutZoomFactor();
}

const LayoutObject* FindTargetLayoutObject(Node*& target_node) {
  LayoutObject* layout_object = target_node->GetLayoutObject();
  if (!layout_object || !layout_object->IsSVG())
    return layout_object;
  // If this is an SVG node, compute the offset to the padding box of the
  // outermost SVG root (== the closest ancestor that has a CSS layout box.)
  while (!layout_object->IsSVGRoot())
    layout_object = layout_object->Parent();
  // Update the target node to point to the SVG root.
  target_node = layout_object->GetNode();
  auto* svg_element = DynamicTo<SVGElement>(target_node);
  DCHECK(!target_node ||
         (svg_element && svg_element->IsOutermostSVGSVGElement()));
  return layout_object;
}

unsigned ButtonsToWebInputEventModifiers(uint16_t buttons) {
  unsigned modifiers = 0;

  if (buttons & static_cast<uint16_t>(WebPointerProperties::Buttons::kLeft))
    modifiers |= WebInputEvent::kLeftButtonDown;
  if (buttons & static_cast<uint16_t>(WebPointerProperties::Buttons::kRight))
    modifiers |= WebInputEvent::kRightButtonDown;
  if (buttons & static_cast<uint16_t>(WebPointerProperties::Buttons::kMiddle))
    modifiers |= WebInputEvent::kMiddleButtonDown;
  if (buttons & static_cast<uint16_t>(WebPointerProperties::Buttons::kBack))
    modifiers |= WebInputEvent::kBackButtonDown;
  if (buttons & static_cast<uint16_t>(WebPointerProperties::Buttons::kForward))
    modifiers |= WebInputEvent::kForwardButtonDown;

  return modifiers;
}

}  // namespace

MouseEvent* MouseEvent::Create(ScriptState* script_state,
                               const AtomicString& type,
                               const MouseEventInit* initializer) {
  LocalDOMWindow* fallback_dom_window = nullptr;
  if (script_state) {
    if (script_state->World().IsIsolatedWorld()) {
      UIEventWithKeyState::DidCreateEventInIsolatedWorld(
          initializer->ctrlKey(), initializer->altKey(),
          initializer->shiftKey(), initializer->metaKey());
    }
    // If we don't have a view, we'll have to get a fallback dom window in
    // order to properly account for device scale factor.
    if (!initializer || !initializer->view()) {
      if (auto* execution_context = ExecutionContext::From(script_state);
          execution_context && execution_context->IsWindow()) {
        fallback_dom_window = static_cast<LocalDOMWindow*>(execution_context);
      }
    }
  }
  return MakeGarbageCollected<MouseEvent>(
      type, initializer, base::TimeTicks::Now(), kRealOrIndistinguishable,
      kMenuSourceNone, fallback_dom_window);
}

MouseEvent* MouseEvent::Create(const AtomicString& event_type,
                               const MouseEventInit* initializer,
                               base::TimeTicks platform_time_stamp,
                               SyntheticEventType synthetic_event_type,
                               WebMenuSourceType menu_source_type) {
  return MakeGarbageCollected<MouseEvent>(
      event_type, initializer, platform_time_stamp, synthetic_event_type,
      menu_source_type);
}

MouseEvent::MouseEvent()
    : position_type_(PositionType::kPosition),
      button_(0),
      buttons_(0),
      related_target_(nullptr),
      synthetic_event_type_(kRealOrIndistinguishable) {}

MouseEvent::MouseEvent(const AtomicString& event_type,
                       const MouseEventInit* initializer,
                       base::TimeTicks platform_time_stamp,
                       SyntheticEventType synthetic_event_type,
                       WebMenuSourceType menu_source_type,
                       LocalDOMWindow* fallback_dom_window)
    : UIEventWithKeyState(event_type, initializer, platform_time_stamp),
      screen_x_(initializer->screenX()),
      screen_y_(initializer->screenY()),
      movement_delta_(initializer->movementX(), initializer->movementY()),
      position_type_(synthetic_event_type == kPositionless
                         ? PositionType::kPositionless
                         : PositionType::kPosition),
      button_(initializer->button()),
      buttons_(initializer->buttons()),
      related_target_(initializer->relatedTarget()),
      synthetic_event_type_(synthetic_event_type),
      menu_source_type_(menu_source_type) {
  InitCoordinates(initializer->clientX(), initializer->clientY(),
                  fallback_dom_window);
  modifiers_ |= ButtonsToWebInputEventModifiers(buttons_);
}

void MouseEvent::InitCoordinates(const double client_x,
                                 const double client_y,
                                 const LocalDOMWindow* fallback_dom_window) {
  client_x_ = page_x_ = client_x;
  client_y_ = page_y_ = client_y;
  absolute_location_ = gfx::PointF(client_x, client_y);

  auto* local_dom_window = DynamicTo<LocalDOMWindow>(view());
  float zoom_factor = LayoutZoomFactor(local_dom_window ? local_dom_window
                                                        : fallback_dom_window);

  if (local_dom_window) {
    if (LocalFrame* frame = local_dom_window->GetFrame()) {
      // Adjust page_x_ and page_y_ by layout viewport scroll offset.
      if (ScrollableArea* scrollable_area = frame->View()->LayoutViewport()) {
        gfx::Vector2d scroll_offset = scrollable_area->ScrollOffsetInt();
        page_x_ += scroll_offset.x() / zoom_factor;
        page_y_ += scroll_offset.y() / zoom_factor;
      }
    }
  }

  // absolute_location_ is not an API value. It's in layout space.
  absolute_location_.Scale(zoom_factor);

  // Correct values of the following are computed lazily, see
  // ComputeRelativePosition().
  offset_x_ = page_x_;
  offset_y_ = page_y_;
  layer_location_ = gfx::PointF(page_x_, page_y_);

  has_cached_relative_position_ = false;
}

void MouseEvent::SetCoordinatesFromWebPointerProperties(
    const WebPointerProperties& web_pointer_properties,
    const LocalDOMWindow* dom_window,
    MouseEventInit* initializer) {
  gfx::PointF client_point;
  gfx::PointF screen_point = web_pointer_properties.PositionInScreen();
  float inverse_zoom_factor = 1.0f;
  if (dom_window && dom_window->GetFrame() && dom_window->GetFrame()->View()) {
    LocalFrame* frame = dom_window->GetFrame();
    gfx::PointF root_frame_point = web_pointer_properties.PositionInWidget();
    if (Page* p = frame->GetPage()) {
      if (p->GetPointerLockController().GetElement() &&
          !p->GetPointerLockController().LockPending()) {
        p->GetPointerLockController().GetPointerLockPosition(&root_frame_point,
                                                             &screen_point);
      }
    }
    gfx::PointF frame_point =
        frame->View()->ConvertFromRootFrame(root_frame_point);
    inverse_zoom_factor = 1.0f / frame->LayoutZoomFactor();
    client_point = gfx::ScalePoint(frame_point, inverse_zoom_factor);
  }

  initializer->setScreenX(screen_point.x());
  initializer->setScreenY(screen_point.y());
  initializer->setClientX(client_point.x());
  initializer->setClientY(client_point.y());

  // TODO(crbug.com/982379): We need to merge the code path of raw movement
  // events and regular events so that we can remove the block below.
  if (web_pointer_properties.is_raw_movement_event) {
    // TODO(nzolghadr): We need to scale movement attrinutes as well. But if we
    // do that here and round it to the int again it causes inconsistencies
    // between screenX/Y and cumulative movementX/Y.
    initializer->setMovementX(web_pointer_properties.movement_x);
    initializer->setMovementY(web_pointer_properties.movement_y);
  }
}

uint16_t MouseEvent::WebInputEventModifiersToButtons(unsigned modifiers) {
  uint16_t buttons = 0;

  if (modifiers & WebInputEvent::kLeftButtonDown)
    buttons |= static_cast<uint16_t>(WebPointerProperties::Buttons::kLeft);
  if (modifiers & WebInputEvent::kRightButtonDown) {
    buttons |= static_cast<uint16_t>(WebPointerProperties::Buttons::kRight);
  }
  if (modifiers & WebInputEvent::kMiddleButtonDown) {
    buttons |= static_cast<uint16_t>(WebPointerProperties::Buttons::kMiddle);
  }
  if (modifiers & WebInputEvent::kBackButtonDown)
    buttons |= static_cast<uint16_t>(WebPointerProperties::Buttons::kBack);
  if (modifiers & WebInputEvent::kForwardButtonDown) {
    buttons |= static_cast<uint16_t>(WebPointerProperties::Buttons::kForward);
  }

  return buttons;
}

void MouseEvent::initMouseEvent(ScriptState* script_state,
                                const AtomicString& type,
                                bool bubbles,
                                bool cancelable,
                                AbstractView* view,
                                int detail,
                                int screen_x,
                                int screen_y,
                                int client_x,
                                int client_y,
                                bool ctrl_key,
                                bool alt_key,
                                bool shift_key,
                                bool meta_key,
                                int16_t button,
                                EventTarget* related_target,
                                uint16_t buttons) {
  if (IsBeingDispatched())
    return;

  if (script_state && script_state->World().IsIsolatedWorld())
    UIEventWithKeyState::DidCreateEventInIsolatedWorld(ctrl_key, alt_key,
                                                       shift_key, meta_key);

  InitModifiers(ctrl_key, alt_key, shift_key, meta_key);
  InitMouseEventInternal(type, bubbles, cancelable, view, detail, screen_x,
                         screen_y, client_x, client_y, GetModifiers(), button,
                         related_target, nullptr, buttons);
}

void MouseEvent::InitMouseEventInternal(
    const AtomicString& type,
    bool bubbles,
    bool cancelable,
    AbstractView* view,
    int detail,
    double screen_x,
    double screen_y,
    double client_x,
    double client_y,
    WebInputEvent::Modifiers modifiers,
    int16_t button,
    EventTarget* related_target,
    InputDeviceCapabilities* source_capabilities,
    uint16_t buttons) {
  InitUIEventInternal(type, bubbles, cancelable, related_target, view, detail,
                      source_capabilities);

  screen_x_ = screen_x;
  screen_y_ = screen_y;
  button_ = button;
  buttons_ = buttons;
  related_target_ = related_target;
  modifiers_ = modifiers;

  InitCoordinates(client_x, client_y);

  // FIXME: SyntheticEventType is not set to RealOrIndistinguishable here.
}

void MouseEvent::InitCoordinatesForTesting(double screen_x,
                                           double screen_y,
                                           double client_x,
                                           double client_y) {
  screen_x_ = screen_x;
  screen_y_ = screen_y;
  InitCoordinates(client_x, client_y);
}

const AtomicString& MouseEvent::InterfaceName() const {
  return event_interface_names::kMouseEvent;
}

bool MouseEvent::IsMouseEvent() const {
  return true;
}

int16_t MouseEvent::button() const {
  const AtomicString& event_name = type();
  if (button_ == -1 || event_name == event_type_names::kMousemove ||
      event_name == event_type_names::kMouseleave ||
      event_name == event_type_names::kMouseenter ||
      event_name == event_type_names::kMouseover ||
      event_name == event_type_names::kMouseout) {
    return 0;
  }
  return button_;
}

bool MouseEvent::IsLeftButton() const {
  return button() == static_cast<int16_t>(WebPointerProperties::Button::kLeft);
}

unsigned MouseEvent::which() const {
  // For the DOM, the return values for left, middle and right mouse buttons are
  // 0, 1, 2, respectively.
  // For the Netscape "which" property, the return values for left, middle and
  // right mouse buttons are 1, 2, 3, respectively.
  // So we must add 1.
  return (unsigned)(button_ + 1);
}

Node* MouseEvent::toElement() const {
  // MSIE extension - "the object toward which the user is moving the mouse
  // pointer"
  if (type() == event_type_names::kMouseout ||
      type() == event_type_names::kMouseleave)
    return relatedTarget() ? relatedTarget()->ToNode() : nullptr;

  return target() ? target()->ToNode() : nullptr;
}

Node* MouseEvent::fromElement() const {
  // MSIE extension - "object from which activation or the mouse pointer is
  // exiting during the event" (huh?)
  if (type() != event_type_names::kMouseout &&
      type() != event_type_names::kMouseleave)
    return relatedTarget() ? relatedTarget()->ToNode() : nullptr;

  return target() ? target()->ToNode() : nullptr;
}

void MouseEvent::Trace(Visitor* visitor) const {
  visitor->Trace(related_target_);
  UIEventWithKeyState::Trace(visitor);
}

DispatchEventResult MouseEvent::DispatchEvent(EventDispatcher& dispatcher) {
  // TODO(mustaq): Move click-specific code to `PointerEvent::DispatchEvent`.
  GetEventPath().AdjustForRelatedTarget(dispatcher.GetNode(), relatedTarget());

  bool is_click = type() == event_type_names::kClick;

  if (!isTrusted())
    return dispatcher.Dispatch();

  if (is_click || type() == event_type_names::kMousedown ||
      type() == event_type_names::kMouseup ||
      type() == event_type_names::kDblclick) {
    GetEventPath().AdjustForDisabledFormControl();
  }

  if (type().empty())
    return DispatchEventResult::kNotCanceled;  // Shouldn't happen.

  if (is_click) {
    auto& path = GetEventPath();
    bool saw_disabled_control = false;
    for (unsigned i = 0; i < path.size(); i++) {
      auto& node = path[i].GetNode();
      if (saw_disabled_control && node.WillRespondToMouseClickEvents()) {
        UseCounter::Count(
            node.GetDocument(),
            WebFeature::kParentOfDisabledFormControlRespondsToMouseEvents);
      }
      if (IsDisabledFormControl(&node))
        saw_disabled_control = true;
    }
  }

  DCHECK(!target() || target() != relatedTarget());

  EventTarget* related_target = relatedTarget();

  DispatchEventResult dispatch_result = dispatcher.Dispatch();

  if (!is_click || detail() != 2)
    return dispatch_result;

  // Special case: If it's a double click event, we also send the dblclick
  // event. This is not part of the DOM specs, but is used for compatibility
  // with the ondblclick="" attribute. This is treated as a separate event in
  // other DOM-compliant browsers like Firefox, and so we do the same.
  MouseEvent& double_click_event = *MouseEvent::Create();
  double_click_event.InitMouseEventInternal(
      event_type_names::kDblclick, bubbles(), cancelable(), view(), detail(),
      screenX(), screenY(), clientX(), clientY(), GetModifiers(), button(),
      related_target, sourceCapabilities(), buttons());
  double_click_event.SetComposed(composed());

  // Inherit the trusted status from the original event.
  double_click_event.SetTrusted(isTrusted());
  if (DefaultHandled())
    double_click_event.SetDefaultHandled();
  DispatchEventResult double_click_dispatch_result =
      EventDispatcher::DispatchEvent(dispatcher.GetNode(), double_click_event);
  if (double_click_dispatch_result != DispatchEventResult::kNotCanceled)
    return double_click_dispatch_result;
  return dispatch_result;
}

void MouseEvent::ReceivedTarget() {
  has_cached_relative_position_ = false;
}

void MouseEvent::ComputeRelativePosition() {
  Node* target_node = target() ? target()->ToNode() : nullptr;
  if (!target_node)
    return;

  // Compute coordinates that are based on the target.
  offset_x_ = page_x_;
  offset_y_ = page_y_;
  layer_location_ = gfx::PointF(page_x_, page_y_);

  LocalDOMWindow* dom_window_for_zoom_factor =
      DynamicTo<LocalDOMWindow>(view());
  if (!dom_window_for_zoom_factor)
    dom_window_for_zoom_factor = target_node->GetDocument().domWindow();

  float zoom_factor = LayoutZoomFactor(dom_window_for_zoom_factor);
  float inverse_zoom_factor = 1 / zoom_factor;

  // Must have an updated layout tree for this math to work correctly.
  target_node->GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kInput);

  // Adjust offsetLocation to be relative to the target's padding box.
  if (const LayoutObject* layout_object = FindTargetLayoutObject(target_node)) {
    gfx::PointF local_pos =
        layout_object->AbsoluteToLocalPoint(AbsoluteLocation());

    if (layout_object->IsInline()) {
      UseCounter::Count(
          target_node->GetDocument(),
          WebFeature::kMouseEventRelativePositionForInlineElement);
    }

    // Adding this here to address crbug.com/570666. Basically we'd like to
    // find the local coordinates relative to the padding box not the border
    // box.
    if (layout_object->IsBoxModelObject()) {
      const auto* layout_box = To<LayoutBoxModelObject>(layout_object);
      local_pos.Offset(-layout_box->BorderLeft(), -layout_box->BorderTop());
    }

    offset_x_ = local_pos.x() * inverse_zoom_factor;
    offset_y_ = local_pos.y() * inverse_zoom_factor;
  }

  // Adjust layerLocation to be relative to the layer.
  // FIXME: event.layerX and event.layerY are poorly defined,
  // and probably don't always correspond to PaintLayer offsets.
  // https://bugs.webkit.org/show_bug.cgi?id=21868
  Node* n = target_node;
  while (n && !n->GetLayoutObject())
    n = n->parentNode();

  if (n) {
    layer_location_.Scale(zoom_factor);
    if (LocalFrameView* view = n->GetLayoutObject()->View()->GetFrameView())
      layer_location_ = view->DocumentToFrame(layer_location_);

    PaintLayer* layer = n->GetLayoutObject()->EnclosingLayer();
    layer = layer->EnclosingSelfPaintingLayer();

    PhysicalOffset physical_offset =
        layer->GetLayoutObject().LocalToAbsolutePoint(PhysicalOffset(),
                                                      kIgnoreTransforms);
    layer_location_ -= gfx::Vector2dF(physical_offset);

    layer_location_.Scale(inverse_zoom_factor);
  }

  has_cached_relative_position_ = true;
}

void MouseEvent::RecordLayerXYMetrics() {
  Node* node = target() ? target()->ToNode() : nullptr;
  if (!node)
    return;
  // Using the target for these metrics is a heuristic for measuring the impact
  // of https://crrev.com/370604#c57. The heuristic will be accurate for canvas
  // elements which do not have children, but will undercount the impact on
  // child elements (e.g., descendants of frames).
  if (IsA<HTMLMediaElement>(node)) {
    UseCounter::Count(node->GetDocument(), WebFeature::kLayerXYWithMediaTarget);
  } else if (IsA<HTMLCanvasElement>(node)) {
    UseCounter::Count(node->GetDocument(),
                      WebFeature::kLayerXYWithCanvasTarget);
  } else if (IsA<HTMLFrameElementBase>(node)) {
    UseCounter::Count(node->GetDocument(), WebFeature::kLayerXYWithFrameTarget);
  } else if (IsA<SVGElement>(node)) {
    UseCounter::Count(node->GetDocument(), WebFeature::kLayerXYWithSVGTarget);
  }
}

int MouseEvent::layerX() {
  if (!has_cached_relative_position_)
    ComputeRelativePosition();

  RecordLayerXYMetrics();

  return ClampTo<int>(std::floor(layer_location_.x()));
}

int MouseEvent::layerY() {
  if (!has_cached_relative_position_)
    ComputeRelativePosition();

  RecordLayerXYMetrics();

  return ClampTo<int>(std::floor(layer_location_.y()));
}

double MouseEvent::offsetX() const {
  if (!HasPosition())
    return 0;
  if (!has_cached_relative_position_)
    const_cast<MouseEvent*>(this)->ComputeRelativePosition();
  return std::round(offset_x_);
}

double MouseEvent::offsetY() const {
  if (!HasPosition())
    return 0;
  if (!has_cached_relative_position_)
    const_cast<MouseEvent*>(this)->ComputeRelativePosition();
  return std::round(offset_y_);
}

}  // namespace blink
