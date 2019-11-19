// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/canvas/canvas_font_cache.h"

#include <memory>
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_context_creation_attributes_core.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_rendering_context.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"

using testing::Mock;

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
  EXPECT_NE(nullptr, CanvasElement().RenderingContext());
  EXPECT_TRUE(CanvasElement().RenderingContext()->Is2d());
  return CanvasElement().RenderingContext();
}

void CanvasFontCacheTest::SetUp() {
  PageTestBase::SetUp();
  GetDocument().documentElement()->SetInnerHTMLFromString(
      "<body><canvas id='c'></canvas></body>");
  UpdateAllLifecyclePhasesForTest();
  canvas_element_ = To<HTMLCanvasElement>(GetDocument().getElementById("c"));
  String canvas_type("2d");
  CanvasContextCreationAttributesCore attributes;
  attributes.alpha = true;
  canvas_element_->GetCanvasRenderingContext(canvas_type, attributes);
  Context2D();  // Calling this for the checks
}

TEST_F(CanvasFontCacheTest, CacheHardLimit) {
  String font_string;
  unsigned i;
  for (i = 0; i < Cache()->HardMaxFonts() + 1; i++) {
    font_string = String::Number(i + 1) + "px sans-serif";
    Context2D()->setFont(font_string);
    if (i < Cache()->HardMaxFonts()) {
      EXPECT_TRUE(Cache()->IsInCache("1px sans-serif"));
    } else {
      EXPECT_FALSE(Cache()->IsInCache("1px sans-serif"));
    }
    EXPECT_TRUE(Cache()->IsInCache(font_string));
  }
}

TEST_F(CanvasFontCacheTest, PageVisibilityChange) {
  Context2D()->setFont("10px sans-serif");
  EXPECT_TRUE(Cache()->IsInCache("10px sans-serif"));
  GetPage().SetVisibilityState(PageVisibilityState::kHidden,
                               /*initial_state=*/false);
  EXPECT_FALSE(Cache()->IsInCache("10px sans-serif"));

  Context2D()->setFont("15px sans-serif");
  EXPECT_FALSE(Cache()->IsInCache("10px sans-serif"));
  EXPECT_TRUE(Cache()->IsInCache("15px sans-serif"));

  Context2D()->setFont("10px sans-serif");
  EXPECT_TRUE(Cache()->IsInCache("10px sans-serif"));
  EXPECT_FALSE(Cache()->IsInCache("15px sans-serif"));

  GetPage().SetVisibilityState(PageVisibilityState::kVisible,
                               /*initial_state=*/false);
  Context2D()->setFont("15px sans-serif");
  Context2D()->setFont("10px sans-serif");
  EXPECT_TRUE(Cache()->IsInCache("10px sans-serif"));
  EXPECT_TRUE(Cache()->IsInCache("15px sans-serif"));
}

}  // namespace blink
