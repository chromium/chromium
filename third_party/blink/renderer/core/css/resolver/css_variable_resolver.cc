// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/resolver/css_variable_resolver.h"

#include "third_party/blink/renderer/core/css/css_custom_property_declaration.h"
#include "third_party/blink/renderer/core/css/css_pending_substitution_value.h"
#include "third_party/blink/renderer/core/css/css_unset_value.h"
#include "third_party/blink/renderer/core/css/css_variable_data.h"
#include "third_party/blink/renderer/core/css/css_variable_reference_value.h"
#include "third_party/blink/renderer/core/css/document_style_environment_variables.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_token.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_token_range.h"
#include "third_party/blink/renderer/core/css/parser/css_property_parser.h"
#include "third_party/blink/renderer/core/css/parser/css_tokenizer.h"
#include "third_party/blink/renderer/core/css/property_registry.h"
#include "third_party/blink/renderer/core/css/resolver/style_builder.h"
#include "third_party/blink/renderer/core/css/resolver/style_builder_converter.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver_state.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver_stats.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/css_property_names.h"
#include "third_party/blink/renderer/core/css_value_keywords.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/style/style_inherited_variables.h"
#include "third_party/blink/renderer/core/style/style_non_inherited_variables.h"
#include "third_party/blink/renderer/core/style_property_shorthand.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/text/text_encoding.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

namespace {

CSSParserToken ResolveUrl(const CSSParserToken& token,
                          Vector<String>& backing_strings,
                          const KURL& base_url,
                          WTF::TextEncoding charset) {
  DCHECK(token.GetType() == kUrlToken || token.GetType() == kStringToken);

  StringView string_view = token.Value();

  if (string_view.IsNull())
    return token;

  String relative_url = string_view.ToString();
  KURL absolute_url = charset.IsValid() ? KURL(base_url, relative_url, charset)
                                        : KURL(base_url, relative_url);

  backing_strings.push_back(absolute_url.GetString());

  return token.CopyWithUpdatedString(StringView(backing_strings.back()));
}

// Registered properties need to substitute as absolute values. This means
// that 'em' units (for instance) are converted to 'px ' and calc()-expressions
// are resolved. This function creates new tokens equivalent to the computed
// value of the registered property.
//
// This is necessary to make things like font-relative units in inherited
// (and registered) custom properties work correctly.
scoped_refptr<CSSVariableData> ComputedVariableData(
    const CSSVariableData& variable_data,
    const CSSValue& computed_value) {
  String text = computed_value.CssText();

  CSSTokenizer tokenizer(text);
  Vector<CSSParserToken> tokens;
  tokens.AppendVector(tokenizer.TokenizeToEOF());

  Vector<String> backing_strings;
  backing_strings.push_back(text);

  const bool has_font_units = false;
  const bool has_root_font_units = false;
  const bool absolutized = true;

  return CSSVariableData::CreateResolved(
      tokens, std::move(backing_strings), variable_data.IsAnimationTainted(),
      has_font_units, has_root_font_units, absolutized);
}

}  // namespace

bool CSSVariableResolver::ResolveFallback(CSSParserTokenRange range,
                                          const Options& options,
                                          Result& result) {
  if (range.AtEnd())
    return false;
  DCHECK_EQ(range.Peek().GetType(), kCommaToken);
  range.Consume();
  return ResolveTokenRange(range, options, result);
}

