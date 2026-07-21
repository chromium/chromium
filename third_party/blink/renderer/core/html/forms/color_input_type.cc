/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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

#include "third_party/blink/renderer/core/html/forms/color_input_type.h"

#include "third_party/blink/public/mojom/choosers/color_chooser.mojom-blink.h"
#include "third_party/blink/public/mojom/frame/color_scheme.mojom-blink.h"
#include "third_party/blink/renderer/core/css/parser/css_parser.h"
#include "third_party/blink/renderer/core/css/parser/css_property_parser.h"
#include "third_party/blink/renderer/core/dom/events/scoped_event_queue.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/html/forms/color_chooser.h"
#include "third_party/blink/renderer/core/html/forms/html_data_list_element.h"
#include "third_party/blink/renderer/core/html/forms/html_data_list_options_collection.h"
#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
#include "third_party/blink/renderer/core/html/forms/html_option_element.h"
#include "third_party/blink/renderer/core/html/html_div_element.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/layout/layout_theme.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/platform/graphics/color.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

// Upper limit of number of datalist suggestions shown.
static const unsigned kMaxSuggestions = 1000;
// Upper limit for the length of the labels for datalist suggestions.
static const unsigned kMaxSuggestionLabelLength = 1000;
// Constant for black color value.
static const char kBlackColorValue[] = "#000000";
// Default color value when input is empty or invalid.
static const char kFallbackColorValue[] = "#000000";

static bool IsValidColorString(const String& value) {
  if (!value.starts_with('#')) {
    return false;
  }
  // We don't accept #rgb and #aarrggbb formats.
  if (value.length() != 7)
    return false;
  Color color;
  return color.SetFromString(value) && color.IsOpaque();
}

ColorInputType::ColorInputType(HTMLInputElement& element)
    : InputType(Type::kColor, element),
      KeyboardClickableInputTypeView(element) {}

ColorInputType::~ColorInputType() = default;

void ColorInputType::Trace(Visitor* visitor) const {
  visitor->Trace(chooser_);
  KeyboardClickableInputTypeView::Trace(visitor);
  ColorChooserClient::Trace(visitor);
  InputType::Trace(visitor);
}

InputTypeView* ColorInputType::CreateView() {
  return this;
}

InputType::ValueMode ColorInputType::GetValueMode() const {
  return ValueMode::kValue;
}

void ColorInputType::CountUsage() {
  CountUsageIfVisible(WebFeature::kInputTypeColor);
}

bool ColorInputType::SupportsRequired() const {
  return false;
}

bool ColorInputType::IsDisplayP3ColorSpace() const {
  return RuntimeEnabledFeatures::InputTypeColorEnhancementsEnabled() &&
         EqualIgnoringAsciiCase(
             GetElement().FastGetAttribute(html_names::kColorspaceAttr),
             "display-p3");
}

bool ColorInputType::HasAlphaComponent() const {
  return RuntimeEnabledFeatures::InputTypeColorEnhancementsEnabled() &&
         GetElement().FastHasAttribute(html_names::kAlphaAttr);
}

