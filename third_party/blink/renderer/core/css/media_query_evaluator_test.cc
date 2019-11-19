// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/media_query_evaluator.h"

#include <memory>
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/css/forced_colors.h"
#include "third_party/blink/renderer/core/css/media_list.h"
#include "third_party/blink/renderer/core/css/media_values_cached.h"
#include "third_party/blink/renderer/core/css/media_values_initial_viewport.h"
#include "third_party/blink/renderer/core/css/parser/css_tokenizer.h"
#include "third_party/blink/renderer/core/css/parser/media_query_parser.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/media_type_names.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

typedef struct {
  const char* input;
  const bool output;
} MediaQueryEvaluatorTestCase;

MediaQueryEvaluatorTestCase g_screen_test_cases[] = {
    {"", 1},
    {" ", 1},
    {"screen", 1},
    {"screen and (color)", 1},
    {"not screen and (color)", 0},
    {"screen and (device-aspect-ratio: 16/9)", 0},
    {"screen and (device-aspect-ratio: 1/1)", 1},
    {"all and (min-color: 2)", 1},
    {"all and (min-color: 32)", 0},
    {"all and (min-color-index: 0)", 1},
    {"all and (min-color-index: 1)", 0},
    {"all and (monochrome)", 0},
    {"all and (min-monochrome: 0)", 1},
    {"all and (grid: 0)", 1},
    {"(resolution: 2dppx)", 1},
    {"(resolution: 1dppx)", 0},
    {"(orientation: portrait)", 1},
    {"(orientation: landscape)", 0},
    {"(orientation: url(portrait))", 0},
    {"(orientation: #portrait)", 0},
    {"(orientation: @portrait)", 0},
    {"(orientation: 'portrait')", 0},
    {"(orientation: @junk portrait)", 0},
    {"screen and (orientation: @portrait) and (max-width: 1000px)", 0},
    {"screen and (orientation: @portrait), (max-width: 1000px)", 1},
    {"tv and (scan: progressive)", 0},
    {"(pointer: coarse)", 0},
    {"(pointer: fine)", 1},
    {"(hover: hover)", 1},
    {"(hover: on-demand)", 0},
    {"(hover: none)", 0},
    {"(display-mode)", 1},
    {"(display-mode: fullscreen)", 0},
    {"(display-mode: standalone)", 0},
    {"(display-mode: minimal-ui)", 0},
    {"(display-mode: browser)", 1},
    {"(display-mode: min-browser)", 0},
    {"(display-mode: url(browser))", 0},
    {"(display-mode: #browser)", 0},
    {"(display-mode: @browser)", 0},
    {"(display-mode: 'browser')", 0},
    {"(display-mode: @junk browser)", 0},
    {"(shape: rect)", 1},
    {"(shape: round)", 0},
    {"(max-device-aspect-ratio: 4294967295/1)", 1},
    {"(min-device-aspect-ratio: 1/4294967296)", 1},
    {nullptr, 0}  // Do not remove the terminator line.
};

MediaQueryEvaluatorTestCase g_monochrome_test_cases[] = {
    {"(color)", 0},
    {"(monochrome)", 1},
    {nullptr, 0}  // Do not remove the terminator line.
};

MediaQueryEvaluatorTestCase g_viewport_test_cases[] = {
    {"all and (min-width: 500px)", 1},
    {"(min-width: 500px)", 1},
    {"(min-width: 501px)", 0},
    {"(max-width: 500px)", 1},
    {"(max-width: 499px)", 0},
    {"(width: 500px)", 1},
    {"(width: 501px)", 0},
    {"(min-height: 500px)", 1},
    {"(min-height: 501px)", 0},
    {"(min-height: 500.02px)", 0},
    {"(max-height: 500px)", 1},
    {"(max-height: calc(500px))", 1},
    {"(max-height: 499.98px)", 0},
    {"(max-height: 499px)", 0},
    {"(height: 500px)", 1},
    {"(height: calc(500px))", 1},
    {"(height: 500.001px)", 1},
    {"(height: 499.999px)", 1},
    {"(height: 500.02px)", 0},
    {"(height: 499.98px)", 0},
    {"(height: 501px)", 0},
    {"(height)", 1},
    {"(width)", 1},
    {"(width: whatisthis)", 0},
    {"screen and (min-width: 400px) and (max-width: 700px)", 1},
    {"(max-aspect-ratio: 4294967296/1)", 1},
    {"(max-aspect-ratio: calc(4294967296) / calc(1)", 1},
    {"(min-aspect-ratio: 1/4294967295)", 1},
    {nullptr, 0}  // Do not remove the terminator line.
};

