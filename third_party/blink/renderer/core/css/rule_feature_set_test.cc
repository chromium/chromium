// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/rule_feature_set.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/css/css_property_value_set.h"
#include "third_party/blink/renderer/core/css/css_selector_list.h"
#include "third_party/blink/renderer/core/css/invalidation/invalidation_set.h"
#include "third_party/blink/renderer/core/css/parser/css_parser.h"
#include "third_party/blink/renderer/core/css/rule_set.h"
#include "third_party/blink/renderer/core/css/style_rule.h"
#include "third_party/blink/renderer/core/dom/element_traversal.h"
#include "third_party/blink/renderer/core/html/html_body_element.h"
#include "third_party/blink/renderer/core/html/html_document.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/html/html_html_element.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class RuleFeatureSetTest : public testing::Test {
 public:
  RuleFeatureSetTest() = default;

  void SetUp() override {
    document_ = MakeGarbageCollected<HTMLDocument>();
    auto* html = MakeGarbageCollected<HTMLHtmlElement>(*document_);
    html->AppendChild(MakeGarbageCollected<HTMLBodyElement>(*document_));
    document_->AppendChild(html);

    document_->body()->SetInnerHTMLFromString("<b><i></i></b>");
  }

  RuleFeatureSet::SelectorPreMatch CollectFeatures(
      const String& selector_text) {
    CSSSelectorList selector_list = CSSParser::ParseSelector(
        StrictCSSParserContext(SecureContextMode::kInsecureContext), nullptr,
        selector_text);

    Vector<wtf_size_t> indices;
    for (const CSSSelector* s = selector_list.First(); s;
         s = selector_list.Next(*s)) {
      indices.push_back(selector_list.SelectorIndex(*s));
    }

    auto* style_rule = MakeGarbageCollected<StyleRule>(
        std::move(selector_list),
        MakeGarbageCollected<MutableCSSPropertyValueSet>(kHTMLStandardMode));

    RuleFeatureSet::SelectorPreMatch result =
        RuleFeatureSet::SelectorPreMatch::kSelectorNeverMatches;
    for (unsigned i = 0; i < indices.size(); ++i) {
      RuleData* rule_data = RuleData::MaybeCreate(style_rule, indices[i], 0,
                                                  kRuleHasNoSpecialState);
      DCHECK(rule_data);
      if (rule_feature_set_.CollectFeaturesFromRuleData(rule_data))
        result = RuleFeatureSet::SelectorPreMatch::kSelectorMayMatch;
    }
    return result;
  }

  void ClearFeatures() { rule_feature_set_.Clear(); }

  void CollectInvalidationSetsForClass(InvalidationLists& invalidation_lists,
                                       const AtomicString& class_name) const {
    Element* element = Traversal<HTMLElement>::FirstChild(
        *Traversal<HTMLElement>::FirstChild(*document_->body()));
    rule_feature_set_.CollectInvalidationSetsForClass(invalidation_lists,
                                                      *element, class_name);
  }

  void CollectInvalidationSetsForId(InvalidationLists& invalidation_lists,
                                    const AtomicString& id) const {
    Element* element = Traversal<HTMLElement>::FirstChild(
        *Traversal<HTMLElement>::FirstChild(*document_->body()));
    rule_feature_set_.CollectInvalidationSetsForId(invalidation_lists, *element,
                                                   id);
  }

  void CollectInvalidationSetsForAttribute(
      InvalidationLists& invalidation_lists,
      const QualifiedName& attribute_name) const {
    Element* element = Traversal<HTMLElement>::FirstChild(
        *Traversal<HTMLElement>::FirstChild(*document_->body()));
    rule_feature_set_.CollectInvalidationSetsForAttribute(
        invalidation_lists, *element, attribute_name);
  }

  void CollectInvalidationSetsForPseudoClass(
      InvalidationLists& invalidation_lists,
      CSSSelector::PseudoType pseudo) const {
    Element* element = Traversal<HTMLElement>::FirstChild(
        *Traversal<HTMLElement>::FirstChild(*document_->body()));
    rule_feature_set_.CollectInvalidationSetsForPseudoClass(invalidation_lists,
                                                            *element, pseudo);
  }

  void CollectPartInvalidationSet(InvalidationLists& invalidation_lists) const {
    rule_feature_set_.CollectPartInvalidationSet(invalidation_lists);
  }

  void CollectUniversalSiblingInvalidationSet(
      InvalidationLists& invalidation_lists) {
    rule_feature_set_.CollectUniversalSiblingInvalidationSet(invalidation_lists,
                                                             1);
  }

  void CollectNthInvalidationSet(InvalidationLists& invalidation_lists) {
    rule_feature_set_.CollectNthInvalidationSet(invalidation_lists);
  }

  void AddTo(RuleFeatureSet& rule_feature_set) {
    rule_feature_set.Add(rule_feature_set_);
  }

  using BackingType = InvalidationSet::BackingType;

  template <BackingType type>
  HashSet<AtomicString> ToHashSet(
      typename InvalidationSet::Backing<type>::Range range) {
    HashSet<AtomicString> hash_set;
    for (auto str : range)
      hash_set.insert(str);
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

  void ExpectNoInvalidation(InvalidationSetVector& invalidation_sets) {
    EXPECT_EQ(0u, invalidation_sets.size());
  }

  void ExpectSelfInvalidation(InvalidationSetVector& invalidation_sets) {
    EXPECT_EQ(1u, invalidation_sets.size());
    EXPECT_TRUE(invalidation_sets[0]->InvalidatesSelf());
  }

  void ExpectNoSelfInvalidation(InvalidationSetVector& invalidation_sets) {
    EXPECT_EQ(1u, invalidation_sets.size());
    EXPECT_FALSE(invalidation_sets[0]->InvalidatesSelf());
  }

  void ExpectSelfInvalidationSet(InvalidationSetVector& invalidation_sets) {
    EXPECT_EQ(1u, invalidation_sets.size());
    EXPECT_TRUE(invalidation_sets[0]->IsSelfInvalidationSet());
  }

  void ExpectNotSelfInvalidationSet(InvalidationSetVector& invalidation_sets) {
    EXPECT_EQ(1u, invalidation_sets.size());
    EXPECT_FALSE(invalidation_sets[0]->IsSelfInvalidationSet());
  }

  void ExpectWholeSubtreeInvalidation(
      InvalidationSetVector& invalidation_sets) {
    EXPECT_EQ(1u, invalidation_sets.size());
    EXPECT_TRUE(invalidation_sets[0]->WholeSubtreeInvalid());
  }

  void ExpectClassInvalidation(const AtomicString& class_name,
                               InvalidationSetVector& invalidation_sets) {
    EXPECT_EQ(1u, invalidation_sets.size());
    HashSet<AtomicString> classes = ClassSet(*invalidation_sets[0]);
    EXPECT_EQ(1u, classes.size());
    EXPECT_TRUE(classes.Contains(class_name));
  }

  void ExpectClassInvalidation(const AtomicString& first_class_name,
                               const AtomicString& second_class_name,
                               InvalidationSetVector& invalidation_sets) {
    EXPECT_EQ(1u, invalidation_sets.size());
    HashSet<AtomicString> classes = ClassSet(*invalidation_sets[0]);
    EXPECT_EQ(2u, classes.size());
    EXPECT_TRUE(classes.Contains(first_class_name));
    EXPECT_TRUE(classes.Contains(second_class_name));
  }

  void ExpectClassInvalidation(const AtomicString& first_class_name,
                               const AtomicString& second_class_name,
                               const AtomicString& third_class_name,
                               InvalidationSetVector& invalidation_sets) {
    EXPECT_EQ(1u, invalidation_sets.size());
    HashSet<AtomicString> classes = ClassSet(*invalidation_sets[0]);
    EXPECT_EQ(3u, classes.size());
    EXPECT_TRUE(classes.Contains(first_class_name));
    EXPECT_TRUE(classes.Contains(second_class_name));
    EXPECT_TRUE(classes.Contains(third_class_name));
  }

  void ExpectSiblingClassInvalidation(
      unsigned max_direct_adjacent_selectors,
      const AtomicString& sibling_name,
      InvalidationSetVector& invalidation_sets) {
    EXPECT_EQ(1u, invalidation_sets.size());
    const auto& sibling_invalidation_set =
        To<SiblingInvalidationSet>(*invalidation_sets[0]);
    HashSet<AtomicString> classes = ClassSet(sibling_invalidation_set);
    EXPECT_EQ(1u, classes.size());
    EXPECT_TRUE(classes.Contains(sibling_name));
    EXPECT_EQ(max_direct_adjacent_selectors,
              sibling_invalidation_set.MaxDirectAdjacentSelectors());
  }

  void ExpectSiblingIdInvalidation(unsigned max_direct_adjacent_selectors,
                                   const AtomicString& sibling_name,
                                   InvalidationSetVector& invalidation_sets) {
    EXPECT_EQ(1u, invalidation_sets.size());
    const auto& sibling_invalidation_set =
        To<SiblingInvalidationSet>(*invalidation_sets[0]);
    HashSet<AtomicString> ids = IdSet(*invalidation_sets[0]);
    EXPECT_EQ(1u, ids.size());
    EXPECT_TRUE(ids.Contains(sibling_name));
    EXPECT_EQ(max_direct_adjacent_selectors,
              sibling_invalidation_set.MaxDirectAdjacentSelectors());
  }

  void ExpectSiblingDescendantInvalidation(
      unsigned max_direct_adjacent_selectors,
      const AtomicString& sibling_name,
      const AtomicString& descendant_name,
      InvalidationSetVector& invalidation_sets) {
    EXPECT_EQ(1u, invalidation_sets.size());
    const auto& sibling_invalidation_set =
        To<SiblingInvalidationSet>(*invalidation_sets[0]);
    HashSet<AtomicString> classes = ClassSet(sibling_invalidation_set);
    EXPECT_EQ(1u, classes.size());
    EXPECT_TRUE(classes.Contains(sibling_name));
    EXPECT_EQ(max_direct_adjacent_selectors,
              sibling_invalidation_set.MaxDirectAdjacentSelectors());

    HashSet<AtomicString> descendant_classes =
        ClassSet(*sibling_invalidation_set.SiblingDescendants());
    EXPECT_EQ(1u, descendant_classes.size());
    EXPECT_TRUE(descendant_classes.Contains(descendant_name));
  }

  void ExpectSiblingDescendantInvalidation(
      unsigned max_direct_adjacent_selectors,
      const AtomicString& descendant_name,
      InvalidationSetVector& invalidation_sets) {
    ASSERT_EQ(1u, invalidation_sets.size());
    const auto& sibling_invalidation_set =
        To<SiblingInvalidationSet>(*invalidation_sets[0]);
    EXPECT_TRUE(sibling_invalidation_set.WholeSubtreeInvalid());
    EXPECT_EQ(max_direct_adjacent_selectors,
              sibling_invalidation_set.MaxDirectAdjacentSelectors());
    ASSERT_TRUE(sibling_invalidation_set.SiblingDescendants());
    HashSet<AtomicString> descendant_classes =
        ClassSet(*sibling_invalidation_set.SiblingDescendants());
    EXPECT_EQ(1u, descendant_classes.size());
    EXPECT_TRUE(descendant_classes.Contains(descendant_name));
  }

  void ExpectSiblingNoDescendantInvalidation(
      InvalidationSetVector& invalidation_sets) {
    EXPECT_EQ(1u, invalidation_sets.size());
    const auto& sibling_invalidation_set =
        To<SiblingInvalidationSet>(*invalidation_sets[0]);
    EXPECT_FALSE(sibling_invalidation_set.SiblingDescendants());
  }

  void ExpectSiblingWholeSubtreeInvalidation(
      InvalidationSetVector& invalidation_sets) {
    ASSERT_EQ(1u, invalidation_sets.size());
    const auto& sibling_invalidation_set =
        To<SiblingInvalidationSet>(*invalidation_sets[0]);
    ASSERT_TRUE(sibling_invalidation_set.SiblingDescendants());
    EXPECT_TRUE(
        sibling_invalidation_set.SiblingDescendants()->WholeSubtreeInvalid());
  }

  void ExpectIdInvalidation(const AtomicString& id,
                            InvalidationSetVector& invalidation_sets) {
    EXPECT_EQ(1u, invalidation_sets.size());
    HashSet<AtomicString> ids = IdSet(*invalidation_sets[0]);
    EXPECT_EQ(1u, ids.size());
    EXPECT_TRUE(ids.Contains(id));
  }

  void ExpectIdInvalidation(const AtomicString& first_id,
                            const AtomicString& second_id,
                            InvalidationSetVector& invalidation_sets) {
    EXPECT_EQ(1u, invalidation_sets.size());
    HashSet<AtomicString> ids = IdSet(*invalidation_sets[0]);
    EXPECT_EQ(2u, ids.size());
    EXPECT_TRUE(ids.Contains(first_id));
    EXPECT_TRUE(ids.Contains(second_id));
  }

  void ExpectTagNameInvalidation(const AtomicString& tag_name,
                                 InvalidationSetVector& invalidation_sets) {
    EXPECT_EQ(1u, invalidation_sets.size());
    HashSet<AtomicString> tag_names = TagNameSet(*invalidation_sets[0]);
    EXPECT_EQ(1u, tag_names.size());
    EXPECT_TRUE(tag_names.Contains(tag_name));
  }

  void ExpectTagNameInvalidation(const AtomicString& first_tag_name,
                                 const AtomicString& second_tag_name,
                                 InvalidationSetVector& invalidation_sets) {
    EXPECT_EQ(1u, invalidation_sets.size());
    HashSet<AtomicString> tag_names = TagNameSet(*invalidation_sets[0]);
    EXPECT_EQ(2u, tag_names.size());
    EXPECT_TRUE(tag_names.Contains(first_tag_name));
    EXPECT_TRUE(tag_names.Contains(second_tag_name));
  }

  void ExpectAttributeInvalidation(const AtomicString& attribute,
                                   InvalidationSetVector& invalidation_sets) {
    EXPECT_EQ(1u, invalidation_sets.size());
    HashSet<AtomicString> attributes = AttributeSet(*invalidation_sets[0]);
    EXPECT_EQ(1u, attributes.size());
    EXPECT_TRUE(attributes.Contains(attribute));
  }

  void ExpectFullRecalcForRuleSetInvalidation(bool expected) {
    EXPECT_EQ(expected,
              rule_feature_set_.NeedsFullRecalcForRuleSetInvalidation());
  }

  void ExpectPartsInvalidation(InvalidationSetVector& invalidation_sets) {
    EXPECT_EQ(1u, invalidation_sets.size());
    EXPECT_TRUE(invalidation_sets[0]->InvalidatesParts());
  }

  enum class RefCount { kOne, kMany };

  template <typename MapType, typename KeyType>
  void ExpectRefCountForInvalidationSet(const MapType& map,
                                        const KeyType& key,
                                        RefCount ref_count) {
    auto it = map.find(key);
    ASSERT_NE(map.end(), it);

    if (ref_count == RefCount::kOne) {
      EXPECT_TRUE(it->value->HasOneRef());

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
        EXPECT_TRUE(sibling_descendants_has_one_ref);
        EXPECT_TRUE(descendants_has_one_ref);
      }
    } else {
      EXPECT_FALSE(it->value->HasOneRef());
    }
  }

  void ExpectRefCountForClassInvalidationSet(
      const RuleFeatureSet& rule_feature_set,
      const AtomicString& class_name,
      RefCount ref_count) {
    ExpectRefCountForInvalidationSet(rule_feature_set.class_invalidation_sets_,
                                     class_name, ref_count);
  }

  void ExpectRefCountForAttributeInvalidationSet(
      const RuleFeatureSet& rule_feature_set,
      const AtomicString& attribute,
      RefCount ref_count) {
    ExpectRefCountForInvalidationSet(
        rule_feature_set.attribute_invalidation_sets_, attribute, ref_count);
  }

  void ExpectRefCountForIdInvalidationSet(
      const RuleFeatureSet& rule_feature_set,
      const AtomicString& id,
      RefCount ref_count) {
    ExpectRefCountForInvalidationSet(rule_feature_set.id_invalidation_sets_, id,
                                     ref_count);
  }

  void ExpectRefCountForPseudoInvalidationSet(
      const RuleFeatureSet& rule_feature_set,
      CSSSelector::PseudoType key,
      RefCount ref_count) {
    ExpectRefCountForInvalidationSet(rule_feature_set.pseudo_invalidation_sets_,
                                     key, ref_count);
  }

 private:
  RuleFeatureSet rule_feature_set_;
  Persistent<Document> document_;
};