scoped_refptr<CSSVariableData> CSSVariableResolver::ValueForCustomProperty(
    AtomicString name,
    const Options& options) {
  if (variables_seen_.Contains(name)) {
    cycle_start_points_.insert(name);
    return nullptr;
  }

  DCHECK(registry_ || !RuntimeEnabledFeatures::CSSVariables2Enabled());
  const PropertyRegistration* registration =
      registry_ ? registry_->Registration(name) : nullptr;

  CSSVariableData* variable_data = GetVariable(name, registration);

  if (!variable_data)
    return registration ? registration->InitialVariableData() : nullptr;

  scoped_refptr<CSSVariableData> resolved_data =
      ResolveCustomPropertyIfNeeded(name, variable_data, options);

  if (resolved_data) {
    if (IsVariableDisallowed(*resolved_data, options, registration))
      return nullptr;
  }

  if (!registration) {
    if (resolved_data != variable_data && options.absolutize)
      SetVariable(name, registration, resolved_data);
    return resolved_data;
  }

  const CSSValue* value = GetRegisteredVariable(name, *registration);
  const CSSValue* resolved_value = value;

  // The computed value of a registered property must be stored as a CSSValue
  // on ComputedStyle. If we have resolved this custom property before, we
  // may already have a CSSValue. If not, we must produce that value now.
  if (!resolved_value && resolved_data) {
    resolved_value = resolved_data->ParseForSyntax(
        registration->Syntax(), state_.GetDocument().GetSecureContextMode());
    if (!resolved_value) {
      // Parsing failed. Set resolved_data to nullptr to indicate that we
      // currently don't have a token stream matching the registered syntax.
      resolved_data = nullptr;
    }
  }

  // If either parsing or resolution failed, and this property inherits,
  // take inherited values instead of falling back on initial.
  if (registration->Inherits() && !resolved_data) {
    resolved_data = state_.ParentStyle()->GetVariable(name, true);
    resolved_value = state_.ParentStyle()->GetRegisteredVariable(name, true);
  }

  DCHECK(!!resolved_data == !!resolved_value);

  // If so enabled by options, and the resolved_data isn't absolutized already
  // (which is the case when resolved_data was inherited from the parent style),
  // perform absolutization now.
  if (options.absolutize) {
    if (resolved_value && !resolved_data->IsAbsolutized()) {
      resolved_value = &StyleBuilderConverter::ConvertRegisteredPropertyValue(
          state_, *resolved_value);
      resolved_data =
          ComputedVariableData(*resolved_data.get(), *resolved_value);
    }
    if (resolved_data != variable_data)
      SetVariable(name, registration, resolved_data);
  }

  // Even if options.absolutize=false, we store the resolved_value if we
  // parsed it. This is required to calculate the animations update before
  // the absolutization pass.
  if (value != resolved_value)
    SetRegisteredVariable(name, *registration, resolved_value);

  if (!resolved_data) {
    return registration->InitialVariableData();
  }

  return resolved_data;
}

scoped_refptr<CSSVariableData> CSSVariableResolver::ResolveCustomProperty(
    AtomicString name,
    const CSSVariableData& variable_data,
    const Options& options,
    bool resolve_urls,
    bool& cycle_detected) {
  DCHECK(variable_data.NeedsVariableResolution() || resolve_urls);

  Result result;
  result.is_animation_tainted = variable_data.IsAnimationTainted();
  result.has_font_units = variable_data.HasFontUnits();
  result.has_root_font_units = variable_data.HasRootFontUnits();
  result.absolutized = variable_data.IsAbsolutized();
  result.backing_strings.AppendVector(variable_data.BackingStrings());
  DCHECK(!variables_seen_.Contains(name));
  variables_seen_.insert(name);
  bool success = ResolveTokenRange(variable_data.Tokens(), options, result);
  variables_seen_.erase(name);

  if (!success || !cycle_start_points_.IsEmpty()) {
    cycle_start_points_.erase(name);
    cycle_detected = true;
    return nullptr;
  }
  cycle_detected = false;

  if (resolve_urls) {
    ResolveRelativeUrls(result.tokens, result.backing_strings,
                        variable_data.BaseURL(), variable_data.Charset());
  }

  return CSSVariableData::CreateResolved(
      result.tokens, std::move(result.backing_strings),
      result.is_animation_tainted, result.has_font_units,
      result.has_root_font_units, result.absolutized);
}

