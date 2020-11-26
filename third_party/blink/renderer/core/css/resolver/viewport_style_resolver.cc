/*
 * Copyright (C) 2012-2013 Intel Corporation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/css/resolver/viewport_style_resolver.h"

#include "third_party/blink/renderer/core/css/css_default_style_sheets.h"
#include "third_party/blink/renderer/core/css/css_numeric_literal_value.h"
#include "third_party/blink/renderer/core/css/css_primitive_value_mappings.h"
#include "third_party/blink/renderer/core/css/css_property_value_set.h"
#include "third_party/blink/renderer/core/css/css_style_sheet.h"
#include "third_party/blink/renderer/core/css/css_to_length_conversion_data.h"
#include "third_party/blink/renderer/core/css/document_style_sheet_collection.h"
#include "third_party/blink/renderer/core/css/media_values_initial_viewport.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/css/style_rule.h"
#include "third_party/blink/renderer/core/css/style_rule_import.h"
#include "third_party/blink/renderer/core/css/style_sheet_contents.h"
#include "third_party/blink/renderer/core/css_value_keywords.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/frame/viewport_data.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/page/viewport_description.h"

namespace blink {

namespace {

bool HasViewportFitProperty(const CSSPropertyValueSet* property_set) {
  DCHECK(property_set);
  return RuntimeEnabledFeatures::DisplayCutoutAPIEnabled() &&
         property_set->HasProperty(CSSPropertyID::kViewportFit);
}

}  // namespace

ViewportStyleResolver::ViewportStyleResolver(Document& document)
    : document_(document) {
  DCHECK(document.GetFrame());
  initial_viewport_medium_ = MakeGarbageCollected<MediaQueryEvaluator>(
      MakeGarbageCollected<MediaValuesInitialViewport>(*document.GetFrame()));
}

void ViewportStyleResolver::Reset() {
  property_set_ = nullptr;
  has_viewport_units_ = false;
  DCHECK(initial_style_);
  initial_style_->SetHasViewportUnits(false);
  needs_update_ = kNoUpdate;
}

void ViewportStyleResolver::CollectViewportRulesFromUASheets() {
  CSSDefaultStyleSheets& default_style_sheets =
      CSSDefaultStyleSheets::Instance();
  mojom::blink::ViewportStyle viewport_style =
      document_->GetSettings() ? document_->GetSettings()->GetViewportStyle()
                               : mojom::blink::ViewportStyle::kDefault;
  StyleSheetContents* viewport_contents = nullptr;
  switch (viewport_style) {
    case mojom::blink::ViewportStyle::kDefault:
      break;
    case mojom::blink::ViewportStyle::kMobile:
      viewport_contents = default_style_sheets.EnsureMobileViewportStyleSheet();
      break;
    case mojom::blink::ViewportStyle::kTelevision:
      viewport_contents =
          default_style_sheets.EnsureTelevisionViewportStyleSheet();
      break;
  }
  if (viewport_contents)
    CollectViewportRules(viewport_contents->ChildRules());

  if (document_->IsMobileDocument()) {
    CollectViewportRules(
        default_style_sheets.EnsureXHTMLMobileProfileStyleSheet()
            ->ChildRules());
  }
  DCHECK(!default_style_sheets.DefaultStyleSheet()->HasViewportRule());
}

void ViewportStyleResolver::CollectViewportRules(
    const HeapVector<Member<StyleRuleBase>>& rules) {
  for (auto& rule : rules) {
    if (auto* viewport_rule = DynamicTo<StyleRuleViewport>(rule.Get()))
      AddViewportRule(*viewport_rule);
  }
}

void ViewportStyleResolver::AddViewportRule(StyleRuleViewport& viewport_rule) {
  CSSPropertyValueSet& property_set = viewport_rule.MutableProperties();

  unsigned property_count = property_set.PropertyCount();
  if (!property_count)
    return;

  if (!property_set_) {
    property_set_ = property_set.MutableCopy();
    return;
  }

  // We cannot use MergeAndOverrideOnConflict() here because it doesn't
  // respect the !important declaration (but AddRespectingCascade() does).
  for (unsigned i = 0; i < property_count; ++i) {
    CSSPropertyValueSet::PropertyReference property =
        property_set.PropertyAt(i);
    property_set_->AddRespectingCascade(
        CSSPropertyValue(property.PropertyMetadata(), property.Value()));
  }
}

void ViewportStyleResolver::Resolve() {
  if (!property_set_) {
    document_->GetViewportData().SetViewportDescription(
        ViewportDescription(ViewportDescription::kUserAgentStyleSheet));
    return;
  }

  ViewportDescription description(ViewportDescription::kUserAgentStyleSheet);

  description.user_zoom = ViewportArgumentValue(CSSPropertyID::kUserZoom);
  description.zoom = ViewportArgumentValue(CSSPropertyID::kZoom);
  description.min_zoom = ViewportArgumentValue(CSSPropertyID::kMinZoom);
  description.max_zoom = ViewportArgumentValue(CSSPropertyID::kMaxZoom);
  description.min_width = ViewportLengthValue(CSSPropertyID::kMinWidth);
  description.max_width = ViewportLengthValue(CSSPropertyID::kMaxWidth);
  description.min_height = ViewportLengthValue(CSSPropertyID::kMinHeight);
  description.max_height = ViewportLengthValue(CSSPropertyID::kMaxHeight);
  description.orientation = ViewportArgumentValue(CSSPropertyID::kOrientation);
  if (HasViewportFitProperty(property_set_))
    description.SetViewportFit(ViewportFitValue());

  document_->GetViewportData().SetViewportDescription(description);

  DCHECK(initial_style_);
  if (initial_style_->HasViewportUnits())
    has_viewport_units_ = true;
}

float ViewportStyleResolver::ViewportArgumentValue(CSSPropertyID id) const {
  float default_value = ViewportDescription::kValueAuto;

  // UserZoom default value is CSSValueID::kZoom, which maps to true, meaning
  // that yes, it is user scalable. When the value is set to CSSValueID::kFixed,
  // we return false.
  if (id == CSSPropertyID::kUserZoom)
    default_value = 1;

  const CSSValue* value = property_set_->GetPropertyCSSValue(id);
  auto* identifier_value = DynamicTo<CSSIdentifierValue>(value);
  if (!value || !(value->IsPrimitiveValue() || identifier_value))
    return default_value;

  if (identifier_value) {
    switch (identifier_value->GetValueID()) {
      case CSSValueID::kAuto:
        return default_value;
      case CSSValueID::kLandscape:
        return ViewportDescription::kValueLandscape;
      case CSSValueID::kPortrait:
        return ViewportDescription::kValuePortrait;
      case CSSValueID::kZoom:
        return default_value;
      case CSSValueID::kInternalExtendToZoom:
        return ViewportDescription::kValueExtendToZoom;
      case CSSValueID::kFixed:
        return 0;
      default:
        return default_value;
    }
  }

  const auto* primitive_value = To<CSSPrimitiveValue>(value);

  if (primitive_value->IsNumber() || primitive_value->IsPx())
    return primitive_value->GetFloatValue();

  if (const auto* numeric_literal =
          DynamicTo<CSSNumericLiteralValue>(primitive_value)) {
    if (numeric_literal->IsFontRelativeLength()) {
      return primitive_value->GetFloatValue() *
             initial_style_->GetFontDescription().ComputedSize();
    }
  }

  if (primitive_value->IsPercentage()) {
    float percent_value = primitive_value->GetFloatValue() / 100.0f;
    switch (id) {
      case CSSPropertyID::kMaxZoom:
      case CSSPropertyID::kMinZoom:
      case CSSPropertyID::kZoom:
        return percent_value;
      default:
        NOTREACHED();
        break;
    }
  }

  NOTREACHED();
  return default_value;
}

Length ViewportStyleResolver::ViewportLengthValue(CSSPropertyID id) {
  DCHECK(id == CSSPropertyID::kMaxHeight || id == CSSPropertyID::kMinHeight ||
         id == CSSPropertyID::kMaxWidth || id == CSSPropertyID::kMinWidth);

  const CSSValue* value = property_set_->GetPropertyCSSValue(id);
  if (!value || !(value->IsPrimitiveValue() || value->IsIdentifierValue()))
    return Length();  // auto

  if (auto* identifier_value = DynamicTo<CSSIdentifierValue>(value)) {
    CSSValueID value_id = identifier_value->GetValueID();
    if (value_id == CSSValueID::kInternalExtendToZoom)
      return Length::ExtendToZoom();
    if (value_id == CSSValueID::kAuto)
      return Length::Auto();
  }

  const auto* primitive_value = To<CSSPrimitiveValue>(value);

  LocalFrameView* view = document_->GetFrame()->View();
  DCHECK(view);

  CSSToLengthConversionData::FontSizes font_sizes(initial_style_.get(),
                                                  initial_style_.get());
  CSSToLengthConversionData::ViewportSize viewport_size(
      view->InitialViewportWidth(), view->InitialViewportHeight());

  Length result = primitive_value->ConvertToLength(CSSToLengthConversionData(
      initial_style_.get(), font_sizes, viewport_size, 1.0f));

  if (result.IsFixed() && document_->GetPage()) {
    float scaled_value =
        document_->GetPage()->GetChromeClient().WindowToViewportScalar(
            document_->GetFrame(), result.GetFloatValue());
    result = Length::Fixed(scaled_value);
  }
  return result;
}

mojom::ViewportFit ViewportStyleResolver::ViewportFitValue() const {
  const CSSValue* value =
      property_set_->GetPropertyCSSValue(CSSPropertyID::kViewportFit);
  if (auto* identifier_value = DynamicTo<CSSIdentifierValue>(value)) {
    switch (identifier_value->GetValueID()) {
      case CSSValueID::kCover:
        return mojom::ViewportFit::kCover;
      case CSSValueID::kContain:
        return mojom::ViewportFit::kContain;
      case CSSValueID::kAuto:
      default:
        return mojom::ViewportFit::kAuto;
    }
  }

  NOTREACHED();
  return mojom::ViewportFit::kAuto;
}

void ViewportStyleResolver::InitialStyleChanged() {
  initial_style_ = nullptr;
  // We need to recollect if the initial font size changed and media queries
  // depend on font relative lengths.
  needs_update_ = kCollectRules;
}

void ViewportStyleResolver::InitialViewportChanged() {
  if (needs_update_ == kCollectRules)
    return;
  if (has_viewport_units_)
    needs_update_ = kResolve;
  if (needs_update_ == kNoUpdate)
    return;
  document_->ScheduleLayoutTreeUpdateIfNeeded();
}

void ViewportStyleResolver::SetNeedsCollectRules() {
  needs_update_ = kCollectRules;
  document_->ScheduleLayoutTreeUpdateIfNeeded();
}

void ViewportStyleResolver::UpdateViewport(
    DocumentStyleSheetCollection& collection) {
  if (needs_update_ == kNoUpdate) {
    // If initial_style_ is cleared it means things are dirty, so we should not
    // end up here.
    DCHECK(initial_style_);
    return;
  }
  if (!initial_style_)
    initial_style_ = document_->GetStyleResolver().StyleForViewport();
  if (needs_update_ == kCollectRules) {
    Reset();
    CollectViewportRulesFromUASheets();
  }
  Resolve();
  needs_update_ = kNoUpdate;
}

void ViewportStyleResolver::Trace(Visitor* visitor) const {
  visitor->Trace(document_);
  visitor->Trace(property_set_);
  visitor->Trace(initial_viewport_medium_);
}

}  // namespace blink
