/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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

#include "third_party/blink/renderer/core/css/font_face.h"

#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/bindings/core/v8/string_or_array_buffer_or_array_buffer_view.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_font_face_descriptors.h"
#include "third_party/blink/renderer/core/css/binary_data_font_face_source.h"
#include "third_party/blink/renderer/core/css/css_font_face.h"
#include "third_party/blink/renderer/core/css/css_font_face_src_value.h"
#include "third_party/blink/renderer/core/css/css_font_family_value.h"
#include "third_party/blink/renderer/core/css/css_font_selector.h"
#include "third_party/blink/renderer/core/css/css_font_style_range_value.h"
#include "third_party/blink/renderer/core/css/css_identifier_value.h"
#include "third_party/blink/renderer/core/css/css_property_value_set.h"
#include "third_party/blink/renderer/core/css/css_unicode_range_value.h"
#include "third_party/blink/renderer/core/css/css_value_list.h"
#include "third_party/blink/renderer/core/css/local_font_face_source.h"
#include "third_party/blink/renderer/core/css/offscreen_font_selector.h"
#include "third_party/blink/renderer/core/css/parser/at_rule_descriptor_parser.h"
#include "third_party/blink/renderer/core/css/parser/css_parser.h"
#include "third_party/blink/renderer/core/css/remote_font_face_source.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/css/style_rule.h"
#include "third_party/blink/renderer/core/css_value_keywords.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer_view.h"
#include "third_party/blink/renderer/core/workers/worker_global_scope.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/font_family_names.h"
#include "third_party/blink/renderer/platform/fonts/font_metrics_override.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/instrumentation/histogram.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/shared_buffer.h"

namespace blink {

namespace {

const CSSValue* ParseCSSValue(const ExecutionContext* context,
                              const String& value,
                              AtRuleDescriptorID descriptor_id) {
  auto* window = DynamicTo<LocalDOMWindow>(context);
  CSSParserContext* parser_context =
      window ? MakeGarbageCollected<CSSParserContext>(*window->document())
             : MakeGarbageCollected<CSSParserContext>(*context);
  return AtRuleDescriptorParser::ParseFontFaceDescriptor(descriptor_id, value,
                                                         *parser_context);
}

CSSFontFace* CreateCSSFontFace(FontFace* font_face,
                               const CSSValue* unicode_range) {
  Vector<UnicodeRange> ranges;
  if (const auto* range_list = To<CSSValueList>(unicode_range)) {
    unsigned num_ranges = range_list->length();
    for (unsigned i = 0; i < num_ranges; i++) {
      const auto& range =
          To<cssvalue::CSSUnicodeRangeValue>(range_list->Item(i));
      ranges.push_back(UnicodeRange(range.From(), range.To()));
    }
  }

  return MakeGarbageCollected<CSSFontFace>(font_face, ranges);
}

const CSSValue* ConvertFontMetricOverrideValue(const CSSValue* parsed_value) {
  if (parsed_value && parsed_value->IsIdentifierValue()) {
    // We store the "normal" keyword value as nullptr
    DCHECK_EQ(CSSValueID::kNormal,
              To<CSSIdentifierValue>(parsed_value)->GetValueID());
    return nullptr;
  }
  return parsed_value;
}

}  // namespace

FontFace* FontFace::Create(ExecutionContext* context,
                           const AtomicString& family,
                           StringOrArrayBufferOrArrayBufferView& source,
                           const FontFaceDescriptors* descriptors) {
  if (source.IsString())
    return Create(context, family, source.GetAsString(), descriptors);
  if (source.IsArrayBuffer())
    return Create(context, family, source.GetAsArrayBuffer(), descriptors);
  if (source.IsArrayBufferView()) {
    return Create(context, family, source.GetAsArrayBufferView().View(),
                  descriptors);
  }
  NOTREACHED();
  return nullptr;
}

FontFace* FontFace::Create(ExecutionContext* context,
                           const AtomicString& family,
                           const String& source,
                           const FontFaceDescriptors* descriptors) {
  FontFace* font_face =
      MakeGarbageCollected<FontFace>(context, family, descriptors);

  const CSSValue* src = ParseCSSValue(context, source, AtRuleDescriptorID::Src);
  if (!src || !src->IsValueList()) {
    font_face->SetError(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kSyntaxError,
        "The source provided ('" + source +
            "') could not be parsed as a value list."));
  }

