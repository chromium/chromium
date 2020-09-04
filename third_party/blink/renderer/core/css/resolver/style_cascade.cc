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
#include "third_party/blink/renderer/core/css/resolver/cascade_expansion.h"
#include "third_party/blink/renderer/core/css/resolver/cascade_interpolations.h"
#include "third_party/blink/renderer/core/css/resolver/cascade_resolver.h"
#include "third_party/blink/renderer/core/css/resolver/css_property_priority.h"
#include "third_party/blink/renderer/core/css/resolver/style_builder.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver_state.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/style_property_shorthand.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

namespace {

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

// TODO(crbug.com/1105782): It is currently unclear how to handle 'revert'
// at computed-value-time. For now we treat it as 'unset'.
const CSSValue* TreatRevertAsUnset(const CSSValue* value) {
  if (value && value->IsRevertValue())
    return cssvalue::CSSUnsetValue::Create();
  return value;
}

const CSSValue* Parse(const CSSProperty& property,
                      CSSParserTokenRange range,
                      const CSSParserContext* context) {
  return CSSPropertyParser::ParseSingleValue(property.PropertyID(), range,
                                             context);
}

const CSSValue* ValueAt(const MatchResult& result, uint32_t position) {
  size_t matched_properties_index = DecodeMatchedPropertiesIndex(position);
  size_t declaration_index = DecodeDeclarationIndex(position);
  const MatchedPropertiesVector& vector = result.GetMatchedProperties();
  const CSSPropertyValueSet* set = vector[matched_properties_index].properties;
  return &set->PropertyAt(declaration_index).Value();
}

PropertyHandle ToPropertyHandle(const CSSProperty& property,
                                CascadePriority priority) {
  uint32_t position = priority.GetPosition();
  CSSPropertyID id = DecodeInterpolationPropertyID(position);
  if (id == CSSPropertyID::kVariable) {
    DCHECK(IsA<CustomProperty>(property));
    return PropertyHandle(property.GetPropertyNameAtomicString());
  }
  return PropertyHandle(CSSProperty::Get(id),
                        DecodeIsPresentationAttribute(position));
}

// https://drafts.csswg.org/css-cascade-4/#default
CascadeOrigin TargetOriginForRevert(CascadeOrigin origin) {
  switch (origin) {
    case CascadeOrigin::kNone:
    case CascadeOrigin::kTransition:
      NOTREACHED();
      return CascadeOrigin::kNone;
    case CascadeOrigin::kUserAgent:
      return CascadeOrigin::kNone;
    case CascadeOrigin::kUser:
      return CascadeOrigin::kUserAgent;
    case CascadeOrigin::kAuthor:
    case CascadeOrigin::kAnimation:
      return CascadeOrigin::kUser;
  }
}

CSSPropertyID UnvisitedID(CSSPropertyID id) {
  if (id == CSSPropertyID::kVariable)
    return id;
  const CSSProperty& property = CSSProperty::Get(id);
  if (!property.IsVisited())
    return id;
  return property.GetUnvisitedProperty()->PropertyID();
}

bool IsRevert(const CSSValue& value) {
  // TODO(andruud): Don't transport CSS-wide keywords in
  // CustomPropertyDeclaration.
  return value.IsRevertValue() ||
         (value.IsCustomPropertyDeclaration() &&
          To<CSSCustomPropertyDeclaration>(value).IsRevert());
}

bool IsInterpolation(CascadePriority priority) {
  switch (priority.GetOrigin()) {
    case CascadeOrigin::kAnimation:
    case CascadeOrigin::kTransition:
      return true;
    case CascadeOrigin::kNone:
    case CascadeOrigin::kUserAgent:
    case CascadeOrigin::kUser:
    case CascadeOrigin::kAuthor:
      return false;
  }
}

}  // namespace

MatchResult& StyleCascade::MutableMatchResult() {
  needs_match_result_analyze_ = true;
  return match_result_;
}

void StyleCascade::AddInterpolations(const ActiveInterpolationsMap* map,
                                     CascadeOrigin origin) {
  DCHECK(map);
  needs_interpolations_analyze_ = true;
  interpolations_.Add(map, origin);
}

