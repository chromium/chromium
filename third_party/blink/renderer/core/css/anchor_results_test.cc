// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/anchor_results.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/style/scoped_css_name.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/geometry/calculation_expression_node.h"

namespace blink {

class AnchorResultsTest : public PageTestBase {
 public:
  struct Options {
    STACK_ALLOCATED();

   public:
    AnchorScope::Mode mode = AnchorScope::Mode::kNone;
    CSSAnchorQueryType query_type = CSSAnchorQueryType::kAnchor;
    AnchorSpecifierValue::Type specifier_type =
        AnchorSpecifierValue::Type::kDefault;
    float percentage = 0;
    AtomicString name;
    TreeScope* tree_scope = nullptr;
    absl::variant<CSSAnchorValue, CSSAnchorSizeValue> value =
        CSSAnchorValue::kStart;
  };

  AnchorSpecifierValue* CreateAnchorSpecifierValue(
      AnchorSpecifierValue::Type type,
      AtomicString name,
      TreeScope* tree_scope) {
    switch (type) {
      case AnchorSpecifierValue::Type::kDefault:
        return AnchorSpecifierValue::Default();
      case AnchorSpecifierValue::Type::kImplicit:
        return AnchorSpecifierValue::Implicit();
      case AnchorSpecifierValue::Type::kNamed:
        return MakeGarbageCollected<AnchorSpecifierValue>(
            *MakeGarbageCollected<ScopedCSSName>(name, tree_scope));
    }
  }

  AnchorItem* CreateItem(Options options) {
    return MakeGarbageCollected<AnchorItem>(
        options.mode, AnchorQuery(options.query_type,
                                  CreateAnchorSpecifierValue(
                                      options.specifier_type, options.name,
                                      options.tree_scope),
                                  options.percentage, options.value));
  }