  font_face->InitCSSFontFace(context, *src);
  return font_face;
}

FontFace* FontFace::Create(ExecutionContext* context,
                           const AtomicString& family,
                           DOMArrayBuffer* source,
                           const FontFaceDescriptors* descriptors) {
  FontFace* font_face =
      MakeGarbageCollected<FontFace>(context, family, descriptors);
  font_face->InitCSSFontFace(static_cast<const unsigned char*>(source->Data()),
                             source->ByteLengthAsSizeT());
  return font_face;
}

FontFace* FontFace::Create(ExecutionContext* context,
                           const AtomicString& family,
                           DOMArrayBufferView* source,
                           const FontFaceDescriptors* descriptors) {
  FontFace* font_face =
      MakeGarbageCollected<FontFace>(context, family, descriptors);
  font_face->InitCSSFontFace(
      static_cast<const unsigned char*>(source->BaseAddress()),
      source->byteLengthAsSizeT());
  return font_face;
}

FontFace* FontFace::Create(Document* document,
                           const StyleRuleFontFace* font_face_rule) {
  const CSSPropertyValueSet& properties = font_face_rule->Properties();

  // Obtain the font-family property and the src property. Both must be defined.
  const CSSValue* family =
      properties.GetPropertyCSSValue(AtRuleDescriptorID::FontFamily);
  if (!family || (!family->IsFontFamilyValue() && !family->IsIdentifierValue()))
    return nullptr;
  const CSSValue* src = properties.GetPropertyCSSValue(AtRuleDescriptorID::Src);
  if (!src || !src->IsValueList())
    return nullptr;

  FontFace* font_face =
      MakeGarbageCollected<FontFace>(document->GetExecutionContext());

  if (font_face->SetFamilyValue(*family) &&
      font_face->SetPropertyFromStyle(properties,
                                      AtRuleDescriptorID::FontStyle) &&
      font_face->SetPropertyFromStyle(properties,
                                      AtRuleDescriptorID::FontWeight) &&
      font_face->SetPropertyFromStyle(properties,
                                      AtRuleDescriptorID::FontStretch) &&
      font_face->SetPropertyFromStyle(properties,
                                      AtRuleDescriptorID::UnicodeRange) &&
      font_face->SetPropertyFromStyle(properties,
                                      AtRuleDescriptorID::FontVariant) &&
      font_face->SetPropertyFromStyle(
          properties, AtRuleDescriptorID::FontFeatureSettings) &&
      font_face->SetPropertyFromStyle(properties,
                                      AtRuleDescriptorID::FontDisplay) &&
      font_face->SetPropertyFromStyle(properties,
                                      AtRuleDescriptorID::AscentOverride) &&
      font_face->SetPropertyFromStyle(properties,
                                      AtRuleDescriptorID::DescentOverride) &&
      font_face->SetPropertyFromStyle(properties,
                                      AtRuleDescriptorID::LineGapOverride) &&
      font_face->SetPropertyFromStyle(properties,
                                      AtRuleDescriptorID::AdvanceOverride) &&
      font_face->GetFontSelectionCapabilities().IsValid() &&
      !font_face->family().IsEmpty()) {
    font_face->InitCSSFontFace(document->GetExecutionContext(), *src);
    return font_face;
  }
  return nullptr;
}

FontFace::FontFace(ExecutionContext* context)
    : ExecutionContextClient(context), status_(kUnloaded) {}