void StyleCascade::Apply(CascadeFilter filter) {
  AnalyzeIfNeeded();

  CascadeResolver resolver(filter, ++generation_);

  ApplyCascadeAffecting(resolver);

  // Affects the computed value of 'color', hence needs to happen before
  // high-priority properties.
  LookupAndApply(GetCSSPropertyColorScheme(), resolver);

  ApplyWebkitBorderImage(resolver);

  // -webkit-mask-image needs to be applied before -webkit-mask-composite,
  // otherwise -webkit-mask-composite has no effect.
  LookupAndApply(GetCSSPropertyWebkitMaskImage(), resolver);

  ApplyHighPriority(resolver);

  ApplyMatchResult(resolver);
  ApplyInterpolations(resolver);

  if (state_.Style()->HasAppearance()) {
    if (resolver.AuthorFlags() & CSSProperty::kBackground)
      state_.Style()->SetHasAuthorBackground();
    if (resolver.AuthorFlags() & CSSProperty::kBorder)
      state_.Style()->SetHasAuthorBorder();
  }
  ForceColors();
}

std::unique_ptr<CSSBitset> StyleCascade::GetImportantSet() {
  AnalyzeIfNeeded();
  if (!map_.HasImportant())
    return nullptr;
  auto set = std::make_unique<CSSBitset>();
  for (CSSPropertyID id : map_.NativeBitset()) {
    // We use the unvisited ID because visited/unvisited colors are currently
    // interpolated together.
    // TODO(crbug.com/1062217): Interpolate visited colors separately
    set->Or(UnvisitedID(id), map_.At(CSSPropertyName(id)).IsImportant());
  }
  return set;
}

void StyleCascade::Reset() {
  map_.Reset();
  match_result_.Reset();
  interpolations_.Reset();
  generation_ = 0;
  depends_on_cascade_affecting_property_ = false;
}

const CSSValue* StyleCascade::Resolve(const CSSPropertyName& name,
                                      const CSSValue& value,
                                      CascadeOrigin origin,
                                      CascadeResolver& resolver) {
  CSSPropertyRef ref(name, state_.GetDocument());

  const CSSValue* resolved =
      Resolve(ResolveSurrogate(ref.GetProperty()), value, origin, resolver);

  DCHECK(resolved);

  if (resolved->IsInvalidVariableValue())
    return nullptr;

  return resolved;
}

HeapHashMap<CSSPropertyName, Member<const CSSValue>>
StyleCascade::GetCascadedValues() const {
  DCHECK(!needs_match_result_analyze_);
  DCHECK(!needs_interpolations_analyze_);
  DCHECK_GE(generation_, 0);

  HeapHashMap<CSSPropertyName, Member<const CSSValue>> result;

  for (CSSPropertyID id : map_.NativeBitset()) {
    CSSPropertyName name(id);
    CascadePriority priority = map_.At(name);
    DCHECK(priority.HasOrigin());
    if (IsInterpolation(priority))
      continue;
    const CSSValue* cascaded = ValueAt(match_result_, priority.GetPosition());
    DCHECK(cascaded);
    result.Set(name, cascaded);
  }

  for (const auto& entry : map_.GetCustomMap()) {
    CascadePriority priority = entry.value;
    DCHECK(priority.HasOrigin());
    if (IsInterpolation(priority))
      continue;
    const CSSValue* cascaded = ValueAt(match_result_, priority.GetPosition());
    DCHECK(cascaded);
    result.Set(entry.key, cascaded);
  }

  return result;
}

void StyleCascade::AnalyzeIfNeeded() {
  if (needs_match_result_analyze_) {
    AnalyzeMatchResult();
    needs_match_result_analyze_ = false;
  }
  if (needs_interpolations_analyze_) {
    AnalyzeInterpolations();
    needs_interpolations_analyze_ = false;
  }
}

void StyleCascade::AnalyzeMatchResult() {
  for (auto e : match_result_.Expansions(GetDocument(), CascadeFilter())) {
    for (; !e.AtEnd(); e.Next()) {
      const CSSProperty& property = ResolveSurrogate(e.Property());
      map_.Add(property.GetCSSPropertyName(), e.Priority());
    }
  }

  MaybeUseCountSummaryDisplayBlock();
}

