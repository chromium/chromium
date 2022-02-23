// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/loader/font_preload_manager.h"

#include "base/test/scoped_feature_list.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/html/html_head_element.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/layout/layout_shift_tracker.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

namespace blink {

class FontPreloadManagerTest : public SimTest {
 public:
  static Vector<char> ReadAhemWoff2() {
    return test::ReadFromFile(test::CoreTestDataPath("Ahem.woff2"))
        ->CopyAs<Vector<char>>();
  }

 protected:
  FontPreloadManager& GetFontPreloadManager() {
    return GetDocument().GetFontPreloadManager();
  }

  using State = FontPreloadManager::State;
  State GetState() { return GetFontPreloadManager().state_; }

  void DisableFontPreloadManagerTimeout() {
    GetFontPreloadManager().DisableTimeoutForTest();
  }

  Element* GetTarget() { return GetDocument().getElementById("target"); }

  const Font& GetTargetFont() {
    return GetTarget()->GetLayoutObject()->Style()->GetFont();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(FontPreloadManagerTest, FastFontFinishBeforeBody) {
  SimRequest main_resource("https://example.com", "text/html");
  SimSubresourceRequest font_resource("https://example.com/font.woff",
                                      "font/woff2");

  LoadURL("https://example.com");
  main_resource.Write(R"HTML(
    <!doctype html>
    <head>
      <link rel="preload" as="font" type="font/woff2"
            href="https://example.com/font.woff">
  )HTML");

  // Make sure timer doesn't fire in case the test runs slow.
  GetFontPreloadManager().SetRenderDelayTimeoutForTest(base::TimeDelta::Max());

  // Rendering is blocked due to ongoing font preloading.
  EXPECT_TRUE(Compositor().DeferMainFrameUpdate());
  EXPECT_TRUE(GetFontPreloadManager().HasPendingRenderBlockingFonts());
  EXPECT_EQ(State::kLoading, GetState());

  font_resource.Complete();
  test::RunPendingTasks();

  // Font preloading no longer blocks renderings. However, rendering is still
  // blocked, as we don't have BODY yet.
  EXPECT_TRUE(Compositor().DeferMainFrameUpdate());
  EXPECT_FALSE(GetFontPreloadManager().HasPendingRenderBlockingFonts());
  EXPECT_EQ(State::kLoaded, GetState());

  main_resource.Complete("</head><body>some text</body>");

  // Rendering starts after BODY has arrived, as the font was loaded earlier.
  EXPECT_FALSE(Compositor().DeferMainFrameUpdate());
  EXPECT_FALSE(GetFontPreloadManager().HasPendingRenderBlockingFonts());
  EXPECT_EQ(State::kUnblocked, GetState());
}

TEST_F(FontPreloadManagerTest, FastFontFinishAfterBody) {
  SimRequest main_resource("https://example.com", "text/html");
  SimSubresourceRequest font_resource("https://example.com/font.woff",
                                      "font/woff2");

  LoadURL("https://example.com");
  main_resource.Write(R"HTML(
    <!doctype html>
    <head>
      <link rel="preload" as="font" type="font/woff2"
            href="https://example.com/font.woff">
  )HTML");

  // Rendering is blocked due to ongoing font preloading.
  EXPECT_TRUE(Compositor().DeferMainFrameUpdate());
  EXPECT_TRUE(GetFontPreloadManager().HasPendingRenderBlockingFonts());
  EXPECT_EQ(State::kLoading, GetState());

  main_resource.Complete("</head><body>some text</body>");

  // Rendering is still blocked by font, even if we already have BODY, because
  // the font was *not* loaded earlier.
  EXPECT_TRUE(Compositor().DeferMainFrameUpdate());
  EXPECT_TRUE(GetFontPreloadManager().HasPendingRenderBlockingFonts());
  EXPECT_EQ(State::kLoading, GetState());

  font_resource.Complete();
  test::RunPendingTasks();

  // Rendering starts after font preloading has finished.
  EXPECT_FALSE(Compositor().DeferMainFrameUpdate());
  EXPECT_FALSE(GetFontPreloadManager().HasPendingRenderBlockingFonts());
  EXPECT_EQ(State::kUnblocked, GetState());
}

TEST_F(FontPreloadManagerTest, SlowFontTimeoutBeforeBody) {
  SimRequest main_resource("https://example.com", "text/html");
  SimSubresourceRequest font_resource("https://example.com/font.woff",
                                      "font/woff2");

  LoadURL("https://example.com");
  main_resource.Write(R"HTML(
    <!doctype html>
    <head>
      <link rel="preload" as="font" type="font/woff2"
            href="https://example.com/font.woff">
  )HTML");

  // Rendering is blocked due to ongoing font preloading.
  EXPECT_TRUE(Compositor().DeferMainFrameUpdate());
  EXPECT_TRUE(GetFontPreloadManager().HasPendingRenderBlockingFonts());
  EXPECT_EQ(State::kLoading, GetState());

  GetFontPreloadManager().FontPreloadingDelaysRenderingTimerFired(nullptr);

  // Font preloading no longer blocks renderings after the timeout fires.
  // However, rendering is still blocked, as we don't have BODY yet.
  EXPECT_TRUE(Compositor().DeferMainFrameUpdate());
  EXPECT_FALSE(GetFontPreloadManager().HasPendingRenderBlockingFonts());
  EXPECT_EQ(State::kUnblocked, GetState());

  main_resource.Complete("</head><body>some text</body>");

  // Rendering starts after BODY has arrived.
  EXPECT_FALSE(Compositor().DeferMainFrameUpdate());
  EXPECT_FALSE(GetFontPreloadManager().HasPendingRenderBlockingFonts());
  EXPECT_EQ(State::kUnblocked, GetState());

  font_resource.Complete();
}

TEST_F(FontPreloadManagerTest, SlowFontTimeoutAfterBody) {
  SimRequest main_resource("https://example.com", "text/html");
  SimSubresourceRequest font_resource("https://example.com/font.woff",
                                      "font/woff2");

  LoadURL("https://example.com");
  main_resource.Write(R"HTML(
    <!doctype html>
    <head>
      <link rel="preload" as="font" type="font/woff2"
            href="https://example.com/font.woff">
  )HTML");

  // Rendering is blocked due to ongoing font preloading.
  EXPECT_TRUE(Compositor().DeferMainFrameUpdate());
  EXPECT_TRUE(GetFontPreloadManager().HasPendingRenderBlockingFonts());
  EXPECT_EQ(State::kLoading, GetState());

  main_resource.Complete("</head><body>some text</body>");

  // Rendering is still blocked by font, even if we already have BODY.
  EXPECT_TRUE(Compositor().DeferMainFrameUpdate());
  EXPECT_TRUE(GetFontPreloadManager().HasPendingRenderBlockingFonts());
  EXPECT_EQ(State::kLoading, GetState());

  GetFontPreloadManager().FontPreloadingDelaysRenderingTimerFired(nullptr);

  // Rendering starts after we've waited for the font preloading long enough.
  EXPECT_FALSE(Compositor().DeferMainFrameUpdate());
  EXPECT_FALSE(GetFontPreloadManager().HasPendingRenderBlockingFonts());
  EXPECT_EQ(State::kUnblocked, GetState());

  font_resource.Complete();
}

// A trivial test case to verify test setup
TEST_F(FontPreloadManagerTest, RegularWebFont) {
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

  // Now rendering has started, as there's no blocking resources.
  EXPECT_FALSE(Compositor().DeferMainFrameUpdate());
  EXPECT_EQ(State::kUnblocked, GetState());

  font_resource.Complete(ReadAhemWoff2());

  // Now everything is loaded. The web font should be used in rendering.
  Compositor().BeginFrame().DrawCount();
  EXPECT_EQ(250, GetTarget()->OffsetWidth());
  EXPECT_FALSE(GetTargetFont().ShouldSkipDrawing());
}

TEST_F(FontPreloadManagerTest, OptionalFontWithoutPreloading) {
  SimRequest main_resource("https://example.com", "text/html");
  SimSubresourceRequest font_resource("https://example.com/Ahem.woff2",
                                      "font/woff2");

  LoadURL("https://example.com");
  main_resource.Write(R"HTML(
    <!doctype html>
    <style>
      @font-face {
        font-family: custom-font;
        src: url(https://example.com/Ahem.woff2) format("woff2");
        font-display: optional;
      }
      #target {
        font: 25px/1 custom-font, monospace;
      }
    </style>
    <span id=target>0123456789</span>
    <script>document.fonts.load('25px/1 custom-font');</script>
  )HTML");

  // Now rendering has started, as there's no blocking resources.
  EXPECT_FALSE(Compositor().DeferMainFrameUpdate());
  EXPECT_EQ(State::kUnblocked, GetState());

  font_resource.Complete(ReadAhemWoff2());

  // Although the optional web font isn't preloaded, it finished loading before
  // the first time we try to render with it. Therefore it's used.
  Compositor().BeginFrame().Contains(SimCanvas::kText);
  EXPECT_EQ(250, GetTarget()->OffsetWidth());
  EXPECT_FALSE(GetTargetFont().ShouldSkipDrawing());

  main_resource.Finish();
}

TEST_F(FontPreloadManagerTest, OptionalFontMissingFirstFrame) {
  SimRequest main_resource("https://example.com", "text/html");
  SimSubresourceRequest font_resource("https://example.com/Ahem.woff2",
                                      "font/woff2");

  LoadURL("https://example.com");
  main_resource.Write(R"HTML(
    <!doctype html>
    <style>
      @font-face {
        font-family: custom-font;
        src: url(https://example.com/Ahem.woff2) format("woff2");
        font-display: optional;
      }
      #target {
        font: 25px/1 custom-font, monospace;
      }
    </style>
    <span id=target>0123456789</span>
  )HTML");

  // Now rendering has started, as there's no blocking resources.
  EXPECT_FALSE(Compositor().DeferMainFrameUpdate());
  EXPECT_EQ(State::kUnblocked, GetState());

  // We render visible fallback as the 'optional' web font hasn't loaded.
  Compositor().BeginFrame();
  EXPECT_GT(250, GetTarget()->OffsetWidth());
  EXPECT_FALSE(GetTargetFont().ShouldSkipDrawing());

  font_resource.Complete(ReadAhemWoff2());

  // Since we have rendered fallback for the 'optional' font, even after it
  // finishes loading, we shouldn't use it, as otherwise there's a relayout.
  Compositor().BeginFrame();
  EXPECT_GT(250, GetTarget()->OffsetWidth());
  EXPECT_FALSE(GetTargetFont().ShouldSkipDrawing());

  main_resource.Finish();
}

TEST_F(FontPreloadManagerTest, OptionalFontForcedLayoutNoLayoutShift) {
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
        font-display: optional;
      }
      #target {
        font: 25px/1 custom-font, monospace;
      }
    </style>
    <span id=target>0123456789</span>
    <span>Element to track layout shift when font changes</span>
  )HTML");