scoped_refptr<CSSVariableData>
CSSVariableResolver::ResolveCustomPropertyIfNeeded(
    AtomicString name,
    CSSVariableData* variable_data,
    const Options& options) {
  DCHECK(variable_data);
  bool resolve_urls = ShouldResolveRelativeUrls(name, *variable_data);
  if (!variable_data->NeedsVariableResolution() && !resolve_urls)
    return variable_data;
  bool unused_cycle_detected;
  return ResolveCustomProperty(name, *variable_data, options, resolve_urls,
                               unused_cycle_detected);
}

void CSSVariableResolver::ResolveRelativeUrls(
    Vector<CSSParserToken>& tokens,
    Vector<String>& backing_strings,
    const KURL& base_url,
    const WTF::TextEncoding& charset) {
  CSSParserToken* token = tokens.begin();
  CSSParserToken* end = tokens.end();

  while (token < end) {
    if (token->GetType() == kUrlToken) {
      *token = ResolveUrl(*token, backing_strings, base_url, charset);
    } else if (token->FunctionId() == CSSValueUrl) {
      if (token + 1 < end && token[1].GetType() == kStringToken)
        token[1] = ResolveUrl(token[1], backing_strings, base_url, charset);
    }
    ++token;
  }
}

bool CSSVariableResolver::ShouldResolveRelativeUrls(
    const AtomicString& name,
    const CSSVariableData& variable_data) {
  if (!variable_data.NeedsUrlResolution())
    return false;
  const PropertyRegistration* registration =
      registry_ ? registry_->Registration(name) : nullptr;
  return registration ? registration->Syntax().HasUrlSyntax() : false;
}

bool CSSVariableResolver::IsVariableDisallowed(
    const CSSVariableData& variable_data,
    const Options& options,
    const PropertyRegistration* registration) {
  return (options.disallow_animation_tainted &&
          variable_data.IsAnimationTainted()) ||
         (registration && options.disallow_registered_font_units &&
          variable_data.HasFontUnits()) ||
         (registration && options.disallow_registered_root_font_units &&
          variable_data.HasRootFontUnits());
}

CSSVariableData* CSSVariableResolver::GetVariable(
    const AtomicString& name,
    const PropertyRegistration* registration) {
  if (!registration || registration->Inherits()) {
    return inherited_variables_ ? inherited_variables_->GetVariable(name)
                                : nullptr;
  }
  return non_inherited_variables_ ? non_inherited_variables_->GetVariable(name)
                                  : nullptr;
}

const CSSValue* CSSVariableResolver::GetRegisteredVariable(
    const AtomicString& name,
    const PropertyRegistration& registration) {
  if (registration.Inherits()) {
    return inherited_variables_ ? inherited_variables_->RegisteredVariable(name)
                                : nullptr;
  }
  return non_inherited_variables_
             ? non_inherited_variables_->RegisteredVariable(name)
             : nullptr;
}

void CSSVariableResolver::SetVariable(
    const AtomicString& name,
    const PropertyRegistration* registration,
    scoped_refptr<CSSVariableData> variable_data) {
  if (!registration || registration->Inherits()) {
    DCHECK(inherited_variables_);
    inherited_variables_->SetVariable(name, std::move(variable_data));
  } else {
    DCHECK(non_inherited_variables_);
    non_inherited_variables_->SetVariable(name, std::move(variable_data));
  }
}

void CSSVariableResolver::SetRegisteredVariable(
    const AtomicString& name,
    const PropertyRegistration& registration,
    const CSSValue* value) {
  if (registration.Inherits()) {
    DCHECK(inherited_variables_);
    inherited_variables_->SetRegisteredVariable(name, value);
  } else {
    DCHECK(non_inherited_variables_);
    non_inherited_variables_->SetRegisteredVariable(name, value);
  }
}

