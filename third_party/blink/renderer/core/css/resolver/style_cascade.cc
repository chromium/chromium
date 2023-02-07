// Copyright 2019 The Chromium Authors
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
#include "third_party/blink/renderer/core/css/css_cyclic_variable_value.h"
#include "third_party/blink/renderer/core/css/css_font_selector.h"
#include "third_party/blink/renderer/core/css/css_invalid_variable_value.h"
#include "third_party/blink/renderer/core/css/css_numeric_literal_value.h"
#include "third_party/blink/renderer/core/css/css_pending_substitution_value.h"
#include "third_party/blink/renderer/core/css/css_unset_value.h"
#include "third_party/blink/renderer/core/css/css_variable_data.h"
#include "third_party/blink/renderer/core/css/css_variable_reference_value.h"
#include "third_party/blink/renderer/core/css/document_style_environment_variables.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_local_context.h"
#include "third_party/blink/renderer/core/css/parser/css_property_parser.h"
#include "third_party/blink/renderer/core/css/properties/css_parsing_utils.h"
#include "third_party/blink/renderer/core/css/properties/css_property.h"
#include "third_party/blink/renderer/core/css/properties/css_property_ref.h"
#include "third_party/blink/renderer/core/css/properties/longhands.h"
#include "third_party/blink/renderer/core/css/property_bitsets.h"
#include "third_party/blink/renderer/core/css/property_registry.h"
#include "third_party/blink/renderer/core/css/resolver/cascade_expansion-inl.h"
#include "third_party/blink/renderer/core/css/resolver/cascade_expansion.h"
#include "third_party/blink/renderer/core/css/resolver/cascade_interpolations.h"
#include "third_party/blink/renderer/core/css/resolver/cascade_resolver.h"
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

const CSSValue* Parse(const CSSProperty& property,
                      CSSParserTokenRange range,
                      const CSSParserContext* context) {
  return CSSPropertyParser::ParseSingleValue(property.PropertyID(), range,
                                             context);
}

const CSSValue* ValueAt(const MatchResult& result, uint32_t position) {
  wtf_size_t matched_properties_index = DecodeMatchedPropertiesIndex(position);
  wtf_size_t declaration_index = DecodeDeclarationIndex(position);
  const MatchedPropertiesVector& vector = result.GetMatchedProperties();
  const CSSPropertyValueSet* set = vector[matched_properties_index].properties;
  return &set->PropertyAt(declaration_index).Value();
}

const TreeScope& TreeScopeAt(const MatchResult& result, uint32_t position) {
  wtf_size_t matched_properties_index = DecodeMatchedPropertiesIndex(position);
  const MatchedProperties& properties =
      result.GetMatchedProperties()[matched_properties_index];
  DCHECK_EQ(properties.types_.origin, CascadeOrigin::kAuthor);
  return result.ScopeFromTreeOrder(properties.types_.tree_order);
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
    case CascadeOrigin::kAuthorPresentationalHint:
    case CascadeOrigin::kAuthor:
    case CascadeOrigin::kAnimation:
      return CascadeOrigin::kUser;
  }
}

CSSPropertyID UnvisitedID(CSSPropertyID id) {
  if (id == CSSPropertyID::kVariable) {
    return id;
  }
  const CSSProperty& property = CSSProperty::Get(id);
  if (!property.IsVisited()) {
    return id;
  }
  return property.GetUnvisitedProperty()->PropertyID();
}

bool IsInterpolation(CascadePriority priority) {
  switch (priority.GetOrigin()) {
    case CascadeOrigin::kAnimation:
    case CascadeOrigin::kTransition:
      return true;
    case CascadeOrigin::kNone:
    case CascadeOrigin::kUserAgent:
    case CascadeOrigin::kUser:
    case CascadeOrigin::kAuthorPresentationalHint:
    case CascadeOrigin::kAuthor:
      return false;
  }
}

#if DCHECK_IS_ON()

bool HasUnresolvedReferences(CSSParserTokenRange range) {
  while (!range.AtEnd()) {
    switch (range.Consume().FunctionId()) {
      case CSSValueID::kVar:
      case CSSValueID::kEnv:
        return true;
      default:
        continue;
    }
  }
  return false;
}

#endif  // DCHECK_IS_ON()

}  // namespace

