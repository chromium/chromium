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
           const ComputedStyle& parent_style) {
    cache_.Add(key.InnerKey(), style, parent_style);
  }

  const CachedMatchedProperties* Find(const TestKey& key,
                                      const ComputedStyle& style,
                                      const ComputedStyle& parent_style) {
    StyleResolverState state(document_, *document_.body(), StyleRecalcContext(),
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
    return GetDocument().GetStyleResolver().InitialStyleForElement();
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

TEST_F(MatchedPropertiesCacheTest, WritingModeDependency) {
  TestCache cache(GetDocument());

  auto parent_a = CreateStyle();
  auto parent_b = CreateStyle();
  auto style_a = CreateStyle();
  auto style_b = CreateStyle();
  parent_a->SetWritingMode(WritingMode::kHorizontalTb);
  parent_b->SetWritingMode(WritingMode::kVerticalRl);

  TestKey key("display:block", 1, GetDocument());

  cache.Add(key, *style_a, *parent_a);
  EXPECT_TRUE(cache.Find(key, *style_a, *parent_a));
  EXPECT_TRUE(cache.Find(key, *style_b, *parent_a));
  EXPECT_FALSE(cache.Find(key, *style_b, *parent_b));
}

TEST_F(MatchedPropertiesCacheTest, DirectionDependency) {
  TestCache cache(GetDocument());

  auto parent_a = CreateStyle();
  auto parent_b = CreateStyle();
  auto style_a = CreateStyle();
  auto style_b = CreateStyle();
  parent_a->SetDirection(TextDirection::kLtr);
  parent_b->SetDirection(TextDirection::kRtl);

  TestKey key("display:block", 1, GetDocument());

  cache.Add(key, *style_a, *parent_a);
  EXPECT_TRUE(cache.Find(key, *style_a, *parent_a));
  EXPECT_TRUE(cache.Find(key, *style_b, *parent_a));
  EXPECT_FALSE(cache.Find(key, *style_b, *parent_b));
}

TEST_F(MatchedPropertiesCacheTest, VariableDependency) {
  TestCache cache(GetDocument());

  auto parent_a = CreateStyle();
  auto parent_b = CreateStyle();
  auto style_a = CreateStyle();
  auto style_b = CreateStyle();
  parent_a->SetVariableData("--x", CreateVariableData("1px"), true);
  parent_b->SetVariableData("--x", CreateVariableData("2px"), true);
  style_a->SetHasVariableReferenceFromNonInheritedProperty();
  style_b->SetHasVariableReferenceFromNonInheritedProperty();

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
  auto style_a = CreateStyle();
  auto style_b = CreateStyle();
  style_a->SetHasVariableReferenceFromNonInheritedProperty();
  style_b->SetHasVariableReferenceFromNonInheritedProperty();

  TestKey key("top:var(--x)", 1, GetDocument());

  cache.Add(key, *style_a, *parent_a);
  // parent_a/b both have no variables, so this should be a cache hit.
  EXPECT_TRUE(cache.Find(key, *style_a, *parent_a));
  EXPECT_TRUE(cache.Find(key, *style_b, *parent_a));
  EXPECT_TRUE(cache.Find(key, *style_b, *parent_b));
}

TEST_F(MatchedPropertiesCacheTest, NoVariableDependency) {
  TestCache cache(GetDocument());

  auto parent_a = CreateStyle();
  auto parent_b = CreateStyle();
  auto style_a = CreateStyle();
  auto style_b = CreateStyle();
  parent_a->SetVariableData("--x", CreateVariableData("1px"), true);
  parent_b->SetVariableData("--x", CreateVariableData("2px"), true);

  TestKey key("top:var(--x)", 1, GetDocument());

  cache.Add(key, *style_a, *parent_a);
  // parent_a/b both have variables, but style_a/b is not marked as
  // depending on them.
  EXPECT_TRUE(cache.Find(key, *style_a, *parent_a));
  EXPECT_TRUE(cache.Find(key, *style_b, *parent_a));
  EXPECT_TRUE(cache.Find(key, *style_b, *parent_b));
}

}  // namespace blink