void StyleCascade::AnalyzeInterpolations() {
  const auto& entries = interpolations_.GetEntries();
  for (size_t i = 0; i < entries.size(); ++i) {
    for (const auto& active_interpolation : *entries[i].map) {
      auto name = active_interpolation.key.GetCSSPropertyName();
      uint32_t position = EncodeInterpolationPosition(
          name.Id(), i, active_interpolation.key.IsPresentationAttribute());
      CascadePriority priority(entries[i].origin, false, 0, position);

      CSSPropertyRef ref(name, GetDocument());
      DCHECK(ref.IsValid());
      const CSSProperty& property = ResolveSurrogate(ref.GetProperty());

      map_.Add(property.GetCSSPropertyName(), priority);

      // Since an interpolation for an unvisited property also causes an
      // interpolation of the visited property, add the visited property to
      // the map as well.
      // TODO(crbug.com/1062217): Interpolate visited colors separately
      if (const CSSProperty* visited = property.GetVisitedProperty())
        map_.Add(visited->GetCSSPropertyName(), priority);
    }
  }
}

void StyleCascade::Reanalyze() {
  map_.Reset();
  generation_ = 0;
  depends_on_cascade_affecting_property_ = false;

  needs_match_result_analyze_ = true;
  needs_interpolations_analyze_ = true;
  AnalyzeIfNeeded();
}

void StyleCascade::ApplyCascadeAffecting(CascadeResolver& resolver) {
  // During the initial call to Analyze, we speculatively assume that the
  // direction/writing-mode inherited from the parent will be the final
  // direction/writing-mode. If either property ends up with another value,
  // our assumption was incorrect, and we have to Reanalyze with the correct
  // values on ComputedStyle.
  auto direction = state_.Style()->Direction();
  auto writing_mode = state_.Style()->GetWritingMode();

  LookupAndApply(GetCSSPropertyDirection(), resolver);
  LookupAndApply(GetCSSPropertyWritingMode(), resolver);
  LookupAndApply(GetCSSPropertyForcedColorAdjust(), resolver);

  if (depends_on_cascade_affecting_property_) {
    // We could avoid marking these if this cascade provided a value, but
    // marking them unconditionally keeps it simple. See also note about
    // over-marking in StyleResolverState::Dependencies.
    MarkDependency(GetCSSPropertyDirection());
    MarkDependency(GetCSSPropertyWritingMode());
    if (direction != state_.Style()->Direction() ||
        writing_mode != state_.Style()->GetWritingMode()) {
      Reanalyze();
    }
  }
}

void StyleCascade::ApplyHighPriority(CascadeResolver& resolver) {
  uint64_t bits = map_.HighPriorityBits();

  if (bits) {
    using HighPriority = CSSPropertyPriorityData<kHighPropertyPriority>;
    int first = static_cast<int>(HighPriority::First());
    int last = static_cast<int>(HighPriority::Last());
    for (int i = first; i <= last; ++i) {
      if (bits & (static_cast<uint64_t>(1) << i))
        LookupAndApply(CSSProperty::Get(convertToCSSPropertyID(i)), resolver);
    }
  }

  state_.GetFontBuilder().CreateFont(state_.StyleRef(), state_.ParentStyle());
  state_.SetConversionFontSizes(CSSToLengthConversionData::FontSizes(
      state_.Style(), state_.RootElementStyle()));
  state_.SetConversionZoom(state_.Style()->EffectiveZoom());
}

