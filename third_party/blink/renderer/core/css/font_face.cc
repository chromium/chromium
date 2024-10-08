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

#include "base/metrics/histogram_macros.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_font_face_descriptors.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_font_face_load_status.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_arraybuffer_arraybufferview_string.h"
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
#include "third_party/blink/renderer/core/css/css_value_pair.h"
#include "third_party/blink/renderer/core/css/local_font_face_source.h"
#include "third_party/blink/renderer/core/css/media_values.h"
#include "third_party/blink/renderer/core/css/media_values_cached.h"
#include "third_party/blink/renderer/core/css/media_values_dynamic.h"
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
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/loader/render_blocking_resource_manager.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer_view.h"
#include "third_party/blink/renderer/core/workers/worker_global_scope.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/font_family_names.h"
#include "third_party/blink/renderer/platform/fonts/font_metrics_override.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
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
  HeapVector<UnicodeRange> ranges;
  if (const auto* range_list = To<CSSValueList>(unicode_range)) {
    unsigned num_ranges = range_list->length();
    for (unsigned i = 0; i < num_ranges; i++) {
      const auto& range =
          To<cssvalue::CSSUnicodeRangeValue>(range_list->Item(i));
      ranges.push_back(UnicodeRange(range.From(), range.To()));
    }
  }

  return MakeGarbageCollected<CSSFontFace>(font_face, std::move(ranges));
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

const CSSValue* ConvertSizeAdjustValue(const CSSValue* parsed_value) {
  // We store the initial value 100% as nullptr
  if (parsed_value && To<CSSPrimitiveValue>(parsed_value)->IsHundred() ==
                          CSSPrimitiveValue::BoolStatus::kTrue) {
    return nullptr;
  }
  return parsed_value;
}

}  // namespace

FontFace* FontFace::Create(
    ExecutionContext* execution_context,
    const AtomicString& family,
    const V8UnionArrayBufferOrArrayBufferViewOrString* source,
    const FontFaceDescriptors* descriptors) {
  DCHECK(source);

  switch (source->GetContentType()) {
    case V8UnionArrayBufferOrArrayBufferViewOrString::ContentType::kArrayBuffer:
      return Create(execution_context, family, source->GetAsArrayBuffer(),
                    descriptors);
    case V8UnionArrayBufferOrArrayBufferViewOrString::ContentType::
        kArrayBufferView:
      return Create(execution_context, family,
                    source->GetAsArrayBufferView().Get(), descriptors);
    case V8UnionArrayBufferOrArrayBufferViewOrString::ContentType::kString:
      return Create(execution_context, family, source->GetAsString(),
                    descriptors);
  }

  NOTREACHED_IN_MIGRATION();
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
  font_face->InitCSSFontFace(context,
                             static_cast<const unsigned char*>(source->Data()),
                             source->ByteLength());
  return font_face;
}

FontFace* FontFace::Create(ExecutionContext* context,
                           const AtomicString& family,
                           DOMArrayBufferView* source,
                           const FontFaceDescriptors* descriptors) {
  FontFace* font_face =
      MakeGarbageCollected<FontFace>(context, family, descriptors);
  font_face->InitCSSFontFace(
      context, static_cast<const unsigned char*>(source->BaseAddress()),
      source->byteLength());
  return font_face;
}

FontFace* FontFace::Create(Document* document,
                           const StyleRuleFontFace* font_face_rule,
                           bool is_user_style) {
  const CSSPropertyValueSet& properties = font_face_rule->Properties();

  // Obtain the font-family property and the src property. Both must be defined.
  auto* family = DynamicTo<CSSFontFamilyValue>(
      properties.GetPropertyCSSValue(AtRuleDescriptorID::FontFamily));
  if (!family) {
    return nullptr;
  }
  const CSSValue* src = properties.GetPropertyCSSValue(AtRuleDescriptorID::Src);
  if (!src || !src->IsValueList()) {
    return nullptr;
  }

  FontFace* font_face = MakeGarbageCollected<FontFace>(
      document->GetExecutionContext(), font_face_rule, is_user_style);
  font_face->SetFamilyValue(*family);

  if (font_face->SetPropertyFromStyle(properties,
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
                                      AtRuleDescriptorID::SizeAdjust) &&
      font_face->GetFontSelectionCapabilities().IsValid()) {
    font_face->InitCSSFontFace(document->GetExecutionContext(), *src);
    return font_face;
  }
  return nullptr;
}