MatchResult& StyleCascade::MutableMatchResult() {
  DCHECK(!generation_) << "Apply has already been called";
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
  state_.UpdateLengthConversionData();

  CascadeResolver resolver(filter, ++generation_);

  ApplyCascadeAffecting(resolver);

  if (map_.NativeBitset().Has(CSSPropertyID::kColorScheme)) {
    // Affects the computed value of 'color', hence needs to happen before
    // high-priority properties.
    LookupAndApply(GetCSSPropertyColorScheme(), resolver);
  }

  if (map_.NativeBitset().Has(CSSPropertyID::kMathDepth)) {
    // Affects the computed value of 'font-size', hence needs to happen before
    // high-priority properties.
    LookupAndApply(GetCSSPropertyMathDepth(), resolver);
  }

  if (map_.NativeBitset().Has(CSSPropertyID::kWebkitMaskImage)) {
    // -webkit-mask-image needs to be applied before -webkit-mask-composite,
    // otherwise -webkit-mask-composite has no effect.
    LookupAndApply(GetCSSPropertyWebkitMaskImage(), resolver);
  }

  if (map_.NativeBitset().Has(CSSPropertyID::kForcedColorAdjust)) {
    // Affects the computed value of color when it is inherited and
    // forced-color- adjust is set to preserve-parent-color.
    LookupAndApply(GetCSSPropertyForcedColorAdjust(), resolver);
  }

  ApplyHighPriority(resolver);

  if (map_.NativeBitset().Has(CSSPropertyID::kLineHeight)) {
    LookupAndApply(GetCSSPropertyLineHeight(), resolver);
  }
  state_.UpdateLineHeight();

  ApplyWideOverlapping(resolver);

  ApplyMatchResult(resolver);
  ApplyInterpolations(resolver);

  // These three flags are only used if HasAppearance() is set
  // (they are used for knowing whether appearance: auto is to be overridden),
  // but we compute them nevertheless, to avoid suddenly having to compute them
  // after-the-fact if inline style is updated incrementally.
  if (resolver.AuthorFlags() & CSSProperty::kBackground) {
    state_.StyleBuilder().SetHasAuthorBackground();
  }
  if (resolver.AuthorFlags() & CSSProperty::kBorder) {
    state_.StyleBuilder().SetHasAuthorBorder();
  }
  if (resolver.AuthorFlags() & CSSProperty::kBorderRadius) {
    state_.StyleBuilder().SetHasAuthorBorderRadius();
  }

  if ((state_.StyleBuilder().InsideLink() != EInsideLink::kInsideVisitedLink &&
       (resolver.AuthorFlags() & CSSProperty::kHighlightColors)) ||
      (state_.StyleBuilder().InsideLink() == EInsideLink::kInsideVisitedLink &&
       (resolver.AuthorFlags() & CSSProperty::kVisitedHighlightColors))) {
    state_.StyleBuilder().SetHasAuthorHighlightColors();
  }

  if (resolver.Flags() & CSSProperty::kAnimation) {
    state_.SetCanAffectAnimations();
  }
  if (resolver.RejectedFlags() & CSSProperty::kLegacyOverlapping) {
    state_.SetRejectedLegacyOverlapping();
  }

  // TOOD(crbug.com/1334570):
  //
  // Count applied H1 font-size from html.css UA stylesheet where H1 is inside
  // a sectioning element matching selectors like:
  //
  // :-webkit-any(article,aside,nav,section) h1 { ... }
  //
  if (!state_.GetElement().HasTagName(html_names::kH1Tag)) {
    return;
  }
  if (CascadePriority* priority =
          map_.Find(GetCSSPropertyFontSize().GetCSSPropertyName())) {
    if (priority->GetOrigin() != CascadeOrigin::kUserAgent) {
      return;
    }
    const CSSValue* value = ValueAt(match_result_, priority->GetPosition());
    if (const auto* numeric = DynamicTo<CSSNumericLiteralValue>(value)) {
      DCHECK(numeric->GetType() == CSSNumericLiteralValue::UnitType::kEms);
      if (numeric->DoubleValue() != 2.0) {
        CountUse(WebFeature::kH1UserAgentFontSizeInSectionApplied);
      }
    }
  }
}