bool CSSVariableResolver::ResolveVariableReference(CSSParserTokenRange range,
                                                   const Options& options,
                                                   bool is_env_variable,
                                                   Result& result) {
  range.ConsumeWhitespace();
  DCHECK_EQ(range.Peek().GetType(), kIdentToken);
  AtomicString variable_name =
      range.ConsumeIncludingWhitespace().Value().ToAtomicString();
  DCHECK(range.AtEnd() || (range.Peek().GetType() == kCommaToken));

  if (!variables_seen_.Contains(variable_name)) {
    ApplyAnimation(variable_name);
    // Null custom property storage may become non-null after application, we
    // must refresh these cached values.
    inherited_variables_ = state_.Style()->InheritedVariables();
    non_inherited_variables_ = state_.Style()->NonInheritedVariables();
  }

  if (registry_ && !is_env_variable) {
    registry_->MarkReferenced(variable_name);
  }

  scoped_refptr<CSSVariableData> variable_data =
      is_env_variable ? ValueForEnvironmentVariable(variable_name)
                      : ValueForCustomProperty(variable_name, options);

  if (!variable_data) {
    // TODO(alancutter): Append the registered initial custom property value if
    // we are disallowing an animation tainted value.
    return ResolveFallback(range, options, result);
  }

  result.tokens.AppendVector(variable_data->Tokens());
  // TODO(alancutter): Avoid adding backing strings multiple times in a row.
  result.backing_strings.AppendVector(variable_data->BackingStrings());
  result.is_animation_tainted |= variable_data->IsAnimationTainted();
  result.has_font_units |= variable_data->HasFontUnits();
  result.has_root_font_units |= variable_data->HasRootFontUnits();
  result.absolutized &= variable_data->IsAbsolutized();

  Result trash;
  ResolveFallback(range, options, trash);
  return true;
}

scoped_refptr<CSSVariableData> CSSVariableResolver::ValueForEnvironmentVariable(
    const AtomicString& name) {
  // If we are in a User Agent Shadow DOM then we should not record metrics.
  ContainerNode& scope_root = state_.GetTreeScope().RootNode();
  bool is_ua_scope =
      scope_root.IsShadowRoot() && ToShadowRoot(scope_root).IsUserAgent();

  return state_.GetDocument()
      .GetStyleEngine()
      .EnsureEnvironmentVariables()
      .ResolveVariable(name, !is_ua_scope);
}

bool CSSVariableResolver::ResolveTokenRange(CSSParserTokenRange range,
                                            const Options& options,
                                            Result& result) {
  bool success = true;
  while (!range.AtEnd()) {
    const CSSParserToken& token = range.Peek();
    if (token.FunctionId() == CSSValueVar ||
        token.FunctionId() == CSSValueEnv) {
      success &=
          ResolveVariableReference(range.ConsumeBlock(), options,
                                   token.FunctionId() == CSSValueEnv, result);
    } else {
      result.tokens.push_back(range.Consume());
    }
  }
  return success;
}

const CSSValue* CSSVariableResolver::ResolveVariableReferences(
    CSSPropertyID id,
    const CSSValue& value,
    bool disallow_animation_tainted) {
  DCHECK(!CSSProperty::Get(id).IsShorthand());

  Options options;
  options.disallow_animation_tainted = disallow_animation_tainted;

  if (id == CSSPropertyFontSize) {
    bool is_root =
        state_.GetElement() &&
        state_.GetElement() == state_.GetDocument().documentElement();
    options.disallow_registered_font_units = true;
    options.disallow_registered_root_font_units = is_root;
  }

  if (value.IsPendingSubstitutionValue()) {
    return ResolvePendingSubstitutions(id, ToCSSPendingSubstitutionValue(value),
                                       options);
  }

  if (value.IsVariableReferenceValue()) {
    return ResolveVariableReferences(id, ToCSSVariableReferenceValue(value),
                                     options);
  }

  NOTREACHED();
  return nullptr;
}

const CSSValue* CSSVariableResolver::ResolveVariableReferences(
    CSSPropertyID id,
    const CSSVariableReferenceValue& value,
    const Options& options) {
  Result result;

  if (!ResolveTokenRange(value.VariableDataValue()->Tokens(), options,
                         result)) {
    return cssvalue::CSSUnsetValue::Create();
  }
  const CSSValue* resolved_value = CSSPropertyParser::ParseSingleValue(
      id, result.tokens, value.ParserContext());
  if (!resolved_value)
    return cssvalue::CSSUnsetValue::Create();
  return resolved_value;
}