FontFace::FontFace(ExecutionContext* context,
                   const StyleRuleFontFace* style_rule,
                   bool is_user_style)
    : ActiveScriptWrappable<FontFace>({}),
      ExecutionContextClient(context),
      style_rule_(style_rule),
      status_(kUnloaded),
      is_user_style_(is_user_style) {}

FontFace::FontFace(ExecutionContext* context,
                   const AtomicString& family,
                   const FontFaceDescriptors* descriptors)
    : ActiveScriptWrappable<FontFace>({}),
      ExecutionContextClient(context),
      family_(family),
      status_(kUnloaded) {
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
  SetPropertyFromString(context, descriptors->ascentOverride(),
                        AtRuleDescriptorID::AscentOverride);
  SetPropertyFromString(context, descriptors->descentOverride(),
                        AtRuleDescriptorID::DescentOverride);
  SetPropertyFromString(context, descriptors->lineGapOverride(),
                        AtRuleDescriptorID::LineGapOverride);
  SetPropertyFromString(context, descriptors->sizeAdjust(),
                        AtRuleDescriptorID::SizeAdjust);
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

String FontFace::ascentOverride() const {
  return ascent_override_ ? ascent_override_->CssText() : "normal";
}

String FontFace::descentOverride() const {
  return descent_override_ ? descent_override_->CssText() : "normal";
}

String FontFace::lineGapOverride() const {
  return line_gap_override_ ? line_gap_override_->CssText() : "normal";
}

String FontFace::sizeAdjust() const {
  return size_adjust_ ? size_adjust_->CssText() : "100%";
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

void FontFace::setAscentOverride(ExecutionContext* context,
                                 const String& s,
                                 ExceptionState& exception_state) {
  SetPropertyFromString(context, s, AtRuleDescriptorID::AscentOverride,
                        &exception_state);
}

void FontFace::setDescentOverride(ExecutionContext* context,
                                  const String& s,
                                  ExceptionState& exception_state) {
  SetPropertyFromString(context, s, AtRuleDescriptorID::DescentOverride,
                        &exception_state);
}

void FontFace::setLineGapOverride(ExecutionContext* context,
                                  const String& s,
                                  ExceptionState& exception_state) {
  SetPropertyFromString(context, s, AtRuleDescriptorID::LineGapOverride,
                        &exception_state);
}

void FontFace::setSizeAdjust(ExecutionContext* context,
                             const String& s,
                             ExceptionState& exception_state) {
  SetPropertyFromString(context, s, AtRuleDescriptorID::SizeAdjust,
                        &exception_state);
}

void FontFace::SetPropertyFromString(const ExecutionContext* context,
                                     const String& s,
                                     AtRuleDescriptorID descriptor_id,
                                     ExceptionState* exception_state) {
  const CSSValue* value = ParseCSSValue(context, s, descriptor_id);
  if (value && SetPropertyValue(value, descriptor_id)) {
    return;
  }

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
      if (value && !value->IsValueList()) {
        return false;
      }
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
      if (css_font_face_) {
        css_font_face_->SetDisplay(CSSValueToFontDisplay(display_.Get()));
      }
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
    case AtRuleDescriptorID::SizeAdjust:
      size_adjust_ = ConvertSizeAdjustValue(value);
      break;
    default:
      NOTREACHED_IN_MIGRATION();
      return false;
  }
  return true;
}

void FontFace::SetFamilyValue(const CSSFontFamilyValue& family_value) {
  family_ = family_value.Value();
}

V8FontFaceLoadStatus FontFace::status() const {
  switch (status_) {
    case kUnloaded:
      return V8FontFaceLoadStatus(V8FontFaceLoadStatus::Enum::kUnloaded);
    case kLoading:
      return V8FontFaceLoadStatus(V8FontFaceLoadStatus::Enum::kLoading);
    case kLoaded:
      return V8FontFaceLoadStatus(V8FontFaceLoadStatus::Enum::kLoaded);
    case kError:
      return V8FontFaceLoadStatus(V8FontFaceLoadStatus::Enum::kError);
  }
  NOTREACHED();
}

void FontFace::SetLoadStatus(LoadStatusType status) {
  status_ = status;
  DCHECK(status_ != kError || error_);

  if (!GetExecutionContext()) {
    return;
  }

  if (status_ == kLoaded || status_ == kError) {
    if (loaded_property_) {
      if (status_ == kLoaded) {
        GetExecutionContext()
            ->GetTaskRunner(TaskType::kDOMManipulation)
            ->PostTask(FROM_HERE,
                       WTF::BindOnce(&LoadedProperty::Resolve<FontFace*>,
                                     WrapPersistent(loaded_property_.Get()),
                                     WrapPersistent(this)));
      } else {
        GetExecutionContext()
            ->GetTaskRunner(TaskType::kDOMManipulation)
            ->PostTask(FROM_HERE,
                       WTF::BindOnce(&LoadedProperty::Reject<DOMException*>,
                                     WrapPersistent(loaded_property_.Get()),
                                     WrapPersistent(error_.Get())));
      }
    }

    GetExecutionContext()
        ->GetTaskRunner(TaskType::kDOMManipulation)
        ->PostTask(FROM_HERE, WTF::BindOnce(&FontFace::RunCallbacks,
                                            WrapPersistent(this)));
  }
}

void FontFace::RunCallbacks() {
  HeapVector<Member<LoadFontCallback>> callbacks;
  callbacks_.swap(callbacks);
  for (wtf_size_t i = 0; i < callbacks.size(); ++i) {
    if (status_ == kLoaded) {
      callbacks[i]->NotifyLoaded(this);
    } else {
      callbacks[i]->NotifyError(this);
    }
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

ScriptPromise<FontFace> FontFace::FontStatusPromise(ScriptState* script_state) {
  if (!loaded_property_) {
    loaded_property_ = MakeGarbageCollected<LoadedProperty>(
        ExecutionContext::From(script_state));
    if (status_ == kLoaded) {
      loaded_property_->Resolve(this);
    } else if (status_ == kError) {
      loaded_property_->Reject(error_.Get());
    }
  }
  return loaded_property_->Promise(script_state->World());
}

ScriptPromise<FontFace> FontFace::load(ScriptState* script_state) {
  if (status_ == kUnloaded) {
    css_font_face_->Load();
  }
  DidBeginImperativeLoad();
  return FontStatusPromise(script_state);
}

void FontFace::LoadWithCallback(LoadFontCallback* callback) {
  if (status_ == kUnloaded) {
    css_font_face_->Load();
  }
  AddCallback(callback);
}

void FontFace::AddCallback(LoadFontCallback* callback) {
  if (status_ == kLoaded) {
    callback->NotifyLoaded(this);
  } else if (status_ == kError) {
    callback->NotifyError(this);
  } else {
    callbacks_.push_back(callback);
  }
}

FontSelectionCapabilities FontFace::GetFontSelectionCapabilities() const {
  // FontSelectionCapabilities represents a range of available width, slope and
  // weight values. The first value of each pair is the minimum value, the
  // second is the maximum value.
  FontSelectionCapabilities normal_capabilities(
      {kNormalWidthValue, kNormalWidthValue},
      {kNormalSlopeValue, kNormalSlopeValue},
      {kNormalWeightValue, kNormalWeightValue});
  FontSelectionCapabilities capabilities(normal_capabilities);

  if (stretch_) {
    if (auto* stretch_identifier_value =
            DynamicTo<CSSIdentifierValue>(stretch_.Get())) {
      switch (stretch_identifier_value->GetValueID()) {
        case CSSValueID::kUltraCondensed:
          capabilities.width = {kUltraCondensedWidthValue,
                                kUltraCondensedWidthValue,
                                FontSelectionRange::RangeType::kSetExplicitly};
          break;
        case CSSValueID::kExtraCondensed:
          capabilities.width = {kExtraCondensedWidthValue,
                                kExtraCondensedWidthValue,
                                FontSelectionRange::RangeType::kSetExplicitly};
          break;
        case CSSValueID::kCondensed:
          capabilities.width = {kCondensedWidthValue, kCondensedWidthValue,
                                FontSelectionRange::RangeType::kSetExplicitly};
          break;
        case CSSValueID::kSemiCondensed:
          capabilities.width = {kSemiCondensedWidthValue,
                                kSemiCondensedWidthValue,
                                FontSelectionRange::RangeType::kSetExplicitly};
          break;
        case CSSValueID::kSemiExpanded:
          capabilities.width = {kSemiExpandedWidthValue,
                                kSemiExpandedWidthValue,
                                FontSelectionRange::RangeType::kSetExplicitly};
          break;
        case CSSValueID::kExpanded:
          capabilities.width = {kExpandedWidthValue, kExpandedWidthValue,
                                FontSelectionRange::RangeType::kSetExplicitly};
          break;
        case CSSValueID::kExtraExpanded:
          capabilities.width = {kExtraExpandedWidthValue,
                                kExtraExpandedWidthValue,
                                FontSelectionRange::RangeType::kSetExplicitly};
          break;
        case CSSValueID::kUltraExpanded:
          capabilities.width = {kUltraExpandedWidthValue,
                                kUltraExpandedWidthValue,
                                FontSelectionRange::RangeType::kSetExplicitly};
          break;
        case CSSValueID::kAuto:
          capabilities.width = {kNormalWidthValue, kNormalWidthValue,
                                FontSelectionRange::RangeType::kSetFromAuto};
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
      if (stretch_list->length() != 2) {
        return normal_capabilities;
      }
      const auto* stretch_from =
          DynamicTo<CSSPrimitiveValue>(&stretch_list->Item(0));
      const auto* stretch_to =
          DynamicTo<CSSPrimitiveValue>(&stretch_list->Item(1));
      if (!stretch_from || !stretch_to) {
        return normal_capabilities;
      }
      if (!stretch_from->IsPercentage() || !stretch_to->IsPercentage()) {
        return normal_capabilities;
      }
      // https://drafts.csswg.org/css-fonts/#font-prop-desc
      // "User agents must swap the computed value of the startpoint and
      // endpoint of the range in order to forbid decreasing ranges."
      if (stretch_from->ComputeValueInCanonicalUnit(EnsureLengthResolver()) <
          stretch_to->ComputeValueInCanonicalUnit(EnsureLengthResolver())) {
        capabilities.width = {
            FontSelectionValue(stretch_from->ComputeValueInCanonicalUnit(
                EnsureLengthResolver())),
            FontSelectionValue(stretch_to->ComputeValueInCanonicalUnit(
                EnsureLengthResolver())),
            FontSelectionRange::RangeType::kSetExplicitly};
      } else {
        capabilities.width = {
            FontSelectionValue(stretch_to->ComputeValueInCanonicalUnit(
                EnsureLengthResolver())),
            FontSelectionValue(stretch_from->ComputeValueInCanonicalUnit(
                EnsureLengthResolver())),
            FontSelectionRange::RangeType::kSetExplicitly};
      }
    } else if (auto* stretch_primitive_value =
                   DynamicTo<CSSPrimitiveValue>(stretch_.Get())) {
      float stretch_value =
          stretch_primitive_value->ComputeValueInCanonicalUnit(
              EnsureLengthResolver());
      capabilities.width = {FontSelectionValue(stretch_value),
                            FontSelectionValue(stretch_value),
                            FontSelectionRange::RangeType::kSetExplicitly};
    } else {
      NOTREACHED_IN_MIGRATION();
      return normal_capabilities;
    }
  }

  if (style_) {
    if (auto* identifier_value = DynamicTo<CSSIdentifierValue>(style_.Get())) {
      switch (identifier_value->GetValueID()) {
        case CSSValueID::kNormal:
          capabilities.slope = {kNormalSlopeValue, kNormalSlopeValue,
                                FontSelectionRange::RangeType::kSetExplicitly};
          break;
        case CSSValueID::kOblique:
          capabilities.slope = {kItalicSlopeValue, kItalicSlopeValue,
                                FontSelectionRange::RangeType::kSetExplicitly};
          break;
        case CSSValueID::kItalic:
          capabilities.slope = {kItalicSlopeValue, kItalicSlopeValue,
                                FontSelectionRange::RangeType::kSetExplicitly};
          break;
        case CSSValueID::kAuto:
          capabilities.slope = {kNormalSlopeValue, kNormalSlopeValue,
                                FontSelectionRange::RangeType::kSetFromAuto};
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
          if (font_style_id == CSSValueID::kNormal) {
            capabilities.slope = {
                kNormalSlopeValue, kNormalSlopeValue,
                FontSelectionRange::RangeType::kSetExplicitly};
          }
          DCHECK(font_style_id == CSSValueID::kItalic ||
                 font_style_id == CSSValueID::kOblique);
          capabilities.slope = {kItalicSlopeValue, kItalicSlopeValue,
                                FontSelectionRange::RangeType::kSetExplicitly};
        } else {
          DCHECK(font_style_id == CSSValueID::kOblique);
          size_t oblique_values_size =
              range_value->GetObliqueValues()->length();
          if (oblique_values_size == 1) {
            const auto& range_start =
                To<CSSPrimitiveValue>(range_value->GetObliqueValues()->Item(0));
            FontSelectionValue oblique_range(
                range_start.ComputeValueInCanonicalUnit(
                    EnsureLengthResolver()));
            capabilities.slope = {
                oblique_range, oblique_range,
                FontSelectionRange::RangeType::kSetExplicitly};
          } else {
            DCHECK_EQ(oblique_values_size, 2u);
            const auto& range_start =
                To<CSSPrimitiveValue>(range_value->GetObliqueValues()->Item(0));
            const auto& range_end =
                To<CSSPrimitiveValue>(range_value->GetObliqueValues()->Item(1));
            // https://drafts.csswg.org/css-fonts/#font-prop-desc
            // "User agents must swap the computed value of the startpoint and
            // endpoint of the range in order to forbid decreasing ranges."
            if (range_start.ComputeValueInCanonicalUnit(
                    EnsureLengthResolver()) <
                range_end.ComputeValueInCanonicalUnit(EnsureLengthResolver())) {
              capabilities.slope = {
                  FontSelectionValue(range_start.ComputeValueInCanonicalUnit(
                      EnsureLengthResolver())),
                  FontSelectionValue(range_end.ComputeValueInCanonicalUnit(
                      EnsureLengthResolver())),
                  FontSelectionRange::RangeType::kSetExplicitly};
            } else {
              capabilities.slope = {
                  FontSelectionValue(range_end.ComputeValueInCanonicalUnit(
                      EnsureLengthResolver())),
                  FontSelectionValue(range_start.ComputeValueInCanonicalUnit(
                      EnsureLengthResolver())),
                  FontSelectionRange::RangeType::kSetExplicitly};
            }
          }
        }
      }
    } else {
      NOTREACHED_IN_MIGRATION();
      return normal_capabilities;
    }
  }

  if (weight_) {
    if (auto* identifier_value = DynamicTo<CSSIdentifierValue>(weight_.Get())) {
      switch (identifier_value->GetValueID()) {
        case CSSValueID::kNormal:
          capabilities.weight = {kNormalWeightValue, kNormalWeightValue,
                                 FontSelectionRange::RangeType::kSetExplicitly};
          break;
        case CSSValueID::kBold:
          capabilities.weight = {kBoldWeightValue, kBoldWeightValue,
                                 FontSelectionRange::RangeType::kSetExplicitly};
          break;
        case CSSValueID::kAuto:
          capabilities.weight = {kNormalWeightValue, kNormalWeightValue,
                                 FontSelectionRange::RangeType::kSetFromAuto};
          break;
        default:
          NOTREACHED_IN_MIGRATION();
          break;
      }
    } else if (const auto* weight_list =
                   DynamicTo<CSSValueList>(weight_.Get())) {
      if (weight_list->length() != 2) {
        return normal_capabilities;
      }
      const auto* weight_from =
          DynamicTo<CSSPrimitiveValue>(&weight_list->Item(0));
      const auto* weight_to =
          DynamicTo<CSSPrimitiveValue>(&weight_list->Item(1));
      if (!weight_from || !weight_to) {
        return normal_capabilities;
      }
      if (!weight_from->IsNumber() || !weight_to->IsNumber() ||
          weight_from->ComputeValueInCanonicalUnit(EnsureLengthResolver()) <
              1 ||
          weight_to->ComputeValueInCanonicalUnit(EnsureLengthResolver()) >
              1000) {
        return normal_capabilities;
      }
      // https://drafts.csswg.org/css-fonts/#font-prop-desc
      // "User agents must swap the computed value of the startpoint and
      // endpoint of the range in order to forbid decreasing ranges."
      if (weight_from->ComputeValueInCanonicalUnit(EnsureLengthResolver()) <
          weight_to->ComputeValueInCanonicalUnit(EnsureLengthResolver())) {
        capabilities.weight = {
            FontSelectionValue(weight_from->ComputeValueInCanonicalUnit(
                EnsureLengthResolver())),
            FontSelectionValue(
                weight_to->ComputeValueInCanonicalUnit(EnsureLengthResolver())),
            FontSelectionRange::RangeType::kSetExplicitly};
      } else {
        capabilities.weight = {
            FontSelectionValue(
                weight_to->ComputeValueInCanonicalUnit(EnsureLengthResolver())),
            FontSelectionValue(weight_from->ComputeValueInCanonicalUnit(
                EnsureLengthResolver())),
            FontSelectionRange::RangeType::kSetExplicitly};
      }
    } else if (auto* weight_primitive_value =
                   DynamicTo<CSSPrimitiveValue>(weight_.Get())) {
      float weight_value = weight_primitive_value->ComputeValueInCanonicalUnit(
          EnsureLengthResolver());
      if (weight_value < 1 || weight_value > 1000) {
        return normal_capabilities;
      }
      capabilities.weight = {FontSelectionValue(weight_value),
                             FontSelectionValue(weight_value),
                             FontSelectionRange::RangeType::kSetExplicitly};
    } else {
      NOTREACHED_IN_MIGRATION();
      return normal_capabilities;
    }
  }

  return capabilities;
}

size_t FontFace::ApproximateBlankCharacterCount() const {
  if (status_ == kLoading) {
    return css_font_face_->ApproximateBlankCharacterCount();
  }
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
  if (error_) {
    return;
  }

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
      NOTREACHED_IN_MIGRATION();
    }
    if (!item.IsLocal()) {
      if (ContextAllowsDownload(context) && item.IsSupportedFormat()) {
        RemoteFontFaceSource* source =
            MakeGarbageCollected<RemoteFontFaceSource>(
                css_font_face_, font_selector,
                CSSValueToFontDisplay(display_.Get()),
                context->GetTaskRunner(TaskType::kFontLoading));
        item.Fetch(context, source);
        css_font_face_->AddSource(source);
      }
    } else {
      css_font_face_->AddSource(MakeGarbageCollected<LocalFontFaceSource>(
          css_font_face_, font_selector, item.LocalResource()));
    }
  }
}

void FontFace::InitCSSFontFace(ExecutionContext* context,
                               const unsigned char* data,
                               size_t size) {
  css_font_face_ = CreateCSSFontFace(this, unicode_range_.Get());
  if (error_) {
    return;
  }

  scoped_refptr<SharedBuffer> buffer = SharedBuffer::Create(data, size);
  BinaryDataFontFaceSource* source =
      MakeGarbageCollected<BinaryDataFontFaceSource>(
          css_font_face_, buffer.get(), ots_parse_message_);
  if (source->IsValid()) {
    SetLoadStatus(kLoaded);
  } else {
    if (!ots_parse_message_.empty()) {
      context->AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
          mojom::blink::ConsoleMessageSource::kOther,
          mojom::blink::ConsoleMessageLevel::kWarning,
          "OTS parsing error: " + ots_parse_message_));
    }
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
  visitor->Trace(size_adjust_);
  visitor->Trace(error_);
  visitor->Trace(loaded_property_);
  visitor->Trace(css_font_face_);
  visitor->Trace(callbacks_);
  visitor->Trace(style_rule_);
  visitor->Trace(media_values_);
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
  if (!DomWindow() ||
      !DomWindow()->document()->GetRenderBlockingResourceManager()) {
    return;
  }
  DomWindow()
      ->document()
      ->GetRenderBlockingResourceManager()
      ->AddImperativeFontLoading(this);
}

