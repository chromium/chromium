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
#include "third_party/blink/renderer/core/css/font_face_descriptors.h"
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
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/frame/use_counter.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer_view.h"
#include "third_party/blink/renderer/core/workers/worker_global_scope.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/font_family_names.h"
#include "third_party/blink/renderer/platform/histogram.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/shared_buffer.h"

namespace blink {

namespace {

const CSSValue* ParseCSSValue(const ExecutionContext* context,
                              const String& value,
                              AtRuleDescriptorID descriptor_id) {
  CSSParserContext* parser_context =
      IsA<Document>(context) ? CSSParserContext::Create(*To<Document>(context))
                             : CSSParserContext::Create(*context);
  return AtRuleDescriptorParser::ParseFontFaceDescriptor(descriptor_id, value,
                                                         *parser_context);
}

FontDisplay CSSValueToFontDisplay(const CSSValue* value) {
  if (value && value->IsIdentifierValue()) {
    switch (ToCSSIdentifierValue(value)->GetValueID()) {
      case CSSValueAuto:
        return kFontDisplayAuto;
      case CSSValueBlock:
        return kFontDisplayBlock;
      case CSSValueSwap:
        return kFontDisplaySwap;
      case CSSValueFallback:
        return kFontDisplayFallback;
      case CSSValueOptional:
        return kFontDisplayOptional;
      default:
        break;
    }
  }
  return kFontDisplayAuto;
}

CSSFontFace* CreateCSSFontFace(FontFace* font_face,
                               const CSSValue* unicode_range) {
  Vector<UnicodeRange> ranges;
  if (const CSSValueList* range_list = ToCSSValueList(unicode_range)) {
    unsigned num_ranges = range_list->length();
    for (unsigned i = 0; i < num_ranges; i++) {
      const CSSUnicodeRangeValue& range =
          ToCSSUnicodeRangeValue(range_list->Item(i));
      ranges.push_back(UnicodeRange(range.From(), range.To()));
    }
  }

  return new CSSFontFace(font_face, ranges);
}

}  // namespace

FontFace* FontFace::Create(ExecutionContext* context,
                           const AtomicString& family,
                           StringOrArrayBufferOrArrayBufferView& source,
                           const FontFaceDescriptors& descriptors) {
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
                           const FontFaceDescriptors& descriptors) {
  FontFace* font_face = new FontFace(context, family, descriptors);

  const CSSValue* src = ParseCSSValue(context, source, AtRuleDescriptorID::Src);
  if (!src || !src->IsValueList()) {
    font_face->SetError(
        DOMException::Create(DOMExceptionCode::kSyntaxError,
                             "The source provided ('" + source +
                                 "') could not be parsed as a value list."));
  }

  font_face->InitCSSFontFace(context, *src);
  return font_face;
}

FontFace* FontFace::Create(ExecutionContext* context,
                           const AtomicString& family,
                           DOMArrayBuffer* source,
                           const FontFaceDescriptors& descriptors) {
  FontFace* font_face = new FontFace(context, family, descriptors);
  font_face->InitCSSFontFace(static_cast<const unsigned char*>(source->Data()),
                             source->ByteLength());
  return font_face;
}

FontFace* FontFace::Create(ExecutionContext* context,
                           const AtomicString& family,
                           DOMArrayBufferView* source,
                           const FontFaceDescriptors& descriptors) {
  FontFace* font_face = new FontFace(context, family, descriptors);
  font_face->InitCSSFontFace(
      static_cast<const unsigned char*>(source->BaseAddress()),
      source->byteLength());
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

  FontFace* font_face = new FontFace(document);

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
      font_face->GetFontSelectionCapabilities().IsValid() &&
      !font_face->family().IsEmpty()) {
    font_face->InitCSSFontFace(document, *src);
    return font_face;
  }
  return nullptr;
}

FontFace::FontFace(ExecutionContext* context)
    : ContextClient(context), status_(kUnloaded) {}

