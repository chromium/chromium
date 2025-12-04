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
#include "third_party/blink/renderer/core/css/media_list.h"
#include "third_party/blink/renderer/core/css/media_query_exp.h"
#include "third_party/blink/renderer/core/css/navigation_query.h"
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
#include "third_party/blink/renderer/core/layout/layout_view.h"
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

AtomicString ConsumeAndComputeVariableName(CSSParserTokenStream& stream,
                                           const CSSParserContext& context,
                                           StyleResolverState& state) {
  stream.ConsumeWhitespace();
  if (stream.Peek().GetType() == kIdentToken) {
    // Note that CSSVariableParser has previously checked that the ident
    // has a valid form.
    return stream.ConsumeIncludingWhitespaceRaw().Value().ToAtomicString();
  }
  // ident()
  DCHECK_EQ(stream.Peek().FunctionId(), CSSValueID::kIdent);
  CSSFunctionValue* ident_function =
      css_parsing_utils::ConsumeIdentFunction(stream, context);
  DCHECK(ident_function);
  AtomicString computed_ident = CSSCustomIdentValue::ComputeIdent(
      *ident_function, state.CssToLengthConversionData());
  if (!CSSVariableParser::IsValidVariableName(computed_ident)) {
    return AtomicString("unknown");
  }
  return computed_ident;
}

