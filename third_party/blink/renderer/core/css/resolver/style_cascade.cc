// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/resolver/style_cascade.h"

#include "third_party/blink/renderer/core/animation/css/css_animations.h"
#include "third_party/blink/renderer/core/animation/css_interpolation_environment.h"
#include "third_party/blink/renderer/core/animation/css_interpolation_types_map.h"
#include "third_party/blink/renderer/core/animation/invalidatable_interpolation.h"
#include "third_party/blink/renderer/core/animation/property_handle.h"
#include "third_party/blink/renderer/core/animation/transition_interpolation.h"
#include "third_party/blink/renderer/core/css/css_custom_property_declaration.h"
#include "third_party/blink/renderer/core/css/css_font_selector.h"
#include "third_party/blink/renderer/core/css/css_invalid_variable_value.h"
#include "third_party/blink/renderer/core/css/css_pending_interpolation_value.h"
#include "third_party/blink/renderer/core/css/css_pending_substitution_value.h"
#include "third_party/blink/renderer/core/css/css_unset_value.h"
#include "third_party/blink/renderer/core/css/css_variable_data.h"
#include "third_party/blink/renderer/core/css/css_variable_reference_value.h"
#include "third_party/blink/renderer/core/css/document_style_environment_variables.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_local_context.h"
#include "third_party/blink/renderer/core/css/parser/css_property_parser.h"
#include "third_party/blink/renderer/core/css/properties/css_property.h"
#include "third_party/blink/renderer/core/css/properties/css_property_ref.h"
#include "third_party/blink/renderer/core/css/property_registry.h"
#include "third_party/blink/renderer/core/css/resolver/css_property_priority.h"
#include "third_party/blink/renderer/core/css/resolver/style_builder.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver_state.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/style/computed_style.h"

namespace blink {

namespace {

class NullAnimator : public StyleCascade::Animator {
  STACK_ALLOCATED();