FontFace::FontFace(ExecutionContext* context,
                   const AtomicString& family,
                   const FontFaceDescriptors& descriptors)
    : ContextClient(context), family_(family), status_(kUnloaded) {
  SetPropertyFromString(context, descriptors.style(),
                        AtRuleDescriptorID::FontStyle);
  SetPropertyFromString(context, descriptors.weight(),
                        AtRuleDescriptorID::FontWeight);
  SetPropertyFromString(context, descriptors.stretch(),
                        AtRuleDescriptorID::FontStretch);
  SetPropertyFromString(context, descriptors.unicodeRange(),
                        AtRuleDescriptorID::UnicodeRange);
  SetPropertyFromString(context, descriptors.variant(),
                        AtRuleDescriptorID::FontVariant);
  SetPropertyFromString(context, descriptors.featureSettings(),
                        AtRuleDescriptorID::FontFeatureSettings);
  SetPropertyFromString(context, descriptors.display(),
                        AtRuleDescriptorID::FontDisplay);
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
  if (exception_state)
    exception_state->ThrowDOMException(DOMExceptionCode::kSyntaxError, message);
  else
    SetError(DOMException::Create(DOMExceptionCode::kSyntaxError, message));
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
    default:
      NOTREACHED();
      return false;
  }
  return true;
}

