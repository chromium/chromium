// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/core/css/font_face_set_document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

namespace blink {

class FontDisplayAutoLCPAlignTest : public SimTest {
 public:
  static Vector<char> ReadAhemWoff2() {
    return *test::ReadFromFile(test::CoreTestDataPath("Ahem.woff2"));
  }

  static Vector<char> ReadMaterialIconsWoff2() {
    return *test::ReadFromFile(
        test::CoreTestDataPath("MaterialIcons-Regular.woff2"));
  }

 protected:
  Element* GetTarget() {
    return GetDocument().getElementById(AtomicString("target"));
  }

  const Font& GetFont(const Element* element) {
    return element->GetLayoutObject()->Style()->GetFont();
  }

  const Font& GetTargetFont() { return GetFont(GetTarget()); }
};

TEST_F(FontDisplayAutoLCPAlignTest, FontFinishesBeforeLCPLimit) {
  SimRequest main_resource("https://example.com", "text/html");
  SimSubresourceRequest font_resource("https://example.com/Ahem.woff2",
                                      "font/woff2");

  LoadURL("https://example.com");
  main_resource.Complete(R"HTML(
    <!doctype html>
    <style>
      @font-face {
        font-family: custom-font;
        src: url(https://example.com/Ahem.woff2) format("woff2");
      }
      #target {
        font: 25px/1 custom-font, monospace;
      }
    </style>
    <span id=target style="position:relative">0123456789</span>
  )HTML");

  // The first frame is rendered with invisible fallback, as the web font is
  // still loading, and is in the block display period.
  Compositor().BeginFrame();
  EXPECT_GT(250, GetTarget()->OffsetWidth());
  EXPECT_TRUE(GetTargetFont().ShouldSkipDrawing());

  font_resource.Complete(ReadAhemWoff2());

  // The next frame is rendered with the web font.
  Compositor().BeginFrame();
  EXPECT_EQ(250, GetTarget()->OffsetWidth());
  EXPECT_FALSE(GetTargetFont().ShouldSkipDrawing());
}

TEST_F(FontDisplayAutoLCPAlignTest, FontFinishesAfterLCPLimit) {
  SimRequest main_resource("https://example.com", "text/html");
  SimSubresourceRequest font_resource("https://example.com/Ahem.woff2",
                                      "font/woff2");

  LoadURL("https://example.com");
  main_resource.Complete(R"HTML(
    <!doctype html>
    <style>
      @font-face {
        font-family: custom-font;
        src: url(https://example.com/Ahem.woff2) format("woff2");
      }
      #target {
        font: 25px/1 custom-font, monospace;
      }
    </style>
    <span id=target style="position:relative">0123456789</span>
  )HTML");

  // The first frame is rendered with invisible fallback, as the web font is
  // still loading, and is in the block display period.
  Compositor().BeginFrame();
  EXPECT_GT(250, GetTarget()->OffsetWidth());
  EXPECT_TRUE(GetTargetFont().ShouldSkipDrawing());

  // Wait until we reach the LCP limit, and the relevant timeout fires.
  test::RunDelayedTasks(DocumentLoader::kLCPLimit);

  // After reaching the LCP limit, the web font should enter the swap
  // display period. We should render visible fallback for it.
  Compositor().BeginFrame();
  EXPECT_GT(250, GetTarget()->OffsetWidth());
  EXPECT_FALSE(GetTargetFont().ShouldSkipDrawing());

  font_resource.Complete(ReadAhemWoff2());

  // The web font swaps in after finishing loading.
  Compositor().BeginFrame();
  EXPECT_EQ(250, GetTarget()->OffsetWidth());
  EXPECT_FALSE(GetTargetFont().ShouldSkipDrawing());
}

TEST_F(FontDisplayAutoLCPAlignTest, FontFaceAddedAfterLCPLimit) {
  SimRequest main_resource("https://example.com", "text/html");
  SimSubresourceRequest font_resource("https://example.com/Ahem.woff2",
                                      "font/woff2");

  LoadURL("https://example.com");
  main_resource.Write("<!doctype html>");

  // Wait until we reach the LCP limit, and the relevant timeout fires.
  test::RunDelayedTasks(DocumentLoader::kLCPLimit);

  main_resource.Complete(R"HTML(
    <style>
      @font-face {
        font-family: custom-font;
        src: url(https://example.com/Ahem.woff2) format("woff2");
      }
      #target {
        font: 25px/1 custom-font, monospace;
      }
    </style>
    <span id=target style="position:relative">0123456789</span>
  )HTML");

  font_resource.Complete(ReadAhemWoff2());

  // The web font swaps in after finishing loading.
  Compositor().BeginFrame();
  EXPECT_EQ(250, GetTarget()->OffsetWidth());
  EXPECT_FALSE(GetTargetFont().ShouldSkipDrawing());
}

TEST_F(FontDisplayAutoLCPAlignTest, FontFaceInMemoryCacheAddedAfterLCPLimit) {
  SimRequest main_resource("https://example.com", "text/html");
  SimSubresourceRequest font_resource("https://example.com/Ahem.woff2",
                                      "font/woff2");

  LoadURL("https://example.com");
  main_resource.Write(R"HTML(
    <!doctype html>
    <link rel="preload" as="font" type="font/woff2"
          href="https://example.com/Ahem.woff2" crossorigin>
  )HTML");

  font_resource.Complete(ReadAhemWoff2());

  // Wait until we reach the LCP limit, and the relevant timeout fires.
  test::RunDelayedTasks(DocumentLoader::kLCPLimit);

  main_resource.Complete(R"HTML(
    <style>
      @font-face {
        font-family: custom-font;
        src: url(https://example.com/Ahem.woff2) format("woff2");
      }
      #target {
        font: 25px/1 custom-font, monospace;
      }
    </style>
    <span id=target style="position:relative">0123456789</span>
  )HTML");

  // The font face is added after the LCP limit, but it's already preloaded and
  // available from the memory cache. We'll render with it as it's immediate
  // available.
  Compositor().BeginFrame();
  EXPECT_EQ(250, GetTarget()->OffsetWidth());
  EXPECT_FALSE(GetTargetFont().ShouldSkipDrawing());
}

}  // namespace blink
