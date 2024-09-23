// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/loader/render_blocking_resource_manager.h"

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

class RenderBlockingResourceManagerTest : public SimTest {
 public:
  static Vector<char> ReadAhemWoff2() {
    return *test::ReadFromFile(test::CoreTestDataPath("Ahem.woff2"));
  }

 protected:
  RenderBlockingResourceManager& GetRenderBlockingResourceManager() {
    return *GetDocument().GetRenderBlockingResourceManager();
  }

  bool HasRenderBlockingResources() {
    return GetRenderBlockingResourceManager().HasRenderBlockingResources();
  }

  void DisableFontPreloadTimeout() {
    GetRenderBlockingResourceManager().DisableFontPreloadTimeoutForTest();
  }
  void SetFontPreloadTimeout(base::TimeDelta timeout) {
    GetRenderBlockingResourceManager().SetFontPreloadTimeoutForTest(timeout);
  }
  bool FontPreloadTimerIsActive() {
    return GetRenderBlockingResourceManager().FontPreloadTimerIsActiveForTest();
  }

  Element* GetTarget() {
    return GetDocument().getElementById(AtomicString("target"));
  }

  const Font& GetTargetFont() {
    return GetTarget()->GetLayoutObject()->Style()->GetFont();
  }
};

TEST_F(RenderBlockingResourceManagerTest, FastFontFinishBeforeBody) {
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
  SetFontPreloadTimeout(base::Seconds(30));

  // Rendering is blocked due to ongoing font preloading.
  EXPECT_TRUE(Compositor().DeferMainFrameUpdate());
  EXPECT_TRUE(HasRenderBlockingResources());

  font_resource.Complete();
  test::RunPendingTasks();

  // Font preloading no longer blocks renderings. However, rendering is still
  // blocked, as we don't have BODY yet.
  EXPECT_TRUE(Compositor().DeferMainFrameUpdate());
  EXPECT_FALSE(HasRenderBlockingResources());

  main_resource.Complete("</head><body>some text</body>");

  // Rendering starts after BODY has arrived, as the font was loaded earlier.
  EXPECT_FALSE(Compositor().DeferMainFrameUpdate());
  EXPECT_FALSE(HasRenderBlockingResources());
}

TEST_F(RenderBlockingResourceManagerTest, FastFontFinishAfterBody) {
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
  EXPECT_TRUE(HasRenderBlockingResources());

  main_resource.Complete("</head><body>some text</body>");

  // Rendering is still blocked by font, even if we already have BODY, because
  // the font was *not* loaded earlier.
  EXPECT_TRUE(Compositor().DeferMainFrameUpdate());
  EXPECT_TRUE(HasRenderBlockingResources());

  font_resource.Complete();
  test::RunPendingTasks();

  // Rendering starts after font preloading has finished.
  EXPECT_FALSE(Compositor().DeferMainFrameUpdate());
  EXPECT_FALSE(HasRenderBlockingResources());
}

TEST_F(RenderBlockingResourceManagerTest, SlowFontTimeoutBeforeBody) {
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
  EXPECT_TRUE(HasRenderBlockingResources());

  GetRenderBlockingResourceManager().FontPreloadingTimerFired(nullptr);

  // Font preloading no longer blocks renderings after the timeout fires.
  // However, rendering is still blocked, as we don't have BODY yet.
  EXPECT_TRUE(Compositor().DeferMainFrameUpdate());
  EXPECT_FALSE(HasRenderBlockingResources());

  main_resource.Complete("</head><body>some text</body>");

  // Rendering starts after BODY has arrived.
  EXPECT_FALSE(Compositor().DeferMainFrameUpdate());
  EXPECT_FALSE(HasRenderBlockingResources());

  font_resource.Complete();
}

TEST_F(RenderBlockingResourceManagerTest, SlowFontTimeoutAfterBody) {
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
  EXPECT_TRUE(HasRenderBlockingResources());

  main_resource.Complete("</head><body>some text</body>");

  // Rendering is still blocked by font, even if we already have BODY.
  EXPECT_TRUE(Compositor().DeferMainFrameUpdate());
  EXPECT_TRUE(HasRenderBlockingResources());

  GetRenderBlockingResourceManager().FontPreloadingTimerFired(nullptr);

  // Rendering starts after we've waited for the font preloading long enough.
  EXPECT_FALSE(Compositor().DeferMainFrameUpdate());
  EXPECT_FALSE(HasRenderBlockingResources());

  font_resource.Complete();
}