  // Now rendering has started, as there's no blocking resources.
  EXPECT_FALSE(Compositor().DeferMainFrameUpdate());
  EXPECT_EQ(State::kUnblocked, GetState());

  // Force layout update, which lays out target but doesn't paint anything.
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);
  test::RunPendingTasks();

  EXPECT_GT(250, GetTarget()->OffsetWidth());

  // Can't check ShouldSkipDrawing(), as it calls PaintRequested() on the font.

  font_resource.Complete(ReadAhemWoff2());

  // Even though target has been laid out with a fallback font, we can still
  // relayout with the web font since it hasn't been painted yet, which means
  // relayout and repaint do not cause layout shifting.
  Compositor().BeginFrame();
  test::RunPendingTasks();
  EXPECT_EQ(250, GetTarget()->OffsetWidth());
  EXPECT_FALSE(GetTargetFont().ShouldSkipDrawing());
  EXPECT_EQ(0.0, GetDocument().View()->GetLayoutShiftTracker().Score());
}

TEST_F(FontPreloadManagerTest, OptionalFontRemoveAndReadd) {
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
        font-display: optional;
      }
      #target {
        font: 25px/1 custom-font, monospace;
      }
    </style>
    <span id=target>0123456789</span>
  )HTML");

  // Now rendering has started, as there's no blocking resources.
  EXPECT_FALSE(Compositor().DeferMainFrameUpdate());
  EXPECT_EQ(State::kUnblocked, GetState());

  // The 'optional' web font isn't used, as it didn't finish loading before
  // rendering started. Text is rendered in visible fallback.
  Compositor().BeginFrame().Contains(SimCanvas::kText);
  EXPECT_GT(250, GetTarget()->OffsetWidth());
  EXPECT_FALSE(GetTargetFont().ShouldSkipDrawing());

  font_resource.Complete(ReadAhemWoff2());

  Element* style = GetDocument().QuerySelector("style");
  style->remove();
  GetDocument().head()->appendChild(style);

  // After removing and readding the style sheet, we've created a new font face
  // that got loaded immediately from the memory cache. So it can be used.
  Compositor().BeginFrame().Contains(SimCanvas::kText);
  EXPECT_EQ(250, GetTarget()->OffsetWidth());
  EXPECT_FALSE(GetTargetFont().ShouldSkipDrawing());
}

