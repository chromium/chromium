// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/resolver/style_cascade.h"

#include <bit>
#include <optional>

#include "base/notreached.h"
#include "third_party/blink/renderer/core/animation/css/css_animations.h"
#include "third_party/blink/renderer/core/animation/css_interpolation_environment.h"
#include "third_party/blink/renderer/core/animation/interpolation_types_map.h"
#include "third_party/blink/renderer/core/animation/invalidatable_interpolation.h"
#include "third_party/blink/renderer/core/animation/property_handle.h"
#include "third_party/blink/renderer/core/animation/transition_interpolation.h"
#include "third_party/blink/renderer/core/css/container_query_evaluator.h"
#include "third_party/blink/renderer/core/css/css_attr_type.h"
#include "third_party/blink/renderer/core/css/css_cyclic_variable_value.h"
#include "third_party/blink/renderer/core/css/css_flip_revert_value.h"
#include "third_party/blink/renderer/core/css/css_font_selector.h"
#include "third_party/blink/renderer/core/css/css_if_eval.h"
#include "third_party/blink/renderer/core/css/css_invalid_variable_value.h"
#include "third_party/blink/renderer/core/css/css_math_function_value.h"
#include "third_party/blink/renderer/core/css/css_numeric_literal_value.h"
#include "third_party/blink/renderer/core/css/css_pending_substitution_value.h"
#include "third_party/blink/renderer/core/css/css_primitive_value.h"
#include "third_party/blink/renderer/core/css/css_syntax_string_parser.h"
#include "third_party/blink/renderer/core/css/css_unparsed_declaration_value.h"
#include "third_party/blink/renderer/core/css/css_unset_value.h"
#include "third_party/blink/renderer/core/css/css_value.h"
#include "third_party/blink/renderer/core/css/css_variable_data.h"
#include "third_party/blink/renderer/core/css/document_style_environment_variables.h"
#include "third_party/blink/renderer/core/css/if_condition.h"
#include "third_party/blink/renderer/core/css/kleene_value.h"
#include "third_party/blink/renderer/core/css/media_eval_utils.h"
#include "third_party/blink/renderer/core/css/media_list.h"
#include "third_party/blink/renderer/core/css/media_query_exp.h"
#include "third_party/blink/renderer/core/css/parser/css_if_parser.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_fast_paths.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_local_context.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_token.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_token_stream.h"
#include "third_party/blink/renderer/core/css/parser/css_property_parser.h"
#include "third_party/blink/renderer/core/css/parser/css_tokenizer.h"
#include "third_party/blink/renderer/core/css/parser/css_variable_parser.h"
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
#include "third_party/blink/renderer/core/css/resolver/scoped_style_resolver.h"
#include "third_party/blink/renderer/core/css/resolver/style_builder.h"
#include "third_party/blink/renderer/core/css/resolver/style_builder_converter.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver_state.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/css/style_rule_function_declarations.h"
#include "third_party/blink/renderer/core/css/try_value_flips.h"
#include "third_party/blink/renderer/core/css_value_keywords.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/frame/deprecation/deprecation.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/html/forms/html_select_element.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/style_property_shorthand.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/text/string_view.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

