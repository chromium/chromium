// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/resolver/matched_properties_cache.h"

#include "third_party/blink/renderer/core/css/css_property_name.h"
#include "third_party/blink/renderer/core/css/css_test_helpers.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver_state.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"

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
    result_.FinishAddingPresentationalHints();
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
           const ComputedStyle& parent_style) {
    cache_.Add(key.InnerKey(), ComputedStyle::Clone(style),
               ComputedStyle::Clone(parent_style));
  }

  const CachedMatchedProperties* Find(const TestKey& key,
                                      const ComputedStyle& style,
                                      const ComputedStyle& parent_style) {
    StyleResolverState state(document_, *document_.body(),
                             nullptr /* StyleRecalcContext */,
                             StyleRequest(&parent_style));
    state.SetStyle(ComputedStyle::Clone(style));
    return cache_.Find(key.InnerKey(), state);
  }

 private:
  MatchedPropertiesCache cache_;
  Document& document_;
};

using TestCache = MatchedPropertiesCacheTestCache;

class MatchedPropertiesCacheTest : public PageTestBase {
 public:
  scoped_refptr<ComputedStyle> CreateStyle() {
    return GetDocument().GetStyleResolver().CreateComputedStyle();
  }
  ComputedStyleBuilder CreateStyleBuilder() {
    return GetDocument().GetStyleResolver().CreateComputedStyleBuilder();
  }
};

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
  auto builder = CreateStyleBuilder();
  builder.SetIsEnsuredOutsideFlatTree();
  auto ensured_style = builder.TakeStyle();

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
  auto style = CreateStyle();

  auto builder = CreateStyleBuilder();
  builder.SetIsEnsuredInDisplayNone();
  auto parent_none = builder.TakeStyle();

  builder = CreateStyleBuilder();
  builder.SetIsEnsuredOutsideFlatTree();
  auto style_flat = builder.TakeStyle();

  TestKey key1("display:block", 1, GetDocument());

  cache.Add(key1, *style, *parent_none);
  EXPECT_TRUE(cache.Find(key1, *style_flat, *parent));

  cache.Add(key1, *style_flat, *parent);
  EXPECT_TRUE(cache.Find(key1, *style, *parent_none));
}

TEST_F(MatchedPropertiesCacheTest, WritingModeDependency) {
  TestCache cache(GetDocument());

  auto parent_builder_a = CreateStyleBuilder();
  parent_builder_a.SetWritingMode(WritingMode::kHorizontalTb);
  auto parent_builder_b = CreateStyleBuilder();
  parent_builder_b.SetWritingMode(WritingMode::kVerticalRl);

  auto parent_a = parent_builder_a.TakeStyle();
  auto parent_b = parent_builder_b.TakeStyle();

  auto style_a = CreateStyle();
  auto style_b = CreateStyle();

  TestKey key("display:block", 1, GetDocument());

  cache.Add(key, *style_a, *parent_a);
  EXPECT_TRUE(cache.Find(key, *style_a, *parent_a));
  EXPECT_TRUE(cache.Find(key, *style_b, *parent_a));
  EXPECT_FALSE(cache.Find(key, *style_b, *parent_b));
}

TEST_F(MatchedPropertiesCacheTest, DirectionDependency) {
  TestCache cache(GetDocument());

  auto parent_builder_a = CreateStyleBuilder();
  parent_builder_a.SetDirection(TextDirection::kLtr);
  auto parent_builder_b = CreateStyleBuilder();
  parent_builder_b.SetDirection(TextDirection::kRtl);

  auto parent_a = parent_builder_a.TakeStyle();
  auto parent_b = parent_builder_b.TakeStyle();

  auto style_a = CreateStyle();
  auto style_b = CreateStyle();

  TestKey key("display:block", 1, GetDocument());

  cache.Add(key, *style_a, *parent_a);
  EXPECT_TRUE(cache.Find(key, *style_a, *parent_a));
  EXPECT_TRUE(cache.Find(key, *style_b, *parent_a));
  EXPECT_FALSE(cache.Find(key, *style_b, *parent_b));
}