FontMetricsOverride FontFace::GetFontMetricsOverride() const {
  FontMetricsOverride result;
  if (ascent_override_) {
    result.ascent_override =
        To<CSSPrimitiveValue>(*ascent_override_)
            .ComputeValueInCanonicalUnit(EnsureLengthResolver()) /
        100;
  }
  if (descent_override_) {
    result.descent_override =
        To<CSSPrimitiveValue>(*descent_override_)
            .ComputeValueInCanonicalUnit(EnsureLengthResolver()) /
        100;
  }
  if (line_gap_override_) {
    result.line_gap_override =
        To<CSSPrimitiveValue>(*line_gap_override_)
            .ComputeValueInCanonicalUnit(EnsureLengthResolver()) /
        100;
  }
  return result;
}

float FontFace::GetSizeAdjust() const {
  DCHECK(size_adjust_);
  return To<CSSPrimitiveValue>(*size_adjust_)
             .ComputeValueInCanonicalUnit(EnsureLengthResolver()) /
         100;
}

Document* FontFace::GetDocument() const {
  auto* window = DynamicTo<LocalDOMWindow>(GetExecutionContext());
  return window ? window->document() : nullptr;
}

const CSSLengthResolver& FontFace::EnsureLengthResolver() const {
  if (!media_values_) {
    Document* document = GetDocument();
    media_values_ = document ? MediaValuesDynamic::Create(*document)
                             : MakeGarbageCollected<MediaValuesCached>();
  }
  return *media_values_;
}

}  // namespace blink