TEST_F(FontPreloadManagerTest, OptionalFontSlowPreloading) {
  SimRequest main_resource("https://example.com", "text/html");
  SimSubresourceRequest font_resource("https://example.com/Ahem.woff2",
                                      "font/woff2");

  LoadURL("https://example.com");
  main_resource.Complete(R"HTML(
    <!doctype html>
    <link rel="preload" as="font" type="font/woff2"
          href="https://example.com/Ahem.woff2" crossorigin>
    <style>
      @font-face {
        font-family: custom-font;
        src: url(https://example.com/Ahem.woff2) format("woff2");
        font-display: optional;
      }
      #target {
        font: 25px/1 custom-font, monospace;
      }
    </style>
    <span id=target>0123456789</span>
  )HTML");

  // Rendering is blocked due to font being preloaded.
  EXPECT_TRUE(Compositor().DeferMainFrameUpdate());
  EXPECT_TRUE(GetFontPreloadManager().HasPendingRenderBlockingFonts());
  EXPECT_EQ(State::kLoading, GetState());

  GetFontPreloadManager().FontPreloadingDelaysRenderingTimerFired(nullptr);

  // Rendering is unblocked after the font preloading has timed out.
  EXPECT_FALSE(Compositor().DeferMainFrameUpdate());
  EXPECT_FALSE(GetFontPreloadManager().HasPendingRenderBlockingFonts());
  EXPECT_EQ(State::kUnblocked, GetState());

  // First frame renders text with visible fallback, as the 'optional' web font
  // isn't loaded yet, and should be treated as in the failure period.
  Compositor().BeginFrame();
  EXPECT_GT(250, GetTarget()->OffsetWidth());
  EXPECT_FALSE(GetTargetFont().ShouldSkipDrawing());

  font_resource.Complete(ReadAhemWoff2());

  // The 'optional' web font should not cause relayout even if it finishes
  // loading now.
  Compositor().BeginFrame();
  EXPECT_GT(250, GetTarget()->OffsetWidth());
  EXPECT_FALSE(GetTargetFont().ShouldSkipDrawing());
}

