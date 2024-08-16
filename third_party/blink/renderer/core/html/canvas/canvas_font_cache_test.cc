// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/canvas/canvas_font_cache.h"

#include <memory>
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/dom_implementation.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_context_creation_attributes_core.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_rendering_context.h"
#include "third_party/blink/renderer/core/html/canvas/html_canvas_element.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"

namespace blink {

class CanvasFontCacheTest : public PageTestBase {
 protected:
  CanvasFontCacheTest();
  void SetUp() override;

  HTMLCanvasElement& CanvasElement() const { return *canvas_element_; }
  CanvasRenderingContext* Context2D() const;
  CanvasFontCache* Cache() { return GetDocument().GetCanvasFontCache(); }

 private:
  Persistent<HTMLCanvasElement> canvas_element_;
};

CanvasFontCacheTest::CanvasFontCacheTest() = default;

CanvasRenderingContext* CanvasFontCacheTest::Context2D() const {
  // If the following check fails, perhaps you forgot to call createContext
  // in your test?
  CHECK(CanvasElement().RenderingContext());
  CHECK(CanvasElement().RenderingContext()->IsRenderingContext2D());
  return CanvasElement().RenderingContext();
}

void CanvasFontCacheTest::SetUp() {
  PageTestBase::SetUp();
  GetDocument().documentElement()->setInnerHTML(
      "<body><canvas id='c'></canvas></body>");
  UpdateAllLifecyclePhasesForTest();
  canvas_element_ =
      To<HTMLCanvasElement>(GetDocument().getElementById(AtomicString("c")));
  String canvas_type("2d");
  CanvasContextCreationAttributesCore attributes;
  attributes.alpha = true;
  canvas_element_->GetCanvasRenderingContext(canvas_type, attributes);
  Context2D();  // Calling this for the checks
}

TEST_F(CanvasFontCacheTest, CacheHardLimit) {
  for (unsigned i = 0; i < Cache()->HardMaxFonts() + 1; ++i) {
    String font_string;
    font_string = String::Number(i + 1) + "px sans-serif";
    Context2D()->setFontForTesting(font_string);
    if (i < Cache()->HardMaxFonts()) {
      EXPECT_TRUE(Cache()->IsInCache("1px sans-serif"));
    } else {
      EXPECT_FALSE(Cache()->IsInCache("1px sans-serif"));
    }
    EXPECT_TRUE(Cache()->IsInCache(font_string));
  }
}

TEST_F(CanvasFontCacheTest, PageVisibilityChange) {
  Context2D()->setFontForTesting("10px sans-serif");
  EXPECT_TRUE(Cache()->IsInCache("10px sans-serif"));
  GetPage().SetVisibilityState(mojom::blink::PageVisibilityState::kHidden,
                               /*is_initial_state=*/false);
  EXPECT_FALSE(Cache()->IsInCache("10px sans-serif"));

  Context2D()->setFontForTesting("15px sans-serif");
  EXPECT_FALSE(Cache()->IsInCache("10px sans-serif"));
  EXPECT_TRUE(Cache()->IsInCache("15px sans-serif"));

  Context2D()->setFontForTesting("10px sans-serif");
  EXPECT_TRUE(Cache()->IsInCache("10px sans-serif"));
  EXPECT_FALSE(Cache()->IsInCache("15px sans-serif"));

  GetPage().SetVisibilityState(mojom::blink::PageVisibilityState::kVisible,
                               /*is_initial_state=*/false);
  Context2D()->setFontForTesting("15px sans-serif");
  Context2D()->setFontForTesting("10px sans-serif");
  EXPECT_TRUE(Cache()->IsInCache("10px sans-serif"));
  EXPECT_TRUE(Cache()->IsInCache("15px sans-serif"));
}

TEST_F(CanvasFontCacheTest, CreateDocumentFontCache) {
  // Create a document via script not connected to a tab or frame.
  Document* document = GetDocument().implementation().createHTMLDocument();
  // This document should also create a CanvasFontCache and should not crash.
  EXPECT_TRUE(document->GetCanvasFontCache());
}

// Regression test for crbug.com/1421699.
// When page becomes hidden the cache should be cleared. If this does not
// happen, setFontForTesting() should clear the cache instead.
TEST_F(CanvasFontCacheTest, HardMaxFontsOnPageVisibility) {
  GetPage().SetVisibilityState(mojom::blink::PageVisibilityState::kVisible,
                               /*is_initial_state=*/false);
  // Fill up the font cache.
  for (unsigned i = 0; i < Cache()->HardMaxFonts(); ++i) {
    String font_string;
    font_string = String::Number(i + 1) + "px sans-serif";
    Context2D()->setFontForTesting(font_string);
    EXPECT_TRUE(Cache()->IsInCache(font_string));
    EXPECT_EQ(Cache()->GetCacheSize(), i + 1);
  }
  EXPECT_EQ(Cache()->GetCacheSize(), Cache()->HardMaxFonts());

  // Set initial state to true to not trigger a flush.
  GetPage().SetVisibilityState(mojom::blink::PageVisibilityState::kHidden,
                               /*is_initial_state=*/true);
  // Set font should detect that things are out-of-sync and clear the cache.
  Context2D()->setFontForTesting("15px serif");
  EXPECT_EQ(Cache()->GetCacheSize(), 1u);
}

}  // namespace blink
