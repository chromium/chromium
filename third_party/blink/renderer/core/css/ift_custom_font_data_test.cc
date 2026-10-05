// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/ift_custom_font_data.h"

#include <memory>
#include <optional>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/css/css_font_face.h"
#include "third_party/blink/renderer/core/css/css_font_face_source.h"
#include "third_party/blink/renderer/core/css/css_font_selector.h"
#include "third_party/blink/renderer/core/css/font_face.h"
#include "third_party/blink/renderer/core/css/font_face_cache.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"
#include "third_party/blink/renderer/platform/fonts/custom_font_data.h"
#include "third_party/blink/renderer/platform/fonts/font_description.h"
#include "third_party/blink/renderer/platform/fonts/font_selection_types.h"
#include "third_party/blink/renderer/platform/fonts/simple_font_data.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {
namespace {

class IftCustomFontDataTest : public SimTest {
 protected:
  void SetUp() override {
    SimTest::SetUp();
    scoped_feature_ =
        std::make_unique<ScopedIncrementalFontTransferForTest>(true);
  }

  // Loads a document with an IFT `@font-face` and returns the resulting
  // `CustomFontData`.
  const CustomFontData* LoadCustomFontData() {
    SimRequest main_resource("https://example.com/index.html", "text/html");
    SimSubresourceRequest font_resource(
        "https://example.com/fonts/roboto-ift.ttf", "font/ttf");

    LoadURL("https://example.com/index.html");
    main_resource.Complete(R"HTML(
      <!doctype html>
      <style>
        @font-face {
          font-family: ift-font;
          src: url(https://example.com/fonts/roboto-ift.ttf);
        }
        #target {
          font-family: ift-font;
        }
      </style>
      <span id="target">a</span>
    )HTML");

    Compositor().BeginFrame();
    std::optional<Vector<char>> font_bytes =
        test::ReadFromFile(test::PlatformTestDataPath("roboto-ift.ttf"));
    CHECK(font_bytes.has_value());
    font_resource.Complete(*font_bytes);

    const auto& faces = GetDocument()
                            .GetStyleEngine()
                            .GetFontSelector()
                            ->GetFontFaceCache()
                            ->CssConnectedFontFaces();
    CHECK(!faces.empty());
    FontFace* font_face = faces.begin()->Get();
    auto* source =
        const_cast<CSSFontFaceSource*>(font_face->CssFontFace()->FrontSource());
    if (!source) {
      return nullptr;
    }
    const SimpleFontData* font_data =
        source->GetFontData(FontDescription(), FontSelectionCapabilities());
    return font_data ? font_data->GetCustomFontData() : nullptr;
  }

 private:
  std::unique_ptr<ScopedIncrementalFontTransferForTest> scoped_feature_;
};

TEST_F(IftCustomFontDataTest, StoresIftCustomFontData) {
  const CustomFontData* custom_font_data = LoadCustomFontData();
  ASSERT_NE(custom_font_data, nullptr);
  // Unlike the default `CustomFontData`, `IftCustomFontData` does not yet
  // support any text.
  EXPECT_FALSE(custom_font_data->IftRequireSubset("a"));
}

}  // namespace
}  // namespace blink