FontFace::FontFace(ExecutionContext* context,
                   const AtomicString& family,
                   const FontFaceDescriptors* descriptors)
    : ExecutionContextClient(context), family_(family), status_(kUnloaded) {
  SetPropertyFromString(context, descriptors->style(),
                        AtRuleDescriptorID::FontStyle);
  SetPropertyFromString(context, descriptors->weight(),
                        AtRuleDescriptorID::FontWeight);
  SetPropertyFromString(context, descriptors->stretch(),
                        AtRuleDescriptorID::FontStretch);
  SetPropertyFromString(context, descriptors->unicodeRange(),
                        AtRuleDescriptorID::UnicodeRange);
  SetPropertyFromString(context, descriptors->variant(),
                        AtRuleDescriptorID::FontVariant);
  SetPropertyFromString(context, descriptors->featureSettings(),
                        AtRuleDescriptorID::FontFeatureSettings);
  SetPropertyFromString(context, descriptors->display(),
                        AtRuleDescriptorID::FontDisplay);
  // TODO(xiaochengh): Add override descriptors to FontFaceDescriptors
}

FontFace::~FontFace() = default;

String FontFace::style() const {
  return style_ ? style_->CssText() : "normal";
}

String FontFace::weight() const {
  return weight_ ? weight_->CssText() : "normal";
}

String FontFace::stretch() const {
  return stretch_ ? stretch_->CssText() : "normal";
}

String FontFace::unicodeRange() const {
  return unicode_range_ ? unicode_range_->CssText() : "U+0-10FFFF";
}

String FontFace::variant() const {
  return variant_ ? variant_->CssText() : "normal";
}

String FontFace::featureSettings() const {
  return feature_settings_ ? feature_settings_->CssText() : "normal";
}

String FontFace::display() const {
  return display_ ? display_->CssText() : "auto";
}

void FontFace::setStyle(ExecutionContext* context,
                        const String& s,
                        ExceptionState& exception_state) {
  SetPropertyFromString(context, s, AtRuleDescriptorID::FontStyle,
                        &exception_state);
}

void FontFace::setWeight(ExecutionContext* context,
                         const String& s,
                         ExceptionState& exception_state) {
  SetPropertyFromString(context, s, AtRuleDescriptorID::FontWeight,
                        &exception_state);
}

void FontFace::setStretch(ExecutionContext* context,
                          const String& s,
                          ExceptionState& exception_state) {
  SetPropertyFromString(context, s, AtRuleDescriptorID::FontStretch,
                        &exception_state);
}

void FontFace::setUnicodeRange(ExecutionContext* context,
                               const String& s,
                               ExceptionState& exception_state) {
  SetPropertyFromString(context, s, AtRuleDescriptorID::UnicodeRange,
                        &exception_state);
}

void FontFace::setVariant(ExecutionContext* context,
                          const String& s,
                          ExceptionState& exception_state) {
  SetPropertyFromString(context, s, AtRuleDescriptorID::FontVariant,
                        &exception_state);
}

void FontFace::setFeatureSettings(ExecutionContext* context,
                                  const String& s,
                                  ExceptionState& exception_state) {
  SetPropertyFromString(context, s, AtRuleDescriptorID::FontFeatureSettings,
                        &exception_state);
}

void FontFace::setDisplay(ExecutionContext* context,
                          const String& s,
                          ExceptionState& exception_state) {
  SetPropertyFromString(context, s, AtRuleDescriptorID::FontDisplay,
                        &exception_state);
}

void FontFace::SetPropertyFromString(const ExecutionContext* context,
                                     const String& s,
                                     AtRuleDescriptorID descriptor_id,
                                     ExceptionState* exception_state) {
  const CSSValue* value = ParseCSSValue(context, s, descriptor_id);
  if (value && SetPropertyValue(value, descriptor_id))
    return;

  String message = "Failed to set '" + s + "' as a property value.";
  if (exception_state) {
    exception_state->ThrowDOMException(DOMExceptionCode::kSyntaxError, message);
  } else {
    SetError(MakeGarbageCollected<DOMException>(DOMExceptionCode::kSyntaxError,
                                                message));
  }
}

bool FontFace::SetPropertyFromStyle(const CSSPropertyValueSet& properties,
                                    AtRuleDescriptorID property_id) {
  return SetPropertyValue(properties.GetPropertyCSSValue(property_id),
                          property_id);
}

