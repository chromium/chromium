/*
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
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
 *
 */

#include "third_party/blink/renderer/core/html/html_meter_element.h"

#include "third_party/blink/renderer/core/display_lock/display_lock_utilities.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/html/html_div_element.h"
#include "third_party/blink/renderer/core/html/html_slot_element.h"
#include "third_party/blink/renderer/core/html/parser/html_parser_idioms.h"
#include "third_party/blink/renderer/core/html/shadow/shadow_element_names.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "ui/base/ui_base_features.h"

namespace blink {

HTMLMeterElement::HTMLMeterElement(Document& document)
    : HTMLElement(html_names::kMeterTag, document) {
  UseCounter::Count(document, WebFeature::kMeterElement);
  SetHasCustomStyleCallbacks();
  EnsureUserAgentShadowRoot();
}

HTMLMeterElement::~HTMLMeterElement() = default;

LayoutObject* HTMLMeterElement::CreateLayoutObject(const ComputedStyle& style) {
  switch (style.EffectiveAppearance()) {
    case kMeterPart:
      UseCounter::Count(GetDocument(),
                        WebFeature::kMeterElementWithMeterAppearance);
      break;
    case kNoControlPart:
      UseCounter::Count(GetDocument(),
                        WebFeature::kMeterElementWithNoneAppearance);
      break;
    default:
      break;
  }
  return HTMLElement::CreateLayoutObject(style);
}

void HTMLMeterElement::DidRecalcStyle(const StyleRecalcChange change) {
  HTMLElement::DidRecalcStyle(change);
  if (const ComputedStyle* style = GetComputedStyle()) {
    bool is_horizontal = style->IsHorizontalWritingMode();
    bool is_ltr = style->IsLeftToRightDirection();
    if (is_horizontal && is_ltr) {
      UseCounter::Count(GetDocument(), WebFeature::kMeterElementHorizontalLtr);
    } else if (is_horizontal && !is_ltr) {
      UseCounter::Count(GetDocument(), WebFeature::kMeterElementHorizontalRtl);
    } else if (is_ltr) {
      UseCounter::Count(GetDocument(), WebFeature::kMeterElementVerticalLtr);
    } else {
      UseCounter::Count(GetDocument(), WebFeature::kMeterElementVerticalRtl);
    }
  }
}

void HTMLMeterElement::ParseAttribute(
    const AttributeModificationParams& params) {
  const QualifiedName& name = params.name;
  if (name == html_names::kValueAttr || name == html_names::kMinAttr ||
      name == html_names::kMaxAttr || name == html_names::kLowAttr ||
      name == html_names::kHighAttr || name == html_names::kOptimumAttr)
    DidElementStateChange();
  else
    HTMLElement::ParseAttribute(params);
}

double HTMLMeterElement::value() const {
  double value = GetFloatingPointAttribute(html_names::kValueAttr, 0);
  return std::min(std::max(value, min()), max());
}

void HTMLMeterElement::setValue(double value) {
  SetFloatingPointAttribute(html_names::kValueAttr, value);
}

double HTMLMeterElement::min() const {
  return GetFloatingPointAttribute(html_names::kMinAttr, 0);
}

void HTMLMeterElement::setMin(double min) {
  SetFloatingPointAttribute(html_names::kMinAttr, min);
}

double HTMLMeterElement::max() const {
  return std::max(
      GetFloatingPointAttribute(html_names::kMaxAttr, std::max(1.0, min())),
      min());
}

void HTMLMeterElement::setMax(double max) {
  SetFloatingPointAttribute(html_names::kMaxAttr, max);
}

double HTMLMeterElement::low() const {
  double low = GetFloatingPointAttribute(html_names::kLowAttr, min());
  return std::min(std::max(low, min()), max());
}

void HTMLMeterElement::setLow(double low) {
  SetFloatingPointAttribute(html_names::kLowAttr, low);
}

double HTMLMeterElement::high() const {
  double high = GetFloatingPointAttribute(html_names::kHighAttr, max());
  return std::min(std::max(high, low()), max());
}

void HTMLMeterElement::setHigh(double high) {
  SetFloatingPointAttribute(html_names::kHighAttr, high);
}

double HTMLMeterElement::optimum() const {
  double optimum =
      GetFloatingPointAttribute(html_names::kOptimumAttr, (max() + min()) / 2);
  return std::min(std::max(optimum, min()), max());
}

void HTMLMeterElement::setOptimum(double optimum) {
  SetFloatingPointAttribute(html_names::kOptimumAttr, optimum);
}

HTMLMeterElement::GaugeRegion HTMLMeterElement::GetGaugeRegion() const {
  double low_value = low();
  double high_value = high();
  double the_value = value();
  double optimum_value = optimum();

  if (optimum_value < low_value) {
    // The optimum range stays under low
    if (the_value <= low_value)
      return kGaugeRegionOptimum;
    if (the_value <= high_value)
      return kGaugeRegionSuboptimal;
    return kGaugeRegionEvenLessGood;
  }

  if (high_value < optimum_value) {
    // The optimum range stays over high
    if (high_value <= the_value)
      return kGaugeRegionOptimum;
    if (low_value <= the_value)
      return kGaugeRegionSuboptimal;
    return kGaugeRegionEvenLessGood;
  }

  // The optimum range stays between high and low.
  // According to the standard, <meter> never show GaugeRegionEvenLessGood in
  // this case because the value is never less or greater than min or max.
  if (low_value <= the_value && the_value <= high_value)
    return kGaugeRegionOptimum;
  return kGaugeRegionSuboptimal;
}

double HTMLMeterElement::ValueRatio() const {
  double min = this->min();
  double max = this->max();
  double value = this->value();

  if (max <= min)
    return 0;
  return (value - min) / (max - min);
}

void HTMLMeterElement::DidElementStateChange() {
  UpdateValueAppearance(ValueRatio() * 100);
}

void HTMLMeterElement::DidAddUserAgentShadowRoot(ShadowRoot& root) {
  DCHECK(!value_);

  auto* inner = MakeGarbageCollected<HTMLDivElement>(GetDocument());
  inner->SetShadowPseudoId(shadow_element_names::kPseudoMeterInnerElement);
  root.AppendChild(inner);

  auto* bar = MakeGarbageCollected<HTMLDivElement>(GetDocument());
  bar->SetShadowPseudoId(AtomicString("-webkit-meter-bar"));

  value_ = MakeGarbageCollected<HTMLDivElement>(GetDocument());
  UpdateValueAppearance(0);
  bar->AppendChild(value_);

  inner->AppendChild(bar);

  if (!RuntimeEnabledFeatures::MeterAppearanceNoneFallbackStyleEnabled()) {
    auto* fallback = MakeGarbageCollected<HTMLDivElement>(GetDocument());
    fallback->AppendChild(MakeGarbageCollected<HTMLSlotElement>(GetDocument()));
    fallback->SetShadowPseudoId(AtomicString("-internal-fallback"));
    root.AppendChild(fallback);
  }
}

void HTMLMeterElement::UpdateValueAppearance(double percentage) {
  DEFINE_STATIC_LOCAL(AtomicString, optimum_pseudo_id,
                      ("-webkit-meter-optimum-value"));
  DEFINE_STATIC_LOCAL(AtomicString, suboptimum_pseudo_id,
                      ("-webkit-meter-suboptimum-value"));
  DEFINE_STATIC_LOCAL(AtomicString, even_less_good_pseudo_id,
                      ("-webkit-meter-even-less-good-value"));

  value_->SetInlineStyleProperty(CSSPropertyID::kInlineSize, percentage,
                                 CSSPrimitiveValue::UnitType::kPercentage);
  value_->SetInlineStyleProperty(CSSPropertyID::kBlockSize, 100,
                                 CSSPrimitiveValue::UnitType::kPercentage);
  switch (GetGaugeRegion()) {
    case kGaugeRegionOptimum:
      value_->SetShadowPseudoId(optimum_pseudo_id);
      break;
    case kGaugeRegionSuboptimal:
      value_->SetShadowPseudoId(suboptimum_pseudo_id);
      break;
    case kGaugeRegionEvenLessGood:
      value_->SetShadowPseudoId(even_less_good_pseudo_id);
      break;
  }
}

bool HTMLMeterElement::CanContainRangeEndPoint() const {
  if (DisplayLockUtilities::LockedAncestorPreventingPaint(*this)) {
    // If this element is DisplayLocked, then we can't access GetComputedStyle.
    // Even with GetComputedStyle's scoped unlock, this function may be called
    // during selection modification which prevents lifecycle updates that the
    // unlock would incur.
    return false;
  }
  return GetComputedStyle() && !GetComputedStyle()->HasEffectiveAppearance();
}

void HTMLMeterElement::AdjustStyle(ComputedStyleBuilder& builder) {
  // Descendants of the <meter> UA shadow host use
  // a -internal-shadow-host-has-non-auto-appearance selector which depends on
  // the computed value of the host's 'appearance'.
  // This information is propagated via StyleUAShadowHostData to ensure
  // invalidation of those descendants when the appearance changes.

  builder.SetUAShadowHostData(std::make_unique<StyleUAShadowHostData>(
      /* width */ Length(),
      /* height */ Length(),
      StyleAspectRatio(EAspectRatioType::kAuto, gfx::SizeF()),
      /* alt_text */ g_null_atom,
      /* alt_attr */ g_null_atom,
      /* src_attr */ g_null_atom, builder.HasEffectiveAppearance()));
}

void HTMLMeterElement::Trace(Visitor* visitor) const {
  visitor->Trace(value_);
  HTMLElement::Trace(visitor);
}

}  // namespace blink