TEST_F(FontPreloadManagerTest, OptionalFontFastPreloading) {
  SimRequest main_resource("https://example.com", "text/html");
  SimSubresourceRequest font_resource("https://example.com/Ahem.woff2",
                                      "font/woff2");

  LoadURL("https://example.com");
  main_resource.Complete(R"HTML(
    <!doctype html>
    <link rel="preload" as="font" type="font/woff2"
          href="https://example.com/Ahem.woff2" crossorigin>
    <style>
      @font-face {
        font-family: custom-font;
        src: url(https://example.com/Ahem.woff2) format("woff2");
        font-display: optional;
      }
      #target {
        font: 25px/1 custom-font, monospace;
      }
    </style>
    <span id=target>0123456789</span>
  )HTML");

  // Rendering is blocked due to font being preloaded.
  EXPECT_TRUE(Compositor().DeferMainFrameUpdate());
  EXPECT_TRUE(GetFontPreloadManager().HasPendingRenderBlockingFonts());
  EXPECT_EQ(State::kLoading, GetState());

  // There are test flakes due to FontPreloadManager timeout firing before the
  // ResourceFinishObserver gets notified. So we disable the timeout.
  DisableFontPreloadManagerTimeout();

  font_resource.Complete(ReadAhemWoff2());
  test::RunPendingTasks();

  // Rendering is unblocked after the font is preloaded.
  EXPECT_FALSE(Compositor().DeferMainFrameUpdate());
  EXPECT_FALSE(GetFontPreloadManager().HasPendingRenderBlockingFonts());
  EXPECT_EQ(State::kUnblocked, GetState());

  // The 'optional' web font should be used in the first paint.
  Compositor().BeginFrame();
  EXPECT_EQ(250, GetTarget()->OffsetWidth());
  EXPECT_FALSE(GetTargetFont().ShouldSkipDrawing());
}