String ColorInputType::SerializeColorForColorSpaceAndAlpha(Color color) const {
  const bool alpha = HasAlphaComponent();
  // If the alpha attribute is absent, the color is made fully opaque.
  if (!alpha) {
    color = color.MakeOpaque();
  }

  if (IsDisplayP3ColorSpace()) {
    if (color.GetColorSpace() != Color::ColorSpace::kDisplayP3) {
      // An achromatic (neutral) sRGB color has identical Display P3 components
      // because the two color spaces share a white point. The float matrix
      // conversion would otherwise introduce ~1e-4 of error, so convert
      // neutrals directly to keep them exact (e.g. white serializes as
      // "color(display-p3 1 1 1)").
      Color srgb = color;
      srgb.ConvertToColorSpace(Color::ColorSpace::kSRGB);
      if (!srgb.HasNoneParams() && srgb.Param0() == srgb.Param1() &&
          srgb.Param0() == srgb.Param2()) {
        color =
            Color::FromColorSpace(Color::ColorSpace::kDisplayP3, srgb.Param0(),
                                  srgb.Param1(), srgb.Param2(), srgb.Alpha());
      } else {
        color.ConvertToColorSpace(Color::ColorSpace::kDisplayP3);
      }
    }
  } else if (alpha) {
    // limited-srgb with alpha serializes as color(srgb ...).
    color.ConvertToColorSpace(Color::ColorSpace::kSRGB);
  } else {
    // limited-srgb without alpha serializes as a valid lowercase simple color
    // (#rrggbb).
    return Color::FromRGBA32(color.MakeOpaque().Rgb())
        .SerializeAsCanvasColor()
        .ToAsciiLower();
  }

  // Resolve any missing ("none") components to zero before serializing. e.g.
  // "color(display-p3 3 none .2)" becomes "color(display-p3 3 0 0.2)". Note
  // that out-of-gamut component values are intentionally preserved.
  if (color.HasNoneParams()) {
    color = Color::FromColorSpace(color.GetColorSpace(),
                                  color.Param0IsNone() ? 0.0f : color.Param0(),
                                  color.Param1IsNone() ? 0.0f : color.Param1(),
                                  color.Param2IsNone() ? 0.0f : color.Param2(),
                                  color.AlphaIsNone() ? 0.0f : color.Alpha());
  }
  return color.SerializeAsCSSColor();
}

String ColorInputType::SanitizeValue(const String& proposed_value) const {
  if (RuntimeEnabledFeatures::InputTypeColorEnhancementsEnabled()) {
    Color color;
    // 'currentcolor' is resolved to black.
    if (CssValueKeywordID(proposed_value) == CSSValueID::kCurrentcolor) {
      color = Color::kBlack;
    } else if (!CSSParser::ParseColor(color, proposed_value) &&
               !CSSParser::ParseSystemColor(
                   color, proposed_value, mojom::blink::ColorScheme::kLight,
                   /*color_provider=*/nullptr,
                   /*can_expose_accent_color=*/false)) {
      // An unparsable value falls back to opaque black.
      color = Color::kBlack;
    }
    return SerializeColorForColorSpaceAndAlpha(color);
  }

  if (RuntimeEnabledFeatures::ColorInputAcceptsCSSColorsEnabled()) {
    // 'currentcolor' is resolved to black.
    if (CssValueKeywordID(proposed_value) == CSSValueID::kCurrentcolor) {
      return kBlackColorValue;
    }

    Color color;
    // Handle most color formats (hex, rgb, hsl, etc.) and System Colors (e.g.
    // "ActiveBorder").
    if (CSSParser::ParseColor(color, proposed_value) ||
        CSSParser::ParseSystemColor(color, proposed_value,
                                    mojom::blink::ColorScheme::kLight,
                                    /*color_provider=*/nullptr,
                                    /*can_expose_accent_color=*/false)) {
      // If the input color has transparency, we drop the alpha channel (make
      // it opaque). Convert to sRGB and serialize as #rrggbb.
      return Color::FromRGBA32(color.MakeOpaque().Rgb())
          .SerializeAsCanvasColor()
          .ToAsciiLower();
    }
    return kFallbackColorValue;
  }

  if (!IsValidColorString(proposed_value)) {
    return kFallbackColorValue;
  }
  return proposed_value.ToAsciiLower();
}

Color ColorInputType::ValueAsColor() const {
  Color color;
  // The stored value is already sanitized, so it is one of #rrggbb,
  // color(srgb ...) or color(display-p3 ...), all of which ParseColor handles.
  bool success = CSSParser::ParseColor(color, GetElement().Value());
  DCHECK(success);
  return color;
}

