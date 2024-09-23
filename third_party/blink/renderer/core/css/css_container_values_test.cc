// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_container_values.h"

#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"

namespace blink {

class CSSContainerValuesTest : public PageTestBase {
 public:
  void SetUp() override {
    PageTestBase::SetUp();
    GetDocument().body()->setInnerHTML(R"HTML(
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
        static_cast<ContainerSnappedFlags>(ContainerSnapped::kNone));
  }

  CSSContainerValues* CreateSnappedValues(ContainerSnappedFlags snapped) {
    return MakeGarbageCollected<CSSContainerValues>(
        GetDocument(), ContainerElement(), std::nullopt, std::nullopt,
        ContainerStuckPhysical::kNo, ContainerStuckPhysical::kNo, snapped);
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

}  // namespace blink