TEST_F(RuleFeatureSetTest, interleavedDescendantSibling1) {
  EXPECT_EQ(RuleFeatureSet::kSelectorMayMatch, CollectFeatures(".p"));

  InvalidationLists invalidation_lists;
  CollectInvalidationSetsForClass(invalidation_lists, "p");
  ExpectSelfInvalidation(invalidation_lists.descendants);
  ExpectNoInvalidation(invalidation_lists.siblings);
}

TEST_F(RuleFeatureSetTest, interleavedDescendantSibling2) {
  EXPECT_EQ(RuleFeatureSet::kSelectorMayMatch, CollectFeatures(".o + .p"));

  InvalidationLists invalidation_lists;
  CollectInvalidationSetsForClass(invalidation_lists, "o");
  ExpectNoInvalidation(invalidation_lists.descendants);
  ExpectSiblingClassInvalidation(1, "p", invalidation_lists.siblings);
}

TEST_F(RuleFeatureSetTest, interleavedDescendantSibling3) {
  EXPECT_EQ(RuleFeatureSet::kSelectorMayMatch,
            CollectFeatures(".m + .n .o + .p"));

  InvalidationLists invalidation_lists;
  CollectInvalidationSetsForClass(invalidation_lists, "n");
  ExpectNoSelfInvalidation(invalidation_lists.descendants);
  ExpectClassInvalidation("p", invalidation_lists.descendants);
  ExpectNoInvalidation(invalidation_lists.siblings);
}