const CSSValue* CSSVariableResolver::ResolvePendingSubstitutions(
    CSSPropertyID id,
    const CSSPendingSubstitutionValue& pending_value,
    const Options& options) {
  // Longhands from shorthand references follow this path.
  HeapHashMap<CSSPropertyID, Member<const CSSValue>>& property_cache =
      state_.ParsedPropertiesForPendingSubstitutionCache(pending_value);

  const CSSValue* value = property_cache.at(id);
  if (!value) {
    // TODO(timloh): We shouldn't retry this for all longhands if the shorthand
    // ends up invalid.
    CSSVariableReferenceValue* shorthand_value = pending_value.ShorthandValue();
    CSSPropertyID shorthand_property_id = pending_value.ShorthandPropertyId();

    Result result;
    if (ResolveTokenRange(shorthand_value->VariableDataValue()->Tokens(),
                          options, result)) {
      HeapVector<CSSPropertyValue, 256> parsed_properties;

      if (CSSPropertyParser::ParseValue(
              shorthand_property_id, false, CSSParserTokenRange(result.tokens),
              shorthand_value->ParserContext(), parsed_properties,
              StyleRule::RuleType::kStyle)) {
        unsigned parsed_properties_count = parsed_properties.size();
        for (unsigned i = 0; i < parsed_properties_count; ++i) {
          property_cache.Set(parsed_properties[i].Id(),
                             parsed_properties[i].Value());
        }
      }
    }
    value = property_cache.at(id);
  }

  if (value)
    return value;

  return cssvalue::CSSUnsetValue::Create();
}

scoped_refptr<CSSVariableData>
CSSVariableResolver::ResolveCustomPropertyAnimationKeyframe(
    const CSSCustomPropertyDeclaration& keyframe,
    bool& cycle_detected) {
  DCHECK(keyframe.Value());
  DCHECK(keyframe.Value()->NeedsVariableResolution());
  const AtomicString& name = keyframe.GetName();

  if (variables_seen_.Contains(name)) {
    cycle_start_points_.insert(name);
    cycle_detected = true;
    return nullptr;
  }

  bool resolve_urls = false;
  return ResolveCustomProperty(name, *keyframe.Value(), Options(), resolve_urls,
                               cycle_detected);
}

void CSSVariableResolver::ResolveVariableDefinitions() {
  if (!inherited_variables_ && !non_inherited_variables_)
    return;

  Options options;
  options.absolutize = true;

  int variable_count = 0;
  if (inherited_variables_ && inherited_variables_->NeedsResolution()) {
    for (auto& variable : inherited_variables_->data_)
      ValueForCustomProperty(variable.key, options);
    inherited_variables_->ClearNeedsResolution();
    variable_count += inherited_variables_->data_.size();
  }
  if (non_inherited_variables_ && non_inherited_variables_->NeedsResolution()) {
    for (auto& variable : non_inherited_variables_->data_)
      ValueForCustomProperty(variable.key, options);
    non_inherited_variables_->ClearNeedsResolution();
    variable_count += non_inherited_variables_->data_.size();
  }
  INCREMENT_STYLE_STATS_COUNTER(state_.GetDocument().GetStyleEngine(),
                                custom_properties_applied, variable_count);
}

void CSSVariableResolver::ComputeRegisteredVariables() {
  Options options;

  if (inherited_variables_) {
    for (auto& variable : *inherited_variables_->registered_data_)
      ValueForCustomProperty(variable.key, options);
  }
  if (non_inherited_variables_) {
    for (auto& variable : *non_inherited_variables_->registered_data_)
      ValueForCustomProperty(variable.key, options);
  }
}

CSSVariableResolver::CSSVariableResolver(const StyleResolverState& state)
    : state_(state),
      inherited_variables_(state.Style()->InheritedVariables()),
      non_inherited_variables_(state.Style()->NonInheritedVariables()),
      registry_(state.GetDocument().GetPropertyRegistry()) {}

}  // namespace blink
