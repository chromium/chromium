/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2011 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/html/forms/range_input_type.h"

#include <algorithm>
#include <limits>

#include "third_party/blink/renderer/core/accessibility/ax_object_cache.h"
#include "third_party/blink/renderer/core/dom/events/scoped_event_queue.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/events/keyboard_event.h"
#include "third_party/blink/renderer/core/events/mouse_event.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/html/forms/html_data_list_element.h"
#include "third_party/blink/renderer/core/html/forms/html_data_list_options_collection.h"
#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
#include "third_party/blink/renderer/core/html/forms/html_option_element.h"
#include "third_party/blink/renderer/core/html/forms/slider_thumb_element.h"
#include "third_party/blink/renderer/core/html/forms/step_range.h"
#include "third_party/blink/renderer/core/html/html_div_element.h"
#include "third_party/blink/renderer/core/html/parser/html_parser_idioms.h"
#include "third_party/blink/renderer/core/html/shadow/shadow_element_names.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/input_type_names.h"
#include "third_party/blink/renderer/core/layout/layout_slider.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"

namespace blink {

static const int kRangeDefaultMinimum = 0;
static const int kRangeDefaultMaximum = 100;
static const int kRangeDefaultStep = 1;
static const int kRangeDefaultStepBase = 0;
static const int kRangeStepScaleFactor = 1;

static Decimal EnsureMaximum(const Decimal& proposed_value,
                             const Decimal& minimum) {
  return proposed_value >= minimum ? proposed_value : minimum;
}

RangeInputType::RangeInputType(HTMLInputElement& element)
    : InputType(element),
      InputTypeView(element),
      tick_mark_values_dirty_(true) {}

void RangeInputType::Trace(Visitor* visitor) {
  InputTypeView::Trace(visitor);
  InputType::Trace(visitor);
}

InputTypeView* RangeInputType::CreateView() {
  return this;
}

InputType::ValueMode RangeInputType::GetValueMode() const {
  return ValueMode::kValue;
}

void RangeInputType::CountUsage() {
  CountUsageIfVisible(WebFeature::kInputTypeRange);
  if (const ComputedStyle* style = GetElement().GetComputedStyle()) {
    if (style->EffectiveAppearance() == kSliderVerticalPart) {
      UseCounter::Count(GetElement().GetDocument(),
                        WebFeature::kInputTypeRangeVerticalAppearance);
    }
  }
}

const AtomicString& RangeInputType::FormControlType() const {
  return input_type_names::kRange;
}

double RangeInputType::ValueAsDouble() const {
  return ParseToDoubleForNumberType(GetElement().value());
}

void RangeInputType::SetValueAsDouble(double new_value,
                                      TextFieldEventBehavior event_behavior,
                                      ExceptionState& exception_state) const {
  SetValueAsDecimal(Decimal::FromDouble(new_value), event_behavior,
                    exception_state);
}

bool RangeInputType::TypeMismatchFor(const String& value) const {
  return !value.IsEmpty() && !std::isfinite(ParseToDoubleForNumberType(value));
}

bool RangeInputType::SupportsRequired() const {
  return false;
}

StepRange RangeInputType::CreateStepRange(
    AnyStepHandling any_step_handling) const {
  DEFINE_STATIC_LOCAL(
      const StepRange::StepDescription, step_description,
      (kRangeDefaultStep, kRangeDefaultStepBase, kRangeStepScaleFactor));

  const Decimal step_base = FindStepBase(kRangeDefaultStepBase);
  const Decimal minimum =
      ParseToNumber(GetElement().FastGetAttribute(html_names::kMinAttr),
                    kRangeDefaultMinimum);
  const Decimal maximum = EnsureMaximum(
      ParseToNumber(GetElement().FastGetAttribute(html_names::kMaxAttr),
                    kRangeDefaultMaximum),
      minimum);

  const Decimal step = StepRange::ParseStep(
      any_step_handling, step_description,
      GetElement().FastGetAttribute(html_names::kStepAttr));
  // Range type always has range limitations because it has default
  // minimum/maximum.
  // https://html.spec.whatwg.org/C/#range-state-(type=range):concept-input-min-default
  const bool kHasRangeLimitations = true;
  return StepRange(step_base, minimum, maximum, kHasRangeLimitations, step,
                   step_description);
}

bool RangeInputType::IsSteppable() const {
  return true;
}

void RangeInputType::HandleMouseDownEvent(MouseEvent& event) {
  if (GetElement().IsDisabledFormControl())
    return;

  Node* target_node = event.target()->ToNode();
  if (event.button() !=
          static_cast<int16_t>(WebPointerProperties::Button::kLeft) ||
      !target_node)
    return;
  DCHECK(IsShadowHost(GetElement()));
  if (target_node != GetElement() &&
      !target_node->IsDescendantOf(GetElement().UserAgentShadowRoot()))
    return;
  SliderThumbElement* thumb = GetSliderThumbElement();
  if (target_node == thumb)
    return;
  thumb->DragFrom(LayoutPoint(event.AbsoluteLocation()));
}

void RangeInputType::HandleKeydownEvent(KeyboardEvent& event) {
  if (GetElement().IsDisabledFormControl())
    return;

  const String& key = event.key();

  const Decimal current = ParseToNumberOrNaN(GetElement().value());
  DCHECK(current.IsFinite());

  StepRange step_range(CreateStepRange(kRejectAny));

  // FIXME: We can't use stepUp() for the step value "any". So, we increase
  // or decrease the value by 1/100 of the value range. Is it reasonable?
  const Decimal step =
      DeprecatedEqualIgnoringCase(
          GetElement().FastGetAttribute(html_names::kStepAttr), "any")
          ? (step_range.Maximum() - step_range.Minimum()) / 100
          : step_range.Step();
  const Decimal big_step =
      std::max((step_range.Maximum() - step_range.Minimum()) / 10, step);

  TextDirection dir = TextDirection::kLtr;
  bool is_vertical = false;
  if (GetElement().GetLayoutObject()) {
    dir = ComputedTextDirection();
    ControlPart part =
        GetElement().GetLayoutObject()->Style()->EffectiveAppearance();
    is_vertical = part == kSliderVerticalPart;
  }

  Decimal new_value;
  if (key == "ArrowUp") {
    new_value = current + step;
  } else if (key == "ArrowDown") {
    new_value = current - step;
  } else if (key == "ArrowLeft") {
    new_value = (is_vertical || dir == TextDirection::kRtl) ? current + step
                                                            : current - step;
  } else if (key == "ArrowRight") {
    new_value = (is_vertical || dir == TextDirection::kRtl) ? current - step
                                                            : current + step;
  } else if (key == "PageUp") {
    new_value = current + big_step;
  } else if (key == "PageDown") {
    new_value = current - big_step;
  } else if (key == "Home") {
    new_value = is_vertical ? step_range.Maximum() : step_range.Minimum();
  } else if (key == "End") {
    new_value = is_vertical ? step_range.Minimum() : step_range.Maximum();
  } else {
    return;  // Did not match any key binding.
  }

  new_value = step_range.ClampValue(new_value);

  if (new_value != current) {
    EventQueueScope scope;
    TextFieldEventBehavior event_behavior =
        TextFieldEventBehavior::kDispatchInputAndChangeEvent;
    SetValueAsDecimal(new_value, event_behavior, IGNORE_EXCEPTION_FOR_TESTING);

    if (AXObjectCache* cache =
            GetElement().GetDocument().ExistingAXObjectCache())
      cache->HandleValueChanged(&GetElement());
  }

  event.SetDefaultHandled();
}

void RangeInputType::CreateShadowSubtree() {
  DCHECK(IsShadowHost(GetElement()));

  Document& document = GetElement().GetDocument();
  auto* track = MakeGarbageCollected<HTMLDivElement>(document);
  track->SetShadowPseudoId(AtomicString("-webkit-slider-runnable-track"));
  track->setAttribute(html_names::kIdAttr, shadow_element_names::SliderTrack());
  track->AppendChild(MakeGarbageCollected<SliderThumbElement>(document));
  auto* container = MakeGarbageCollected<SliderContainerElement>(document);
  container->AppendChild(track);
  GetElement().UserAgentShadowRoot()->AppendChild(container);
}

LayoutObject* RangeInputType::CreateLayoutObject(const ComputedStyle&,
                                                 LegacyLayout) const {
  return new LayoutSlider(&GetElement());
}

Decimal RangeInputType::ParseToNumber(const String& src,
                                      const Decimal& default_value) const {
  return ParseToDecimalForNumberType(src, default_value);
}

String RangeInputType::Serialize(const Decimal& value) const {
  if (!value.IsFinite())
    return String();
  return SerializeForNumberType(value);
}

// FIXME: Could share this with KeyboardClickableInputTypeView and
// BaseCheckableInputType if we had a common base class.
void RangeInputType::AccessKeyAction(bool send_mouse_events) {
  InputTypeView::AccessKeyAction(send_mouse_events);

  GetElement().DispatchSimulatedClick(
      nullptr, send_mouse_events ? kSendMouseUpDownEvents : kSendNoEvents);
}

void RangeInputType::SanitizeValueInResponseToMinOrMaxAttributeChange() {
  if (GetElement().HasDirtyValue())
    GetElement().setValue(GetElement().value());
  else
    GetElement().SetNonDirtyValue(GetElement().value());
  GetElement().UpdateView();
}

void RangeInputType::StepAttributeChanged() {
  if (GetElement().HasDirtyValue())
    GetElement().setValue(GetElement().value());
  else
    GetElement().SetNonDirtyValue(GetElement().value());
  GetElement().UpdateView();
}

void RangeInputType::DidSetValue(const String&, bool value_changed) {
  if (value_changed)
    GetElement().UpdateView();
}

void RangeInputType::UpdateView() {
  GetSliderThumbElement()->SetPositionFromValue();
}

String RangeInputType::SanitizeValue(const String& proposed_value) const {
  StepRange step_range(CreateStepRange(kRejectAny));
  const Decimal proposed_numeric_value =
      ParseToNumber(proposed_value, step_range.DefaultValue());
  return SerializeForNumberType(step_range.ClampValue(proposed_numeric_value));
}

void RangeInputType::WarnIfValueIsInvalid(const String& value) const {
  if (value.IsEmpty() || !GetElement().SanitizeValue(value).IsEmpty())
    return;
  AddWarningToConsole(
      "The specified value %s is not a valid number. The value must match to "
      "the following regular expression: "
      "-?(\\d+|\\d+\\.\\d+|\\.\\d+)([eE][-+]?\\d+)?",
      value);
}

void RangeInputType::DisabledAttributeChanged() {
  if (GetElement().IsDisabledFormControl())
    GetSliderThumbElement()->StopDragging();
}

bool RangeInputType::ShouldRespectListAttribute() {
  return true;
}

inline SliderThumbElement* RangeInputType::GetSliderThumbElement() const {
  return To<SliderThumbElement>(
      GetElement().UserAgentShadowRoot()->getElementById(
          shadow_element_names::SliderThumb()));
}

inline Element* RangeInputType::SliderTrackElement() const {
  return GetElement().UserAgentShadowRoot()->getElementById(
      shadow_element_names::SliderTrack());
}

void RangeInputType::ListAttributeTargetChanged() {
  tick_mark_values_dirty_ = true;
  if (auto* object = GetElement().GetLayoutObject())
    object->SetSubtreeShouldDoFullPaintInvalidation();
  Element* slider_track_element = SliderTrackElement();
  if (slider_track_element->GetLayoutObject()) {
    slider_track_element->GetLayoutObject()->SetNeedsLayout(
        layout_invalidation_reason::kAttributeChanged);
  }
}

static bool DecimalCompare(const Decimal& a, const Decimal& b) {
  return a < b;
}

void RangeInputType::UpdateTickMarkValues() {
  if (!tick_mark_values_dirty_)
    return;
  tick_mark_values_.clear();
  tick_mark_values_dirty_ = false;
  HTMLDataListElement* data_list = GetElement().DataList();
  if (!data_list)
    return;
  HTMLDataListOptionsCollection* options = data_list->options();
  tick_mark_values_.ReserveCapacity(options->length());
  for (unsigned i = 0; i < options->length(); ++i) {
    HTMLOptionElement* option_element = options->Item(i);
    String option_value = option_element->value();
    if (option_element->IsDisabledFormControl() || option_value.IsEmpty())
      continue;
    if (!GetElement().IsValidValue(option_value))
      continue;
    tick_mark_values_.push_back(ParseToNumber(option_value, Decimal::Nan()));
  }
  tick_mark_values_.ShrinkToFit();
  std::sort(tick_mark_values_.begin(), tick_mark_values_.end(), DecimalCompare);
}

Decimal RangeInputType::FindClosestTickMarkValue(const Decimal& value) {
  UpdateTickMarkValues();
  if (!tick_mark_values_.size())
    return Decimal::Nan();

  wtf_size_t left = 0;
  wtf_size_t right = tick_mark_values_.size();
  wtf_size_t middle;
  while (true) {
    DCHECK_LE(left, right);
    middle = left + (right - left) / 2;
    if (!middle)
      break;
    if (middle == tick_mark_values_.size() - 1 &&
        tick_mark_values_[middle] < value) {
      middle++;
      break;
    }
    if (tick_mark_values_[middle - 1] <= value &&
        tick_mark_values_[middle] >= value)
      break;

    if (tick_mark_values_[middle] < value)
      left = middle;
    else
      right = middle;
  }
  const Decimal closest_left = middle ? tick_mark_values_[middle - 1]
                                      : Decimal::Infinity(Decimal::kNegative);
  const Decimal closest_right = middle != tick_mark_values_.size()
                                    ? tick_mark_values_[middle]
                                    : Decimal::Infinity(Decimal::kPositive);
  if (closest_right - value < value - closest_left)
    return closest_right;
  return closest_left;
}

void RangeInputType::ValueAttributeChanged() {
  UpdateView();
}

}  // namespace blink