TEST_F(RuleFeatureSetTest, interleavedDescendantSibling4) {
  EXPECT_EQ(RuleFeatureSet::kSelectorMayMatch,
            CollectFeatures(".m + .n .o + .p"));

  InvalidationLists invalidation_lists;
  CollectInvalidationSetsForClass(invalidation_lists, "m");
  ExpectNoInvalidation(invalidation_lists.descendants);
  ExpectSiblingDescendantInvalidation(1, "n", "p", invalidation_lists.siblings);
}

TEST_F(RuleFeatureSetTest, interleavedDescendantSibling5) {
  EXPECT_EQ(RuleFeatureSet::kSelectorMayMatch,
            CollectFeatures(".l ~ .m + .n .o + .p"));

  InvalidationLists invalidation_lists;
  CollectInvalidationSetsForClass(invalidation_lists, "l");
  ExpectNoInvalidation(invalidation_lists.descendants);
  ExpectSiblingDescendantInvalidation(
      SiblingInvalidationSet::kDirectAdjacentMax, "n", "p",
      invalidation_lists.siblings);
}

TEST_F(RuleFeatureSetTest, interleavedDescendantSibling6) {
  EXPECT_EQ(RuleFeatureSet::kSelectorMayMatch,
            CollectFeatures(".k > .l ~ .m + .n .o + .p"));

  InvalidationLists invalidation_lists;
  CollectInvalidationSetsForClass(invalidation_lists, "k");
  ExpectClassInvalidation("p", invalidation_lists.descendants);
  ExpectNoInvalidation(invalidation_lists.siblings);
}

TEST_F(RuleFeatureSetTest, anySibling) {
  EXPECT_EQ(RuleFeatureSet::kSelectorMayMatch,
            CollectFeatures(":-webkit-any(.q, .r) ~ .s .t"));

  InvalidationLists invalidation_lists;
  CollectInvalidationSetsForClass(invalidation_lists, "q");
  ExpectNoInvalidation(invalidation_lists.descendants);
  ExpectSiblingDescendantInvalidation(
      SiblingInvalidationSet::kDirectAdjacentMax, "s", "t",
      invalidation_lists.siblings);
}

TEST_F(RuleFeatureSetTest, any) {
  EXPECT_EQ(RuleFeatureSet::kSelectorMayMatch,
            CollectFeatures(":-webkit-any(.w, .x)"));

  InvalidationLists invalidation_lists;
  CollectInvalidationSetsForClass(invalidation_lists, "w");
  ExpectSelfInvalidation(invalidation_lists.descendants);
  ExpectNoInvalidation(invalidation_lists.siblings);
}

TEST_F(RuleFeatureSetTest, repeatedAny) {
  EXPECT_EQ(RuleFeatureSet::kSelectorMayMatch,
            CollectFeatures(":-webkit-any(.v, .w):-webkit-any(.x, .y, .z)"));

  {
    InvalidationLists invalidation_lists;
    CollectInvalidationSetsForClass(invalidation_lists, "v");
    ExpectSelfInvalidation(invalidation_lists.descendants);
    ExpectNoInvalidation(invalidation_lists.siblings);
  }

  {
    InvalidationLists invalidation_lists;
    CollectInvalidationSetsForClass(invalidation_lists, "x");
    ExpectSelfInvalidation(invalidation_lists.descendants);
    ExpectNoInvalidation(invalidation_lists.siblings);
  }
}

TEST_F(RuleFeatureSetTest, anyIdDescendant) {
  EXPECT_EQ(RuleFeatureSet::kSelectorMayMatch,
            CollectFeatures(".a :-webkit-any(#b, #c)"));

  InvalidationLists invalidation_lists;
  CollectInvalidationSetsForClass(invalidation_lists, "a");
  ExpectIdInvalidation("b", "c", invalidation_lists.descendants);
}

TEST_F(RuleFeatureSetTest, repeatedAnyDescendant) {
  EXPECT_EQ(RuleFeatureSet::kSelectorMayMatch,
            CollectFeatures(".a :-webkit-any(.v, .w):-webkit-any(.x, .y, .z)"));

  InvalidationLists invalidation_lists;
  CollectInvalidationSetsForClass(invalidation_lists, "a");
  ExpectClassInvalidation("v", "w", invalidation_lists.descendants);
}

TEST_F(RuleFeatureSetTest, anyTagDescendant) {
  EXPECT_EQ(RuleFeatureSet::kSelectorMayMatch,
            CollectFeatures(".a :-webkit-any(span, div)"));

  InvalidationLists invalidation_lists;
  CollectInvalidationSetsForClass(invalidation_lists, "a");
  ExpectTagNameInvalidation("span", "div", invalidation_lists.descendants);
}

TEST_F(RuleFeatureSetTest, siblingAny) {
  EXPECT_EQ(RuleFeatureSet::kSelectorMayMatch,
            CollectFeatures(".v ~ :-webkit-any(.w, .x)"));

  InvalidationLists invalidation_lists;
  CollectInvalidationSetsForClass(invalidation_lists, "v");
  ExpectNoInvalidation(invalidation_lists.descendants);
  ExpectClassInvalidation("w", "x", invalidation_lists.siblings);
}

TEST_F(RuleFeatureSetTest, descendantSiblingAny) {
  EXPECT_EQ(RuleFeatureSet::kSelectorMayMatch,
            CollectFeatures(".u .v ~ :-webkit-any(.w, .x)"));

  InvalidationLists invalidation_lists;
  CollectInvalidationSetsForClass(invalidation_lists, "u");
  ExpectClassInvalidation("w", "x", invalidation_lists.descendants);
  ExpectNoInvalidation(invalidation_lists.siblings);
}

TEST_F(RuleFeatureSetTest, id) {
  EXPECT_EQ(RuleFeatureSet::kSelectorMayMatch, CollectFeatures("#a #b"));

  InvalidationLists invalidation_lists;
  CollectInvalidationSetsForId(invalidation_lists, "a");
  ExpectIdInvalidation("b", invalidation_lists.descendants);
}

TEST_F(RuleFeatureSetTest, attribute) {
  EXPECT_EQ(RuleFeatureSet::kSelectorMayMatch, CollectFeatures("[c] [d]"));

  InvalidationLists invalidation_lists;
  CollectInvalidationSetsForAttribute(invalidation_lists,
                                      QualifiedName("", "c", ""));
  ExpectAttributeInvalidation("d", invalidation_lists.descendants);
}

TEST_F(RuleFeatureSetTest, pseudoClass) {
  EXPECT_EQ(RuleFeatureSet::kSelectorMayMatch, CollectFeatures(":valid"));

  InvalidationLists invalidation_lists;
  CollectInvalidationSetsForPseudoClass(invalidation_lists,
                                        CSSSelector::kPseudoValid);
  ExpectSelfInvalidation(invalidation_lists.descendants);
}

TEST_F(RuleFeatureSetTest, tagName) {
  EXPECT_EQ(RuleFeatureSet::kSelectorMayMatch, CollectFeatures(":valid e"));

  InvalidationLists invalidation_lists;
  CollectInvalidationSetsForPseudoClass(invalidation_lists,
                                        CSSSelector::kPseudoValid);
  ExpectTagNameInvalidation("e", invalidation_lists.descendants);
}

TEST_F(RuleFeatureSetTest, contentPseudo) {
  EXPECT_EQ(RuleFeatureSet::kSelectorMayMatch,
            CollectFeatures(".a ::content .b"));
  EXPECT_EQ(RuleFeatureSet::kSelectorMayMatch, CollectFeatures(".a .c"));

  InvalidationLists invalidation_lists;
  CollectInvalidationSetsForClass(invalidation_lists, "a");
  ExpectClassInvalidation("c", invalidation_lists.descendants);

  EXPECT_EQ(RuleFeatureSet::kSelectorMayMatch, CollectFeatures(".a .b"));

  invalidation_lists.descendants.clear();
  CollectInvalidationSetsForClass(invalidation_lists, "a");
  ExpectClassInvalidation("b", "c", invalidation_lists.descendants);
}