  unsigned ItemHash(Options options) { return CreateItem(options)->GetHash(); }
};

TEST_F(AnchorResultsTest, ItemEquality) {
  EXPECT_EQ(*CreateItem(Options{}), *CreateItem(Options{}));
  EXPECT_EQ(*CreateItem(Options{.mode = AnchorScope::Mode::kTop}),
            *CreateItem(Options{.mode = AnchorScope::Mode::kTop}));
  EXPECT_EQ(*CreateItem(Options{.query_type = CSSAnchorQueryType::kAnchorSize,
                                .value = CSSAnchorSizeValue::kWidth}),
            *CreateItem(Options{.query_type = CSSAnchorQueryType::kAnchorSize,
                                .value = CSSAnchorSizeValue::kWidth}));
  EXPECT_EQ(*CreateItem(Options{.specifier_type =
                                    AnchorSpecifierValue::Type::kImplicit}),
            *CreateItem(Options{.specifier_type =
                                    AnchorSpecifierValue::Type::kImplicit}));
  EXPECT_EQ(*CreateItem(Options{.percentage = 1.0f}),
            *CreateItem(Options{.percentage = 1.0f}));
  EXPECT_EQ(
      *CreateItem(Options{.specifier_type = AnchorSpecifierValue::Type::kNamed,
                          .name = AtomicString("--foo")}),
      *CreateItem(Options{.specifier_type = AnchorSpecifierValue::Type::kNamed,
                          .name = AtomicString("--foo")}));
  ASSERT_TRUE(GetDocument().body());
  EXPECT_EQ(
      *CreateItem(Options{.specifier_type = AnchorSpecifierValue::Type::kNamed,
                          .name = g_empty_atom,
                          .tree_scope = &GetDocument()}),
      *CreateItem(Options{.specifier_type = AnchorSpecifierValue::Type::kNamed,
                          .name = g_empty_atom,
                          .tree_scope = &GetDocument()}));
  EXPECT_EQ(*CreateItem(Options{.value = CSSAnchorValue::kTop}),
            *CreateItem(Options{.value = CSSAnchorValue::kTop}));
}

TEST_F(AnchorResultsTest, ItemInequality) {
  EXPECT_NE(*CreateItem(Options{.query_type = CSSAnchorQueryType::kAnchorSize}),
            *CreateItem(Options{}));
  EXPECT_NE(*CreateItem(Options{.mode = AnchorScope::Mode::kTop}),
            *CreateItem(Options{.mode = AnchorScope::Mode::kBottom}));
  EXPECT_NE(*CreateItem(Options{.query_type = CSSAnchorQueryType::kAnchorSize,
                                .value = CSSAnchorSizeValue::kWidth}),
            *CreateItem(Options{.query_type = CSSAnchorQueryType::kAnchor}));
  EXPECT_NE(*CreateItem(Options{.specifier_type =
                                    AnchorSpecifierValue::Type::kDefault}),
            *CreateItem(Options{.specifier_type =
                                    AnchorSpecifierValue::Type::kImplicit}));
  EXPECT_NE(*CreateItem(Options{.percentage = 1.0f}),
            *CreateItem(Options{.percentage = 2.0f}));
  EXPECT_NE(
      *CreateItem(Options{.specifier_type = AnchorSpecifierValue::Type::kNamed,
                          .name = AtomicString("--foo")}),
      *CreateItem(Options{.specifier_type = AnchorSpecifierValue::Type::kNamed,
                          .name = AtomicString("--bar")}));
  EXPECT_NE(
      *CreateItem(Options{.specifier_type = AnchorSpecifierValue::Type::kNamed,
                          .name = g_empty_atom,
                          .tree_scope = &GetDocument()}),
      *CreateItem(Options{.specifier_type = AnchorSpecifierValue::Type::kNamed,
                          .name = g_empty_atom,
                          .tree_scope = nullptr}));
  EXPECT_NE(*CreateItem(Options{.value = CSSAnchorValue::kTop}),
            *CreateItem(Options{.value = CSSAnchorValue::kLeft}));
  EXPECT_NE(*CreateItem(Options{.value = CSSAnchorValue::kTop}),
            *CreateItem(Options{.value = CSSAnchorSizeValue::kWidth}));
}

TEST_F(AnchorResultsTest, ItemHashEqual) {
  EXPECT_EQ(ItemHash(Options{}), ItemHash(Options{}));
  EXPECT_EQ(ItemHash(Options{.mode = AnchorScope::Mode::kTop}),
            ItemHash(Options{.mode = AnchorScope::Mode::kTop}));
  EXPECT_EQ(ItemHash(Options{.query_type = CSSAnchorQueryType::kAnchorSize,
                             .value = CSSAnchorSizeValue::kWidth}),
            ItemHash(Options{.query_type = CSSAnchorQueryType::kAnchorSize,
                             .value = CSSAnchorSizeValue::kWidth}));
  EXPECT_EQ(ItemHash(Options{.specifier_type =
                                 AnchorSpecifierValue::Type::kImplicit}),
            ItemHash(Options{.specifier_type =
                                 AnchorSpecifierValue::Type::kImplicit}));
  EXPECT_EQ(ItemHash(Options{.percentage = 1.0f}),
            ItemHash(Options{.percentage = 1.0f}));
  EXPECT_EQ(
      ItemHash(Options{.specifier_type = AnchorSpecifierValue::Type::kNamed,
                       .name = AtomicString("--foo")}),
      ItemHash(Options{.specifier_type = AnchorSpecifierValue::Type::kNamed,
                       .name = AtomicString("--foo")}));
  ASSERT_TRUE(GetDocument().body());
  EXPECT_EQ(
      ItemHash(Options{.specifier_type = AnchorSpecifierValue::Type::kNamed,
                       .name = g_empty_atom,
                       .tree_scope = &GetDocument()}),
      ItemHash(Options{.specifier_type = AnchorSpecifierValue::Type::kNamed,
                       .name = g_empty_atom,
                       .tree_scope = &GetDocument()}));
  EXPECT_EQ(ItemHash(Options{.value = CSSAnchorValue::kTop}),
            ItemHash(Options{.value = CSSAnchorValue::kTop}));
}

TEST_F(AnchorResultsTest, ItemHashNotEqual) {
  EXPECT_NE(ItemHash(Options{.specifier_type =
                                 AnchorSpecifierValue::Type::kImplicit}),
            ItemHash(Options{}));
  EXPECT_NE(ItemHash(Options{.mode = AnchorScope::Mode::kTop}),
            ItemHash(Options{.mode = AnchorScope::Mode::kLeft}));
  EXPECT_NE(ItemHash(Options{.query_type = CSSAnchorQueryType::kAnchorSize,
                             .value = CSSAnchorSizeValue::kWidth}),
            ItemHash(Options{.query_type = CSSAnchorQueryType::kAnchor}));
  EXPECT_NE(
      ItemHash(Options{.specifier_type = AnchorSpecifierValue::Type::kDefault}),
      ItemHash(
          Options{.specifier_type = AnchorSpecifierValue::Type::kImplicit}));
  EXPECT_NE(ItemHash(Options{.percentage = 1.0f}),
            ItemHash(Options{.percentage = 2.0f}));
  EXPECT_NE(
      ItemHash(Options{.specifier_type = AnchorSpecifierValue::Type::kNamed,
                       .name = AtomicString("--foo")}),
      ItemHash(Options{.specifier_type = AnchorSpecifierValue::Type::kNamed,
                       .name = AtomicString("--bar")}));
  EXPECT_NE(
      ItemHash(Options{.specifier_type = AnchorSpecifierValue::Type::kNamed,
                       .name = g_empty_atom,
                       .tree_scope = &GetDocument()}),
      ItemHash(Options{.specifier_type = AnchorSpecifierValue::Type::kNamed,
                       .name = g_empty_atom,
                       .tree_scope = nullptr}));
  EXPECT_NE(ItemHash(Options{.value = CSSAnchorValue::kTop}),
            ItemHash(Options{.value = CSSAnchorValue::kLeft}));
  EXPECT_NE(ItemHash(Options{.query_type = CSSAnchorQueryType::kAnchorSize,
                             .value = CSSAnchorSizeValue::kWidth}),
            ItemHash(Options{.query_type = CSSAnchorQueryType::kAnchorSize,
                             .value = CSSAnchorSizeValue::kHeight}));
}

TEST_F(AnchorResultsTest, MapInsert) {
  AnchorResultMap map;

  const AnchorItem* item1 =
      CreateItem(Options{.mode = AnchorScope::Mode::kSize,
                         .query_type = CSSAnchorQueryType::kAnchorSize,
                         .value = CSSAnchorSizeValue::kWidth});
  const AnchorItem* item2 =
      CreateItem(Options{.mode = AnchorScope::Mode::kSize,
                         .query_type = CSSAnchorQueryType::kAnchorSize,
                         .value = CSSAnchorSizeValue::kWidth});

  const AnchorItem* item3 =
      CreateItem(Options{.mode = AnchorScope::Mode::kSize,
                         .specifier_type = AnchorSpecifierValue::Type::kNamed,
                         .name = AtomicString("--foo")});

  EXPECT_TRUE(map.empty());

  map.Set(item1, LayoutUnit(42.0));

  ASSERT_TRUE(map.Contains(item1));
  ASSERT_TRUE(map.Contains(item2));
  EXPECT_FALSE(map.Contains(item3));

  EXPECT_EQ(LayoutUnit(42.0), map.at(item1));
  EXPECT_EQ(LayoutUnit(42.0), map.at(item2));

  map.Set(item2, std::nullopt);
  ASSERT_TRUE(map.Contains(item1));
  ASSERT_TRUE(map.Contains(item2));
  EXPECT_FALSE(map.Contains(item3));
  EXPECT_EQ(std::nullopt, map.at(item1));
  EXPECT_EQ(std::nullopt, map.at(item2));
}

TEST_F(AnchorResultsTest, IsEmpty) {
  AnchorResults results;
  EXPECT_TRUE(results.IsEmpty());
}

TEST_F(AnchorResultsTest, IsNotEmpty) {
  AnchorResults results;
  results.Set(AnchorScope::Mode::kTop, CreateItem(Options{})->Query(),
              LayoutUnit(42.0));
  EXPECT_FALSE(results.IsEmpty());
}

TEST_F(AnchorResultsTest, IsAnyResultDifferent_NoDiff) {
  AnchorResults results1;
  results1.Set(AnchorScope::Mode::kTop, CreateItem(Options{})->Query(),
               LayoutUnit(42.0));
  AnchorResults results2;
  results2.Set(AnchorScope::Mode::kTop, CreateItem(Options{})->Query(),
               LayoutUnit(42.0));
  EXPECT_FALSE(results1.IsAnyResultDifferent(&results2));
}

TEST_F(AnchorResultsTest, IsAnyResultDifferent_Empty) {
  AnchorResults results1;
  AnchorResults results2;
  results2.Set(AnchorScope::Mode::kTop, CreateItem(Options{})->Query(),
               LayoutUnit(42.0));
  EXPECT_FALSE(results1.IsAnyResultDifferent(&results2));
}

TEST_F(AnchorResultsTest, IsAnyResultDifferent_Diff) {
  AnchorResults results1;
  results1.Set(AnchorScope::Mode::kTop, CreateItem(Options{})->Query(),
               LayoutUnit(42.0));
  AnchorResults results2;
  results2.Set(AnchorScope::Mode::kTop, CreateItem(Options{})->Query(),
               LayoutUnit(84.0));
  EXPECT_TRUE(results1.IsAnyResultDifferent(&results2));
}

TEST_F(AnchorResultsTest, IsAnyResultDifferent_Missing) {
  // Evaluating something causes AnchorResults to add an explicit nullopt
  // for this item, making it no longer empty, and giving the subsequent
  // call to IsAnyResultDifferent something to do.
  AnchorResults results1;
  {
    AnchorScope anchor_scope(AnchorScope::Mode::kTop, &results1);
    results1.Evaluate(CreateItem(Options{})->Query());
  }

  AnchorResults results2;
  results2.Set(AnchorScope::Mode::kTop, CreateItem(Options{})->Query(),
               LayoutUnit(42.0));
  EXPECT_TRUE(results1.IsAnyResultDifferent(&results2));
}

TEST_F(AnchorResultsTest, Evaluate) {
  AnchorResults results;

  AnchorScope::Mode mode = AnchorScope::Mode::kSize;
  const AnchorItem* item =
      CreateItem(Options{.mode = mode,
                         .query_type = CSSAnchorQueryType::kAnchorSize,
                         .value = CSSAnchorSizeValue::kWidth});
  results.Set(mode, item->Query(), LayoutUnit(42.0));

  AnchorScope anchor_scope(mode, &results);
  EXPECT_EQ(LayoutUnit(42.0), results.Evaluate(item->Query()));
}

TEST_F(AnchorResultsTest, EvaluateWrongMode) {
  AnchorResults results;

  AnchorScope::Mode mode = AnchorScope::Mode::kSize;
  const AnchorItem* item =
      CreateItem(Options{.mode = mode,
                         .query_type = CSSAnchorQueryType::kAnchorSize,
                         .value = CSSAnchorSizeValue::kWidth});
  results.Set(mode, item->Query(), LayoutUnit(42.0));

  AnchorScope anchor_scope(AnchorScope::Mode::kTop, &results);
  EXPECT_EQ(std::nullopt, results.Evaluate(item->Query()));
}

}  // namespace blink