bool FontFace::SetPropertyValue(const CSSValue* value,
                                AtRuleDescriptorID descriptor_id) {
  switch (descriptor_id) {
    case AtRuleDescriptorID::FontStyle:
      style_ = value;
      break;
    case AtRuleDescriptorID::FontWeight:
      weight_ = value;
      break;
    case AtRuleDescriptorID::FontStretch:
      stretch_ = value;
      break;
    case AtRuleDescriptorID::UnicodeRange:
      if (value && !value->IsValueList())
        return false;
      unicode_range_ = value;
      break;
    case AtRuleDescriptorID::FontVariant:
      variant_ = value;
      break;
    case AtRuleDescriptorID::FontFeatureSettings:
      feature_settings_ = value;
      break;
    case AtRuleDescriptorID::FontDisplay:
      display_ = value;
      if (css_font_face_)
        css_font_face_->SetDisplay(CSSValueToFontDisplay(display_.Get()));
      break;
    case AtRuleDescriptorID::AscentOverride:
      ascent_override_ = ConvertFontMetricOverrideValue(value);
      break;
    case AtRuleDescriptorID::DescentOverride:
      descent_override_ = ConvertFontMetricOverrideValue(value);
      break;
    case AtRuleDescriptorID::LineGapOverride:
      line_gap_override_ = ConvertFontMetricOverrideValue(value);
      break;
    case AtRuleDescriptorID::AdvanceOverride:
      advance_override_ = value;
      break;
    default:
      NOTREACHED();
      return false;
  }
  return true;
}

bool FontFace::SetFamilyValue(const CSSValue& value) {
  AtomicString family;
  if (auto* family_value = DynamicTo<CSSFontFamilyValue>(value)) {
    family = AtomicString(family_value->Value());
  } else if (auto* identifier_value = DynamicTo<CSSIdentifierValue>(value)) {
    // We need to use the raw text for all the generic family types, since
    // @font-face is a way of actually defining what font to use for those
    // types.
    switch (identifier_value->GetValueID()) {
      case CSSValueID::kSerif:
        family = font_family_names::kWebkitSerif;
        break;
      case CSSValueID::kSansSerif:
        family = font_family_names::kWebkitSansSerif;
        break;
      case CSSValueID::kCursive:
        family = font_family_names::kWebkitCursive;
        break;
      case CSSValueID::kFantasy:
        family = font_family_names::kWebkitFantasy;
        break;
      case CSSValueID::kMonospace:
        family = font_family_names::kWebkitMonospace;
        break;
      default:
        return false;
    }
  }
  family_ = family;
  return true;
}

String FontFace::status() const {
  switch (status_) {
    case kUnloaded:
      return "unloaded";
    case kLoading:
      return "loading";
    case kLoaded:
      return "loaded";
    case kError:
      return "error";
    default:
      NOTREACHED();
  }
  return g_empty_string;
}

void FontFace::SetLoadStatus(LoadStatusType status) {
  status_ = status;
  DCHECK(status_ != kError || error_);

  if (!GetExecutionContext())
    return;

  if (status_ == kLoaded || status_ == kError) {
    if (loaded_property_) {
      if (status_ == kLoaded) {
        GetExecutionContext()
            ->GetTaskRunner(TaskType::kDOMManipulation)
            ->PostTask(FROM_HERE,
                       WTF::Bind(&LoadedProperty::Resolve<FontFace*>,
                                 WrapPersistent(loaded_property_.Get()),
                                 WrapPersistent(this)));
      } else {
        GetExecutionContext()
            ->GetTaskRunner(TaskType::kDOMManipulation)
            ->PostTask(FROM_HERE,
                       WTF::Bind(&LoadedProperty::Reject<DOMException*>,
                                 WrapPersistent(loaded_property_.Get()),
                                 WrapPersistent(error_.Get())));
      }
    }

    GetExecutionContext()
        ->GetTaskRunner(TaskType::kDOMManipulation)
        ->PostTask(FROM_HERE,
                   WTF::Bind(&FontFace::RunCallbacks, WrapPersistent(this)));
  }
}