TEST_F(RuleFeatureSetTest, nonMatchingHost) {
  EXPECT_EQ(RuleFeatureSet::kSelectorNeverMatches, CollectFeatures(".a:host"));
  EXPECT_EQ(RuleFeatureSet::kSelectorNeverMatches,
            CollectFeatures("*:host(.a)"));
  EXPECT_EQ(RuleFeatureSet::kSelectorNeverMatches,
            CollectFeatures("*:host .a"));
  EXPECT_EQ(RuleFeatureSet::kSelectorNeverMatches,
            CollectFeatures("div :host .a"));
  EXPECT_EQ(RuleFeatureSet::kSelectorNeverMatches,
            CollectFeatures(":host:hover .a"));

  InvalidationLists invalidation_lists;
  CollectInvalidationSetsForClass(invalidation_lists, "a");
  ExpectNoInvalidation(invalidation_lists.descendants);
}

TEST_F(RuleFeatureSetTest, nonMatchingHostContext) {
  EXPECT_EQ(RuleFeatureSet::kSelectorNeverMatches,
            CollectFeatures(".a:host-context(*)"));
  EXPECT_EQ(RuleFeatureSet::kSelectorNeverMatches,
            CollectFeatures("*:host-context(.a)"));
  EXPECT_EQ(RuleFeatureSet::kSelectorNeverMatches,
            CollectFeatures("*:host-context(*) .a"));
  EXPECT_EQ(RuleFeatureSet::kSelectorNeverMatches,
            CollectFeatures("div :host-context(div) .a"));
  EXPECT_EQ(RuleFeatureSet::kSelectorNeverMatches,
            CollectFeatures(":host-context(div):hover .a"));

  InvalidationLists invalidation_lists;
  CollectInvalidationSetsForClass(invalidation_lists, "a");
  ExpectNoInvalidation(invalidation_lists.descendants);
}

TEST_F(RuleFeatureSetTest, universalSiblingInvalidationDirectAdjacent) {
  EXPECT_EQ(RuleFeatureSet::kSelectorMayMatch, CollectFeatures("* + .a"));

  InvalidationLists invalidation_lists;
  CollectUniversalSiblingInvalidationSet(invalidation_lists);

  ExpectSiblingClassInvalidation(1, "a", invalidation_lists.siblings);
  ExpectSelfInvalidation(invalidation_lists.siblings);
}

TEST_F(RuleFeatureSetTest, universalSiblingInvalidationMultipleDirectAdjacent) {
  EXPECT_EQ(RuleFeatureSet::kSelectorMayMatch, CollectFeatures("* + .a + .b"));

  InvalidationLists invalidation_lists;
  CollectUniversalSiblingInvalidationSet(invalidation_lists);

  ExpectSiblingClassInvalidation(2, "b", invalidation_lists.siblings);
  ExpectSelfInvalidation(invalidation_lists.siblings);
}

TEST_F(RuleFeatureSetTest,
       universalSiblingInvalidationDirectAdjacentDescendant) {
  EXPECT_EQ(RuleFeatureSet::kSelectorMayMatch, CollectFeatures("* + .a .b"));

  InvalidationLists invalidation_lists;
  CollectUniversalSiblingInvalidationSet(invalidation_lists);

  ExpectSiblingDescendantInvalidation(1, "a", "b", invalidation_lists.siblings);
  ExpectNoSelfInvalidation(invalidation_lists.siblings);
}

TEST_F(RuleFeatureSetTest, universalSiblingInvalidationIndirectAdjacent) {
  EXPECT_EQ(RuleFeatureSet::kSelectorMayMatch, CollectFeatures("* ~ .a"));

  InvalidationLists invalidation_lists;
  CollectUniversalSiblingInvalidationSet(invalidation_lists);

  ExpectSiblingClassInvalidation(SiblingInvalidationSet::kDirectAdjacentMax,
                                 "a", invalidation_lists.siblings);
  ExpectSelfInvalidation(invalidation_lists.siblings);
}

TEST_F(RuleFeatureSetTest,
       universalSiblingInvalidationMultipleIndirectAdjacent) {
  EXPECT_EQ(RuleFeatureSet::kSelectorMayMatch, CollectFeatures("* ~ .a ~ .b"));

  InvalidationLists invalidation_lists;
  CollectUniversalSiblingInvalidationSet(invalidation_lists);

  ExpectSiblingClassInvalidation(SiblingInvalidationSet::kDirectAdjacentMax,
                                 "b", invalidation_lists.siblings);
  ExpectSelfInvalidation(invalidation_lists.siblings);
}

TEST_F(RuleFeatureSetTest,
       universalSiblingInvalidationIndirectAdjacentDescendant) {
  EXPECT_EQ(RuleFeatureSet::kSelectorMayMatch, CollectFeatures("* ~ .a .b"));

  InvalidationLists invalidation_lists;
  CollectUniversalSiblingInvalidationSet(invalidation_lists);

  ExpectSiblingDescendantInvalidation(
      SiblingInvalidationSet::kDirectAdjacentMax, "a", "b",
      invalidation_lists.siblings);
  ExpectNoSelfInvalidation(invalidation_lists.siblings);
}

TEST_F(RuleFeatureSetTest, universalSiblingInvalidationNot) {
  EXPECT_EQ(RuleFeatureSet::kSelectorMayMatch,
            CollectFeatures(":not(.a) + .b"));

  InvalidationLists invalidation_lists;
  CollectUniversalSiblingInvalidationSet(invalidation_lists);

  ExpectSiblingClassInvalidation(1, "b", invalidation_lists.siblings);
  ExpectSelfInvalidation(invalidation_lists.siblings);
}

TEST_F(RuleFeatureSetTest, nonUniversalSiblingInvalidationNot) {
  EXPECT_EQ(RuleFeatureSet::kSelectorMayMatch,
            CollectFeatures("#x:not(.a) + .b"));

  InvalidationLists invalidation_lists;
  CollectUniversalSiblingInvalidationSet(invalidation_lists);

  ExpectNoInvalidation(invalidation_lists.siblings);
}

TEST_F(RuleFeatureSetTest, universalSiblingInvalidationAny) {
  EXPECT_EQ(RuleFeatureSet::kSelectorMayMatch,
            CollectFeatures(":-webkit-any(.a) + .b"));

  InvalidationLists invalidation_lists;
  CollectUniversalSiblingInvalidationSet(invalidation_lists);

  ExpectSiblingClassInvalidation(1, "b", invalidation_lists.siblings);
  ExpectSelfInvalidation(invalidation_lists.siblings);
}

TEST_F(RuleFeatureSetTest, universalSiblingIdInvalidationAny) {
  EXPECT_EQ(RuleFeatureSet::kSelectorMayMatch,
            CollectFeatures(":-webkit-any(.a) + #b"));

  InvalidationLists invalidation_lists;
  CollectUniversalSiblingInvalidationSet(invalidation_lists);

  ExpectSiblingIdInvalidation(1, "b", invalidation_lists.siblings);
  ExpectSelfInvalidation(invalidation_lists.siblings);
}

TEST_F(RuleFeatureSetTest, nonUniversalSiblingInvalidationAny) {
  EXPECT_EQ(RuleFeatureSet::kSelectorMayMatch,
            CollectFeatures("#x:-webkit-any(.a) + .b"));

  InvalidationLists invalidation_lists;
  CollectUniversalSiblingInvalidationSet(invalidation_lists);

  ExpectNoInvalidation(invalidation_lists.siblings);
}

TEST_F(RuleFeatureSetTest, universalSiblingInvalidationType) {
  EXPECT_EQ(RuleFeatureSet::kSelectorMayMatch, CollectFeatures("div + .a"));

  InvalidationLists invalidation_lists;
  CollectUniversalSiblingInvalidationSet(invalidation_lists);

  ExpectSiblingClassInvalidation(1, "a", invalidation_lists.siblings);
  ExpectSelfInvalidation(invalidation_lists.siblings);
}

TEST_F(RuleFeatureSetTest, nonUniversalSiblingInvalidationType) {
  EXPECT_EQ(RuleFeatureSet::kSelectorMayMatch, CollectFeatures("div#x + .a"));

  InvalidationLists invalidation_lists;
  CollectUniversalSiblingInvalidationSet(invalidation_lists);

  ExpectNoInvalidation(invalidation_lists.siblings);
}

TEST_F(RuleFeatureSetTest, universalSiblingInvalidationLink) {
  EXPECT_EQ(RuleFeatureSet::kSelectorMayMatch, CollectFeatures(":link + .a"));

  InvalidationLists invalidation_lists;
  CollectUniversalSiblingInvalidationSet(invalidation_lists);

  ExpectSiblingClassInvalidation(1, "a", invalidation_lists.siblings);
  ExpectSelfInvalidation(invalidation_lists.siblings);
}

TEST_F(RuleFeatureSetTest, nonUniversalSiblingInvalidationLink) {
  EXPECT_EQ(RuleFeatureSet::kSelectorMayMatch, CollectFeatures("#x:link + .a"));

  InvalidationLists invalidation_lists;
  CollectUniversalSiblingInvalidationSet(invalidation_lists);

  ExpectNoInvalidation(invalidation_lists.siblings);
}

