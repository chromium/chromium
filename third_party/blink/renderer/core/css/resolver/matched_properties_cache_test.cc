// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/resolver/matched_properties_cache.h"

#include "third_party/blink/renderer/core/css/css_property_name.h"
#include "third_party/blink/renderer/core/css/css_test_helpers.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

namespace blink {

using css_test_helpers::CreateVariableData;

class MatchedPropertiesCacheTestKey {
  STACK_ALLOCATED();

 public:
  explicit MatchedPropertiesCacheTestKey(String block_text,
                                         unsigned hash,
                                         const TreeScope& tree_scope)
      : key_(ParseBlock(block_text, tree_scope), hash) {}

  const MatchedPropertiesCache::Key& InnerKey() const { return key_; }

 private:
  const MatchResult& ParseBlock(String block_text,
                                const TreeScope& tree_scope) {
    result_.FinishAddingUARules();
    result_.FinishAddingUserRules();
    auto* set = css_test_helpers::ParseDeclarationBlock(block_text);
    result_.AddMatchedProperties(set);
    result_.FinishAddingAuthorRulesForTreeScope(tree_scope);
    return result_;
  }

  MatchResult result_;
  MatchedPropertiesCache::Key key_;
};

using TestKey = MatchedPropertiesCacheTestKey;

class MatchedPropertiesCacheTestCache {
  STACK_ALLOCATED();

 public:
  explicit MatchedPropertiesCacheTestCache(Document& document)
      : document_(document) {}

  ~MatchedPropertiesCacheTestCache() {
    // Required by DCHECK in ~MatchedPropertiesCache.
    cache_.Clear();
  }

  void Add(const TestKey& key,
           const ComputedStyle& style,
           const ComputedStyle& parent_style,
           const Vector<String>& dependencies = Vector<String>()) {
    HashSet<CSSPropertyName> set;
    for (String name_string : dependencies) {
      set.insert(
          *CSSPropertyName::From(document_.GetExecutionContext(), name_string));
    }
    cache_.Add(key.InnerKey(), style, parent_style, set);
  }

  const CachedMatchedProperties* Find(const TestKey& key,
                                      const ComputedStyle& style,
                                      const ComputedStyle& parent_style) {
    StyleResolverState state(document_, *document_.body(), &parent_style,
                             &parent_style);
    state.SetStyle(ComputedStyle::Clone(style));
    return cache_.Find(key.InnerKey(), state);
  }