TEST_F(MatchedPropertiesCacheTest, ColorSchemeDependency) {
  TestCache cache(GetDocument());

  auto builder = CreateStyleBuilder();
  builder.SetDarkColorScheme(false);
  auto parent_a = builder.TakeStyle();

  builder = CreateStyleBuilder();
  builder.SetDarkColorScheme(true);
  auto parent_b = builder.TakeStyle();

  auto style_a = CreateStyle();
  auto style_b = CreateStyle();

  TestKey key("display:block", 1, GetDocument());

  cache.Add(key, *style_a, *parent_a);
  EXPECT_TRUE(cache.Find(key, *style_a, *parent_a));
  EXPECT_TRUE(cache.Find(key, *style_b, *parent_a));
  EXPECT_FALSE(cache.Find(key, *style_b, *parent_b));
}

TEST_F(MatchedPropertiesCacheTest, VariableDependency) {
  TestCache cache(GetDocument());

  auto parent_builder_a = CreateStyleBuilder();
  auto parent_builder_b = CreateStyleBuilder();
  parent_builder_a.SetVariableData("--x", CreateVariableData("1px"), true);
  parent_builder_b.SetVariableData("--x", CreateVariableData("2px"), true);
  auto parent_a = parent_builder_a.TakeStyle();
  auto parent_b = parent_builder_b.TakeStyle();

  auto style_builder_a = CreateStyleBuilder();
  auto style_builder_b = CreateStyleBuilder();
  style_builder_a.SetHasVariableReferenceFromNonInheritedProperty();
  style_builder_b.SetHasVariableReferenceFromNonInheritedProperty();
  auto style_a = style_builder_a.TakeStyle();
  auto style_b = style_builder_b.TakeStyle();

  TestKey key("top:var(--x)", 1, GetDocument());
  cache.Add(key, *style_a, *parent_a);
  EXPECT_TRUE(cache.Find(key, *style_a, *parent_a));
  EXPECT_TRUE(cache.Find(key, *style_b, *parent_a));
  EXPECT_FALSE(cache.Find(key, *style_b, *parent_b));
}

TEST_F(MatchedPropertiesCacheTest, VariableDependencyNoVars) {
  TestCache cache(GetDocument());

  auto parent_a = CreateStyle();
  auto parent_b = CreateStyle();

  auto style_builder_a = CreateStyleBuilder();
  auto style_builder_b = CreateStyleBuilder();
  style_builder_a.SetHasVariableReferenceFromNonInheritedProperty();
  style_builder_b.SetHasVariableReferenceFromNonInheritedProperty();
  auto style_a = style_builder_a.TakeStyle();
  auto style_b = style_builder_b.TakeStyle();

  TestKey key("top:var(--x)", 1, GetDocument());

  cache.Add(key, *style_a, *parent_a);
  // parent_a/b both have no variables, so this should be a cache hit.
  EXPECT_TRUE(cache.Find(key, *style_a, *parent_a));
  EXPECT_TRUE(cache.Find(key, *style_b, *parent_a));
  EXPECT_TRUE(cache.Find(key, *style_b, *parent_b));
}

TEST_F(MatchedPropertiesCacheTest, NoVariableDependency) {
  TestCache cache(GetDocument());

  auto parent_builder_a = CreateStyleBuilder();
  auto parent_builder_b = CreateStyleBuilder();
  parent_builder_a.SetVariableData("--x", CreateVariableData("1px"), true);
  parent_builder_b.SetVariableData("--x", CreateVariableData("2px"), true);
  auto parent_a = parent_builder_a.TakeStyle();
  auto parent_b = parent_builder_b.TakeStyle();
  auto style_a = CreateStyle();
  auto style_b = CreateStyle();

  TestKey key("top:var(--x)", 1, GetDocument());

  cache.Add(key, *style_a, *parent_a);
  // parent_a/b both have variables, but style_a/b is not marked as
  // depending on them.
  EXPECT_TRUE(cache.Find(key, *style_a, *parent_a));
  EXPECT_TRUE(cache.Find(key, *style_b, *parent_a));
  EXPECT_TRUE(cache.Find(key, *style_b, *parent_b));
}

}  // namespace blink