TEST_F(RuleFeatureSetTest, nthInvalidationUniversal) {
  EXPECT_EQ(RuleFeatureSet::kSelectorMayMatch,
            CollectFeatures(":nth-child(2n)"));

  InvalidationLists invalidation_lists;
  CollectNthInvalidationSet(invalidation_lists);

  ExpectNoInvalidation(invalidation_lists.descendants);
  ExpectSelfInvalidation(invalidation_lists.siblings);
  ExpectWholeSubtreeInvalidation(invalidation_lists.siblings);
  ExpectSiblingNoDescendantInvalidation(invalidation_lists.siblings);
}

TEST_F(RuleFeatureSetTest, nthInvalidationClass) {
  EXPECT_EQ(RuleFeatureSet::kSelectorMayMatch,
            CollectFeatures(".a:nth-child(2n)"));

  InvalidationLists invalidation_lists;
  CollectNthInvalidationSet(invalidation_lists);

  ExpectNoInvalidation(invalidation_lists.descendants);
  ExpectSelfInvalidation(invalidation_lists.siblings);
  ExpectSiblingClassInvalidation(SiblingInvalidationSet::kDirectAdjacentMax,
                                 "a", invalidation_lists.siblings);
  ExpectSiblingNoDescendantInvalidation(invalidation_lists.siblings);
}

TEST_F(RuleFeatureSetTest, nthInvalidationUniversalDescendant) {
  EXPECT_EQ(RuleFeatureSet::kSelectorMayMatch,
            CollectFeatures(":nth-child(2n) *"));

  InvalidationLists invalidation_lists;
  CollectNthInvalidationSet(invalidation_lists);

  ExpectNoInvalidation(invalidation_lists.descendants);
  ExpectNoSelfInvalidation(invalidation_lists.siblings);
  ExpectWholeSubtreeInvalidation(invalidation_lists.siblings);
  ExpectSiblingWholeSubtreeInvalidation(invalidation_lists.siblings);
}

TEST_F(RuleFeatureSetTest, nthInvalidationDescendant) {
  EXPECT_EQ(RuleFeatureSet::kSelectorMayMatch,
            CollectFeatures(":nth-child(2n) .a"));

  InvalidationLists invalidation_lists;
  CollectNthInvalidationSet(invalidation_lists);

  ExpectNoInvalidation(invalidation_lists.descendants);
  ExpectNoSelfInvalidation(invalidation_lists.siblings);
  ExpectWholeSubtreeInvalidation(invalidation_lists.siblings);
  ExpectSiblingDescendantInvalidation(
      SiblingInvalidationSet::kDirectAdjacentMax, "a",
      invalidation_lists.siblings);
}

TEST_F(RuleFeatureSetTest, nthInvalidationSibling) {
  EXPECT_EQ(RuleFeatureSet::kSelectorMayMatch,
            CollectFeatures(":nth-child(2n) + .a"));

  InvalidationLists invalidation_lists;
  CollectNthInvalidationSet(invalidation_lists);

  ExpectNoInvalidation(invalidation_lists.descendants);
  ExpectSelfInvalidation(invalidation_lists.siblings);
  ExpectClassInvalidation("a", invalidation_lists.siblings);
}

TEST_F(RuleFeatureSetTest, nthInvalidationSiblingDescendant) {
  EXPECT_EQ(RuleFeatureSet::kSelectorMayMatch,
            CollectFeatures(":nth-child(2n) + .a .b"));

  InvalidationLists invalidation_lists;
  CollectNthInvalidationSet(invalidation_lists);

  ExpectNoInvalidation(invalidation_lists.descendants);
  ExpectNoSelfInvalidation(invalidation_lists.siblings);
  ExpectSiblingDescendantInvalidation(
      SiblingInvalidationSet::kDirectAdjacentMax, "a", "b",
      invalidation_lists.siblings);
}

TEST_F(RuleFeatureSetTest, nthInvalidationNot) {
  EXPECT_EQ(RuleFeatureSet::kSelectorMayMatch,
            CollectFeatures(":not(:nth-child(2n))"));

  InvalidationLists invalidation_lists;
  CollectNthInvalidationSet(invalidation_lists);

  ExpectNoInvalidation(invalidation_lists.descendants);
  ExpectSelfInvalidation(invalidation_lists.siblings);
  ExpectWholeSubtreeInvalidation(invalidation_lists.siblings);
}

TEST_F(RuleFeatureSetTest, nthInvalidationNotClass) {
  EXPECT_EQ(RuleFeatureSet::kSelectorMayMatch,
            CollectFeatures(".a:not(:nth-child(2n))"));

  InvalidationLists invalidation_lists;
  CollectNthInvalidationSet(invalidation_lists);

  ExpectNoInvalidation(invalidation_lists.descendants);
  ExpectSelfInvalidation(invalidation_lists.siblings);
  ExpectSiblingClassInvalidation(SiblingInvalidationSet::kDirectAdjacentMax,
                                 "a", invalidation_lists.siblings);
}

TEST_F(RuleFeatureSetTest, nthInvalidationNotDescendant) {
  EXPECT_EQ(RuleFeatureSet::kSelectorMayMatch,
            CollectFeatures(".blah:not(:nth-child(2n)) .a"));

  InvalidationLists invalidation_lists;
  CollectNthInvalidationSet(invalidation_lists);

  ExpectNoInvalidation(invalidation_lists.descendants);
  ExpectNoSelfInvalidation(invalidation_lists.siblings);
  ExpectWholeSubtreeInvalidation(invalidation_lists.siblings);
  ExpectSiblingDescendantInvalidation(
      SiblingInvalidationSet::kDirectAdjacentMax, "a",
      invalidation_lists.siblings);
}

TEST_F(RuleFeatureSetTest, nthInvalidationAny) {
  EXPECT_EQ(RuleFeatureSet::kSelectorMayMatch,
            CollectFeatures(":-webkit-any(#nomatch, :nth-child(2n))"));

  InvalidationLists invalidation_lists;
  CollectNthInvalidationSet(invalidation_lists);

  ExpectNoInvalidation(invalidation_lists.descendants);
  ExpectSelfInvalidation(invalidation_lists.siblings);
  ExpectWholeSubtreeInvalidation(invalidation_lists.siblings);
  ExpectSiblingNoDescendantInvalidation(invalidation_lists.siblings);
}

TEST_F(RuleFeatureSetTest, nthInvalidationAnyClass) {
  EXPECT_EQ(RuleFeatureSet::kSelectorMayMatch,
            CollectFeatures(".a:-webkit-any(#nomatch, :nth-child(2n))"));

  InvalidationLists invalidation_lists;
  CollectNthInvalidationSet(invalidation_lists);

  ExpectNoInvalidation(invalidation_lists.descendants);
  ExpectSelfInvalidation(invalidation_lists.siblings);
  ExpectClassInvalidation("a", invalidation_lists.siblings);
}

TEST_F(RuleFeatureSetTest, nthInvalidationAnyDescendant) {
  EXPECT_EQ(RuleFeatureSet::kSelectorMayMatch,
            CollectFeatures(".blah:-webkit-any(#nomatch, :nth-child(2n)) .a"));

  InvalidationLists invalidation_lists;
  CollectNthInvalidationSet(invalidation_lists);

  ExpectNoInvalidation(invalidation_lists.descendants);
  ExpectNoSelfInvalidation(invalidation_lists.siblings);
  ExpectSiblingDescendantInvalidation(
      SiblingInvalidationSet::kDirectAdjacentMax, "a",
      invalidation_lists.siblings);
}

TEST_F(RuleFeatureSetTest, RuleSetInvalidationTypeSelector) {
  EXPECT_EQ(RuleFeatureSet::kSelectorMayMatch, CollectFeatures("div"));
  ExpectFullRecalcForRuleSetInvalidation(false);
  ClearFeatures();

  EXPECT_EQ(RuleFeatureSet::kSelectorMayMatch, CollectFeatures("* div"));
  ExpectFullRecalcForRuleSetInvalidation(false);
  ClearFeatures();

  EXPECT_EQ(RuleFeatureSet::kSelectorMayMatch, CollectFeatures("body *"));
  ExpectFullRecalcForRuleSetInvalidation(true);
}

