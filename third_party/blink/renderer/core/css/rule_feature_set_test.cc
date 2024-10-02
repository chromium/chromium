// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/rule_feature_set.h"

#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/core/css/css_property_value_set.h"
#include "third_party/blink/renderer/core/css/css_selector_list.h"
#include "third_party/blink/renderer/core/css/css_test_helpers.h"
#include "third_party/blink/renderer/core/css/invalidation/invalidation_set.h"
#include "third_party/blink/renderer/core/css/parser/css_parser.h"
#include "third_party/blink/renderer/core/css/rule_set.h"
#include "third_party/blink/renderer/core/css/style_rule.h"
#include "third_party/blink/renderer/core/dom/element_traversal.h"
#include "third_party/blink/renderer/core/execution_context/security_context.h"
#include "third_party/blink/renderer/core/html/html_body_element.h"
#include "third_party/blink/renderer/core/html/html_document.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/html/html_html_element.h"
#include "third_party/blink/renderer/core/testing/null_execution_context.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

using testing::AssertionFailure;
using testing::AssertionResult;
using testing::AssertionSuccess;

namespace blink {

class RuleFeatureSetTest : public testing::Test {
 public:
  RuleFeatureSetTest() = default;

  void SetUp() override {
    document_ =
        HTMLDocument::CreateForTest(execution_context_.GetExecutionContext());
    auto* html = MakeGarbageCollected<HTMLHtmlElement>(*document_);
    html->AppendChild(MakeGarbageCollected<HTMLBodyElement>(*document_));
    document_->AppendChild(html);

    document_->body()->setInnerHTML("<b><i></i></b>");
  }

  SelectorPreMatch CollectFeatures(
      const String& selector_text,
      CSSNestingType nesting_type = CSSNestingType::kNone,
      StyleRule* parent_rule_for_nesting = nullptr) {
    return CollectFeaturesTo(selector_text, rule_feature_set_, nesting_type,
                             parent_rule_for_nesting);
  }

  static SelectorPreMatch CollectFeaturesTo(
      base::span<CSSSelector> selector_vector,
      const StyleScope* style_scope,
      RuleFeatureSet& set) {
    if (selector_vector.empty()) {
      return SelectorPreMatch::kNeverMatches;
    }

    auto* style_rule = StyleRule::Create(
        selector_vector,
        MakeGarbageCollected<MutableCSSPropertyValueSet>(kHTMLStandardMode));
    return CollectFeaturesTo(style_rule, style_scope, set);
  }

  static SelectorPreMatch CollectFeaturesTo(StyleRule* style_rule,
                                            const StyleScope* style_scope,
                                            RuleFeatureSet& set) {
    SelectorPreMatch result = SelectorPreMatch::kNeverMatches;
    for (const CSSSelector* s = style_rule->FirstSelector(); s;
         s = CSSSelectorList::Next(*s)) {
      if (set.CollectFeaturesFromSelector(*s, style_scope) ==
          SelectorPreMatch::kMayMatch) {
        result = SelectorPreMatch::kMayMatch;
      }
    }
    return result;
  }

  static SelectorPreMatch CollectFeaturesTo(
      const String& selector_text,
      RuleFeatureSet& set,
      CSSNestingType nesting_type,
      StyleRule* parent_rule_for_nesting) {
    HeapVector<CSSSelector> arena;
    base::span<CSSSelector> selector_vector = CSSParser::ParseSelector(
        StrictCSSParserContext(SecureContextMode::kInsecureContext),
        nesting_type, parent_rule_for_nesting, false /* is_within_scope */,
        nullptr, selector_text, arena);
    return CollectFeaturesTo(selector_vector, nullptr /* style_scope */, set);
  }

  void ClearFeatures() { rule_feature_set_.Clear(); }

  void CollectInvalidationSetsForClass(InvalidationLists& invalidation_lists,
                                       const char* class_name) const {
    Element* element = Traversal<HTMLElement>::FirstChild(
        *Traversal<HTMLElement>::FirstChild(*document_->body()));
    rule_feature_set_.GetRuleInvalidationData().CollectInvalidationSetsForClass(
        invalidation_lists, *element, AtomicString(class_name));
  }

  void CollectInvalidationSetsForId(InvalidationLists& invalidation_lists,
                                    const char* id) const {
    Element* element = Traversal<HTMLElement>::FirstChild(
        *Traversal<HTMLElement>::FirstChild(*document_->body()));
    rule_feature_set_.GetRuleInvalidationData().CollectInvalidationSetsForId(
        invalidation_lists, *element, AtomicString(id));
  }

  void CollectInvalidationSetsForAttribute(
      InvalidationLists& invalidation_lists,
      const QualifiedName& attribute_name) const {
    Element* element = Traversal<HTMLElement>::FirstChild(
        *Traversal<HTMLElement>::FirstChild(*document_->body()));
    rule_feature_set_.GetRuleInvalidationData()
        .CollectInvalidationSetsForAttribute(invalidation_lists, *element,
                                             attribute_name);
  }

  void CollectInvalidationSetsForPseudoClass(
      InvalidationLists& invalidation_lists,
      CSSSelector::PseudoType pseudo) const {
    Element* element = Traversal<HTMLElement>::FirstChild(
        *Traversal<HTMLElement>::FirstChild(*document_->body()));
    rule_feature_set_.GetRuleInvalidationData()
        .CollectInvalidationSetsForPseudoClass(invalidation_lists, *element,
                                               pseudo);
  }

  void CollectPartInvalidationSet(InvalidationLists& invalidation_lists) const {
    rule_feature_set_.GetRuleInvalidationData().CollectPartInvalidationSet(
        invalidation_lists);
  }

  void CollectUniversalSiblingInvalidationSet(
      InvalidationLists& invalidation_lists) {
    rule_feature_set_.GetRuleInvalidationData()
        .CollectUniversalSiblingInvalidationSet(invalidation_lists, 1);
  }

  void CollectNthInvalidationSet(InvalidationLists& invalidation_lists) {
    rule_feature_set_.GetRuleInvalidationData().CollectNthInvalidationSet(
        invalidation_lists);
  }

  bool NeedsHasInvalidationForClass(const char* class_name) {
    return rule_feature_set_.GetRuleInvalidationData()
        .NeedsHasInvalidationForClass(AtomicString(class_name));
  }

  void MergeInto(RuleFeatureSet& rule_feature_set) {
    rule_feature_set.Merge(rule_feature_set_);
  }

  using BackingType = InvalidationSet::BackingType;

  template <BackingType type>
  HashSet<AtomicString> ToHashSet(
      typename InvalidationSet::Backing<type>::Range range) {
    HashSet<AtomicString> hash_set;
    for (auto str : range) {
      hash_set.insert(str);
    }
    return hash_set;
  }

  HashSet<AtomicString> ClassSet(const InvalidationSet& invalidation_set) {
    return ToHashSet<BackingType::kClasses>(invalidation_set.Classes());
  }

  HashSet<AtomicString> IdSet(const InvalidationSet& invalidation_set) {
    return ToHashSet<BackingType::kIds>(invalidation_set.Ids());
  }

  HashSet<AtomicString> TagNameSet(const InvalidationSet& invalidation_set) {
    return ToHashSet<BackingType::kTagNames>(invalidation_set.TagNames());
  }

  HashSet<AtomicString> AttributeSet(const InvalidationSet& invalidation_set) {
    return ToHashSet<BackingType::kAttributes>(invalidation_set.Attributes());
  }

  AssertionResult HasNoInvalidation(InvalidationSetVector& invalidation_sets) {
    if (invalidation_sets.size() != 0) {
      return AssertionFailure() << "has " << invalidation_sets.size()
                                << " invalidation set(s), should have 0";
    }
    return AssertionSuccess();
  }

  AssertionResult HasSelfInvalidation(
      InvalidationSetVector& invalidation_sets) {
    if (invalidation_sets.size() != 1) {
      return AssertionFailure() << "has " << invalidation_sets.size()
                                << " invalidation set(s), should have 1";
    }
    if (!invalidation_sets[0]->InvalidatesSelf()) {
      return AssertionFailure() << "should invalidate self";
    }
    return AssertionSuccess();
  }

  AssertionResult HasNoSelfInvalidation(
      InvalidationSetVector& invalidation_sets) {
    if (invalidation_sets.size() != 1u) {
      return AssertionFailure() << "has " << invalidation_sets.size()
                                << " invalidation set(s), should have 1";
    }
    if (invalidation_sets[0]->InvalidatesSelf()) {
      return AssertionFailure() << "should not invalidate self";
    }
    return AssertionSuccess();
  }

  AssertionResult HasSelfInvalidationSet(
      InvalidationSetVector& invalidation_sets) {
    if (invalidation_sets.size() != 1u) {
      return AssertionFailure() << "has " << invalidation_sets.size()
                                << " invalidation set(s), should have 1";
    }
    if (!invalidation_sets[0]->IsSelfInvalidationSet()) {
      return AssertionFailure() << "should be the self-invalidation set";
    }
    return AssertionSuccess();
  }

  AssertionResult HasNotSelfInvalidationSet(
      InvalidationSetVector& invalidation_sets) {
    if (invalidation_sets.size() != 1u) {
      return AssertionFailure() << "has " << invalidation_sets.size()
                                << " invalidation set(s), should have 1";
    }
    if (invalidation_sets[0]->IsSelfInvalidationSet()) {
      return AssertionFailure() << "should not be the self-invalidation set";
    }
    return AssertionSuccess();
  }

  AssertionResult HasWholeSubtreeInvalidation(
      InvalidationSetVector& invalidation_sets) {
    if (invalidation_sets.size() != 1u) {
      return AssertionFailure() << "has " << invalidation_sets.size()
                                << " invalidation set(s), should have 1";
    }
    if (!invalidation_sets[0]->WholeSubtreeInvalid()) {
      return AssertionFailure() << "should invalidate whole subtree";
    }
    return AssertionSuccess();
  }

  AssertionResult HasClassInvalidation(
      const char* class_name,
      InvalidationSetVector& invalidation_sets) {
    if (invalidation_sets.size() != 1u) {
      return AssertionFailure() << "has " << invalidation_sets.size()
                                << " invalidation set(s), should have 1";
    }
    HashSet<AtomicString> classes = ClassSet(*invalidation_sets[0]);
    if (classes.size() != 1u) {
      return AssertionFailure() << classes.size() << " should be 1";
    }
    if (!classes.Contains(AtomicString(class_name))) {
      return AssertionFailure() << "should invalidate class " << class_name;
    }
    return AssertionSuccess();
  }

  AssertionResult HasClassInvalidation(
      const char* first_class_name,
      const char* second_class_name,
      InvalidationSetVector& invalidation_sets) {
    if (invalidation_sets.size() != 1u) {
      return AssertionFailure() << "has " << invalidation_sets.size()
                                << " invalidation set(s), should have 1";
    }
    HashSet<AtomicString> classes = ClassSet(*invalidation_sets[0]);
    if (classes.size() != 2u) {
      return AssertionFailure() << classes.size() << " should be 2";
    }
    if (!classes.Contains(AtomicString(first_class_name))) {
      return AssertionFailure()
             << "should invalidate class " << first_class_name;
    }
    if (!classes.Contains(AtomicString(second_class_name))) {
      return AssertionFailure()
             << "should invalidate class " << second_class_name;
    }
    return AssertionSuccess();
  }

  AssertionResult HasClassInvalidation(
      const AtomicString& first_class_name,
      const AtomicString& second_class_name,
      const AtomicString& third_class_name,
      InvalidationSetVector& invalidation_sets) {
    if (invalidation_sets.size() != 1u) {
      return AssertionFailure() << "has " << invalidation_sets.size()
                                << " invalidation set(s), should have 1";
    }
    HashSet<AtomicString> classes = ClassSet(*invalidation_sets[0]);
    if (classes.size() != 3u) {
      return AssertionFailure() << classes.size() << " should be 3";
    }
    if (!classes.Contains(first_class_name)) {
      return AssertionFailure()
             << "should invalidate class " << first_class_name;
    }
    if (!classes.Contains(second_class_name)) {
      return AssertionFailure()
             << "should invalidate class " << second_class_name;
    }
    if (!classes.Contains(third_class_name)) {
      return AssertionFailure()
             << "should invalidate class " << third_class_name;
    }
    return AssertionSuccess();
  }

  AssertionResult HasSiblingClassInvalidation(
      unsigned max_direct_adjacent_selectors,
      const char* sibling_name,
      InvalidationSetVector& invalidation_sets) {
    if (invalidation_sets.size() != 1u) {
      return AssertionFailure() << "has " << invalidation_sets.size()
                                << " invalidation set(s), should have 1";
    }
    const auto& sibling_invalidation_set =
        To<SiblingInvalidationSet>(*invalidation_sets[0]);
    HashSet<AtomicString> classes = ClassSet(sibling_invalidation_set);
    if (classes.size() != 1u) {
      return AssertionFailure() << classes.size() << " should be 1";
    }
    if (!classes.Contains(AtomicString(sibling_name))) {
      return AssertionFailure()
             << "should invalidate sibling id " << sibling_name;
    }
    if (sibling_invalidation_set.MaxDirectAdjacentSelectors() !=
        max_direct_adjacent_selectors) {
      return AssertionFailure()
             << sibling_invalidation_set.MaxDirectAdjacentSelectors()
             << " should be " << max_direct_adjacent_selectors;
    }
    return AssertionSuccess();
  }

  AssertionResult HasSiblingIdInvalidation(
      unsigned max_direct_adjacent_selectors,
      const char* sibling_name,
      InvalidationSetVector& invalidation_sets) {
    if (invalidation_sets.size() != 1u) {
      return AssertionFailure() << "has " << invalidation_sets.size()
                                << " invalidation set(s), should have 1";
    }
    const auto& sibling_invalidation_set =
        To<SiblingInvalidationSet>(*invalidation_sets[0]);
    HashSet<AtomicString> ids = IdSet(*invalidation_sets[0]);
    if (ids.size() != 1u) {
      return AssertionFailure() << ids.size() << " should be 1";
    }
    if (!ids.Contains(AtomicString(sibling_name))) {
      return AssertionFailure()
             << "should invalidate sibling id " << sibling_name;
    }
    if (sibling_invalidation_set.MaxDirectAdjacentSelectors() !=
        max_direct_adjacent_selectors) {
      return AssertionFailure()
             << sibling_invalidation_set.MaxDirectAdjacentSelectors()
             << " should be " << max_direct_adjacent_selectors;
    }
    return AssertionSuccess();
  }

  AssertionResult HasSiblingDescendantInvalidation(
      unsigned max_direct_adjacent_selectors,
      const char* sibling_name,
      const char* descendant_name,
      InvalidationSetVector& invalidation_sets) {
    if (invalidation_sets.size() != 1u) {
      return AssertionFailure() << "has " << invalidation_sets.size()
                                << " invalidation set(s), should have 1";
    }
    const auto& sibling_invalidation_set =
        To<SiblingInvalidationSet>(*invalidation_sets[0]);
    HashSet<AtomicString> classes = ClassSet(sibling_invalidation_set);
    if (classes.size() != 1u) {
      return AssertionFailure() << classes.size() << " should be 1";
    }
    if (!classes.Contains(AtomicString(sibling_name))) {
      return AssertionFailure()
             << "classes.Contains(sibling_name) should be true";
    }
    if (sibling_invalidation_set.MaxDirectAdjacentSelectors() !=
        max_direct_adjacent_selectors) {
      return AssertionFailure()
             << sibling_invalidation_set.MaxDirectAdjacentSelectors()
             << " should be " << max_direct_adjacent_selectors;
    }

    HashSet<AtomicString> descendant_classes =
        ClassSet(*sibling_invalidation_set.SiblingDescendants());
    if (descendant_classes.size() != 1u) {
      return AssertionFailure() << descendant_classes.size() << " should be 1";
    }
    if (!descendant_classes.Contains(AtomicString(descendant_name))) {
      return AssertionFailure()
             << "should invalidate descendant class " << descendant_name;
    }
    return AssertionSuccess();
  }

  AssertionResult HasSiblingDescendantInvalidation(
      unsigned max_direct_adjacent_selectors,
      const char* descendant_name,
      InvalidationSetVector& invalidation_sets) {
    if (invalidation_sets.size() != 1u) {
      return AssertionFailure() << "has " << invalidation_sets.size()
                                << " invalidation set(s), should have 1";
    }
    const auto& sibling_invalidation_set =
        To<SiblingInvalidationSet>(*invalidation_sets[0]);
    if (!sibling_invalidation_set.WholeSubtreeInvalid()) {
      return AssertionFailure() << "should sibling-invalidate whole subtree";
    }
    if (sibling_invalidation_set.MaxDirectAdjacentSelectors() !=
        max_direct_adjacent_selectors) {
      return AssertionFailure()
             << sibling_invalidation_set.MaxDirectAdjacentSelectors()
             << " should be " << max_direct_adjacent_selectors;
    }
    if (!sibling_invalidation_set.SiblingDescendants()) {
      return AssertionFailure() << "sibling set should have descendants";
    }
    HashSet<AtomicString> descendant_classes =
        ClassSet(*sibling_invalidation_set.SiblingDescendants());
    if (descendant_classes.size() != 1u) {
      return AssertionFailure() << descendant_classes.size() << " should be 1";
    }
    if (!descendant_classes.Contains(AtomicString(descendant_name))) {
      return AssertionFailure()
             << "should descendant invalidate " << descendant_name;
    }
    return AssertionSuccess();
  }

