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

#include "third_party/blink/public/common/input/web_pointer_properties.h"
#include "third_party/blink/public/strings/grit/blink_strings.h"
#include "third_party/blink/renderer/core/accessibility/ax_object_cache.h"
#include "third_party/blink/renderer/core/dom/events/scoped_event_queue.h"
#include "third_party/blink/renderer/core/dom/events/simulated_click_options.h"
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
#include "third_party/blink/renderer/core/html/forms/slider_track_element.h"
#include "third_party/blink/renderer/core/html/forms/step_range.h"
#include "third_party/blink/renderer/core/html/parser/html_parser_idioms.h"
#include "third_party/blink/renderer/core/html/shadow/shadow_element_names.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/input_type_names.h"
#include "third_party/blink/renderer/core/layout/flex/layout_flexible_box.h"
#include "third_party/blink/renderer/core/layout/layout_block.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/text/platform_locale.h"
#include "third_party/blink/renderer/platform/text/text_direction.h"
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
    : InputType(Type::kRange, element),
      InputTypeView(element),
      tick_mark_values_dirty_(true) {}

void RangeInputType::Trace(Visitor* visitor) const {
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
}

void RangeInputType::DidRecalcStyle(const StyleRecalcChange) {
  if (const ComputedStyle* style = GetElement().GetComputedStyle()) {
    if (RuntimeEnabledFeatures::
            NonStandardAppearanceValueSliderVerticalEnabled() &&
        style->EffectiveAppearance() == kSliderVerticalPart) {
      UseCounter::Count(GetElement().GetDocument(),
                        WebFeature::kInputTypeRangeVerticalAppearance);
    } else {
      bool is_horizontal = style->IsHorizontalWritingMode();
      bool is_ltr = style->IsLeftToRightDirection();
      if (is_horizontal && is_ltr) {
        UseCounter::Count(GetElement().GetDocument(),
                          WebFeature::kInputTypeRangeHorizontalLtr);
      } else if (is_horizontal && !is_ltr) {
        UseCounter::Count(GetElement().GetDocument(),
                          WebFeature::kInputTypeRangeHorizontalRtl);
      } else if (is_ltr) {
        UseCounter::Count(GetElement().GetDocument(),
                          WebFeature::kInputTypeRangeVerticalLtr);
      } else {
        UseCounter::Count(GetElement().GetDocument(),
                          WebFeature::kInputTypeRangeVerticalRtl);
      }
    }
  }
}

double RangeInputType::ValueAsDouble() const {
  return ParseToDoubleForNumberType(GetElement().Value());
}

void RangeInputType::SetValueAsDouble(double new_value,
                                      TextFieldEventBehavior event_behavior,
                                      ExceptionState& exception_state) const {
  SetValueAsDecimal(Decimal::FromDouble(new_value), event_behavior,
                    exception_state);
}

bool RangeInputType::TypeMismatchFor(const String& value) const {
  return !value.empty() && !std::isfinite(ParseToDoubleForNumberType(value));
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
  return StepRange(step_base, minimum, maximum, kHasRangeLimitations,
                   /*has_reversed_range=*/false, step, step_description);
}

void RangeInputType::HandleMouseDownEvent(MouseEvent& event) {
  if (!HasCreatedShadowSubtree()) {
    return;
  }

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
  thumb->DragFrom(PhysicalOffset::FromPointFFloor(event.AbsoluteLocation()));
}