std::unique_ptr<CSSBitset> StyleCascade::GetImportantSet() {
  AnalyzeIfNeeded();
  if (!map_.HasImportant()) {
    return nullptr;
  }
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

  const CSSValue* resolved = Resolve(ResolveSurrogate(ref.GetProperty()), value,
                                     CascadePriority(origin), origin, resolver);

  DCHECK(resolved);

  // TODO(crbug.com/1185745): Cycles in animations get special handling by our
  // implementation. This is not per spec, but the correct behavior is not
  // defined at the moment.
  if (resolved->IsCyclicVariableValue()) {
    return nullptr;
  }

  // TODO(crbug.com/1185745): We should probably not return 'unset' for
  // properties where CustomProperty::SupportsGuaranteedInvalid return true.
  if (resolved->IsInvalidVariableValue()) {
    return cssvalue::CSSUnsetValue::Create();
  }

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
    if (IsInterpolation(priority)) {
      continue;
    }
    const CSSValue* cascaded = ValueAt(match_result_, priority.GetPosition());
    DCHECK(cascaded);
    result.Set(name, cascaded);
  }

  for (const auto& name : map_.GetCustomMap().Keys()) {
    CascadePriority priority = map_.At(name);
    DCHECK(priority.HasOrigin());
    if (IsInterpolation(priority)) {
      continue;
    }
    const CSSValue* cascaded = ValueAt(match_result_, priority.GetPosition());
    DCHECK(cascaded);
    result.Set(name, cascaded);
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
  int index = 0;
  for (const MatchedProperties& properties :
       match_result_.GetMatchedProperties()) {
    ExpandCascade(
        properties, GetDocument(), index++,
        [this](CascadePriority cascade_priority,
               const CSSProperty& css_property, const CSSPropertyName& name,
               const CSSValue& css_value [[maybe_unused]],
               uint16_t tree_order [[maybe_unused]]) {
          if (css_property.IsSurrogate()) {
            const CSSProperty& property = ResolveSurrogate(css_property);
            map_.Add(property.GetCSSPropertyName(), cascade_priority);
          } else {
            map_.Add(name, cascade_priority);
          }
        });
  }
}