  AssertionResult
  HasSiblingAndSiblingDescendantInvalidationForLogicalCombinationsInHas(
      const char* sibling_name,
      const char* sibling_name_for_sibling_descendant,
      const char* descendant_name,
      InvalidationSetVector& invalidation_sets) {
    if (invalidation_sets.size() != 1u) {
      return AssertionFailure() << "has " << invalidation_sets.size()
                                << " invalidation set(s), should have 1";
    }
    const auto& sibling_invalidation_set =
        To<SiblingInvalidationSet>(*invalidation_sets[0]);
    HashSet<AtomicString> classes = ClassSet(sibling_invalidation_set);
    if (classes.size() != 2u) {
      return AssertionFailure() << classes.size() << " should be 2";
    }
    if (!classes.Contains(AtomicString(sibling_name))) {
      return AssertionFailure() << "should sibling invalidate " << sibling_name;
    }
    if (!classes.Contains(AtomicString(sibling_name_for_sibling_descendant))) {
      return AssertionFailure() << "should sibling invalidate "
                                << sibling_name_for_sibling_descendant;
    }
    if (sibling_invalidation_set.MaxDirectAdjacentSelectors() !=
        SiblingInvalidationSet::kDirectAdjacentMax) {
      return AssertionFailure()
             << sibling_invalidation_set.MaxDirectAdjacentSelectors()
             << " should be " << SiblingInvalidationSet::kDirectAdjacentMax;
    }

    HashSet<AtomicString> descendant_classes =
        ClassSet(*sibling_invalidation_set.SiblingDescendants());
    if (descendant_classes.size() != 1u) {
      return AssertionFailure() << descendant_classes.size() << " should be 1";
    }
    if (!descendant_classes.Contains(AtomicString(descendant_name))) {
      return AssertionFailure()
             << "should descendant invalidate " << descendant_name;
    }
    return AssertionSuccess();
  }

  AssertionResult HasSiblingNoDescendantInvalidation(
      InvalidationSetVector& invalidation_sets) {
    if (invalidation_sets.size() != 1u) {
      return AssertionFailure() << "has " << invalidation_sets.size()
                                << " invalidation set(s), should have 1";
    }
    const auto& sibling_invalidation_set =
        To<SiblingInvalidationSet>(*invalidation_sets[0]);
    if (sibling_invalidation_set.SiblingDescendants()) {
      return AssertionFailure() << "should have no descendants";
    }
    return AssertionSuccess();
  }

  AssertionResult HasSiblingWholeSubtreeInvalidation(
      InvalidationSetVector& invalidation_sets) {
    if (invalidation_sets.size() != 1u) {
      return AssertionFailure() << "has " << invalidation_sets.size()
                                << " invalidation set(s), should have 1";
    }
    const auto& sibling_invalidation_set =
        To<SiblingInvalidationSet>(*invalidation_sets[0]);
    if (!sibling_invalidation_set.SiblingDescendants()) {
      return AssertionFailure() << "should have a descendant set";
    };
    if (!sibling_invalidation_set.SiblingDescendants()->WholeSubtreeInvalid()) {
      return AssertionFailure()
             << "sibling descendants should invalidate whole subtree";
    }
    return AssertionSuccess();
  }

  AssertionResult HasIdInvalidation(const char* id,
                                    InvalidationSetVector& invalidation_sets) {
    if (invalidation_sets.size() != 1u) {
      return AssertionFailure() << "has " << invalidation_sets.size()
                                << " invalidation set(s), should have 1";
    }
    HashSet<AtomicString> ids = IdSet(*invalidation_sets[0]);
    if (ids.size() != 1u) {
      return AssertionFailure() << ids.size() << " should be 1";
    }
    if (!ids.Contains(AtomicString(id))) {
      return AssertionFailure() << "should invalidate id " << id;
    }
    return AssertionSuccess();
  }

  AssertionResult HasIdInvalidation(const char* first_id,
                                    const char* second_id,
                                    InvalidationSetVector& invalidation_sets) {
    if (invalidation_sets.size() != 1u) {
      return AssertionFailure() << "has " << invalidation_sets.size()
                                << " invalidation set(s), should have 1";
    }
    HashSet<AtomicString> ids = IdSet(*invalidation_sets[0]);
    if (ids.size() != 2u) {
      return AssertionFailure() << ids.size() << " should be 2";
    }
    if (!ids.Contains(AtomicString(first_id))) {
      return AssertionFailure() << "should invalidate id " << first_id;
    }
    if (!ids.Contains(AtomicString(second_id))) {
      return AssertionFailure() << "should invalidate id " << second_id;
    }
    return AssertionSuccess();
  }

  AssertionResult HasTagNameInvalidation(
      const char* tag_name,
      InvalidationSetVector& invalidation_sets) {
    if (invalidation_sets.size() != 1u) {
      return AssertionFailure() << "has " << invalidation_sets.size()
                                << " invalidation set(s), should have 1";
    }
    HashSet<AtomicString> tag_names = TagNameSet(*invalidation_sets[0]);
    if (tag_names.size() != 1u) {
      return AssertionFailure() << tag_names.size() << " should be 1";
    }
    if (!tag_names.Contains(AtomicString(tag_name))) {
      return AssertionFailure() << "should invalidate tag " << tag_name;
    }
    return AssertionSuccess();
  }

  AssertionResult HasTagNameInvalidation(
      const char* first_tag_name,
      const char* second_tag_name,
      InvalidationSetVector& invalidation_sets) {
    if (invalidation_sets.size() != 1u) {
      return AssertionFailure() << "has " << invalidation_sets.size()
                                << " invalidation set(s), should have 1";
    }
    HashSet<AtomicString> tag_names = TagNameSet(*invalidation_sets[0]);
    if (tag_names.size() != 2u) {
      return AssertionFailure() << tag_names.size() << " should be 2";
    }
    if (!tag_names.Contains(AtomicString(first_tag_name))) {
      return AssertionFailure() << "should invalidate tag " << first_tag_name;
    }
    if (!tag_names.Contains(AtomicString(second_tag_name))) {
      return AssertionFailure() << "should invalidate tag " << second_tag_name;
    }
    return AssertionSuccess();
  }

  AssertionResult HasAttributeInvalidation(
      const char* attribute,
      InvalidationSetVector& invalidation_sets) {
    if (invalidation_sets.size() != 1u) {
      return AssertionFailure() << "has " << invalidation_sets.size()
                                << " invalidation set(s), should have 1";
    }
    HashSet<AtomicString> attributes = AttributeSet(*invalidation_sets[0]);
    if (attributes.size() != 1u) {
      return AssertionFailure() << attributes.size() << " should be 1";
    }
    if (!attributes.Contains(AtomicString(attribute))) {
      return AssertionFailure() << "should invalidate attribute " << attribute;
    }
    return AssertionSuccess();
  }

  AssertionResult HasPartsInvalidation(
      InvalidationSetVector& invalidation_sets) {
    if (invalidation_sets.size() != 1u) {
      return AssertionFailure() << "has " << invalidation_sets.size()
                                << " invalidation set(s), should have 1";
    }
    if (!invalidation_sets[0]->InvalidatesParts()) {
      return AssertionFailure() << "should invalidate parts";
    }
    return AssertionSuccess();
  }

  enum class RefCount { kOne, kMany };

  template <typename MapType, typename KeyType>
  AssertionResult HasRefCountForInvalidationSet(const MapType& map,
                                                const KeyType& key,
                                                RefCount ref_count) {
    auto it = map.find(key);
    if (map.end() == it) {
      return AssertionFailure() << "Could not find " << key;
    }

    if (ref_count == RefCount::kOne) {
      if (!it->value->HasOneRef()) {
        return AssertionFailure() << "should have a single ref";
      }

      // For SiblingInvalidationSets, we also require that the inner
      // InvalidationSets either don't exist, or have a refcount of 1.
      if (it->value->IsSiblingInvalidationSet()) {
        const auto& sibling_invalidation_set =
            To<SiblingInvalidationSet>(*it->value);
        bool sibling_descendants_has_one_ref =
            !sibling_invalidation_set.SiblingDescendants() ||
            sibling_invalidation_set.SiblingDescendants()->HasOneRef();
        bool descendants_has_one_ref =
            !sibling_invalidation_set.Descendants() ||
            sibling_invalidation_set.Descendants()->HasOneRef();
        if (!sibling_descendants_has_one_ref) {
          return AssertionFailure()
                 << "sibling descendants should have a single ref";
        }
        if (!descendants_has_one_ref) {
          return AssertionFailure() << "descendants should have a single ref";
        }
      }
    } else {
      if (it->value->HasOneRef()) {
        return AssertionFailure() << "should be shared";
      }
    }
    return AssertionSuccess();
  }

  AssertionResult HasRefCountForClassInvalidationSet(
      const RuleFeatureSet& rule_feature_set,
      const char* class_name,
      RefCount ref_count) {
    return HasRefCountForInvalidationSet(
        rule_feature_set.GetRuleInvalidationData().class_invalidation_sets,
        AtomicString(class_name), ref_count);
  }

  AssertionResult HasRefCountForAttributeInvalidationSet(
      const RuleFeatureSet& rule_feature_set,
      const char* attribute,
      RefCount ref_count) {
    return HasRefCountForInvalidationSet(
        rule_feature_set.GetRuleInvalidationData().attribute_invalidation_sets,
        AtomicString(attribute), ref_count);
  }

  AssertionResult HasRefCountForIdInvalidationSet(
      const RuleFeatureSet& rule_feature_set,
      const char* id,
      RefCount ref_count) {
    return HasRefCountForInvalidationSet(
        rule_feature_set.GetRuleInvalidationData().id_invalidation_sets,
        AtomicString(id), ref_count);
  }

  AssertionResult HasRefCountForPseudoInvalidationSet(
      const RuleFeatureSet& rule_feature_set,
      CSSSelector::PseudoType key,
      RefCount ref_count) {
    return HasRefCountForInvalidationSet(
        rule_feature_set.GetRuleInvalidationData().pseudo_invalidation_sets,
        key, ref_count);
  }

 protected:
  test::TaskEnvironment task_environment_;
  ScopedNullExecutionContext execution_context_;

