// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_container_values.h"

#include "third_party/blink/renderer/core/css/container_state.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

namespace blink {

class CSSContainerValuesTest : public PageTestBase {
 public:
  void SetUp() override {
    PageTestBase::SetUp();
    GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(R"HTML(
      <div id="container"></div>
    )HTML");
  }

  void SetContainerWritingDirection(WritingMode writing_mode,
                                    TextDirection direction) {
    ComputedStyleBuilder builder(
        *GetDocument().GetStyleResolver().InitialStyleForElement());
    builder.SetWritingMode(writing_mode);
    builder.SetDirection(direction);
    ContainerElement().SetComputedStyle(builder.TakeStyle());
  }

  CSSContainerValues* CreateStickyValues(ContainerStuckPhysical horizontal,
                                         ContainerStuckPhysical vertical) {
    return MakeGarbageCollected<CSSContainerValues>(
        GetDocument(), ContainerElement(), std::nullopt, std::nullopt,
        horizontal, vertical,
        static_cast<ContainerSnappedFlags>(ContainerSnapped::kNone),
        static_cast<ContainerScrollableFlags>(ContainerScrollable::kNone),
        static_cast<ContainerScrollableFlags>(ContainerScrollable::kNone),
        ContainerScrolled::kNone, ContainerScrolled::kNone,
        WritingDirectionMode(WritingMode::kHorizontalTb, TextDirection::kLtr),
        PositionTryFallback());
  }

  CSSContainerValues* CreateSnappedValues(ContainerSnappedFlags snapped) {
    return MakeGarbageCollected<CSSContainerValues>(
        GetDocument(), ContainerElement(), std::nullopt, std::nullopt,
        ContainerStuckPhysical::kNo, ContainerStuckPhysical::kNo, snapped,
        static_cast<ContainerScrollableFlags>(ContainerScrollable::kNone),
        static_cast<ContainerScrollableFlags>(ContainerScrollable::kNone),
        ContainerScrolled::kNone, ContainerScrolled::kNone,
        WritingDirectionMode(WritingMode::kHorizontalTb, TextDirection::kLtr),
        PositionTryFallback());
  }

  CSSContainerValues* CreateScrollableValues(
      ContainerScrollableFlags horizontal,
      ContainerScrollableFlags vertical) {
    return MakeGarbageCollected<CSSContainerValues>(
        GetDocument(), ContainerElement(), std::nullopt, std::nullopt,
        ContainerStuckPhysical::kNo, ContainerStuckPhysical::kNo,
        static_cast<ContainerSnappedFlags>(ContainerSnapped::kNone), horizontal,
        vertical, ContainerScrolled::kNone, ContainerScrolled::kNone,
        WritingDirectionMode(WritingMode::kHorizontalTb, TextDirection::kLtr),
        PositionTryFallback());
  }

  CSSContainerValues* CreateScrolledValues(ContainerScrolled horizontal,
                                           ContainerScrolled vertical) {
    return MakeGarbageCollected<CSSContainerValues>(
        GetDocument(), ContainerElement(), std::nullopt, std::nullopt,
        ContainerStuckPhysical::kNo, ContainerStuckPhysical::kNo,
        static_cast<ContainerSnappedFlags>(ContainerSnapped::kNone),
        static_cast<ContainerScrollableFlags>(ContainerScrollable::kNone),
        static_cast<ContainerScrollableFlags>(ContainerScrollable::kNone),
        horizontal, vertical,
        WritingDirectionMode(WritingMode::kHorizontalTb, TextDirection::kLtr),
        PositionTryFallback());
  }