void FontFace::RunCallbacks() {
  HeapVector<Member<LoadFontCallback>> callbacks;
  callbacks_.swap(callbacks);
  for (wtf_size_t i = 0; i < callbacks.size(); ++i) {
    if (status_ == kLoaded)
      callbacks[i]->NotifyLoaded(this);
    else
      callbacks[i]->NotifyError(this);
  }
}

void FontFace::SetError(DOMException* error) {
  if (!error_) {
    error_ = error ? error
                   : MakeGarbageCollected<DOMException>(
                         DOMExceptionCode::kNetworkError);
  }
  SetLoadStatus(kError);
}

ScriptPromise FontFace::FontStatusPromise(ScriptState* script_state) {
  if (!loaded_property_) {
    loaded_property_ = MakeGarbageCollected<LoadedProperty>(
        ExecutionContext::From(script_state));
    if (status_ == kLoaded)
      loaded_property_->Resolve(this);
    else if (status_ == kError)
      loaded_property_->Reject(error_.Get());
  }
  return loaded_property_->Promise(script_state->World());
}

ScriptPromise FontFace::load(ScriptState* script_state) {
  if (status_ == kUnloaded)
    css_font_face_->Load();
  DidBeginImperativeLoad();
  return FontStatusPromise(script_state);
}

void FontFace::LoadWithCallback(LoadFontCallback* callback) {
  if (status_ == kUnloaded)
    css_font_face_->Load();
  AddCallback(callback);
}

void FontFace::AddCallback(LoadFontCallback* callback) {
  if (status_ == kLoaded)
    callback->NotifyLoaded(this);
  else if (status_ == kError)
    callback->NotifyError(this);
  else
    callbacks_.push_back(callback);
}