 private:
  RuleFeatureSet rule_feature_set_;
  Persistent<Document> document_;
};

TEST_F(RuleFeatureSetTest, interleavedDescendantSibling1) {
  EXPECT_EQ(SelectorPreMatch::kMayMatch, CollectFeatures(".p"));

  InvalidationLists invalidation_lists;
  CollectInvalidationSetsForClass(invalidation_lists, "p");
  EXPECT_TRUE(HasSelfInvalidation(invalidation_lists.descendants));
  EXPECT_TRUE(HasNoInvalidation(invalidation_lists.siblings));
}

TEST_F(RuleFeatureSetTest, interleavedDescendantSibling2) {
  EXPECT_EQ(SelectorPreMatch::kMayMatch, CollectFeatures(".o + .p"));

  InvalidationLists invalidation_lists;
  CollectInvalidationSetsForClass(invalidation_lists, "o");
  EXPECT_TRUE(HasNoInvalidation(invalidation_lists.descendants));
  EXPECT_TRUE(HasSiblingClassInvalidation(1, "p", invalidation_lists.siblings));
}

TEST_F(RuleFeatureSetTest, interleavedDescendantSibling3) {
  EXPECT_EQ(SelectorPreMatch::kMayMatch, CollectFeatures(".m + .n .o + .p"));

  InvalidationLists invalidation_lists;
  CollectInvalidationSetsForClass(invalidation_lists, "n");
  EXPECT_TRUE(HasNoSelfInvalidation(invalidation_lists.descendants));
  EXPECT_TRUE(HasClassInvalidation("p", invalidation_lists.descendants));
  EXPECT_TRUE(HasNoInvalidation(invalidation_lists.siblings));
}

TEST_F(RuleFeatureSetTest, interleavedDescendantSibling4) {
  EXPECT_EQ(SelectorPreMatch::kMayMatch, CollectFeatures(".m + .n .o + .p"));

  InvalidationLists invalidation_lists;
  CollectInvalidationSetsForClass(invalidation_lists, "m");
  EXPECT_TRUE(HasNoInvalidation(invalidation_lists.descendants));
  EXPECT_TRUE(HasSiblingDescendantInvalidation(1, "n", "p",
                                               invalidation_lists.siblings));
}

TEST_F(RuleFeatureSetTest, interleavedDescendantSibling5) {
  EXPECT_EQ(SelectorPreMatch::kMayMatch,
            CollectFeatures(".l ~ .m + .n .o + .p"));

  InvalidationLists invalidation_lists;
  CollectInvalidationSetsForClass(invalidation_lists, "l");
  EXPECT_TRUE(HasNoInvalidation(invalidation_lists.descendants));
  EXPECT_TRUE(HasSiblingDescendantInvalidation(
      SiblingInvalidationSet::kDirectAdjacentMax, "n", "p",
      invalidation_lists.siblings));
}

TEST_F(RuleFeatureSetTest, interleavedDescendantSibling6) {
  EXPECT_EQ(SelectorPreMatch::kMayMatch,
            CollectFeatures(".k > .l ~ .m + .n .o + .p"));

  InvalidationLists invalidation_lists;
  CollectInvalidationSetsForClass(invalidation_lists, "k");
  EXPECT_TRUE(HasClassInvalidation("p", invalidation_lists.descendants));
  EXPECT_TRUE(HasNoInvalidation(invalidation_lists.siblings));
}

TEST_F(RuleFeatureSetTest, anySibling) {
  EXPECT_EQ(SelectorPreMatch::kMayMatch,
            CollectFeatures(":-webkit-any(.q, .r) ~ .s .t"));

  InvalidationLists invalidation_lists;
  CollectInvalidationSetsForClass(invalidation_lists, "q");
  EXPECT_TRUE(HasNoInvalidation(invalidation_lists.descendants));
  EXPECT_TRUE(HasSiblingDescendantInvalidation(
      SiblingInvalidationSet::kDirectAdjacentMax, "s", "t",
      invalidation_lists.siblings));
}

TEST_F(RuleFeatureSetTest, any) {
  EXPECT_EQ(SelectorPreMatch::kMayMatch,
            CollectFeatures(":-webkit-any(.w, .x)"));

  InvalidationLists invalidation_lists;
  CollectInvalidationSetsForClass(invalidation_lists, "w");
  EXPECT_TRUE(HasSelfInvalidation(invalidation_lists.descendants));
  EXPECT_TRUE(HasNoInvalidation(invalidation_lists.siblings));
}

TEST_F(RuleFeatureSetTest, repeatedAny) {
  EXPECT_EQ(SelectorPreMatch::kMayMatch,
            CollectFeatures(":-webkit-any(.v, .w):-webkit-any(.x, .y, .z)"));

  {
    InvalidationLists invalidation_lists;
    CollectInvalidationSetsForClass(invalidation_lists, "v");
    EXPECT_TRUE(HasSelfInvalidation(invalidation_lists.descendants));
    EXPECT_TRUE(HasNoInvalidation(invalidation_lists.siblings));
  }

  {
    InvalidationLists invalidation_lists;
    CollectInvalidationSetsForClass(invalidation_lists, "x");
    EXPECT_TRUE(HasSelfInvalidation(invalidation_lists.descendants));
    EXPECT_TRUE(HasNoInvalidation(invalidation_lists.siblings));
  }
}

TEST_F(RuleFeatureSetTest, anyIdDescendant) {
  EXPECT_EQ(SelectorPreMatch::kMayMatch,
            CollectFeatures(".a :-webkit-any(#b, #c)"));

  InvalidationLists invalidation_lists;
  CollectInvalidationSetsForClass(invalidation_lists, "a");
  EXPECT_TRUE(HasIdInvalidation("b", "c", invalidation_lists.descendants));
}

TEST_F(RuleFeatureSetTest, repeatedAnyDescendant) {
  EXPECT_EQ(SelectorPreMatch::kMayMatch,
            CollectFeatures(".a :-webkit-any(.v, .w):-webkit-any(.x, .y, .z)"));

  InvalidationLists invalidation_lists;
  CollectInvalidationSetsForClass(invalidation_lists, "a");
  EXPECT_TRUE(HasClassInvalidation("v", "w", invalidation_lists.descendants));
}

TEST_F(RuleFeatureSetTest, anyTagDescendant) {
  EXPECT_EQ(SelectorPreMatch::kMayMatch,
            CollectFeatures(".a :-webkit-any(span, div)"));

  InvalidationLists invalidation_lists;
  CollectInvalidationSetsForClass(invalidation_lists, "a");
  EXPECT_TRUE(
      HasTagNameInvalidation("span", "div", invalidation_lists.descendants));
}

TEST_F(RuleFeatureSetTest, siblingAny) {
  EXPECT_EQ(SelectorPreMatch::kMayMatch,
            CollectFeatures(".v ~ :-webkit-any(.w, .x)"));

  InvalidationLists invalidation_lists;
  CollectInvalidationSetsForClass(invalidation_lists, "v");
  EXPECT_TRUE(HasNoInvalidation(invalidation_lists.descendants));
  EXPECT_TRUE(HasClassInvalidation("w", "x", invalidation_lists.siblings));
}

TEST_F(RuleFeatureSetTest, descendantSiblingAny) {
  EXPECT_EQ(SelectorPreMatch::kMayMatch,
            CollectFeatures(".u .v ~ :-webkit-any(.w, .x)"));

  InvalidationLists invalidation_lists;
  CollectInvalidationSetsForClass(invalidation_lists, "u");
  EXPECT_TRUE(HasClassInvalidation("w", "x", invalidation_lists.descendants));
  EXPECT_TRUE(HasNoInvalidation(invalidation_lists.siblings));
}

TEST_F(RuleFeatureSetTest, id) {
  EXPECT_EQ(SelectorPreMatch::kMayMatch, CollectFeatures("#a #b"));

  InvalidationLists invalidation_lists;
  CollectInvalidationSetsForId(invalidation_lists, "a");
  EXPECT_TRUE(HasIdInvalidation("b", invalidation_lists.descendants));
}

TEST_F(RuleFeatureSetTest, attribute) {
  EXPECT_EQ(SelectorPreMatch::kMayMatch, CollectFeatures("[c] [d]"));

  InvalidationLists invalidation_lists;
  CollectInvalidationSetsForAttribute(
      invalidation_lists,
      QualifiedName(g_empty_atom, AtomicString("c"), g_empty_atom));
  EXPECT_TRUE(HasAttributeInvalidation("d", invalidation_lists.descendants));
}

TEST_F(RuleFeatureSetTest, pseudoClass) {
  EXPECT_EQ(SelectorPreMatch::kMayMatch, CollectFeatures(":valid"));

  InvalidationLists invalidation_lists;
  CollectInvalidationSetsForPseudoClass(invalidation_lists,
                                        CSSSelector::kPseudoValid);
  EXPECT_TRUE(HasSelfInvalidation(invalidation_lists.descendants));
}

TEST_F(RuleFeatureSetTest, tagName) {
  EXPECT_EQ(SelectorPreMatch::kMayMatch, CollectFeatures(":valid e"));

  InvalidationLists invalidation_lists;
  CollectInvalidationSetsForPseudoClass(invalidation_lists,
                                        CSSSelector::kPseudoValid);
  EXPECT_TRUE(HasTagNameInvalidation("e", invalidation_lists.descendants));
}

TEST_F(RuleFeatureSetTest, nonMatchingHost) {
  EXPECT_EQ(SelectorPreMatch::kNeverMatches, CollectFeatures(".a:host"));
  EXPECT_EQ(SelectorPreMatch::kNeverMatches, CollectFeatures("*:host(.a)"));
  EXPECT_EQ(SelectorPreMatch::kNeverMatches, CollectFeatures("*:host .a"));
  EXPECT_EQ(SelectorPreMatch::kNeverMatches, CollectFeatures("div :host .a"));
  EXPECT_EQ(SelectorPreMatch::kNeverMatches, CollectFeatures(":host:hover .a"));
  EXPECT_EQ(SelectorPreMatch::kNeverMatches,
            CollectFeatures(":host:has(.b):hover .a"));

  InvalidationLists invalidation_lists;
  CollectInvalidationSetsForClass(invalidation_lists, "a");
  EXPECT_TRUE(HasNoInvalidation(invalidation_lists.descendants));
}

TEST_F(RuleFeatureSetTest, nonMatchingHostContext) {
  EXPECT_EQ(SelectorPreMatch::kNeverMatches,
            CollectFeatures(".a:host-context(*)"));
  EXPECT_EQ(SelectorPreMatch::kNeverMatches,
            CollectFeatures("*:host-context(.a)"));
  EXPECT_EQ(SelectorPreMatch::kNeverMatches,
            CollectFeatures("*:host-context(*) .a"));
  EXPECT_EQ(SelectorPreMatch::kNeverMatches,
            CollectFeatures("div :host-context(div) .a"));
  EXPECT_EQ(SelectorPreMatch::kNeverMatches,
            CollectFeatures(":host-context(div):hover .a"));
  EXPECT_EQ(SelectorPreMatch::kNeverMatches,
            CollectFeatures(":host-context(div):has(.b):hover .a"));

  InvalidationLists invalidation_lists;
  CollectInvalidationSetsForClass(invalidation_lists, "a");
  EXPECT_TRUE(HasNoInvalidation(invalidation_lists.descendants));
}

TEST_F(RuleFeatureSetTest, emptyIsWhere) {
  EXPECT_EQ(SelectorPreMatch::kNeverMatches, CollectFeatures(":is()"));
  EXPECT_EQ(SelectorPreMatch::kNeverMatches, CollectFeatures(":where()"));

  // We do not support :nonsense, so :is()/:where() end up empty.
  // https://drafts.csswg.org/selectors/#typedef-forgiving-selector-list
  EXPECT_EQ(SelectorPreMatch::kNeverMatches, CollectFeatures(":is(:nonsense)"));
  EXPECT_EQ(SelectorPreMatch::kNeverMatches,
            CollectFeatures(":where(:nonsense)"));
  EXPECT_EQ(SelectorPreMatch::kNeverMatches,
            CollectFeatures(".a:is(:nonsense)"));
  EXPECT_EQ(SelectorPreMatch::kNeverMatches,
            CollectFeatures(".b:where(:nonsense)"));
}

TEST_F(RuleFeatureSetTest, universalSiblingInvalidationDirectAdjacent) {
  EXPECT_EQ(SelectorPreMatch::kMayMatch, CollectFeatures("* + .a"));

  InvalidationLists invalidation_lists;
  CollectUniversalSiblingInvalidationSet(invalidation_lists);

  EXPECT_TRUE(HasSiblingClassInvalidation(1, "a", invalidation_lists.siblings));
  EXPECT_TRUE(HasSelfInvalidation(invalidation_lists.siblings));
}

TEST_F(RuleFeatureSetTest, universalSiblingInvalidationMultipleDirectAdjacent) {
  EXPECT_EQ(SelectorPreMatch::kMayMatch, CollectFeatures("* + .a + .b"));

  InvalidationLists invalidation_lists;
  CollectUniversalSiblingInvalidationSet(invalidation_lists);

  EXPECT_TRUE(HasSiblingClassInvalidation(2, "b", invalidation_lists.siblings));
  EXPECT_TRUE(HasSelfInvalidation(invalidation_lists.siblings));
}

TEST_F(RuleFeatureSetTest,
       universalSiblingInvalidationDirectAdjacentDescendant) {
  EXPECT_EQ(SelectorPreMatch::kMayMatch, CollectFeatures("* + .a .b"));

  InvalidationLists invalidation_lists;
  CollectUniversalSiblingInvalidationSet(invalidation_lists);

  EXPECT_TRUE(HasSiblingDescendantInvalidation(1, "a", "b",
                                               invalidation_lists.siblings));
  EXPECT_TRUE(HasNoSelfInvalidation(invalidation_lists.siblings));
}

TEST_F(RuleFeatureSetTest, universalSiblingInvalidationIndirectAdjacent) {
  EXPECT_EQ(SelectorPreMatch::kMayMatch, CollectFeatures("* ~ .a"));

  InvalidationLists invalidation_lists;
  CollectUniversalSiblingInvalidationSet(invalidation_lists);

  EXPECT_TRUE(
      HasSiblingClassInvalidation(SiblingInvalidationSet::kDirectAdjacentMax,
                                  "a", invalidation_lists.siblings));
  EXPECT_TRUE(HasSelfInvalidation(invalidation_lists.siblings));
}

TEST_F(RuleFeatureSetTest,
       universalSiblingInvalidationMultipleIndirectAdjacent) {
  EXPECT_EQ(SelectorPreMatch::kMayMatch, CollectFeatures("* ~ .a ~ .b"));

  InvalidationLists invalidation_lists;
  CollectUniversalSiblingInvalidationSet(invalidation_lists);

  EXPECT_TRUE(
      HasSiblingClassInvalidation(SiblingInvalidationSet::kDirectAdjacentMax,
                                  "b", invalidation_lists.siblings));
  EXPECT_TRUE(HasSelfInvalidation(invalidation_lists.siblings));
}

TEST_F(RuleFeatureSetTest,
       universalSiblingInvalidationIndirectAdjacentDescendant) {
  EXPECT_EQ(SelectorPreMatch::kMayMatch, CollectFeatures("* ~ .a .b"));

  InvalidationLists invalidation_lists;
  CollectUniversalSiblingInvalidationSet(invalidation_lists);

  EXPECT_TRUE(HasSiblingDescendantInvalidation(
      SiblingInvalidationSet::kDirectAdjacentMax, "a", "b",
      invalidation_lists.siblings));
  EXPECT_TRUE(HasNoSelfInvalidation(invalidation_lists.siblings));
}

TEST_F(RuleFeatureSetTest, universalSiblingInvalidationNot) {
  EXPECT_EQ(SelectorPreMatch::kMayMatch, CollectFeatures(":not(.a) + .b"));

  InvalidationLists invalidation_lists;
  CollectUniversalSiblingInvalidationSet(invalidation_lists);

  EXPECT_TRUE(HasSiblingClassInvalidation(1, "b", invalidation_lists.siblings));
  EXPECT_TRUE(HasSelfInvalidation(invalidation_lists.siblings));
}

TEST_F(RuleFeatureSetTest, nonUniversalSiblingInvalidationNot) {
  EXPECT_EQ(SelectorPreMatch::kMayMatch, CollectFeatures("#x:not(.a) + .b"));

  InvalidationLists invalidation_lists;
  CollectUniversalSiblingInvalidationSet(invalidation_lists);

  EXPECT_TRUE(HasNoInvalidation(invalidation_lists.siblings));
}

TEST_F(RuleFeatureSetTest, nonUniversalSiblingInvalidationAny) {
  EXPECT_EQ(SelectorPreMatch::kMayMatch,
            CollectFeatures("#x:-webkit-any(.a) + .b"));

  InvalidationLists invalidation_lists;
  CollectUniversalSiblingInvalidationSet(invalidation_lists);

  EXPECT_TRUE(HasNoInvalidation(invalidation_lists.siblings));
}

TEST_F(RuleFeatureSetTest, universalSiblingInvalidationType) {
  EXPECT_EQ(SelectorPreMatch::kMayMatch, CollectFeatures("div + .a"));

  InvalidationLists invalidation_lists;
  CollectUniversalSiblingInvalidationSet(invalidation_lists);

  EXPECT_TRUE(HasSiblingClassInvalidation(1, "a", invalidation_lists.siblings));
  EXPECT_TRUE(HasSelfInvalidation(invalidation_lists.siblings));
}

TEST_F(RuleFeatureSetTest, nonUniversalSiblingInvalidationType) {
  EXPECT_EQ(SelectorPreMatch::kMayMatch, CollectFeatures("div#x + .a"));

  InvalidationLists invalidation_lists;
  CollectUniversalSiblingInvalidationSet(invalidation_lists);

  EXPECT_TRUE(HasNoInvalidation(invalidation_lists.siblings));
}

TEST_F(RuleFeatureSetTest, universalSiblingInvalidationLink) {
  EXPECT_EQ(SelectorPreMatch::kMayMatch, CollectFeatures(":link + .a"));

  InvalidationLists invalidation_lists;
  CollectUniversalSiblingInvalidationSet(invalidation_lists);

  EXPECT_TRUE(HasSiblingClassInvalidation(1, "a", invalidation_lists.siblings));
  EXPECT_TRUE(HasSelfInvalidation(invalidation_lists.siblings));
}

TEST_F(RuleFeatureSetTest, nonUniversalSiblingInvalidationLink) {
  EXPECT_EQ(SelectorPreMatch::kMayMatch, CollectFeatures("#x:link + .a"));

  InvalidationLists invalidation_lists;
  CollectUniversalSiblingInvalidationSet(invalidation_lists);

  EXPECT_TRUE(HasNoInvalidation(invalidation_lists.siblings));
}

TEST_F(RuleFeatureSetTest, nthInvalidationUniversal) {
  EXPECT_EQ(SelectorPreMatch::kMayMatch, CollectFeatures(":nth-child(2n)"));

  InvalidationLists invalidation_lists;
  CollectNthInvalidationSet(invalidation_lists);

  EXPECT_TRUE(HasNoInvalidation(invalidation_lists.descendants));
  EXPECT_TRUE(HasSelfInvalidation(invalidation_lists.siblings));
  EXPECT_TRUE(HasWholeSubtreeInvalidation(invalidation_lists.siblings));
  EXPECT_TRUE(HasSiblingNoDescendantInvalidation(invalidation_lists.siblings));
}

TEST_F(RuleFeatureSetTest, nthInvalidationClass) {
  EXPECT_EQ(SelectorPreMatch::kMayMatch, CollectFeatures(".a:nth-child(2n)"));

  InvalidationLists invalidation_lists;
  CollectNthInvalidationSet(invalidation_lists);

  EXPECT_TRUE(HasNoInvalidation(invalidation_lists.descendants));
  EXPECT_TRUE(HasSelfInvalidation(invalidation_lists.siblings));
  EXPECT_TRUE(
      HasSiblingClassInvalidation(SiblingInvalidationSet::kDirectAdjacentMax,
                                  "a", invalidation_lists.siblings));
  EXPECT_TRUE(HasSiblingNoDescendantInvalidation(invalidation_lists.siblings));
}

TEST_F(RuleFeatureSetTest, nthInvalidationUniversalDescendant) {
  EXPECT_EQ(SelectorPreMatch::kMayMatch, CollectFeatures(":nth-child(2n) *"));

  InvalidationLists invalidation_lists;
  CollectNthInvalidationSet(invalidation_lists);

  EXPECT_TRUE(HasNoInvalidation(invalidation_lists.descendants));
  EXPECT_TRUE(HasNoSelfInvalidation(invalidation_lists.siblings));
  EXPECT_TRUE(HasWholeSubtreeInvalidation(invalidation_lists.siblings));
  EXPECT_TRUE(HasSiblingWholeSubtreeInvalidation(invalidation_lists.siblings));
}

TEST_F(RuleFeatureSetTest, nthInvalidationDescendant) {
  EXPECT_EQ(SelectorPreMatch::kMayMatch, CollectFeatures(":nth-child(2n) .a"));

  InvalidationLists invalidation_lists;
  CollectNthInvalidationSet(invalidation_lists);

  EXPECT_TRUE(HasNoInvalidation(invalidation_lists.descendants));
  EXPECT_TRUE(HasNoSelfInvalidation(invalidation_lists.siblings));
  EXPECT_TRUE(HasWholeSubtreeInvalidation(invalidation_lists.siblings));
  EXPECT_TRUE(HasSiblingDescendantInvalidation(
      SiblingInvalidationSet::kDirectAdjacentMax, "a",
      invalidation_lists.siblings));
}

TEST_F(RuleFeatureSetTest, nthInvalidationSibling) {
  EXPECT_EQ(SelectorPreMatch::kMayMatch,
            CollectFeatures(":nth-child(2n) + .a"));

  InvalidationLists invalidation_lists;
  CollectNthInvalidationSet(invalidation_lists);

  EXPECT_TRUE(HasNoInvalidation(invalidation_lists.descendants));
  EXPECT_TRUE(HasSelfInvalidation(invalidation_lists.siblings));
  EXPECT_TRUE(HasClassInvalidation("a", invalidation_lists.siblings));
}

TEST_F(RuleFeatureSetTest, nthInvalidationSiblingDescendant) {
  EXPECT_EQ(SelectorPreMatch::kMayMatch,
            CollectFeatures(":nth-child(2n) + .a .b"));

  InvalidationLists invalidation_lists;
  CollectNthInvalidationSet(invalidation_lists);

  EXPECT_TRUE(HasNoInvalidation(invalidation_lists.descendants));
  EXPECT_TRUE(HasNoSelfInvalidation(invalidation_lists.siblings));
  EXPECT_TRUE(HasSiblingDescendantInvalidation(
      SiblingInvalidationSet::kDirectAdjacentMax, "a", "b",
      invalidation_lists.siblings));
}

TEST_F(RuleFeatureSetTest, nthInvalidationNot) {
  EXPECT_EQ(SelectorPreMatch::kMayMatch,
            CollectFeatures(":not(:nth-child(2n))"));

  InvalidationLists invalidation_lists;
  CollectNthInvalidationSet(invalidation_lists);

  EXPECT_TRUE(HasNoInvalidation(invalidation_lists.descendants));
  EXPECT_TRUE(HasSelfInvalidation(invalidation_lists.siblings));
  EXPECT_TRUE(HasWholeSubtreeInvalidation(invalidation_lists.siblings));
}

TEST_F(RuleFeatureSetTest, nthInvalidationNotClass) {
  EXPECT_EQ(SelectorPreMatch::kMayMatch,
            CollectFeatures(".a:not(:nth-child(2n))"));

  InvalidationLists invalidation_lists;
  CollectNthInvalidationSet(invalidation_lists);

  EXPECT_TRUE(HasNoInvalidation(invalidation_lists.descendants));
  EXPECT_TRUE(HasSelfInvalidation(invalidation_lists.siblings));
  EXPECT_TRUE(
      HasSiblingClassInvalidation(SiblingInvalidationSet::kDirectAdjacentMax,
                                  "a", invalidation_lists.siblings));
}

TEST_F(RuleFeatureSetTest, nthInvalidationNotDescendant) {
  EXPECT_EQ(SelectorPreMatch::kMayMatch,
            CollectFeatures(".blah:not(:nth-child(2n)) .a"));

  InvalidationLists invalidation_lists;
  CollectNthInvalidationSet(invalidation_lists);

  EXPECT_TRUE(HasNoInvalidation(invalidation_lists.descendants));
  EXPECT_TRUE(HasNoSelfInvalidation(invalidation_lists.siblings));
  EXPECT_TRUE(HasWholeSubtreeInvalidation(invalidation_lists.siblings));
  EXPECT_TRUE(HasSiblingDescendantInvalidation(
      SiblingInvalidationSet::kDirectAdjacentMax, "a",
      invalidation_lists.siblings));
}

TEST_F(RuleFeatureSetTest, nthInvalidationAny) {
  EXPECT_EQ(SelectorPreMatch::kMayMatch,
            CollectFeatures(":-webkit-any(#nomatch, :nth-child(2n))"));

  InvalidationLists invalidation_lists;
  CollectNthInvalidationSet(invalidation_lists);

  EXPECT_TRUE(HasNoInvalidation(invalidation_lists.descendants));
  EXPECT_TRUE(HasSelfInvalidation(invalidation_lists.siblings));
  EXPECT_TRUE(HasWholeSubtreeInvalidation(invalidation_lists.siblings));
  EXPECT_TRUE(HasSiblingNoDescendantInvalidation(invalidation_lists.siblings));
}

TEST_F(RuleFeatureSetTest, nthInvalidationAnyClass) {
  EXPECT_EQ(SelectorPreMatch::kMayMatch,
            CollectFeatures(".a:-webkit-any(#nomatch, :nth-child(2n))"));

  InvalidationLists invalidation_lists;
  CollectNthInvalidationSet(invalidation_lists);

  EXPECT_TRUE(HasNoInvalidation(invalidation_lists.descendants));
  EXPECT_TRUE(HasSelfInvalidation(invalidation_lists.siblings));
  EXPECT_TRUE(HasClassInvalidation("a", invalidation_lists.siblings));
}

TEST_F(RuleFeatureSetTest, nthInvalidationAnyDescendant) {
  EXPECT_EQ(SelectorPreMatch::kMayMatch,
            CollectFeatures(".blah:-webkit-any(#nomatch, :nth-child(2n)) .a"));

  InvalidationLists invalidation_lists;
  CollectNthInvalidationSet(invalidation_lists);

  EXPECT_TRUE(HasNoInvalidation(invalidation_lists.descendants));
  EXPECT_TRUE(HasNoSelfInvalidation(invalidation_lists.siblings));
  EXPECT_TRUE(HasSiblingDescendantInvalidation(
      SiblingInvalidationSet::kDirectAdjacentMax, "a",
      invalidation_lists.siblings));
}

TEST_F(RuleFeatureSetTest, SelfInvalidationSet) {
  EXPECT_EQ(SelectorPreMatch::kMayMatch, CollectFeatures(".a"));
  EXPECT_EQ(SelectorPreMatch::kMayMatch, CollectFeatures("div .b"));
  EXPECT_EQ(SelectorPreMatch::kMayMatch, CollectFeatures("#c"));
  EXPECT_EQ(SelectorPreMatch::kMayMatch, CollectFeatures("[d]"));
  EXPECT_EQ(SelectorPreMatch::kMayMatch, CollectFeatures(":hover"));

  InvalidationLists invalidation_lists;
  CollectInvalidationSetsForClass(invalidation_lists, "a");
  EXPECT_TRUE(HasSelfInvalidation(invalidation_lists.descendants));
  EXPECT_TRUE(HasSelfInvalidationSet(invalidation_lists.descendants));

  invalidation_lists.descendants.clear();
  CollectInvalidationSetsForClass(invalidation_lists, "b");
  EXPECT_TRUE(HasSelfInvalidation(invalidation_lists.descendants));
  EXPECT_TRUE(HasSelfInvalidationSet(invalidation_lists.descendants));

  invalidation_lists.descendants.clear();
  CollectInvalidationSetsForId(invalidation_lists, "c");
  EXPECT_TRUE(HasSelfInvalidation(invalidation_lists.descendants));
  EXPECT_TRUE(HasSelfInvalidationSet(invalidation_lists.descendants));

  invalidation_lists.descendants.clear();
  CollectInvalidationSetsForAttribute(
      invalidation_lists,
      QualifiedName(g_empty_atom, AtomicString("d"), g_empty_atom));
  EXPECT_TRUE(HasSelfInvalidation(invalidation_lists.descendants));
  EXPECT_TRUE(HasSelfInvalidationSet(invalidation_lists.descendants));

  invalidation_lists.descendants.clear();
  CollectInvalidationSetsForPseudoClass(invalidation_lists,
                                        CSSSelector::kPseudoHover);
  EXPECT_TRUE(HasSelfInvalidation(invalidation_lists.descendants));
  EXPECT_TRUE(HasSelfInvalidationSet(invalidation_lists.descendants));
}

TEST_F(RuleFeatureSetTest, ReplaceSelfInvalidationSet) {
  EXPECT_EQ(SelectorPreMatch::kMayMatch, CollectFeatures(".a"));

  InvalidationLists invalidation_lists;
  CollectInvalidationSetsForClass(invalidation_lists, "a");
  EXPECT_TRUE(HasSelfInvalidation(invalidation_lists.descendants));
  EXPECT_TRUE(HasSelfInvalidationSet(invalidation_lists.descendants));

  EXPECT_EQ(SelectorPreMatch::kMayMatch, CollectFeatures(".a div"));

  invalidation_lists.descendants.clear();
  CollectInvalidationSetsForClass(invalidation_lists, "a");
  EXPECT_TRUE(HasSelfInvalidation(invalidation_lists.descendants));
  EXPECT_TRUE(HasNotSelfInvalidationSet(invalidation_lists.descendants));
}

TEST_F(RuleFeatureSetTest, pseudoIsSibling) {
  EXPECT_EQ(SelectorPreMatch::kMayMatch,
            CollectFeatures(":is(.q, .r) ~ .s .t"));
  {
    InvalidationLists invalidation_lists;
    CollectInvalidationSetsForClass(invalidation_lists, "q");
    EXPECT_TRUE(HasNoInvalidation(invalidation_lists.descendants));
    EXPECT_TRUE(HasSiblingDescendantInvalidation(
        SiblingInvalidationSet::kDirectAdjacentMax, "s", "t",
        invalidation_lists.siblings));
  }
  {
    InvalidationLists invalidation_lists;
    CollectInvalidationSetsForClass(invalidation_lists, "r");
    EXPECT_TRUE(HasNoInvalidation(invalidation_lists.descendants));
    EXPECT_TRUE(HasSiblingDescendantInvalidation(
        SiblingInvalidationSet::kDirectAdjacentMax, "s", "t",
        invalidation_lists.siblings));
  }
}

TEST_F(RuleFeatureSetTest, pseudoIs) {
  EXPECT_EQ(SelectorPreMatch::kMayMatch, CollectFeatures(":is(.w, .x)"));

  {
    InvalidationLists invalidation_lists;
    CollectInvalidationSetsForClass(invalidation_lists, "w");
    EXPECT_TRUE(HasSelfInvalidation(invalidation_lists.descendants));
    EXPECT_TRUE(HasNoInvalidation(invalidation_lists.siblings));
  }
  {
    InvalidationLists invalidation_lists;
    CollectInvalidationSetsForClass(invalidation_lists, "x");
    EXPECT_TRUE(HasSelfInvalidation(invalidation_lists.descendants));
    EXPECT_TRUE(HasNoInvalidation(invalidation_lists.siblings));
  }
}

TEST_F(RuleFeatureSetTest, pseudoIsIdDescendant) {
  EXPECT_EQ(SelectorPreMatch::kMayMatch, CollectFeatures(".a :is(#b, #c)"));

  InvalidationLists invalidation_lists;
  CollectInvalidationSetsForClass(invalidation_lists, "a");
  EXPECT_TRUE(HasIdInvalidation("b", "c", invalidation_lists.descendants));
}

TEST_F(RuleFeatureSetTest, pseudoIsTagDescendant) {
  EXPECT_EQ(SelectorPreMatch::kMayMatch, CollectFeatures(".a :is(span, div)"));

  InvalidationLists invalidation_lists;
  CollectInvalidationSetsForClass(invalidation_lists, "a");
  EXPECT_TRUE(
      HasTagNameInvalidation("span", "div", invalidation_lists.descendants));
}

TEST_F(RuleFeatureSetTest, pseudoIsAnySibling) {
  EXPECT_EQ(SelectorPreMatch::kMayMatch, CollectFeatures(".v ~ :is(.w, .x)"));

  InvalidationLists invalidation_lists;
  CollectInvalidationSetsForClass(invalidation_lists, "v");
  EXPECT_TRUE(HasNoInvalidation(invalidation_lists.descendants));
  EXPECT_TRUE(HasClassInvalidation("w", "x", invalidation_lists.siblings));
}

TEST_F(RuleFeatureSetTest, pseudoIsDescendantSibling) {
  EXPECT_EQ(SelectorPreMatch::kMayMatch,
            CollectFeatures(".u .v ~ :is(.w, .x)"));

  InvalidationLists invalidation_lists;
  CollectInvalidationSetsForClass(invalidation_lists, "u");
  EXPECT_TRUE(HasClassInvalidation("w", "x", invalidation_lists.descendants));
  EXPECT_TRUE(HasNoInvalidation(invalidation_lists.siblings));
}

TEST_F(RuleFeatureSetTest, pseudoIsWithComplexSelectors) {
  EXPECT_EQ(SelectorPreMatch::kMayMatch,
            CollectFeatures(".a :is(.w+.b, .x>#c)"));

  InvalidationLists invalidation_lists;
  CollectInvalidationSetsForClass(invalidation_lists, "a");
  EXPECT_TRUE(HasClassInvalidation("b", invalidation_lists.descendants));
  EXPECT_TRUE(HasIdInvalidation("c", invalidation_lists.descendants));
  EXPECT_TRUE(HasNoInvalidation(invalidation_lists.siblings));
}

TEST_F(RuleFeatureSetTest, pseudoIsNested) {
  EXPECT_EQ(SelectorPreMatch::kMayMatch,
            CollectFeatures(".a :is(.w+.b, .e+:is(.c, #d))"));

  InvalidationLists invalidation_lists;
  CollectInvalidationSetsForClass(invalidation_lists, "a");
  EXPECT_TRUE(HasClassInvalidation("b", "c", invalidation_lists.descendants));
  EXPECT_TRUE(HasIdInvalidation("d", invalidation_lists.descendants));
  EXPECT_TRUE(HasNoInvalidation(invalidation_lists.siblings));
}

TEST_F(RuleFeatureSetTest, pseudoWhere) {
  EXPECT_EQ(SelectorPreMatch::kMayMatch, CollectFeatures(":where(.w, .x)"));

  {
    InvalidationLists invalidation_lists;
    CollectInvalidationSetsForClass(invalidation_lists, "w");
    EXPECT_TRUE(HasSelfInvalidation(invalidation_lists.descendants));
    EXPECT_TRUE(HasNoInvalidation(invalidation_lists.siblings));
  }
  {
    InvalidationLists invalidation_lists;
    CollectInvalidationSetsForClass(invalidation_lists, "x");
    EXPECT_TRUE(HasSelfInvalidation(invalidation_lists.descendants));
    EXPECT_TRUE(HasNoInvalidation(invalidation_lists.siblings));
  }
}

TEST_F(RuleFeatureSetTest, pseudoWhereSibling) {
  EXPECT_EQ(SelectorPreMatch::kMayMatch,
            CollectFeatures(":where(.q, .r) ~ .s .t"));
  {
    InvalidationLists invalidation_lists;
    CollectInvalidationSetsForClass(invalidation_lists, "q");
    EXPECT_TRUE(HasNoInvalidation(invalidation_lists.descendants));
    EXPECT_TRUE(HasSiblingDescendantInvalidation(
        SiblingInvalidationSet::kDirectAdjacentMax, "s", "t",
        invalidation_lists.siblings));
  }
  {
    InvalidationLists invalidation_lists;
    CollectInvalidationSetsForClass(invalidation_lists, "r");
    EXPECT_TRUE(HasNoInvalidation(invalidation_lists.descendants));
    EXPECT_TRUE(HasSiblingDescendantInvalidation(
        SiblingInvalidationSet::kDirectAdjacentMax, "s", "t",
        invalidation_lists.siblings));
  }
}

TEST_F(RuleFeatureSetTest, pseudoWhereIdDescendant) {
  EXPECT_EQ(SelectorPreMatch::kMayMatch, CollectFeatures(".a :where(#b, #c)"));

  InvalidationLists invalidation_lists;
  CollectInvalidationSetsForClass(invalidation_lists, "a");
  EXPECT_TRUE(HasIdInvalidation("b", "c", invalidation_lists.descendants));
}

TEST_F(RuleFeatureSetTest, pseudoWhereTagDescendant) {
  EXPECT_EQ(SelectorPreMatch::kMayMatch,
            CollectFeatures(".a :where(span, div)"));

  InvalidationLists invalidation_lists;
  CollectInvalidationSetsForClass(invalidation_lists, "a");
  EXPECT_TRUE(
      HasTagNameInvalidation("span", "div", invalidation_lists.descendants));
}

TEST_F(RuleFeatureSetTest, pseudoWhereAnySibling) {
  EXPECT_EQ(SelectorPreMatch::kMayMatch,
            CollectFeatures(".v ~ :where(.w, .x)"));

  InvalidationLists invalidation_lists;
  CollectInvalidationSetsForClass(invalidation_lists, "v");
  EXPECT_TRUE(HasNoInvalidation(invalidation_lists.descendants));
  EXPECT_TRUE(HasClassInvalidation("w", "x", invalidation_lists.siblings));
}

TEST_F(RuleFeatureSetTest, pseudoWhereDescendantSibling) {
  EXPECT_EQ(SelectorPreMatch::kMayMatch,
            CollectFeatures(".u .v ~ :where(.w, .x)"));

  InvalidationLists invalidation_lists;
  CollectInvalidationSetsForClass(invalidation_lists, "u");
  EXPECT_TRUE(HasClassInvalidation("w", "x", invalidation_lists.descendants));
  EXPECT_TRUE(HasNoInvalidation(invalidation_lists.siblings));
}

TEST_F(RuleFeatureSetTest, pseudoWhereWithComplexSelectors) {
  EXPECT_EQ(SelectorPreMatch::kMayMatch,
            CollectFeatures(".a :where(.w+.b, .x>#c)"));

  InvalidationLists invalidation_lists;
  CollectInvalidationSetsForClass(invalidation_lists, "a");
  EXPECT_TRUE(HasClassInvalidation("b", invalidation_lists.descendants));
  EXPECT_TRUE(HasIdInvalidation("c", invalidation_lists.descendants));
  EXPECT_TRUE(HasNoInvalidation(invalidation_lists.siblings));
}

TEST_F(RuleFeatureSetTest, pseudoWhereNested) {
  EXPECT_EQ(SelectorPreMatch::kMayMatch,
            CollectFeatures(".a :where(.w+.b, .e+:where(.c, #d))"));

  InvalidationLists invalidation_lists;
  CollectInvalidationSetsForClass(invalidation_lists, "a");
  EXPECT_TRUE(HasClassInvalidation("b", "c", invalidation_lists.descendants));
  EXPECT_TRUE(HasIdInvalidation("d", invalidation_lists.descendants));
  EXPECT_TRUE(HasNoInvalidation(invalidation_lists.siblings));
}

TEST_F(RuleFeatureSetTest, invalidatesParts) {
  EXPECT_EQ(SelectorPreMatch::kMayMatch,
            CollectFeatures(".a .b::part(partname)"));

  {
    InvalidationLists invalidation_lists;
    CollectInvalidationSetsForClass(invalidation_lists, "a");
    EXPECT_EQ(1u, invalidation_lists.descendants.size());
    EXPECT_TRUE(HasNoSelfInvalidation(invalidation_lists.descendants));
    EXPECT_TRUE(invalidation_lists.descendants[0]->TreeBoundaryCrossing());
    EXPECT_TRUE(invalidation_lists.descendants[0]->InvalidatesParts());
  }

  {
    InvalidationLists invalidation_lists;
    CollectInvalidationSetsForClass(invalidation_lists, "b");
    EXPECT_EQ(1u, invalidation_lists.descendants.size());
    EXPECT_TRUE(HasPartsInvalidation(invalidation_lists.descendants));
    EXPECT_FALSE(invalidation_lists.descendants[0]->WholeSubtreeInvalid());
    EXPECT_TRUE(invalidation_lists.descendants[0]->TreeBoundaryCrossing());
    EXPECT_TRUE(invalidation_lists.descendants[0]->InvalidatesParts());
  }

  {
    InvalidationLists invalidation_lists;
    CollectPartInvalidationSet(invalidation_lists);
    EXPECT_EQ(1u, invalidation_lists.descendants.size());
    EXPECT_TRUE(HasPartsInvalidation(invalidation_lists.descendants));
    EXPECT_TRUE(invalidation_lists.descendants[0]->TreeBoundaryCrossing());
    EXPECT_TRUE(invalidation_lists.descendants[0]->InvalidatesParts());
  }
}

TEST_F(RuleFeatureSetTest, invalidatesTerminalHas) {
  EXPECT_EQ(SelectorPreMatch::kMayMatch, CollectFeatures(".a .b:has(.c)"));

  {
    InvalidationLists invalidation_lists;
    CollectInvalidationSetsForClass(invalidation_lists, "a");
    EXPECT_TRUE(HasClassInvalidation("b", invalidation_lists.descendants));
    EXPECT_TRUE(HasNoInvalidation(invalidation_lists.siblings));
    EXPECT_FALSE(NeedsHasInvalidationForClass("a"));
  }

  {
    InvalidationLists invalidation_lists;
    CollectInvalidationSetsForClass(invalidation_lists, "b");
    EXPECT_TRUE(HasSelfInvalidation(invalidation_lists.descendants));
    EXPECT_TRUE(HasNoInvalidation(invalidation_lists.siblings));
    EXPECT_FALSE(NeedsHasInvalidationForClass("b"));
  }

  {
    InvalidationLists invalidation_lists;
    CollectInvalidationSetsForClass(invalidation_lists, "c");
    EXPECT_TRUE(HasNoInvalidation(invalidation_lists.descendants));
    EXPECT_TRUE(HasNoInvalidation(invalidation_lists.siblings));
    EXPECT_TRUE(NeedsHasInvalidationForClass("c"));
  }
}

TEST_F(RuleFeatureSetTest, invalidatesNonTerminalHas) {
  EXPECT_EQ(SelectorPreMatch::kMayMatch, CollectFeatures(".a .b:has(.c) .d"));

  {
    InvalidationLists invalidation_lists;
    CollectInvalidationSetsForClass(invalidation_lists, "a");
    EXPECT_TRUE(HasClassInvalidation("d", invalidation_lists.descendants));
    EXPECT_TRUE(HasNoInvalidation(invalidation_lists.siblings));
    EXPECT_FALSE(NeedsHasInvalidationForClass("a"));
  }

  {
    InvalidationLists invalidation_lists;
    CollectInvalidationSetsForClass(invalidation_lists, "b");
    EXPECT_TRUE(HasClassInvalidation("d", invalidation_lists.descendants));
    EXPECT_TRUE(HasNoInvalidation(invalidation_lists.siblings));
    EXPECT_FALSE(NeedsHasInvalidationForClass("b"));
  }

  {
    InvalidationLists invalidation_lists;
    CollectInvalidationSetsForClass(invalidation_lists, "c");
    EXPECT_TRUE(HasNoInvalidation(invalidation_lists.descendants));
    EXPECT_TRUE(HasNoInvalidation(invalidation_lists.siblings));
    EXPECT_TRUE(NeedsHasInvalidationForClass("c"));
  }

  {
    InvalidationLists invalidation_lists;
    CollectInvalidationSetsForClass(invalidation_lists, "d");
    EXPECT_TRUE(HasSelfInvalidation(invalidation_lists.descendants));
    EXPECT_TRUE(HasNoInvalidation(invalidation_lists.siblings));
    EXPECT_FALSE(NeedsHasInvalidationForClass("d"));
  }

  {
    InvalidationLists invalidation_lists;
    CollectInvalidationSetsForPseudoClass(invalidation_lists,
                                          CSSSelector::kPseudoHas);
    EXPECT_EQ(1u, invalidation_lists.descendants.size());
    EXPECT_TRUE(HasClassInvalidation("d", invalidation_lists.descendants));
    EXPECT_FALSE(invalidation_lists.descendants[0]->TreeBoundaryCrossing());
    EXPECT_TRUE(HasNoInvalidation(invalidation_lists.siblings));
  }
}

TEST_F(RuleFeatureSetTest, invalidatesHasOnShadowHostAtSubjectPosition) {
  EXPECT_EQ(SelectorPreMatch::kMayMatch, CollectFeatures(":host:has(.a)"));

  {
    InvalidationLists invalidation_lists;
    CollectInvalidationSetsForClass(invalidation_lists, "a");
    EXPECT_TRUE(HasNoInvalidation(invalidation_lists.descendants));
    EXPECT_TRUE(HasNoInvalidation(invalidation_lists.siblings));
    EXPECT_TRUE(NeedsHasInvalidationForClass("a"));
  }

  {
    InvalidationLists invalidation_lists;
    CollectInvalidationSetsForPseudoClass(invalidation_lists,
                                          CSSSelector::kPseudoHas);
    EXPECT_EQ(0u, invalidation_lists.descendants.size());
    EXPECT_TRUE(HasNoInvalidation(invalidation_lists.descendants));
    EXPECT_TRUE(HasNoInvalidation(invalidation_lists.siblings));
  }
}

TEST_F(RuleFeatureSetTest, invalidatesHasOnShadowHostAtNonSubjectPosition) {
  EXPECT_EQ(SelectorPreMatch::kMayMatch, CollectFeatures(":host:has(.a) .b"));

  {
    InvalidationLists invalidation_lists;
    CollectInvalidationSetsForClass(invalidation_lists, "a");
    EXPECT_TRUE(HasNoInvalidation(invalidation_lists.descendants));
    EXPECT_TRUE(HasNoInvalidation(invalidation_lists.siblings));
    EXPECT_TRUE(NeedsHasInvalidationForClass("a"));
  }

  {
    InvalidationLists invalidation_lists;
    CollectInvalidationSetsForPseudoClass(invalidation_lists,
                                          CSSSelector::kPseudoHas);
    EXPECT_EQ(1u, invalidation_lists.descendants.size());
    EXPECT_TRUE(HasClassInvalidation("b", invalidation_lists.descendants));
    EXPECT_TRUE(invalidation_lists.descendants[0]->TreeBoundaryCrossing());
    EXPECT_TRUE(HasNoInvalidation(invalidation_lists.siblings));
  }
}

TEST_F(RuleFeatureSetTest, invalidatesHasInShadowTree) {
  EXPECT_EQ(SelectorPreMatch::kMayMatch,
            CollectFeatures(":host .a:has(.b) .c"));

  {
    InvalidationLists invalidation_lists;
    CollectInvalidationSetsForClass(invalidation_lists, "a");
    EXPECT_TRUE(HasClassInvalidation("c", invalidation_lists.descendants));
    EXPECT_TRUE(HasNoInvalidation(invalidation_lists.siblings));
    EXPECT_FALSE(NeedsHasInvalidationForClass("a"));
  }

  {
    InvalidationLists invalidation_lists;
    CollectInvalidationSetsForClass(invalidation_lists, "b");
    EXPECT_TRUE(HasNoInvalidation(invalidation_lists.descendants));
    EXPECT_TRUE(HasNoInvalidation(invalidation_lists.siblings));
    EXPECT_TRUE(NeedsHasInvalidationForClass("b"));
  }

  {
    InvalidationLists invalidation_lists;
    CollectInvalidationSetsForPseudoClass(invalidation_lists,
                                          CSSSelector::kPseudoHas);
    EXPECT_EQ(1u, invalidation_lists.descendants.size());
    EXPECT_TRUE(HasClassInvalidation("c", invalidation_lists.descendants));
    EXPECT_FALSE(invalidation_lists.descendants[0]->TreeBoundaryCrossing());
    EXPECT_TRUE(HasNoInvalidation(invalidation_lists.siblings));
  }
}

TEST_F(RuleFeatureSetTest, invalidatesMultipleHasAfterHostAtSubjectPosition) {
  EXPECT_EQ(SelectorPreMatch::kMayMatch,
            CollectFeatures(":host:has(.a):has(.b)"));

  {
    InvalidationLists invalidation_lists;
    CollectInvalidationSetsForClass(invalidation_lists, "a");
    EXPECT_TRUE(HasNoInvalidation(invalidation_lists.descendants));
    EXPECT_TRUE(HasNoInvalidation(invalidation_lists.siblings));
    EXPECT_TRUE(NeedsHasInvalidationForClass("a"));
  }

  {
    InvalidationLists invalidation_lists;
    CollectInvalidationSetsForClass(invalidation_lists, "b");
    EXPECT_TRUE(HasNoInvalidation(invalidation_lists.descendants));
    EXPECT_TRUE(HasNoInvalidation(invalidation_lists.siblings));
    EXPECT_TRUE(NeedsHasInvalidationForClass("b"));
  }

  {
    InvalidationLists invalidation_lists;
    CollectInvalidationSetsForPseudoClass(invalidation_lists,
                                          CSSSelector::kPseudoHas);
    EXPECT_EQ(0u, invalidation_lists.descendants.size());
    EXPECT_TRUE(HasNoInvalidation(invalidation_lists.descendants));
    EXPECT_TRUE(HasNoInvalidation(invalidation_lists.siblings));
  }
}

TEST_F(RuleFeatureSetTest,
       invalidatesMultipleHasAfterHostAtNonSubjectPosition) {
  EXPECT_EQ(SelectorPreMatch::kMayMatch,
            CollectFeatures(":host:has(.a):has(.b) .c"));

  {
    InvalidationLists invalidation_lists;
    CollectInvalidationSetsForClass(invalidation_lists, "a");
    EXPECT_TRUE(HasNoInvalidation(invalidation_lists.descendants));
    EXPECT_TRUE(HasNoInvalidation(invalidation_lists.siblings));
    EXPECT_TRUE(NeedsHasInvalidationForClass("a"));
  }

  {
    InvalidationLists invalidation_lists;
    CollectInvalidationSetsForClass(invalidation_lists, "b");
    EXPECT_TRUE(HasNoInvalidation(invalidation_lists.descendants));
    EXPECT_TRUE(HasNoInvalidation(invalidation_lists.siblings));
    EXPECT_TRUE(NeedsHasInvalidationForClass("b"));
  }

  {
    InvalidationLists invalidation_lists;
    CollectInvalidationSetsForPseudoClass(invalidation_lists,
                                          CSSSelector::kPseudoHas);
    EXPECT_EQ(1u, invalidation_lists.descendants.size());
    EXPECT_TRUE(HasClassInvalidation("c", invalidation_lists.descendants));
    EXPECT_TRUE(invalidation_lists.descendants[0]->TreeBoundaryCrossing());
    EXPECT_TRUE(HasNoInvalidation(invalidation_lists.siblings));
  }
}

TEST_F(RuleFeatureSetTest, MediaQueryResultFlagsEquality) {
  RuleFeatureSet empty;

  RuleFeatureSet viewport_dependent;
  viewport_dependent.MutableMediaQueryResultFlags().is_viewport_dependent =
      true;

  RuleFeatureSet device_dependent;
  device_dependent.MutableMediaQueryResultFlags().is_device_dependent = true;

  RuleFeatureSet font_unit;
  font_unit.MutableMediaQueryResultFlags().unit_flags =
      MediaQueryExpValue::kFontRelative;

  RuleFeatureSet dynamic_viewport_unit;
  dynamic_viewport_unit.MutableMediaQueryResultFlags().unit_flags =
      MediaQueryExpValue::kDynamicViewport;

  EXPECT_EQ(empty, empty);
  EXPECT_EQ(viewport_dependent, viewport_dependent);
  EXPECT_EQ(device_dependent, device_dependent);
  EXPECT_EQ(font_unit, font_unit);

  EXPECT_NE(viewport_dependent, device_dependent);
  EXPECT_NE(empty, device_dependent);
  EXPECT_NE(font_unit, viewport_dependent);
  EXPECT_NE(font_unit, dynamic_viewport_unit);
}

struct RefTestData {
  const char* main;
  const char* ref;
};

// The test passes if |main| produces the same RuleFeatureSet as |ref|.
RefTestData ref_equal_test_data[] = {
    // clang-format off
    {".a", ".a"},

    // :is
    {":is(.a)", ".a"},
    {":is(.a .b)", ".a .b"},
    {".a :is(.b .c)", ".a .c, .b .c"},
    {".a + :is(.b .c)", ".a + .c, .b .c"},
    {".a + :is(.b .c)", ".a + .c, .b .c"},
    {"div + :is(.b .c)", "div + .c, .b .c"},
    {":is(.a :is(.b + .c))", ".a .c, .b + .c"},
    {".a + :is(.b) :is(.c)", ".a + .b .c"},
    {":is(#a:nth-child(1))", "#a:nth-child(1)"},
    {":is(#a:nth-child(1), #b:nth-child(1))",
     "#a:nth-child(1), #b:nth-child(1)"},
    {":is(#a, #b):nth-child(1)", "#a:nth-child(1), #b:nth-child(1)"},
    {":is(:nth-child(1))", ":nth-child(1)"},
    {".a :is(.b, .c):nth-child(1)", ".a .b:nth-child(1), .a .c:nth-child(1)"},
    // TODO(andruud): We currently add _all_ rightmost features to the nth-
    // sibling set, so .b is added here, since nth-child is present _somewhere_
    // in the rightmost compound. Hence the unexpected '.b:nth-child(1)'
    // selector in the ref.
    {".a :is(.b, .c:nth-child(1))",
     ".a .b, .a .c:nth-child(1), .b:nth-child(1)"},
    {":is(.a) .b", ".a .b"},
    {":is(.a, .b) .c", ".a .c, .b .c"},
    {":is(.a .b, .c .d) .e", ".a .b .e, .c .d .e"},
    {":is(:is(.a .b, .c) :is(.d, .e .f), .g) .h",
     ".a .b .h, .c .h, .d .h, .e .f .h, .g .h"},
    {":is(.a, .b) :is(.c, .d)", ".a .c, .a .d, .b .c, .b .d"},
    {":is(.a .b, .c .d) :is(.e .f, .g .h)",
     ".a .b .f, .a .b .h, .c .d .f, .c .d .h, .e .f, .g .h"},
    {":is(.a + .b)", ".a + .b"},
    {":is(.a + .b, .c + .d) .e", ".a + .b .e, .c + .d .e"},
    {":is(.a ~ .b, .c + .e + .f) :is(.c .d, .e)",
     ".a ~ .b .d, .a ~ .b .e, .c + .e + .f .d, .c + .e + .f .e, .c .d"},
    {":is(.a) + .b", ".a + .b"},
    {":is(.a, .b) + .c", ".a + .c, .b + .c"},
    {":is(.a + .b, .c + .d) + .e", ".a + .b + .e, .c + .d + .e"},
    {":is(.a + .b, .c + .d) + :is(.e + .f, .g + .h)",
     ".a + .b + .f, .a + .b + .h, .c + .d + .f, .c + .d + .h,"
     ".e + .f, .g + .h"},
    {":is(div)", "div"},
    {":is(div, span)", "div, span"},
    {":is(.a, div)", ".a, div"},
    {":is(.a, :is(div, span))", ".a, div, span"},
    {":is(.a, span) :is(div, .b)", ".a div, .a .b, span div, span .b"},
    {":is(.a, span) + :is(div, .b)",
     ".a + div, .a + .b, span + div, span + .b"},
    {":is(.a, .b)::slotted(.c)", ".a::slotted(.c), .b::slotted(.c)"},
    {".a :is(.b, .c)::slotted(.d)", ".a .b::slotted(.d), .a .c::slotted(.d)"},
    {".a + :is(.b, .c)::slotted(.d)",
     ".a + .b::slotted(.d), .a + .c::slotted(.d)"},
    {".a::slotted(:is(.b, .c))", ".a::slotted(.b), .a::slotted(.c)"},
    {":is(.a, .b)::cue(i)", ".a::cue(i), .b::cue(i)"},
    {".a :is(.b, .c)::cue(i)", ".a .b::cue(i), .a .c::cue(i)"},
    {".a + :is(.b, .c)::cue(i)", ".a + .b::cue(i), .a + .c::cue(i)"},
    {".a::cue(:is(.b, .c))", ".a::cue(.b), .a::cue(.c)"},
    {":is(.a, :host + .b, .c) .d", ".a .d, :host + .b .d, .c .d"},
    {":is(.a, :host(.b) .c, .d) div", ".a div, :host(.b) .c div, .d div"},
    {".a::host(:is(.b, .c))", ".a::host(.b), .a::host(.c)"},
    {".a :is(.b, .c)::part(foo)", ".a .b::part(foo), .a .c::part(foo)"},
    {":is(.a, .b)::part(foo)", ".a::part(foo), .b::part(foo)"},
    {":is(.a, .b) :is(.c, .d)::part(foo)",
     ".a .c::part(foo), .a .d ::part(foo),"
     ".b .c::part(foo), .b .d ::part(foo)"},
    {":is(.a, .b)::first-letter", ".a::first-letter, .b::first-letter"},
    {":is(.a, .b .c)::first-line", ".a::first-line, .b .c::first-line"},
    // TODO(andruud): Here we would normally expect a ref:
    // '.a::first-line, .b + .c::first-line', however the latter selector
    // currently marks the sibling invalidation set for .b as whole subtree
    // invalid, whereas the :is() version does not. This could be improved.
    {":is(.a, .b + .c)::first-line", ".a::first-line, .b + .c, .b + .c *"},
    {":is(.a, .b ~ .c > .d)::first-line",
     ".a::first-line, .b ~ .c > .d::first-line"},
    {":is(.a, :host-context(.b), .c)", ".a, :host-context(.b), .c"},
    {":is(.a, :host-context(.b), .c) .d", ".a .d, :host-context(.b) .d, .c .d"},
    {":is(.a, :host-context(.b), .c) + .d",
     ".a + .d, :host-context(.b) + .d, .c + .d"},
    {":host-context(.a) :is(.b, .c)",
     ":host-context(.a) .b, :host-context(.a) .c"},
    {":host-context(:is(.a))", ":host-context(.a)"},
    {":host-context(:is(.a, .b))", ":host-context(.a), :host-context(.b)"},
    {":is(.a, .b + .c).d", ".a.d, .b + .c.d"},
    {".a :is(.b .c .d).e", ".a .d.e, .b .c .d.e"},
    {":is(*)", "*"},
    {".a :is(*)", ".a *"},
    {":is(*) .a", "* .a"},
    {".a + :is(*)", ".a + *"},
    {":is(*) + .a", "* + .a"},
    {".a + :is(.b, *)", ".a + .b, .a + *"},
    {":is(.a, *) + .b", ".a + .b, * + .b"},
    {".a :is(.b, *)", ".a .b, .a *"},
    {":is(.a, *) .b", ".a .b, * .b"},
    {":is(.a + .b, .c) *", ".a + .b *, .c *"},
    {":is(.a + *, .c) *", ".a + * *, .c *"},
    {".a + .b + .c:is(*)", ".a + .b + .c"},
    {".a :not(.b)", ".a *, .b"},
    {".a :not(.b, .c)", ".a *, .b, .c"},
    {".a :not(.b, .c .d)", ".a *, .b, .c .d"},
    {".a :not(.b, .c + .d)", ".a *, .b, .c + .d"},
    {".a + :not(.b, .c + .d)", ".a + *, .b, .c + .d"},
    {":not(.a .b) .c", ".a .c, .b .c"},
    {":not(.a .b, .c) + .d", "* + .d, .a .b + .d, .c + .d"},
    {":not(.a .b, .c .d) :not(.e + .f, .g + .h)",
     ".a .b *, .c .d *, :not(.e + .f), :not(.g + .h)"},
    {":not(.a, .b)", ":not(.a), :not(.b)"},
    {":not(.a .b, .c)", ":not(.a .b), :not(.c)"},
    {":not(.a :not(.b + .c), :not(div))", ":not(.a :not(.b + .c)), :not(div)"},
    {":not(:is(.a))", ":not(.a)"},
    {":not(:is(.a, .b))", ":not(.a), :not(.b)"},
    {":not(:is(.a .b))", ":not(.a .b)"},
    {":not(:is(.a .b, .c + .d))", ":not(.a .b, .c + .d)"},
    {".a :not(:is(.b .c))", ".a :not(.b .c)"},
    {":not(:is(.a)) .b", ":not(.a) .b"},
    {":not(:is(.a .b, .c)) :not(:is(.d + .e, .f))",
     ":not(.a .b, .c) :not(.d + .e, .f)"},
    // We don't have any special support for nested :not(): it's treated
    // as a single :not() level in terms of invalidation:
    {":not(:not(.a))", ":not(.a)"},
    {":not(:not(:not(.a)))", ":not(.a)"},
    {".a :not(:is(:not(.b), .c))", ".a :not(.b), .a :not(.c)"},
    {":not(:is(:not(.a), .b)) .c", ":not(.a) .c, :not(.b) .c"},
    {".a :is(:hover)", ".a :hover"},
    {":is(:hover) .a", ":hover .a"},
    {"button:is(:hover, :focus)", "button:hover, button:focus"},
    {".a :is(.b, :hover)", ".a .b, .a :hover"},
    {".a + :is(:hover) + .c", ".a + :hover + .c"},
    {".a + :is(.b, :hover) + .c", ".a + .b + .c, .a + :hover + .c"},
    {":is(ol, li)::before", "ol::before, li::before"},
    {":is(.a + .b, .c)::before", ".a + .b::before, .c::before"},
    {":is(ol, li)::-internal-input-suggested",
     "ol::-internal-input-suggested, li::-internal-input-suggested"},
    {":is([foo], [bar])", "[foo], [bar]"},
    {".a :is([foo], [bar])", ".a [foo], .a [bar]"},
    {":is([foo], [bar]) .a", "[foo] .a, [bar] .a"},
    {":is([a], [b]) :is([c], [d])", "[a] [c], [a] [d], [b] [c], [b] [d]"},

    {"", "div"},
    {"", "::before"},
    {"", ":host"},
    {"", "*"},
    {"ol", "ul"},
    {"::cue(a)", "::cue(b)"},
    {"div", "span"},
    // clang-format on
};

// The test passes if |main| does not produce the same RuleFeatureSet as |ref|.
RefTestData ref_not_equal_test_data[] = {
    // clang-format off
    {"", ".a"},
    {"", "#a"},
    {"", ":hover"},
    {"", ":host(.a)"},
    {"", ":host-context(.a)"},
    {"", ":not(.a)"},
    {".a", ".b"},
    {".a", ".a, .b"},
    {"#a", "#b"},
    {"[foo]", "[bar]"},
    {":link", ":visited"},
    {".a::before", ".b::after"},
    {".a .b", ".a .c"},
    {".a + .b", ".a + .c"},
    {".a + .b .c", ".a + .b .d"},
    {"div + .a", "div + .b"},
    {".a:nth-child(1)", ".b:nth-child(1)"},
    // clang-format on
};

class RuleFeatureSetRefTest : public RuleFeatureSetTest {
 public:
  void Run(const RefTestData& data) {
    RuleFeatureSet main_set;
    RuleFeatureSet ref_set;

    SCOPED_TRACE(testing::Message() << "Ref: " << data.ref);
    SCOPED_TRACE(testing::Message() << "Main: " << data.main);
    SCOPED_TRACE("Please see RuleFeatureSet::ToString for documentation");

    CollectTo(data.main, main_set);
    CollectTo(data.ref, ref_set);

    Compare(main_set, ref_set);
  }