MediaQueryEvaluatorTestCase g_float_viewport_test_cases[] = {
    {"all and (min-width: 600.5px)", 1},
    {"(min-width: 600px)", 1},
    {"(min-width: 600.5px)", 1},
    {"(min-width: 601px)", 0},
    {"(max-width: 600px)", 0},
    {"(max-width: 600.5px)", 1},
    {"(max-width: 601px)", 1},
    {"(width: 600.5px)", 1},
    {"(width: 601px)", 0},
    {"(min-height: 700px)", 1},
    {"(min-height: 700.125px)", 1},
    {"(min-height: 701px)", 0},
    {"(min-height: 700.141px)", 0},
    {"(max-height: 701px)", 1},
    {"(max-height: 700.125px)", 1},
    {"(max-height: 700px)", 0},
    {"(height: 700.125px)", 1},
    {"(height: 700.141px)", 0},
    {"(height: 700.109px)", 0},
    {"(height: 701px)", 0},
    {nullptr, 0}  // Do not remove the terminator line.
};

MediaQueryEvaluatorTestCase g_float_non_friendly_viewport_test_cases[] = {
    {"(min-width: 821px)", 1},
    {"(max-width: 821px)", 1},
    {"(width: 821px)", 1},
    {"(min-height: 821px)", 1},
    {"(max-height: 821px)", 1},
    {"(height: 821px)", 1},
    {"(width: 100vw)", 1},
    {"(height: 100vh)", 1},
    {nullptr, 0}  // Do not remove the terminator line.
};

MediaQueryEvaluatorTestCase g_print_test_cases[] = {
    {"print and (min-resolution: 1dppx)", 1},
    {"print and (min-resolution: 118dpcm)", 1},
    {"print and (min-resolution: 119dpcm)", 0},
    {nullptr, 0}  // Do not remove the terminator line.
};

MediaQueryEvaluatorTestCase g_non_immersive_test_cases[] = {
    {"(immersive: 1)", 0},
    {"(immersive: 0)", 1},
    {nullptr, 0}  // Do not remove the terminator line.
};

MediaQueryEvaluatorTestCase g_immersive_test_cases[] = {
    {"(immersive: 1)", 1},
    {"(immersive: 0)", 0},
    {nullptr, 0}  // Do not remove the terminator line.
};

MediaQueryEvaluatorTestCase g_non_ua_sheet_immersive_test_cases[] = {
    {"(immersive: 1)", 0},
    {"(immersive: 0)", 0},
    {nullptr, 0}  // Do not remove the terminator line.
};

MediaQueryEvaluatorTestCase g_forcedcolors_active_cases[] = {
    {"(forced-colors: active)", 1},
    {"(forced-colors: none)", 0},
    {nullptr, 0}  // Do not remove the terminator line.
};

MediaQueryEvaluatorTestCase g_forcedcolors_none_cases[] = {
    {"(forced-colors: active)", 0},
    {"(forced-colors: none)", 1},
    {nullptr, 0}  // Do not remove the terminator line.
};

MediaQueryEvaluatorTestCase g_navigationcontrols_back_button_cases[] = {
    {"(navigation-controls: back-button)", 1},
    {"(navigation-controls: none)", 0},
    {nullptr, 0}  // Do not remove the terminator line.
};

MediaQueryEvaluatorTestCase g_navigationcontrols_none_cases[] = {
    {"(navigation-controls: back-button)", 0},
    {"(navigation-controls: none)", 1},
    {nullptr, 0}  // Do not remove the terminator line.
};