void ColorInputType::CreateShadowSubtree() {
  DCHECK(IsShadowHost(GetElement()));

  Document& document = GetElement().GetDocument();
  auto* wrapper_element = MakeGarbageCollected<HTMLDivElement>(document);
  wrapper_element->SetShadowPseudoId(
      AtomicString("-webkit-color-swatch-wrapper"));
  auto* color_swatch = MakeGarbageCollected<HTMLDivElement>(document);
  color_swatch->SetShadowPseudoId(AtomicString("-webkit-color-swatch"));
  wrapper_element->AppendChild(color_swatch);
  GetElement().UserAgentShadowRoot()->AppendChild(wrapper_element);

  GetElement().UpdateView();
}

void ColorInputType::DidSetValue(const String&, bool value_changed) {
  if (!value_changed)
    return;
  GetElement().UpdateView();
  if (chooser_)
    chooser_->SetSelectedColor(ValueAsColor());
}

void ColorInputType::HandleDOMActivateEvent(Event& event) {
  if (GetElement().IsDisabledFormControl())
    return;

  Document& document = GetElement().GetDocument();
  if (!LocalFrame::HasTransientUserActivation(document.GetFrame())) {
    document.AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
        mojom::blink::ConsoleMessageSource::kJavaScript,
        mojom::blink::ConsoleMessageLevel::kWarning,
        "A user gesture is required to show the color picker."));
    return;
  }
  if (RuntimeEnabledFeatures::FileColorPickerConsumeActivationEnabled()) {
    LocalFrame::ConsumeTransientUserActivation(document.GetFrame());
  }

  ChromeClient* chrome_client = GetChromeClient();
  if (chrome_client && !HasOpenedPopup()) {
    UseCounter::Count(document,
                      event.IsFullyTrusted()
                          ? WebFeature::kColorInputTypeChooserByTrustedClick
                          : WebFeature::kColorInputTypeChooserByUntrustedClick);
    OpenPopupView();
  }

  event.SetDefaultHandled();
}

AppearanceValue ColorInputType::AutoAppearance() const {
  return AppearanceValue::kSquareButton;
}

void ColorInputType::OpenPopupView() {
  ChromeClient* chrome_client = GetChromeClient();
  Document& document = GetElement().GetDocument();
  chooser_ = chrome_client->OpenColorChooser(document.GetFrame(), this,
                                             ValueAsColor());
  if (GetElement().GetLayoutObject()) {
    // Invalidate paint to ensure that the focus ring is removed.
    GetElement().GetLayoutObject()->SetShouldDoFullPaintInvalidation();
  }
  GetElement().PseudoStateChanged(CSSSelector::kPseudoOpen);
}

void ColorInputType::ClosePopupView() {
  if (chooser_)
    chooser_->EndChooser();
}

bool ColorInputType::HasOpenedPopup() const {
  return chooser_ != nullptr;
}

bool ColorInputType::IsPickerVisible() const {
  return chooser_ && chooser_->IsPickerVisible();
}

bool ColorInputType::ShouldRespectListAttribute() {
  return true;
}

bool ColorInputType::TypeMismatchFor(const String& value) const {
  if (RuntimeEnabledFeatures::InputTypeColorEnhancementsEnabled()) {
    Color color;
    return !CSSParser::ParseColor(color, value);
  }
  return !IsValidColorString(value);
}

void ColorInputType::WarnIfValueIsInvalid(const String& value) const {
  if (EqualIgnoringAsciiCase(value, GetElement().SanitizeValue(value))) {
    return;
  }
  if (RuntimeEnabledFeatures::InputTypeColorEnhancementsEnabled() ||
      RuntimeEnabledFeatures::ColorInputAcceptsCSSColorsEnabled()) {
    AddWarningToConsole(
        "The specified value %s does not conform to the required format.  The "
        "value must be a valid CSS color.",
        value);
  } else {
    AddWarningToConsole(
        "The specified value %s does not conform to the required format.  The "
        "format is \"#rrggbb\" where rr, gg, bb are two-digit hexadecimal "
        "numbers.",
        value);
  }
}

void ColorInputType::ValueAttributeChanged() {
  if (!GetElement().HasDirtyValue())
    GetElement().UpdateView();
}