TEST_F(RuleFeatureSetTest, RuleSetInvalidationClassIdAttr) {
  EXPECT_EQ(RuleFeatureSet::kSelectorMayMatch, CollectFeatures(".c"));
  ExpectFullRecalcForRuleSetInvalidation(false);
  ClearFeatures();

  EXPECT_EQ(RuleFeatureSet::kSelectorMayMatch, CollectFeatures(".c *"));
  ExpectFullRecalcForRuleSetInvalidation(false);
  ClearFeatures();

  EXPECT_EQ(RuleFeatureSet::kSelectorMayMatch, CollectFeatures("#i"));
  ExpectFullRecalcForRuleSetInvalidation(false);
  ClearFeatures();

  EXPECT_EQ(RuleFeatureSet::kSelectorMayMatch, CollectFeatures("#i *"));
  ExpectFullRecalcForRuleSetInvalidation(false);
  ClearFeatures();

  EXPECT_EQ(RuleFeatureSet::kSelectorMayMatch, CollectFeatures("[attr]"));
  ExpectFullRecalcForRuleSetInvalidation(false);
  ClearFeatures();

  EXPECT_EQ(RuleFeatureSet::kSelectorMayMatch, CollectFeatures("[attr] *"));
  ExpectFullRecalcForRuleSetInvalidation(false);
}

TEST_F(RuleFeatureSetTest, RuleSetInvalidationHoverActiveFocus) {
  EXPECT_EQ(RuleFeatureSet::kSelectorMayMatch,
            CollectFeatures(":hover:active:focus"));
  ExpectFullRecalcForRuleSetInvalidation(true);
}

TEST_F(RuleFeatureSetTest, RuleSetInvalidationHostContext) {
  EXPECT_EQ(RuleFeatureSet::kSelectorMayMatch,
            CollectFeatures(":host-context(.x)"));
  ExpectFullRecalcForRuleSetInvalidation(true);
  ClearFeatures();

  EXPECT_EQ(RuleFeatureSet::kSelectorMayMatch,
            CollectFeatures(":host-context(.x) .y"));
  ExpectFullRecalcForRuleSetInvalidation(false);
}

TEST_F(RuleFeatureSetTest, RuleSetInvalidationHost) {
  EXPECT_EQ(RuleFeatureSet::kSelectorMayMatch, CollectFeatures(":host(.x)"));
  ExpectFullRecalcForRuleSetInvalidation(false);
  ClearFeatures();

  EXPECT_EQ(RuleFeatureSet::kSelectorMayMatch, CollectFeatures(":host(*) .y"));
  ExpectFullRecalcForRuleSetInvalidation(false);
  ClearFeatures();

  EXPECT_EQ(RuleFeatureSet::kSelectorMayMatch, CollectFeatures(":host(.x) .y"));
  ExpectFullRecalcForRuleSetInvalidation(false);
}

TEST_F(RuleFeatureSetTest, RuleSetInvalidationNot) {
  EXPECT_EQ(RuleFeatureSet::kSelectorMayMatch, CollectFeatures(":not(.x)"));
  ExpectFullRecalcForRuleSetInvalidation(true);
  ClearFeatures();

  EXPECT_EQ(RuleFeatureSet::kSelectorMayMatch,
            CollectFeatures(":not(.x) :hover"));
  ExpectFullRecalcForRuleSetInvalidation(true);
  ClearFeatures();

  EXPECT_EQ(RuleFeatureSet::kSelectorMayMatch, CollectFeatures(":not(.x) .y"));
  ExpectFullRecalcForRuleSetInvalidation(false);
  ClearFeatures();

  EXPECT_EQ(RuleFeatureSet::kSelectorMayMatch,
            CollectFeatures(":not(.x) + .y"));
  ExpectFullRecalcForRuleSetInvalidation(false);
}

TEST_F(RuleFeatureSetTest, RuleSetInvalidationCustomPseudo) {
  EXPECT_EQ(RuleFeatureSet::kSelectorMayMatch,
            CollectFeatures("::-webkit-slider-thumb"));
  ExpectFullRecalcForRuleSetInvalidation(false);
  ClearFeatures();

  EXPECT_EQ(RuleFeatureSet::kSelectorMayMatch,
            CollectFeatures(".x::-webkit-slider-thumb"));
  ExpectFullRecalcForRuleSetInvalidation(false);
  ClearFeatures();

  EXPECT_EQ(RuleFeatureSet::kSelectorMayMatch,
            CollectFeatures(".x + ::-webkit-slider-thumb"));
  ExpectFullRecalcForRuleSetInvalidation(false);
}

TEST_F(RuleFeatureSetTest, RuleSetInvalidationSlotted) {
  EXPECT_EQ(RuleFeatureSet::kSelectorMayMatch, CollectFeatures("::slotted(*)"));
  ExpectFullRecalcForRuleSetInvalidation(true);
  ClearFeatures();

  EXPECT_EQ(RuleFeatureSet::kSelectorMayMatch,
            CollectFeatures("::slotted(.y)"));
  ExpectFullRecalcForRuleSetInvalidation(false);
  ClearFeatures();

  EXPECT_EQ(RuleFeatureSet::kSelectorMayMatch,
            CollectFeatures(".x::slotted(.y)"));
  ExpectFullRecalcForRuleSetInvalidation(false);
  ClearFeatures();

  EXPECT_EQ(RuleFeatureSet::kSelectorMayMatch,
            CollectFeatures("[x] ::slotted(.y)"));
  ExpectFullRecalcForRuleSetInvalidation(false);
}

TEST_F(RuleFeatureSetTest, RuleSetInvalidationAnyPseudo) {
  EXPECT_EQ(RuleFeatureSet::kSelectorMayMatch,
            CollectFeatures(":-webkit-any(*, #x)"));
  ExpectFullRecalcForRuleSetInvalidation(true);
  ClearFeatures();

  EXPECT_EQ(RuleFeatureSet::kSelectorMayMatch,
            CollectFeatures(".x:-webkit-any(*, #y)"));
  ExpectFullRecalcForRuleSetInvalidation(false);
  ClearFeatures();

  EXPECT_EQ(RuleFeatureSet::kSelectorMayMatch,
            CollectFeatures(":-webkit-any(:-webkit-any(.a, .b), #x)"));
  ExpectFullRecalcForRuleSetInvalidation(false);
  ClearFeatures();

  EXPECT_EQ(RuleFeatureSet::kSelectorMayMatch,
            CollectFeatures(":-webkit-any(:-webkit-any(.a, *), #x)"));
  ExpectFullRecalcForRuleSetInvalidation(true);
  ClearFeatures();

  EXPECT_EQ(RuleFeatureSet::kSelectorMayMatch,
            CollectFeatures(":-webkit-any(*, .a) *"));
  ExpectFullRecalcForRuleSetInvalidation(true);
}

TEST_F(RuleFeatureSetTest, SelfInvalidationSet) {
  EXPECT_EQ(RuleFeatureSet::kSelectorMayMatch, CollectFeatures(".a"));
  EXPECT_EQ(RuleFeatureSet::kSelectorMayMatch, CollectFeatures("div .b"));
  EXPECT_EQ(RuleFeatureSet::kSelectorMayMatch, CollectFeatures("#c"));
  EXPECT_EQ(RuleFeatureSet::kSelectorMayMatch, CollectFeatures("[d]"));
  EXPECT_EQ(RuleFeatureSet::kSelectorMayMatch, CollectFeatures(":hover"));

  InvalidationLists invalidation_lists;
  CollectInvalidationSetsForClass(invalidation_lists, "a");
  ExpectSelfInvalidation(invalidation_lists.descendants);
  ExpectSelfInvalidationSet(invalidation_lists.descendants);

  invalidation_lists.descendants.clear();
  CollectInvalidationSetsForClass(invalidation_lists, "b");
  ExpectSelfInvalidation(invalidation_lists.descendants);
  ExpectSelfInvalidationSet(invalidation_lists.descendants);

  invalidation_lists.descendants.clear();
  CollectInvalidationSetsForId(invalidation_lists, "c");
  ExpectSelfInvalidation(invalidation_lists.descendants);
  ExpectSelfInvalidationSet(invalidation_lists.descendants);

  invalidation_lists.descendants.clear();
  CollectInvalidationSetsForAttribute(invalidation_lists,
                                      QualifiedName("", "d", ""));
  ExpectSelfInvalidation(invalidation_lists.descendants);
  ExpectSelfInvalidationSet(invalidation_lists.descendants);

  invalidation_lists.descendants.clear();
  CollectInvalidationSetsForPseudoClass(invalidation_lists,
                                        CSSSelector::kPseudoHover);
  ExpectSelfInvalidation(invalidation_lists.descendants);
  ExpectSelfInvalidationSet(invalidation_lists.descendants);
}

TEST_F(RuleFeatureSetTest, ReplaceSelfInvalidationSet) {
  EXPECT_EQ(RuleFeatureSet::kSelectorMayMatch, CollectFeatures(".a"));

  InvalidationLists invalidation_lists;
  CollectInvalidationSetsForClass(invalidation_lists, "a");
  ExpectSelfInvalidation(invalidation_lists.descendants);
  ExpectSelfInvalidationSet(invalidation_lists.descendants);

  EXPECT_EQ(RuleFeatureSet::kSelectorMayMatch, CollectFeatures(".a div"));

  invalidation_lists.descendants.clear();
  CollectInvalidationSetsForClass(invalidation_lists, "a");
  ExpectSelfInvalidation(invalidation_lists.descendants);
  ExpectNotSelfInvalidationSet(invalidation_lists.descendants);
}