void TestMQEvaluator(MediaQueryEvaluatorTestCase* test_cases,
                     const MediaQueryEvaluator& media_query_evaluator,
                     CSSParserMode mode) {
  scoped_refptr<MediaQuerySet> query_set = nullptr;
  for (unsigned i = 0; test_cases[i].input; ++i) {
    if (String(test_cases[i].input).IsEmpty()) {
      query_set = MediaQuerySet::Create();
    } else {
      query_set = MediaQueryParser::ParseMediaQuerySetInMode(
          CSSParserTokenRange(
              CSSTokenizer(test_cases[i].input).TokenizeToEOF()),
          mode);
    }
    EXPECT_EQ(test_cases[i].output, media_query_evaluator.Eval(*query_set))
        << "Query: " << test_cases[i].input;
  }
}

void TestMQEvaluator(MediaQueryEvaluatorTestCase* test_cases,
                     const MediaQueryEvaluator& media_query_evaluator) {
  TestMQEvaluator(test_cases, media_query_evaluator, kHTMLStandardMode);
}

TEST(MediaQueryEvaluatorTest, Cached) {
  MediaValuesCached::MediaValuesCachedData data;
  data.viewport_width = 500;
  data.viewport_height = 500;
  data.device_width = 500;
  data.device_height = 500;
  data.device_pixel_ratio = 2.0;
  data.color_bits_per_component = 24;
  data.monochrome_bits_per_component = 0;
  data.primary_pointer_type = kPointerTypeFine;
  data.primary_hover_type = kHoverTypeHover;
  data.default_font_size = 16;
  data.three_d_enabled = true;
  data.media_type = media_type_names::kScreen;
  data.strict_mode = true;
  data.display_mode = blink::mojom::DisplayMode::kBrowser;
  data.display_shape = kDisplayShapeRect;
  data.immersive_mode = false;

  // Default values.
  {
    auto* media_values = MakeGarbageCollected<MediaValuesCached>(data);
    MediaQueryEvaluator media_query_evaluator(*media_values);
    TestMQEvaluator(g_screen_test_cases, media_query_evaluator);
    TestMQEvaluator(g_viewport_test_cases, media_query_evaluator);
    TestMQEvaluator(g_non_immersive_test_cases, media_query_evaluator,
                    kUASheetMode);
    TestMQEvaluator(g_non_ua_sheet_immersive_test_cases, media_query_evaluator);
  }

  // Print values.
  {
    data.media_type = media_type_names::kPrint;
    auto* media_values = MakeGarbageCollected<MediaValuesCached>(data);
    MediaQueryEvaluator media_query_evaluator(*media_values);
    TestMQEvaluator(g_print_test_cases, media_query_evaluator);
    data.media_type = media_type_names::kScreen;
  }

  // Monochrome values.
  {
    data.color_bits_per_component = 0;
    data.monochrome_bits_per_component = 8;
    auto* media_values = MakeGarbageCollected<MediaValuesCached>(data);
    MediaQueryEvaluator media_query_evaluator(*media_values);
    TestMQEvaluator(g_monochrome_test_cases, media_query_evaluator);
    data.color_bits_per_component = 24;
    data.monochrome_bits_per_component = 0;
  }

  // Immersive values.
  {
    data.immersive_mode = true;
    auto* media_values = MakeGarbageCollected<MediaValuesCached>(data);
    MediaQueryEvaluator media_query_evaluator(*media_values);
    TestMQEvaluator(g_immersive_test_cases, media_query_evaluator,
                    kUASheetMode);
    TestMQEvaluator(g_non_ua_sheet_immersive_test_cases, media_query_evaluator);
    data.immersive_mode = false;
  }
}

TEST(MediaQueryEvaluatorTest, Dynamic) {
  auto page_holder = std::make_unique<DummyPageHolder>(IntSize(500, 500));
  page_holder->GetFrameView().SetMediaType(media_type_names::kScreen);

  MediaQueryEvaluator media_query_evaluator(&page_holder->GetFrame());
  TestMQEvaluator(g_viewport_test_cases, media_query_evaluator);
  page_holder->GetFrameView().SetMediaType(media_type_names::kPrint);
  TestMQEvaluator(g_print_test_cases, media_query_evaluator);
}

