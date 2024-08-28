// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/events/pointer_event.h"

#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_pointer_event_init.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/events/event_dispatcher.h"
#include "third_party/blink/renderer/core/dom/events/event_path.h"
#include "third_party/blink/renderer/core/events/pointer_event_util.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"

namespace blink {

PointerEvent::PointerEvent(const AtomicString& type,
                           const PointerEventInit* initializer,
                           base::TimeTicks platform_time_stamp,
                           MouseEvent::SyntheticEventType synthetic_event_type,
                           WebMenuSourceType menu_source_type,
                           bool prevent_counting_as_interaction)
    : MouseEvent(type,
                 initializer,
                 platform_time_stamp,
                 synthetic_event_type,
                 menu_source_type),
      pointer_id_(0),
      width_(0),
      height_(0),
      pressure_(0),
      tilt_x_(0),
      tilt_y_(0),
      azimuth_angle_(0),
      altitude_angle_(kPiDouble / 2),
      tangential_pressure_(0),
      twist_(0),
      is_primary_(false),
      coalesced_events_targets_dirty_(false),
      predicted_events_targets_dirty_(false),
      persistent_device_id_(0),
      prevent_counting_as_interaction_(prevent_counting_as_interaction) {
  if (initializer->hasPointerId())
    pointer_id_ = initializer->pointerId();
  if (initializer->hasWidth())
    width_ = initializer->width();
  if (initializer->hasHeight())
    height_ = initializer->height();
  if (initializer->hasPressure())
    pressure_ = initializer->pressure();
  if (initializer->hasTiltX())
    tilt_x_ = initializer->tiltX();
  if (initializer->hasTiltY())
    tilt_y_ = initializer->tiltY();
  if (initializer->hasTangentialPressure())
    tangential_pressure_ = initializer->tangentialPressure();
  if (initializer->hasTwist())
    twist_ = initializer->twist();
  if (initializer->hasPointerType())
    pointer_type_ = initializer->pointerType();
  if (initializer->hasIsPrimary())
    is_primary_ = initializer->isPrimary();
  if (initializer->hasCoalescedEvents()) {
    for (auto coalesced_event : initializer->coalescedEvents())
      coalesced_events_.push_back(coalesced_event);
  }
  if (initializer->hasPredictedEvents()) {
    for (auto predicted_event : initializer->predictedEvents())
      predicted_events_.push_back(predicted_event);
  }
  if (initializer->hasAzimuthAngle())
    azimuth_angle_ = initializer->azimuthAngle();
  if (initializer->hasAltitudeAngle())
    altitude_angle_ = initializer->altitudeAngle();
  if ((initializer->hasTiltX() || initializer->hasTiltY()) &&
      !initializer->hasAzimuthAngle() && !initializer->hasAltitudeAngle()) {
    azimuth_angle_ = PointerEventUtil::AzimuthFromTilt(
        PointerEventUtil::TransformToTiltInValidRange(tilt_x_),
        PointerEventUtil::TransformToTiltInValidRange(tilt_y_));
    altitude_angle_ = PointerEventUtil::AltitudeFromTilt(
        PointerEventUtil::TransformToTiltInValidRange(tilt_x_),
        PointerEventUtil::TransformToTiltInValidRange(tilt_y_));
  }
  if ((initializer->hasAzimuthAngle() || initializer->hasAltitudeAngle()) &&
      !initializer->hasTiltX() && !initializer->hasTiltY()) {
    tilt_x_ = PointerEventUtil::TiltXFromSpherical(
        PointerEventUtil::TransformToAzimuthInValidRange(azimuth_angle_),
        PointerEventUtil::TransformToAltitudeInValidRange(altitude_angle_));
    tilt_y_ = PointerEventUtil::TiltYFromSpherical(
        PointerEventUtil::TransformToAzimuthInValidRange(azimuth_angle_),
        PointerEventUtil::TransformToAltitudeInValidRange(altitude_angle_));
  }
  if (initializer->hasPersistentDeviceId()) {
    persistent_device_id_ = initializer->persistentDeviceId();
  }
}

bool PointerEvent::IsMouseEvent() const {
  if (type() == event_type_names::kClick ||
      type() == event_type_names::kAuxclick ||
      type() == event_type_names::kContextmenu) {
    return true;
  }

  return false;
}

bool PointerEvent::ShouldHaveIntegerCoordinates() const {
  if (type() == event_type_names::kClick ||
      type() == event_type_names::kContextmenu ||
      type() == event_type_names::kAuxclick) {
    return true;
  }
  return false;
}

bool PointerEvent::IsPointerEvent() const {
  return true;
}

double PointerEvent::offsetX() const {
  if (ShouldHaveIntegerCoordinates())
    return MouseEvent::offsetX();
  if (!HasPosition())
    return 0;
  if (!has_cached_relative_position_)
    const_cast<PointerEvent*>(this)->ComputeRelativePosition();
  return offset_x_;
}

double PointerEvent::offsetY() const {
  if (ShouldHaveIntegerCoordinates())
    return MouseEvent::offsetY();
  if (!HasPosition())
    return 0;
  if (!has_cached_relative_position_)
    const_cast<PointerEvent*>(this)->ComputeRelativePosition();
  return offset_y_;
}

void PointerEvent::ReceivedTarget() {
  if (!RuntimeEnabledFeatures::PointerEventTargetsInEventListsEnabled()) {
    coalesced_events_targets_dirty_ = true;
    predicted_events_targets_dirty_ = true;
  }
  MouseEvent::ReceivedTarget();
}

Node* PointerEvent::toElement() const {
  return nullptr;
}

Node* PointerEvent::fromElement() const {
  return nullptr;
}

HeapVector<Member<PointerEvent>> PointerEvent::getCoalescedEvents() {
  if (auto* local_dom_window = DynamicTo<LocalDOMWindow>(view())) {
    auto* document = local_dom_window->document();
    if (document && !local_dom_window->isSecureContext()) {
      UseCounter::Count(document,
                        WebFeature::kGetCoalescedEventsInInsecureContext);
    }
  }

  if (coalesced_events_targets_dirty_) {
    CHECK(!RuntimeEnabledFeatures::PointerEventTargetsInEventListsEnabled());
    for (auto coalesced_event : coalesced_events_)
      coalesced_event->SetTarget(target());
    coalesced_events_targets_dirty_ = false;
  }
  return coalesced_events_;
}

HeapVector<Member<PointerEvent>> PointerEvent::getPredictedEvents() {
  if (predicted_events_targets_dirty_) {
    CHECK(!RuntimeEnabledFeatures::PointerEventTargetsInEventListsEnabled());
    for (auto predicted_event : predicted_events_)
      predicted_event->SetTarget(target());
    predicted_events_targets_dirty_ = false;
  }
  return predicted_events_;
}

base::TimeTicks PointerEvent::OldestPlatformTimeStamp() const {
  if (coalesced_events_.size() > 0) {
    // Assume that time stamps of coalesced events are in ascending order.
    return coalesced_events_[0]->PlatformTimeStamp();
  }
  return PlatformTimeStamp();
}

void PointerEvent::Trace(Visitor* visitor) const {
  visitor->Trace(coalesced_events_);
  visitor->Trace(predicted_events_);
  MouseEvent::Trace(visitor);
}

DispatchEventResult PointerEvent::DispatchEvent(EventDispatcher& dispatcher) {
  if (type().empty())
    return DispatchEventResult::kNotCanceled;  // Shouldn't happen.

  if (isTrusted() &&
      RuntimeEnabledFeatures::PointerEventTargetsInEventListsEnabled()) {
    // TODO(mustaq@chromium.org): When the RTE flag is removed, get rid of
    // `coalesced_events_targets_dirty_` and `predicted_events_targets_dirty_`.

    for (auto coalesced_event : coalesced_events_) {
      coalesced_event->SetTarget(&dispatcher.GetNode());
    }
    for (auto predicted_event : predicted_events_) {
      predicted_event->SetTarget(&dispatcher.GetNode());
    }
  }

  if (type() == event_type_names::kClick) {
    return MouseEvent::DispatchEvent(dispatcher);
  }

  DCHECK(!target() || target() != relatedTarget());

  GetEventPath().AdjustForRelatedTarget(dispatcher.GetNode(), relatedTarget());

  return dispatcher.Dispatch();
}

PointerId PointerEvent::pointerIdForBindings() const {
  if (auto* document = GetDocument())
    UseCounter::Count(document, WebFeature::kPointerId);
  return pointerId();
}

Document* PointerEvent::GetDocument() const {
  if (auto* local_dom_window = DynamicTo<LocalDOMWindow>(view()))
    return local_dom_window->document();
  return nullptr;
}

}  // namespace blink