TEST_F(FontPreloadManagerTest, OptionalFontSlowImperativeLoad) {
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
        font-display: optional;
      }
      #target {
        font: 25px/1 custom-font, monospace;
      }
    </style>
    <script>
    document.fonts.load('25px/1 custom-font');
    </script>
    <span id=target>0123456789</span>
  )HTML");

  // Rendering is blocked due to font being loaded via JavaScript API.
  EXPECT_TRUE(Compositor().DeferMainFrameUpdate());
  EXPECT_TRUE(GetFontPreloadManager().HasPendingRenderBlockingFonts());
  EXPECT_EQ(State::kLoading, GetState());

  GetFontPreloadManager().FontPreloadingDelaysRenderingTimerFired(nullptr);

  // Rendering is unblocked after the font preloading has timed out.
  EXPECT_FALSE(Compositor().DeferMainFrameUpdate());
  EXPECT_FALSE(GetFontPreloadManager().HasPendingRenderBlockingFonts());
  EXPECT_EQ(State::kUnblocked, GetState());

  // First frame renders text with visible fallback, as the 'optional' web font
  // isn't loaded yet, and should be treated as in the failure period.
  Compositor().BeginFrame();
  EXPECT_GT(250, GetTarget()->OffsetWidth());
  EXPECT_FALSE(GetTargetFont().ShouldSkipDrawing());

  font_resource.Complete(ReadAhemWoff2());

  // The 'optional' web font should not cause relayout even if it finishes
  // loading now.
  Compositor().BeginFrame();
  EXPECT_GT(250, GetTarget()->OffsetWidth());
  EXPECT_FALSE(GetTargetFont().ShouldSkipDrawing());
}

TEST_F(FontPreloadManagerTest, OptionalFontFastImperativeLoad) {
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
        font-display: optional;
      }
      #target {
        font: 25px/1 custom-font, monospace;
      }
    </style>
    <script>
    document.fonts.load('25px/1 custom-font');
    </script>
    <span id=target>0123456789</span>
  )HTML");

  // Make sure timer doesn't fire in case the test runs slow.
  GetFontPreloadManager().SetRenderDelayTimeoutForTest(base::TimeDelta::Max());

  // Rendering is blocked due to font being preloaded.
  EXPECT_TRUE(Compositor().DeferMainFrameUpdate());
  EXPECT_TRUE(GetFontPreloadManager().HasPendingRenderBlockingFonts());
  EXPECT_EQ(State::kLoading, GetState());

  font_resource.Complete(ReadAhemWoff2());
  test::RunPendingTasks();

  // Rendering is unblocked after the font is preloaded.
  EXPECT_FALSE(Compositor().DeferMainFrameUpdate());
  EXPECT_FALSE(GetFontPreloadManager().HasPendingRenderBlockingFonts());
  EXPECT_EQ(State::kUnblocked, GetState());

  // The 'optional' web font should be used in the first paint.
  Compositor().BeginFrame();
  EXPECT_EQ(250, GetTarget()->OffsetWidth());
  EXPECT_FALSE(GetTargetFont().ShouldSkipDrawing());
}

}  // namespace blink