FontSelectionCapabilities FontFace::GetFontSelectionCapabilities() const {
  // FontSelectionCapabilities represents a range of available width, slope and
  // weight values. The first value of each pair is the minimum value, the
  // second is the maximum value.
  FontSelectionCapabilities normal_capabilities(
      {NormalWidthValue(), NormalWidthValue()},
      {NormalSlopeValue(), NormalSlopeValue()},
      {NormalWeightValue(), NormalWeightValue()});
  FontSelectionCapabilities capabilities(normal_capabilities);

  if (stretch_) {
    if (auto* stretch_identifier_value =
            DynamicTo<CSSIdentifierValue>(stretch_.Get())) {
      switch (stretch_identifier_value->GetValueID()) {
        case CSSValueID::kUltraCondensed:
          capabilities.width = {UltraCondensedWidthValue(),
                                UltraCondensedWidthValue()};
          break;
        case CSSValueID::kExtraCondensed:
          capabilities.width = {ExtraCondensedWidthValue(),
                                ExtraCondensedWidthValue()};
          break;
        case CSSValueID::kCondensed:
          capabilities.width = {CondensedWidthValue(), CondensedWidthValue()};
          break;
        case CSSValueID::kSemiCondensed:
          capabilities.width = {SemiCondensedWidthValue(),
                                SemiCondensedWidthValue()};
          break;
        case CSSValueID::kSemiExpanded:
          capabilities.width = {SemiExpandedWidthValue(),
                                SemiExpandedWidthValue()};
          break;
        case CSSValueID::kExpanded:
          capabilities.width = {ExpandedWidthValue(), ExpandedWidthValue()};
          break;
        case CSSValueID::kExtraExpanded:
          capabilities.width = {ExtraExpandedWidthValue(),
                                ExtraExpandedWidthValue()};
          break;
        case CSSValueID::kUltraExpanded:
          capabilities.width = {UltraExpandedWidthValue(),
                                UltraExpandedWidthValue()};
          break;
        default:
          break;
      }
    } else if (const auto* stretch_list =
                   DynamicTo<CSSValueList>(stretch_.Get())) {
      // Transition FontFace interpretation of parsed values from
      // CSSIdentifierValue to CSSValueList or CSSPrimitiveValue.
      // TODO(drott) crbug.com/739139: Update the parser to only produce
      // CSSPrimitiveValue or CSSValueList.
      if (stretch_list->length() != 2)
        return normal_capabilities;
      const auto* stretch_from =
          DynamicTo<CSSPrimitiveValue>(&stretch_list->Item(0));
      const auto* stretch_to =
          DynamicTo<CSSPrimitiveValue>(&stretch_list->Item(1));
      if (!stretch_from || !stretch_to)
        return normal_capabilities;
      if (!stretch_from->IsPercentage() || !stretch_to->IsPercentage())
        return normal_capabilities;
      // https://drafts.csswg.org/css-fonts/#font-prop-desc
      // "User agents must swap the computed value of the startpoint and
      // endpoint of the range in order to forbid decreasing ranges."
      if (stretch_from->GetFloatValue() < stretch_to->GetFloatValue()) {
        capabilities.width = {FontSelectionValue(stretch_from->GetFloatValue()),
                              FontSelectionValue(stretch_to->GetFloatValue())};
      } else {
        capabilities.width = {
            FontSelectionValue(stretch_to->GetFloatValue()),
            FontSelectionValue(stretch_from->GetFloatValue())};
      }
    } else if (auto* stretch_primitive_value =
                   DynamicTo<CSSPrimitiveValue>(stretch_.Get())) {
      float stretch_value = stretch_primitive_value->GetFloatValue();
      capabilities.width = {FontSelectionValue(stretch_value),
                            FontSelectionValue(stretch_value)};
    } else {
      NOTREACHED();
      return normal_capabilities;
    }
  }

  if (style_) {
    if (auto* identifier_value = DynamicTo<CSSIdentifierValue>(style_.Get())) {
      switch (identifier_value->GetValueID()) {
        case CSSValueID::kNormal:
          capabilities.slope = {NormalSlopeValue(), NormalSlopeValue()};
          break;
        case CSSValueID::kOblique:
          capabilities.slope = {ItalicSlopeValue(), ItalicSlopeValue()};
          break;
        case CSSValueID::kItalic:
          capabilities.slope = {ItalicSlopeValue(), ItalicSlopeValue()};
          break;
        default:
          break;
      }
    } else if (const auto* range_value =
                   DynamicTo<cssvalue::CSSFontStyleRangeValue>(style_.Get())) {
      if (range_value->GetFontStyleValue()->IsIdentifierValue()) {
        CSSValueID font_style_id =
            range_value->GetFontStyleValue()->GetValueID();
        if (!range_value->GetObliqueValues()) {
          if (font_style_id == CSSValueID::kNormal)
            capabilities.slope = {NormalSlopeValue(), NormalSlopeValue()};
          DCHECK(font_style_id == CSSValueID::kItalic ||
                 font_style_id == CSSValueID::kOblique);
          capabilities.slope = {ItalicSlopeValue(), ItalicSlopeValue()};
        } else {
          DCHECK(font_style_id == CSSValueID::kOblique);
          size_t oblique_values_size =
              range_value->GetObliqueValues()->length();
          if (oblique_values_size == 1) {
            const auto& range_start =
                To<CSSPrimitiveValue>(range_value->GetObliqueValues()->Item(0));
            FontSelectionValue oblique_range(range_start.GetFloatValue());
            capabilities.slope = {oblique_range, oblique_range};
          } else {
            DCHECK_EQ(oblique_values_size, 2u);
            const auto& range_start =
                To<CSSPrimitiveValue>(range_value->GetObliqueValues()->Item(0));
            const auto& range_end =
                To<CSSPrimitiveValue>(range_value->GetObliqueValues()->Item(1));
            // https://drafts.csswg.org/css-fonts/#font-prop-desc
            // "User agents must swap the computed value of the startpoint and
            // endpoint of the range in order to forbid decreasing ranges."
            if (range_start.GetFloatValue() < range_end.GetFloatValue()) {
              capabilities.slope = {
                  FontSelectionValue(range_start.GetFloatValue()),
                  FontSelectionValue(range_end.GetFloatValue())};
            } else {
              capabilities.slope = {
                  FontSelectionValue(range_end.GetFloatValue()),
                  FontSelectionValue(range_start.GetFloatValue())};
            }
          }
        }
      }
    } else {
      NOTREACHED();
      return normal_capabilities;
    }
  }

  if (weight_) {
    if (auto* identifier_value = DynamicTo<CSSIdentifierValue>(weight_.Get())) {
      switch (identifier_value->GetValueID()) {
        // Although 'lighter' and 'bolder' are valid keywords for
        // font-weights, they are invalid inside font-face rules so they are
        // ignored. Reference:
        // http://www.w3.org/TR/css3-fonts/#descdef-font-weight.
        case CSSValueID::kLighter:
        case CSSValueID::kBolder:
          break;
        case CSSValueID::kNormal:
          capabilities.weight = {NormalWeightValue(), NormalWeightValue()};
          break;
        case CSSValueID::kBold:
          capabilities.weight = {BoldWeightValue(), BoldWeightValue()};
          break;
        default:
          NOTREACHED();
          break;
      }
    } else if (const auto* weight_list =
                   DynamicTo<CSSValueList>(weight_.Get())) {
      if (weight_list->length() != 2)
        return normal_capabilities;
      const auto* weight_from =
          DynamicTo<CSSPrimitiveValue>(&weight_list->Item(0));
      const auto* weight_to =
          DynamicTo<CSSPrimitiveValue>(&weight_list->Item(1));
      if (!weight_from || !weight_to)
        return normal_capabilities;
      if (!weight_from->IsNumber() || !weight_to->IsNumber() ||
          weight_from->GetFloatValue() < 1 || weight_to->GetFloatValue() > 1000)
        return normal_capabilities;
      // https://drafts.csswg.org/css-fonts/#font-prop-desc
      // "User agents must swap the computed value of the startpoint and
      // endpoint of the range in order to forbid decreasing ranges."
      if (weight_from->GetFloatValue() < weight_to->GetFloatValue()) {
        capabilities.weight = {FontSelectionValue(weight_from->GetFloatValue()),
                               FontSelectionValue(weight_to->GetFloatValue())};
      } else {
        capabilities.weight = {
            FontSelectionValue(weight_to->GetFloatValue()),
            FontSelectionValue(weight_from->GetFloatValue())};
      }
    } else if (auto* weight_primitive_value =
                   DynamicTo<CSSPrimitiveValue>(weight_.Get())) {
      float weight_value = weight_primitive_value->GetFloatValue();
      if (weight_value < 1 || weight_value > 1000)
        return normal_capabilities;
      capabilities.weight = {FontSelectionValue(weight_value),
                             FontSelectionValue(weight_value)};
    } else {
      NOTREACHED();
      return normal_capabilities;
    }
  }

  return capabilities;
}