TEST_F(RuleFeatureSetTest, pseudoIsSibling) {
  EXPECT_EQ(RuleFeatureSet::kSelectorMayMatch,
            CollectFeatures(":is(.q, .r) ~ .s .t"));
  {
    InvalidationLists invalidation_lists;
    CollectInvalidationSetsForClass(invalidation_lists, "q");
    ExpectNoInvalidation(invalidation_lists.descendants);
    ExpectSiblingDescendantInvalidation(
        SiblingInvalidationSet::kDirectAdjacentMax, "s", "t",
        invalidation_lists.siblings);
  }
  {
    InvalidationLists invalidation_lists;
    CollectInvalidationSetsForClass(invalidation_lists, "r");
    ExpectNoInvalidation(invalidation_lists.descendants);
    ExpectSiblingDescendantInvalidation(
        SiblingInvalidationSet::kDirectAdjacentMax, "s", "t",
        invalidation_lists.siblings);
  }
}

TEST_F(RuleFeatureSetTest, pseudoIs) {
  EXPECT_EQ(RuleFeatureSet::kSelectorMayMatch, CollectFeatures(":is(.w, .x)"));

  {
    InvalidationLists invalidation_lists;
    CollectInvalidationSetsForClass(invalidation_lists, "w");
    ExpectSelfInvalidation(invalidation_lists.descendants);
    ExpectNoInvalidation(invalidation_lists.siblings);
  }
  {
    InvalidationLists invalidation_lists;
    CollectInvalidationSetsForClass(invalidation_lists, "x");
    ExpectSelfInvalidation(invalidation_lists.descendants);
    ExpectNoInvalidation(invalidation_lists.siblings);
  }
}

TEST_F(RuleFeatureSetTest, pseudoIsIdDescendant) {
  EXPECT_EQ(RuleFeatureSet::kSelectorMayMatch,
            CollectFeatures(".a :is(#b, #c)"));

  InvalidationLists invalidation_lists;
  CollectInvalidationSetsForClass(invalidation_lists, "a");
  ExpectIdInvalidation("b", "c", invalidation_lists.descendants);
}

TEST_F(RuleFeatureSetTest, pseudoIsTagDescendant) {
  EXPECT_EQ(RuleFeatureSet::kSelectorMayMatch,
            CollectFeatures(".a :is(span, div)"));

  InvalidationLists invalidation_lists;
  CollectInvalidationSetsForClass(invalidation_lists, "a");
  ExpectTagNameInvalidation("span", "div", invalidation_lists.descendants);
}

TEST_F(RuleFeatureSetTest, pseudoIsAnySibling) {
  EXPECT_EQ(RuleFeatureSet::kSelectorMayMatch,
            CollectFeatures(".v ~ :is(.w, .x)"));

  InvalidationLists invalidation_lists;
  CollectInvalidationSetsForClass(invalidation_lists, "v");
  ExpectNoInvalidation(invalidation_lists.descendants);
  ExpectClassInvalidation("w", "x", invalidation_lists.siblings);
}

TEST_F(RuleFeatureSetTest, pseudoIsDescendantSibling) {
  EXPECT_EQ(RuleFeatureSet::kSelectorMayMatch,
            CollectFeatures(".u .v ~ :is(.w, .x)"));

  InvalidationLists invalidation_lists;
  CollectInvalidationSetsForClass(invalidation_lists, "u");
  ExpectClassInvalidation("w", "x", invalidation_lists.descendants);
  ExpectNoInvalidation(invalidation_lists.siblings);
}

TEST_F(RuleFeatureSetTest, pseudoIsWithComplexSelectors) {
  EXPECT_EQ(RuleFeatureSet::kSelectorMayMatch,
            CollectFeatures(".a :is(.w+.b, .x>#c)"));

  InvalidationLists invalidation_lists;
  CollectInvalidationSetsForClass(invalidation_lists, "a");
  ExpectClassInvalidation("b", invalidation_lists.descendants);
  ExpectIdInvalidation("c", invalidation_lists.descendants);
  ExpectNoInvalidation(invalidation_lists.siblings);
}

TEST_F(RuleFeatureSetTest, pseudoIsNested) {
  EXPECT_EQ(RuleFeatureSet::kSelectorMayMatch,
            CollectFeatures(".a :is(.w+.b, .e+:is(.c, #d))"));

  InvalidationLists invalidation_lists;
  CollectInvalidationSetsForClass(invalidation_lists, "a");
  ExpectClassInvalidation("b", "c", invalidation_lists.descendants);
  ExpectIdInvalidation("d", invalidation_lists.descendants);
  ExpectNoInvalidation(invalidation_lists.siblings);
}

TEST_F(RuleFeatureSetTest, pseudoIsTooLarge) {
  // RuleData cannot support selectors at index 8192 or beyond so the expansion
  // is limited to this size
  EXPECT_EQ(RuleFeatureSet::kSelectorNeverMatches,
            CollectFeatures(":is(.a#a, .b#b, .c#c, .d#d) + "
                            ":is(.e#e, .f#f, .g#g, .h#h) + "
                            ":is(.i#i, .j#j, .k#k, .l#l) + "
                            ":is(.m#m, .n#n, .o#o, .p#p) + "
                            ":is(.q#q, .r#r, .s#s, .t#t) + "
                            ":is(.u#u, .v#v, .w#w, .x#x)"));

  InvalidationLists invalidation_lists;
  CollectInvalidationSetsForClass(invalidation_lists, "a");
  ExpectNoInvalidation(invalidation_lists.descendants);
  ExpectNoInvalidation(invalidation_lists.siblings);
}

TEST_F(RuleFeatureSetTest, pseudoWhere) {
  EXPECT_EQ(RuleFeatureSet::kSelectorMayMatch,
            CollectFeatures(":where(.w, .x)"));

  {
    InvalidationLists invalidation_lists;
    CollectInvalidationSetsForClass(invalidation_lists, "w");
    ExpectSelfInvalidation(invalidation_lists.descendants);
    ExpectNoInvalidation(invalidation_lists.siblings);
  }
  {
    InvalidationLists invalidation_lists;
    CollectInvalidationSetsForClass(invalidation_lists, "x");
    ExpectSelfInvalidation(invalidation_lists.descendants);
    ExpectNoInvalidation(invalidation_lists.siblings);
  }
}

TEST_F(RuleFeatureSetTest, pseudoWhereSibling) {
  EXPECT_EQ(RuleFeatureSet::kSelectorMayMatch,
            CollectFeatures(":where(.q, .r) ~ .s .t"));
  {
    InvalidationLists invalidation_lists;
    CollectInvalidationSetsForClass(invalidation_lists, "q");
    ExpectNoInvalidation(invalidation_lists.descendants);
    ExpectSiblingDescendantInvalidation(
        SiblingInvalidationSet::kDirectAdjacentMax, "s", "t",
        invalidation_lists.siblings);
  }
  {
    InvalidationLists invalidation_lists;
    CollectInvalidationSetsForClass(invalidation_lists, "r");
    ExpectNoInvalidation(invalidation_lists.descendants);
    ExpectSiblingDescendantInvalidation(
        SiblingInvalidationSet::kDirectAdjacentMax, "s", "t",
        invalidation_lists.siblings);
  }
}

TEST_F(RuleFeatureSetTest, pseudoWhereIdDescendant) {
  EXPECT_EQ(RuleFeatureSet::kSelectorMayMatch,
            CollectFeatures(".a :where(#b, #c)"));

  InvalidationLists invalidation_lists;
  CollectInvalidationSetsForClass(invalidation_lists, "a");
  ExpectIdInvalidation("b", "c", invalidation_lists.descendants);
}

TEST_F(RuleFeatureSetTest, pseudoWhereTagDescendant) {
  EXPECT_EQ(RuleFeatureSet::kSelectorMayMatch,
            CollectFeatures(".a :where(span, div)"));

  InvalidationLists invalidation_lists;
  CollectInvalidationSetsForClass(invalidation_lists, "a");
  ExpectTagNameInvalidation("span", "div", invalidation_lists.descendants);
}

TEST_F(RuleFeatureSetTest, pseudoWhereAnySibling) {
  EXPECT_EQ(RuleFeatureSet::kSelectorMayMatch,
            CollectFeatures(".v ~ :where(.w, .x)"));

  InvalidationLists invalidation_lists;
  CollectInvalidationSetsForClass(invalidation_lists, "v");
  ExpectNoInvalidation(invalidation_lists.descendants);
  ExpectClassInvalidation("w", "x", invalidation_lists.siblings);
}