void StyleCascade::ApplyWebkitBorderImage(CascadeResolver& resolver) {
  const CascadePriority* priority =
      map_.Find(CSSPropertyName(CSSPropertyID::kWebkitBorderImage));
  if (!priority)
    return;

  // -webkit-border-image is a surrogate for the border-image (shorthand).
  // By applying -webkit-border-image first, we avoid having to "partially"
  // apply -webkit-border-image depending on the border-image-* longhands that
  // have already been applied.
  // See also crbug.com/1056600
  LookupAndApply(GetCSSPropertyWebkitBorderImage(), resolver);

  const auto& shorthand = borderImageShorthand();
  const CSSProperty** longhands = shorthand.properties();
  for (unsigned i = 0; i < shorthand.length(); ++i) {
    const CSSProperty& longhand = *longhands[i];
    if (CascadePriority* p = map_.Find(longhand.GetCSSPropertyName())) {
      // If -webkit-border-image has higher priority than a border-image
      // longhand, we skip applying that longhand.
      if (*p < *priority)
        *p = CascadePriority(*p, resolver.generation_);
    }
  }
}

void StyleCascade::ApplyMatchResult(CascadeResolver& resolver) {
  for (auto e : match_result_.Expansions(GetDocument(), resolver.filter_)) {
    for (; !e.AtEnd(); e.Next()) {
      auto priority = CascadePriority(e.Priority(), resolver.generation_);
      const CSSProperty& property = ResolveSurrogate(e.Property());
      CascadePriority* p = map_.Find(property.GetCSSPropertyName());
      if (!p || *p >= priority)
        continue;
      *p = priority;
      CascadeOrigin origin = priority.GetOrigin();
      const CSSValue* value = Resolve(property, e.Value(), origin, resolver);
      StyleBuilder::ApplyProperty(property, state_, *value);
    }
  }
}

void StyleCascade::ApplyInterpolations(CascadeResolver& resolver) {
  const auto& entries = interpolations_.GetEntries();
  for (size_t i = 0; i < entries.size(); ++i) {
    const auto& entry = entries[i];
    ApplyInterpolationMap(*entry.map, entry.origin, i, resolver);
  }
}

void StyleCascade::ApplyInterpolationMap(const ActiveInterpolationsMap& map,
                                         CascadeOrigin origin,
                                         size_t index,
                                         CascadeResolver& resolver) {
  for (const auto& entry : map) {
    auto name = entry.key.GetCSSPropertyName();
    uint32_t position = EncodeInterpolationPosition(
        name.Id(), index, entry.key.IsPresentationAttribute());
    CascadePriority priority(origin, false, 0, position);
    priority = CascadePriority(priority, resolver.generation_);

    CSSPropertyRef ref(name, GetDocument());
    if (resolver.filter_.Rejects(ref.GetProperty()))
      continue;

    const CSSProperty& property = ResolveSurrogate(ref.GetProperty());

    CascadePriority* p = map_.Find(property.GetCSSPropertyName());
    if (!p || *p >= priority) {
      continue;
    }
    *p = priority;

    ApplyInterpolation(property, priority, *entry.value, resolver);
  }
}

void StyleCascade::ApplyInterpolation(
    const CSSProperty& property,
    CascadePriority priority,
    const ActiveInterpolations& interpolations,
    CascadeResolver& resolver) {
  DCHECK(!property.IsSurrogate());

  const Interpolation& interpolation = *interpolations.front();
  if (IsA<InvalidatableInterpolation>(interpolation)) {
    CSSInterpolationTypesMap map(state_.GetDocument().GetPropertyRegistry(),
                                 state_.GetDocument());
    CSSInterpolationEnvironment environment(map, state_, this, &resolver);
    InvalidatableInterpolation::ApplyStack(interpolations, environment);
  } else {
    To<TransitionInterpolation>(interpolation).Apply(state_);
  }

  // Applying a color property interpolation will also unconditionally apply
  // the -internal-visited- counterpart (see CSSColorInterpolationType::
  // ApplyStandardPropertyValue). To make sure !important rules in :visited
  // selectors win over animations, we re-apply the -internal-visited property
  // if its priority is higher.
  //
  // TODO(crbug.com/1062217): Interpolate visited colors separately
  if (const CSSProperty* visited = property.GetVisitedProperty()) {
    CascadePriority* visited_priority =
        map_.Find(visited->GetCSSPropertyName());
    if (visited_priority && priority < *visited_priority) {
      DCHECK(visited_priority->IsImportant());
      // Resetting generation to zero makes it possible to apply the
      // visited property again.
      *visited_priority = CascadePriority(*visited_priority, 0);
      LookupAndApply(*visited, resolver);
    }
  }
}