 private:
  MatchedPropertiesCache cache_;
  Document& document_;
};

using TestCache = MatchedPropertiesCacheTestCache;

class MatchedPropertiesCacheTest
    : public PageTestBase,
      private ScopedCSSMatchedPropertiesCacheDependenciesForTest {
 public:
  MatchedPropertiesCacheTest()
      : ScopedCSSMatchedPropertiesCacheDependenciesForTest(true) {}

  scoped_refptr<ComputedStyle> CreateStyle() {
    return StyleResolver::InitialStyleForElement(GetDocument());
  }
};

TEST_F(MatchedPropertiesCacheTest, ClearEntry) {
  MatchResult result;
  result.AddMatchedProperties(
      css_test_helpers::ParseDeclarationBlock("top:inherit"));

  auto style = CreateStyle();
  auto parent = CreateStyle();

  HashSet<CSSPropertyName> dependencies;
  dependencies.insert(CSSPropertyName(CSSPropertyID::kTop));

  auto* entry = MakeGarbageCollected<CachedMatchedProperties>();
  entry->Set(*style, *parent, result.GetMatchedProperties(), dependencies);

  EXPECT_TRUE(entry->computed_style);
  EXPECT_TRUE(entry->parent_computed_style);
  EXPECT_FALSE(entry->matched_properties.IsEmpty());
  EXPECT_FALSE(entry->matched_properties_types.IsEmpty());
  EXPECT_TRUE(entry->dependencies);

  entry->Clear();

  EXPECT_FALSE(entry->computed_style);
  EXPECT_FALSE(entry->parent_computed_style);
  EXPECT_TRUE(entry->matched_properties.IsEmpty());
  EXPECT_TRUE(entry->matched_properties_types.IsEmpty());
  EXPECT_FALSE(entry->dependencies);
}

TEST_F(MatchedPropertiesCacheTest, NoDependencies) {
  MatchResult result;
  auto style = CreateStyle();
  auto parent = CreateStyle();

  HashSet<CSSPropertyName> dependencies;

  auto* entry = MakeGarbageCollected<CachedMatchedProperties>();
  entry->Set(*style, *parent, result.GetMatchedProperties(), dependencies);

  EXPECT_FALSE(entry->dependencies);
}

TEST_F(MatchedPropertiesCacheTest, OneDependency) {
  MatchResult result;
  auto style = CreateStyle();
  auto parent = CreateStyle();

  HashSet<CSSPropertyName> dependencies;
  dependencies.insert(CSSPropertyName(CSSPropertyID::kTop));

  auto* entry = MakeGarbageCollected<CachedMatchedProperties>();
  entry->Set(*style, *parent, result.GetMatchedProperties(), dependencies);

  ASSERT_TRUE(entry->dependencies);
  EXPECT_EQ("top", entry->dependencies[0]);
  EXPECT_EQ(g_null_atom, entry->dependencies[1]);
}

TEST_F(MatchedPropertiesCacheTest, TwoDependencies) {
  MatchResult result;
  auto style = CreateStyle();
  auto parent = CreateStyle();

  HashSet<CSSPropertyName> dependencies;
  dependencies.insert(CSSPropertyName(CSSPropertyID::kTop));
  dependencies.insert(CSSPropertyName(CSSPropertyID::kLeft));

  auto* entry = MakeGarbageCollected<CachedMatchedProperties>();
  entry->Set(*style, *parent, result.GetMatchedProperties(), dependencies);

  ASSERT_TRUE(entry->dependencies);
  EXPECT_TRUE(entry->dependencies[0] == "top" ||
              entry->dependencies[0] == "left");
  EXPECT_TRUE(entry->dependencies[1] == "top" ||
              entry->dependencies[1] == "left");
  EXPECT_NE(entry->dependencies[0], entry->dependencies[1]);
  EXPECT_TRUE(entry->dependencies[2] == g_null_atom);
}

TEST_F(MatchedPropertiesCacheTest, AllowedKeyValues) {
  unsigned empty = HashTraits<unsigned>::EmptyValue();
  unsigned deleted = std::numeric_limits<unsigned>::max();

  ASSERT_EQ(0u, HashTraits<unsigned>::EmptyValue());
  ASSERT_TRUE(HashTraits<unsigned>::IsDeletedValue(deleted));

  EXPECT_FALSE(TestKey("left:0", empty, GetDocument()).InnerKey().IsValid());
  EXPECT_TRUE(TestKey("left:0", empty + 1, GetDocument()).InnerKey().IsValid());
  EXPECT_TRUE(
      TestKey("left:0", deleted - 1, GetDocument()).InnerKey().IsValid());
  EXPECT_FALSE(TestKey("left:0", deleted, GetDocument()).InnerKey().IsValid());
}

TEST_F(MatchedPropertiesCacheTest, InvalidKeyForUncacheableMatchResult) {
  MatchResult result;
  result.SetIsCacheable(false);
  EXPECT_FALSE(MatchedPropertiesCache::Key(result).IsValid());
}

TEST_F(MatchedPropertiesCacheTest, Miss) {
  TestCache cache(GetDocument());
  TestKey key("color:red", 1, GetDocument());

  auto style = CreateStyle();
  auto parent = CreateStyle();

  EXPECT_FALSE(cache.Find(key, *style, *parent));
}

TEST_F(MatchedPropertiesCacheTest, Hit) {
  TestCache cache(GetDocument());
  TestKey key("color:red", 1, GetDocument());

  auto style = CreateStyle();
  auto parent = CreateStyle();

  cache.Add(key, *style, *parent);
  EXPECT_TRUE(cache.Find(key, *style, *parent));
}

TEST_F(MatchedPropertiesCacheTest, HitOnlyForAddedEntry) {
  TestCache cache(GetDocument());

  auto style = CreateStyle();
  auto parent = CreateStyle();

  TestKey key1("color:red", 1, GetDocument());
  TestKey key2("display:block", 2, GetDocument());

  cache.Add(key1, *style, *parent);

  EXPECT_TRUE(cache.Find(key1, *style, *parent));
  EXPECT_FALSE(cache.Find(key2, *style, *parent));
}

TEST_F(MatchedPropertiesCacheTest, HitWithStandardDependency) {
  TestCache cache(GetDocument());

  auto style = CreateStyle();
  auto parent = CreateStyle();

  TestKey key("top:inherit", 1, GetDocument());

  cache.Add(key, *style, *parent, Vector<String>{"top"});
  EXPECT_TRUE(cache.Find(key, *style, *parent));
}

TEST_F(MatchedPropertiesCacheTest, MissWithStandardDependency) {
  TestCache cache(GetDocument());

  auto style = CreateStyle();

  auto parent1 = CreateStyle();
  parent1->SetTop(Length(1, Length::kFixed));

  auto parent2 = CreateStyle();
  parent2->SetTop(Length(2, Length::kFixed));

  TestKey key("top:inherit", 1, GetDocument());
  cache.Add(key, *style, *parent1, Vector<String>{"top"});
  EXPECT_TRUE(cache.Find(key, *style, *parent1));
  EXPECT_FALSE(cache.Find(key, *style, *parent2));
}

TEST_F(MatchedPropertiesCacheTest, HitWithCustomDependency) {
  TestCache cache(GetDocument());

  auto style = CreateStyle();

  auto parent = CreateStyle();
  parent->SetVariableData("--x", CreateVariableData("1px"), true);

  TestKey key("top:var(--x)", 1, GetDocument());

  cache.Add(key, *style, *parent, Vector<String>{"--x"});
  EXPECT_TRUE(cache.Find(key, *style, *parent));
}

TEST_F(MatchedPropertiesCacheTest, MissWithCustomDependency) {
  TestCache cache(GetDocument());

  auto style = CreateStyle();

  auto parent1 = CreateStyle();
  parent1->SetVariableData("--x", CreateVariableData("1px"), true);

  auto parent2 = CreateStyle();
  parent2->SetVariableData("--x", CreateVariableData("2px"), true);

  TestKey key("top:var(--x)", 1, GetDocument());

  cache.Add(key, *style, *parent1, Vector<String>{"--x"});
  EXPECT_FALSE(cache.Find(key, *style, *parent2));
}

TEST_F(MatchedPropertiesCacheTest, HitWithMultipleCustomDependencies) {
  TestCache cache(GetDocument());

  auto style = CreateStyle();

  auto parent1 = CreateStyle();
  parent1->SetVariableData("--x", CreateVariableData("1px"), true);
  parent1->SetVariableData("--y", CreateVariableData("2px"), true);
  parent1->SetVariableData("--z", CreateVariableData("3px"), true);

  auto parent2 = ComputedStyle::Clone(*parent1);
  parent2->SetVariableData("--z", CreateVariableData("4px"), true);

  TestKey key("top:var(--x);left:var(--y)", 1, GetDocument());

  // Does not depend on --z, so doesn't matter that --z changed.
  cache.Add(key, *style, *parent1, Vector<String>{"--x", "--y"});
  EXPECT_TRUE(cache.Find(key, *style, *parent2));
}

TEST_F(MatchedPropertiesCacheTest, MissWithMultipleCustomDependencies) {
  TestCache cache(GetDocument());

  auto style = CreateStyle();

  auto parent1 = CreateStyle();
  parent1->SetVariableData("--x", CreateVariableData("1px"), true);
  parent1->SetVariableData("--y", CreateVariableData("2px"), true);

  auto parent2 = ComputedStyle::Clone(*parent1);
  parent2->SetVariableData("--y", CreateVariableData("3px"), true);

  TestKey key("top:var(--x);left:var(--y)", 1, GetDocument());

  cache.Add(key, *style, *parent1, Vector<String>{"--x", "--y"});
  EXPECT_FALSE(cache.Find(key, *style, *parent2));
}

TEST_F(MatchedPropertiesCacheTest, HitWithMixedDependencies) {
  TestCache cache(GetDocument());

  auto style = CreateStyle();

  auto parent1 = CreateStyle();
  parent1->SetVariableData("--x", CreateVariableData("1px"), true);
  parent1->SetVariableData("--y", CreateVariableData("2px"), true);
  parent1->SetLeft(Length(3, Length::kFixed));
  parent1->SetRight(Length(4, Length::kFixed));

  auto parent2 = ComputedStyle::Clone(*parent1);
  parent2->SetVariableData("--y", CreateVariableData("5px"), true);
  parent2->SetRight(Length(6, Length::kFixed));

  TestKey key("left:inherit;top:var(--x)", 1, GetDocument());

  cache.Add(key, *style, *parent1, Vector<String>{"left", "--x"});
  EXPECT_TRUE(cache.Find(key, *style, *parent2));
}

TEST_F(MatchedPropertiesCacheTest, ExplicitlyInheritedCacheable) {
  ASSERT_TRUE(GetCSSPropertyVerticalAlign().IsComputedValueComparable());

  TestCache cache(GetDocument());

  auto style = CreateStyle();
  auto parent = CreateStyle();
  parent->SetChildHasExplicitInheritance();

  StyleResolverState state(GetDocument(), *GetDocument().body(), parent.get(),
                           parent.get());
  state.SetStyle(style);
  // Simulate explicit inheritance on vertical-align.
  state.MarkDependency(GetCSSPropertyVerticalAlign());

  EXPECT_TRUE(MatchedPropertiesCache::IsCacheable(state));
}

TEST_F(MatchedPropertiesCacheTest, NotCacheableWithIncomparableDependency) {
  const CSSProperty& incomparable = GetCSSPropertyInternalEmptyLineHeight();
  ASSERT_FALSE(incomparable.IsComputedValueComparable());

  TestCache cache(GetDocument());

  auto style = CreateStyle();
  auto parent = CreateStyle();
  parent->SetChildHasExplicitInheritance();

  StyleResolverState state(GetDocument(), *GetDocument().body(), parent.get(),
                           parent.get());
  state.SetStyle(style);
  // Simulate explicit inheritance on the incomparable property.
  state.MarkDependency(incomparable);

  EXPECT_FALSE(MatchedPropertiesCache::IsCacheable(state));
}

TEST_F(MatchedPropertiesCacheTest, WritingModeCacheable) {
  ASSERT_NE(WritingMode::kVerticalRl,
            ComputedStyleInitialValues::InitialWritingMode());

  TestCache cache(GetDocument());

  auto style = CreateStyle();
  auto parent = CreateStyle();
  style->SetWritingMode(WritingMode::kVerticalRl);

  StyleResolverState state(GetDocument(), *GetDocument().body(), parent.get(),
                           parent.get());
  state.SetStyle(style);

  EXPECT_TRUE(MatchedPropertiesCache::IsCacheable(state));
}

TEST_F(MatchedPropertiesCacheTest, DirectionCacheable) {
  ASSERT_NE(TextDirection::kRtl,
            ComputedStyleInitialValues::InitialDirection());

  TestCache cache(GetDocument());

  auto style = CreateStyle();
  auto parent = CreateStyle();
  style->SetDirection(TextDirection::kRtl);

  StyleResolverState state(GetDocument(), *GetDocument().body(), parent.get(),
                           parent.get());
  state.SetStyle(style);

  EXPECT_TRUE(MatchedPropertiesCache::IsCacheable(state));
}

TEST_F(MatchedPropertiesCacheTest, VarInNonInheritedPropertyCachable) {
  TestCache cache(GetDocument());

  auto style = CreateStyle();
  // Simulate non-inherited-property: var(--my-prop)
  style->SetHasVariableReferenceFromNonInheritedProperty();

  auto parent = CreateStyle();

  StyleResolverState state(GetDocument(), *GetDocument().body(), parent.get(),
                           parent.get());
  state.SetStyle(style);

  EXPECT_TRUE(MatchedPropertiesCache::IsCacheable(state));
}

TEST_F(MatchedPropertiesCacheTest, MaxDependencies) {
  TestCache cache(GetDocument());

  auto style = CreateStyle();
  auto parent = CreateStyle();

  StyleResolverState state(GetDocument(), *GetDocument().body(), parent.get(),
                           parent.get());
  state.SetStyle(style);
  for (size_t i = 0; i < StyleResolverState::kMaxDependencies; i++) {
    CustomProperty property(AtomicString(String::Format("--x%zu", i)),
                            GetDocument());
    state.MarkDependency(property);
    EXPECT_TRUE(MatchedPropertiesCache::IsCacheable(state));
  }
  CustomProperty property("--y", GetDocument());
  state.MarkDependency(property);
  // Limit exceeded.
  EXPECT_FALSE(MatchedPropertiesCache::IsCacheable(state));
}

TEST_F(MatchedPropertiesCacheTest,
       ExplicitlyInheritedNotCacheableWithoutFeature) {
  ScopedCSSMatchedPropertiesCacheDependenciesForTest scoped_feature(false);

  TestCache cache(GetDocument());

  auto style = CreateStyle();
  auto parent = CreateStyle();
  parent->SetChildHasExplicitInheritance();

  StyleResolverState state(GetDocument(), *GetDocument().body(), parent.get(),
                           parent.get());
  state.SetStyle(style);

  EXPECT_FALSE(MatchedPropertiesCache::IsCacheable(state));
}

TEST_F(MatchedPropertiesCacheTest,
       VarInNonInheritedPropertyNotCachableWithoutFeature) {
  ScopedCSSMatchedPropertiesCacheDependenciesForTest scoped_feature(false);

  TestCache cache(GetDocument());

  auto style = CreateStyle();
  auto parent = CreateStyle();
  // Simulate non-inherited-property: var(--my-prop)
  style->SetHasVariableReferenceFromNonInheritedProperty();

  StyleResolverState state(GetDocument(), *GetDocument().body(), parent.get(),
                           parent.get());
  state.SetStyle(style);

  EXPECT_FALSE(MatchedPropertiesCache::IsCacheable(state));
}

TEST_F(MatchedPropertiesCacheTest, WritingModeNotCacheableWithoutFeature) {
  ScopedCSSMatchedPropertiesCacheDependenciesForTest scoped_feature(false);

  ASSERT_NE(WritingMode::kVerticalRl,
            ComputedStyleInitialValues::InitialWritingMode());

  TestCache cache(GetDocument());

  auto style = CreateStyle();
  auto parent = CreateStyle();
  style->SetWritingMode(WritingMode::kVerticalRl);

  StyleResolverState state(GetDocument(), *GetDocument().body(), parent.get(),
                           parent.get());
  state.SetStyle(style);

  EXPECT_FALSE(MatchedPropertiesCache::IsCacheable(state));
}

TEST_F(MatchedPropertiesCacheTest, DirectionNotCacheableWithoutFeature) {
  ScopedCSSMatchedPropertiesCacheDependenciesForTest scoped_feature(false);

  ASSERT_NE(TextDirection::kRtl,
            ComputedStyleInitialValues::InitialDirection());

  TestCache cache(GetDocument());

  auto style = CreateStyle();
  auto parent = CreateStyle();
  style->SetDirection(TextDirection::kRtl);

  StyleResolverState state(GetDocument(), *GetDocument().body(), parent.get(),
                           parent.get());
  state.SetStyle(style);

  EXPECT_FALSE(MatchedPropertiesCache::IsCacheable(state));
}

TEST_F(MatchedPropertiesCacheTest, EnsuredInDisplayNone) {
  TestCache cache(GetDocument());

  auto style = CreateStyle();
  auto parent = CreateStyle();
  auto ensured_parent = CreateStyle();
  ensured_parent->SetIsEnsuredInDisplayNone();

  TestKey key1("display:block", 1, GetDocument());

  cache.Add(key1, *style, *parent);
  EXPECT_TRUE(cache.Find(key1, *style, *parent));
  EXPECT_TRUE(cache.Find(key1, *style, *ensured_parent));

  cache.Add(key1, *style, *ensured_parent);
  EXPECT_FALSE(cache.Find(key1, *style, *parent));
  EXPECT_TRUE(cache.Find(key1, *style, *ensured_parent));
}

TEST_F(MatchedPropertiesCacheTest, EnsuredOutsideFlatTree) {
  TestCache cache(GetDocument());

  auto style = CreateStyle();
  auto parent = CreateStyle();
  auto ensured_style = CreateStyle();
  ensured_style->SetIsEnsuredOutsideFlatTree();

  TestKey key1("display:block", 1, GetDocument());

  cache.Add(key1, *style, *parent);
  EXPECT_TRUE(cache.Find(key1, *style, *parent));
  EXPECT_TRUE(cache.Find(key1, *ensured_style, *parent));

  cache.Add(key1, *ensured_style, *parent);
  EXPECT_FALSE(cache.Find(key1, *style, *parent));
  EXPECT_TRUE(cache.Find(key1, *ensured_style, *parent));
}

TEST_F(MatchedPropertiesCacheTest, EnsuredOutsideFlatTreeAndDisplayNone) {
  TestCache cache(GetDocument());

  auto parent = CreateStyle();
  auto parent_none = CreateStyle();
  auto style = CreateStyle();
  auto style_flat = CreateStyle();
  parent_none->SetIsEnsuredInDisplayNone();
  style_flat->SetIsEnsuredOutsideFlatTree();

  TestKey key1("display:block", 1, GetDocument());

  cache.Add(key1, *style, *parent_none);
  EXPECT_TRUE(cache.Find(key1, *style_flat, *parent));

  cache.Add(key1, *style_flat, *parent);
  EXPECT_TRUE(cache.Find(key1, *style, *parent_none));
}

}  // namespace blink
