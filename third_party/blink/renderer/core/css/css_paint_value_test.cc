// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_paint_value.h"

#include <memory>

#include "base/auto_reset.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/css/css_custom_ident_value.h"
#include "third_party/blink/renderer/core/css/css_syntax_definition.h"
#include "third_party/blink/renderer/core/css/mock_css_paint_image_generator.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/frame_test_helpers.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/style/style_generated_image.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/platform/graphics/paint_generated_image.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/url_test_helpers.h"

using testing::_;
using testing::NiceMock;
using testing::Return;
using testing::ReturnRef;
using testing::Values;

namespace blink {
namespace {

enum {
  kCSSPaintAPIArguments = 1 << 0,
  kOffMainThreadCSSPaint = 1 << 1,
};

class CSSPaintValueTest : public RenderingTest,
                          public ::testing::WithParamInterface<unsigned>,
                          private ScopedCSSPaintAPIArgumentsForTest,
                          private ScopedOffMainThreadCSSPaintForTest {
 public:
  CSSPaintValueTest()
      : ScopedCSSPaintAPIArgumentsForTest(GetParam() & kCSSPaintAPIArguments),
        ScopedOffMainThreadCSSPaintForTest(GetParam() &
                                           kOffMainThreadCSSPaint) {}

  // TODO(xidachen): a mock_generator is used in many tests in this file, put
  // that in a Setup method.
};

INSTANTIATE_TEST_SUITE_P(All,
                         CSSPaintValueTest,
                         Values(0,
                                kCSSPaintAPIArguments,
                                kOffMainThreadCSSPaint,
                                kCSSPaintAPIArguments |
                                    kOffMainThreadCSSPaint));

// CSSPaintImageGenerator requires that CSSPaintImageGeneratorCreateFunction be
// a static method. As such, it cannot access a class member and so instead we
// store a pointer to the overriding generator globally.
MockCSSPaintImageGenerator* g_override_generator = nullptr;
CSSPaintImageGenerator* ProvideOverrideGenerator(
    const String&,
    const Document&,
    CSSPaintImageGenerator::Observer*) {
  return g_override_generator;
}
}  // namespace

TEST_P(CSSPaintValueTest, DelayPaintUntilGeneratorReady) {
  NiceMock<MockCSSPaintImageGenerator>* mock_generator =
      MakeGarbageCollected<NiceMock<MockCSSPaintImageGenerator>>();
  base::AutoReset<MockCSSPaintImageGenerator*> scoped_override_generator(
      &g_override_generator, mock_generator);
  base::AutoReset<CSSPaintImageGenerator::CSSPaintImageGeneratorCreateFunction>
      scoped_create_function(
          CSSPaintImageGenerator::GetCreateFunctionForTesting(),
          ProvideOverrideGenerator);

  const gfx::SizeF target_size(100, 100);

  SetBodyInnerHTML(R"HTML(
    <div id="target"></div>
  )HTML");
  LayoutObject* target = GetLayoutObjectByElementId("target");
  const ComputedStyle& style = *target->Style();

  auto* ident =
      MakeGarbageCollected<CSSCustomIdentValue>(AtomicString("testpainter"));
  CSSPaintValue* paint_value = MakeGarbageCollected<CSSPaintValue>(ident, true);

  // Initially the generator is not ready, so GetImage should fail (and no paint
  // should happen).
  EXPECT_CALL(*mock_generator, Paint(_, _, _)).Times(0);
  EXPECT_FALSE(
      paint_value->GetImage(*target, GetDocument(), style, target_size));

  // Now mark the generator as ready - GetImage should then succeed.
  ON_CALL(*mock_generator, IsImageGeneratorReady()).WillByDefault(Return(true));
  // In off-thread CSS Paint, the actual paint call is deferred and so will
  // never happen.
  if (!RuntimeEnabledFeatures::OffMainThreadCSSPaintEnabled()) {
    EXPECT_CALL(*mock_generator, Paint(_, _, _))
        .WillRepeatedly(
            Return(PaintGeneratedImage::Create(PaintRecord(), target_size)));
  }

  EXPECT_TRUE(
      paint_value->GetImage(*target, GetDocument(), style, target_size));
}

// Regression test for crbug.com/998439. The problem is that GetImage is called
// on a new document. This test simulates the situation by having two different
// documents and call GetImage on different ones.
TEST_P(CSSPaintValueTest, GetImageCalledOnMultipleDocuments) {
  const gfx::SizeF target_size(100, 100);

  SetBodyInnerHTML(R"HTML(<div id="target"></div>)HTML");
  LayoutObject* target = GetLayoutObjectByElementId("target");
  const ComputedStyle& style = *target->Style();

  auto* ident =
      MakeGarbageCollected<CSSCustomIdentValue>(AtomicString("testpainter"));
  CSSPaintValue* paint_value = MakeGarbageCollected<CSSPaintValue>(ident, true);

  EXPECT_EQ(paint_value->NumberOfGeneratorsForTesting(), 0u);
  paint_value->GetImage(*target, GetDocument(), style, target_size);
  // A new generator should be created if there is no generator exists.
  EXPECT_EQ(paint_value->NumberOfGeneratorsForTesting(), 1u);

  auto new_page_holder = std::make_unique<DummyPageHolder>(gfx::Size(800, 600));
  // Call GetImage on a new Document should not crash.
  paint_value->GetImage(*target, new_page_holder->GetDocument(), style,
                        target_size);
  EXPECT_EQ(paint_value->NumberOfGeneratorsForTesting(), 2u);
}

TEST_P(CSSPaintValueTest, NativeInvalidationPropertiesWithNoGenerator) {
  SetBodyInnerHTML(R"HTML(<div id="target"></div>)HTML");

  auto* ident =
      MakeGarbageCollected<CSSCustomIdentValue>(AtomicString("testpainter"));
  CSSPaintValue* paint_value = MakeGarbageCollected<CSSPaintValue>(ident, true);

  EXPECT_EQ(paint_value->NumberOfGeneratorsForTesting(), 0u);
  // There is no generator, so returning a nullptr.
  EXPECT_EQ(paint_value->NativeInvalidationProperties(GetDocument()), nullptr);
}

TEST_P(CSSPaintValueTest, CustomInvalidationPropertiesWithNoGenerator) {
  SetBodyInnerHTML(R"HTML(<div id="target"></div>)HTML");

  auto* ident =
      MakeGarbageCollected<CSSCustomIdentValue>(AtomicString("testpainter"));
  CSSPaintValue* paint_value = MakeGarbageCollected<CSSPaintValue>(ident, true);

  EXPECT_EQ(paint_value->NumberOfGeneratorsForTesting(), 0u);
  // There is no generator, so returning a nullptr.
  EXPECT_EQ(paint_value->CustomInvalidationProperties(GetDocument()), nullptr);
}

TEST_P(CSSPaintValueTest, PrintingMustFallbackToMainThread) {
  if (!RuntimeEnabledFeatures::OffMainThreadCSSPaintEnabled()) {
    return;
  }

  NiceMock<MockCSSPaintImageGenerator>* mock_generator =
      MakeGarbageCollected<NiceMock<MockCSSPaintImageGenerator>>();
  base::AutoReset<MockCSSPaintImageGenerator*> scoped_override_generator(
      &g_override_generator, mock_generator);
  base::AutoReset<CSSPaintImageGenerator::CSSPaintImageGeneratorCreateFunction>
      scoped_create_function(
          CSSPaintImageGenerator::GetCreateFunctionForTesting(),
          ProvideOverrideGenerator);

  const gfx::SizeF target_size(100, 100);

  SetBodyInnerHTML(R"HTML(
    <div id="target"></div>
  )HTML");
  LayoutObject* target = GetLayoutObjectByElementId("target");
  const ComputedStyle& style = *target->Style();

  auto* ident =
      MakeGarbageCollected<CSSCustomIdentValue>(AtomicString("testpainter"));
  CSSPaintValue* paint_value = MakeGarbageCollected<CSSPaintValue>(ident, true);

  ON_CALL(*mock_generator, IsImageGeneratorReady()).WillByDefault(Return(true));
  // This PW can be composited, so we should only fall back to main once, in
  // the case where we are printing.
  EXPECT_CALL(*mock_generator, Paint(_, _, _))
      .Times(1)
      .WillOnce(
          Return(PaintGeneratedImage::Create(PaintRecord(), target_size)));

  ASSERT_TRUE(
      paint_value->GetImage(*target, GetDocument(), style, target_size));

  // Start printing; our paint should run on the main thread (and thus call
  // Paint).
  GetDocument().SetPrinting(Document::kPrinting);
  ASSERT_TRUE(
      paint_value->GetImage(*target, GetDocument(), style, target_size));

  // Stop printing; we should return to the compositor.
  GetDocument().SetPrinting(Document::kNotPrinting);
  ASSERT_TRUE(
      paint_value->GetImage(*target, GetDocument(), style, target_size));
}

// Regression test for https://crbug.com/835589.
TEST_P(CSSPaintValueTest, DoNotPaintForLink) {
  SetBodyInnerHTML(R"HTML(
    <style>
      a {
        background-image: paint(linkpainter);
        width: 100px;
        height: 100px;
      }
    </style>
    <a href="http://www.example.com" id="target"></a>
  )HTML");
  LayoutObject* target = GetLayoutObjectByElementId("target");
  const ComputedStyle& style = *target->Style();
  ASSERT_NE(style.InsideLink(), EInsideLink::kNotInsideLink);

  auto* ident =
      MakeGarbageCollected<CSSCustomIdentValue>(AtomicString("linkpainter"));
  CSSPaintValue* paint_value = MakeGarbageCollected<CSSPaintValue>(ident, true);
  EXPECT_FALSE(paint_value->GetImage(*target, GetDocument(), style,
                                     gfx::SizeF(100, 100)));
}

// Regression test for https://crbug.com/835589.
TEST_P(CSSPaintValueTest, DoNotPaintWhenAncestorHasLink) {
  SetBodyInnerHTML(R"HTML(
    <style>
      a {
        width: 200px;
        height: 200px;
      }
      b {
        background-image: paint(linkpainter);
        width: 100px;
        height: 100px;
      }
    </style>
    <a href="http://www.example.com" id="ancestor">
      <b id="target"></b>
    </a>
  )HTML");
  LayoutObject* target = GetLayoutObjectByElementId("target");
  const ComputedStyle& style = *target->Style();
  ASSERT_NE(style.InsideLink(), EInsideLink::kNotInsideLink);

  auto* ident =
      MakeGarbageCollected<CSSCustomIdentValue>(AtomicString("linkpainter"));
  CSSPaintValue* paint_value = MakeGarbageCollected<CSSPaintValue>(ident, true);
  EXPECT_FALSE(paint_value->GetImage(*target, GetDocument(), style,
                                     gfx::SizeF(100, 100)));
}

TEST_P(CSSPaintValueTest, BuildInputArgumentValuesNotCrash) {
  auto* ident =
      MakeGarbageCollected<CSSCustomIdentValue>(AtomicString("testpainter"));
  CSSPaintValue* paint_value = MakeGarbageCollected<CSSPaintValue>(ident, true);

  ASSERT_EQ(paint_value->GetParsedInputArgumentsForTesting(), nullptr);
  Vector<std::unique_ptr<CrossThreadStyleValue>> cross_thread_input_arguments;
  paint_value->BuildInputArgumentValuesForTesting(cross_thread_input_arguments);
  EXPECT_EQ(cross_thread_input_arguments.size(), 0u);
}

}  // namespace blink