bool ConsumeComma(CSSParserTokenStream& stream) {
  if (stream.Peek().GetType() == kCommaToken) {
    stream.ConsumeRaw();
    return true;
  }
  return false;
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

const MixinParameterBindings* MixinParameterBindingsAt(
    const MatchResult& result,
    uint32_t position) {
  wtf_size_t matched_properties_index = DecodeMatchedPropertiesIndex(position);
  const MatchedProperties& properties =
      result.GetMatchedProperties()[matched_properties_index];
  return properties.mixin_parameter_bindings;
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

  // For performance avoid stack initialization on this large object.
  STACK_UNINITIALIZED CascadeResolver resolver(filter, ++generation_);

  ApplyCascadeAffecting(resolver);

  ApplyViewportUnitAffecting(resolver);
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

const CSSValue* StyleCascade::Resolve(
    const CSSPropertyName& name,
    const CSSValue& value,
    const TreeScope* tree_scope,
    const MixinParameterBindings* mixin_parameter_bindings,
    CascadeOrigin origin,
    CascadeResolver& resolver) {
  CSSPropertyRef ref(name, state_.GetDocument());

  const CSSValue* resolved = Resolve(ResolveSurrogate(ref.GetProperty()), value,
                                     tree_scope, mixin_parameter_bindings,
                                     CascadePriority(origin), origin, resolver);

  DCHECK(resolved);

  // TODO(crbug.com/40753334): Cycles in animations get special handling by our
  // implementation. This is not per spec, but the correct behavior is not
  // defined at the moment.
  if (resolved->IsCyclicVariableValue()) {
    return nullptr;
  }

  // TODO(crbug.com/40753334): We should probably not return 'unset' for
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

const CSSValue* StyleCascade::Resolve(
    StyleResolverState& state,
    const CSSPropertyName& name,
    const CSSValue& value,
    const TreeScope* tree_scope,
    const MixinParameterBindings* mixin_parameter_bindings) {
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

  return cascade.Resolve(name, value, tree_scope, mixin_parameter_bindings,
                         origin, resolver);
}

const CSSUnparsedDeclarationValue* StyleCascade::ResolveSubstitutions(
    StyleResolverState& state,
    const CSSUnparsedDeclarationValue& value,
    const TreeScope* tree_scope,
    const MixinParameterBindings* mixin_parameter_bindings) {
  STACK_UNINITIALIZED StyleCascade cascade(state);
  CascadeResolver resolver(CascadeFilter(), /*generation=*/0);
  const CSSParserContext* context = cascade.GetParserContext(value);
  CSSParserTokenStream stream(value.VariableDataValue()->OriginalText());
  TokenSequence sequence;
  if (!cascade.ResolveTokensInto(stream, tree_scope, resolver, *context,
                                 /*function_context=*/nullptr,
                                 /*stop_type=*/kEOFToken, sequence)) {
    return nullptr;
  }
  return MakeGarbageCollected<CSSUnparsedDeclarationValue>(
      sequence.BuildVariableData());
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

// We have to resolve these early because they affect scrollbars, which are
// sometimes subtracted from the viewport when deriving viewport units.
void StyleCascade::ApplyViewportUnitAffecting(CascadeResolver& resolver) {
  if (!IsRootElement() || state_.IsForPseudoElement() ||
      match_result_.PseudoElementStyles().Has(kPseudoIdScrollbar)) {
    return;
  }
  if (map_.NativeBitset().Has(CSSPropertyID::kOverflowX)) {
    LookupAndApply(GetCSSPropertyOverflowX(), resolver);
  }
  if (map_.NativeBitset().Has(CSSPropertyID::kOverflowY)) {
    LookupAndApply(GetCSSPropertyOverflowY(), resolver);
  }
  if (map_.NativeBitset().Has(CSSPropertyID::kScrollbarGutter)) {
    LookupAndApply(GetCSSPropertyScrollbarGutter(), resolver);
  }
  if (map_.NativeBitset().Has(CSSPropertyID::kScrollbarWidth)) {
    LookupAndApply(GetCSSPropertyScrollbarWidth(), resolver);
  }
  PaintLayerScrollableArea::StyleBasedScrollbarData some_scroll_properties{
      state_.StyleBuilder().OverflowX(), state_.StyleBuilder().OverflowY(),
      static_cast<ScrollbarGutter>(state_.StyleBuilder().ScrollbarGutter()),
      state_.StyleBuilder().ScrollbarWidth(),
      // WritingMode is resolved early in `ApplyCascadeAffecting`.
      state_.StyleBuilder().GetWritingMode()};

  LayoutView* view = state_.GetDocument().GetLayoutView();
  DCHECK(view);
  const gfx::Size to_subtract =
      view->GetScrollableArea()->ComputeScrollbarWidthsForViewportUnits(
          some_scroll_properties);

  // This should take care of any v* units on the root element for the remainder
  // of this style resolution.
  if (RuntimeEnabledFeatures::SmallerViewportUnitsEnabled()) {
    state_.SubtractScrollbarsFromViewportUnits(to_subtract);
  }

  state_.StyleBuilder().SetUnconditionalScrollbarSize(to_subtract);
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
  const MixinParameterBindings* mixin_parameter_bindings =
      GetMixinParameterBindings(*priority);
  value = Resolve(property, *value, tree_scope, mixin_parameter_bindings,
                  *priority, origin, resolver);
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
  if (other_text.empty()) {
    // There are no tokens, so this cannot affect anything.
    // In particular, do not overwrite last_token_ with kEOFToken.
    return true;
  }
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

const CSSValue* StyleCascade::Resolve(
    const CSSProperty& property,
    const CSSValue& value,
    const TreeScope* tree_scope,
    const MixinParameterBindings* mixin_parameter_bindings,
    CascadePriority priority,
    CascadeOrigin& origin,
    CascadeResolver& resolver) {
  DCHECK(!property.IsSurrogate());

  const CSSValue* result = ResolveSubstitutions(
      property, value, tree_scope, mixin_parameter_bindings, resolver);
  DCHECK(result);

  if (result->IsRevertValue()) {
    return ResolveRevert(property, *result, tree_scope, origin, resolver);
  }
  if (result->IsRevertLayerValue() || TreatAsRevertLayer(priority)) {
    return ResolveRevertLayer(property, tree_scope, priority, origin, resolver);
  }
  if (result->IsRevertRuleValue()) {
    return ResolveRevertRule(property, tree_scope, priority, origin, resolver);
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

const CSSValue*
StyleCascade::MakeFunctionContextFromMixinAndResolveSubstitutions(
    const CSSProperty& property,
    const CSSValue& value,
    const TreeScope* tree_scope,
    const HeapVector<Member<const MixinParameterBindings>, 4>& binding_chain,
    unsigned binding_index,
    FunctionContext* function_context,
    CascadeResolver& resolver) {
  if (binding_index == binding_chain.size()) {
    // We have all the mixin context that we need, so we can now do the actual
    // resolution.
    if (const auto* v = DynamicTo<CSSUnparsedDeclarationValue>(value)) {
      if (property.GetCSSPropertyName().IsCustomProperty()) {
        return ResolveCustomProperty(property, *v, tree_scope, function_context,
                                     resolver);
      } else {
        return ResolveVariableReference(property, *v, tree_scope,
                                        function_context, resolver);
      }
    }
    if (const auto* v =
            DynamicTo<cssvalue::CSSPendingSubstitutionValue>(value)) {
      return ResolvePendingSubstitution(property, *v, tree_scope,
                                        function_context, resolver);
    }

    return &value;
  }

  const CSSParserContext* context;
  if (const auto* v = DynamicTo<CSSUnparsedDeclarationValue>(value)) {
    context = GetParserContext(*v);
  } else {
    // The caller will never send us down this path unless we have
    // either a CSSUnparsedDeclarationValue or a CSSPendingSubstitutionValue.
    context = GetParserContext(
        *To<cssvalue::CSSPendingSubstitutionValue>(value).ShorthandValue());
  }

  HeapHashMap<String, Member<CSSVariableData>> function_arguments;
  HeapHashMap<String, Member<CSSVariableData>> unresolved_defaults;
  HashMap<String, const CSSSyntaxDefinition*> local_types;

  const MixinParameterBindings* mixin_parameter_bindings =
      binding_chain[binding_index];
  for (const auto& binding : mixin_parameter_bindings->GetBindings()) {
    local_types.insert(binding.key, &binding.value.syntax);
    ResolveFunctionParameter(binding.key, binding.value.value,
                             binding.value.default_value, binding.value.syntax,
                             tree_scope, resolver, *context, function_context,
                             function_arguments, unresolved_defaults);
  }

  if (!ResolveUnresolvedFunctionDefaults(
          unresolved_defaults, local_types, /*function=*/nullptr,
          /*tree_scope=*/nullptr, function_context, resolver, context,
          function_arguments)) {
    return nullptr;
  }

  FunctionContext ctx = {
      .arguments = function_arguments,
      .locals = {},  // Populated by ApplyLocalVariables.
      .unresolved_locals = {},
      .local_types = local_types,
      .parent = function_context,
  };
  ApplyLocalVariables(resolver, *context, ctx);

  return MakeFunctionContextFromMixinAndResolveSubstitutions(
      property, value, tree_scope, binding_chain, binding_index + 1, &ctx,
      resolver);
}

const CSSValue* StyleCascade::ResolveSubstitutions(
    const CSSProperty& property,
    const CSSValue& value,
    const TreeScope* tree_scope,
    const MixinParameterBindings* mixin_parameter_bindings,
    CascadeResolver& resolver) {
  HeapVector<Member<const MixinParameterBindings>, 4> binding_chain;
  if (mixin_parameter_bindings) {
    // Even though we are within a mixin, it's not given that we
    // actually need the mixin bindings to compute a value;
    // there's no need to use CPU cycles to set up the entire stack
    // just to resolve “color: red”. Furthermore, without
    // a CSSUnparsedDeclarationValue, we don't have a parser context
    // to resolve all of these values against. So in these cases,
    // we just ignore the entire mixin context.
    if (const auto* v1 = DynamicTo<CSSUnparsedDeclarationValue>(value)) {
      if (!v1->VariableDataValue()->NeedsVariableResolution()) {
        mixin_parameter_bindings = nullptr;
      }
    } else if (const auto* v2 =
                   DynamicTo<cssvalue::CSSPendingSubstitutionValue>(value)) {
      DCHECK(
          v2->ShorthandValue()->VariableDataValue()->NeedsVariableResolution());
    } else {
      mixin_parameter_bindings = nullptr;
    }
  }

  for (const MixinParameterBindings* cur_bindings = mixin_parameter_bindings;
       cur_bindings; cur_bindings = cur_bindings->GetParentMixin()) {
    binding_chain.push_back(cur_bindings);
  }
  std::ranges::reverse(binding_chain);

  return MakeFunctionContextFromMixinAndResolveSubstitutions(
      property, value, tree_scope, binding_chain, 0,
      /*function_context=*/nullptr, resolver);
}

const CSSValue* StyleCascade::ResolveCustomProperty(
    const CSSProperty& property,
    const CSSUnparsedDeclarationValue& decl,
    const TreeScope* tree_scope,
    FunctionContext* function_context_from_mixins,
    CascadeResolver& resolver) {
  DCHECK(!property.IsSurrogate());

  DCHECK(!resolver.IsLocked(property));
  CascadeResolver::AutoLock lock(property, resolver);

  CSSVariableData* data = decl.VariableDataValue();

  if (data->NeedsVariableResolution()) {
    data = ResolveVariableData(data, tree_scope, *GetParserContext(decl),
                               function_context_from_mixins, resolver);
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
    FunctionContext* function_context_from_mixins,
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
                        function_context_from_mixins,
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
    FunctionContext* function_context_from_mixins,
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
                           function_context_from_mixins,
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

  // Reaching this point means that the shorthand parser did not produce
  // a value for `property` (one of the longhands), which should never happen.
  //
  // TODO(crbug.com/40527196): We can reach here due to our incorrect adjustment
  // of writing-mode and direction for table display types.
  return cssvalue::CSSUnsetValue::Create();
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
                     GetTreeScope(*p), GetMixinParameterBindings(*p), *p,
                     origin, resolver);
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
                 GetTreeScope(*p), GetMixinParameterBindings(*p), *p, origin,
                 resolver);
}

const CSSValue* StyleCascade::ResolveRevertRule(const CSSProperty& property,
                                                const TreeScope* tree_scope,
                                                CascadePriority priority,
                                                CascadeOrigin& origin,
                                                CascadeResolver& resolver) {
  const CascadePriority* p = map_.FindRevertRule(property.GetCSSPropertyName(),
                                                 priority.GetRuleIndex());
  if (!p || !p->HasOrigin()) {
    origin = CascadeOrigin::kNone;
    return cssvalue::CSSUnsetValue::Create();
  }
  origin = p->GetOrigin();
  return Resolve(property, *ValueAt(match_result_, p->GetPosition()),
                 GetTreeScope(*p), GetMixinParameterBindings(*p), *p, origin,
                 resolver);
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
      value.Transform(), state_.GetAnchoredContainerWritingDirection());
  return Resolve(property, *flipped, tree_scope,
                 /*mixin_parameter_bindings=*/nullptr, priority, origin,
                 resolver);
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
    } else if (token.FunctionId() == CSSValueID::kInherit) {
      CSSParserTokenStream::BlockGuard guard(stream);
      success &= ResolveInheritInto(stream, tree_scope, resolver, context,
                                    function_context, out);
    } else if (token.FunctionId() == CSSValueID::kEnv) {
      CSSParserTokenStream::BlockGuard guard(stream);
      success &= ResolveEnvInto(stream, tree_scope, resolver, context, out);
    } else if (token.FunctionId() == CSSValueID::kAttr) {
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
  AtomicString var_name =
      ConsumeAndComputeVariableName(stream, context, state_);
  // Note that `var_name` may be "unknown" when an ident() function
  // didn't produce a valid variable name. Looking up this custom property
  // name (which is unreachable for authors, and therefore never set)
  // automatically gives the correct IACVT/fallback behavior without
  // any explicit handling.
  DCHECK(stream.AtEnd() || (stream.Peek().GetType() == kCommaToken));

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
        // Note that we should indeed pass `function_context` here,
        // and not `frame`. This is because the `function_context
        // is only used to resolve the fallback, which must be interpreted
        // in the function context holding the var() function.
        return AppendDataWithFallback(local_variable.value(), stream,
                                      tree_scope, resolver, context,
                                      function_context, out);
      }
      // Note that there is no "lookup and apply" step for arguments; one
      // argument cannot reference another using var() or similar.
      if (std::optional<CSSVariableData*> argument =
              FindOrNullopt(frame->arguments, var_name)) {
        return AppendDataWithFallback(argument.value(), stream, tree_scope,
                                      resolver, context, function_context, out);
      }
    }
  }

  CustomProperty property(var_name, state_.GetDocument());

  // Any custom property referenced (by anything, even just once) in the
  // document can currently not be animated on the compositor. Hence we mark
  // properties that have been referenced.
  MarkIsReferenced(property);

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

  if (resolver.InCycle()) {
    // Either DetectCycle() or LookupAndApply() caused a cycle.
    return false;
  }
  return AppendDataWithFallback(data, stream, tree_scope, resolver, context,
                                function_context, out);
}

bool StyleCascade::ResolveInheritInto(CSSParserTokenStream& stream,
                                      const TreeScope* tree_scope,
                                      CascadeResolver& resolver,
                                      const CSSParserContext& context,
                                      FunctionContext* function_context,
                                      TokenSequence& out) {
  if (function_context) {
    // In a function context, we can simply pass the parent function context.
    // ResolveVarInto can already handle this via its dynamic scoping mechanism
    // for local variables.
    return ResolveVarInto(stream, tree_scope, resolver, context,
                          function_context->parent, out);
  }

  // If we're at the top-level (i.e. not in a function context),
  // then we need to explicitly find the inherited value. Note that since
  // we're depending on the already-finished ComputedStyle of the parent
  // element, cycles with properties on *this* element cannot happen.
  AtomicString var_name =
      ConsumeAndComputeVariableName(stream, context, state_);
  if (!stream.AtEnd()) {
    DCHECK_EQ(stream.Peek().GetType(), kCommaToken);
  }
  CustomProperty property(var_name, state_.GetDocument());
  if (!property.IsInherited()) {
    state_.StyleBuilder().SetHasExplicitInheritance();
    state_.ParentStyle()->SetChildHasExplicitInheritance();
  }
  CSSVariableData* data = GetInheritedVariableData(property);
  return AppendDataWithFallback(data, stream, tree_scope, resolver, context,
                                /*function_context=*/nullptr, out);
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
  HashMap<String, const CSSSyntaxDefinition*> local_types;

  HeapVector<String> arguments = CSSVariableParser::ConsumeFunctionArguments(
      stream, function->GetParameters().size());
  if (!stream.AtEnd()) {
    return false;
  }

  unsigned parameter_idx = 0;
  for (const StyleRuleFunction::Parameter& parameter :
       function->GetParameters()) {
    local_types.insert(parameter.name, &parameter.type);

    if (parameter_idx < arguments.size()) {
      CSSVariableData* argument_data = CSSVariableData::Create(
          arguments[parameter_idx],
          /*is_animation_tainted=*/false, /*is_attr_tainted=*/false,
          /*needs_variable_resolution=*/true);

      ResolveFunctionParameter(parameter.name, argument_data,
                               parameter.default_value, parameter.type,
                               tree_scope, resolver, context, function_context,
                               function_arguments, unresolved_defaults);
    } else if (parameter.default_value) {
      unresolved_defaults.insert(parameter.name, parameter.default_value);
    } else {
      // Argument was missing, with no default.
      return false;
    }
    ++parameter_idx;
  }

  if (!ResolveUnresolvedFunctionDefaults(
          unresolved_defaults, local_types, function, function_tree_scope,
          function_context, resolver, &context, function_arguments)) {
    return false;
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

  FunctionContext local_function_context{
      .function = function,
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

  CSSVariableData* ret_data = ResolveTypedExpression(
      *unresolved_result, function_tree_scope, &function->GetReturnType(),
      resolver, context, &local_function_context);
  if (ret_data == nullptr) {
    return false;
  }

  DCHECK(!ret_data->NeedsVariableResolution());
  return out.Append(ret_data, ret_data->IsAttrTainted(),
                    CSSVariableData::kMaxVariableBytes);
}

void StyleCascade::ResolveFunctionParameter(
    const String& name,
    CSSVariableData* argument_data,
    CSSVariableData* default_value,
    const CSSSyntaxDefinition& type,
    const TreeScope* tree_scope,
    CascadeResolver& resolver,
    const CSSParserContext& context,
    FunctionContext* function_context,
    HeapHashMap<String, Member<CSSVariableData>>& function_arguments,
    HeapHashMap<String, Member<CSSVariableData>>& unresolved_defaults) {
  if (argument_data) {
    // We need to resolve the argument in the context of this function,
    // so that we can do type coercion on the resolved value before the
    // call. In particular, we want any var() within the argument to be
    // resolved in our context; e.g., --foo(var(--a)) should be our a,
    // not foo's a (if that even exists).
    //
    // Note that if this expression comes from directly a function call,
    // as in the example above (and if the return and argument types are
    // the same), we will effectively do type parsing of exactly the
    // same data twice. This is wasteful, and it's possible that we
    // should do something about it if it proves to be a common case.
    argument_data = ResolveTypedExpression(*argument_data, tree_scope, &type,
                                           resolver, context, function_context);
  }

  // An argument generally "captures" a failed resolution, without
  // propagation to the outer declaration; if e.g. a var() reference
  // fails, we should instead use the default value.
  if (argument_data) {
    function_arguments.insert(name, argument_data);
  } else if (default_value) {
    unresolved_defaults.insert(name, default_value);
  } else {
    // An explicit nullptr is needed for shadowing; even if an
    // argument did not resolve successfully, we should not be able to
    // reach a variable with the same name defined in an outer scope.
    function_arguments.insert(name, nullptr);
  }
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
bool StyleCascade::ResolveUnresolvedFunctionDefaults(
    const HeapHashMap<String, Member<CSSVariableData>>& unresolved_defaults,
    const HashMap<String, const CSSSyntaxDefinition*>& local_types,
    StyleRuleFunction* function,
    const TreeScope* tree_scope,
    FunctionContext* function_context,
    CascadeResolver& resolver,
    const CSSParserContext* context,
    HeapHashMap<String, Member<CSSVariableData>>& function_arguments) {
  if (!unresolved_defaults.empty()) {
    FunctionContext default_context{
        .function = function,
        .tree_scope = tree_scope,
        .arguments = function_arguments,
        .locals = {},  // Populated by ApplyLocalVariables.
        .unresolved_locals = unresolved_defaults,
        .local_types = local_types,
        .parent = function_context};

    ApplyLocalVariables(resolver, *context, default_context);

    // Resolving a default may place this function in a cycle,
    // e.g. @function --f(--x:--f()).
    if (resolver.InCycle()) {
      return false;
    }

    // All the resolved locals (i.e. resolved defaults) now exist
    // in `default_context.locals`. We merge all the newly resolved
    // defaulted arguments into `function_arguments`, to make the full set
    // of arguments visible to the "real" stack frame.
    for (const auto& [name, val] : default_context.locals) {
      function_arguments.insert(name, val);
    }
  }
  return true;
}

bool StyleCascade::AppendDataWithFallback(CSSVariableData* data,
                                          CSSParserTokenStream& stream,
                                          const TreeScope* tree_scope,
                                          CascadeResolver& resolver,
                                          const CSSParserContext& context,
                                          FunctionContext* function_context,
                                          TokenSequence& out) {
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

// Resolves a typed expression; in practice, either a function
// argument or its return value. In practice, this is about taking a string
// and coercing it into the given type -- and then the caller will convert it
// right back to a string again. This is pretty suboptimal, but it's the way
// registered properties also work, and crucially, without such a resolve step
// (which needs a type), we would not be able to collapse calc() expressions
// and similar, which could cause massive blowup as the values are passed
// through a large tree of function calls.
CSSVariableData* StyleCascade::ResolveTypedExpression(
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

const CSSSyntaxDefinition* StyleCascade::FindVariableType(
    const AtomicString& name,
    FunctionContext* function_context) {
  for (FunctionContext* frame = function_context; frame;
       frame = frame->parent) {
    if (const CSSSyntaxDefinition* type =
            FindOrNull(frame->local_types, name)) {
      return type;
    }
  }
  if (const PropertyRegistration* registration = PropertyRegistration::From(
          GetDocument().GetExecutionContext(), name)) {
    return &registration->Syntax();
  }
  return nullptr;
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
      .function = function_context.function};
  if (resolver.DetectCycle(cycle_node)) {
    return nullptr;
  }
  CascadeResolver::AutoLock lock(cycle_node, resolver);
  // See comment about mixin_parameter_bindings in ResolveFunctionInto().
  CSSVariableData* resolved =
      ResolveTypedExpression(unresolved, function_context.tree_scope, type,
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
    return GetKeywordVariableData(name, *css_wide, resolver, context,
                                  &function_context);
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
    } else if (auto* navigation_rule =
                   DynamicTo<StyleRuleNavigation>(child.Get())) {
      // TODO(crbug.com/431374376): Implement
      (void)navigation_rule;
      NOTREACHED() << "Not yet implemented.";
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

  // Normal env().
  DCHECK(stream.AtEnd() || (stream.Peek().GetType() == kCommaToken) ||
         (stream.Peek().GetType() == kNumberToken));

  Vector<unsigned> indices;
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
  if (data) {
    return out.Append(data, data->IsAttrTainted());
  }

  // Fallback.
  if (ConsumeComma(stream)) {
    return ResolveTokensInto(stream, tree_scope, resolver, context,
                             /* function_context */ nullptr,
                             /* stop_type */ kEOFToken, out);
  }
  return false;
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
    // Since attributes are not parsed by CSSVariableParser, we first need to
    // parse substituted value, not to run into DCHECKs while resolving
    // substitutions.
    if (!CSSVariableParser::ParseDeclarationValue(attribute_value, false,
                                                  context)) {
      // Trigger fallback:
      substituted_attribute_value = g_null_atom;
    } else {
      if (!ResolveTokensInto(attribute_value_stream, tree_scope, resolver,
                             context, function_context,
                             /* stop_type */ kEOFToken,
                             substituted_attribute_token_sequence)) {
        // Trigger fallback:
        substituted_attribute_value = g_null_atom;
      } else {
        substituted_attribute_value =
            substituted_attribute_token_sequence.OriginalText();
      }
    }
  }
  if (resolver.InCycle()) {
    // ResolveTokensInto caused a cycle; trigger fallback.
    substituted_attribute_value = g_null_atom;
  }

  // Parse value according to the attribute type.
  // https://drafts.csswg.org/css-values-5/#typedef-attr-type
  const CSSValue* substitution_value =
      (substituted_attribute_value.IsNull())
          ? nullptr
          : attr_type->Parse(substituted_attribute_value, context);

  if (substitution_value) {
    return out.Append(substitution_value, /*is_attr_tainted=*/true,
                      CSSVariableData::kMaxVariableBytes);
  }
  DCHECK((stream.Peek().GetType() == kCommaToken) || stream.AtEnd());

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
    if (!fallback.Append(String("''"), /*is_attr_tainted=*/true,
                         CSSVariableData::kMaxVariableBytes)) {
      return false;
    }
  } else {
    return false;
  }

  return out.AppendFallback(fallback, /*is_attr_tainted=*/true,
                            CSSVariableData::kMaxVariableBytes);
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

  // Note that the InBaseAppearance() flag is set by StyleAdjuster,
  // which hasn't happened yet. Therefore we also need to check
  // state_.StyleBuilder().Appearance() here, which we are using instead of
  // EffectiveAppearance() because EffectiveAppearance hasn't been calculated
  // from Appearance yet.
  Element& element = state_.GetElement();
  bool has_base_appearance = false;
  if (element.SupportsBaseAppearance(state_.StyleBuilder().Appearance())) {
    has_base_appearance = true;
  } else if (state_.StyleBuilder().InBaseAppearance()) {
    // Don't allow base appearance to be inherited to elements which actually
    // support the appearance property.
    bool could_support_base_appearance =
        element.SupportsBaseAppearance(AppearanceValue::kBase) ||
        element.SupportsBaseAppearance(AppearanceValue::kBaseSelect);
    has_base_appearance = !could_support_base_appearance;
  }

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

CSSVariableData* StyleCascade::GetInitialVariableData(
    const CustomProperty& property) {
  const StyleInitialData* initial_data = state_.StyleBuilder().InitialData();
  if (!initial_data) {
    return nullptr;
  }
  return initial_data->GetVariableData(property.GetPropertyNameAtomicString());
}

CSSVariableData* StyleCascade::GetInheritedVariableData(
    const CustomProperty& property) {
  if (!state_.ParentStyle()) {
    return GetInitialVariableData(property);
  }
  return state_.ParentStyle()->GetVariableData(
      property.GetPropertyNameAtomicString());
}

CSSVariableData* StyleCascade::GetKeywordVariableData(
    const AtomicString& name,
    const CSSValue& keyword_value,
    CascadeResolver& resolver,
    const CSSParserContext& context,
    FunctionContext* function_context) {
  if (function_context) {
    // https://drafts.csswg.org/css-mixins-1/#resolve-function-styles
    if (keyword_value.IsInitialValue()) {
      return FindOrNullopt(function_context->arguments, name).value_or(nullptr);
    }
    if (keyword_value.IsInheritedValue()) {
      // The inherited value is whatever var(`name`) would resolve to
      // in the parent stack frame.
      return ResolveLikeVar(name, resolver, context, function_context->parent);
    }
    // "All other CSS-wide keywords resolve to the guaranteed-invalid value."
  } else {
    CustomProperty property(name, GetDocument());
    if (keyword_value.IsInitialValue()) {
      return GetInitialVariableData(property);
    }
    if (keyword_value.IsInheritedValue()) {
      return GetInheritedVariableData(property);
    }
    if (keyword_value.IsUnsetValue()) {
      if (state_.IsInheritedForUnset(property)) {
        return GetInheritedVariableData(property);
      } else {
        return GetInitialVariableData(property);
      }
    }
  }
  // revert and revert-layer keywords
  return nullptr;
}

const CSSValue* StyleCascade::CoerceIntoNumericValue(
    StyleResolverState& state,
    const CSSUnparsedDeclarationValue& unparsed_value,
    const TreeScope* tree_scope,
    const CSSParserContext& context) {
  STACK_UNINITIALIZED StyleCascade cascade(state);
  CascadeResolver resolver(CascadeFilter(), /* generation */ 0);
  bool is_attr_tainted_unused;
  return cascade.CoerceIntoNumericValueInternal(unparsed_value, tree_scope,
                                                resolver, context, nullptr,
                                                is_attr_tainted_unused);
}

const CSSValue* StyleCascade::CoerceIntoNumericValueInternal(
    const CSSUnparsedDeclarationValue& unparsed_value,
    const TreeScope* tree_scope,
    CascadeResolver& resolver,
    const CSSParserContext& context,
    FunctionContext* function_context,
    bool& is_attr_tainted) {
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

  is_attr_tainted |= data->IsAttrTainted();

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
    const CSSValue* reference = CoerceIntoNumericValueInternal(
        feature.ReferenceValue(), tree_scope, resolver, context,
        function_context, is_attr_tainted);
    if (!reference) {
      return KleeneValue::kFalse;
    }
    if (bounds.left.IsValid()) {
      const auto& left =
          To<CSSUnparsedDeclarationValue>(bounds.left.value.GetCSSValue());
      const CSSValue* left_resolved =
          CoerceIntoNumericValueInternal(left, tree_scope, resolver, context,
                                         function_context, is_attr_tainted);
      if (!left_resolved) {
        return KleeneValue::kFalse;
      }
      result = KleeneAnd(result,
                         MediaQueryEvaluator::EvalStyleRange(
                             *reference, *left_resolved, bounds.left.op, true));
    }
    if (bounds.right.IsValid()) {
      const auto& right =
          To<CSSUnparsedDeclarationValue>(bounds.right.value.GetCSSValue());
      const CSSValue* right_resolved =
          CoerceIntoNumericValueInternal(right, tree_scope, resolver, context,
                                         function_context, is_attr_tainted);
      if (!right_resolved) {
        return KleeneValue::kFalse;
      }
      result = KleeneAnd(
          result, MediaQueryEvaluator::EvalStyleRange(
                      *reference, *right_resolved, bounds.right.op, false));
    }
    return result;
  }

  DCHECK(bounds.right.op == MediaQueryOperator::kNone);

  AtomicString property_name(feature.Name());

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

  CSSVariableData* computed_query_data = nullptr;

  if (query_specified.IsCSSWideKeyword()) {
    // Cascade-dependent keywords are not valid here.
    // https://drafts.csswg.org/css-cascade-5/#cascade-dependent-keyword
    if (query_specified.IsRevertValue() ||
        query_specified.IsRevertLayerValue()) {
      return KleeneValue::kFalse;
    }
    computed_query_data = GetKeywordVariableData(
        property_name, query_specified, resolver, context, function_context);
    if (!computed_data && !computed_query_data) {
      // A CSS-wide keyword was used to query for <guaranteed-invalid>,
      // e.g. style(--x:initial).
      return KleeneValue::kTrue;
    }
  } else {
    const auto& decl_value = To<CSSUnparsedDeclarationValue>(query_specified);
    const CSSSyntaxDefinition* type =
        FindVariableType(property_name, function_context);
    computed_query_data =
        ResolveTypedExpression(*decl_value.VariableDataValue(), tree_scope,
                               type, resolver, context, function_context);
  }

  if (!computed_data || !computed_query_data) {
    return KleeneValue::kFalse;
  }

  if (computed_query_data->IsAttrTainted()) {
    is_attr_tainted = true;
  }

  if (computed_data->EqualsIgnoringAttrTainting(*computed_query_data)) {
    return KleeneValue::kTrue;
  }

  return KleeneValue::kFalse;
}

bool StyleCascade::EvalIfCondition(CSSParserTokenStream& stream,
                                   const TreeScope* tree_scope,
                                   CascadeResolver& resolver,
                                   const CSSParserContext& context,
                                   FunctionContext* function_context,
                                   bool& is_attr_tainted) {
  CSSIfParser parser(context);
  const ConditionalExpNode* root_expression = parser.ConsumeIfCondition(stream);
  DCHECK(root_expression);
  stream.ConsumeWhitespace();
  DCHECK_EQ(stream.Peek().GetType(), kColonToken);
  stream.ConsumeIncludingWhitespace();

  class Handler : public ConditionalExpNodeVisitor {
    STACK_ALLOCATED();

   public:
    using EvaluateStyleFunc =
        base::FunctionRef<KleeneValue(const MediaQueryFeatureExpNode&)>;

    Handler(EvaluateStyleFunc evaluate_style_func,
            StyleResolverState& resolver_state)
        : evaluate_style_func_(evaluate_style_func),
          resolver_state_(resolver_state) {}

    KleeneValue EvaluateNavigationExpNode(
        const NavigationExpNode& node) override {
      // Evaluate navigation() function
      bool result =
          node.NavigationTest().Matches(resolver_state_.GetDocument());
      return result ? KleeneValue::kTrue : KleeneValue::kFalse;
    }

    KleeneValue EvaluateMediaQueryFeatureExpNode(
        const MediaQueryFeatureExpNode& node) override {
      // Evaluate style() function
      return evaluate_style_func_(node);
    }

    KleeneValue EvaluateMediaQuerySet(const MediaQuerySet& query) override {
      // Evaluate media() function
      DCHECK(RuntimeEnabledFeatures::CSSInlineIfForMediaQueriesEnabled());
      resolver_state_.StyleBuilder().SetAffectedByFunctionalMedia();
      StyleEngine& style_engine =
          resolver_state_.GetDocument().GetStyleEngine();
      bool result = style_engine.EvaluateFunctionalMediaQuery(query);
      return result ? KleeneValue::kTrue : KleeneValue::kFalse;
    }

   private:
    EvaluateStyleFunc evaluate_style_func_;
    StyleResolverState& resolver_state_;
  };

  auto evaluate_if_style_feature_func =
      [&](const MediaQueryFeatureExpNode& feature) {
        return EvalIfStyleFeature(feature, tree_scope, resolver, context,
                                  function_context, is_attr_tainted);
      };

  Handler evaluation_context(evaluate_if_style_feature_func, state_);
  return root_expression->Evaluate(evaluation_context) == KleeneValue::kTrue;
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
    Vector<unsigned> indices) const {
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

void StyleCascade::MarkIsReferenced(const CustomProperty& referenced) {
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

const MixinParameterBindings* StyleCascade::GetMixinParameterBindings(
    CascadePriority priority) const {
  CascadeOrigin origin = priority.GetOrigin();
  if (origin == CascadeOrigin::kAuthor) {
    return MixinParameterBindingsAt(match_result_, priority.GetPosition());
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
