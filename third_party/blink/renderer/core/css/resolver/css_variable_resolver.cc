// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/resolver/css_variable_resolver.h"

#include "third_party/blink/renderer/core/css/css_custom_property_declaration.h"
#include "third_party/blink/renderer/core/css/css_invalid_variable_value.h"
#include "third_party/blink/renderer/core/css/css_pending_substitution_value.h"
#include "third_party/blink/renderer/core/css/css_property_names.h"
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
#include "third_party/blink/renderer/core/css_value_keywords.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/style/style_inherited_variables.h"
#include "third_party/blink/renderer/core/style/style_non_inherited_variables.h"
#include "third_party/blink/renderer/core/style_property_shorthand.h"
#include "third_party/blink/renderer/platform/wtf/text/text_encoding.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

CSSVariableResolver::Fallback CSSVariableResolver::ResolveFallback(
    CSSParserTokenRange range,
    const Options& options,
    const PropertyRegistration* registration,
    Result& result) {
  if (range.AtEnd())
    return Fallback::kNone;
  DCHECK_EQ(range.Peek().GetType(), kCommaToken);
  range.Consume();
  size_t first_fallback_token = result.tokens.size();
  if (!ResolveTokenRange(range, options, result))
    return Fallback::kFail;
  if (registration) {
    CSSParserTokenRange resolved_range(result.tokens);
    resolved_range = resolved_range.MakeSubRange(
        &resolved_range.Peek(first_fallback_token), resolved_range.end());
    const CSSParserContext* context =
        StrictCSSParserContext(state_.GetDocument().GetSecureContextMode());
    const bool is_animation_tainted = false;
    if (!registration->Syntax().Parse(resolved_range, context,
                                      is_animation_tainted))
      return Fallback::kFail;
  }
  return Fallback::kSuccess;
}

scoped_refptr<CSSVariableData> CSSVariableResolver::ValueForCustomProperty(
    AtomicString name,
    const Options& options,
    bool& unit_cycle) {
  if (variables_seen_.Contains(name)) {
    cycle_start_points_.insert(name);
    return nullptr;
  }

  DCHECK(registry_ || !RuntimeEnabledFeatures::CSSVariables2Enabled());
  const PropertyRegistration* registration =
      registry_ ? registry_->Registration(name) : nullptr;

  CSSVariableData* variable_data = GetVariableData(name, registration);

  if (!variable_data)
    return nullptr;

  bool cycle_detected = false;
  scoped_refptr<CSSVariableData> resolved_data = ResolveCustomPropertyIfNeeded(
      name, variable_data, options, cycle_detected);

  if (!resolved_data && cycle_detected) {
    if (options.absolutize)
      SetInvalidVariable(name, registration);
    return nullptr;
  }

  if (resolved_data) {
    if (IsDisallowedByFontUnitFlags(*resolved_data, options, registration)) {
      unit_cycle = true;
      SetInvalidVariable(name, registration);
      return nullptr;
    }
    if (IsDisallowedByAnimationTaintedFlag(*resolved_data, options))
      return nullptr;
  }

  if (!registration) {
    if (resolved_data != variable_data && options.absolutize)
      SetVariableData(name, registration, resolved_data);
    return resolved_data;
  }

  const CSSValue* value = GetVariableValue(name, *registration);
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

  // If either parsing or resolution failed, fall back on "unset".
  if (!resolved_data) {
    if (registration->Inherits()) {
      resolved_data = state_.ParentStyle()->GetVariableData(name, true);
      resolved_value = state_.ParentStyle()->GetVariableValue(name, true);
    } else {
      resolved_data = registration->InitialVariableData();
      resolved_value = registration->Initial();
    }
  }

  // Registered custom properties substitute as token sequences equivalent to
  // their computed values. CSSVariableData instances which represent such token
  // sequences are called "absolutized".
  if (resolved_value && !resolved_data->IsAbsolutized()) {
    resolved_value = &StyleBuilderConverter::ConvertRegisteredPropertyValue(
        state_, *resolved_value, resolved_data->BaseURL(),
        resolved_data->Charset());
    resolved_data =
        StyleBuilderConverter::ConvertRegisteredPropertyVariableData(
            *resolved_value, resolved_data->IsAnimationTainted());
    DCHECK(resolved_data->IsAbsolutized());
  }

  // If options.absolutize=false, we want to keep the original (non-absolute)
  // token sequence to retain any var()-references. This makes it possible to
  // resolve the var()-reference again, using a different (e.g. animated) value.
  if (options.absolutize && resolved_data != variable_data)
    SetVariableData(name, registration, resolved_data);

  // The options.absolutize flag does not apply to the computed value, only
  // to the tokens used for substitution. Hence, store the computed value on
  // ComputedStyle, regardless of the flag. This is needed to correctly
  // calculate animations.
  if (value != resolved_value)
    SetVariableValue(name, *registration, resolved_value);

  return resolved_data;
}