void RangeInputType::HandleKeydownEvent(KeyboardEvent& event) {
  if (GetElement().IsDisabledFormControl())
    return;

  const AtomicString key(event.key());

  const Decimal current = ParseToNumberOrNaN(GetElement().Value());
  DCHECK(current.IsFinite());

  StepRange step_range(CreateStepRange(kRejectAny));

  // FIXME: We can't use stepUp() for the step value "any". So, we increase
  // or decrease the value by 1/100 of the value range. Is it reasonable?
  const Decimal step =
      EqualIgnoringASCIICase(
          GetElement().FastGetAttribute(html_names::kStepAttr), "any")
          ? (step_range.Maximum() - step_range.Minimum()) / 100
          : step_range.Step();
  const Decimal big_step =
      std::max((step_range.Maximum() - step_range.Minimum()) / 10, step);

  bool is_up = false;
  bool is_down = false;
  if (RuntimeEnabledFeatures::VerticalInputRangeKeyOperationFixEnabled()) {
    WritingDirectionMode writing_direction = {WritingMode::kHorizontalTb,
                                              TextDirection::kLtr};
    if (const auto* style = GetElement().GetComputedStyle()) {
      writing_direction = style->GetWritingDirection();
      // `appearance: slider-vertical` is equivalent to `writing-mode:
      // vertical-rl; direction: rtl`.
      if (RuntimeEnabledFeatures::
              NonStandardAppearanceValueSliderVerticalEnabled() &&
          writing_direction.IsHorizontal() &&
          style->EffectiveAppearance() == kSliderVerticalPart) {
        writing_direction = {WritingMode::kVerticalRl, TextDirection::kRtl};
      }
    }
    const PhysicalToLogical<const AtomicString*> key_mapper(
        writing_direction, &keywords::kArrowUp, &keywords::kArrowRight,
        &keywords::kArrowDown, &keywords::kArrowLeft);
    is_up = key == *key_mapper.InlineEnd() || key == *key_mapper.LineOver();
    is_down =
        key == *key_mapper.InlineStart() || key == *key_mapper.LineUnder();
  } else {
    TextDirection dir = TextDirection::kLtr;
    if (GetElement().GetLayoutObject()) {
      dir = ComputedTextDirection();
    }
    if (key == keywords::kArrowUp) {
      is_up = true;
    } else if (key == keywords::kArrowDown) {
      is_down = true;
    } else if (key == keywords::kArrowLeft) {
      if (dir == TextDirection::kRtl) {
        is_up = true;
      } else {
        is_down = true;
      }
    } else if (key == keywords::kArrowRight) {
      if (dir == TextDirection::kRtl) {
        is_down = true;
      } else {
        is_up = true;
      }
    }
  }

  Decimal new_value;
  if (is_up) {
    new_value = current + step;
  } else if (is_down) {
    new_value = current - step;
  } else if (key == keywords::kPageUp) {
    new_value = current + big_step;
  } else if (key == keywords::kPageDown) {
    new_value = current - big_step;
  } else if (key == keywords::kHome) {
    new_value = step_range.Minimum();
  } else if (key == keywords::kEnd) {
    new_value = step_range.Maximum();
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
  auto* track = MakeGarbageCollected<blink::SliderTrackElement>(document);
  track->SetShadowPseudoId(shadow_element_names::kPseudoSliderTrack);
  track->setAttribute(html_names::kIdAttr,
                      shadow_element_names::kIdSliderTrack);
  track->AppendChild(MakeGarbageCollected<SliderThumbElement>(document));
  auto* container = MakeGarbageCollected<SliderContainerElement>(document);
  container->AppendChild(track);
  GetElement().UserAgentShadowRoot()->AppendChild(container);
}

LayoutObject* RangeInputType::CreateLayoutObject(const ComputedStyle&) const {
  // TODO(crbug.com/1131352): input[type=range] should not use flexbox.
  return MakeGarbageCollected<LayoutFlexibleBox>(&GetElement());
}

void RangeInputType::AdjustStyle(ComputedStyleBuilder& builder) {
  builder.SetInlineBlockBaselineEdge(EInlineBlockBaselineEdge::kBorderBox);
  InputTypeView::AdjustStyle(builder);
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
void RangeInputType::AccessKeyAction(
    SimulatedClickCreationScope creation_scope) {
  InputTypeView::AccessKeyAction(creation_scope);
  GetElement().DispatchSimulatedClick(nullptr, creation_scope);
}

void RangeInputType::SanitizeValueInResponseToMinOrMaxAttributeChange() {
  if (GetElement().HasDirtyValue())
    GetElement().SetValue(GetElement().Value());
  else
    GetElement().SetNonDirtyValue(GetElement().Value());
  GetElement().UpdateView();
}

void RangeInputType::StepAttributeChanged() {
  if (GetElement().HasDirtyValue())
    GetElement().SetValue(GetElement().Value());
  else
    GetElement().SetNonDirtyValue(GetElement().Value());
  GetElement().UpdateView();
}

void RangeInputType::DidSetValue(const String&, bool value_changed) {
  if (value_changed)
    GetElement().UpdateView();
}

ControlPart RangeInputType::AutoAppearance() const {
  return kSliderHorizontalPart;
}

void RangeInputType::UpdateView() {
  if (HasCreatedShadowSubtree()) {
    GetSliderThumbElement()->SetPositionFromValue();
  }
}

String RangeInputType::SanitizeValue(const String& proposed_value) const {
  StepRange step_range(CreateStepRange(kRejectAny));
  const Decimal proposed_numeric_value =
      ParseToNumber(proposed_value, step_range.DefaultValue());
  return SerializeForNumberType(step_range.ClampValue(proposed_numeric_value));
}

void RangeInputType::WarnIfValueIsInvalid(const String& value) const {
  if (value.empty() || !GetElement().SanitizeValue(value).empty())
    return;
  AddWarningToConsole(
      "The specified value %s cannot be parsed, or is out of range.", value);
}

String RangeInputType::RangeOverflowText(const Decimal& maximum) const {
  return GetLocale().QueryString(IDS_FORM_VALIDATION_RANGE_OVERFLOW,
                                 LocalizeValue(Serialize(maximum)));
}

String RangeInputType::RangeUnderflowText(const Decimal& minimum) const {
  return GetLocale().QueryString(IDS_FORM_VALIDATION_RANGE_UNDERFLOW,
                                 LocalizeValue(Serialize(minimum)));
}

String RangeInputType::RangeInvalidText(const Decimal& minimum,
                                        const Decimal& maximum) const {
  return GetLocale().QueryString(IDS_FORM_VALIDATION_RANGE_REVERSED,
                                 LocalizeValue(Serialize(minimum)),
                                 LocalizeValue(Serialize(maximum)));
}

void RangeInputType::DisabledAttributeChanged() {
  if (!HasCreatedShadowSubtree()) {
    return;
  }
  if (GetElement().IsDisabledFormControl())
    GetSliderThumbElement()->StopDragging();
}

bool RangeInputType::ShouldRespectListAttribute() {
  return true;
}

inline SliderThumbElement* RangeInputType::GetSliderThumbElement() const {
  return To<SliderThumbElement>(
      GetElement().UserAgentShadowRoot()->getElementById(
          shadow_element_names::kIdSliderThumb));
}

inline Element* RangeInputType::SliderTrackElement() const {
  if (!HasCreatedShadowSubtree()) {
    return nullptr;
  }

  return GetElement().UserAgentShadowRoot()->getElementById(
      shadow_element_names::kIdSliderTrack);
}

void RangeInputType::ListAttributeTargetChanged() {
  tick_mark_values_dirty_ = true;
  if (auto* object = GetElement().GetLayoutObject())
    object->SetSubtreeShouldDoFullPaintInvalidation();
  Element* slider_track_element = SliderTrackElement();
  if (slider_track_element && slider_track_element->GetLayoutObject()) {
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
  tick_mark_values_.reserve(options->length());
  for (unsigned i = 0; i < options->length(); ++i) {
    HTMLOptionElement* option_element = options->Item(i);
    String option_value = option_element->value();
    if (option_element->IsDisabledFormControl() || option_value.empty())
      continue;
    if (!GetElement().IsValidValue(option_value))
      continue;
    tick_mark_values_.push_back(ParseToNumber(option_value, Decimal::Nan()));
  }
  tick_mark_values_.shrink_to_fit();
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

bool RangeInputType::IsDraggedSlider() const {
  return GetSliderThumbElement()->IsActive();
}

}  // namespace blink