void StyleCascade::LookupAndApply(const CSSPropertyName& name,
                                  CascadeResolver& resolver) {
  CSSPropertyRef ref(name, state_.GetDocument());
  DCHECK(ref.IsValid());
  LookupAndApply(ref.GetProperty(), resolver);
}

void StyleCascade::LookupAndApply(const CSSProperty& property,
                                  CascadeResolver& resolver) {
  DCHECK(!property.IsSurrogate());

  CSSPropertyName name = property.GetCSSPropertyName();
  DCHECK(!resolver.IsLocked(property));

  CascadePriority* p = map_.Find(name);
  if (!p)
    return;
  CascadePriority priority(*p, resolver.generation_);
  if (*p >= priority)
    return;
  *p = priority;

  if (resolver.filter_.Rejects(property))
    return;

  LookupAndApplyValue(property, priority, resolver);
}

void StyleCascade::LookupAndApplyValue(const CSSProperty& property,
                                       CascadePriority priority,
                                       CascadeResolver& resolver) {
  DCHECK(!property.IsSurrogate());

  if (priority.GetOrigin() < CascadeOrigin::kAnimation)
    LookupAndApplyDeclaration(property, priority, resolver);
  else if (priority.GetOrigin() >= CascadeOrigin::kAnimation)
    LookupAndApplyInterpolation(property, priority, resolver);
}

void StyleCascade::LookupAndApplyDeclaration(const CSSProperty& property,
                                             CascadePriority priority,
                                             CascadeResolver& resolver) {
  DCHECK(!property.IsSurrogate());
  DCHECK(priority.GetOrigin() < CascadeOrigin::kAnimation);
  const CSSValue* value = ValueAt(match_result_, priority.GetPosition());
  DCHECK(value);
  value = Resolve(property, *value, priority.GetOrigin(), resolver);
  DCHECK(!value->IsVariableReferenceValue());
  DCHECK(!value->IsPendingSubstitutionValue());
  StyleBuilder::ApplyProperty(property, state_, *value);
}

void StyleCascade::LookupAndApplyInterpolation(const CSSProperty& property,
                                               CascadePriority priority,
                                               CascadeResolver& resolver) {
  DCHECK(!property.IsSurrogate());

  // Interpolations for -internal-visited properties are applied via the
  // interpolation for the main (unvisited) property, so we don't need to
  // apply it twice.
  // TODO(crbug.com/1062217): Interpolate visited colors separately
  if (property.IsVisited())
    return;
  DCHECK(priority.GetOrigin() >= CascadeOrigin::kAnimation);
  size_t index = DecodeInterpolationIndex(priority.GetPosition());
  DCHECK_LE(index, interpolations_.GetEntries().size());
  const ActiveInterpolationsMap& map = *interpolations_.GetEntries()[index].map;
  PropertyHandle handle = ToPropertyHandle(property, priority);
  const auto& entry = map.find(handle);
  DCHECK_NE(entry, map.end());
  ApplyInterpolation(property, priority, *entry->value, resolver);
}

void StyleCascade::ForceColors() {
  // TODO(almaher): Return if the color is already a system color.
  if (!GetDocument().InForcedColorsMode() ||
      state_.Style()->ForcedColorAdjust() == EForcedColorAdjust::kNone)
    return;

  int bg_color_alpha =
      state_.Style()
          ->VisitedDependentColor(GetCSSPropertyBackgroundColor())
          .Alpha();

  // TODO(almaher): Do this for all Forced Colors Mode properties.
  CSSPropertyName name(CSSPropertyID::kBackgroundColor);
  CSSPropertyRef ref(name, state_.GetDocument());
  DCHECK(ref.IsValid());

  StyleBuilder::ApplyProperty(ref.GetProperty(), state_,
                              *GetForcedColorValue(name));

  // Preserve the author/user defined background alpha channel.
  state_.Style()->SetBackgroundColor(
      StyleColor(state_.Style()->BackgroundColor().ResolveWithAlpha(
          state_.Style()->GetCurrentColor(), WebColorScheme::kLight,
          bg_color_alpha)));
}