scoped_refptr<CSSVariableData> CSSVariableResolver::ResolveCustomProperty(
    AtomicString name,
    const CSSVariableData& variable_data,
    const Options& options,
    bool& cycle_detected) {
  DCHECK(variable_data.NeedsVariableResolution());

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

  if (!cycle_start_points_.IsEmpty())
    cycle_detected = true;
  if (!success || cycle_detected) {
    cycle_start_points_.erase(name);
    return nullptr;
  }
  cycle_detected = false;

  return CSSVariableData::CreateResolved(
      result.tokens, std::move(result.backing_strings),
      result.is_animation_tainted, result.has_font_units,
      result.has_root_font_units, result.absolutized, variable_data.BaseURL(),
      variable_data.Charset());
}

scoped_refptr<CSSVariableData>
CSSVariableResolver::ResolveCustomPropertyIfNeeded(
    AtomicString name,
    CSSVariableData* variable_data,
    const Options& options,
    bool& cycle_detected) {
  DCHECK(variable_data);
  if (!variable_data->NeedsVariableResolution())
    return variable_data;
  return ResolveCustomProperty(name, *variable_data, options, cycle_detected);
}

bool CSSVariableResolver::IsDisallowedByFontUnitFlags(
    const CSSVariableData& variable_data,
    const Options& options,
    const PropertyRegistration* registration) {
  return (registration && options.disallow_registered_font_units &&
          variable_data.HasFontUnits()) ||
         (registration && options.disallow_registered_root_font_units &&
          variable_data.HasRootFontUnits());
}

bool CSSVariableResolver::IsDisallowedByAnimationTaintedFlag(
    const CSSVariableData& variable_data,
    const Options& options) {
  return options.disallow_animation_tainted &&
         variable_data.IsAnimationTainted();
}

CSSVariableData* CSSVariableResolver::GetVariableData(
    const AtomicString& name,
    const PropertyRegistration* registration) {
  return state_.Style()->GetVariableData(
      name, !registration || registration->Inherits());
}

const CSSValue* CSSVariableResolver::GetVariableValue(
    const AtomicString& name,
    const PropertyRegistration& registration) {
  return state_.Style()->GetVariableValue(name, registration.Inherits());
}

void CSSVariableResolver::SetVariableData(
    const AtomicString& name,
    const PropertyRegistration* registration,
    scoped_refptr<CSSVariableData> variable_data) {
  if (!registration || registration->Inherits()) {
    DCHECK(inherited_variables_);
    inherited_variables_->SetData(name, std::move(variable_data));
  } else {
    DCHECK(non_inherited_variables_);
    non_inherited_variables_->SetData(name, std::move(variable_data));
  }
}

void CSSVariableResolver::SetVariableValue(
    const AtomicString& name,
    const PropertyRegistration& registration,
    const CSSValue* value) {
  if (registration.Inherits()) {
    DCHECK(inherited_variables_);
    inherited_variables_->SetValue(name, value);
  } else {
    DCHECK(non_inherited_variables_);
    non_inherited_variables_->SetValue(name, value);
  }
}

void CSSVariableResolver::SetInvalidVariable(
    const AtomicString& name,
    const PropertyRegistration* registration) {
  SetVariableData(name, registration, nullptr);
  if (registration)
    SetVariableValue(name, *registration, nullptr);
}

const CSSParserContext* CSSVariableResolver::GetParserContext(
    const CSSVariableReferenceValue& value) const {
  // TODO(crbug.com/985028): CSSVariableReferenceValue should always have
  // a CSSParserContext.
  if (value.ParserContext())
    return value.ParserContext();
  return StrictCSSParserContext(state_.GetDocument().GetSecureContextMode());
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

  const PropertyRegistration* registration = nullptr;
  if (registry_) {
    registration = registry_->Registration(variable_name);
    if (!is_env_variable)
      registry_->MarkReferenced(variable_name);
  }

  bool unit_cycle = false;
  scoped_refptr<CSSVariableData> variable_data =
      is_env_variable
          ? ValueForEnvironmentVariable(variable_name)
          : ValueForCustomProperty(variable_name, options, unit_cycle);

  if (unit_cycle)
    return false;

  if (!variable_data) {
    // TODO(alancutter): Append the registered initial custom property value if
    // we are disallowing an animation tainted value.
    return ResolveFallback(range, options, registration, result) ==
           Fallback::kSuccess;
  }

  if (variable_data->Tokens().size() > kMaxSubstitutionTokens)
    return false;

  result.tokens.AppendVector(variable_data->Tokens());
  // TODO(alancutter): Avoid adding backing strings multiple times in a row.
  result.backing_strings.AppendVector(variable_data->BackingStrings());
  result.is_animation_tainted |= variable_data->IsAnimationTainted();
  result.has_font_units |= variable_data->HasFontUnits();
  result.has_root_font_units |= variable_data->HasRootFontUnits();
  result.absolutized &= variable_data->IsAbsolutized();

  Result trash;
  Fallback fallback = ResolveFallback(range, options, registration, trash);

  // For registered properties, the fallback (if present) must be valid, even
  // if it's not used.
  if (registration && fallback == Fallback::kFail)
    return false;

  return true;
}