 public:
  void Apply(const CSSProperty&,
             const cssvalue::CSSPendingInterpolationValue&,
             StyleCascade::Resolver&) override {}
};

AtomicString ConsumeVariableName(CSSParserTokenRange& range) {
  range.ConsumeWhitespace();
  CSSParserToken ident_token = range.ConsumeIncludingWhitespace();
  DCHECK_EQ(ident_token.GetType(), kIdentToken);
  return ident_token.Value().ToAtomicString();
}

bool ConsumeComma(CSSParserTokenRange& range) {
  if (range.Peek().GetType() == kCommaToken) {
    range.Consume();
    return true;
  }
  return false;
}

const CSSValue* Parse(const CSSProperty& property,
                      CSSParserTokenRange range,
                      const CSSParserContext* context) {
  return CSSPropertyParser::ParseSingleValue(property.PropertyID(), range,
                                             context);
}

constexpr bool IsImportant(StyleCascade::Origin origin) {
  return static_cast<uint8_t>(origin) & StyleCascade::kImportantBit;
}

static_assert(!IsImportant(StyleCascade::Origin::kNone),
              "Origin::kNone is not important");
static_assert(!IsImportant(StyleCascade::Origin::kUserAgent),
              "Origin::kUserAgent is not important");
static_assert(!IsImportant(StyleCascade::Origin::kUser),
              "Origin::kUser is not important");
static_assert(!IsImportant(StyleCascade::Origin::kAnimation),
              "Origin::kAnimation is not important");
static_assert(IsImportant(StyleCascade::Origin::kImportantAuthor),
              "Origin::kImportantAuthor is important");
static_assert(IsImportant(StyleCascade::Origin::kImportantUser),
              "Origin::kImportantUser is important");
static_assert(IsImportant(StyleCascade::Origin::kImportantUserAgent),
              "Origin::kImportantUserAgent is important");
static_assert(!IsImportant(StyleCascade::Origin::kTransition),
              "Origin::kTransition is not important");

}  // namespace

StyleCascade::Priority::Priority(Origin origin, uint16_t tree_order)
    : priority_((static_cast<uint64_t>(origin) << 32) |
                (static_cast<uint64_t>(tree_order) << 16) |
                StyleCascade::kMaxCascadeOrder) {}

StyleCascade::Origin StyleCascade::Priority::GetOrigin() const {
  return static_cast<StyleCascade::Origin>((priority_ >> 32) & 0xFF);
}

bool StyleCascade::Priority::operator>=(const Priority& other) const {
  uint64_t important_xor = IsImportant(GetOrigin()) ? 0 : (0xFF << 16);
  return (priority_ ^ important_xor) >= (other.priority_ ^ important_xor);
}

void StyleCascade::Add(const CSSPropertyName& name,
                       const CSSValue* value,
                       Priority priority) {
  auto result = cascade_.insert(name, Value());
  if (priority >= result.stored_value->value.GetPriority()) {
    result.stored_value->value =
        Value(value, priority.WithCascadeOrder(++order_));
  }
}

void StyleCascade::Apply() {
  NullAnimator animator;
  Apply(animator);
}

void StyleCascade::Apply(Animator& animator) {
  Resolver resolver(animator);

  // TODO(crbug.com/985031): Set bits ::Add-time to know if we need to do this.
  ApplyHighPriority(resolver);

  // TODO(crbug.com/985010): Improve with non-destructive Apply.
  while (!cascade_.IsEmpty()) {
    auto iter = cascade_.begin();
    const CSSPropertyName& name = iter->key;
    Apply(name, resolver);
  }
}

void StyleCascade::RemoveAnimationPriority() {
  using AnimPrio = CSSPropertyPriorityData<kAnimationPropertyPriority>;
  int first = static_cast<int>(AnimPrio::First());
  int last = static_cast<int>(AnimPrio::Last());
  for (int i = first; i <= last; ++i) {
    CSSPropertyName name(convertToCSSPropertyID(i));
    cascade_.erase(name);
  }
}

const CSSValue* StyleCascade::Resolve(const CSSPropertyName& name,
                                      const CSSValue& value,
                                      Resolver& resolver) {
  CSSPropertyRef ref(name, state_.GetDocument());

  const CSSValue* resolved = Resolve(ref.GetProperty(), value, resolver);

  DCHECK(resolved);

  if (resolved->IsInvalidVariableValue())
    return nullptr;

  return resolved;
}

void StyleCascade::ApplyHighPriority(Resolver& resolver) {
  using HighPriority = CSSPropertyPriorityData<kHighPropertyPriority>;
  int first = static_cast<int>(HighPriority::First());
  int last = static_cast<int>(HighPriority::Last());
  for (int i = first; i <= last; ++i)
    Apply(CSSProperty::Get(convertToCSSPropertyID(i)), resolver);

  state_.GetFontBuilder().CreateFont(
      state_.GetDocument().GetStyleEngine().GetFontSelector(),
      state_.StyleRef());
  state_.SetConversionFontSizes(CSSToLengthConversionData::FontSizes(
      state_.Style(), state_.RootElementStyle()));
  state_.SetConversionZoom(state_.Style()->EffectiveZoom());
}

void StyleCascade::Apply(const CSSPropertyName& name) {
  NullAnimator animator;
  Resolver resolver(animator);
  Apply(name, resolver);
}

void StyleCascade::Apply(const CSSPropertyName& name, Resolver& resolver) {
  CSSPropertyRef ref(name, state_.GetDocument());
  DCHECK(ref.IsValid());
  Apply(ref.GetProperty(), resolver);
}

void StyleCascade::Apply(const CSSProperty& property, Resolver& resolver) {
  CSSPropertyName name = property.GetCSSPropertyName();

  DCHECK(!resolver.IsLocked(name));

  Value cascaded = cascade_.Take(property.GetCSSPropertyName());
  if (cascaded.IsEmpty())
    return;

  const CSSValue* value = cascaded.GetValue();

  if (const auto* v =
          DynamicTo<cssvalue::CSSPendingInterpolationValue>(value)) {
    resolver.animator_.Apply(property, *v, resolver);
    return;
  }

  value = Resolve(property, *value, resolver);

  DCHECK(!value->IsVariableReferenceValue());
  DCHECK(!value->IsPendingSubstitutionValue());
  DCHECK(!value->IsPendingInterpolationValue());

  if (!resolver.filter_.Add(property, cascaded))
    return;

  StyleBuilder::ApplyProperty(property, state_, *value);
}

bool StyleCascade::HasValue(const CSSPropertyName& name,
                            const CSSValue* value) const {
  auto iter = cascade_.find(name);
  return (iter != cascade_.end()) && (iter->value.GetValue() == value);
}

const CSSValue* StyleCascade::GetValue(const CSSPropertyName& name) const {
  auto iter = cascade_.find(name);
  return (iter != cascade_.end()) ? iter->value.GetValue() : nullptr;
}

void StyleCascade::ReplaceValue(const CSSPropertyName& name,
                                const CSSValue* value) {
  auto iter = cascade_.find(name);
  if (iter != cascade_.end())
    iter->value = Value(value, iter->value.GetPriority());
}

bool StyleCascade::IsRootElement() const {
  return &state_.GetElement() == state_.GetDocument().documentElement();
}

StyleCascade::TokenSequence::TokenSequence(const CSSVariableData* data)
    : backing_strings_(data->BackingStrings()),
      is_animation_tainted_(data->IsAnimationTainted()),
      has_font_units_(data->HasFontUnits()),
      has_root_font_units_(data->HasRootFontUnits()),
      base_url_(data->BaseURL()),
      charset_(data->Charset()) {}

void StyleCascade::TokenSequence::Append(const TokenSequence& sequence) {
  tokens_.AppendVector(sequence.tokens_);
  backing_strings_.AppendVector(sequence.backing_strings_);
  is_animation_tainted_ |= sequence.is_animation_tainted_;
  has_font_units_ |= sequence.has_font_units_;
  has_root_font_units_ |= sequence.has_root_font_units_;
}

void StyleCascade::TokenSequence::Append(const CSSVariableData* data) {
  tokens_.AppendVector(data->Tokens());
  backing_strings_.AppendVector(data->BackingStrings());
  is_animation_tainted_ |= data->IsAnimationTainted();
  has_font_units_ |= data->HasFontUnits();
  has_root_font_units_ |= data->HasRootFontUnits();
}

void StyleCascade::TokenSequence::Append(const CSSParserToken& token) {
  tokens_.push_back(token);
}

scoped_refptr<CSSVariableData>
StyleCascade::TokenSequence::BuildVariableData() {
  // TODO(andruud): Why not also std::move tokens?
  const bool absolutized = true;
  return CSSVariableData::CreateResolved(
      tokens_, std::move(backing_strings_), is_animation_tainted_,
      has_font_units_, has_root_font_units_, absolutized, base_url_, charset_);
}

const CSSValue* StyleCascade::Resolve(const CSSProperty& property,
                                      const CSSValue& value,
                                      Resolver& resolver) {
  if (const auto* v = DynamicTo<CSSCustomPropertyDeclaration>(value))
    return ResolveCustomProperty(property, *v, resolver);
  if (const auto* v = DynamicTo<CSSVariableReferenceValue>(value))
    return ResolveVariableReference(property, *v, resolver);
  if (const auto* v = DynamicTo<cssvalue::CSSPendingSubstitutionValue>(value))
    return ResolvePendingSubstitution(property, *v, resolver);
  return &value;
}

const CSSValue* StyleCascade::ResolveCustomProperty(
    const CSSProperty& property,
    const CSSCustomPropertyDeclaration& decl,
    Resolver& resolver) {
  DCHECK(!resolver.IsLocked(property));
  AutoLock lock(property, resolver);

  // TODO(andruud): Don't transport css-wide keywords in this value.
  if (!decl.Value())
    return &decl;

  scoped_refptr<CSSVariableData> data = decl.Value();

  if (data->NeedsVariableResolution())
    data = ResolveVariableData(data.get(), resolver);

  if (HasFontSizeDependency(To<CustomProperty>(property), data.get()))
    resolver.DetectCycle(GetCSSPropertyFontSize());

  if (resolver.InCycle())
    return CSSInvalidVariableValue::Create();

  if (!data) {
    // TODO(crbug.com/980930): Treat custom properties as unset here,
    // not invalid. This behavior is enforced by WPT, but violates the spec.
    if (const auto* custom_property = DynamicTo<CustomProperty>(property)) {
      if (!custom_property->IsRegistered())
        return CSSInvalidVariableValue::Create();
    }
    return cssvalue::CSSUnsetValue::Create();
  }

  if (data == decl.Value())
    return &decl;

  return MakeGarbageCollected<CSSCustomPropertyDeclaration>(decl.GetName(),
                                                            data);
}

const CSSValue* StyleCascade::ResolveVariableReference(
    const CSSProperty& property,
    const CSSVariableReferenceValue& value,
    Resolver& resolver) {
  DCHECK(!resolver.IsLocked(property));
  AutoLock lock(property, resolver);

  const CSSVariableData* data = value.VariableDataValue();
  const CSSParserContext* context = GetParserContext(value);

  DCHECK(data);
  DCHECK(context);

  TokenSequence sequence;

  if (ResolveTokensInto(data->Tokens(), resolver, sequence)) {
    if (const auto* parsed = Parse(property, sequence.TokenRange(), context))
      return parsed;
  }

  return cssvalue::CSSUnsetValue::Create();
}

const CSSValue* StyleCascade::ResolvePendingSubstitution(
    const CSSProperty& property,
    const cssvalue::CSSPendingSubstitutionValue& value,
    Resolver& resolver) {
  DCHECK(!resolver.IsLocked(property));
  AutoLock lock(property, resolver);

  DCHECK_NE(property.PropertyID(), CSSPropertyID::kVariable);

  CSSVariableReferenceValue* shorthand_value = value.ShorthandValue();
  const auto* shorthand_data = shorthand_value->VariableDataValue();
  CSSPropertyID shorthand_property_id = value.ShorthandPropertyId();

  TokenSequence sequence;

  if (!ResolveTokensInto(shorthand_data->Tokens(), resolver, sequence))
    return cssvalue::CSSUnsetValue::Create();

  HeapVector<CSSPropertyValue, 256> parsed_properties;
  const bool important = false;

  if (!CSSPropertyParser::ParseValue(
          shorthand_property_id, important, sequence.TokenRange(),
          shorthand_value->ParserContext(), parsed_properties,
          StyleRule::RuleType::kStyle)) {
    return cssvalue::CSSUnsetValue::Create();
  }

  // For -internal-visited-properties with CSSPendingSubstitutionValues,
  // the inner 'shorthand_property_id' will expand to a set of longhands
  // containing the unvisited equivalent. Hence, when parsing the
  // CSSPendingSubstitutionValue, we look for the unvisited property in
  // parsed_properties.
  const CSSProperty* unvisited_property =
      property.IsVisited() ? property.GetUnvisitedProperty() : &property;

  const CSSValue* result = nullptr;

  unsigned parsed_properties_count = parsed_properties.size();
  for (unsigned i = 0; i < parsed_properties_count; ++i) {
    const CSSProperty& longhand = CSSProperty::Get(parsed_properties[i].Id());
    const CSSPropertyName& name = longhand.GetCSSPropertyName();
    const CSSValue* parsed = parsed_properties[i].Value();

    if (unvisited_property == &longhand)
      result = parsed;
    else if (HasValue(name, &value))
      ReplaceValue(name, parsed);
  }

  DCHECK(result);
  return result;
}

scoped_refptr<CSSVariableData> StyleCascade::ResolveVariableData(
    CSSVariableData* data,
    Resolver& resolver) {
  DCHECK(data && data->NeedsVariableResolution());

  TokenSequence sequence(data);

  if (!ResolveTokensInto(data->Tokens(), resolver, sequence))
    return nullptr;

  return sequence.BuildVariableData();
}

bool StyleCascade::ResolveTokensInto(CSSParserTokenRange range,
                                     Resolver& resolver,
                                     TokenSequence& out) {
  bool success = true;
  while (!range.AtEnd()) {
    const CSSParserToken& token = range.Peek();
    if (token.FunctionId() == CSSValueID::kVar)
      success &= ResolveVarInto(range.ConsumeBlock(), resolver, out);
    else if (token.FunctionId() == CSSValueID::kEnv)
      success &= ResolveEnvInto(range.ConsumeBlock(), resolver, out);
    else
      out.Append(range.Consume());
  }
  return success;
}

bool StyleCascade::ResolveVarInto(CSSParserTokenRange range,
                                  Resolver& resolver,
                                  TokenSequence& out) {
  AtomicString variable_name = ConsumeVariableName(range);
  DCHECK(range.AtEnd() || (range.Peek().GetType() == kCommaToken));

  CustomProperty property(variable_name, state_.GetDocument());

  // Any custom property referenced (by anything, even just once) in the
  // document can currently not be animated on the compositor. Hence we mark
  // properties that have been referenced.
  MarkReferenced(property);

  if (!resolver.DetectCycle(property)) {
    // We are about to substitute var(property). In order to do that, we must
    // know the computed value of 'property', hence we Apply it.
    //
    // We can however not do this if we're in a cycle. If a cycle is detected
    // here, it means we are already resolving 'property', and have discovered
    // a reference to 'property' during that resolution.
    Apply(property, resolver);
  }

  // Note that even if we are in a cycle, we must proceed in order to discover
  // secondary cycles via the var() fallback.

  scoped_refptr<CSSVariableData> data = GetVariableData(property);

  // If substitution is not allowed, treat the value as
  // invalid-at-computed-value-time.
  //
  // https://drafts.csswg.org/css-variables/#animation-tainted
  if (!resolver.AllowSubstitution(data.get()))
    data = nullptr;

  // If we have a fallback, we must process it to look for cycles,
  // even if we aren't going to use the fallback.
  //
  // https://drafts.csswg.org/css-variables/#cycles
  if (ConsumeComma(range)) {
    TokenSequence fallback;
    bool success = ResolveTokensInto(range, resolver, fallback);
    // The fallback must match the syntax of the referenced custom property.
    // https://drafts.css-houdini.org/css-properties-values-api-1/#fallbacks-in-var-references
    if (!ValidateFallback(property, fallback.TokenRange()))
      return false;
    if (!data && success)
      data = fallback.BuildVariableData();
  }

  if (!data || resolver.InCycle())
    return false;

  // https://drafts.csswg.org/css-variables/#long-variables
  if (data->Tokens().size() > kMaxSubstitutionTokens)
    return false;

  out.Append(data.get());

  return true;
}

bool StyleCascade::ResolveEnvInto(CSSParserTokenRange range,
                                  Resolver& resolver,
                                  TokenSequence& out) {
  AtomicString variable_name = ConsumeVariableName(range);
  DCHECK(range.AtEnd() || (range.Peek().GetType() == kCommaToken));

  CSSVariableData* data = GetEnvironmentVariable(variable_name);

  if (!data) {
    if (ConsumeComma(range))
      return ResolveTokensInto(range, resolver, out);
    return false;
  }

  out.Append(data);

  return true;
}

CSSVariableData* StyleCascade::GetVariableData(
    const CustomProperty& property) const {
  const AtomicString& name = property.GetPropertyNameAtomicString();
  const bool is_inherited = property.IsInherited();
  return state_.StyleRef().GetVariableData(name, is_inherited);
}

CSSVariableData* StyleCascade::GetEnvironmentVariable(
    const AtomicString& name) const {
  // If we are in a User Agent Shadow DOM then we should not record metrics.
  ContainerNode& scope_root = state_.GetTreeScope().RootNode();
  auto* shadow_root = DynamicTo<ShadowRoot>(&scope_root);
  bool is_ua_scope = shadow_root && shadow_root->IsUserAgent();

  return state_.GetDocument()
      .GetStyleEngine()
      .EnsureEnvironmentVariables()
      .ResolveVariable(name, !is_ua_scope);
}

const CSSParserContext* StyleCascade::GetParserContext(
    const CSSVariableReferenceValue& value) {
  // TODO(crbug.com/985028): CSSVariableReferenceValue should always have a
  // CSSParserContext. (CSSUnparsedValue violates this).
  if (value.ParserContext())
    return value.ParserContext();
  return StrictCSSParserContext(state_.GetDocument().GetSecureContextMode());
}

bool StyleCascade::HasFontSizeDependency(const CustomProperty& property,
                                         CSSVariableData* data) const {
  if (!property.IsRegistered() || !data)
    return false;
  if (data->HasFontUnits())
    return true;
  if (data->HasRootFontUnits() && IsRootElement())
    return true;
  return false;
}

bool StyleCascade::ValidateFallback(const CustomProperty& property,
                                    CSSParserTokenRange range) const {
  if (!property.IsRegistered())
    return true;
  auto context_mode = state_.GetDocument().GetSecureContextMode();
  auto var_mode = CSSParserLocalContext::VariableMode::kTyped;
  auto* context = StrictCSSParserContext(context_mode);
  auto local_context = CSSParserLocalContext().WithVariableMode(var_mode);
  return property.ParseSingleValue(range, *context, local_context);
}

void StyleCascade::MarkReferenced(const CustomProperty& property) {
  if (!property.IsInherited())
    state_.Style()->SetHasVariableReferenceFromNonInheritedProperty();
  if (!property.IsRegistered())
    return;
  const AtomicString& name = property.GetPropertyNameAtomicString();
  state_.GetDocument().GetPropertyRegistry()->MarkReferenced(name);
}

bool StyleCascade::Filter::Add(const CSSProperty& property,
                               const Value& value) {
  Priority& slot = GetSlot(property);
  if (value.GetPriority() >= slot) {
    slot = value.GetPriority();
    return true;
  }
  return false;
}

StyleCascade::Priority& StyleCascade::Filter::GetSlot(
    const CSSProperty& property) {
  // TODO(crbug.com/985043): Ribbonize?
  switch (property.PropertyID()) {
    case CSSPropertyID::kWritingMode:
    case CSSPropertyID::kWebkitWritingMode:
      return writing_mode_;
    case CSSPropertyID::kZoom:
    case CSSPropertyID::kInternalEffectiveZoom:
      return zoom_;
    default:
      none_ = Priority();
      return none_;
  }
}

bool StyleCascade::Resolver::IsLocked(const CSSProperty& property) const {
  return IsLocked(property.GetCSSPropertyName());
}

bool StyleCascade::Resolver::IsLocked(const CSSPropertyName& name) const {
  return stack_.Contains(name);
}

bool StyleCascade::Resolver::AllowSubstitution(CSSVariableData* data) const {
  if (data && data->IsAnimationTainted() && stack_.size()) {
    const CSSPropertyName& name = stack_.back();
    if (name.IsCustomProperty())
      return true;
    const CSSProperty& property = CSSProperty::Get(name.Id());
    return !CSSAnimations::IsAnimationAffectingProperty(property);
  }
  return true;
}

bool StyleCascade::Resolver::DetectCycle(const CSSProperty& property) {
  wtf_size_t index = stack_.Find(property.GetCSSPropertyName());
  if (index == kNotFound)
    return false;
  cycle_depth_ = std::min(cycle_depth_, index);
  return true;
}

bool StyleCascade::Resolver::InCycle() const {
  return cycle_depth_ != kNotFound;
}

StyleCascade::AutoLock::AutoLock(const CSSProperty& property,
                                 Resolver& resolver)
    : AutoLock(property.GetCSSPropertyName(), resolver) {}

StyleCascade::AutoLock::AutoLock(const CSSPropertyName& name,
                                 Resolver& resolver)
    : resolver_(resolver) {
  DCHECK(!resolver.IsLocked(name));
  resolver_.stack_.push_back(name);
}

StyleCascade::AutoLock::~AutoLock() {
  resolver_.stack_.pop_back();
  if (resolver_.stack_.size() <= resolver_.cycle_depth_)
    resolver_.cycle_depth_ = kNotFound;
}

}  // namespace blink