const CSSValue* StyleCascade::GetForcedColorValue(CSSPropertyName name) {
  CascadePriority* p = map_.Find(name, CascadeOrigin::kUserAgent);
  if (p)
    return ValueAt(match_result_, p->GetPosition());
  if (name.Id() == CSSPropertyID::kBackgroundColor)
    return CSSIdentifierValue::Create(CSSValueID::kCanvas);
  return cssvalue::CSSUnsetValue::Create();
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
  return CSSVariableData::CreateResolved(
      std::move(tokens_), std::move(backing_strings_), is_animation_tainted_,
      has_font_units_, has_root_font_units_, base_url_, charset_);
}

bool StyleCascade::ShouldRevert(const CSSProperty& property,
                                const CSSValue& value,
                                CascadeOrigin origin) {
  return IsRevert(value) ||
         (state_.GetDocument().InForcedColorsMode() &&
          state_.Style()->ForcedColorAdjust() != EForcedColorAdjust::kNone &&
          property.IsAffectedByForcedColors() &&
          !(property.PropertyID() == CSSPropertyID::kBackgroundImage &&
            value.MayContainUrl()) &&
          origin >= CascadeOrigin::kAuthor);
}

const CSSValue* StyleCascade::Resolve(const CSSProperty& property,
                                      const CSSValue& value,
                                      CascadeOrigin origin,
                                      CascadeResolver& resolver) {
  DCHECK(!property.IsSurrogate());
  if (ShouldRevert(property, value, origin))
    return ResolveRevert(property, value, origin, resolver);
  resolver.CollectAuthorFlags(property, origin);
  if (const auto* v = DynamicTo<CSSCustomPropertyDeclaration>(value))
    return ResolveCustomProperty(property, *v, origin, resolver);
  if (const auto* v = DynamicTo<CSSVariableReferenceValue>(value))
    return ResolveVariableReference(property, *v, resolver);
  if (const auto* v = DynamicTo<cssvalue::CSSPendingSubstitutionValue>(value))
    return ResolvePendingSubstitution(property, *v, resolver);
  return &value;
}