scoped_refptr<CSSVariableData> CSSVariableResolver::ValueForEnvironmentVariable(
    const AtomicString& name) {
  // If we are in a User Agent Shadow DOM then we should not record metrics.
  ContainerNode& scope_root = state_.GetTreeScope().RootNode();
  auto* shadow_root = DynamicTo<ShadowRoot>(&scope_root);
  bool is_ua_scope = shadow_root && shadow_root->IsUserAgent();

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
    if (token.FunctionId() == CSSValueID::kVar ||
        token.FunctionId() == CSSValueID::kEnv) {
      success &= ResolveVariableReference(
          range.ConsumeBlock(), options, token.FunctionId() == CSSValueID::kEnv,
          result);
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

  if (id == CSSPropertyID::kFontSize) {
    bool is_root =
        &state_.GetElement() == state_.GetDocument().documentElement();
    options.disallow_registered_font_units = true;
    options.disallow_registered_root_font_units = is_root;
  }

  if (auto* substition_value =
          DynamicTo<cssvalue::CSSPendingSubstitutionValue>(value)) {
    return ResolvePendingSubstitutions(id, *substition_value, options);
  }

  if (auto* variable_reference_value =
          DynamicTo<CSSVariableReferenceValue>(value)) {
    return ResolveVariableReferences(id, *variable_reference_value, options);
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
      id, result.tokens, GetParserContext(value));
  if (!resolved_value)
    return cssvalue::CSSUnsetValue::Create();
  return resolved_value;
}

const CSSValue* CSSVariableResolver::ResolvePendingSubstitutions(
    CSSPropertyID id,
    const cssvalue::CSSPendingSubstitutionValue& pending_value,
    const Options& options) {
  DCHECK_NE(CSSPropertyID::kVariable, id);

  // For -internal-visited-* properties, we pretend that we're resolving the
  // unvisited counterpart. This is because the CSSPendingSubstitutionValue
  // held by the -internal-visited-* property contains a shorthand that expands
  // to unvisited properties.
  const CSSProperty& property = CSSProperty::Get(id);
  CSSPropertyID cache_id = id;
  if (property.IsVisited())
    cache_id = property.GetUnvisitedProperty()->PropertyID();

  // Longhands from shorthand references follow this path.
  HeapHashMap<CSSPropertyID, Member<const CSSValue>>& property_cache =
      state_.ParsedPropertiesForPendingSubstitutionCache(pending_value);

  const CSSValue* value = property_cache.at(cache_id);
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
    value = property_cache.at(cache_id);
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

  return ResolveCustomProperty(name, *keyframe.Value(), Options(),
                               cycle_detected);
}

void CSSVariableResolver::ResolveVariableDefinitions() {
  if (!inherited_variables_ && !non_inherited_variables_)
    return;

  Options options;
  options.absolutize = true;

  int variable_count = 0;
  if (inherited_variables_ && inherited_variables_->NeedsResolution()) {
    for (auto& variable : inherited_variables_->Data()) {
      bool cycle_detected = false;
      ValueForCustomProperty(variable.key, options, cycle_detected);
    }
    inherited_variables_->ClearNeedsResolution();
    variable_count += inherited_variables_->Data().size();
  }
  if (non_inherited_variables_ && non_inherited_variables_->NeedsResolution()) {
    for (auto& variable : non_inherited_variables_->Data()) {
      bool cycle_detected = false;
      ValueForCustomProperty(variable.key, options, cycle_detected);
    }
    non_inherited_variables_->ClearNeedsResolution();
    variable_count += non_inherited_variables_->Data().size();
  }
  INCREMENT_STYLE_STATS_COUNTER(state_.GetDocument().GetStyleEngine(),
                                custom_properties_applied, variable_count);
}

void CSSVariableResolver::ComputeRegisteredVariables() {
  Options options;

  if (inherited_variables_) {
    for (auto& variable : inherited_variables_->Values()) {
      bool cycle_detected = false;
      ValueForCustomProperty(variable.key, options, cycle_detected);
    }
  }
  if (non_inherited_variables_) {
    for (auto& variable : non_inherited_variables_->Values()) {
      bool cycle_detected = false;
      ValueForCustomProperty(variable.key, options, cycle_detected);
    }
  }
}

CSSVariableResolver::CSSVariableResolver(const StyleResolverState& state)
    : state_(state),
      inherited_variables_(state.Style()->InheritedVariables()),
      non_inherited_variables_(state.Style()->NonInheritedVariables()),
      registry_(state.GetDocument().GetPropertyRegistry()) {}

}  // namespace blink