  virtual void CollectTo(
      const char*,
      RuleFeatureSet&,
      CSSNestingType nesting_type = CSSNestingType::kNone,
      StyleRule* parent_rule_for_nesting = nullptr) const = 0;
  virtual void Compare(const RuleFeatureSet&, const RuleFeatureSet&) const = 0;
};

class RuleFeatureSetSelectorRefTest : public RuleFeatureSetRefTest {
 public:
  void CollectTo(const char* text,
                 RuleFeatureSet& set,
                 CSSNestingType nesting_type = CSSNestingType::kNone,
                 StyleRule* parent_rule_for_nesting = nullptr) const override {
    CollectFeaturesTo(text, set, nesting_type, parent_rule_for_nesting);
  }
};

class RuleFeatureSetRefEqualTest
    : public RuleFeatureSetSelectorRefTest,
      public testing::WithParamInterface<RefTestData> {
 public:
  void Compare(const RuleFeatureSet& main,
               const RuleFeatureSet& ref) const override {
    EXPECT_EQ(main, ref);
  }
};

INSTANTIATE_TEST_SUITE_P(RuleFeatureSetTest,
                         RuleFeatureSetRefEqualTest,
                         testing::ValuesIn(ref_equal_test_data));

TEST_P(RuleFeatureSetRefEqualTest, All) {
  Run(GetParam());
}

class RuleFeatureSetRefNotEqualTest
    : public RuleFeatureSetSelectorRefTest,
      public testing::WithParamInterface<RefTestData> {
 public:
  void Compare(const RuleFeatureSet& main,
               const RuleFeatureSet& ref) const override {
    EXPECT_NE(main, ref);
  }
};

INSTANTIATE_TEST_SUITE_P(RuleFeatureSetTest,
                         RuleFeatureSetRefNotEqualTest,
                         testing::ValuesIn(ref_not_equal_test_data));

TEST_P(RuleFeatureSetRefNotEqualTest, All) {
  Run(GetParam());
}

RefTestData ref_scope_equal_test_data[] = {
    // Note that for ordering consistency :is() is sometimes used
    // "unnecessarily" in the refs below.
    {"@scope (.a) { div {} }", ".a div, .a:is(div) {}"},
    {"@scope (#a) { div {} }", "#a div, #a:is(div) {}"},
    {"@scope (main) { div {} }", "main div, main:is(div) {}"},
    {"@scope ([foo]) { div {} }", "[foo] div, [foo]:is(div) {}"},
    {"@scope (.a) { .b {} }", ".a .b, .a.b {}"},
    {"@scope (.a) { #b {} }", ".a #b, .a#b {}"},
    {"@scope (.a) { [foo] {} }", ".a [foo], .a[foo] {}"},
    {"@scope (.a) { .a {} }", ".a .a, .a.a {}"},

    // Sibling combinators:
    {"@scope (.a) { .b + .c {} }", ".b + .c, .a .c, .a {}"},
    {"@scope (.a, .b) { .c + .d {} }",
     ".c + .d, :is(.a, .b) .d, :is(.a, .b) {}"},
    {"@scope (.a) { .b ~ :scope {} }", ".b ~ *, .a *, .a {}"},
    {"@scope (.a) { .b + :scope {} }", ".b + *, .a *, .a {}"},

    // Multiple items in selector lists:
    {"@scope (.a, .b) { div {} }", ":is(.a, .b) div, :is(.a, .b):is(div) {}"},
    {"@scope (.a, :is(.b, .c)) { div {} }",
     ":is(.a, .b, .c) div, :is(.a, .b, .c):is(div) {}"},

    // Using "to" keyword:
    {"@scope (.a, .b) to (.c, .d) { div {} }",
     ":is(.a, .b, .c, .d) div, :is(.a, .b):is(.c, .d):is(div) {}"},

    // TODO(crbug.com/1280240): Many of the following tests current expect
    // whole-subtree invalidation, because we don't extract any features from
    // :scope. That should be improved.

    // Explicit :scope:
    {"@scope (.a) { :scope {} }", ".a *, .a {}"},
    {"@scope (.a) { .b :scope {} }", ".a :is(.b *), .b .a {}"},
    {"@scope (.a, .b) { :scope {} }", ":is(.a, .b) *, :is(.a, .b) {}"},

    {"@scope (.a) to (:scope) { .b {} }", ".a .b, .a.b {}"},
    {"@scope (.a) to (:scope) { :scope {} }", ".a *, .a {}"},
    {"@scope (.a, .b) { @scope (.c, :scope .d) { .e {} } }",
     ":is(.a, .b):is(.c, .d) .e, :is(.a, .b):is(.c, .d):is(.e) {}"},

    // &
    {"@scope (.a) { & {} }", ".a .a {}"},
    {"@scope (.a) { .b & {} }", ".b .a, .a .a {}"},
    {"@scope (.a, .b) { & {} }", ":is(.a, .b) :is(.a, .b) {}"},

    {"@scope (.a, .b) { @scope (.c, & .d) { .e {} } }",
     ":is(.a, .b, .c, .d) .e, :is(.a, .b), :is(.c, .d) {}"},
    {"@scope (.a) to (&) { .b {} }", ".a .b, .a {}"},
    {"@scope (.a) to (&) { & {} }", ".a .a {}"},

    // Nested @scopes
    {"@scope (.a, .b) { @scope (.c, .d) { .e {} } }",
     ":is(.a, .b, .c, .d) .e, :is(.a, .b, .c, .d):is(.e) {}"},
    {"@scope (.a, .b) { @scope (.c, .d) { :scope {} } }",
     ":is(.a, .b, .c, .d) *, :is(.a, .b, .c, .d) {}"},
    {"@scope (.a, .b) { @scope (:scope, .c) { :scope {} } }",
     ":is(.a, .b, .c) *, :is(.a, .b, .c) {}"},
    {"@scope (.a) to (.b) { @scope (.c) to (.d) { .e {} } }",
     ":is(.a, .b, .c, .d) .e, :is(.a, .b):is(.c, .d):is(.e) {}"},
};

class RuleFeatureSetScopeRefTest
    : public RuleFeatureSetRefTest,
      public testing::WithParamInterface<RefTestData> {
 public:

  void CollectTo(const char* text,
                 RuleFeatureSet& set,
                 CSSNestingType nesting_type = CSSNestingType::kNone,
                 StyleRule* parent_rule_for_nesting = nullptr) const override {
    Document* document =
        Document::CreateForTest(execution_context_.GetExecutionContext());
    StyleRuleBase* rule = css_test_helpers::ParseRule(*document, text);
    ASSERT_TRUE(rule);

    const StyleScope* scope = nullptr;

    // Find the inner StyleRule.
    while (IsA<StyleRuleScope>(rule)) {
      auto& scope_rule = To<StyleRuleScope>(*rule);
      scope = scope_rule.GetStyleScope().CopyWithParent(scope);
      const HeapVector<Member<StyleRuleBase>>& child_rules =
          scope_rule.ChildRules();
      ASSERT_EQ(1u, child_rules.size());
      rule = child_rules[0].Get();
    }

    auto* style_rule = DynamicTo<StyleRule>(rule);
    ASSERT_TRUE(style_rule);

    CollectFeaturesTo(style_rule, scope, set);
  }

  void Compare(const RuleFeatureSet& main,
               const RuleFeatureSet& ref) const override {
    EXPECT_EQ(main, ref);
  }
};

INSTANTIATE_TEST_SUITE_P(SelectorChecker,
                         RuleFeatureSetScopeRefTest,
                         testing::ValuesIn(ref_scope_equal_test_data));

TEST_P(RuleFeatureSetScopeRefTest, All) {
  Run(GetParam());
}

TEST_F(RuleFeatureSetTest, CopyOnWrite) {
  // RuleFeatureSet local1 has an entry in each of the class/id/attribute/
  // pseudo sets.
  RuleFeatureSet local1;
  CollectFeatures(".a .b");
  CollectFeatures("#d .e");
  CollectFeatures("[thing] .f");
  CollectFeatures(":hover .h");
  MergeInto(local1);
  ClearFeatures();
  EXPECT_TRUE(HasRefCountForClassInvalidationSet(local1, "a", RefCount::kOne));
  EXPECT_TRUE(HasRefCountForIdInvalidationSet(local1, "d", RefCount::kOne));
  EXPECT_TRUE(
      HasRefCountForAttributeInvalidationSet(local1, "thing", RefCount::kOne));
  EXPECT_TRUE(HasRefCountForPseudoInvalidationSet(
      local1, CSSSelector::kPseudoHover, RefCount::kOne));

  // RuleFeatureSet local2 overlaps partially with local1.
  RuleFeatureSet local2;
  CollectFeatures(".a .c");
  CollectFeatures("#d img");
  MergeInto(local2);
  ClearFeatures();
  EXPECT_TRUE(HasRefCountForClassInvalidationSet(local2, "a", RefCount::kOne));
  EXPECT_TRUE(HasRefCountForIdInvalidationSet(local2, "d", RefCount::kOne));

  // RuleFeatureSet local3 overlaps partially with local1, but not with local2.
  RuleFeatureSet local3;
  CollectFeatures("[thing] .g");
  CollectFeatures(":hover .i");
  MergeInto(local3);
  ClearFeatures();
  EXPECT_TRUE(
      HasRefCountForAttributeInvalidationSet(local3, "thing", RefCount::kOne));
  EXPECT_TRUE(HasRefCountForPseudoInvalidationSet(
      local3, CSSSelector::kPseudoHover, RefCount::kOne));

  // Using an empty RuleFeatureSet to simulate the global RuleFeatureSet:
  RuleFeatureSet global;

  // After adding local1, we expect to share the InvalidationSets with local1.
  global.Merge(local1);
  EXPECT_TRUE(HasRefCountForClassInvalidationSet(global, "a", RefCount::kMany));
  EXPECT_TRUE(HasRefCountForIdInvalidationSet(global, "d", RefCount::kMany));
  EXPECT_TRUE(
      HasRefCountForAttributeInvalidationSet(global, "thing", RefCount::kMany));
  EXPECT_TRUE(HasRefCountForPseudoInvalidationSet(
      global, CSSSelector::kPseudoHover, RefCount::kMany));

  // For the InvalidationSet keys that overlap with local1, |global| now had to
  // copy the existing InvalidationSets at those keys before modifying them,
  // so we expect |global| to be the only reference holder to those
  // InvalidationSets.
  global.Merge(local2);
  EXPECT_TRUE(HasRefCountForClassInvalidationSet(global, "a", RefCount::kOne));
  EXPECT_TRUE(HasRefCountForIdInvalidationSet(global, "d", RefCount::kOne));
  EXPECT_TRUE(
      HasRefCountForAttributeInvalidationSet(global, "thing", RefCount::kMany));
  EXPECT_TRUE(HasRefCountForPseudoInvalidationSet(
      global, CSSSelector::kPseudoHover, RefCount::kMany));

  global.Merge(local3);
  EXPECT_TRUE(HasRefCountForClassInvalidationSet(global, "a", RefCount::kOne));
  EXPECT_TRUE(HasRefCountForIdInvalidationSet(global, "d", RefCount::kOne));
  EXPECT_TRUE(
      HasRefCountForAttributeInvalidationSet(global, "thing", RefCount::kOne));
  EXPECT_TRUE(HasRefCountForPseudoInvalidationSet(
      global, CSSSelector::kPseudoHover, RefCount::kOne));
}

TEST_F(RuleFeatureSetTest, CopyOnWrite_SiblingDescendantPairs) {
  // Test data:
  Vector<const char*> data;
  // Descendant.
  data.push_back(".a .b0");
  data.push_back(".a .b1");
  // Sibling.
  data.push_back(".a + .b2");
  data.push_back(".a + .b3");
  // Sibling with sibling descendants.
  data.push_back(".a + .b4 .b5");
  data.push_back(".a + .b6 .b7");
  // Sibling with descendants.
  data.push_back(".a + .b8, .a .b9");
  data.push_back(".a + .b10, .a .b11");
  // Sibling with sibling descendants and descendants.
  data.push_back(".a + .b12 .b13, .a .b14");
  data.push_back(".a + .b15 .b16, .a .b17");

  // For each possible pair in |data|, make sure that we are properly sharing
  // the InvalidationSet from |local1| until we add the InvalidationSet from
  // |local2|.
  for (const char* selector1 : data) {
    for (const char* selector2 : data) {
      RuleFeatureSet local1;
      CollectFeatures(selector1);
      MergeInto(local1);
      ClearFeatures();

      RuleFeatureSet local2;
      CollectFeatures(selector2);
      MergeInto(local2);
      ClearFeatures();

      RuleFeatureSet global;
      global.Merge(local1);
      EXPECT_TRUE(
          HasRefCountForClassInvalidationSet(global, "a", RefCount::kMany));
      global.Merge(local2);
      EXPECT_TRUE(
          HasRefCountForClassInvalidationSet(global, "a", RefCount::kOne));
    }
  }
}

TEST_F(RuleFeatureSetTest, CopyOnWrite_SelfInvalidation) {
  RuleFeatureSet local1;
  CollectFeatures(".a");
  MergeInto(local1);
  ClearFeatures();

  RuleFeatureSet local2;
  CollectFeatures(".a");
  MergeInto(local2);
  ClearFeatures();

  // Adding the SelfInvalidationSet to the SelfInvalidationSet does not cause
  // a copy.
  RuleFeatureSet global;
  global.Merge(local1);
  EXPECT_TRUE(HasRefCountForClassInvalidationSet(global, "a", RefCount::kMany));
  global.Merge(local2);
  EXPECT_TRUE(HasRefCountForClassInvalidationSet(global, "a", RefCount::kMany));
}

TEST_F(RuleFeatureSetTest, isPseudoContainingComplexInsideHas1) {
  EXPECT_EQ(SelectorPreMatch::kMayMatch, CollectFeatures(".a:has(:is(.b .c))"));

  {
    InvalidationLists invalidation_lists;
    CollectInvalidationSetsForClass(invalidation_lists, "a");
    EXPECT_TRUE(HasSelfInvalidation(invalidation_lists.descendants));
    EXPECT_TRUE(HasNoInvalidation(invalidation_lists.siblings));
  }

  {
    InvalidationLists invalidation_lists;
    CollectInvalidationSetsForClass(invalidation_lists, "b");
    EXPECT_TRUE(HasClassInvalidation("a", invalidation_lists.descendants));
    EXPECT_TRUE(HasNoInvalidation(invalidation_lists.siblings));
  }

  {
    InvalidationLists invalidation_lists;
    CollectInvalidationSetsForClass(invalidation_lists, "c");
    EXPECT_TRUE(HasNoInvalidation(invalidation_lists.descendants));
    EXPECT_TRUE(HasNoInvalidation(invalidation_lists.siblings));
  }
}

TEST_F(RuleFeatureSetTest, isPseudoContainingComplexInsideHas2) {
  EXPECT_EQ(SelectorPreMatch::kMayMatch,
            CollectFeatures(".a:has(:is(.b > .c))"));

  {
    InvalidationLists invalidation_lists;
    CollectInvalidationSetsForClass(invalidation_lists, "a");
    EXPECT_TRUE(HasSelfInvalidation(invalidation_lists.descendants));
    EXPECT_TRUE(HasNoInvalidation(invalidation_lists.siblings));
  }

  {
    InvalidationLists invalidation_lists;
    CollectInvalidationSetsForClass(invalidation_lists, "b");
    EXPECT_TRUE(HasClassInvalidation("a", invalidation_lists.descendants));
    EXPECT_TRUE(HasNoInvalidation(invalidation_lists.siblings));
  }

  {
    InvalidationLists invalidation_lists;
    CollectInvalidationSetsForClass(invalidation_lists, "c");
    EXPECT_TRUE(HasNoInvalidation(invalidation_lists.descendants));
    EXPECT_TRUE(HasNoInvalidation(invalidation_lists.siblings));
  }
}

TEST_F(RuleFeatureSetTest, isPseudoContainingComplexInsideHas3) {
  EXPECT_EQ(SelectorPreMatch::kMayMatch,
            CollectFeatures(".a:has(~ :is(.b ~ .c))"));

  {
    InvalidationLists invalidation_lists;
    CollectInvalidationSetsForClass(invalidation_lists, "a");
    EXPECT_TRUE(HasSelfInvalidation(invalidation_lists.descendants));
    EXPECT_TRUE(HasNoInvalidation(invalidation_lists.siblings));
  }

  {
    InvalidationLists invalidation_lists;
    CollectInvalidationSetsForClass(invalidation_lists, "b");
    EXPECT_TRUE(HasNoInvalidation(invalidation_lists.descendants));
    EXPECT_TRUE(
        HasSiblingClassInvalidation(SiblingInvalidationSet::kDirectAdjacentMax,
                                    "a", invalidation_lists.siblings));
    EXPECT_TRUE(
        HasSiblingNoDescendantInvalidation(invalidation_lists.siblings));
  }

  {
    InvalidationLists invalidation_lists;
    CollectInvalidationSetsForClass(invalidation_lists, "c");
    EXPECT_TRUE(HasNoInvalidation(invalidation_lists.descendants));
    EXPECT_TRUE(HasNoInvalidation(invalidation_lists.siblings));
  }
}

TEST_F(RuleFeatureSetTest, isPseudoContainingComplexInsideHas4) {
  EXPECT_EQ(SelectorPreMatch::kMayMatch,
            CollectFeatures(".a:has(~ :is(.b + .c))"));

  {
    InvalidationLists invalidation_lists;
    CollectInvalidationSetsForClass(invalidation_lists, "a");
    EXPECT_TRUE(HasSelfInvalidation(invalidation_lists.descendants));
    EXPECT_TRUE(HasNoInvalidation(invalidation_lists.siblings));
  }

  {
    InvalidationLists invalidation_lists;
    CollectInvalidationSetsForClass(invalidation_lists, "b");
    EXPECT_TRUE(HasNoInvalidation(invalidation_lists.descendants));
    EXPECT_TRUE(
        HasSiblingClassInvalidation(SiblingInvalidationSet::kDirectAdjacentMax,
                                    "a", invalidation_lists.siblings));
    EXPECT_TRUE(
        HasSiblingNoDescendantInvalidation(invalidation_lists.siblings));
  }

  {
    InvalidationLists invalidation_lists;
    CollectInvalidationSetsForClass(invalidation_lists, "c");
    EXPECT_TRUE(HasNoInvalidation(invalidation_lists.descendants));
    EXPECT_TRUE(HasNoInvalidation(invalidation_lists.siblings));
  }
}

TEST_F(RuleFeatureSetTest, isPseudoContainingComplexInsideHas5) {
  EXPECT_EQ(SelectorPreMatch::kMayMatch,
            CollectFeatures(".a:has(~ :is(.b .c ~ .d))"));

  {
    InvalidationLists invalidation_lists;
    CollectInvalidationSetsForClass(invalidation_lists, "a");
    EXPECT_TRUE(HasSelfInvalidation(invalidation_lists.descendants));
    EXPECT_TRUE(HasNoInvalidation(invalidation_lists.siblings));
  }

  {
    InvalidationLists invalidation_lists;
    CollectInvalidationSetsForClass(invalidation_lists, "b");
    EXPECT_TRUE(HasClassInvalidation("a", invalidation_lists.descendants));
    EXPECT_TRUE(HasNoInvalidation(invalidation_lists.siblings));
  }

  {
    InvalidationLists invalidation_lists;
    CollectInvalidationSetsForClass(invalidation_lists, "c");
    EXPECT_TRUE(HasNoInvalidation(invalidation_lists.descendants));
    EXPECT_TRUE(
        HasSiblingClassInvalidation(SiblingInvalidationSet::kDirectAdjacentMax,
                                    "a", invalidation_lists.siblings));
    EXPECT_TRUE(
        HasSiblingNoDescendantInvalidation(invalidation_lists.siblings));
  }

  {
    InvalidationLists invalidation_lists;
    CollectInvalidationSetsForClass(invalidation_lists, "d");
    EXPECT_TRUE(HasNoInvalidation(invalidation_lists.descendants));
    EXPECT_TRUE(HasNoInvalidation(invalidation_lists.siblings));
  }
}

TEST_F(RuleFeatureSetTest, isPseudoContainingComplexInsideHas6) {
  EXPECT_EQ(SelectorPreMatch::kMayMatch,
            CollectFeatures(".a:has(~ :is(.b > .c + .d))"));

  {
    InvalidationLists invalidation_lists;
    CollectInvalidationSetsForClass(invalidation_lists, "a");
    EXPECT_TRUE(HasSelfInvalidation(invalidation_lists.descendants));
    EXPECT_TRUE(HasNoInvalidation(invalidation_lists.siblings));
  }

  {
    InvalidationLists invalidation_lists;
    CollectInvalidationSetsForClass(invalidation_lists, "b");
    EXPECT_TRUE(HasClassInvalidation("a", invalidation_lists.descendants));
    EXPECT_TRUE(HasNoInvalidation(invalidation_lists.siblings));
  }

  {
    InvalidationLists invalidation_lists;
    CollectInvalidationSetsForClass(invalidation_lists, "c");
    EXPECT_TRUE(HasNoInvalidation(invalidation_lists.descendants));
    EXPECT_TRUE(
        HasSiblingClassInvalidation(SiblingInvalidationSet::kDirectAdjacentMax,
                                    "a", invalidation_lists.siblings));
    EXPECT_TRUE(
        HasSiblingNoDescendantInvalidation(invalidation_lists.siblings));
  }

  {
    InvalidationLists invalidation_lists;
    CollectInvalidationSetsForClass(invalidation_lists, "d");
    EXPECT_TRUE(HasNoInvalidation(invalidation_lists.descendants));
    EXPECT_TRUE(HasNoInvalidation(invalidation_lists.siblings));
  }
}

TEST_F(RuleFeatureSetTest, isPseudoContainingComplexInsideHas7) {
  EXPECT_EQ(SelectorPreMatch::kMayMatch,
            CollectFeatures(".a:has(:is(.b ~ .c .d))"));

  {
    InvalidationLists invalidation_lists;
    CollectInvalidationSetsForClass(invalidation_lists, "a");
    EXPECT_TRUE(HasSelfInvalidation(invalidation_lists.descendants));
    EXPECT_TRUE(HasNoInvalidation(invalidation_lists.siblings));
  }

  {
    InvalidationLists invalidation_lists;
    CollectInvalidationSetsForClass(invalidation_lists, "b");
    EXPECT_TRUE(HasNoInvalidation(invalidation_lists.descendants));
    EXPECT_TRUE(
        HasSiblingAndSiblingDescendantInvalidationForLogicalCombinationsInHas(
            "a", "c", "a", invalidation_lists.siblings));
  }

  {
    InvalidationLists invalidation_lists;
    CollectInvalidationSetsForClass(invalidation_lists, "c");
    EXPECT_TRUE(HasClassInvalidation("a", invalidation_lists.descendants));
    EXPECT_TRUE(HasNoInvalidation(invalidation_lists.siblings));
  }

  {
    InvalidationLists invalidation_lists;
    CollectInvalidationSetsForClass(invalidation_lists, "d");
    EXPECT_TRUE(HasNoInvalidation(invalidation_lists.descendants));
    EXPECT_TRUE(HasNoInvalidation(invalidation_lists.siblings));
  }
}

TEST_F(RuleFeatureSetTest, isPseudoContainingComplexInsideHas8) {
  EXPECT_EQ(SelectorPreMatch::kMayMatch,
            CollectFeatures(".a:has(:is(.b + .c > .d))"));

  {
    InvalidationLists invalidation_lists;
    CollectInvalidationSetsForClass(invalidation_lists, "a");
    EXPECT_TRUE(HasSelfInvalidation(invalidation_lists.descendants));
    EXPECT_TRUE(HasNoInvalidation(invalidation_lists.siblings));
  }

  {
    InvalidationLists invalidation_lists;
    CollectInvalidationSetsForClass(invalidation_lists, "b");
    EXPECT_TRUE(HasNoInvalidation(invalidation_lists.descendants));
    EXPECT_TRUE(
        HasSiblingAndSiblingDescendantInvalidationForLogicalCombinationsInHas(
            "a", "c", "a", invalidation_lists.siblings));
  }

  {
    InvalidationLists invalidation_lists;
    CollectInvalidationSetsForClass(invalidation_lists, "c");
    EXPECT_TRUE(HasClassInvalidation("a", invalidation_lists.descendants));
    EXPECT_TRUE(HasNoInvalidation(invalidation_lists.siblings));
  }

  {
    InvalidationLists invalidation_lists;
    CollectInvalidationSetsForClass(invalidation_lists, "d");
    EXPECT_TRUE(HasNoInvalidation(invalidation_lists.descendants));
    EXPECT_TRUE(HasNoInvalidation(invalidation_lists.siblings));
  }
}

TEST_F(RuleFeatureSetTest, isPseudoContainingComplexInsideHas9) {
  EXPECT_EQ(SelectorPreMatch::kMayMatch,
            CollectFeatures(".a:has(:is(:is(.b, .c) .d))"));

  {
    InvalidationLists invalidation_lists;
    CollectInvalidationSetsForClass(invalidation_lists, "a");
    EXPECT_TRUE(HasSelfInvalidation(invalidation_lists.descendants));
    EXPECT_TRUE(HasNoInvalidation(invalidation_lists.siblings));
  }

  {
    InvalidationLists invalidation_lists;
    CollectInvalidationSetsForClass(invalidation_lists, "b");
    EXPECT_TRUE(HasClassInvalidation("a", invalidation_lists.descendants));
    EXPECT_TRUE(HasNoInvalidation(invalidation_lists.siblings));
  }

  {
    InvalidationLists invalidation_lists;
    CollectInvalidationSetsForClass(invalidation_lists, "c");
    EXPECT_TRUE(HasClassInvalidation("a", invalidation_lists.descendants));
    EXPECT_TRUE(HasNoInvalidation(invalidation_lists.siblings));
  }

  {
    InvalidationLists invalidation_lists;
    CollectInvalidationSetsForClass(invalidation_lists, "d");
    EXPECT_TRUE(HasNoInvalidation(invalidation_lists.descendants));
    EXPECT_TRUE(HasNoInvalidation(invalidation_lists.siblings));
  }
}

TEST_F(RuleFeatureSetTest, isPseudoContainingComplexInsideHas10) {
  EXPECT_EQ(SelectorPreMatch::kMayMatch,
            CollectFeatures(".a:has(~ :is(:is(.b, .c) ~ .d))"));

  {
    InvalidationLists invalidation_lists;
    CollectInvalidationSetsForClass(invalidation_lists, "a");
    EXPECT_TRUE(HasSelfInvalidation(invalidation_lists.descendants));
    EXPECT_TRUE(HasNoInvalidation(invalidation_lists.siblings));
  }

  {
    InvalidationLists invalidation_lists;
    CollectInvalidationSetsForClass(invalidation_lists, "b");
    EXPECT_TRUE(HasNoInvalidation(invalidation_lists.descendants));
    EXPECT_TRUE(
        HasSiblingClassInvalidation(SiblingInvalidationSet::kDirectAdjacentMax,
                                    "a", invalidation_lists.siblings));
    EXPECT_TRUE(
        HasSiblingNoDescendantInvalidation(invalidation_lists.siblings));
  }

  {
    InvalidationLists invalidation_lists;
    CollectInvalidationSetsForClass(invalidation_lists, "c");
    EXPECT_TRUE(HasNoInvalidation(invalidation_lists.descendants));
    EXPECT_TRUE(
        HasSiblingClassInvalidation(SiblingInvalidationSet::kDirectAdjacentMax,
                                    "a", invalidation_lists.siblings));
    EXPECT_TRUE(
        HasSiblingNoDescendantInvalidation(invalidation_lists.siblings));
  }

  {
    InvalidationLists invalidation_lists;
    CollectInvalidationSetsForClass(invalidation_lists, "d");
    EXPECT_TRUE(HasNoInvalidation(invalidation_lists.descendants));
    EXPECT_TRUE(HasNoInvalidation(invalidation_lists.siblings));
  }
}

TEST_F(RuleFeatureSetTest, isPseudoContainingComplexInsideHas11) {
  EXPECT_EQ(SelectorPreMatch::kMayMatch, CollectFeatures(":has(:is(.a .b))"));

  {
    InvalidationLists invalidation_lists;
    CollectInvalidationSetsForClass(invalidation_lists, "a");
    EXPECT_TRUE(HasWholeSubtreeInvalidation(invalidation_lists.descendants));
    EXPECT_TRUE(HasNoInvalidation(invalidation_lists.siblings));
  }

  {
    InvalidationLists invalidation_lists;
    CollectInvalidationSetsForClass(invalidation_lists, "b");
    EXPECT_TRUE(HasNoInvalidation(invalidation_lists.descendants));
    EXPECT_TRUE(HasNoInvalidation(invalidation_lists.siblings));
  }
}

TEST_F(RuleFeatureSetTest, isPseudoContainingComplexInsideHas12) {
  EXPECT_EQ(SelectorPreMatch::kMayMatch,
            CollectFeatures(":has(~ :is(.a ~ .b))"));

  {
    InvalidationLists invalidation_lists;
    CollectInvalidationSetsForClass(invalidation_lists, "a");
    EXPECT_TRUE(HasNoInvalidation(invalidation_lists.descendants));
    EXPECT_TRUE(HasSelfInvalidation(invalidation_lists.siblings));
    EXPECT_TRUE(
        HasSiblingNoDescendantInvalidation(invalidation_lists.siblings));
  }

  {
    InvalidationLists invalidation_lists;
    CollectInvalidationSetsForClass(invalidation_lists, "b");
    EXPECT_TRUE(HasNoInvalidation(invalidation_lists.descendants));
    EXPECT_TRUE(HasNoInvalidation(invalidation_lists.siblings));
  }
}

TEST_F(RuleFeatureSetTest, isPseudoContainingComplexInsideHas13) {
  EXPECT_EQ(SelectorPreMatch::kMayMatch,
            CollectFeatures(".a:has(~ :is(.b ~ .c .d ~ .e))"));

  {
    InvalidationLists invalidation_lists;
    CollectInvalidationSetsForClass(invalidation_lists, "a");
    EXPECT_TRUE(HasSelfInvalidation(invalidation_lists.descendants));
    EXPECT_TRUE(HasNoInvalidation(invalidation_lists.siblings));
  }

  {
    InvalidationLists invalidation_lists;
    CollectInvalidationSetsForClass(invalidation_lists, "b");
    EXPECT_TRUE(HasNoInvalidation(invalidation_lists.descendants));
    EXPECT_TRUE(
        HasSiblingAndSiblingDescendantInvalidationForLogicalCombinationsInHas(
            "a", "c", "a", invalidation_lists.siblings));
  }

  {
    InvalidationLists invalidation_lists;
    CollectInvalidationSetsForClass(invalidation_lists, "c");
    EXPECT_TRUE(HasClassInvalidation("a", invalidation_lists.descendants));
    EXPECT_TRUE(HasNoInvalidation(invalidation_lists.siblings));
  }

  {
    InvalidationLists invalidation_lists;
    CollectInvalidationSetsForClass(invalidation_lists, "d");
    EXPECT_TRUE(HasNoInvalidation(invalidation_lists.descendants));
    EXPECT_TRUE(
        HasSiblingClassInvalidation(SiblingInvalidationSet::kDirectAdjacentMax,
                                    "a", invalidation_lists.siblings));
    EXPECT_TRUE(
        HasSiblingNoDescendantInvalidation(invalidation_lists.siblings));
  }

  {
    InvalidationLists invalidation_lists;
    CollectInvalidationSetsForClass(invalidation_lists, "e");
    EXPECT_TRUE(HasNoInvalidation(invalidation_lists.descendants));
    EXPECT_TRUE(HasNoInvalidation(invalidation_lists.siblings));
  }
}

TEST_F(RuleFeatureSetTest, isPseudoContainingComplexInsideHas14) {
  EXPECT_EQ(SelectorPreMatch::kMayMatch,
            CollectFeatures(".a:has(~ :is(.b ~ .c)) .d"));

  {
    InvalidationLists invalidation_lists;
    CollectInvalidationSetsForClass(invalidation_lists, "b");
    EXPECT_TRUE(HasSiblingDescendantInvalidation(
        SiblingInvalidationSet::kDirectAdjacentMax, "a", "d",
        invalidation_lists.siblings));
    EXPECT_TRUE(HasNoSelfInvalidation(invalidation_lists.siblings));
  }

  {
    InvalidationLists invalidation_lists;
    CollectInvalidationSetsForClass(invalidation_lists, "c");
    EXPECT_TRUE(HasNoInvalidation(invalidation_lists.descendants));
    EXPECT_TRUE(HasNoInvalidation(invalidation_lists.siblings));
  }
}

TEST_F(RuleFeatureSetTest, isPseudoContainingComplexInsideHas15) {
  EXPECT_EQ(SelectorPreMatch::kMayMatch,
            CollectFeatures(".a:has(~ :is(* ~ .b))"));
  {
    InvalidationLists invalidation_lists;
    CollectUniversalSiblingInvalidationSet(invalidation_lists);

    EXPECT_TRUE(
        HasSiblingClassInvalidation(SiblingInvalidationSet::kDirectAdjacentMax,
                                    "a", invalidation_lists.siblings));
    EXPECT_TRUE(HasSelfInvalidation(invalidation_lists.siblings));
  }

  {
    InvalidationLists invalidation_lists;
    CollectInvalidationSetsForClass(invalidation_lists, "b");
    EXPECT_TRUE(HasNoInvalidation(invalidation_lists.descendants));
    EXPECT_TRUE(HasNoInvalidation(invalidation_lists.siblings));
  }
}

TEST_F(RuleFeatureSetTest, isPseudoContainingComplexInsideHas16) {
  EXPECT_EQ(SelectorPreMatch::kMayMatch,
            CollectFeatures(".a:has(~ :is(* ~ .b)) .c"));

  {
    InvalidationLists invalidation_lists;
    CollectUniversalSiblingInvalidationSet(invalidation_lists);

    EXPECT_TRUE(HasSiblingDescendantInvalidation(
        SiblingInvalidationSet::kDirectAdjacentMax, "a", "c",
        invalidation_lists.siblings));
    EXPECT_TRUE(HasNoSelfInvalidation(invalidation_lists.siblings));
  }

  {
    InvalidationLists invalidation_lists;
    CollectInvalidationSetsForClass(invalidation_lists, "b");
    EXPECT_TRUE(HasNoInvalidation(invalidation_lists.descendants));
    EXPECT_TRUE(HasNoInvalidation(invalidation_lists.siblings));
  }
}

TEST_F(RuleFeatureSetTest, isPseudoContainingComplexInsideHas17) {
  EXPECT_EQ(SelectorPreMatch::kMayMatch,
            CollectFeatures(".a :has(:is(.b .c)).d"));

  {
    InvalidationLists invalidation_lists;
    CollectInvalidationSetsForClass(invalidation_lists, "a");
    EXPECT_TRUE(HasClassInvalidation("d", invalidation_lists.descendants));
    EXPECT_TRUE(HasNoInvalidation(invalidation_lists.siblings));
  }

  {
    InvalidationLists invalidation_lists;
    CollectInvalidationSetsForClass(invalidation_lists, "b");
    EXPECT_TRUE(HasWholeSubtreeInvalidation(invalidation_lists.descendants));
    EXPECT_TRUE(HasNoInvalidation(invalidation_lists.siblings));
  }

  {
    InvalidationLists invalidation_lists;
    CollectInvalidationSetsForClass(invalidation_lists, "c");
    EXPECT_TRUE(HasNoInvalidation(invalidation_lists.descendants));
    EXPECT_TRUE(HasNoInvalidation(invalidation_lists.siblings));
  }

  {
    InvalidationLists invalidation_lists;
    CollectInvalidationSetsForClass(invalidation_lists, "d");
    EXPECT_TRUE(HasSelfInvalidation(invalidation_lists.descendants));
    EXPECT_TRUE(HasNoInvalidation(invalidation_lists.siblings));
  }
}

TEST_F(RuleFeatureSetTest, isPseudoContainingComplexInsideHas18) {
  EXPECT_EQ(SelectorPreMatch::kMayMatch,
            CollectFeatures(".a:has(~ :is(.b ~ :is(.c ~ .d)))"));

  {
    InvalidationLists invalidation_lists;
    CollectInvalidationSetsForClass(invalidation_lists, "a");
    EXPECT_TRUE(HasSelfInvalidation(invalidation_lists.descendants));
    EXPECT_TRUE(HasNoInvalidation(invalidation_lists.siblings));
  }

  {
    InvalidationLists invalidation_lists;
    CollectInvalidationSetsForClass(invalidation_lists, "b");
    EXPECT_TRUE(HasNoInvalidation(invalidation_lists.descendants));
    EXPECT_TRUE(
        HasSiblingClassInvalidation(SiblingInvalidationSet::kDirectAdjacentMax,
                                    "a", invalidation_lists.siblings));
    EXPECT_TRUE(
        HasSiblingNoDescendantInvalidation(invalidation_lists.siblings));
  }

  {
    InvalidationLists invalidation_lists;
    CollectInvalidationSetsForClass(invalidation_lists, "c");
    EXPECT_TRUE(HasNoInvalidation(invalidation_lists.descendants));
    EXPECT_TRUE(
        HasSiblingClassInvalidation(SiblingInvalidationSet::kDirectAdjacentMax,
                                    "a", invalidation_lists.siblings));
    EXPECT_TRUE(
        HasSiblingNoDescendantInvalidation(invalidation_lists.siblings));
  }

  {
    InvalidationLists invalidation_lists;
    CollectInvalidationSetsForClass(invalidation_lists, "d");
    EXPECT_TRUE(HasNoInvalidation(invalidation_lists.descendants));
    EXPECT_TRUE(HasNoInvalidation(invalidation_lists.siblings));
  }
}

TEST_F(RuleFeatureSetTest, isPseudoContainingComplexInsideHas19) {
  EXPECT_EQ(SelectorPreMatch::kMayMatch,
            CollectFeatures(".a:has(~ :is(:is(.b ~ .c) ~ .d))"));

  {
    InvalidationLists invalidation_lists;
    CollectInvalidationSetsForClass(invalidation_lists, "a");
    EXPECT_TRUE(HasSelfInvalidation(invalidation_lists.descendants));
    EXPECT_TRUE(HasNoInvalidation(invalidation_lists.siblings));
  }

  {
    InvalidationLists invalidation_lists;
    CollectInvalidationSetsForClass(invalidation_lists, "b");
    EXPECT_TRUE(HasNoInvalidation(invalidation_lists.descendants));
    EXPECT_TRUE(
        HasSiblingClassInvalidation(SiblingInvalidationSet::kDirectAdjacentMax,
                                    "a", invalidation_lists.siblings));
    EXPECT_TRUE(
        HasSiblingNoDescendantInvalidation(invalidation_lists.siblings));
  }

  {
    InvalidationLists invalidation_lists;
    CollectInvalidationSetsForClass(invalidation_lists, "c");
    EXPECT_TRUE(HasNoInvalidation(invalidation_lists.descendants));
    EXPECT_TRUE(
        HasSiblingClassInvalidation(SiblingInvalidationSet::kDirectAdjacentMax,
                                    "a", invalidation_lists.siblings));
    EXPECT_TRUE(
        HasSiblingNoDescendantInvalidation(invalidation_lists.siblings));
  }

  {
    InvalidationLists invalidation_lists;
    CollectInvalidationSetsForClass(invalidation_lists, "d");
    EXPECT_TRUE(HasNoInvalidation(invalidation_lists.descendants));
    EXPECT_TRUE(HasNoInvalidation(invalidation_lists.siblings));
  }
}

TEST_F(RuleFeatureSetTest, NestedSelector) {
  // Create a parent rule.
  HeapVector<CSSSelector> arena;
  base::span<CSSSelector> selector_vector = CSSParser::ParseSelector(
      StrictCSSParserContext(SecureContextMode::kInsecureContext),
      CSSNestingType::kNone,
      /*parent_rule_for_nesting=*/nullptr, /*is_within_scope=*/false, nullptr,
      ".a, .b", arena);
  auto* parent_rule = StyleRule::Create(
      selector_vector,
      MakeGarbageCollected<MutableCSSPropertyValueSet>(kHTMLStandardMode));

  EXPECT_EQ(SelectorPreMatch::kMayMatch,
            CollectFeatures("& .c", CSSNestingType::kNesting,
                            /*parent_rule_for_nesting=*/parent_rule));

  for (const char* parent_class : {"a", "b"}) {
    SCOPED_TRACE(parent_class);

    InvalidationLists invalidation_lists;
    CollectInvalidationSetsForClass(invalidation_lists, parent_class);
    EXPECT_TRUE(HasClassInvalidation("c", invalidation_lists.descendants));
    EXPECT_TRUE(HasNoInvalidation(invalidation_lists.siblings));
  }

  {
    InvalidationLists invalidation_lists;
    CollectInvalidationSetsForClass(invalidation_lists, "c");
    EXPECT_TRUE(HasSelfInvalidation(invalidation_lists.descendants));
    EXPECT_TRUE(HasNoInvalidation(invalidation_lists.siblings));
  }
}

TEST_F(RuleFeatureSetTest, BloomFilterForClassSelfInvalidation) {
  // Add enough dummy classes that the filter will be created.
  for (unsigned i = 0; i < 100; ++i) {
    CollectFeatures(".dummy");
  }

  EXPECT_EQ(SelectorPreMatch::kMayMatch, CollectFeatures(".p"));

  {
    InvalidationLists invalidation_lists;
    CollectInvalidationSetsForClass(invalidation_lists, "p");
    EXPECT_TRUE(HasSelfInvalidation(invalidation_lists.descendants));
    EXPECT_TRUE(HasNoInvalidation(invalidation_lists.siblings));
  }

  {
    InvalidationLists invalidation_lists;
    CollectInvalidationSetsForClass(invalidation_lists, "q");
    EXPECT_TRUE(HasNoInvalidation(invalidation_lists.descendants));
    EXPECT_TRUE(HasNoInvalidation(invalidation_lists.siblings));
  }
}

TEST_F(RuleFeatureSetTest, BloomFilterForIdSelfInvalidation) {
  // Add enough dummy IDs that the filter will be created.
  for (unsigned i = 0; i < 100; ++i) {
    CollectFeatures("#dummy");
  }

  EXPECT_EQ(SelectorPreMatch::kMayMatch, CollectFeatures("#foo"));

  {
    InvalidationLists invalidation_lists;
    CollectInvalidationSetsForId(invalidation_lists, "foo");
    EXPECT_TRUE(HasSelfInvalidation(invalidation_lists.descendants));
    EXPECT_TRUE(HasNoInvalidation(invalidation_lists.siblings));
  }

  {
    InvalidationLists invalidation_lists;
    CollectInvalidationSetsForId(invalidation_lists, "bar");
    EXPECT_TRUE(HasNoInvalidation(invalidation_lists.descendants));
    EXPECT_TRUE(HasNoInvalidation(invalidation_lists.siblings));
  }
}

}  // namespace blink