TEST_F(RuleFeatureSetTest, pseudoWhereDescendantSibling) {
  EXPECT_EQ(RuleFeatureSet::kSelectorMayMatch,
            CollectFeatures(".u .v ~ :where(.w, .x)"));

  InvalidationLists invalidation_lists;
  CollectInvalidationSetsForClass(invalidation_lists, "u");
  ExpectClassInvalidation("w", "x", invalidation_lists.descendants);
  ExpectNoInvalidation(invalidation_lists.siblings);
}

TEST_F(RuleFeatureSetTest, pseudoWhereWithComplexSelectors) {
  EXPECT_EQ(RuleFeatureSet::kSelectorMayMatch,
            CollectFeatures(".a :where(.w+.b, .x>#c)"));

  InvalidationLists invalidation_lists;
  CollectInvalidationSetsForClass(invalidation_lists, "a");
  ExpectClassInvalidation("b", invalidation_lists.descendants);
  ExpectIdInvalidation("c", invalidation_lists.descendants);
  ExpectNoInvalidation(invalidation_lists.siblings);
}

TEST_F(RuleFeatureSetTest, pseudoWhereNested) {
  EXPECT_EQ(RuleFeatureSet::kSelectorMayMatch,
            CollectFeatures(".a :where(.w+.b, .e+:where(.c, #d))"));

  InvalidationLists invalidation_lists;
  CollectInvalidationSetsForClass(invalidation_lists, "a");
  ExpectClassInvalidation("b", "c", invalidation_lists.descendants);
  ExpectIdInvalidation("d", invalidation_lists.descendants);
  ExpectNoInvalidation(invalidation_lists.siblings);
}

TEST_F(RuleFeatureSetTest, pseudoWhereTooLarge) {
  // RuleData cannot support selectors at index 8192 or beyond so the expansion
  // is limited to this size
  EXPECT_EQ(RuleFeatureSet::kSelectorNeverMatches,
            CollectFeatures(":where(.a#a, .b#b, .c#c, .d#d) + "
                            ":where(.e#e, .f#f, .g#g, .h#h) + "
                            ":where(.i#i, .j#j, .k#k, .l#l) + "
                            ":where(.m#m, .n#n, .o#o, .p#p) + "
                            ":where(.q#q, .r#r, .s#s, .t#t) + "
                            ":where(.u#u, .v#v, .w#w, .x#x)"));

  InvalidationLists invalidation_lists;
  CollectInvalidationSetsForClass(invalidation_lists, "a");
  ExpectNoInvalidation(invalidation_lists.descendants);
  ExpectNoInvalidation(invalidation_lists.siblings);
}

TEST_F(RuleFeatureSetTest, invalidatesParts) {
  EXPECT_EQ(RuleFeatureSet::kSelectorMayMatch,
            CollectFeatures(".a .b::part(partname)"));

  {
    InvalidationLists invalidation_lists;
    CollectInvalidationSetsForClass(invalidation_lists, "a");
    EXPECT_EQ(1u, invalidation_lists.descendants.size());
    ExpectNoSelfInvalidation(invalidation_lists.descendants);
    EXPECT_TRUE(invalidation_lists.descendants[0]->TreeBoundaryCrossing());
    EXPECT_TRUE(invalidation_lists.descendants[0]->InvalidatesParts());
  }

  {
    InvalidationLists invalidation_lists;
    CollectInvalidationSetsForClass(invalidation_lists, "b");
    EXPECT_EQ(1u, invalidation_lists.descendants.size());
    ExpectPartsInvalidation(invalidation_lists.descendants);
    EXPECT_FALSE(invalidation_lists.descendants[0]->WholeSubtreeInvalid());
    EXPECT_TRUE(invalidation_lists.descendants[0]->TreeBoundaryCrossing());
    EXPECT_TRUE(invalidation_lists.descendants[0]->InvalidatesParts());
  }

  {
    InvalidationLists invalidation_lists;
    CollectPartInvalidationSet(invalidation_lists);
    EXPECT_EQ(1u, invalidation_lists.descendants.size());
    ExpectPartsInvalidation(invalidation_lists.descendants);
    EXPECT_TRUE(invalidation_lists.descendants[0]->TreeBoundaryCrossing());
    EXPECT_TRUE(invalidation_lists.descendants[0]->InvalidatesParts());
  }
}

TEST_F(RuleFeatureSetTest, CopyOnWrite) {
  // RuleFeatureSet local1 has an entry in each of the class/id/attribute/
  // pseudo sets.
  RuleFeatureSet local1;
  CollectFeatures(".a .b");
  CollectFeatures("#d .e");
  CollectFeatures("[thing] .f");
  CollectFeatures(":hover .h");
  AddTo(local1);
  ClearFeatures();
  ExpectRefCountForClassInvalidationSet(local1, "a", RefCount::kOne);
  ExpectRefCountForIdInvalidationSet(local1, "d", RefCount::kOne);
  ExpectRefCountForAttributeInvalidationSet(local1, "thing", RefCount::kOne);
  ExpectRefCountForPseudoInvalidationSet(local1, CSSSelector::kPseudoHover,
                                         RefCount::kOne);

  // RuleFeatureSet local2 overlaps partially with local1.
  RuleFeatureSet local2;
  CollectFeatures(".a .c");
  CollectFeatures("#d img");
  AddTo(local2);
  ClearFeatures();
  ExpectRefCountForClassInvalidationSet(local2, "a", RefCount::kOne);
  ExpectRefCountForIdInvalidationSet(local2, "d", RefCount::kOne);

  // RuleFeatureSet local3 overlaps partially with local1, but not with local2.
  RuleFeatureSet local3;
  CollectFeatures("[thing] .g");
  CollectFeatures(":hover .i");
  AddTo(local3);
  ClearFeatures();
  ExpectRefCountForAttributeInvalidationSet(local3, "thing", RefCount::kOne);
  ExpectRefCountForPseudoInvalidationSet(local3, CSSSelector::kPseudoHover,
                                         RefCount::kOne);

  // Using an empty RuleFeatureSet to simulate the global RuleFeatureSet:
  RuleFeatureSet global;

  // After adding local1, we expect to share the InvalidationSets with local1.
  global.Add(local1);
  ExpectRefCountForClassInvalidationSet(global, "a", RefCount::kMany);
  ExpectRefCountForIdInvalidationSet(global, "d", RefCount::kMany);
  ExpectRefCountForAttributeInvalidationSet(global, "thing", RefCount::kMany);
  ExpectRefCountForPseudoInvalidationSet(global, CSSSelector::kPseudoHover,
                                         RefCount::kMany);

  // For the InvalidationSet keys that overlap with local1, |global| now had to
  // copy the existing InvalidationSets at those keys before modifying them,
  // so we expect |global| to be the only reference holder to those
  // InvalidationSets.
  global.Add(local2);
  ExpectRefCountForClassInvalidationSet(global, "a", RefCount::kOne);
  ExpectRefCountForIdInvalidationSet(global, "d", RefCount::kOne);
  ExpectRefCountForAttributeInvalidationSet(global, "thing", RefCount::kMany);
  ExpectRefCountForPseudoInvalidationSet(global, CSSSelector::kPseudoHover,
                                         RefCount::kMany);

  global.Add(local3);
  ExpectRefCountForClassInvalidationSet(global, "a", RefCount::kOne);
  ExpectRefCountForIdInvalidationSet(global, "d", RefCount::kOne);
  ExpectRefCountForAttributeInvalidationSet(global, "thing", RefCount::kOne);
  ExpectRefCountForPseudoInvalidationSet(global, CSSSelector::kPseudoHover,
                                         RefCount::kOne);
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
      AddTo(local1);
      ClearFeatures();

      RuleFeatureSet local2;
      CollectFeatures(selector2);
      AddTo(local2);
      ClearFeatures();

      RuleFeatureSet global;
      global.Add(local1);
      ExpectRefCountForClassInvalidationSet(global, "a", RefCount::kMany);
      global.Add(local2);
      ExpectRefCountForClassInvalidationSet(global, "a", RefCount::kOne);
    }
  }
}

TEST_F(RuleFeatureSetTest, CopyOnWrite_SelfInvalidation) {
  RuleFeatureSet local1;
  CollectFeatures(".a");
  AddTo(local1);
  ClearFeatures();

  RuleFeatureSet local2;
  CollectFeatures(".a");
  AddTo(local2);
  ClearFeatures();

  // Adding the SelfInvalidationSet to the SelfInvalidationSet does not cause
  // a copy.
  RuleFeatureSet global;
  global.Add(local1);
  ExpectRefCountForClassInvalidationSet(global, "a", RefCount::kMany);
  global.Add(local2);
  ExpectRefCountForClassInvalidationSet(global, "a", RefCount::kMany);
}

}  // namespace blink