bool FontFace::SetFamilyValue(const CSSValue& family_value) {
  AtomicString family;
  if (family_value.IsFontFamilyValue()) {
    family = AtomicString(ToCSSFontFamilyValue(family_value).Value());
  } else if (family_value.IsIdentifierValue()) {
    // We need to use the raw text for all the generic family types, since
    // @font-face is a way of actually defining what font to use for those
    // types.
    switch (ToCSSIdentifierValue(family_value).GetValueID()) {
      case CSSValueSerif:
        family = FontFamilyNames::webkit_serif;
        break;
      case CSSValueSansSerif:
        family = FontFamilyNames::webkit_sans_serif;
        break;
      case CSSValueCursive:
        family = FontFamilyNames::webkit_cursive;
        break;
      case CSSValueFantasy:
        family = FontFamilyNames::webkit_fantasy;
        break;
      case CSSValueMonospace:
        family = FontFamilyNames::webkit_monospace;
        break;
      case CSSValueWebkitPictograph:
        family = FontFamilyNames::webkit_pictograph;
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

  // When promises are resolved with 'thenables', instead of the object being
  // returned directly, the 'then' method is executed (the resolver tries to
  // resolve the thenable). This can lead to synchronous script execution, so we
  // post a task. This does not apply to promise rejection (i.e. a thenable
  // would be returned as is).
  if (status_ == kLoaded || status_ == kError) {
    if (loaded_property_) {
      if (status_ == kLoaded) {
        GetExecutionContext()
            ->GetTaskRunner(TaskType::kDOMManipulation)
            ->PostTask(FROM_HERE,
                       WTF::Bind(&LoadedProperty::Resolve<FontFace*>,
                                 WrapPersistent(loaded_property_.Get()),
                                 WrapPersistent(this)));
      } else
        loaded_property_->Reject(error_.Get());
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
    error_ =
        error ? error : DOMException::Create(DOMExceptionCode::kNetworkError);
  }
  SetLoadStatus(kError);
}

ScriptPromise FontFace::FontStatusPromise(ScriptState* script_state) {
  if (!loaded_property_) {
    loaded_property_ = new LoadedProperty(ExecutionContext::From(script_state),
                                          this, LoadedProperty::kLoaded);
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
    if (stretch_->IsIdentifierValue()) {
      switch (ToCSSIdentifierValue(stretch_.Get())->GetValueID()) {
        case CSSValueUltraCondensed:
          capabilities.width = {UltraCondensedWidthValue(),
                                UltraCondensedWidthValue()};
          break;
        case CSSValueExtraCondensed:
          capabilities.width = {ExtraCondensedWidthValue(),
                                ExtraCondensedWidthValue()};
          break;
        case CSSValueCondensed:
          capabilities.width = {CondensedWidthValue(), CondensedWidthValue()};
          break;
        case CSSValueSemiCondensed:
          capabilities.width = {SemiCondensedWidthValue(),
                                SemiCondensedWidthValue()};
          break;
        case CSSValueSemiExpanded:
          capabilities.width = {SemiExpandedWidthValue(),
                                SemiExpandedWidthValue()};
          break;
        case CSSValueExpanded:
          capabilities.width = {ExpandedWidthValue(), ExpandedWidthValue()};
          break;
        case CSSValueExtraExpanded:
          capabilities.width = {ExtraExpandedWidthValue(),
                                ExtraExpandedWidthValue()};
          break;
        case CSSValueUltraExpanded:
          capabilities.width = {UltraExpandedWidthValue(),
                                UltraExpandedWidthValue()};
          break;
        default:
          break;
      }
    } else if (stretch_->IsValueList()) {
      // Transition FontFace interpretation of parsed values from
      // CSSIdentifierValue to CSSValueList or CSSPrimitiveValue.
      // TODO(drott) crbug.com/739139: Update the parser to only produce
      // CSSPrimitiveValue or CSSValueList.
      const CSSValueList* stretch_list = ToCSSValueList(stretch_);
      if (stretch_list->length() != 2)
        return normal_capabilities;
      if (!stretch_list->Item(0).IsPrimitiveValue() ||
          !stretch_list->Item(1).IsPrimitiveValue())
        return normal_capabilities;
      const CSSPrimitiveValue* stretch_from =
          ToCSSPrimitiveValue(&stretch_list->Item(0));
      const CSSPrimitiveValue* stretch_to =
          ToCSSPrimitiveValue(&stretch_list->Item(1));
      if (!stretch_from->IsPercentage() || !stretch_to->IsPercentage())
        return normal_capabilities;
      capabilities.width = {FontSelectionValue(stretch_from->GetFloatValue()),
                            FontSelectionValue(stretch_to->GetFloatValue())};
    } else if (stretch_->IsPrimitiveValue()) {
      float stretch_value =
          ToCSSPrimitiveValue(stretch_.Get())->GetFloatValue();
      capabilities.width = {FontSelectionValue(stretch_value),
                            FontSelectionValue(stretch_value)};
    } else {
      NOTREACHED();
      return normal_capabilities;
    }
  }

  if (style_) {
    if (style_->IsIdentifierValue()) {
      switch (ToCSSIdentifierValue(style_.Get())->GetValueID()) {
        case CSSValueNormal:
          capabilities.slope = {NormalSlopeValue(), NormalSlopeValue()};
          break;
        case CSSValueOblique:
          capabilities.slope = {ItalicSlopeValue(), ItalicSlopeValue()};
          break;
        case CSSValueItalic:
          capabilities.slope = {ItalicSlopeValue(), ItalicSlopeValue()};
          break;
        default:
          break;
      }
    } else if (style_->IsFontStyleRangeValue()) {
      const cssvalue::CSSFontStyleRangeValue* range_value =
          cssvalue::ToCSSFontStyleRangeValue(style_);
      if (range_value->GetFontStyleValue()->IsIdentifierValue()) {
        CSSValueID font_style_id =
            range_value->GetFontStyleValue()->GetValueID();
        if (!range_value->GetObliqueValues()) {
          if (font_style_id == CSSValueNormal)
            capabilities.slope = {NormalSlopeValue(), NormalSlopeValue()};
          DCHECK(font_style_id == CSSValueItalic ||
                 font_style_id == CSSValueOblique);
          capabilities.slope = {ItalicSlopeValue(), ItalicSlopeValue()};
        } else {
          DCHECK(font_style_id == CSSValueOblique);
          size_t oblique_values_size =
              range_value->GetObliqueValues()->length();
          if (oblique_values_size == 1) {
            const CSSPrimitiveValue& range_start =
                ToCSSPrimitiveValue(range_value->GetObliqueValues()->Item(0));
            FontSelectionValue oblique_range(range_start.GetFloatValue());
            capabilities.slope = {oblique_range, oblique_range};
          } else {
            DCHECK_EQ(oblique_values_size, 2u);
            const CSSPrimitiveValue& range_start =
                ToCSSPrimitiveValue(range_value->GetObliqueValues()->Item(0));
            const CSSPrimitiveValue& range_end =
                ToCSSPrimitiveValue(range_value->GetObliqueValues()->Item(1));
            capabilities.slope = {
                FontSelectionValue(range_start.GetFloatValue()),
                FontSelectionValue(range_end.GetFloatValue())};
          }
        }
      }
    } else {
      NOTREACHED();
      return normal_capabilities;
    }
  }

  if (weight_) {
    if (weight_->IsIdentifierValue()) {
      switch (ToCSSIdentifierValue(weight_.Get())->GetValueID()) {
        // Although 'lighter' and 'bolder' are valid keywords for
        // font-weights, they are invalid inside font-face rules so they are
        // ignored. Reference:
        // http://www.w3.org/TR/css3-fonts/#descdef-font-weight.
        case CSSValueLighter:
        case CSSValueBolder:
          break;
        case CSSValueNormal:
          capabilities.weight = {NormalWeightValue(), NormalWeightValue()};
          break;
        case CSSValueBold:
          capabilities.weight = {BoldWeightValue(), BoldWeightValue()};
          break;
        default:
          NOTREACHED();
          break;
      }
    } else if (weight_->IsValueList()) {
      const CSSValueList* weight_list = ToCSSValueList(weight_);
      if (weight_list->length() != 2)
        return normal_capabilities;
      if (!weight_list->Item(0).IsPrimitiveValue() ||
          !weight_list->Item(1).IsPrimitiveValue())
        return normal_capabilities;
      const CSSPrimitiveValue* weight_from =
          ToCSSPrimitiveValue(&weight_list->Item(0));
      const CSSPrimitiveValue* weight_to =
          ToCSSPrimitiveValue(&weight_list->Item(1));
      if (!weight_from->IsNumber() || !weight_to->IsNumber() ||
          weight_from->GetFloatValue() < 1 || weight_to->GetFloatValue() > 1000)
        return normal_capabilities;
      capabilities.weight = {FontSelectionValue(weight_from->GetFloatValue()),
                             FontSelectionValue(weight_to->GetFloatValue())};
    } else if (weight_->IsPrimitiveValue()) {
      float weight_value = ToCSSPrimitiveValue(weight_.Get())->GetFloatValue();
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
  if (const Document* document = DynamicTo<Document>(context)) {
    const Settings* settings = document->GetSettings();
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
  DCHECK(src.IsValueList());
  const CSSValueList& src_list = ToCSSValueList(src);
  int src_length = src_list.length();

  for (int i = 0; i < src_length; i++) {
    // An item in the list either specifies a string (local font name) or a URL
    // (remote font to download).
    const CSSFontFaceSrcValue& item = ToCSSFontFaceSrcValue(src_list.Item(i));

    if (!item.IsLocal()) {
      if (ContextAllowsDownload(context) && item.IsSupportedFormat()) {
        FontSelector* font_selector = nullptr;
        if (auto* document = DynamicTo<Document>(context)) {
          font_selector = document->GetStyleEngine().GetFontSelector();
        } else if (auto* scope = DynamicTo<WorkerGlobalScope>(context)) {
          font_selector = scope->GetFontSelector();
        } else {
          NOTREACHED();
        }
        RemoteFontFaceSource* source =
            new RemoteFontFaceSource(css_font_face_, font_selector,
                                     CSSValueToFontDisplay(display_.Get()));
        item.Fetch(context, source);
        css_font_face_->AddSource(source);
      }
    } else {
      css_font_face_->AddSource(new LocalFontFaceSource(item.GetResource()));
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
      new BinaryDataFontFaceSource(buffer.get(), ots_parse_message_);
  if (source->IsValid())
    SetLoadStatus(kLoaded);
  else
    SetError(DOMException::Create(DOMExceptionCode::kSyntaxError,
                                  "Invalid font data in ArrayBuffer."));
  css_font_face_->AddSource(source);
}

void FontFace::Trace(blink::Visitor* visitor) {
  visitor->Trace(style_);
  visitor->Trace(weight_);
  visitor->Trace(stretch_);
  visitor->Trace(unicode_range_);
  visitor->Trace(variant_);
  visitor->Trace(feature_settings_);
  visitor->Trace(display_);
  visitor->Trace(error_);
  visitor->Trace(loaded_property_);
  visitor->Trace(css_font_face_);
  visitor->Trace(callbacks_);
  ScriptWrappable::Trace(visitor);
  ContextClient::Trace(visitor);
}

bool FontFace::HadBlankText() const {
  return css_font_face_->HadBlankText();
}

bool FontFace::HasPendingActivity() const {
  return status_ == kLoading && GetExecutionContext();
}

}  // namespace blink