size_t FontFace::ApproximateBlankCharacterCount() const {
  if (status_ == kLoading)
    return css_font_face_->ApproximateBlankCharacterCount();
  return 0;
}

bool ContextAllowsDownload(ExecutionContext* context) {
  if (!context) {
    return false;
  }
  if (const auto* window = DynamicTo<LocalDOMWindow>(context)) {
    const Settings* settings =
        window->GetFrame() ? window->GetFrame()->GetSettings() : nullptr;
    return settings && settings->GetDownloadableBinaryFontsEnabled();
  }
  // TODO(fserb): ideally, we would like to have the settings value available
  // on workers. Right now, we don't support that.
  return true;
}

void FontFace::InitCSSFontFace(ExecutionContext* context, const CSSValue& src) {
  css_font_face_ = CreateCSSFontFace(this, unicode_range_.Get());
  if (error_)
    return;

  // Each item in the src property's list is a single CSSFontFaceSource. Put
  // them all into a CSSFontFace.
  const auto& src_list = To<CSSValueList>(src);
  int src_length = src_list.length();

  for (int i = 0; i < src_length; i++) {
    // An item in the list either specifies a string (local font name) or a URL
    // (remote font to download).
    const CSSFontFaceSrcValue& item = To<CSSFontFaceSrcValue>(src_list.Item(i));

    FontSelector* font_selector = nullptr;
    if (auto* window = DynamicTo<LocalDOMWindow>(context)) {
      font_selector = window->document()->GetStyleEngine().GetFontSelector();
    } else if (auto* scope = DynamicTo<WorkerGlobalScope>(context)) {
      font_selector = scope->GetFontSelector();
    } else {
      NOTREACHED();
    }
    if (!item.IsLocal()) {
      if (ContextAllowsDownload(context) && item.IsSupportedFormat()) {
        RemoteFontFaceSource* source =
            MakeGarbageCollected<RemoteFontFaceSource>(
                css_font_face_, font_selector,
                CSSValueToFontDisplay(display_.Get()));
        item.Fetch(context, source);
        css_font_face_->AddSource(source);
      }
    } else {
      css_font_face_->AddSource(MakeGarbageCollected<LocalFontFaceSource>(
          css_font_face_, font_selector, item.GetResource()));
    }
  }

  if (display_) {
    DEFINE_THREAD_SAFE_STATIC_LOCAL(
        EnumerationHistogram, font_display_histogram,
        ("WebFont.FontDisplayValue", kFontDisplayEnumMax));
    font_display_histogram.Count(CSSValueToFontDisplay(display_.Get()));
  }
}