 private:
  Element& ContainerElement() {
    return *GetDocument().getElementById(AtomicString("container"));
  }
};

TEST_F(CSSContainerValuesTest, StickyHorizontalTbLtr) {
  SetContainerWritingDirection(WritingMode::kHorizontalTb, TextDirection::kLtr);
  MediaValues* values = CreateStickyValues(ContainerStuckPhysical::kRight,
                                           ContainerStuckPhysical::kTop);
  EXPECT_EQ(values->StuckInline(), ContainerStuckLogical::kEnd);
  EXPECT_EQ(values->StuckBlock(), ContainerStuckLogical::kStart);
}

TEST_F(CSSContainerValuesTest, StickyHorizontalTbRtl) {
  SetContainerWritingDirection(WritingMode::kHorizontalTb, TextDirection::kRtl);
  MediaValues* values = CreateStickyValues(ContainerStuckPhysical::kRight,
                                           ContainerStuckPhysical::kTop);
  EXPECT_EQ(values->StuckInline(), ContainerStuckLogical::kStart);
  EXPECT_EQ(values->StuckBlock(), ContainerStuckLogical::kStart);
}

TEST_F(CSSContainerValuesTest, StickyVerticalLrLtr) {
  SetContainerWritingDirection(WritingMode::kVerticalLr, TextDirection::kLtr);
  MediaValues* values = CreateStickyValues(ContainerStuckPhysical::kRight,
                                           ContainerStuckPhysical::kTop);
  EXPECT_EQ(values->StuckInline(), ContainerStuckLogical::kStart);
  EXPECT_EQ(values->StuckBlock(), ContainerStuckLogical::kEnd);
}

TEST_F(CSSContainerValuesTest, StickyVerticalLrRtl) {
  SetContainerWritingDirection(WritingMode::kVerticalLr, TextDirection::kRtl);
  MediaValues* values = CreateStickyValues(ContainerStuckPhysical::kRight,
                                           ContainerStuckPhysical::kTop);
  EXPECT_EQ(values->StuckInline(), ContainerStuckLogical::kEnd);
  EXPECT_EQ(values->StuckBlock(), ContainerStuckLogical::kEnd);
}

TEST_F(CSSContainerValuesTest, StickyVerticalRlLtr) {
  SetContainerWritingDirection(WritingMode::kVerticalRl, TextDirection::kLtr);
  MediaValues* values = CreateStickyValues(ContainerStuckPhysical::kRight,
                                           ContainerStuckPhysical::kTop);
  EXPECT_EQ(values->StuckInline(), ContainerStuckLogical::kStart);
  EXPECT_EQ(values->StuckBlock(), ContainerStuckLogical::kStart);
}

TEST_F(CSSContainerValuesTest, StickyVerticalRlRtl) {
  SetContainerWritingDirection(WritingMode::kVerticalRl, TextDirection::kRtl);
  MediaValues* values = CreateStickyValues(ContainerStuckPhysical::kRight,
                                           ContainerStuckPhysical::kTop);
  EXPECT_EQ(values->StuckInline(), ContainerStuckLogical::kEnd);
  EXPECT_EQ(values->StuckBlock(), ContainerStuckLogical::kStart);
}

TEST_F(CSSContainerValuesTest, SnappedNone) {
  SetContainerWritingDirection(WritingMode::kHorizontalTb, TextDirection::kLtr);
  MediaValues* values = CreateSnappedValues(
      static_cast<ContainerSnappedFlags>(ContainerSnapped::kNone));
  EXPECT_FALSE(values->SnappedBlock());
  EXPECT_FALSE(values->SnappedInline());
  EXPECT_FALSE(values->Snapped());
}

TEST_F(CSSContainerValuesTest, SnappedX) {
  SetContainerWritingDirection(WritingMode::kHorizontalTb, TextDirection::kLtr);
  MediaValues* values = CreateSnappedValues(
      static_cast<ContainerSnappedFlags>(ContainerSnapped::kX));
  EXPECT_TRUE(values->SnappedX());
  EXPECT_FALSE(values->SnappedY());
  EXPECT_TRUE(values->Snapped());
}

TEST_F(CSSContainerValuesTest, SnappedY) {
  SetContainerWritingDirection(WritingMode::kHorizontalTb, TextDirection::kLtr);
  MediaValues* values = CreateSnappedValues(
      static_cast<ContainerSnappedFlags>(ContainerSnapped::kY));
  EXPECT_FALSE(values->SnappedX());
  EXPECT_TRUE(values->SnappedY());
  EXPECT_TRUE(values->Snapped());
}

TEST_F(CSSContainerValuesTest, SnappedBlock) {
  SetContainerWritingDirection(WritingMode::kHorizontalTb, TextDirection::kLtr);
  MediaValues* values = CreateSnappedValues(
      static_cast<ContainerSnappedFlags>(ContainerSnapped::kY));
  EXPECT_TRUE(values->SnappedBlock());
  EXPECT_FALSE(values->SnappedInline());
  EXPECT_TRUE(values->Snapped());
}

TEST_F(CSSContainerValuesTest, SnappedInline) {
  SetContainerWritingDirection(WritingMode::kHorizontalTb, TextDirection::kLtr);
  MediaValues* values = CreateSnappedValues(
      static_cast<ContainerSnappedFlags>(ContainerSnapped::kX));
  EXPECT_FALSE(values->SnappedBlock());
  EXPECT_TRUE(values->SnappedInline());
  EXPECT_TRUE(values->Snapped());
}

TEST_F(CSSContainerValuesTest, SnappedBoth) {
  SetContainerWritingDirection(WritingMode::kHorizontalTb, TextDirection::kLtr);
  MediaValues* values = CreateSnappedValues(
      static_cast<ContainerSnappedFlags>(ContainerSnapped::kX) |
      static_cast<ContainerSnappedFlags>(ContainerSnapped::kY));
  EXPECT_TRUE(values->SnappedBlock());
  EXPECT_TRUE(values->SnappedInline());
  EXPECT_TRUE(values->Snapped());
}

TEST_F(CSSContainerValuesTest, ScrollableHorizontalTbLtr) {
  SetContainerWritingDirection(WritingMode::kHorizontalTb, TextDirection::kLtr);
  MediaValues* values = CreateScrollableValues(
      static_cast<ContainerScrollableFlags>(ContainerScrollable::kEnd),
      static_cast<ContainerScrollableFlags>(ContainerScrollable::kStart));
  EXPECT_EQ(values->ScrollableInline(),
            static_cast<ContainerScrollableFlags>(ContainerScrollable::kEnd));
  EXPECT_EQ(values->ScrollableBlock(),
            static_cast<ContainerScrollableFlags>(ContainerScrollable::kStart));
}

TEST_F(CSSContainerValuesTest, ScrollableHorizontalTbRtl) {
  SetContainerWritingDirection(WritingMode::kHorizontalTb, TextDirection::kRtl);
  MediaValues* values = CreateScrollableValues(
      static_cast<ContainerScrollableFlags>(ContainerScrollable::kEnd),
      static_cast<ContainerScrollableFlags>(ContainerScrollable::kStart));
  EXPECT_EQ(values->ScrollableInline(),
            static_cast<ContainerScrollableFlags>(ContainerScrollable::kStart));
  EXPECT_EQ(values->ScrollableBlock(),
            static_cast<ContainerScrollableFlags>(ContainerScrollable::kStart));
}

TEST_F(CSSContainerValuesTest, ScrollableVerticalLrLtr) {
  SetContainerWritingDirection(WritingMode::kVerticalLr, TextDirection::kLtr);
  MediaValues* values = CreateScrollableValues(
      static_cast<ContainerScrollableFlags>(ContainerScrollable::kEnd),
      static_cast<ContainerScrollableFlags>(ContainerScrollable::kStart));
  EXPECT_EQ(values->ScrollableInline(),
            static_cast<ContainerScrollableFlags>(ContainerScrollable::kStart));
  EXPECT_EQ(values->ScrollableBlock(),
            static_cast<ContainerScrollableFlags>(ContainerScrollable::kEnd));
}

TEST_F(CSSContainerValuesTest, ScrollableVerticalLrRtl) {
  SetContainerWritingDirection(WritingMode::kVerticalLr, TextDirection::kRtl);
  MediaValues* values = CreateScrollableValues(
      static_cast<ContainerScrollableFlags>(ContainerScrollable::kEnd),
      static_cast<ContainerScrollableFlags>(ContainerScrollable::kStart));
  EXPECT_EQ(values->ScrollableInline(),
            static_cast<ContainerScrollableFlags>(ContainerScrollable::kEnd));
  EXPECT_EQ(values->ScrollableBlock(),
            static_cast<ContainerScrollableFlags>(ContainerScrollable::kEnd));
}

TEST_F(CSSContainerValuesTest, ScrollableVerticalRlLtr) {
  SetContainerWritingDirection(WritingMode::kVerticalRl, TextDirection::kLtr);
  MediaValues* values = CreateScrollableValues(
      static_cast<ContainerScrollableFlags>(ContainerScrollable::kEnd),
      static_cast<ContainerScrollableFlags>(ContainerScrollable::kStart));
  EXPECT_EQ(values->ScrollableInline(),
            static_cast<ContainerScrollableFlags>(ContainerScrollable::kStart));
  EXPECT_EQ(values->ScrollableBlock(),
            static_cast<ContainerScrollableFlags>(ContainerScrollable::kStart));
}

TEST_F(CSSContainerValuesTest, ScrollableVerticalRlRtl) {
  SetContainerWritingDirection(WritingMode::kVerticalRl, TextDirection::kRtl);
  MediaValues* values = CreateScrollableValues(
      static_cast<ContainerScrollableFlags>(ContainerScrollable::kEnd),
      static_cast<ContainerScrollableFlags>(ContainerScrollable::kStart));
  EXPECT_EQ(values->ScrollableInline(),
            static_cast<ContainerScrollableFlags>(ContainerScrollable::kEnd));
  EXPECT_EQ(values->ScrollableBlock(),
            static_cast<ContainerScrollableFlags>(ContainerScrollable::kStart));
}

TEST_F(CSSContainerValuesTest, ScrolledHorizontal) {
  ScopedCSSScrolledContainerQueriesForTest scoped(true);
  SetContainerWritingDirection(WritingMode::kVerticalLr, TextDirection::kRtl);
  MediaValues* values =
      CreateScrolledValues(ContainerScrolled::kEnd, ContainerScrolled::kStart);
  EXPECT_EQ(values->ScrolledHorizontal(), ContainerScrolled::kEnd);
}

TEST_F(CSSContainerValuesTest, ScrolledVertical) {
  ScopedCSSScrolledContainerQueriesForTest scoped(true);
  SetContainerWritingDirection(WritingMode::kVerticalLr, TextDirection::kRtl);
  MediaValues* values =
      CreateScrolledValues(ContainerScrolled::kEnd, ContainerScrolled::kStart);
  EXPECT_EQ(values->ScrolledVertical(), ContainerScrolled::kStart);
}

TEST_F(CSSContainerValuesTest, ScrolledHorizontalTbLtr) {
  ScopedCSSScrolledContainerQueriesForTest scoped(true);
  SetContainerWritingDirection(WritingMode::kHorizontalTb, TextDirection::kLtr);
  MediaValues* values =
      CreateScrolledValues(ContainerScrolled::kEnd, ContainerScrolled::kStart);
  EXPECT_EQ(values->ScrolledInline(), ContainerScrolled::kEnd);
  EXPECT_EQ(values->ScrolledBlock(), ContainerScrolled::kStart);
}

TEST_F(CSSContainerValuesTest, ScrolledHorizontalTbRtl) {
  ScopedCSSScrolledContainerQueriesForTest scoped(true);
  SetContainerWritingDirection(WritingMode::kHorizontalTb, TextDirection::kRtl);
  MediaValues* values =
      CreateScrolledValues(ContainerScrolled::kEnd, ContainerScrolled::kStart);
  EXPECT_EQ(values->ScrolledInline(), ContainerScrolled::kStart);
  EXPECT_EQ(values->ScrolledBlock(), ContainerScrolled::kStart);
}

TEST_F(CSSContainerValuesTest, ScrolledVerticalLrLtr) {
  ScopedCSSScrolledContainerQueriesForTest scoped(true);
  SetContainerWritingDirection(WritingMode::kVerticalLr, TextDirection::kLtr);
  MediaValues* values =
      CreateScrolledValues(ContainerScrolled::kEnd, ContainerScrolled::kStart);
  EXPECT_EQ(values->ScrolledInline(), ContainerScrolled::kStart);
  EXPECT_EQ(values->ScrolledBlock(), ContainerScrolled::kEnd);
}

TEST_F(CSSContainerValuesTest, ScrolledVerticalLrRtl) {
  ScopedCSSScrolledContainerQueriesForTest scoped(true);
  SetContainerWritingDirection(WritingMode::kVerticalLr, TextDirection::kRtl);
  MediaValues* values =
      CreateScrolledValues(ContainerScrolled::kEnd, ContainerScrolled::kStart);
  EXPECT_EQ(values->ScrolledInline(), ContainerScrolled::kEnd);
  EXPECT_EQ(values->ScrolledBlock(), ContainerScrolled::kEnd);
}

TEST_F(CSSContainerValuesTest, ScrolledVerticalRlLtr) {
  ScopedCSSScrolledContainerQueriesForTest scoped(true);
  SetContainerWritingDirection(WritingMode::kVerticalRl, TextDirection::kLtr);
  MediaValues* values =
      CreateScrolledValues(ContainerScrolled::kEnd, ContainerScrolled::kStart);
  EXPECT_EQ(values->ScrolledInline(), ContainerScrolled::kStart);
  EXPECT_EQ(values->ScrolledBlock(), ContainerScrolled::kStart);
}

TEST_F(CSSContainerValuesTest, ScrolledVerticalRlRtl) {
  ScopedCSSScrolledContainerQueriesForTest scoped(true);
  SetContainerWritingDirection(WritingMode::kVerticalRl, TextDirection::kRtl);
  MediaValues* values =
      CreateScrolledValues(ContainerScrolled::kEnd, ContainerScrolled::kStart);
  EXPECT_EQ(values->ScrolledInline(), ContainerScrolled::kEnd);
  EXPECT_EQ(values->ScrolledBlock(), ContainerScrolled::kStart);
}

}  // namespace blink