TEST(MediaQueryEvaluatorTest, DynamicNoView) {
  auto page_holder = std::make_unique<DummyPageHolder>(IntSize(500, 500));
  LocalFrame* frame = &page_holder->GetFrame();
  page_holder.reset();
  ASSERT_EQ(nullptr, frame->View());
  MediaQueryEvaluator media_query_evaluator(frame);
  scoped_refptr<MediaQuerySet> query_set = MediaQuerySet::Create("foobar");
  EXPECT_FALSE(media_query_evaluator.Eval(*query_set));
}

TEST(MediaQueryEvaluatorTest, CachedFloatViewport) {
  MediaValuesCached::MediaValuesCachedData data;
  data.viewport_width = 600.5;
  data.viewport_height = 700.125;
  auto* media_values = MakeGarbageCollected<MediaValuesCached>(data);

  MediaQueryEvaluator media_query_evaluator(*media_values);
  TestMQEvaluator(g_float_viewport_test_cases, media_query_evaluator);
}

TEST(MediaQueryEvaluatorTest, CachedFloatViewportNonFloatFriendly) {
  MediaValuesCached::MediaValuesCachedData data;
  data.viewport_width = 821;
  data.viewport_height = 821;
  auto* media_values = MakeGarbageCollected<MediaValuesCached>(data);

  MediaQueryEvaluator media_query_evaluator(*media_values);
  TestMQEvaluator(g_float_non_friendly_viewport_test_cases,
                  media_query_evaluator);
}

TEST(MediaQueryEvaluatorTest, InitialViewport) {
  auto page_holder = std::make_unique<DummyPageHolder>(IntSize(500, 500));
  page_holder->GetFrameView().SetMediaType(media_type_names::kScreen);
  page_holder->GetFrameView().SetLayoutSizeFixedToFrameSize(false);
  page_holder->GetFrameView().SetInitialViewportSize(IntSize(500, 500));
  page_holder->GetFrameView().SetLayoutSize(IntSize(800, 800));
  page_holder->GetFrameView().SetFrameRect(IntRect(0, 0, 800, 800));

  MediaQueryEvaluator media_query_evaluator(
      MakeGarbageCollected<MediaValuesInitialViewport>(
          page_holder->GetFrame()));
  TestMQEvaluator(g_viewport_test_cases, media_query_evaluator);
}

TEST(MediaQueryEvaluatorTest, DynamicImmersive) {
  auto page_holder = std::make_unique<DummyPageHolder>(IntSize(500, 500));
  page_holder->GetFrameView().SetMediaType(media_type_names::kScreen);

  MediaQueryEvaluator media_query_evaluator(&page_holder->GetFrame());
  page_holder->GetDocument().GetSettings()->SetImmersiveModeEnabled(false);

  TestMQEvaluator(g_non_immersive_test_cases, media_query_evaluator,
                  kUASheetMode);
  page_holder->GetDocument().GetSettings()->SetImmersiveModeEnabled(true);
  TestMQEvaluator(g_immersive_test_cases, media_query_evaluator, kUASheetMode);
}

TEST(MediaQueryEvaluatorTest, CachedForcedColors) {
  ScopedForcedColorsForTest scoped_feature(true);

  MediaValuesCached::MediaValuesCachedData data;
  data.forced_colors = ForcedColors::kNone;
  MediaValues* media_values = MakeGarbageCollected<MediaValuesCached>(data);

  // Forced colors - none.
  MediaQueryEvaluator media_query_evaluator(*media_values);
  TestMQEvaluator(g_forcedcolors_none_cases, media_query_evaluator);

  // Forced colors - active.
  {
    data.forced_colors = ForcedColors::kActive;
    MediaValues* media_values = MakeGarbageCollected<MediaValuesCached>(data);
    MediaQueryEvaluator media_query_evaluator(*media_values);
    TestMQEvaluator(g_forcedcolors_active_cases, media_query_evaluator);
  }
}

}  // namespace blink