void StyleCascade::AnalyzeInterpolations() {
  const auto& entries = interpolations_.GetEntries();
  for (wtf_size_t i = 0; i < entries.size(); ++i) {
    for (const auto& active_interpolation : *entries[i].map) {
      auto name = active_interpolation.key.GetCSSPropertyName();
      uint32_t position = EncodeInterpolationPosition(
          name.Id(), i, active_interpolation.key.IsPresentationAttribute());
      CascadePriority priority(entries[i].origin, false, 0, false, 0, position);

      CSSPropertyRef ref(name, GetDocument());
      DCHECK(ref.IsValid());
      const CSSProperty& property = ResolveSurrogate(ref.GetProperty());

      map_.Add(property.GetCSSPropertyName(), priority);

      // Since an interpolation for an unvisited property also causes an
      // interpolation of the visited property, add the visited property to
      // the map as well.
      // TODO(crbug.com/1062217): Interpolate visited colors separately
      if (kPropertiesWithVisited.Has(property.PropertyID())) {
        if (const CSSProperty* visited = property.GetVisitedProperty()) {
          map_.Add(visited->GetCSSPropertyName(), priority);
        }
      }
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
  auto direction = state_.StyleBuilder().Direction();
  auto writing_mode = state_.StyleBuilder().GetWritingMode();

  if (map_.NativeBitset().Has(CSSPropertyID::kDirection)) {
    LookupAndApply(GetCSSPropertyDirection(), resolver);
  }
  if (map_.NativeBitset().Has(CSSPropertyID::kWritingMode)) {
    LookupAndApply(GetCSSPropertyWritingMode(), resolver);
  }

  if (depends_on_cascade_affecting_property_) {
    if (direction != state_.StyleBuilder().Direction() ||
        writing_mode != state_.StyleBuilder().GetWritingMode()) {
      Reanalyze();
    }
  }
}

void StyleCascade::ApplyHighPriority(CascadeResolver& resolver) {
  uint64_t bits = map_.HighPriorityBits();

  if (bits) {
    int first = static_cast<int>(kFirstHighPriorityCSSProperty);
    int last = static_cast<int>(kLastHighPriorityCSSProperty);
    for (int i = first; i <= last; ++i) {
      if (bits & (static_cast<uint64_t>(1) << i)) {
        LookupAndApply(CSSProperty::Get(ConvertToCSSPropertyID(i)), resolver);
      }
    }
  }

  state_.UpdateFont();
}

void StyleCascade::ApplyWideOverlapping(CascadeResolver& resolver) {
  // Overlapping properties are handled as follows:
  //
  // 1. Apply the "wide" longhand which represents the entire computed value
  //    first. This is not always the non-legacy property,
  //    e.g.-webkit-border-image is one such longhand.
  // 2. For the other overlapping longhands (each of which represent a *part*
  //    of that computed value), *skip* applying that longhand if the wide
  //    longhand has a higher priority.
  //
  // This allows us to always apply the "wide" longhand in a fixed order versus
  // the other overlapping longhands, but still produce the same result as if
  // everything was applied in the order the properties were specified.

  // Skip `property` if its priority is lower than the incoming priority.
  // Skipping basically means pretending it's already applied by setting the
  // generation.
  auto maybe_skip = [this, &resolver](const CSSProperty& property,
                                      CascadePriority priority) {
    if (CascadePriority* p = map_.Find(property.GetCSSPropertyName())) {
      if (*p < priority) {
        *p = CascadePriority(*p, resolver.generation_);
      }
    }
  };

  const CSSProperty& webkit_border_image = GetCSSPropertyWebkitBorderImage();
  if (!resolver.filter_.Rejects(webkit_border_image)) {
    if (const CascadePriority* priority =
            map_.Find(webkit_border_image.GetCSSPropertyName())) {
      LookupAndApply(webkit_border_image, resolver);

      const auto& shorthand = borderImageShorthand();
      const CSSProperty** longhands = shorthand.properties();
      for (unsigned i = 0; i < shorthand.length(); ++i) {
        maybe_skip(*longhands[i], *priority);
      }
    }
  }

  const CSSProperty& perspective_origin = GetCSSPropertyPerspectiveOrigin();
  if (!resolver.filter_.Rejects(perspective_origin)) {
    if (const CascadePriority* priority =
            map_.Find(perspective_origin.GetCSSPropertyName())) {
      LookupAndApply(perspective_origin, resolver);
      maybe_skip(GetCSSPropertyWebkitPerspectiveOriginX(), *priority);
      maybe_skip(GetCSSPropertyWebkitPerspectiveOriginY(), *priority);
    }
  }

  const CSSProperty& transform_origin = GetCSSPropertyTransformOrigin();
  if (!resolver.filter_.Rejects(transform_origin)) {
    if (const CascadePriority* priority =
            map_.Find(transform_origin.GetCSSPropertyName())) {
      LookupAndApply(transform_origin, resolver);
      maybe_skip(GetCSSPropertyWebkitTransformOriginX(), *priority);
      maybe_skip(GetCSSPropertyWebkitTransformOriginY(), *priority);
      maybe_skip(GetCSSPropertyWebkitTransformOriginZ(), *priority);
    }
  }

  // vertical-align will become a shorthand in the future - in order to
  // mitigate the forward compat risk, skip the baseline-source longhand.
  const CSSProperty& vertical_align = GetCSSPropertyVerticalAlign();
  if (!resolver.filter_.Rejects(vertical_align)) {
    if (const CascadePriority* priority =
            map_.Find(vertical_align.GetCSSPropertyName())) {
      LookupAndApply(vertical_align, resolver);
      maybe_skip(GetCSSPropertyBaselineSource(), *priority);
    }
  }
}

// Go through all properties that were found during the analyze phase
// (e.g. in AnalyzeMatchResult()) and actually apply them. We need to do this
// in a second phase so that we know which ones actually won the cascade
// before we start applying, as some properties can affect others.
void StyleCascade::ApplyMatchResult(CascadeResolver& resolver) {
  for (CSSPropertyID id : map_.NativeBitset()) {
    CascadePriority* p = map_.FindKnownToExist(id);
    const CascadePriority priority = *p;
    if (priority.GetGeneration() >= resolver.generation_) {
      // Already applied this generation.
      // Also checked in LookupAndApplyDeclaration,
      // but done here to get a fast exit.
      continue;
    }
    if (IsInterpolation(priority)) {
      continue;
    }

    const CSSProperty& property = CSSProperty::Get(id);
    if (resolver.Rejects(property)) {
      continue;
    }
    LookupAndApplyDeclaration(property, p, resolver);
  }

  for (auto& [name, priority_list] : map_.GetCustomMap()) {
    CascadePriority* p = &map_.Top(priority_list);
    CascadePriority priority = *p;
    if (priority.GetGeneration() >= resolver.generation_) {
      continue;
    }
    if (IsInterpolation(priority)) {
      continue;
    }

    CustomProperty property(name.ToAtomicString(), GetDocument());
    if (resolver.Rejects(property)) {
      continue;
    }
    LookupAndApplyDeclaration(property, p, resolver);
  }
}

void StyleCascade::ApplyInterpolations(CascadeResolver& resolver) {
  const auto& entries = interpolations_.GetEntries();
  for (wtf_size_t i = 0; i < entries.size(); ++i) {
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
    CascadePriority priority(origin, false, 0, false, 0, position);
    priority = CascadePriority(priority, resolver.generation_);

    CSSPropertyRef ref(name, GetDocument());
    if (resolver.Rejects(ref.GetProperty())) {
      continue;
    }

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

  CSSInterpolationTypesMap map(state_.GetDocument().GetPropertyRegistry(),
                               state_.GetDocument());
  CSSInterpolationEnvironment environment(map, state_, this, &resolver);

  const Interpolation& interpolation = *interpolations.front();
  if (IsA<InvalidatableInterpolation>(interpolation)) {
    InvalidatableInterpolation::ApplyStack(interpolations, environment);
  } else {
    To<TransitionInterpolation>(interpolation).Apply(environment);
  }

  // Applying a color property interpolation will also unconditionally apply
  // the -internal-visited- counterpart (see CSSColorInterpolationType::
  // ApplyStandardPropertyValue). To make sure !important rules in :visited
  // selectors win over animations, we re-apply the -internal-visited property
  // if its priority is higher.
  //
  // TODO(crbug.com/1062217): Interpolate visited colors separately
  if (kPropertiesWithVisited.Has(property.PropertyID())) {
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

  CascadePriority* priority = map_.Find(name);
  if (!priority) {
    return;
  }

  if (resolver.Rejects(property)) {
    return;
  }

  LookupAndApplyValue(property, priority, resolver);
}

void StyleCascade::LookupAndApplyValue(const CSSProperty& property,
                                       CascadePriority* priority,
                                       CascadeResolver& resolver) {
  DCHECK(!property.IsSurrogate());

  if (priority->GetOrigin() < CascadeOrigin::kAnimation) {
    LookupAndApplyDeclaration(property, priority, resolver);
  } else if (priority->GetOrigin() >= CascadeOrigin::kAnimation) {
    LookupAndApplyInterpolation(property, priority, resolver);
  }
}

void StyleCascade::LookupAndApplyDeclaration(const CSSProperty& property,
                                             CascadePriority* priority,
                                             CascadeResolver& resolver) {
  if (priority->GetGeneration() >= resolver.generation_) {
    // Already applied this generation.
    return;
  }
  *priority = CascadePriority(*priority, resolver.generation_);
  DCHECK(!property.IsSurrogate());
  DCHECK(priority->GetOrigin() < CascadeOrigin::kAnimation);
  const CSSValue* value = ValueAt(match_result_, priority->GetPosition());
  DCHECK(value);
  CascadeOrigin origin = priority->GetOrigin();
  value = Resolve(property, *value, *priority, origin, resolver);
  DCHECK(!value->IsVariableReferenceValue());
  DCHECK(!value->IsPendingSubstitutionValue());
  const TreeScope* tree_scope{nullptr};
  if (origin == CascadeOrigin::kAuthor) {
    tree_scope = &TreeScopeAt(match_result_, priority->GetPosition());
  } else if (origin == CascadeOrigin::kAuthorPresentationalHint) {
    tree_scope = &GetDocument();
  }
  StyleBuilder::ApplyPhysicalProperty(property, state_,
                                      value->EnsureScopedValue(tree_scope));
}

void StyleCascade::LookupAndApplyInterpolation(const CSSProperty& property,
                                               CascadePriority* priority,
                                               CascadeResolver& resolver) {
  if (priority->GetGeneration() >= resolver.generation_) {
    // Already applied this generation.
    return;
  }
  *priority = CascadePriority(*priority, resolver.generation_);

  DCHECK(!property.IsSurrogate());

  // Interpolations for -internal-visited properties are applied via the
  // interpolation for the main (unvisited) property, so we don't need to
  // apply it twice.
  // TODO(crbug.com/1062217): Interpolate visited colors separately
  if (property.IsVisited()) {
    return;
  }
  DCHECK(priority->GetOrigin() >= CascadeOrigin::kAnimation);
  wtf_size_t index = DecodeInterpolationIndex(priority->GetPosition());
  DCHECK_LE(index, interpolations_.GetEntries().size());
  const ActiveInterpolationsMap& map = *interpolations_.GetEntries()[index].map;
  PropertyHandle handle = ToPropertyHandle(property, *priority);
  const auto& entry = map.find(handle);
  DCHECK_NE(entry, map.end());
  ApplyInterpolation(property, *priority, *entry->value, resolver);
}

bool StyleCascade::IsRootElement() const {
  return &state_.GetElement() == state_.GetDocument().documentElement();
}

StyleCascade::TokenSequence::TokenSequence(const CSSVariableData* data)
    : is_animation_tainted_(data->IsAnimationTainted()),
      has_font_units_(data->HasFontUnits()),
      has_root_font_units_(data->HasRootFontUnits()),
      has_line_height_units_(data->HasLineHeightUnits()) {}

bool StyleCascade::TokenSequence::AppendTokens(
    base::span<const CSSParserToken> tokens,
    wtf_size_t limit) {
  // https://drafts.csswg.org/css-variables/#long-variables
  if (tokens.size() > limit) {
    return false;
  }
  tokens_.Append(tokens.data(), static_cast<wtf_size_t>(tokens.size()));
  return true;
}

bool StyleCascade::TokenSequence::Append(const TokenSequence& sequence,
                                         wtf_size_t limit) {
  if (!AppendTokens(base::span<const CSSParserToken>{sequence.tokens_},
                    limit)) {
    return false;
  }
  is_animation_tainted_ |= sequence.is_animation_tainted_;
  has_font_units_ |= sequence.has_font_units_;
  has_root_font_units_ |= sequence.has_root_font_units_;
  has_line_height_units_ |= sequence.has_line_height_units_;
  return true;
}

bool StyleCascade::TokenSequence::Append(CSSVariableData* data,
                                         wtf_size_t limit) {
  if (!AppendTokens(data->Tokens(), limit)) {
    return false;
  }
  is_animation_tainted_ |= data->IsAnimationTainted();
  has_font_units_ |= data->HasFontUnits();
  has_root_font_units_ |= data->HasRootFontUnits();
  has_line_height_units_ |= data->HasLineHeightUnits();
  return true;
}

void StyleCascade::TokenSequence::Append(const CSSParserToken& token) {
  tokens_.push_back(token);
}

scoped_refptr<CSSVariableData>
StyleCascade::TokenSequence::BuildVariableData() {
  // TODO(crbug.com/661854): We have no original_text_ in this case,
  // even though we should.
  return CSSVariableData::Create(
      CSSTokenizedValue{CSSParserTokenRange{tokens_}, StringView{}},
      is_animation_tainted_, /*needs_variable_resolution=*/false);
}

const CSSValue* StyleCascade::Resolve(const CSSProperty& property,
                                      const CSSValue& value,
                                      CascadePriority priority,
                                      CascadeOrigin& origin,
                                      CascadeResolver& resolver) {
  DCHECK(!property.IsSurrogate());

  const CSSValue* result = ResolveSubstitutions(property, value, resolver);
  DCHECK(result);

  if (result->IsRevertValue()) {
    return ResolveRevert(property, *result, origin, resolver);
  }
  if (result->IsRevertLayerValue()) {
    return ResolveRevertLayer(property, *result, priority, origin, resolver);
  }

  resolver.CollectFlags(property, origin);

  return result;
}

const CSSValue* StyleCascade::ResolveSubstitutions(const CSSProperty& property,
                                                   const CSSValue& value,
                                                   CascadeResolver& resolver) {
  if (const auto* v = DynamicTo<CSSCustomPropertyDeclaration>(value)) {
    return ResolveCustomProperty(property, *v, resolver);
  }
  if (const auto* v = DynamicTo<CSSVariableReferenceValue>(value)) {
    return ResolveVariableReference(property, *v, resolver);
  }
  if (const auto* v = DynamicTo<cssvalue::CSSPendingSubstitutionValue>(value)) {
    return ResolvePendingSubstitution(property, *v, resolver);
  }
  return &value;
}

const CSSValue* StyleCascade::ResolveCustomProperty(
    const CSSProperty& property,
    const CSSCustomPropertyDeclaration& decl,
    CascadeResolver& resolver) {
  DCHECK(!property.IsSurrogate());

  DCHECK(!resolver.IsLocked(property));
  CascadeResolver::AutoLock lock(property, resolver);

  scoped_refptr<CSSVariableData> data = &decl.Value();

  if (data->NeedsVariableResolution()) {
    data = ResolveVariableData(data.get(), resolver);
  }

  if (HasFontSizeDependency(To<CustomProperty>(property), data.get())) {
    resolver.DetectCycle(GetCSSPropertyFontSize());
  }

  if (HasLineHeightDependency(To<CustomProperty>(property), data.get())) {
    resolver.DetectCycle(GetCSSPropertyLineHeight());
  }

  state_.StyleBuilder().SetHasVariableDeclaration();

  if (resolver.InCycle()) {
    return CSSCyclicVariableValue::Create();
  }

  if (!data) {
    return CSSInvalidVariableValue::Create();
  }

  if (data == &decl.Value()) {
    return &decl;
  }

  // If a declaration, once all var() functions are substituted in, contains
  // only a CSS-wide keyword (and possibly whitespace), its value is determined
  // as if that keyword were its specified value all along.
  //
  // https://drafts.csswg.org/css-variables/#substitute-a-var
  {
    CSSParserTokenRange range = data->TokenRange();
    range.ConsumeWhitespace();
    CSSValue* value = css_parsing_utils::ConsumeCSSWideKeyword(range);
    if (value && range.AtEnd()) {
      return value;
    }
  }

  return MakeGarbageCollected<CSSCustomPropertyDeclaration>(
      data, decl.ParserContext());
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

  if (ResolveTokensInto(CSSParserTokenRange{data->Tokens()}, resolver,
                        sequence)) {
    if (const auto* parsed = Parse(property, sequence.TokenRange(), context)) {
      return parsed;
    }
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

    if (!ResolveTokensInto(CSSParserTokenRange{shorthand_data->Tokens()},
                           resolver, sequence)) {
      return cssvalue::CSSUnsetValue::Create();
    }

    HeapVector<CSSPropertyValue, 64> parsed_properties;
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
    if (unvisited_property == &ResolveSurrogate(longhand)) {
      return parsed;
    }
  }

  NOTREACHED();
  return cssvalue::CSSUnsetValue::Create();
}

const CSSValue* StyleCascade::ResolveRevert(const CSSProperty& property,
                                            const CSSValue& value,
                                            CascadeOrigin& origin,
                                            CascadeResolver& resolver) {
  MaybeUseCountRevert(value);

  CascadeOrigin target_origin = TargetOriginForRevert(origin);

  switch (target_origin) {
    case CascadeOrigin::kTransition:
    case CascadeOrigin::kNone:
      return cssvalue::CSSUnsetValue::Create();
    case CascadeOrigin::kUserAgent:
    case CascadeOrigin::kUser:
    case CascadeOrigin::kAuthorPresentationalHint:
    case CascadeOrigin::kAuthor:
    case CascadeOrigin::kAnimation: {
      const CascadePriority* p =
          map_.Find(property.GetCSSPropertyName(), target_origin);
      if (!p) {
        origin = CascadeOrigin::kNone;
        return cssvalue::CSSUnsetValue::Create();
      }
      origin = p->GetOrigin();
      return Resolve(property, *ValueAt(match_result_, p->GetPosition()), *p,
                     origin, resolver);
    }
  }
}

const CSSValue* StyleCascade::ResolveRevertLayer(const CSSProperty& property,
                                                 const CSSValue& value,
                                                 CascadePriority priority,
                                                 CascadeOrigin& origin,
                                                 CascadeResolver& resolver) {
  const CascadePriority* p = map_.FindRevertLayer(
      property.GetCSSPropertyName(), priority.ForLayerComparison());
  if (!p) {
    origin = CascadeOrigin::kNone;
    return cssvalue::CSSUnsetValue::Create();
  }
  origin = p->GetOrigin();
  return Resolve(property, *ValueAt(match_result_, p->GetPosition()), *p,
                 origin, resolver);
}

scoped_refptr<CSSVariableData> StyleCascade::ResolveVariableData(
    CSSVariableData* data,
    CascadeResolver& resolver) {
  DCHECK(data && data->NeedsVariableResolution());

  TokenSequence sequence(data);

  if (!ResolveTokensInto(CSSParserTokenRange{data->Tokens()}, resolver,
                         sequence)) {
    return nullptr;
  }

  return sequence.BuildVariableData();
}

bool StyleCascade::ResolveTokensInto(CSSParserTokenRange range,
                                     CascadeResolver& resolver,
                                     TokenSequence& out) {
  bool success = true;
  while (!range.AtEnd()) {
    const CSSParserToken& token = range.Peek();
    if (token.FunctionId() == CSSValueID::kVar) {
      success &= ResolveVarInto(range.ConsumeBlock(), resolver, out);
    } else if (token.FunctionId() == CSSValueID::kEnv) {
      success &= ResolveEnvInto(range.ConsumeBlock(), resolver, out);
    } else {
      out.Append(range.Consume());
    }
  }
  return success;
}

bool StyleCascade::ResolveVarInto(CSSParserTokenRange range,
                                  CascadeResolver& resolver,
                                  TokenSequence& out) {
  CustomProperty property(ConsumeVariableName(range), state_.GetDocument());
  DCHECK(range.AtEnd() || (range.Peek().GetType() == kCommaToken));

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
  if (!resolver.AllowSubstitution(data.get())) {
    data = nullptr;
  }

  // If we have a fallback, we must process it to look for cycles,
  // even if we aren't going to use the fallback.
  //
  // https://drafts.csswg.org/css-variables/#cycles
  if (ConsumeComma(range)) {
    TokenSequence fallback;
    bool success = ResolveTokensInto(range, resolver, fallback);
    // The fallback must match the syntax of the referenced custom property.
    // https://drafts.css-houdini.org/css-properties-values-api-1/#fallbacks-in-var-references
    if (!ValidateFallback(property, fallback.TokenRange())) {
      return false;
    }
    if (!data) {
      return success && out.Append(fallback, kMaxSubstitutionTokens);
    }
  }

  if (!data || resolver.InCycle()) {
    return false;
  }

  return out.Append(data.get(), kMaxSubstitutionTokens);
}

bool StyleCascade::ResolveEnvInto(CSSParserTokenRange range,
                                  CascadeResolver& resolver,
                                  TokenSequence& out) {
  AtomicString variable_name = ConsumeVariableName(range);
  DCHECK(range.AtEnd() || (range.Peek().GetType() == kCommaToken) ||
         (range.Peek().GetType() == kNumberToken));

  WTF::Vector<unsigned> indices;
  if (!range.AtEnd() && range.Peek().GetType() != kCommaToken) {
    do {
      const CSSParserToken& token = range.ConsumeIncludingWhitespace();
      DCHECK(token.GetNumericValueType() == kIntegerValueType);
      DCHECK(token.NumericValue() >= 0.);
      indices.push_back(static_cast<unsigned>(token.NumericValue()));
    } while (range.Peek().GetType() == kNumberToken);
  }

  DCHECK(range.AtEnd() || (range.Peek().GetType() == kCommaToken));

  CSSVariableData* data = GetEnvironmentVariable(variable_name, indices);

  if (!data) {
    if (ConsumeComma(range)) {
      return ResolveTokensInto(range, resolver, out);
    }
    return false;
  }

  return out.Append(data);
}

CSSVariableData* StyleCascade::GetVariableData(
    const CustomProperty& property) const {
  const AtomicString& name = property.GetPropertyNameAtomicString();
  const bool is_inherited = property.IsInherited();
  return state_.StyleBuilder().GetVariableData(name, is_inherited);
}

CSSVariableData* StyleCascade::GetEnvironmentVariable(
    const AtomicString& name,
    WTF::Vector<unsigned> indices) const {
  // If we are in a User Agent Shadow DOM then we should not record metrics.
  ContainerNode& scope_root = state_.GetElement().GetTreeScope().RootNode();
  auto* shadow_root = DynamicTo<ShadowRoot>(&scope_root);
  bool is_ua_scope = shadow_root && shadow_root->IsUserAgent();

  return state_.GetDocument()
      .GetStyleEngine()
      .EnsureEnvironmentVariables()
      .ResolveVariable(name, std::move(indices), !is_ua_scope);
}

const CSSParserContext* StyleCascade::GetParserContext(
    const CSSVariableReferenceValue& value) {
  // TODO(crbug.com/985028): CSSVariableReferenceValue should always have a
  // CSSParserContext. (CSSUnparsedValue violates this).
  if (value.ParserContext()) {
    return value.ParserContext();
  }
  return StrictCSSParserContext(
      state_.GetDocument().GetExecutionContext()->GetSecureContextMode());
}

bool StyleCascade::HasFontSizeDependency(const CustomProperty& property,
                                         CSSVariableData* data) const {
  if (!property.IsRegistered() || !data) {
    return false;
  }
  if (data->HasFontUnits() || data->HasLineHeightUnits()) {
    return true;
  }
  if (data->HasRootFontUnits() && IsRootElement()) {
    return true;
  }
  return false;
}

bool StyleCascade::HasLineHeightDependency(const CustomProperty& property,
                                           CSSVariableData* data) const {
  if (!property.IsRegistered() || !data) {
    return false;
  }
  if (data->HasLineHeightUnits()) {
    return true;
  }
  return false;
}

bool StyleCascade::ValidateFallback(const CustomProperty& property,
                                    CSSParserTokenRange range) const {
#if DCHECK_IS_ON()
  DCHECK(!HasUnresolvedReferences(range));
#endif  // DCHECK_IS_ON()
  if (!property.IsRegistered()) {
    return true;
  }
  auto context_mode =
      state_.GetDocument().GetExecutionContext()->GetSecureContextMode();
  auto var_mode = CSSParserLocalContext::VariableMode::kTyped;
  auto* context = StrictCSSParserContext(context_mode);
  auto local_context = CSSParserLocalContext().WithVariableMode(var_mode);
  return property.ParseSingleValue(range, *context, local_context);
}

void StyleCascade::MarkIsReferenced(const CSSProperty& referencer,
                                    const CustomProperty& referenced) {
  if (!referenced.IsRegistered()) {
    return;
  }
  const AtomicString& name = referenced.GetPropertyNameAtomicString();
  state_.GetDocument().EnsurePropertyRegistry().MarkReferenced(name);
}

void StyleCascade::MarkHasVariableReference(const CSSProperty& property) {
  if (!property.IsInherited()) {
    state_.StyleBuilder().SetHasVariableReferenceFromNonInheritedProperty();
  }
  state_.StyleBuilder().SetHasVariableReference();
}

const Document& StyleCascade::GetDocument() const {
  return state_.GetDocument();
}

const CSSProperty& StyleCascade::ResolveSurrogate(const CSSProperty& property) {
  if (!property.IsSurrogate()) {
    return property;
  }
  // This marks the cascade as dependent on cascade-affecting properties
  // even for simple surrogates like -webkit-writing-mode, but there isn't
  // currently a flag to distinguish such surrogates from e.g. css-logical
  // properties.
  depends_on_cascade_affecting_property_ = true;
  const CSSProperty* original =
      property.SurrogateFor(state_.StyleBuilder().Direction(),
                            state_.StyleBuilder().GetWritingMode());
  DCHECK(original);
  return *original;
}

void StyleCascade::CountUse(WebFeature feature) {
  GetDocument().CountUse(feature);
}

void StyleCascade::MaybeUseCountRevert(const CSSValue& value) {
  if (value.IsRevertValue()) {
    CountUse(WebFeature::kCSSKeywordRevert);
  }
}

}  // namespace blink