const CSSValue* StyleCascade::ResolveCustomProperty(
    const CSSProperty& property,
    const CSSCustomPropertyDeclaration& decl,
    CascadeOrigin origin,
    CascadeResolver& resolver) {
  DCHECK(!property.IsSurrogate());

  // TODO(andruud): Don't transport css-wide keywords in this value.
  if (!decl.Value())
    return &decl;

  DCHECK(!resolver.IsLocked(property));
  CascadeResolver::AutoLock lock(property, resolver);

  scoped_refptr<CSSVariableData> data = decl.Value();

  if (data->NeedsVariableResolution())
    data = ResolveVariableData(data.get(), resolver);

  if (HasFontSizeDependency(To<CustomProperty>(property), data.get()))
    resolver.DetectCycle(GetCSSPropertyFontSize());

  state_.Style()->SetHasVariableDeclaration();

  if (resolver.InCycle())
    return CSSInvalidVariableValue::Create();

  if (!data) {
    MaybeUseCountInvalidVariableUnset(To<CustomProperty>(property));
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
    CascadeResolver& resolver) {
  DCHECK(!property.IsSurrogate());
  DCHECK(!resolver.IsLocked(property));
  CascadeResolver::AutoLock lock(property, resolver);

  const CSSVariableData* data = value.VariableDataValue();
  const CSSParserContext* context = GetParserContext(value);

  MarkHasVariableReference(property);

  DCHECK(data);
  DCHECK(context);

  TokenSequence sequence;

  if (ResolveTokensInto(data->Tokens(), resolver, sequence)) {
    if (const auto* parsed = Parse(property, sequence.TokenRange(), context))
      return TreatRevertAsUnset(parsed);
  }

  return cssvalue::CSSUnsetValue::Create();
}

const CSSValue* StyleCascade::ResolvePendingSubstitution(
    const CSSProperty& property,
    const cssvalue::CSSPendingSubstitutionValue& value,
    CascadeResolver& resolver) {
  DCHECK(!property.IsSurrogate());
  DCHECK(!resolver.IsLocked(property));
  CascadeResolver::AutoLock lock(property, resolver);

  CascadePriority priority = map_.At(property.GetCSSPropertyName());
  DCHECK_NE(property.PropertyID(), CSSPropertyID::kVariable);
  DCHECK_NE(priority.GetOrigin(), CascadeOrigin::kNone);

  MarkHasVariableReference(property);

  // If the previous call to ResolvePendingSubstitution parsed 'value', then
  // we don't need to do it again.
  bool is_cached = resolver.shorthand_cache_.value == &value;

  if (!is_cached) {
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

    resolver.shorthand_cache_.value = &value;
    resolver.shorthand_cache_.parsed_properties = std::move(parsed_properties);
  }

  const auto& parsed_properties = resolver.shorthand_cache_.parsed_properties;

  // For -internal-visited-properties with CSSPendingSubstitutionValues,
  // the inner 'shorthand_property_id' will expand to a set of longhands
  // containing the unvisited equivalent. Hence, when parsing the
  // CSSPendingSubstitutionValue, we look for the unvisited property in
  // parsed_properties.
  const CSSProperty* unvisited_property =
      property.IsVisited() ? property.GetUnvisitedProperty() : &property;

  unsigned parsed_properties_count = parsed_properties.size();
  for (unsigned i = 0; i < parsed_properties_count; ++i) {
    const CSSProperty& longhand = CSSProperty::Get(parsed_properties[i].Id());
    const CSSValue* parsed = parsed_properties[i].Value();

    // When using var() in a css-logical shorthand (e.g. margin-inline),
    // the longhands here will also be logical.
    if (unvisited_property == &ResolveSurrogate(longhand))
      return TreatRevertAsUnset(parsed);
  }

  NOTREACHED();
  return cssvalue::CSSUnsetValue::Create();
}

const CSSValue* StyleCascade::ResolveRevert(const CSSProperty& property,
                                            const CSSValue& value,
                                            CascadeOrigin origin,
                                            CascadeResolver& resolver) {
  MaybeUseCountRevert(value);

  CascadeOrigin target_origin = TargetOriginForRevert(origin);

  switch (target_origin) {
    case CascadeOrigin::kTransition:
    case CascadeOrigin::kNone:
      return cssvalue::CSSUnsetValue::Create();
    case CascadeOrigin::kUserAgent:
    case CascadeOrigin::kUser:
    case CascadeOrigin::kAuthor:
    case CascadeOrigin::kAnimation: {
      CascadePriority* p =
          map_.Find(property.GetCSSPropertyName(), target_origin);
      if (!p)
        return cssvalue::CSSUnsetValue::Create();
      return Resolve(property, *ValueAt(match_result_, p->GetPosition()),
                     target_origin, resolver);
    }
  }
}

scoped_refptr<CSSVariableData> StyleCascade::ResolveVariableData(
    CSSVariableData* data,
    CascadeResolver& resolver) {
  DCHECK(data && data->NeedsVariableResolution());

  TokenSequence sequence(data);

  if (!ResolveTokensInto(data->Tokens(), resolver, sequence))
    return nullptr;

  return sequence.BuildVariableData();
}

bool StyleCascade::ResolveTokensInto(CSSParserTokenRange range,
                                     CascadeResolver& resolver,
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
                                  CascadeResolver& resolver,
                                  TokenSequence& out) {
  AtomicString variable_name = ConsumeVariableName(range);
  DCHECK(range.AtEnd() || (range.Peek().GetType() == kCommaToken));

  CustomProperty property(variable_name, state_.GetDocument());

  // Any custom property referenced (by anything, even just once) in the
  // document can currently not be animated on the compositor. Hence we mark
  // properties that have been referenced.
  DCHECK(resolver.CurrentProperty());
  MarkIsReferenced(*resolver.CurrentProperty(), property);

  if (!resolver.DetectCycle(property)) {
    // We are about to substitute var(property). In order to do that, we must
    // know the computed value of 'property', hence we Apply it.
    //
    // We can however not do this if we're in a cycle. If a cycle is detected
    // here, it means we are already resolving 'property', and have discovered
    // a reference to 'property' during that resolution.
    LookupAndApply(property, resolver);
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
                                  CascadeResolver& resolver,
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
  return StrictCSSParserContext(
      state_.GetDocument().GetExecutionContext()->GetSecureContextMode());
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
  auto context_mode =
      state_.GetDocument().GetExecutionContext()->GetSecureContextMode();
  auto var_mode = CSSParserLocalContext::VariableMode::kTyped;
  auto* context = StrictCSSParserContext(context_mode);
  auto local_context = CSSParserLocalContext().WithVariableMode(var_mode);
  return property.ParseSingleValue(range, *context, local_context);
}

void StyleCascade::MarkIsReferenced(const CSSProperty& referencer,
                                    const CustomProperty& referenced) {
  // For simplicity, we mark all inherited custom property references as
  // dependencies, even though it might not be a dependency if this cascade
  // defines a value for that property.
  if (!referencer.IsInherited() && referenced.IsInherited())
    MarkDependency(referenced);
  if (!referenced.IsRegistered())
    return;
  const AtomicString& name = referenced.GetPropertyNameAtomicString();
  state_.GetDocument().EnsurePropertyRegistry().MarkReferenced(name);
}

void StyleCascade::MarkHasVariableReference(const CSSProperty& property) {
  if (!property.IsInherited())
    state_.Style()->SetHasVariableReferenceFromNonInheritedProperty();
  state_.Style()->SetHasVariableReference();
}

void StyleCascade::MarkDependency(const CSSProperty& property) {
  state_.MarkDependency(property);
}

const Document& StyleCascade::GetDocument() const {
  return state_.GetDocument();
}

const CSSProperty& StyleCascade::ResolveSurrogate(const CSSProperty& property) {
  if (!property.IsSurrogate())
    return property;
  // This marks the cascade as dependent on cascade-affecting properties
  // even for simple surrogates like -webkit-writing-mode, but there isn't
  // currently a flag to distinguish such surrogates from e.g. css-logical
  // properties.
  depends_on_cascade_affecting_property_ = true;
  const CSSProperty* original = property.SurrogateFor(
      state_.Style()->Direction(), state_.Style()->GetWritingMode());
  DCHECK(original);
  return *original;
}

void StyleCascade::CountUse(WebFeature feature) {
  GetDocument().CountUse(feature);
}

void StyleCascade::MaybeUseCountRevert(const CSSValue& value) {
  // In forced colors mode, any value can behave like 'revert' [1], but we
  // should only use-count the true uses of 'revert'.
  // [1] https://drafts.csswg.org/css-color-adjust-1/#forced-colors-properties
  if (IsRevert(value))
    CountUse(WebFeature::kCSSKeywordRevert);
}

// TODO(crbug.com/590014): Remove this when display type of <summary> is fixed
void StyleCascade::MaybeUseCountSummaryDisplayBlock() {
  if (!state_.GetElement().HasTagName(html_names::kSummaryTag))
    return;
  CascadePriority priority = map_.At(CSSPropertyName(CSSPropertyID::kDisplay));
  if (priority.GetOrigin() <= CascadeOrigin::kUserAgent)
    return;
  const CSSValue* value = ValueAt(match_result_, priority.GetPosition());
  if (auto* identifier = DynamicTo<CSSIdentifierValue>(value)) {
    if (identifier->GetValueID() == CSSValueID::kBlock)
      CountUse(WebFeature::kSummaryElementWithDisplayBlockAuthorRule);
  }
}

void StyleCascade::MaybeUseCountInvalidVariableUnset(
    const CustomProperty& property) {
  if (!property.SupportsGuaranteedInvalid())
    return;
  if (!property.IsInherited() && !property.HasInitialValue())
    return;
  const AtomicString& name = property.GetPropertyNameAtomicString();
  const ComputedStyle* parent_style = state_.ParentStyle();
  if (parent_style &&
      parent_style->GetVariableData(name, property.IsInherited())) {
    CountUse(WebFeature::kCSSInvalidVariableUnset);
  }
}

}  // namespace blink