void FontFace::InitCSSFontFace(const unsigned char* data, size_t size) {
  css_font_face_ = CreateCSSFontFace(this, unicode_range_.Get());
  if (error_)
    return;

  scoped_refptr<SharedBuffer> buffer = SharedBuffer::Create(data, size);
  BinaryDataFontFaceSource* source =
      MakeGarbageCollected<BinaryDataFontFaceSource>(
          css_font_face_, buffer.get(), ots_parse_message_);
  if (source->IsValid()) {
    SetLoadStatus(kLoaded);
  } else {
    SetError(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kSyntaxError, "Invalid font data in ArrayBuffer."));
  }
  css_font_face_->AddSource(source);
}

void FontFace::Trace(Visitor* visitor) const {
  visitor->Trace(style_);
  visitor->Trace(weight_);
  visitor->Trace(stretch_);
  visitor->Trace(unicode_range_);
  visitor->Trace(variant_);
  visitor->Trace(feature_settings_);
  visitor->Trace(display_);
  visitor->Trace(ascent_override_);
  visitor->Trace(descent_override_);
  visitor->Trace(line_gap_override_);
  visitor->Trace(advance_override_);
  visitor->Trace(error_);
  visitor->Trace(loaded_property_);
  visitor->Trace(css_font_face_);
  visitor->Trace(callbacks_);
  ScriptWrappable::Trace(visitor);
  ExecutionContextClient::Trace(visitor);
}

bool FontFace::HadBlankText() const {
  return css_font_face_->HadBlankText();
}

bool FontFace::HasPendingActivity() const {
  return status_ == kLoading && GetExecutionContext();
}

FontDisplay FontFace::GetFontDisplay() const {
  return CSSValueToFontDisplay(display_.Get());
}

void FontFace::DidBeginImperativeLoad() {
  if (!DomWindow())
    return;
  DomWindow()->document()->GetFontPreloadManager().ImperativeFontLoadingStarted(
      this);
}

FontMetricsOverride FontFace::GetFontMetricsOverride() const {
  FontMetricsOverride result;
  if (ascent_override_) {
    result.ascent_override =
        To<CSSPrimitiveValue>(*ascent_override_).GetFloatValue() / 100;
  }
  if (descent_override_) {
    result.descent_override =
        To<CSSPrimitiveValue>(*descent_override_).GetFloatValue() / 100;
  }
  if (line_gap_override_) {
    result.line_gap_override =
        To<CSSPrimitiveValue>(*line_gap_override_).GetFloatValue() / 100;
  }
  if (advance_override_) {
    result.advance_override =
        To<CSSPrimitiveValue>(*advance_override_).GetFloatValue();
  }
  return result;
}

}  // namespace blink