// A trivial test case to verify test setup
TEST_F(RenderBlockingResourceManagerTest, RegularWebFont) {
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

  font_resource.Complete(ReadAhemWoff2());

  // Now everything is loaded. The web font should be used in rendering.
  Compositor().BeginFrame().DrawCount();
  EXPECT_EQ(250, GetTarget()->OffsetWidth());
  EXPECT_FALSE(GetTargetFont().ShouldSkipDrawing());
}

TEST_F(RenderBlockingResourceManagerTest, OptionalFontWithoutPreloading) {
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

  font_resource.Complete(ReadAhemWoff2());

  // Although the optional web font isn't preloaded, it finished loading before
  // the first time we try to render with it. Therefore it's used.
  Compositor().BeginFrame().Contains(SimCanvas::kText);
  EXPECT_EQ(250, GetTarget()->OffsetWidth());
  EXPECT_FALSE(GetTargetFont().ShouldSkipDrawing());

  main_resource.Finish();
}

TEST_F(RenderBlockingResourceManagerTest, OptionalFontMissingFirstFrame) {
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

TEST_F(RenderBlockingResourceManagerTest,
       OptionalFontForcedLayoutNoLayoutShift) {
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

TEST_F(RenderBlockingResourceManagerTest, OptionalFontRemoveAndReadd) {
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

  // The 'optional' web font isn't used, as it didn't finish loading before
  // rendering started. Text is rendered in visible fallback.
  Compositor().BeginFrame().Contains(SimCanvas::kText);
  EXPECT_GT(250, GetTarget()->OffsetWidth());
  EXPECT_FALSE(GetTargetFont().ShouldSkipDrawing());

  font_resource.Complete(ReadAhemWoff2());

  Element* style = GetDocument().QuerySelector(AtomicString("style"));
  style->remove();
  GetDocument().head()->appendChild(style);

  // After removing and readding the style sheet, we've created a new font face
  // that got loaded immediately from the memory cache. So it can be used.
  Compositor().BeginFrame().Contains(SimCanvas::kText);
  EXPECT_EQ(250, GetTarget()->OffsetWidth());
  EXPECT_FALSE(GetTargetFont().ShouldSkipDrawing());
}

TEST_F(RenderBlockingResourceManagerTest, OptionalFontSlowPreloading) {
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
  EXPECT_TRUE(HasRenderBlockingResources());

  GetRenderBlockingResourceManager().FontPreloadingTimerFired(nullptr);

  // Rendering is unblocked after the font preloading has timed out.
  EXPECT_FALSE(Compositor().DeferMainFrameUpdate());
  EXPECT_FALSE(HasRenderBlockingResources());

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

TEST_F(RenderBlockingResourceManagerTest, OptionalFontFastPreloading) {
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
  EXPECT_TRUE(HasRenderBlockingResources());

  // There are test flakes due to RenderBlockingResourceManager timeout firing
  // before the ResourceFinishObserver gets notified. So we disable the timeout.
  DisableFontPreloadTimeout();

  font_resource.Complete(ReadAhemWoff2());
  test::RunPendingTasks();

  // Rendering is unblocked after the font is preloaded.
  EXPECT_FALSE(Compositor().DeferMainFrameUpdate());
  EXPECT_FALSE(HasRenderBlockingResources());

  // The 'optional' web font should be used in the first paint.
  Compositor().BeginFrame();
  EXPECT_EQ(250, GetTarget()->OffsetWidth());
  EXPECT_FALSE(GetTargetFont().ShouldSkipDrawing());
}

TEST_F(RenderBlockingResourceManagerTest, OptionalFontSlowImperativeLoad) {
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
  EXPECT_TRUE(HasRenderBlockingResources());

  GetRenderBlockingResourceManager().FontPreloadingTimerFired(nullptr);

  // Rendering is unblocked after the font preloading has timed out.
  EXPECT_FALSE(Compositor().DeferMainFrameUpdate());
  EXPECT_FALSE(HasRenderBlockingResources());

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

TEST_F(RenderBlockingResourceManagerTest, OptionalFontFastImperativeLoad) {
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
  SetFontPreloadTimeout(base::Seconds(30));

  // Rendering is blocked due to font being preloaded.
  EXPECT_TRUE(Compositor().DeferMainFrameUpdate());
  EXPECT_TRUE(HasRenderBlockingResources());

  font_resource.Complete(ReadAhemWoff2());
  test::RunPendingTasks();

  // Rendering is unblocked after the font is preloaded.
  EXPECT_FALSE(Compositor().DeferMainFrameUpdate());
  EXPECT_FALSE(HasRenderBlockingResources());

  // The 'optional' web font should be used in the first paint.
  Compositor().BeginFrame();
  EXPECT_EQ(250, GetTarget()->OffsetWidth());
  EXPECT_FALSE(GetTargetFont().ShouldSkipDrawing());
}

TEST_F(RenderBlockingResourceManagerTest, ScriptInsertedBodyUnblocksRendering) {
  SimRequest main_resource("https://example.com", "text/html");
  SimSubresourceRequest style_resource("https://example.com/sheet.css",
                                       "text/css");

  LoadURL("https://example.com");
  main_resource.Write(R"HTML(
    <!doctype html>
    <link rel="stylesheet" href="sheet.css">
  )HTML");

  Element* body = GetDocument().CreateElementForBinding(AtomicString("body"));
  GetDocument().setBody(To<HTMLElement>(body), ASSERT_NO_EXCEPTION);

  // Rendering should be blocked by the pending stylesheet.
  EXPECT_TRUE(GetDocument().body());
  EXPECT_TRUE(Compositor().DeferMainFrameUpdate());

  style_resource.Complete("body { width: 100px; }");

  // Rendering should be unblocked as all render-blocking resources are loaded
  // and there is a body, even though it's not inserted by parser.
  EXPECT_FALSE(Compositor().DeferMainFrameUpdate());
  Compositor().BeginFrame();
  EXPECT_EQ(100, GetDocument().body()->OffsetWidth());

  main_resource.Finish();
}

// https://crbug.com/1308083
TEST_F(RenderBlockingResourceManagerTest, ParserBlockingScriptBeforeFont) {
  SimRequest main_resource("https://example.com", "text/html");
  SimSubresourceRequest font_resource("https://example.com/font.woff2",
                                      "font/woff2");
  SimSubresourceRequest script_resource("https://example.com/script.js",
                                        "application/javascript");

  LoadURL("https://example.com");

  // Make sure timer doesn't fire in case the test runs slow.
  SetFontPreloadTimeout(base::Seconds(30));

  main_resource.Complete(R"HTML(
    <!doctype html>
    <script src="script.js"></script>
    <link rel="preload" as="font" type="font/woff2"
          href="font.woff2" crossorigin>
    <div>
      Lorem ipsum
    </div>
  )HTML");

  // Rendering is still blocked.
  EXPECT_TRUE(Compositor().DeferMainFrameUpdate());

  // Parser is blocked by the synchronous script, so <link> isn't inserted yet.
  EXPECT_FALSE(GetDocument().QuerySelector(AtomicString("link")));

  // Preload scanner should have started font preloading and also the timer.
  // This should happen before the parser sets up the preload link element.
  EXPECT_TRUE(FontPreloadTimerIsActive());

  script_resource.Complete();
  font_resource.Complete();
}

class RenderBlockingFontTest : public RenderBlockingResourceManagerTest {
 public:
  void SetUp() override {
    // Use a longer timeout to prevent flakiness when test is running slow.
    std::map<std::string, std::string> parameters;
    parameters["max-fcp-delay"] = "500";
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        features::kRenderBlockingFonts, parameters);
    SimTest::SetUp();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(RenderBlockingFontTest, FastFontPreloadWithoutOtherBlockingResources) {
  SimRequest main_resource("https://example.com", "text/html");
  SimSubresourceRequest font_resource("https://example.com/font.woff2",
                                      "font/woff2");

  LoadURL("https://example.com");
  main_resource.Complete(R"HTML(
    <!doctype html>
    <link rel="preload" as="font" type="font/woff2" crossorigin
          href="https://example.com/font.woff2">
    Body Content
  )HTML");

  // Rendering is blocked by font.
  EXPECT_TRUE(Compositor().DeferMainFrameUpdate());

  font_resource.Complete(ReadAhemWoff2());
  test::RunPendingTasks();

  // Rendering is unblocked after font preload finishes.
  EXPECT_FALSE(Compositor().DeferMainFrameUpdate());
}

TEST_F(RenderBlockingFontTest, SlowFontPreloadWithoutOtherBlockingResources) {
  SimRequest main_resource("https://example.com", "text/html");
  SimSubresourceRequest font_resource("https://example.com/font.woff2",
                                      "font/woff2");

  LoadURL("https://example.com");
  main_resource.Complete(R"HTML(
    <!doctype html>
    <link rel="preload" as="font" type="font/woff2" crossorigin
          href="https://example.com/font.woff2">
    Body Content
  )HTML");

  // Rendering is blocked by font.
  EXPECT_TRUE(Compositor().DeferMainFrameUpdate());

  // Wait until we've delayed FCP for the max allowed amount of time, and the
  // relevant timeout fires.
  test::RunDelayedTasks(
      base::Milliseconds(features::kMaxFCPDelayMsForRenderBlockingFonts.Get()));

  // Rendering is unblocked as max FCP delay is reached.
  EXPECT_FALSE(Compositor().DeferMainFrameUpdate());

  font_resource.Complete(ReadAhemWoff2());
}

TEST_F(RenderBlockingFontTest,
       SlowFontPreloadAndSlowBodyWithoutOtherBlockingResources) {
  SimRequest main_resource("https://example.com", "text/html");
  SimSubresourceRequest font_resource("https://example.com/font.woff2",
                                      "font/woff2");

  LoadURL("https://example.com");
  main_resource.Write(R"HTML(
    <!doctype html>
    <link rel="preload" as="font" type="font/woff2" crossorigin
          href="https://example.com/font.woff2">
  )HTML");

  // Rendering is blocked by font.
  EXPECT_TRUE(Compositor().DeferMainFrameUpdate());

  // Wait until we've blocked rendering for the max allowed amount of time since
  // navigation, and the relevant timeout fires.
  test::RunDelayedTasks(base::Milliseconds(
      features::kMaxBlockingTimeMsForRenderBlockingFonts.Get()));

  // The font preload is no longer render-blocking, but Rendering is still
  // blocked because the document has no body.
  EXPECT_FALSE(GetRenderBlockingResourceManager().HasRenderBlockingFonts());
  EXPECT_TRUE(Compositor().DeferMainFrameUpdate());

  main_resource.Complete("Body Content");

  // Rendering is unblocked after body is inserted.
  EXPECT_FALSE(Compositor().DeferMainFrameUpdate());

  font_resource.Complete(ReadAhemWoff2());
}

TEST_F(RenderBlockingFontTest, FastFontPreloadWithOtherBlockingResources) {
  SimRequest main_resource("https://example.com", "text/html");
  SimSubresourceRequest font_resource("https://example.com/font.woff2",
                                      "font/woff2");
  SimSubresourceRequest css_resource("https://example.com/style.css",
                                     "text/css");

  LoadURL("https://example.com");
  main_resource.Complete(R"HTML(
    <!doctype html>
    <link rel="preload" as="font" type="font/woff2" crossorigin
          href="https://example.com/font.woff2">
    <link rel="stylesheet" href="https://example.com/style.css">
    Body Content
  )HTML");

  font_resource.Complete(ReadAhemWoff2());
  test::RunPendingTasks();

  // Rendering is still blocked by the style sheet.
  EXPECT_TRUE(Compositor().DeferMainFrameUpdate());

  css_resource.Complete("body { color: red; }");
  test::RunPendingTasks();

  // Rendering is unblocked after all resources are loaded.
  EXPECT_FALSE(Compositor().DeferMainFrameUpdate());
}

TEST_F(RenderBlockingFontTest, FontPreloadExceedingMaxBlockingTime) {
  SimRequest main_resource("https://example.com", "text/html");
  SimSubresourceRequest font_resource("https://example.com/font.woff2",
                                      "font/woff2");
  SimSubresourceRequest css_resource("https://example.com/style.css",
                                     "text/css");

  LoadURL("https://example.com");
  main_resource.Complete(R"HTML(
    <!doctype html>
    <link rel="preload" as="font" type="font/woff2" crossorigin
          href="https://example.com/font.woff2">
    <link rel="stylesheet" href="https://example.com/style.css">
    Body Content
  )HTML");

  // Wait until we've blocked rendering for the max allowed amount of time since
  // navigation, and the relevant timeout fires.
  test::RunDelayedTasks(base::Milliseconds(
      features::kMaxBlockingTimeMsForRenderBlockingFonts.Get()));

  // The font preload is no longer render-blocking, but we still have a
  // render-blocking style sheet.
  EXPECT_FALSE(GetRenderBlockingResourceManager().HasRenderBlockingFonts());
  EXPECT_TRUE(Compositor().DeferMainFrameUpdate());

  css_resource.Complete("body { color: red; }");
  test::RunPendingTasks();

  // Rendering is unblocked after the style sheet is loaded.
  EXPECT_FALSE(GetRenderBlockingResourceManager().HasRenderBlockingFonts());
  EXPECT_FALSE(Compositor().DeferMainFrameUpdate());

  font_resource.Complete(ReadAhemWoff2());
}

TEST_F(RenderBlockingFontTest, FontPreloadExceedingMaxFCPDelay) {
  SimRequest main_resource("https://example.com", "text/html");
  SimSubresourceRequest font_resource("https://example.com/font.woff2",
                                      "font/woff2");
  SimSubresourceRequest css_resource("https://example.com/style.css",
                                     "text/css");

  LoadURL("https://example.com");
  main_resource.Complete(R"HTML(
    <!doctype html>
    <link rel="preload" as="font" type="font/woff2" crossorigin
          href="https://example.com/font.woff2">
    <link rel="stylesheet" href="https://example.com/style.css">
    Body Content
  )HTML");

  css_resource.Complete("body { color: red; }");
  test::RunPendingTasks();

  // Now the font is the only render-blocking resource, and rendering would have
  // started without the font.
  EXPECT_TRUE(GetRenderBlockingResourceManager().HasRenderBlockingFonts());
  EXPECT_FALSE(
      GetRenderBlockingResourceManager().HasNonFontRenderBlockingResources());
  EXPECT_TRUE(Compositor().DeferMainFrameUpdate());

  test::RunDelayedTasks(
      base::Milliseconds(features::kMaxFCPDelayMsForRenderBlockingFonts.Get()));

  // After delaying FCP for the max allowed time, the font is no longer
  // render-blocking.
  EXPECT_FALSE(GetRenderBlockingResourceManager().HasRenderBlockingFonts());
  EXPECT_FALSE(Compositor().DeferMainFrameUpdate());

  font_resource.Complete(ReadAhemWoff2());
}

TEST_F(RenderBlockingFontTest, FontPreloadExceedingBothLimits) {
  SimRequest main_resource("https://example.com", "text/html");
  SimSubresourceRequest font_resource("https://example.com/font.woff2",
                                      "font/woff2");
  SimSubresourceRequest css_resource("https://example.com/style.css",
                                     "text/css");

  LoadURL("https://example.com");
  main_resource.Complete(R"HTML(
    <!doctype html>
    <link rel="preload" as="font" type="font/woff2" crossorigin
          href="https://example.com/font.woff2">
    <link rel="stylesheet" href="https://example.com/style.css">
    Body Content
  )HTML");

  css_resource.Complete("body { color: red; }");

  EXPECT_TRUE(Compositor().DeferMainFrameUpdate());

  test::RunDelayedTasks(
      base::Milliseconds(features::kMaxFCPDelayMsForRenderBlockingFonts.Get()));
  test::RunDelayedTasks(base::Milliseconds(
      features::kMaxBlockingTimeMsForRenderBlockingFonts.Get()));

  EXPECT_FALSE(Compositor().DeferMainFrameUpdate());

  font_resource.Complete(ReadAhemWoff2());
}

}  // namespace blink