void ColorInputType::DidChooseColor(const Color& color) {
  if (will_be_destroyed_ || GetElement().IsDisabledFormControl() ||
      color == ValueAsColor())
    return;
  EventQueueScope scope;
  GetElement().SetValueFromRenderer(SerializeColorForColorSpaceAndAlpha(color));
  GetElement().UpdateView();
}

void ColorInputType::DidEndChooser() {
  GetElement().EnqueueChangeEvent();
  chooser_.Clear();
  if (GetElement().GetLayoutObject()) {
    // Invalidate paint to ensure that the focus ring is shown.
    GetElement().GetLayoutObject()->SetShouldDoFullPaintInvalidation();
  }
  GetElement().PseudoStateChanged(CSSSelector::kPseudoOpen);
}

void ColorInputType::ColorSpaceOrAlphaAttributeChanged() {
  if (!RuntimeEnabledFeatures::InputTypeColorEnhancementsEnabled()) {
    return;
  }
  // Re-run the value sanitization algorithm so the serialization reflects the
  // new `colorspace`/`alpha` attributes. For a non-dirty value we re-sanitize
  // from the `value` content attribute rather than the cached value, since the
  // latter may have lost information (e.g. an alpha channel) under the previous
  // attributes.
  if (GetElement().HasDirtyValue()) {
    GetElement().SetValue(GetElement().Value());
  } else {
    GetElement().SetNonDirtyValue(
        GetElement().FastGetAttribute(html_names::kValueAttr));
  }
  GetElement().UpdateView();
}

void ColorInputType::UpdateView() {
  HTMLElement* color_swatch = ShadowColorSwatch();
  if (!color_swatch)
    return;

  color_swatch->SetInlineStyleProperty(CSSPropertyID::kBackgroundColor,
                                       GetElement().Value());
}

HTMLElement* ColorInputType::ShadowColorSwatch() const {
  ShadowRoot* shadow = GetElement().UserAgentShadowRoot();
  if (shadow) {
    CHECK(IsA<HTMLElement>(shadow->firstChild()->firstChild()));
    return To<HTMLElement>(shadow->firstChild()->firstChild());
  }
  return nullptr;
}

Element& ColorInputType::OwnerElement() const {
  return GetElement();
}

gfx::Rect ColorInputType::ElementRectRelativeToLocalRoot() const {
  return GetElement().GetDocument().View()->ConvertToRootFrame(
      GetElement().PixelSnappedBoundingBox());
}

Color ColorInputType::CurrentColor() {
  return ValueAsColor();
}

bool ColorInputType::ShouldShowSuggestions() const {
  return GetElement().FastHasAttribute(html_names::kListAttr);
}

Vector<mojom::blink::ColorSuggestionPtr> ColorInputType::Suggestions() const {
  Vector<mojom::blink::ColorSuggestionPtr> suggestions;
  HTMLDataListElement* data_list = GetElement().DataList();
  if (data_list) {
    HTMLDataListOptionsCollection* options = data_list->options();
    for (unsigned i = 0; HTMLOptionElement* option = options->Item(i); i++) {
      if (option->IsDisabledFormControl() || option->value().empty())
        continue;
      if (!GetElement().IsValidValue(option->value()))
        continue;
      Color color;
      if (!CSSParser::ParseColor(color, option->value())) {
        continue;
      }
      suggestions.push_back(mojom::blink::ColorSuggestion::New(
          color.Rgb(), option->label().substr(0, kMaxSuggestionLabelLength)));
      if (suggestions.size() >= kMaxSuggestions)
        break;
    }
  }
  return suggestions;
}

AXObject* ColorInputType::PopupRootAXObject() {
  return chooser_ ? chooser_->RootAXObject(&GetElement()) : nullptr;
}

ColorChooserClient* ColorInputType::GetColorChooserClient() {
  return this;
}

bool ColorInputType::SupportsBaseAppearance(
    Element::BaseAppearanceValue value) const {
  return RuntimeEnabledFeatures::AppearanceBaseEnabled() &&
         value == Element::BaseAppearanceValue::kBase;
}

}  // namespace blink