namespace {

AtomicString ConsumeVariableName(CSSParserTokenStream& stream) {
  stream.ConsumeWhitespace();
  CSSParserToken ident_token = stream.ConsumeIncludingWhitespaceRaw();
  DCHECK_EQ(ident_token.GetType(), kIdentToken);
  return ident_token.Value().ToAtomicString();
}

bool ConsumeComma(CSSParserTokenStream& stream) {
  if (stream.Peek().GetType() == kCommaToken) {
    stream.ConsumeRaw();
    return true;
  }
  return false;
}

template <CSSParserTokenType... Types>
StringView ConsumeUntilPeekedTypeIs(CSSParserTokenStream& stream) {
  wtf_size_t value_start_offset = stream.LookAheadOffset();
  stream.SkipUntilPeekedTypeIs<Types...>();
  wtf_size_t value_end_offset = stream.LookAheadOffset();
  return stream.StringRangeAt(value_start_offset,
                              value_end_offset - value_start_offset);
}

const CSSValue* ParseAsCSSWideKeyword(const CSSVariableData& data) {
  CSSParserTokenStream stream(data.OriginalText());
  stream.ConsumeWhitespace();
  CSSValue* value = css_parsing_utils::ConsumeCSSWideKeyword(stream);
  return stream.AtEnd() ? value : nullptr;
}

const CSSValue* Parse(const CSSProperty& property,
                      CSSParserTokenStream& stream,
                      const CSSParserContext* context) {
  return CSSPropertyParser::ParseSingleValue(property.PropertyID(), stream,
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
  DCHECK_EQ(properties.data_.origin, CascadeOrigin::kAuthor);
  return result.ScopeFromTreeOrder(properties.data_.tree_order);
}

PropertyHandle ToPropertyHandle(const CSSProperty& property,
                                CascadePriority priority) {
  uint32_t position = priority.GetPosition();
  CSSPropertyID id = DecodeInterpolationPropertyID(position);
  if (id == CSSPropertyID::kVariable) {
    DCHECK(IsA<CustomProperty>(property));
    return PropertyHandle(property.GetPropertyNameAtomicString());
  }
  return PropertyHandle(CSSProperty::Get(id));
}

// https://drafts.csswg.org/css-cascade-4/#default
CascadeOrigin TargetOriginForRevert(CascadeOrigin origin) {
  switch (origin) {
    case CascadeOrigin::kNone:
    case CascadeOrigin::kTransition:
      NOTREACHED();
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

std::optional<CSSVariableData*> FindOrNullopt(
    const HeapHashMap<String, Member<CSSVariableData>>& map,
    const String& key) {
  auto it = map.find(key);
  if (it == map.end()) {
    return std::nullopt;
  }
  return it->value.Get();
}

const CSSSyntaxDefinition* FindOrNull(
    const HashMap<String, const CSSSyntaxDefinition*>& map,
    const String& key) {
  auto it = map.find(key);
  if (it == map.end()) {
    return nullptr;
  }
  return it->value;
}

// The `container_tree_scope` is the tree scope holding the @container
// rule being evaluated. For @container rules within @function, this is
// the same tree scope as the enclosing @function is defined in.
bool EvaluateContainerQuery(Element& element,
                            PseudoId pseudo_id,
                            const ContainerQuery& query,
                            const TreeScope* container_tree_scope,
                            Element* nearest_size_container,
                            MatchResult& match_result) {
  const ContainerSelector& selector = query.Selector();
  if (!selector.SelectsAnyContainer()) {
    return false;
  }
  // TODO(crbug.com/394500600): Calling SetDependencyFlags here works,
  // but it's arguably a bit late when considering that MatchResult
  // is supposed to be the output of ElementRuleCollector.
  // Consider refactoring.
  ContainerQueryEvaluator::SetDependencyFlags(query, match_result);

  Element* starting_element = ContainerQueryEvaluator::DetermineStartingElement(
      element, pseudo_id, selector, nearest_size_container);
  Element* container = ContainerQueryEvaluator::FindContainer(
      starting_element, selector, container_tree_scope);
  if (!container) {
    return false;
  }
  ContainerQueryEvaluator& evaluator =
      container->EnsureContainerQueryEvaluator();
  using Change = ContainerQueryEvaluator::Change;
  Change change = starting_element == container ? Change::kNearestContainer
                                                : Change::kDescendantContainers;
  return evaluator.EvalAndAdd(query, change, match_result);
}

bool IsVariableNameOnly(StringView str) {
  if (!CSSVariableParser::IsValidVariableName(str)) {
    return false;
  }
  CSSParserTokenStream stream(str);
  if (stream.Peek().GetType() != kIdentToken) {
    return false;
  }
  stream.ConsumeIncludingWhitespace();
  return stream.AtEnd();
}

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

  ApplyHighPriority(resolver);
  state_.UpdateFont();

  if (map_.NativeBitset().Has(CSSPropertyID::kLineHeight)) {
    LookupAndApply(GetCSSPropertyLineHeight(), resolver);
  }
  state_.UpdateLineHeight();

  ApplyWideOverlapping(resolver);

  ApplyMatchResult(resolver);
  ApplyInterpolations(resolver);

  state_.SetComputedStyleFlagsFromAuthorFlags(resolver.AuthorFlags());
  if (resolver.Flags() & CSSProperty::kAnimation) {
    state_.StyleBuilder().SetCanAffectAnimations();
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
  if (state_.GetElement().HasTagName(html_names::kH1Tag)) {
    if (CascadePriority* priority =
            map_.Find(GetCSSPropertyFontSize().GetCSSPropertyName())) {
      if (priority->GetOrigin() != CascadeOrigin::kUserAgent) {
        return;
      }
      const CSSValue* value = ValueAt(match_result_, priority->GetPosition());
      if (const auto* numeric = DynamicTo<CSSNumericLiteralValue>(value)) {
        DCHECK(numeric->GetType() == CSSNumericLiteralValue::UnitType::kEms);
        if (numeric->DoubleValue() != 2.0) {
          Deprecation::CountDeprecation(
              GetDocument().GetExecutionContext(),
              WebFeature::kH1UserAgentFontSizeInSectionApplied);
        }
      }
    }
  }

  ApplyUnresolvedEnv(resolver);
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
                                      const TreeScope* tree_scope,
                                      CascadeOrigin origin,
                                      CascadeResolver& resolver) {
  CSSPropertyRef ref(name, state_.GetDocument());

  const CSSValue* resolved =
      Resolve(ResolveSurrogate(ref.GetProperty()), value, tree_scope,
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
    if (IsInterpolation(priority)) {
      continue;
    }
    if (!priority.HasOrigin()) {
      // Declarations added for explicit defaults (AddExplicitDefaults)
      // should not be observable.
      continue;
    }
    const CSSValue* cascaded = ValueAt(match_result_, priority.GetPosition());
    DCHECK(cascaded);
    result.Set(name, cascaded);
  }

  for (const auto& name : map_.GetCustomMap().Keys()) {
    CascadePriority priority = map_.At(CSSPropertyName(name));
    DCHECK(priority.HasOrigin());
    if (IsInterpolation(priority)) {
      continue;
    }
    const CSSValue* cascaded = ValueAt(match_result_, priority.GetPosition());
    DCHECK(cascaded);
    result.Set(CSSPropertyName(name), cascaded);
  }

  return result;
}

const CSSValue* StyleCascade::Resolve(StyleResolverState& state,
                                      const CSSPropertyName& name,
                                      const CSSValue& value,
                                      const TreeScope* tree_scope) {
  STACK_UNINITIALIZED StyleCascade cascade(state);

  // Since the cascade map is empty, the CascadeResolver isn't important,
  // as there can be no cycles in an empty map. We just instantiate it to
  // satisfy the API.
  CascadeResolver resolver(CascadeFilter(), /* generation */ 0);

  // The origin is relevant for 'revert', but since the cascade map
  // is empty, there will be nothing to revert to regardless of the origin
  // We use kNone, because kAuthor (etc) imply that the `value` originates
  // from a location on the `MatchResult`, which is not the case.
  CascadeOrigin origin = CascadeOrigin::kNone;

  return cascade.Resolve(name, value, tree_scope, origin, resolver);
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
  AddExplicitDefaults();

  int index = 0;
  for (const MatchedProperties& properties :
       match_result_.GetMatchedProperties()) {
    ExpandCascade(
        properties, GetDocument(), index++,
        [this](CascadePriority cascade_priority,
               const AtomicString& custom_property_name) {
          map_.Add(custom_property_name, cascade_priority);
        },
        [this](CascadePriority cascade_priority, CSSPropertyID property_id) {
          if (kSurrogateProperties.Has(property_id)) {
            const CSSProperty& property =
                ResolveSurrogate(CSSProperty::Get(property_id));
            map_.Add(property.PropertyID(), cascade_priority);
          } else {
            map_.Add(property_id, cascade_priority);
          }
        });
  }
}

void StyleCascade::AnalyzeInterpolations() {
  const auto& entries = interpolations_.GetEntries();
  for (wtf_size_t i = 0; i < entries.size(); ++i) {
    for (const auto& active_interpolation : *entries[i].map) {
      auto name = active_interpolation.key.GetCSSPropertyName();
      uint32_t position = EncodeInterpolationPosition(name.Id(), i);
      CascadePriority priority(entries[i].origin,
                               /* important */ false,
                               /* tree_order */ 0,
                               /* is_inline_style */ false,
                               /* is_try_style */ false,
                               /* is_try_tactics_style */ false,
                               /* layer_order */ 0, position);

      CSSPropertyRef ref(name, GetDocument());
      DCHECK(ref.IsValid());

      if (name.IsCustomProperty()) {
        map_.Add(name.ToAtomicString(), priority);
      } else {
        const CSSProperty& property = ResolveSurrogate(ref.GetProperty());
        map_.Add(property.PropertyID(), priority);

        // Since an interpolation for an unvisited property also causes an
        // interpolation of the visited property, add the visited property to
        // the map as well.
        // TODO(crbug.com/1062217): Interpolate visited colors separately
        if (const CSSProperty* visited = property.GetVisitedProperty()) {
          map_.Add(visited->PropertyID(), priority);
        }
      }
    }
  }
}

// The implicit defaulting behavior of inherited properties is to take
// the value of the parent style [1]. However, we never reach
// Longhand::ApplyInherit for implicit defaults, which is needed to adjust
// Lengths with premultiplied zoom. Therefore, all inherited properties
// are instead explicitly defaulted [2] when the effective zoom has changed
// versus the parent zoom.
//
// [1] https://drafts.csswg.org/css-cascade/#defaulting
// [2] https://drafts.csswg.org/css-cascade/#defaulting-keywords
void StyleCascade::AddExplicitDefaults() {
  if (state_.GetDocument().StandardizedBrowserZoomEnabled() &&
      effective_zoom_changed_) {
    // These inherited properties can contain lengths:
    //
    //   -webkit-border-horizontal-spacing
    //   -webkit-border-vertical-spacing
    //   -webkit-text-stroke-width
    //   letter-spacing
    //   line-height
    //   list-style-image *
    //   stroke-dasharray
    //   stroke-dashoffset
    //   stroke-width **
    //   text-indent
    //   text-shadow
    //   text-underline-offset
    //   word-spacing
    //
    // * list-style-image need not be recomputed on zoom change because list
    // image marker is sized to 1em and font-size is already correctly zoomed.
    //
    // ** stroke-width gets special handling elsewhere.
    map_.Add(CSSPropertyID::kLetterSpacing,
             CascadePriority(CascadeOrigin::kNone));
    map_.Add(CSSPropertyID::kLineHeight, CascadePriority(CascadeOrigin::kNone));
    map_.Add(CSSPropertyID::kStrokeDasharray,
             CascadePriority(CascadeOrigin::kNone));
    map_.Add(CSSPropertyID::kStrokeDashoffset,
             CascadePriority(CascadeOrigin::kNone));
    map_.Add(CSSPropertyID::kTextIndent, CascadePriority(CascadeOrigin::kNone));
    map_.Add(CSSPropertyID::kTextShadow, CascadePriority(CascadeOrigin::kNone));
    map_.Add(CSSPropertyID::kTextUnderlineOffset,
             CascadePriority(CascadeOrigin::kNone));
    map_.Add(CSSPropertyID::kWebkitTextStrokeWidth,
             CascadePriority(CascadeOrigin::kNone));
    map_.Add(CSSPropertyID::kWebkitBorderHorizontalSpacing,
             CascadePriority(CascadeOrigin::kNone));
    map_.Add(CSSPropertyID::kWebkitBorderVerticalSpacing,
             CascadePriority(CascadeOrigin::kNone));
    map_.Add(CSSPropertyID::kWordSpacing,
             CascadePriority(CascadeOrigin::kNone));
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
  // Similarly, we assume that the effective zoom of this element
  // is the same as the parent's effective zoom. If it isn't,
  // we re-cascade with explicit defaults inserted at CascadeOrigin::kNone.
  //
  // See also StyleCascade::AddExplicitDefaults.
  float effective_zoom = state_.StyleBuilder().EffectiveZoom();

  if (map_.NativeBitset().Has(CSSPropertyID::kDirection)) {
    LookupAndApply(GetCSSPropertyDirection(), resolver);
  }
  if (map_.NativeBitset().Has(CSSPropertyID::kWritingMode)) {
    LookupAndApply(GetCSSPropertyWritingMode(), resolver);
  }
  if (map_.NativeBitset().Has(CSSPropertyID::kZoom)) {
    LookupAndApply(GetCSSPropertyZoom(), resolver);
  }

  bool reanalyze = false;

  if (depends_on_cascade_affecting_property_) {
    if (direction != state_.StyleBuilder().Direction() ||
        writing_mode != state_.StyleBuilder().GetWritingMode()) {
      reanalyze = true;
    }
  }
  if (effective_zoom != state_.StyleBuilder().EffectiveZoom()) {
    effective_zoom_changed_ = true;
    reanalyze = true;
  }

  if (reanalyze) {
    Reanalyze();
  }
}

void StyleCascade::ApplyHighPriority(CascadeResolver& resolver) {
  uint64_t bits = map_.HighPriorityBits();

  while (bits) {
    int i = std::countr_zero(bits);
    bits &= bits - 1;  // Clear the lowest bit.
    LookupAndApply(CSSProperty::Get(ConvertToCSSPropertyID(i)), resolver);
  }
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
  if (resolver.filter_.Accepts(webkit_border_image)) {
    if (const CascadePriority* priority =
            map_.Find(webkit_border_image.GetCSSPropertyName())) {
      LookupAndApply(webkit_border_image, resolver);

      const auto& shorthand = borderImageShorthand();
      for (const CSSProperty* const longhand : shorthand.properties()) {
        maybe_skip(*longhand, *priority);
      }
    }
  }

  const CSSProperty& perspective_origin = GetCSSPropertyPerspectiveOrigin();
  if (resolver.filter_.Accepts(perspective_origin)) {
    if (const CascadePriority* priority =
            map_.Find(perspective_origin.GetCSSPropertyName())) {
      LookupAndApply(perspective_origin, resolver);
      maybe_skip(GetCSSPropertyWebkitPerspectiveOriginX(), *priority);
      maybe_skip(GetCSSPropertyWebkitPerspectiveOriginY(), *priority);
    }
  }

  const CSSProperty& transform_origin = GetCSSPropertyTransformOrigin();
  if (resolver.filter_.Accepts(transform_origin)) {
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
  if (resolver.filter_.Accepts(vertical_align)) {
    if (const CascadePriority* priority =
            map_.Find(vertical_align.GetCSSPropertyName())) {
      LookupAndApply(vertical_align, resolver);
      maybe_skip(GetCSSPropertyBaselineSource(), *priority);
    }
  }

  // Note that -webkit-box-decoration-break isn't really more (or less)
  // "wide" than the non-prefixed counterpart, but they still share
  // a ComputedStyle location, and therefore need to be handled here.
  const CSSProperty& webkit_box_decoration_break =
      GetCSSPropertyWebkitBoxDecorationBreak();
  if (resolver.filter_.Accepts(webkit_box_decoration_break)) {
    if (const CascadePriority* priority =
            map_.Find(webkit_box_decoration_break.GetCSSPropertyName())) {
      LookupAndApply(webkit_box_decoration_break, resolver);
      maybe_skip(GetCSSPropertyBoxDecorationBreak(), *priority);
    }
  }
}

// Go through all properties that were found during the analyze phase
// (e.g. in AnalyzeMatchResult()) and actually apply them. We need to do this
// in a second phase so that we know which ones actually won the cascade
// before we start applying, as some properties can affect others.
void StyleCascade::ApplyMatchResult(CascadeResolver& resolver) {
  // All the high-priority properties were dealt with in ApplyHighPriority(),
  // so we don't need to look at them again. (That would be a no-op due to
  // the generation check below, but it's cheaper just to mask them out
  // entirely.)
  for (auto it = map_.NativeBitset().BeginAfterHighPriority();
       it != map_.NativeBitset().end(); ++it) {
    CSSPropertyID id = *it;
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

    CustomProperty property(name, GetDocument());
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
    uint32_t position = EncodeInterpolationPosition(name.Id(), index);
    CascadePriority priority(origin,
                             /* important */ false,
                             /* tree_order */ 0,
                             /* is_inline_style */ false,
                             /* is_try_style */ false,
                             /* is_try_tactics_style */ false,
                             /* layer_order */ 0, position);
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

  InterpolationTypesMap map(state_.GetDocument().GetPropertyRegistry(),
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
  CascadeOrigin origin = priority->GetOrigin();
  // Values at CascadeOrigin::kNone are used for explicit defaulting,
  // see StyleCascade::AddExplicitDefaults.
  const CSSValue* value = (origin == CascadeOrigin::kNone)
                              ? cssvalue::CSSUnsetValue::Create()
                              : ValueAt(match_result_, priority->GetPosition());
  DCHECK(value);
  const TreeScope* tree_scope = GetTreeScope(*priority);
  value = Resolve(property, *value, tree_scope, *priority, origin, resolver);
  DCHECK(IsA<CustomProperty>(property) || !value->IsUnparsedDeclaration());
  DCHECK(!value->IsPendingSubstitutionValue());
  value = &value->EnsureScopedValue(tree_scope);
  StyleBuilder::ApplyPhysicalProperty(property, state_, *value);
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
  CHECK_NE(entry, map.end());
  ApplyInterpolation(property, *priority, *entry->value, resolver);
}

bool StyleCascade::IsRootElement() const {
  return &state_.GetElement() == state_.GetDocument().documentElement();
}

StyleCascade::TokenSequence::TokenSequence(const CSSVariableData* data)
    : is_animation_tainted_(data->IsAnimationTainted()),
      has_font_units_(data->HasFontUnits()),
      has_root_font_units_(data->HasRootFontUnits()),
      has_line_height_units_(data->HasLineHeightUnits()),
      has_dashed_functions_(data->HasDashedFunctions()) {}

bool StyleCascade::TokenSequence::AppendFallback(const TokenSequence& sequence,
                                                 bool is_attr_tainted,
                                                 wtf_size_t byte_limit) {
  // https://drafts.csswg.org/css-variables/#long-variables
  if (original_text_.length() + sequence.original_text_.length() > byte_limit) {
    return false;
  }
  size_t start = original_text_.length();

  StringView other_text = sequence.original_text_;
  other_text =
      CSSVariableParser::StripTrailingWhitespaceAndComments(other_text);

  CSSTokenizer tokenizer(other_text);
  CSSParserToken first_token = tokenizer.TokenizeSingleWithComments();

  if (NeedsInsertedComment(last_token_, first_token)) {
    original_text_.Append("/**/");
  }
  original_text_.Append(other_text);
  last_token_ = last_non_whitespace_token_ =
      sequence.last_non_whitespace_token_;

  is_animation_tainted_ |= sequence.is_animation_tainted_;
  has_font_units_ |= sequence.has_font_units_;
  has_root_font_units_ |= sequence.has_root_font_units_;
  has_line_height_units_ |= sequence.has_line_height_units_;
  has_dashed_functions_ |= sequence.has_dashed_functions_;

  size_t end = original_text_.length();
  if (is_attr_tainted) {
    attr_taint_ranges_.emplace_back(std::make_pair(start, end));
  }
  return true;
}

static bool IsNonWhitespaceToken(const CSSParserToken& token) {
  return token.GetType() != kWhitespaceToken &&
         token.GetType() != kCommentToken;
}

bool StyleCascade::TokenSequence::Append(StringView str,
                                         bool is_attr_tainted,
                                         wtf_size_t byte_limit) {
  // https://drafts.csswg.org/css-variables/#long-variables
  if (original_text_.length() + str.length() > byte_limit) {
    return false;
  }
  size_t start = original_text_.length();
  CSSTokenizer tokenizer(str);
  const CSSParserToken first_token = tokenizer.TokenizeSingleWithComments();
  if (first_token.GetType() != kEOFToken) {
    CSSVariableData::ExtractFeatures(
        first_token, has_font_units_, has_root_font_units_,
        has_line_height_units_, has_dashed_functions_);
    if (NeedsInsertedComment(last_token_, first_token)) {
      original_text_.Append("/**/");
    }
    last_token_ = first_token.CopyWithoutValue();
    if (IsNonWhitespaceToken(first_token)) {
      last_non_whitespace_token_ = first_token;
    }
    while (true) {
      const CSSParserToken token = tokenizer.TokenizeSingleWithComments();
      if (token.GetType() == kEOFToken) {
        break;
      } else {
        CSSVariableData::ExtractFeatures(
            token, has_font_units_, has_root_font_units_,
            has_line_height_units_, has_dashed_functions_);
        last_token_ = token.CopyWithoutValue();
        if (IsNonWhitespaceToken(token)) {
          last_non_whitespace_token_ = token;
        }
      }
    }
  }
  original_text_.Append(str);

  size_t end = original_text_.length();
  if (is_attr_tainted) {
    attr_taint_ranges_.emplace_back(std::make_pair(start, end));
  }
  return true;
}

bool StyleCascade::TokenSequence::Append(const CSSValue* value,
                                         bool is_attr_tainted,
                                         wtf_size_t byte_limit) {
  return Append(value->CssText(), is_attr_tainted, byte_limit);
}

bool StyleCascade::TokenSequence::Append(CSSVariableData* data,
                                         bool is_attr_tainted,
                                         wtf_size_t byte_limit) {
  if (!Append(data->OriginalText(), is_attr_tainted, byte_limit)) {
    return false;
  }
  is_animation_tainted_ |= data->IsAnimationTainted();
  return true;
}

void StyleCascade::TokenSequence::Append(const CSSParserToken& token,
                                         bool is_attr_tainted,
                                         StringView original_text) {
  CSSVariableData::ExtractFeatures(token, has_font_units_, has_root_font_units_,
                                   has_line_height_units_,
                                   has_dashed_functions_);
  size_t start = original_text_.length();
  if (NeedsInsertedComment(last_token_, token)) {
    original_text_.Append("/**/");
  }
  last_token_ = token.CopyWithoutValue();
  if (IsNonWhitespaceToken(token)) {
    last_non_whitespace_token_ = token;
  }
  original_text_.Append(original_text);
  size_t end = original_text_.length();
  if (is_attr_tainted) {
    attr_taint_ranges_.emplace_back(std::make_pair(start, end));
  }
}

bool StyleCascade::TokenSequence::Append(TokenSequence& sequence,
                                         bool is_attr_tainted,
                                         wtf_size_t byte_limit) {
  if (!Append(sequence.OriginalText(),
              is_attr_tainted || !sequence.GetAttrTaintedRanges()->empty(),
              byte_limit)) {
    return false;
  }
  is_animation_tainted_ |= sequence.is_animation_tainted_;
  return true;
}

CSSVariableData* StyleCascade::TokenSequence::BuildVariableData() {
  return CSSVariableData::Create(
      original_text_, is_animation_tainted_, !attr_taint_ranges_.empty(),
      /*needs_variable_resolution=*/false, has_font_units_,
      has_root_font_units_, has_line_height_units_, has_dashed_functions_);
}

const CSSValue* StyleCascade::Resolve(const CSSProperty& property,
                                      const CSSValue& value,
                                      const TreeScope* tree_scope,
                                      CascadePriority priority,
                                      CascadeOrigin& origin,
                                      CascadeResolver& resolver) {
  DCHECK(!property.IsSurrogate());

  const CSSValue* result =
      ResolveSubstitutions(property, value, tree_scope, resolver);
  DCHECK(result);

  if (result->IsRevertValue()) {
    return ResolveRevert(property, *result, tree_scope, origin, resolver);
  }
  if (result->IsRevertLayerValue() || TreatAsRevertLayer(priority)) {
    return ResolveRevertLayer(property, tree_scope, priority, origin, resolver);
  }
  if (const auto* v = DynamicTo<CSSFlipRevertValue>(result)) {
    return ResolveFlipRevert(property, *v, tree_scope, priority, origin,
                             resolver);
  }
  resolver.CollectFlags(property, origin);
  if (const auto* v = DynamicTo<CSSMathFunctionValue>(result)) {
    return ResolveMathFunction(property, *v, tree_scope);
  }

  return result;
}

const CSSValue* StyleCascade::ResolveSubstitutions(const CSSProperty& property,
                                                   const CSSValue& value,
                                                   const TreeScope* tree_scope,
                                                   CascadeResolver& resolver) {
  if (const auto* v = DynamicTo<CSSUnparsedDeclarationValue>(value)) {
    if (property.GetCSSPropertyName().IsCustomProperty()) {
      return ResolveCustomProperty(property, *v, tree_scope, resolver);
    } else {
      return ResolveVariableReference(property, *v, tree_scope, resolver);
    }
  }
  if (const auto* v = DynamicTo<cssvalue::CSSPendingSubstitutionValue>(value)) {
    return ResolvePendingSubstitution(property, *v, tree_scope, resolver);
  }
  return &value;
}

const CSSValue* StyleCascade::ResolveCustomProperty(
    const CSSProperty& property,
    const CSSUnparsedDeclarationValue& decl,
    const TreeScope* tree_scope,
    CascadeResolver& resolver) {
  DCHECK(!property.IsSurrogate());

  DCHECK(!resolver.IsLocked(property));
  CascadeResolver::AutoLock lock(property, resolver);

  CSSVariableData* data = decl.VariableDataValue();

  if (data->NeedsVariableResolution()) {
    data = ResolveVariableData(data, tree_scope, *GetParserContext(decl),
                               /*function_context=*/nullptr, resolver);
  }

  if (HasFontSizeDependency(To<CustomProperty>(property), data)) {
    resolver.DetectCycle(GetCSSPropertyFontSize());
  }

  if (HasLineHeightDependency(To<CustomProperty>(property), data)) {
    resolver.DetectCycle(GetCSSPropertyLineHeight());
  }

  if (resolver.InCycle()) {
    return CSSCyclicVariableValue::Create();
  }

  if (!data) {
    return CSSInvalidVariableValue::Create();
  }

  if (data == decl.VariableDataValue()) {
    return &decl;
  }

  // If a declaration, once all var() functions are substituted in, contains
  // only a CSS-wide keyword (and possibly whitespace), its value is determined
  // as if that keyword were its specified value all along.
  //
  // https://drafts.csswg.org/css-variables/#substitute-a-var
  {
    CSSParserTokenStream stream(data->OriginalText());
    stream.ConsumeWhitespace();
    CSSValue* value = css_parsing_utils::ConsumeCSSWideKeyword(stream);
    if (value && stream.AtEnd()) {
      return value;
    }
  }

  return MakeGarbageCollected<CSSUnparsedDeclarationValue>(
      data, decl.ParserContext());
}

const CSSValue* StyleCascade::ResolveVariableReference(
    const CSSProperty& property,
    const CSSUnparsedDeclarationValue& value,
    const TreeScope* tree_scope,
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

  CSSParserTokenStream stream(data->OriginalText());
  if (ResolveTokensInto(stream, tree_scope, resolver, *context,
                        /* function_context */ nullptr,
                        /* stop_type */ kEOFToken, sequence)) {
    // TODO(sesse): It would be nice if we had some way of combining
    // ResolveTokensInto() and the re-tokenization. This is basically
    // what we pay by using the streaming parser everywhere; we tokenize
    // everything involving variable references twice.
    CSSParserTokenStream stream2(sequence.OriginalText(),
                                 sequence.GetAttrTaintedRanges());
    if (const auto* parsed = Parse(property, stream2, context)) {
      return parsed;
    }
  }

  return cssvalue::CSSUnsetValue::Create();
}

const CSSValue* StyleCascade::ResolvePendingSubstitution(
    const CSSProperty& property,
    const cssvalue::CSSPendingSubstitutionValue& value,
    const TreeScope* tree_scope,
    CascadeResolver& resolver) {
  DCHECK(!property.IsSurrogate());
  DCHECK(!resolver.IsLocked(property));
  CascadeResolver::AutoLock lock(property, resolver);

  DCHECK_NE(property.PropertyID(), CSSPropertyID::kVariable);

  MarkHasVariableReference(property);

  // If the previous call to ResolvePendingSubstitution parsed 'value', then
  // we don't need to do it again.
  bool is_cached = resolver.shorthand_cache_.value == &value;

  if (!is_cached) {
    CSSUnparsedDeclarationValue* shorthand_value = value.ShorthandValue();
    const auto* shorthand_data = shorthand_value->VariableDataValue();
    CSSPropertyID shorthand_property_id = value.ShorthandPropertyId();

    TokenSequence sequence;

    CSSParserTokenStream stream(shorthand_data->OriginalText());
    if (!ResolveTokensInto(stream, tree_scope, resolver,
                           *GetParserContext(*shorthand_value),
                           /* function_context */ nullptr,
                           /* stop_type */ kEOFToken, sequence)) {
      return cssvalue::CSSUnsetValue::Create();
    }

    HeapVector<CSSPropertyValue, 64> parsed_properties;

    // NOTE: We don't actually need the original text to be comment-stripped,
    // since we're not storing it in a custom property anywhere.
    CSSParserTokenStream stream2(sequence.OriginalText());
    if (!CSSPropertyParser::ParseValue(
            shorthand_property_id, /*allow_important_annotation=*/false,
            stream2, shorthand_value->ParserContext(), parsed_properties,
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
    const CSSProperty& longhand =
        CSSProperty::Get(parsed_properties[i].PropertyID());

    // When using var() in a css-logical shorthand (e.g. margin-inline),
    // the longhands here will also be logical.
    if (unvisited_property == &ResolveSurrogate(longhand)) {
      return &parsed_properties[i].Value();
    }
  }

  NOTREACHED();
}

const CSSValue* StyleCascade::ResolveRevert(const CSSProperty& property,
                                            const CSSValue& value,
                                            const TreeScope* tree_scope,
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
      if (!p || !p->HasOrigin()) {
        origin = CascadeOrigin::kNone;
        return cssvalue::CSSUnsetValue::Create();
      }
      origin = p->GetOrigin();
      return Resolve(property, *ValueAt(match_result_, p->GetPosition()),
                     GetTreeScope(*p), *p, origin, resolver);
    }
  }
}

const CSSValue* StyleCascade::ResolveRevertLayer(const CSSProperty& property,
                                                 const TreeScope* tree_scope,
                                                 CascadePriority priority,
                                                 CascadeOrigin& origin,
                                                 CascadeResolver& resolver) {
  const CascadePriority* p = map_.FindRevertLayer(
      property.GetCSSPropertyName(), priority.ForLayerComparison());
  if (!p || !p->HasOrigin()) {
    origin = CascadeOrigin::kNone;
    return cssvalue::CSSUnsetValue::Create();
  }
  origin = p->GetOrigin();
  return Resolve(property, *ValueAt(match_result_, p->GetPosition()),
                 GetTreeScope(*p), *p, origin, resolver);
}

const CSSValue* StyleCascade::ResolveFlipRevert(const CSSProperty& property,
                                                const CSSFlipRevertValue& value,
                                                const TreeScope* tree_scope,
                                                CascadePriority priority,
                                                CascadeOrigin& origin,
                                                CascadeResolver& resolver) {
  const CSSProperty& to_property =
      ResolveSurrogate(CSSProperty::Get(value.PropertyID()));
  const CSSValue* unflipped =
      ResolveRevertLayer(to_property, tree_scope, priority, origin, resolver);
  // Note: the value is transformed *from* the property we're reverting *to*.
  const CSSValue* flipped = TryValueFlips::FlipValue(
      /* from_property */ to_property.PropertyID(), unflipped,
      value.Transform(), state_.StyleBuilder().GetWritingDirection());
  return Resolve(property, *flipped, tree_scope, priority, origin, resolver);
}

// Math functions can become invalid at computed-value time. Currently, this
// is only possible for invalid anchor*() functions.
//
// https://drafts.csswg.org/css-anchor-position-1/#anchor-valid
// https://drafts.csswg.org/css-anchor-position-1/#anchor-size-valid
const CSSValue* StyleCascade::ResolveMathFunction(
    const CSSProperty& property,
    const CSSMathFunctionValue& math_value,
    const TreeScope* tree_scope) {
  if (!math_value.HasAnchorFunctions()) {
    return &math_value;
  }

  const CSSLengthResolver& length_resolver = state_.CssToLengthConversionData();

  // Calling HasInvalidAnchorFunctions evaluates the anchor*() functions
  // inside the CSSMathFunctionValue. Evaluating anchor*() requires that we
  // have the correct AnchorEvaluator::Mode, so we need to set that just like
  // we do for during e.g. Left::ApplyValue, Right::ApplyValue, etc.
  AnchorScope anchor_scope(property.PropertyID(),
                           length_resolver.GetAnchorEvaluator());
  // HasInvalidAnchorFunctions actually evaluates any anchor*() queries
  // within the CSSMathFunctionValue, and this requires the TreeScope to
  // be populated.
  const auto& scoped_math_value =
      To<CSSMathFunctionValue>(math_value.EnsureScopedValue(tree_scope));
  if (scoped_math_value.HasInvalidAnchorFunctions(length_resolver)) {
    return cssvalue::CSSUnsetValue::Create();
  }
  return &scoped_math_value;
}

CSSVariableData* StyleCascade::ResolveVariableData(
    CSSVariableData* data,
    const TreeScope* tree_scope,
    const CSSParserContext& context,
    FunctionContext* function_context,
    CascadeResolver& resolver) {
  DCHECK(data && data->NeedsVariableResolution());

  TokenSequence sequence(data);

  CSSParserTokenStream stream(data->OriginalText());
  if (!ResolveTokensInto(stream, tree_scope, resolver, context,
                         function_context,
                         /*stop_type=*/kEOFToken, sequence)) {
    return nullptr;
  }

  return sequence.BuildVariableData();
}

bool StyleCascade::ResolveTokensInto(CSSParserTokenStream& stream,
                                     const TreeScope* tree_scope,
                                     CascadeResolver& resolver,
                                     const CSSParserContext& context,
                                     FunctionContext* function_context,
                                     CSSParserTokenType stop_type,
                                     TokenSequence& out) {
  bool success = true;
  int nesting_level = 0;
  while (true) {
    const CSSParserToken& token = stream.Peek();
    if (token.IsEOF()) {
      break;
    } else if (token.GetType() == stop_type && nesting_level == 0) {
      break;
    } else if (token.FunctionId() == CSSValueID::kVar) {
      CSSParserTokenStream::BlockGuard guard(stream);
      success &= ResolveVarInto(stream, tree_scope, resolver, context,
                                function_context, out);
    } else if (token.FunctionId() == CSSValueID::kEnv) {
      CSSParserTokenStream::BlockGuard guard(stream);
      success &= ResolveEnvInto(stream, tree_scope, resolver, context, out);
    } else if (token.FunctionId() == CSSValueID::kAttr &&
               RuntimeEnabledFeatures::CSSAdvancedAttrFunctionEnabled()) {
      CSSParserTokenStream::BlockGuard guard(stream);
      state_.StyleBuilder().SetHasAttrFunction();
      success &= ResolveAttrInto(stream, tree_scope, resolver, context,
                                 function_context, out);
    } else if (token.FunctionId() == CSSValueID::kInternalAutoBase) {
      CSSParserTokenStream::BlockGuard guard(stream);
      success &=
          ResolveAutoBaseInto(stream, tree_scope, resolver, context, out);
    } else if (token.FunctionId() == CSSValueID::kIf &&
               RuntimeEnabledFeatures::CSSInlineIfForStyleQueriesEnabled()) {
      CSSParserTokenStream::BlockGuard guard(stream);
      success &= ResolveIfInto(stream, tree_scope, resolver, context,
                               function_context, out);
    } else if (token.GetType() == kFunctionToken &&
               CSSVariableParser::IsValidVariableName(token.Value()) &&
               RuntimeEnabledFeatures::CSSFunctionsEnabled()) {
      // User-defined CSS function.
      CSSParserTokenStream::BlockGuard guard(stream);
      success &= ResolveFunctionInto(token.Value(), tree_scope, stream,
                                     resolver, context, function_context, out);
    } else {
      if (token.GetBlockType() == CSSParserToken::kBlockStart) {
        ++nesting_level;
      } else if (token.GetBlockType() == CSSParserToken::kBlockEnd) {
        if (nesting_level == 0) {
          // Attempting to go outside our block.
          break;
        }
        --nesting_level;
      }
      wtf_size_t start = stream.Offset();
      stream.ConsumeRaw();
      wtf_size_t end = stream.Offset();

      // NOTE: This will include any comment tokens that ConsumeRaw()
      // skipped over; i.e., any comment will be attributed to the
      // token after it and any trailing comments will be skipped.
      // This is fine, because trailing comments (sans whitespace)
      // should be skipped anyway.
      out.Append(token, stream.IsAttrTainted(start, end),
                 stream.StringRangeAt(start, end - start));
    }
  }
  return success;
}

bool StyleCascade::ResolveVarInto(CSSParserTokenStream& stream,
                                  const TreeScope* tree_scope,
                                  CascadeResolver& resolver,
                                  const CSSParserContext& context,
                                  FunctionContext* function_context,
                                  TokenSequence& out) {
  AtomicString var_name = ConsumeVariableName(stream);
  DCHECK(stream.AtEnd() || (stream.Peek().GetType() == kCommaToken));

  // TODO(crbug.com/416640817): All of this fallback handling can be removed
  // when the CSSShortCircuitVarAttr flag is removed:
  //
  // If we have a fallback, we must process it to look for cycles,
  // even if we are not going to use the fallback.
  //
  // https://drafts.csswg.org/css-variables/#cycles
  TokenSequence fallback;
  bool has_fallback = false;
  // Note: has_comma may be `true` even for fallbacks which contain
  // invalid var(). This is needed for syntax validation of fallbacks for
  // registered custom properties.
  // TODO(crbug.com/372475301): Remove this, if possible.
  bool has_comma = false;
  bool fallback_caused_cycle = false;  // For use-counting.
  if (!RuntimeEnabledFeatures::CSSShortCircuitVarAttrEnabled() &&
      ConsumeComma(stream)) {
    has_comma = true;
    stream.ConsumeWhitespace();
    // Note that we can enter this function while in a cycle.
    bool in_cycle_before = resolver.InCycle();
    has_fallback = ResolveTokensInto(stream, tree_scope, resolver, context,
                                     function_context,
                                     /* stop_type */ kEOFToken, fallback);
    // Even if the above call to ResolveTokensInto caused a cycle
    // (resolver.InCycle()==true), we must proceed to look for cycles in the
    // non-fallback branch. For example, suppose we are currently resolving
    // the ', var(--z)' part of the following:
    //
    //  --x: var(--y, var(--z));
    //  --y: var(--x);
    //  --z: var(--x);
    //
    // The properties --x and --z would be detected as cyclic as a result,
    // but we also need to discover the cycle between --x and --y.
    fallback_caused_cycle = !in_cycle_before && resolver.InCycle();
  }

  // Within a function context (i.e. when resolving values within the body of
  // an @function rule), var() must first look for local variables
  // and arguments.
  //
  // https://drafts.csswg.org/css-mixins-1/#locally-substitute-a-var
  if (function_context) {
    // Locals shadow arguments, which shadow custom properties
    // from the element.

    for (FunctionContext* frame = function_context; frame;
         frame = frame->parent) {
      // Ensure that any local variable with a matching name is applied
      // (i.e. exists on frame->locals).
      LookupAndApplyLocalVariable(var_name, resolver, context, *frame);
      if (std::optional<CSSVariableData*> local_variable =
              FindOrNullopt(frame->locals, var_name)) {
        if (RuntimeEnabledFeatures::CSSShortCircuitVarAttrEnabled()) {
          // Note that we should indeed pass `function_context` here,
          // and not `frame`. This is because the `function_context
          // is only used to resolve the fallback, which must be interpreted
          // in the function context holding the var() function.
          return AppendDataWithFallback(local_variable.value(), stream,
                                        tree_scope, resolver, context,
                                        function_context, out);
        }
        return ResolveArgumentOrLocalInto(
            local_variable.value(), (has_fallback ? &fallback : nullptr), out);
      }
      // Note that there is no "lookup and apply" step for arguments; one
      // argument cannot reference another using var() or similar.
      if (std::optional<CSSVariableData*> argument =
              FindOrNullopt(frame->arguments, var_name)) {
        if (RuntimeEnabledFeatures::CSSShortCircuitVarAttrEnabled()) {
          return AppendDataWithFallback(argument.value(), stream, tree_scope,
                                        resolver, context, function_context,
                                        out);
        }
        return ResolveArgumentOrLocalInto(
            argument.value(), (has_fallback ? &fallback : nullptr), out);
      }
    }
  }

  CustomProperty property(var_name, state_.GetDocument());

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

  CSSVariableData* data = GetVariableData(property);

  // If substitution is not allowed, treat the value as
  // invalid-at-computed-value-time.
  //
  // https://drafts.csswg.org/css-variables/#animation-tainted
  if (!resolver.AllowSubstitution(data)) {
    data = nullptr;
  }

  if (RuntimeEnabledFeatures::CSSShortCircuitVarAttrEnabled()) {
    if (resolver.InCycle()) {
      // Either DetectCycle() or LookupAndApply() caused a cycle.
      return false;
    }
    return AppendDataWithFallback(data, stream, tree_scope, resolver, context,
                                  function_context, out);
  }

  // Note that this check catches cycles detected by the DetectCycle call above,
  // but also any cycles detected during processing of the fallback near the
  // start of this function.
  if (resolver.InCycle()) {
    if (data && fallback_caused_cycle) {
      // If we do have `data`, we're not actually going to use the fallback.
      // TODO(crbug.com/397690639): Ignore cycles in unused fallbacks.
      CountUse(WebFeature::kCSSVarFallbackCycle);
    }
    return false;
  }

  // The fallback must match the syntax of the referenced custom
  // property, even if the fallback isn't used.
  //
  // TODO(crbug.com/372475301): Remove this, if possible.
  //
  // https://drafts.css-houdini.org/css-properties-values-api-1/#fallbacks-in-var-references
  if (!RuntimeEnabledFeatures::CSSTypeAgnosticVarFallbackEnabled() &&
      has_comma && !ValidateFallback(property, fallback.OriginalText())) {
    CountUse(WebFeature::kVarFallbackValidation);
    return false;
  }

  if (!data) {
    // No substitution value found; attempt fallback.
    if (has_fallback) {
      return out.AppendFallback(fallback,
                                !fallback.GetAttrTaintedRanges()->empty(),
                                CSSVariableData::kMaxVariableBytes);
    }
    return false;
  }

  return out.Append(data, data->IsAttrTainted(),
                    CSSVariableData::kMaxVariableBytes);
}

bool StyleCascade::ResolveFunctionInto(StringView function_name,
                                       const TreeScope* tree_scope,
                                       CSSParserTokenStream& stream,
                                       CascadeResolver& resolver,
                                       const CSSParserContext& context,
                                       FunctionContext* function_context,
                                       TokenSequence& out) {
  state_.StyleBuilder().SetAffectedByCSSFunction();

  // Note that in this function, we basically have an "outer" tree scope
  // (`tree_scope`) and an "inner" tree scope (`function_tree_scope`).
  // When resolving a <dashed-function> like --foo(arg), arg is resolved
  // in the outer tree scope (which is whatever tree scope the containing
  // declaration exists in), whereas declarations inside the function body
  // (locals + 'result') resolve in the tree scope where --foo() was found.
  // This is to ensure that any <dashed-function>s within --foo()'s body
  // are looked up starting from the tree scope where --foo() is *defined*
  // (not *used*).

  state_.SetHasTreeScopedReference();
  auto [/*StyleRuleFunction*/ function, function_tree_scope] =
      GetDocument().GetStyleEngine().FindFunctionAcrossScopes(
          AtomicString(function_name), tree_scope);
  if (!function) {
    return false;
  }

  CascadeResolver::CycleNode cycle_node = {
      .type = CascadeResolver::CycleNode::Type::kFunction,
      .function = function};
  if (resolver.DetectCycle(cycle_node)) {
    return false;
  }
  CascadeResolver::AutoLock lock(cycle_node, resolver);

  // When parsing function arguments, one of three things can happen
  // (per argument):
  //
  //   1. The argument is present, and valid. In this case, the value ends
  //      up in `function_arguments`, fully resolved.
  //   2. The argument is defaulted, meaning that the argument is missing,
  //      but the parameter has a default. In this case, that default value
  //      ends up in `unresolved_defaults`, and if the parameter has a
  //      non-universal type, that type ends up in `default_types`. The default
  //      values may refer to other arguments, which means that that basically
  //      resolve "inside" the function (unlike non-defaulted arguments).
  //   3. The argument is either missing (with no default), or present but
  //      invalid in some other way (e.g. type mismatch). In this case,
  //      `ResolveFunctionInto` as a whole fails.
  //
  // Soon after all of these hash maps are populated, there is a process
  // of resolving the `unresolved_defaults` (against their corresponding types),
  // and inserting the results into `function_arguments`. This is explained
  // a bit further down in the function.
  HeapHashMap<String, Member<CSSVariableData>> function_arguments;
  HeapHashMap<String, Member<CSSVariableData>> unresolved_defaults;
  HashMap<String, const CSSSyntaxDefinition*> default_types;

  auto insert_default = [&unresolved_defaults, &default_types](
                            const StyleRuleFunction::Parameter& parameter) {
    DCHECK(parameter.default_value);
    unresolved_defaults.insert(parameter.name, parameter.default_value);
    if (!parameter.type.IsUniversal()) {
      default_types.insert(parameter.name, &parameter.type);
    }
  };

  bool first_parameter = true;
  for (const StyleRuleFunction::Parameter& parameter :
       function->GetParameters()) {
    stream.ConsumeWhitespace();

    if (!stream.AtEnd() &&
        (first_parameter || stream.Peek().GetType() == kCommaToken)) {
      first_parameter = false;
      if (stream.Peek().GetType() == kCommaToken) {
        stream.ConsumeIncludingWhitespace();
      }
      StringView argument_string;
      // Handle {}-wrapper.
      // https://drafts.csswg.org/css-values-5/#component-function-commas
      if (stream.Peek().GetType() == kLeftBraceToken) {
        CSSParserTokenStream::BlockGuard guard(stream);
        stream.ConsumeWhitespace();
        DCHECK(!stream.AtEnd());
        argument_string = ConsumeUntilPeekedTypeIs<>(stream);
      } else {
        argument_string = ConsumeUntilPeekedTypeIs<kCommaToken>(stream);
      }
      DCHECK(!argument_string.empty());  // Handled parse-time.
      CSSVariableData* argument_data = CSSVariableData::Create(
          argument_string.ToString(),
          /*is_animation_tainted=*/false, /*is_attr_tainted=*/false,
          /*needs_variable_resolution=*/true);

      // We need to resolve the argument in the context of this function,
      // so that we can do type coercion on the resolved value before the call.
      // In particular, we want any var() within the argument to be resolved
      // in our context; e.g., --foo(var(--a)) should be our a, not foo's a
      // (if that even exists).
      //
      // Note that if this expression comes from directly a function call,
      // as in the example above (and if the return and argument types are the
      // same), we will effectively do type parsing of exactly the same data
      // twice. This is wasteful, and it's possible that we should do something
      // about it if it proves to be a common case.
      argument_data =
          ResolveFunctionExpression(*argument_data, tree_scope, &parameter.type,
                                    resolver, context, function_context);

      // An argument generally "captures" a failed resolution, without
      // propagation to the outer declaration; if e.g. a var() reference fails,
      // we should instead use the default value.
      if (argument_data) {
        function_arguments.insert(parameter.name, argument_data);
      } else if (parameter.default_value) {
        insert_default(parameter);
      } else {
        // An explicit nullptr is needed for shadowing; even if an argument
        // did not resolve successfully, we should not be able to reach
        // a variable with the same name defined in an outer scope.
        function_arguments.insert(parameter.name, nullptr);
      }
    } else if (parameter.default_value) {
      insert_default(parameter);
    } else {
      // Argument was missing, with no default.
      return false;
    }
  }

  if (!stream.AtEnd()) {
    // This could mean that we have more arguments than we have parameters,
    // which isn't allowed.
    return false;
  }

  // Defaulted arguments essentially resolve as typed locals in their
  // own "private" stack frame. We pretend that `unresolved_defaults`
  // are unresolved local variables, and apply those local variables
  // as normal. (This also means cycles between defaulted arguments
  // are handled correctly.)
  //
  // This roughly corresponds to the first invocation of "resolve function
  // styles" in "evaluate a custom function".
  // https://drafts.csswg.org/css-mixins-1/#evaluate-a-custom-function
  if (!unresolved_defaults.empty()) {
    FunctionContext default_context{
        .function = *function,
        .tree_scope = function_tree_scope,
        .arguments = function_arguments,
        .locals = {},  // Populated by ApplyLocalVariables.
        .unresolved_locals = unresolved_defaults,
        .local_types = default_types,
        .parent = function_context};

    ApplyLocalVariables(resolver, context, default_context);

    // Resolving a default may place this function in a cycle,
    // e.g. @function --f(--x:--f()).
    if (resolver.InCycle()) {
      return false;
    }

    // All the resolved locals (i.e. resolved defaults) now exist
    // in `default_context.locals`. We merge all the newly resolved defaulted
    // arguments into `function_arguments`, to make the full set of arguments
    // visible to the "real" stack frame (`local_function_context`).
    for (const auto& [name, value] : default_context.locals) {
      function_arguments.insert(name, value);
    }
  }

  CSSVariableData* unresolved_result = nullptr;
  HeapHashMap<String, Member<CSSVariableData>> unresolved_locals;

  // Flattens the function body, consisting of any number of
  // CSSFunctionDeclarations and conditional rules, into the final
  // (unresolved) values for 'result'/locals.
  FlattenFunctionBody(*function, function_tree_scope, unresolved_result,
                      unresolved_locals);

  if (!unresolved_result) {
    return false;
  }

  // Always empty; local variables are untyped.
  HashMap<String, const CSSSyntaxDefinition*> local_types;

  FunctionContext local_function_context{
      .function = *function,
      .tree_scope = function_tree_scope,
      .arguments = function_arguments,
      .locals = {},  // Populated by ApplyLocalVariables.
      .unresolved_locals = unresolved_locals,
      .local_types = local_types,
      .parent = function_context};

  ApplyLocalVariables(resolver, context, local_function_context);

  // Applying local variables may place this function in a cycle.
  if (resolver.InCycle()) {
    return false;
  }

  CSSVariableData* ret_data = ResolveFunctionExpression(
      *unresolved_result, function_tree_scope, &function->GetReturnType(),
      resolver, context, &local_function_context);
  if (ret_data == nullptr) {
    return false;
  }
  DCHECK(!ret_data->NeedsVariableResolution());
  return out.Append(ret_data, ret_data->IsAttrTainted(),
                    CSSVariableData::kMaxVariableBytes);
}

bool StyleCascade::ResolveArgumentOrLocalInto(CSSVariableData* data,
                                              const TokenSequence* fallback,
                                              TokenSequence& out) {
  CHECK(!RuntimeEnabledFeatures::CSSShortCircuitVarAttrEnabled());

  // Note: `data` may be nullptr when a local variable became invalid
  // due to e.g. failed substitutions.
  if (data) {
    DCHECK(!data->NeedsVariableResolution());
    return out.Append(data, data->IsAttrTainted(),
                      CSSVariableData::kMaxVariableBytes);
  }
  if (fallback) {
    return out.AppendFallback(*fallback,
                              !fallback->GetAttrTaintedRanges()->empty(),
                              CSSVariableData::kMaxVariableBytes);
  }
  return false;
}

bool StyleCascade::AppendDataWithFallback(CSSVariableData* data,
                                          CSSParserTokenStream& stream,
                                          const TreeScope* tree_scope,
                                          CascadeResolver& resolver,
                                          const CSSParserContext& context,
                                          FunctionContext* function_context,
                                          TokenSequence& out) {
  CHECK(RuntimeEnabledFeatures::CSSShortCircuitVarAttrEnabled());

  if (data) {
    DCHECK(!data->NeedsVariableResolution());
    return out.Append(data, data->IsAttrTainted(),
                      CSSVariableData::kMaxVariableBytes);
  }
  // Empty/invalid data; try fallback:
  if (ConsumeComma(stream)) {
    stream.ConsumeWhitespace();
    TokenSequence fallback;
    if (ResolveTokensInto(stream, tree_scope, resolver, context,
                          function_context,
                          /*stop_type=*/kEOFToken, fallback)) {
      return out.AppendFallback(
          fallback,
          /*is_attr_tainted=*/!fallback.GetAttrTaintedRanges()->empty(),
          CSSVariableData::kMaxVariableBytes);
    }
  }
  return false;
}

// Resolves an expression within a function; in practice, either a function
// argument or its return value. In practice, this is about taking a string
// and coercing it into the given type -- and then the caller will convert it
// right back to a string again. This is pretty suboptimal, but it's the way
// registered properties also work, and crucially, without such a resolve step
// (which needs a type), we would not be able to collapse calc() expressions
// and similar, which could cause massive blowup as the values are passed
// through a large tree of function calls.
CSSVariableData* StyleCascade::ResolveFunctionExpression(
    CSSVariableData& unresolved,
    const TreeScope* tree_scope,
    const CSSSyntaxDefinition* type,
    CascadeResolver& resolver,
    const CSSParserContext& context,
    FunctionContext* function_context) {
  CSSVariableData* data = &unresolved;
  if (data->NeedsVariableResolution()) {
    data = ResolveVariableData(data, tree_scope, context, function_context,
                               resolver);
  }
  if (!data) {
    return nullptr;
  }
  // Note that we avoid CSSSyntaxDefinition::Parse for the universal syntax,
  // because it currently disallows CSS-wide keywords.
  // TODO(crbug.com/400340579): Universal syntax should allow CSS-wide keywords.
  if (!type || type->IsUniversal()) {
    return data;
  }
  const CSSValue* value = type->Parse(data->OriginalText(), context,
                                      /*is_animation_tainted=*/false);
  if (!value) {
    return nullptr;
  }
  // Resolve the value as if it were a registered property, to get rid of
  // extraneous calc(), resolve lengths and so on.
  value = &StyleBuilderConverter::ConvertRegisteredPropertyValue(state_, *value,
                                                                 &context);
  return StyleBuilderConverter::ConvertRegisteredPropertyVariableData(
      *value, data->IsAnimationTainted(), data->IsAttrTainted());
}

void StyleCascade::ApplyLocalVariables(CascadeResolver& resolver,
                                       const CSSParserContext& context,
                                       FunctionContext& function_context) {
  for (const auto& [name, data] : function_context.unresolved_locals) {
    if (function_context.locals.find(name) != function_context.locals.end()) {
      // Already applied. This can happen because a call to ResolveLocalVariable
      // may trigger application of other local variables via var().
      continue;
    }
    const CSSSyntaxDefinition* type =
        FindOrNull(function_context.local_types, name);
    CSSVariableData* resolved = ResolveLocalVariable(
        AtomicString(name), *data, type, resolver, context, function_context);
    // Note: The following call may insert an explicit nullptr;
    // this is intentional.
    function_context.locals.insert(name, resolved);
  }
}

void StyleCascade::LookupAndApplyLocalVariable(
    const String& name,
    CascadeResolver& resolver,
    const CSSParserContext& context,
    FunctionContext& function_context) {
  auto resolved_it = function_context.locals.find(name);
  if (resolved_it != function_context.locals.end()) {
    // Already applied.
    return;
  }

  auto unresolved_it = function_context.unresolved_locals.find(name);
  if (unresolved_it == function_context.unresolved_locals.end()) {
    // Does not exist.
    return;
  }

  const CSSSyntaxDefinition* type =
      FindOrNull(function_context.local_types, name);
  CSSVariableData* resolved =
      ResolveLocalVariable(AtomicString(name), *unresolved_it->value, type,
                           resolver, context, function_context);
  // Note: we may insert an explicit nullptr here; this is intentional.
  function_context.locals.insert(name, resolved);
}

CSSVariableData* StyleCascade::ResolveLocalVariable(
    const AtomicString& name,
    CSSVariableData& unresolved,
    const CSSSyntaxDefinition* type,
    CascadeResolver& resolver,
    const CSSParserContext& context,
    FunctionContext& function_context) {
  CascadeResolver::CycleNode cycle_node = {
      .type = CascadeResolver::CycleNode::Type::kLocalVariable,
      .name = name,
      .function = &function_context.function};
  if (resolver.DetectCycle(cycle_node)) {
    return nullptr;
  }
  CascadeResolver::AutoLock lock(cycle_node, resolver);
  CSSVariableData* resolved =
      ResolveFunctionExpression(unresolved, function_context.tree_scope, type,
                                resolver, context, &function_context);

  if (!resolved) {
    return nullptr;
  }

  // The CSS-wide keywords 'initial' and 'inherit' have special meaning
  // for local variables: 'initial' refers the argument value of the
  // same name, and 'inherit' refers to the value of the outer stack
  // frame.
  //
  // https://drafts.csswg.org/css-mixins-1/#resolve-function-styles
  if (const CSSValue* css_wide = ParseAsCSSWideKeyword(*resolved)) {
    if (css_wide->IsInitialValue()) {
      return FindOrNullopt(function_context.arguments, name).value_or(nullptr);
    }
    if (css_wide->IsInheritedValue()) {
      // The inherited value is whatever var(`name`) would resolve to
      // in the parent stack frame.
      return ResolveLikeVar(name, resolver, context, function_context.parent);
    }
    // Other CSS-wide keywords (e.g. 'revert') are invalid.
    return nullptr;
  }

  return resolved;
}

void StyleCascade::FlattenFunctionBody(
    StyleRuleGroup& group,
    const TreeScope* function_tree_scope,
    CSSVariableData*& result,
    HeapHashMap<String, Member<CSSVariableData>>& locals) {
  for (const Member<StyleRuleBase>& child : group.ChildRules()) {
    if (auto* function_declarations =
            DynamicTo<StyleRuleFunctionDeclarations>(child.Get())) {
      const CSSPropertyValueSet& propety_value_set =
          function_declarations->Properties();
      for (const CSSPropertyValue& property_value :
           propety_value_set.Properties()) {
        if (property_value.PropertyID() == CSSPropertyID::kVariable) {
          const auto& unresolved_local =
              To<CSSUnparsedDeclarationValue>(property_value.Value());
          locals.Set(property_value.CustomPropertyName(),
                     unresolved_local.VariableDataValue());
        }
      }
      if (auto* r = DynamicTo<CSSUnparsedDeclarationValue>(
              propety_value_set.GetPropertyCSSValue(CSSPropertyID::kResult))) {
        result = r->VariableDataValue();
      }
    } else if (auto* supports_rule =
                   DynamicTo<StyleRuleSupports>(child.Get())) {
      if (supports_rule->ConditionIsSupported()) {
        FlattenFunctionBody(*supports_rule, function_tree_scope, result,
                            locals);
      }
    } else if (auto* media_rule = DynamicTo<StyleRuleMedia>(child.Get())) {
      state_.StyleBuilder().SetAffectedByFunctionalMedia();
      if (GetDocument().GetStyleEngine().EvaluateFunctionalMediaQuery(
              *media_rule->MediaQueries())) {
        FlattenFunctionBody(*media_rule, function_tree_scope, result, locals);
      }
    } else if (auto* container_rule =
                   DynamicTo<StyleRuleContainer>(child.Get())) {
      state_.StyleBuilder().SetHasContainerRelativeValue();
      if (EvaluateContainerQuery(
              state_.GetElement(), state_.GetPseudoId(),
              container_rule->GetContainerQuery(), function_tree_scope,
              state_.NearestSizeContainer(), match_result_)) {
        FlattenFunctionBody(*container_rule, function_tree_scope, result,
                            locals);
      }
    }
  }
}

bool StyleCascade::ResolveEnvInto(CSSParserTokenStream& stream,
                                  const TreeScope* tree_scope,
                                  CascadeResolver& resolver,
                                  const CSSParserContext& context,
                                  TokenSequence& out) {
  state_.StyleBuilder().SetHasEnv();
  AtomicString variable_name = ConsumeVariableName(stream);

  if (variable_name == "safe-area-inset-bottom") {
    state_.StyleBuilder().SetHasEnvSafeAreaInsetBottom();
    state_.GetDocument()
        .GetStyleEngine()
        .SetNeedsToUpdateComplexSafeAreaConstraints();
  }

  DCHECK(stream.AtEnd() || (stream.Peek().GetType() == kCommaToken) ||
         (stream.Peek().GetType() == kNumberToken));

  WTF::Vector<unsigned> indices;
  if (!stream.AtEnd() && stream.Peek().GetType() != kCommaToken) {
    do {
      const CSSParserToken& token = stream.ConsumeIncludingWhitespaceRaw();
      DCHECK(token.GetNumericValueType() == kIntegerValueType);
      DCHECK(token.NumericValue() >= 0.);
      indices.push_back(static_cast<unsigned>(token.NumericValue()));
    } while (stream.Peek().GetType() == kNumberToken);
  }

  DCHECK(stream.AtEnd() || (stream.Peek().GetType() == kCommaToken));

  CSSVariableData* data =
      GetEnvironmentVariable(variable_name, std::move(indices));

  if (!data) {
    if (ConsumeComma(stream)) {
      return ResolveTokensInto(stream, tree_scope, resolver, context,
                               /* function_context */ nullptr,
                               /* stop_type */ kEOFToken, out);
    }
    return false;
  }

  return out.Append(data, data->IsAttrTainted());
}

bool StyleCascade::ResolveAttrInto(CSSParserTokenStream& stream,
                                   const TreeScope* tree_scope,
                                   CascadeResolver& resolver,
                                   const CSSParserContext& context,
                                   FunctionContext* function_context,
                                   TokenSequence& out) {
  AtomicString local_name = ConsumeVariableName(stream);
  CascadeResolver::CycleNode cycle_node = {
      .type = CascadeResolver::CycleNode::Type::kAttribute, .name = local_name};
  if (resolver.DetectCycle(cycle_node)) {
    return false;
  }
  CascadeResolver::AutoLock lock(cycle_node, resolver);
  std::optional<CSSAttrType> attr_type = CSSAttrType::Consume(stream);
  bool missing_attr_type = false;
  if (!attr_type.has_value()) {
    missing_attr_type = true;
    attr_type = CSSAttrType::GetDefaultValue();
  }
  DCHECK(stream.AtEnd() || stream.Peek().GetType() == kCommaToken);

  Element& element = state_.GetUltimateOriginatingElementOrSelf();
  // TODO(crbug.com/387281256): Support namespaces.
  const String& attribute_value = element.getAttributeNS(
      /*namespace_uri=*/g_null_atom, element.LowercaseIfNecessary(local_name));

  // Handle substitution in attribute value
  // https://drafts.csswg.org/css-values-5/#parse-with-a-syntax
  String substituted_attribute_value = attribute_value;
  if (!attribute_value.IsNull() && attr_type->IsSyntax()) {
    TokenSequence substituted_attribute_token_sequence;
    CSSParserTokenStream attribute_value_stream(attribute_value);
    if (!ResolveTokensInto(attribute_value_stream, tree_scope, resolver,
                           context, function_context,
                           /* stop_type */ kEOFToken,
                           substituted_attribute_token_sequence)) {
      return false;
    }
    substituted_attribute_value =
        substituted_attribute_token_sequence.OriginalText();
  }

  // Parse value according to the attribute type.
  // https://drafts.csswg.org/css-values-5/#typedef-attr-type
  const CSSValue* substitution_value =
      (substituted_attribute_value.IsNull())
          ? nullptr
          : attr_type->Parse(substituted_attribute_value, context);

  if (RuntimeEnabledFeatures::CSSShortCircuitVarAttrEnabled()) {
    if (substitution_value) {
      return out.Append(substitution_value, /*is_attr_tainted=*/true,
                        CSSVariableData::kMaxVariableBytes);
    }

    TokenSequence fallback;
    if (ConsumeComma(stream)) {
      stream.ConsumeWhitespace();
      if (!ResolveTokensInto(stream, tree_scope, resolver, context,
                             function_context,
                             /*stop_type=*/kEOFToken, fallback)) {
        return false;
      }
    } else if (missing_attr_type) {
      // If the <attr-type> argument is omitted, the fallback defaults to the
      // empty string if omitted.
      // https://drafts.csswg.org/css-values-5/#attr-notation
      if (!fallback.Append("''", /*is_attr_tainted=*/true,
                           CSSVariableData::kMaxVariableBytes)) {
        return false;
      }
    } else {
      return false;
    }

    return out.AppendFallback(fallback, /*is_attr_tainted=*/true,
                              CSSVariableData::kMaxVariableBytes);
  };

  // Resolve fallback
  if (ConsumeComma(stream)) {
    stream.ConsumeWhitespace();

    TokenSequence fallback;
    DCHECK(!resolver.InCycle());
    if (!ResolveTokensInto(stream, tree_scope, resolver, context,
                           function_context,
                           /* stop_type */ kEOFToken, fallback)) {
      if (substitution_value && resolver.InCycle()) {
        CountUse(WebFeature::kCSSAttrFallbackCycle);
      }
      return false;
    }
    if (!substitution_value) {
      return out.AppendFallback(fallback, /* is_attr_tainted */ true,
                                CSSVariableData::kMaxVariableBytes);
    }
  }

  if (missing_attr_type && !substitution_value) {
    // If the <attr-type> argument is omitted, the fallback defaults to the
    // empty string if omitted.
    // https://drafts.csswg.org/css-values-5/#attr-notation
    return out.Append(String("''"),
                      /* is_attr_tainted */ true,
                      CSSVariableData::kMaxVariableBytes);
  }

  if (substitution_value) {
    out.Append(substitution_value, /* is_attr_tainted */ true,
               CSSVariableData::kMaxVariableBytes);
    return true;
  }

  return false;
}

bool StyleCascade::ResolveAutoBaseInto(CSSParserTokenStream& stream,
                                       const TreeScope* tree_scope,
                                       CascadeResolver& resolver,
                                       const CSSParserContext& context,
                                       TokenSequence& out) {
  const CSSProperty& appearance = GetCSSPropertyAppearance();
  if (resolver.DetectCycle(appearance)) {
    return false;
  }
  LookupAndApply(appearance, resolver);

  // Note that the InBaseSelectAppearance() flag is set by StyleAdjuster,
  // which hasn't happened yet. Therefore we also need to check
  // HasBaseSelectAppearance() here.
  bool has_base_appearance = state_.StyleBuilder().HasBaseSelectAppearance() ||
                             state_.StyleBuilder().InBaseSelectAppearance();

  if (has_base_appearance) {
    // We want to the second argument.
    stream.SkipUntilPeekedTypeIs<kCommaToken>();
    CHECK(!stream.AtEnd());
    stream.ConsumeIncludingWhitespace();  // kCommaToken
  }

  return ResolveTokensInto(stream, tree_scope, resolver, context,
                           /* function_context */ nullptr,
                           /* stop_type */ kCommaToken, out);
}

bool StyleCascade::EvalIfInitial(CSSVariableData* value,
                                 const CustomProperty& property) {
  if (!property.IsRegistered()) {
    return !value;
  }
  const StyleInitialData* initial_data = state_.StyleBuilder().InitialData();
  DCHECK(initial_data);
  CSSVariableData* initial_variable_data =
      initial_data->GetVariableData(property.GetPropertyNameAtomicString());
  return value->EqualsIgnoringAttrTainting(*initial_variable_data);
}

bool StyleCascade::EvalIfInherit(CSSVariableData* value,
                                 const CustomProperty& property) {
  if (!state_.ParentStyle()) {
    return EvalIfInitial(value, property);
  }

  bool is_inherited_property = property.IsInherited();

  CSSVariableData* parent_data = state_.ParentStyle()->GetVariableData(
      property.GetPropertyNameAtomicString(), is_inherited_property);

  return value->EqualsIgnoringAttrTainting(*parent_data);
}

bool StyleCascade::EvalIfKeyword(const CSSValue& keyword_value,
                                 CSSVariableData* value,
                                 const CustomProperty& property) {
  if (keyword_value.IsInitialValue()) {
    return EvalIfInitial(value, property);
  }

  if (keyword_value.IsInheritedValue()) {
    return EvalIfInherit(value, property);
  }

  if (keyword_value.IsUnsetValue()) {
    if (state_.IsInheritedForUnset(property)) {
      return EvalIfInherit(value, property);
    } else {
      return EvalIfInitial(value, property);
    }
  }

  // revert and revert-layer keywords
  return false;
}

const CSSValue* StyleCascade::CoerceIntoNumericValue(
    const CSSUnparsedDeclarationValue& unparsed_value,
    const TreeScope* tree_scope,
    CascadeResolver& resolver,
    const CSSParserContext& context,
    FunctionContext* function_context) {
  StringView unparsed_value_str(
      unparsed_value.VariableDataValue()->OriginalText());
  CSSVariableData* data = nullptr;
  if (IsVariableNameOnly(unparsed_value_str)) {
    data = ResolveLikeVar(AtomicString(unparsed_value_str), resolver, context,
                          function_context);
  } else {
    CSSParserTokenStream decl_value_stream(unparsed_value_str);
    TokenSequence substituted_token_sequence;
    if (ResolveTokensInto(
            decl_value_stream, tree_scope, resolver, context, function_context,
            /* stop_type */ kEOFToken, substituted_token_sequence)) {
      data = substituted_token_sequence.BuildVariableData();
    }
  }

  if (!data) {
    return nullptr;
  }

  CSSSyntaxDefinition syntax_definition =
      CSSSyntaxDefinition::CreateNumericSyntax();
  const CSSValue* parsed_value = syntax_definition.Parse(
      data->OriginalText(), context,
      /* is_animation_tainted= */ data->IsAnimationTainted(),
      /* is_attr_tainted= */ data->IsAttrTainted());

  if (!parsed_value) {
    return nullptr;
  }

  const auto* primitive_value = DynamicTo<CSSPrimitiveValue>(parsed_value);

  if (!primitive_value) {
    return nullptr;
  }

  if (!primitive_value->IsCalculated() &&
      (primitive_value->IsPx() || primitive_value->IsPercentage())) {
    return parsed_value;
  }

  if (primitive_value->IsLength() || primitive_value->IsPercentage() ||
      !primitive_value->IsResolvableBeforeLayout()) {
    Length length = primitive_value->ConvertToLength(
        state_.CssToLengthConversionData().Unzoomed());
    return CSSPrimitiveValue::CreateFromLength(length, 1);
  }

  if (primitive_value->IsNumber()) {
    return CSSNumericLiteralValue::Create(
        primitive_value->ComputeNumber(state_.CssToLengthConversionData()),
        CSSPrimitiveValue::UnitType::kNumber);
  }

  if (primitive_value->IsAngle()) {
    return CSSNumericLiteralValue::Create(
        primitive_value->ComputeDegrees(state_.CssToLengthConversionData()),
        CSSPrimitiveValue::UnitType::kDegrees);
  }

  if (primitive_value->IsTime()) {
    return CSSNumericLiteralValue::Create(
        primitive_value->ComputeSeconds(state_.CssToLengthConversionData()),
        CSSPrimitiveValue::UnitType::kSeconds);
  }

  if (primitive_value->IsResolution()) {
    return CSSNumericLiteralValue::Create(
        primitive_value->ComputeDotsPerPixel(
            state_.CssToLengthConversionData()),
        CSSPrimitiveValue::UnitType::kDotsPerPixel);
  }
  return nullptr;
}

KleeneValue StyleCascade::EvalIfStyleFeature(
    const MediaQueryFeatureExpNode& feature,
    const TreeScope* tree_scope,
    CascadeResolver& resolver,
    const CSSParserContext& context,
    FunctionContext* function_context,
    bool& is_attr_tainted) {
  const MediaQueryExpBounds& bounds = feature.Bounds();

  if (bounds.IsRange()) {
    DCHECK(RuntimeEnabledFeatures::CSSContainerStyleQueriesRangeEnabled());
    DCHECK(feature.HasStyleRange());
    KleeneValue result = KleeneValue::kTrue;
    const CSSValue* reference =
        CoerceIntoNumericValue(feature.ReferenceValue(), tree_scope, resolver,
                               context, function_context);
    if (!reference) {
      return KleeneValue::kFalse;
    }
    if (bounds.left.IsValid()) {
      const auto& left =
          To<CSSUnparsedDeclarationValue>(bounds.left.value.GetCSSValue());
      const CSSValue* left_resolved = CoerceIntoNumericValue(
          left, tree_scope, resolver, context, function_context);
      if (!left_resolved) {
        return KleeneValue::kFalse;
      }
      result = KleeneAnd(
          result, MediaQueryEvaluator::EvalIfRange(*reference, *left_resolved,
                                                   bounds.left.op, true));
    }
    if (bounds.right.IsValid()) {
      const auto& right =
          To<CSSUnparsedDeclarationValue>(bounds.right.value.GetCSSValue());
      const CSSValue* right_resolved = CoerceIntoNumericValue(
          right, tree_scope, resolver, context, function_context);
      if (!right_resolved) {
        return KleeneValue::kFalse;
      }
      result = KleeneAnd(
          result, MediaQueryEvaluator::EvalIfRange(*reference, *right_resolved,
                                                   bounds.right.op, false));
    }
    return result;
  }

  DCHECK(bounds.right.op == MediaQueryOperator::kNone);

  AtomicString property_name(feature.Name());
  CustomProperty property(property_name, GetDocument());

  CSSVariableData* computed_data =
      ResolveLikeVar(property_name, resolver, context, function_context);

  if (resolver.InCycle()) {
    return KleeneValue::kFalse;
  }

  if (computed_data && computed_data->IsAttrTainted()) {
    is_attr_tainted = true;
  }

  if (!bounds.right.value.IsValid()) {
    return computed_data ? KleeneValue::kTrue : KleeneValue::kFalse;
  }

  const CSSValue& query_specified = bounds.right.value.GetCSSValue();

  if (query_specified.IsCSSWideKeyword()) {
    return EvalIfKeyword(query_specified, computed_data, property)
               ? KleeneValue::kTrue
               : KleeneValue::kFalse;
  }

  if (!computed_data) {
    return KleeneValue::kFalse;
  }

  const auto& decl_value = To<CSSUnparsedDeclarationValue>(query_specified);

  CSSParserTokenStream decl_value_stream(
      decl_value.VariableDataValue()->OriginalText());
  TokenSequence substituted_token_sequence;
  if (!ResolveTokensInto(
          decl_value_stream, tree_scope, resolver, context, function_context,
          /* stop_type */ kEOFToken, substituted_token_sequence)) {
    return KleeneValue::kFalse;
  }

  CSSVariableData* computed_query_data =
      substituted_token_sequence.BuildVariableData();

  if (property.IsRegistered()) {
    const CSSValue* parsed_value =
        property.Parse(substituted_token_sequence.OriginalText(), context,
                       CSSParserLocalContext());
    if (!parsed_value) {
      return KleeneValue::kFalse;
    }
    const CSSValue& computed_query_value =
        StyleBuilderConverter::ConvertRegisteredPropertyValue(
            state_, *parsed_value, decl_value.ParserContext());
    computed_query_data =
        StyleBuilderConverter::ConvertRegisteredPropertyVariableData(
            computed_query_value, /* is_animation_tainted */ false,
            computed_query_data->IsAttrTainted());
  }

  if (computed_query_data->IsAttrTainted()) {
    is_attr_tainted = true;
  }

  if (computed_data->EqualsIgnoringAttrTainting(*computed_query_data)) {
    return KleeneValue::kTrue;
  }

  return KleeneValue::kFalse;
}

KleeneValue StyleCascade::EvalIfTest(const IfCondition& if_condition,
                                     const TreeScope* tree_scope,
                                     CascadeResolver& resolver,
                                     const CSSParserContext& context,
                                     FunctionContext* function_context,
                                     bool& is_attr_tainted) {
  if (auto* n = DynamicTo<IfTestStyle>(if_condition)) {
    const MediaQueryExpNode* query_exp = n->GetMediaQueryExpNode();
    DCHECK(query_exp);

    return MediaEval(*query_exp, [this, &tree_scope, &resolver, &context,
                                  &function_context, &is_attr_tainted](
                                     const MediaQueryFeatureExpNode& feature) {
      return EvalIfStyleFeature(feature, tree_scope, resolver, context,
                                function_context, is_attr_tainted);
    });
  }
  if (auto* n = DynamicTo<IfTestMedia>(if_condition)) {
    DCHECK(RuntimeEnabledFeatures::CSSInlineIfForMediaQueriesEnabled());

    const MediaQuerySet* query_set = n->GetMediaQuerySet();
    DCHECK(query_set);

    state_.StyleBuilder().SetAffectedByFunctionalMedia();
    return GetDocument().GetStyleEngine().EvaluateFunctionalMediaQuery(
               *query_set)
               ? KleeneValue::kTrue
               : KleeneValue::kFalse;
  }
  if (auto* n = DynamicTo<IfTestSupports>(if_condition)) {
    DCHECK(RuntimeEnabledFeatures::CSSInlineIfForSupportsQueriesEnabled());
    return n->GetResult() ? KleeneValue::kTrue : KleeneValue::kFalse;
  }
  NOTREACHED();
}

bool StyleCascade::EvalIfCondition(CSSParserTokenStream& stream,
                                   const TreeScope* tree_scope,
                                   CascadeResolver& resolver,
                                   const CSSParserContext& context,
                                   FunctionContext* function_context,
                                   bool& is_attr_tainted) {
  CSSIfParser parser(context);
  const IfCondition* if_condition = parser.ConsumeIfCondition(stream);
  DCHECK(if_condition);
  stream.ConsumeWhitespace();
  DCHECK_EQ(stream.Peek().GetType(), kColonToken);
  stream.ConsumeIncludingWhitespace();

  return IfEval(*if_condition,
                [this, &tree_scope, &resolver, &context, &function_context,
                 &is_attr_tainted](const IfCondition& if_condition) {
                  return EvalIfTest(if_condition, tree_scope, resolver, context,
                                    function_context, is_attr_tainted);
                }) == KleeneValue::kTrue;
}

bool StyleCascade::ResolveIfInto(CSSParserTokenStream& stream,
                                 const TreeScope* tree_scope,
                                 CascadeResolver& resolver,
                                 const CSSParserContext& context,
                                 FunctionContext* function_context,
                                 TokenSequence& out) {
  stream.ConsumeWhitespace();
  bool is_attr_tainted = false;
  bool eval_result = EvalIfCondition(stream, tree_scope, resolver, context,
                                     function_context, is_attr_tainted);
  while (!eval_result) {
    stream.SkipUntilPeekedTypeIs<kSemicolonToken>();
    if (stream.AtEnd()) {
      // None of the conditions matched, so should be IACVT.
      return false;
    }
    stream.ConsumeIncludingWhitespace();  // kSemicolonToken
    if (stream.AtEnd()) {
      // None of the conditions matched, so should be IACVT.
      return false;
    }
    eval_result = EvalIfCondition(stream, tree_scope, resolver, context,
                                  function_context, is_attr_tainted);
  }
  TokenSequence if_result;
  if (!ResolveTokensInto(stream, tree_scope, resolver, context,
                         function_context,
                         /* stop_type */ kSemicolonToken, if_result)) {
    return false;
  }

  return out.Append(if_result, is_attr_tainted);
}

CSSVariableData* StyleCascade::ResolveLikeVar(
    const AtomicString& property_name,
    CascadeResolver& resolver,
    const CSSParserContext& context,
    FunctionContext* function_context) {
  CSSParserTokenStream property_name_stream(property_name);
  TokenSequence sequence;
  // To avoid duplicating lookup logic, we pretend that we're resolving
  // a var() with `property_name`. This will resolve to the appropriate
  // custom property, local variable, or function argument. We also get
  // cycle handling for free.
  //
  // We can safely pass tree_scope=nullptr here; since this pretend-var()
  // is guaranteed to not have any fallback, there is no way for it to
  // contain any <dashed-functions> (that would require a tree-scoped lookup).
  if (ResolveVarInto(property_name_stream, /*tree_scope=*/nullptr, resolver,
                     context, function_context, sequence)) {
    return sequence.BuildVariableData();
  }
  return nullptr;
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
    const CSSUnparsedDeclarationValue& value) {
  // TODO(crbug.com/985028): CSSUnparsedDeclarationValue should always have a
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
                                    StringView value) const {
  if (!property.IsRegistered()) {
    return true;
  }
  auto context_mode =
      state_.GetDocument().GetExecutionContext()->GetSecureContextMode();
  auto* context = StrictCSSParserContext(context_mode);
  auto local_context = CSSParserLocalContext();
  return property.Parse(value, *context, local_context);
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
  state_.StyleBuilder().SetHasVariableReference();
}

void StyleCascade::ApplyUnresolvedEnv(CascadeResolver& resolver) {
  // Currently the only field that depends on parsing unresolved env().
  ApplyIsBottomRelativeToSafeAreaInset(resolver);
}

void StyleCascade::ApplyIsBottomRelativeToSafeAreaInset(
    CascadeResolver& resolver) {
  if (!state_.StyleBuilder().HasEnvSafeAreaInsetBottom() ||
      !map_.NativeBitset().Has(CSSPropertyID::kBottom)) {
    return;
  }

  const CascadePriority* p = map_.FindKnownToExist(CSSPropertyID::kBottom);
  CascadeOrigin origin = p->GetOrigin();
  if (origin >= CascadeOrigin::kAnimation) {
    // Effect values from animations/transition do not contain env().
    return;
  }

  const CSSValue* value = ValueAt(match_result_, p->GetPosition());
  const auto* unparsed = DynamicTo<CSSUnparsedDeclarationValue>(value);
  if (!unparsed) {
    return;  // Does not contain env().
  }

  // First, check if env(safe-inset-area-bottom [, ...]) exists anywhere
  // in the specified value.
  if (!css_parsing_utils::ContainsSafeAreaInsetBottom(
          unparsed->VariableDataValue()->OriginalText())) {
    return;
  }

  // Resolve all substitution functions within that value, and check if we end
  // up with a simple calc() sum of literal <length> values.
  CascadeResolver::AutoLock lock(GetCSSPropertyBottom(), resolver);
  TokenSequence sequence;
  const TreeScope& tree_scope = TreeScopeAt(match_result_, p->GetPosition());
  CSSParserTokenStream stream(unparsed->VariableDataValue()->OriginalText());
  if (!ResolveTokensInto(stream, &tree_scope, resolver,
                         *unparsed->ParserContext(),
                         /*function_context=*/nullptr,
                         /*stop_type=*/kEOFToken, sequence)) {
    return;
  }

  if (css_parsing_utils::IsSimpleSum(sequence.OriginalText())) {
    state_.StyleBuilder().SetIsBottomRelativeToSafeAreaInset(true);

    UseCounter::Count(
        state_.GetDocument(),
        WebFeature::kCSSEnvironmentVariable_SafeAreaInsetBottom_FastPath);
  }
}

bool StyleCascade::TreatAsRevertLayer(CascadePriority priority) const {
  return priority.IsTryStyle() && !ComputedStyle::HasOutOfFlowPosition(
                                      state_.StyleBuilder().GetPosition());
}

const Document& StyleCascade::GetDocument() const {
  return state_.GetDocument();
}

const TreeScope* StyleCascade::GetTreeScope(CascadePriority priority) const {
  CascadeOrigin origin = priority.GetOrigin();
  if (origin == CascadeOrigin::kAuthor) {
    return &TreeScopeAt(match_result_, priority.GetPosition());
  }
  if (origin == CascadeOrigin::kAuthorPresentationalHint) {
    return &GetDocument();
  }
  return nullptr;
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
      property.SurrogateFor(state_.StyleBuilder().GetWritingDirection());
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
