// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/media_query_evaluator.h"

#include <memory>
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/css/forced_colors.h"
#include "third_party/blink/renderer/core/css/media_list.h"
#include "third_party/blink/renderer/core/css/media_values.h"
#include "third_party/blink/renderer/core/css/media_values_cached.h"
#include "third_party/blink/renderer/core/css/parser/css_tokenizer.h"
#include "third_party/blink/renderer/core/css/parser/media_query_parser.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/media_type_names.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

struct MediaQueryEvaluatorTestCase {
  const char* input;
  const bool output;
};

MediaQueryEvaluatorTestCase g_screen_test_cases[] = {
    {"", true},
    {" ", true},
    {"screen", true},
    {"screen and (color)", true},
    {"not screen and (color)", false},
    {"screen and (device-aspect-ratio: 16/9)", false},
    {"screen and (device-aspect-ratio: 1/1)", true},
    {"all and (min-color: 2)", true},
    {"all and (min-color: 32)", false},
    {"all and (min-color-index: 0)", true},
    {"all and (min-color-index: 1)", false},
    {"all and (monochrome)", false},
    {"all and (min-monochrome: 0)", true},
    {"all and (grid: 0)", true},
    {"(resolution: 2dppx)", true},
    {"(resolution: 1dppx)", false},
    {"(orientation: portrait)", true},
    {"(orientation: landscape)", false},
    {"(orientation: url(portrait))", false},
    {"(orientation: #portrait)", false},
    {"(orientation: @portrait)", false},
    {"(orientation: 'portrait')", false},
    {"(orientation: @junk portrait)", false},
    {"screen and (orientation: @portrait) and (max-width: 1000px)", false},
    {"screen and (orientation: @portrait), (max-width: 1000px)", true},
    {"tv and (scan: progressive)", false},
    {"(pointer: coarse)", false},
    {"(pointer: fine)", true},
    {"(hover: hover)", true},
    {"(hover: on-demand)", false},
    {"(hover: none)", false},
    {"(display-mode)", true},
    {"(display-mode: fullscreen)", false},
    {"(display-mode: standalone)", false},
    {"(display-mode: minimal-ui)", false},
    {"(display-mode: browser)", true},
    {"(display-mode: min-browser)", false},
    {"(display-mode: url(browser))", false},
    {"(display-mode: #browser)", false},
    {"(display-mode: @browser)", false},
    {"(display-mode: 'browser')", false},
    {"(display-mode: @junk browser)", false},
    {"(max-device-aspect-ratio: 4294967295/1)", true},
    {"(min-device-aspect-ratio: 1/4294967296)", true},
    {nullptr, false}  // Do not remove the terminator line.
};

MediaQueryEvaluatorTestCase g_monochrome_test_cases[] = {
    {"(color)", false},
    {"(monochrome)", true},
    {nullptr, false}  // Do not remove the terminator line.
};

MediaQueryEvaluatorTestCase g_viewport_test_cases[] = {
    {"all and (min-width: 500px)", true},
    {"(min-width: 500px)", true},
    {"(min-width: 501px)", false},
    {"(max-width: 500px)", true},
    {"(max-width: 499px)", false},
    {"(width: 500px)", true},
    {"(width: 501px)", false},
    {"(min-height: 500px)", true},
    {"(min-height: 501px)", false},
    {"(min-height: 500.02px)", false},
    {"(max-height: 500px)", true},
    {"(max-height: calc(500px))", true},
    {"(max-height: 499.98px)", false},
    {"(max-height: 499px)", false},
    {"(height: 500px)", true},
    {"(height: calc(500px))", true},
    {"(height: 500.001px)", true},
    {"(height: 499.999px)", true},
    {"(height: 500.02px)", false},
    {"(height: 499.98px)", false},
    {"(height: 501px)", false},
    {"(height)", true},
    {"(width)", true},
    {"(width: whatisthis)", false},
    {"screen and (min-width: 400px) and (max-width: 700px)", true},
    {"(max-aspect-ratio: 4294967296/1)", true},
    {"(max-aspect-ratio: calc(4294967296) / calc(1)", true},
    {"(min-aspect-ratio: 1/4294967295)", true},
    {nullptr, false}  // Do not remove the terminator line.
};

MediaQueryEvaluatorTestCase g_float_viewport_test_cases[] = {
    {"all and (min-width: 600.5px)", true},
    {"(min-width: 600px)", true},
    {"(min-width: 600.5px)", true},
    {"(min-width: 601px)", false},
    {"(max-width: 600px)", false},
    {"(max-width: 600.5px)", true},
    {"(max-width: 601px)", true},
    {"(width: 600.5px)", true},
    {"(width: 601px)", false},
    {"(min-height: 700px)", true},
    {"(min-height: 700.125px)", true},
    {"(min-height: 701px)", false},
    {"(min-height: 700.141px)", false},
    {"(max-height: 701px)", true},
    {"(max-height: 700.125px)", true},
    {"(max-height: 700px)", false},
    {"(height: 700.125px)", true},
    {"(height: 700.141px)", false},
    {"(height: 700.109px)", false},
    {"(height: 701px)", false},
    {nullptr, false}  // Do not remove the terminator line.
};

MediaQueryEvaluatorTestCase g_float_non_friendly_viewport_test_cases[] = {
    {"(min-width: 821px)", true},
    {"(max-width: 821px)", true},
    {"(width: 821px)", true},
    {"(min-height: 821px)", true},
    {"(max-height: 821px)", true},
    {"(height: 821px)", true},
    {"(width: 100vw)", true},
    {"(height: 100vh)", true},
    {nullptr, false}  // Do not remove the terminator line.
};

MediaQueryEvaluatorTestCase g_print_test_cases[] = {
    {"print and (min-resolution: 1dppx)", true},
    {"print and (min-resolution: 118dpcm)", true},
    {"print and (min-resolution: 119dpcm)", false},
    {nullptr, false}  // Do not remove the terminator line.
};

MediaQueryEvaluatorTestCase g_non_immersive_test_cases[] = {
    {"(immersive: 1)", false},
    {"(immersive: 0)", true},
    {nullptr, false}  // Do not remove the terminator line.
};

MediaQueryEvaluatorTestCase g_immersive_test_cases[] = {
    {"(immersive: 1)", true},
    {"(immersive: 0)", false},
    {nullptr, false}  // Do not remove the terminator line.
};

MediaQueryEvaluatorTestCase g_non_ua_sheet_immersive_test_cases[] = {
    {"(immersive: 1)", false},
    {"(immersive: 0)", false},
    {nullptr, false}  // Do not remove the terminator line.
};

MediaQueryEvaluatorTestCase g_forcedcolors_active_cases[] = {
    {"(forced-colors: active)", true},
    {"(forced-colors: none)", false},
    {nullptr, false}  // Do not remove the terminator line.
};

MediaQueryEvaluatorTestCase g_forcedcolors_none_cases[] = {
    {"(forced-colors: active)", false},
    {"(forced-colors: none)", true},
    {nullptr, false}  // Do not remove the terminator line.
};

MediaQueryEvaluatorTestCase g_preferscontrast_nopreference_cases[] = {
    {"(prefers-contrast)", false},
    {"(prefers-contrast: more)", false},
    {"(prefers-contrast: less)", false},
    {"(prefers-contrast: forced)", false},
    {"(prefers-contrast: no-preference)", true},
    {nullptr, false}  // Do not remove the terminator line.
};

MediaQueryEvaluatorTestCase g_preferscontrast_more_cases[] = {
    {"(prefers-contrast)", true},
    {"(prefers-contrast: more)", true},
    {"(prefers-contrast: less)", false},
    {"(prefers-contrast: forced)", false},
    {"(prefers-contrast: no-preference)", false},
    {nullptr, false}  // Do not remove the terminator line.
};

MediaQueryEvaluatorTestCase g_preferscontrast_less_cases[] = {
    {"(prefers-contrast)", true},
    {"(prefers-contrast: more)", false},
    {"(prefers-contrast: less)", true},
    {"(prefers-contrast: forced)", false},
    {"(prefers-contrast: no-preference)", false},
    {nullptr, false}  // Do not remove the terminator line.
};

MediaQueryEvaluatorTestCase g_preferscontrast_forced_cases[] = {
    {"(prefers-contrast)", true},
    {"(prefers-contrast: more)", false},
    {"(prefers-contrast: less)", false},
    {"(prefers-contrast: forced)", true},
    {"(prefers-contrast: no-preference)", false},
    {nullptr, false}  // Do not remove the terminator line.
};

MediaQueryEvaluatorTestCase g_preferscontrast_forced_more_cases[] = {
    {"(prefers-contrast)", true},
    {"(prefers-contrast: more)", true},
    {"(prefers-contrast: less)", false},
    {"(prefers-contrast: forced)", true},
    {"(prefers-contrast: no-preference)", false},
    {nullptr, false}  // Do not remove the terminator line.
};

MediaQueryEvaluatorTestCase g_preferscontrast_forced_less_cases[] = {
    {"(prefers-contrast)", true},
    {"(prefers-contrast: more)", false},
    {"(prefers-contrast: less)", true},
    {"(prefers-contrast: forced)", true},
    {"(prefers-contrast: no-preference)", false},
    {nullptr, false}  // Do not remove the terminator line.
};

MediaQueryEvaluatorTestCase g_navigationcontrols_back_button_cases[] = {
    {"(navigation-controls: back-button)", true},
    {"(navigation-controls: none)", false},
    {nullptr, false}  // Do not remove the terminator line.
};

MediaQueryEvaluatorTestCase g_navigationcontrols_none_cases[] = {
    {"(navigation-controls: back-button)", false},
    {"(navigation-controls: none)", true},
    {nullptr, false}  // Do not remove the terminator line.
};

MediaQueryEvaluatorTestCase g_screen_spanning_none_cases[] = {
    {"(screen-spanning)", false},
    {"(screen-spanning: single-fold-vertical)", false},
    {"(screen-spanning: single-fold-horizontal)", false},
    {"(screen-spanning: none)", true},
    {"(screen-spanning: 1px)", false},
    {"(screen-spanning: 16/9)", false},
    {nullptr, false}  // Do not remove the terminator line.
};

MediaQueryEvaluatorTestCase g_screen_spanning_single_fold_vertical_cases[] = {
    {"(screen-spanning)", true},
    {"(screen-spanning: single-fold-vertical)", true},
    {"(screen-spanning: single-fold-horizontal)", false},
    {"(screen-spanning: none)", false},
    {nullptr, false}  // Do not remove the terminator line.
};

MediaQueryEvaluatorTestCase g_screen_spanning_single_fold_horizontal_cases[] = {
    {"(screen-spanning)", true},
    {"(screen-spanning: single-fold-vertical)", false},
    {"(screen-spanning: single-fold-horizontal)", true},
    {"(screen-spanning: none)", false},
    {nullptr, false}  // Do not remove the terminator line.
};

MediaQueryEvaluatorTestCase g_device_posture_none_cases[] = {
    {"(device-posture)", true},
    {"(device-posture: laptop)", false},
    {"(device-posture: flat)", false},
    {"(device-posture: tent)", false},
    {"(device-posture: tablet)", false},
    {"(device-posture: book)", false},
    {"(device-posture: no-fold)", true},
    {"(device-posture: 15)", false},
    {"(device-posture: 2px)", false},
    {"(device-posture: 16/9)", false},
    {nullptr, false}  // Do not remove the terminator line.
};

MediaQueryEvaluatorTestCase g_device_posture_laptop_cases[] = {
    {"(device-posture)", true},
    {"(device-posture: laptop)", true},
    {"(device-posture: flat)", false},
    {"(device-posture: tent)", false},
    {"(device-posture: tablet)", false},
    {"(device-posture: book)", false},
    {"(device-posture: no-fold)", false},
    {nullptr, false}  // Do not remove the terminator line.
};

MediaQueryEvaluatorTestCase g_device_posture_flat_cases[] = {
    {"(device-posture)", true},
    {"(device-posture: laptop)", false},
    {"(device-posture: flat)", true},
    {"(device-posture: tent)", false},
    {"(device-posture: tablet)", false},
    {"(device-posture: book)", false},
    {"(device-posture: no-fold)", false},
    {nullptr, false}  // Do not remove the terminator line.
};

MediaQueryEvaluatorTestCase g_device_posture_tent_cases[] = {
    {"(device-posture)", true},
    {"(device-posture: laptop)", false},
    {"(device-posture: flat)", false},
    {"(device-posture: tent)", true},
    {"(device-posture: tablet)", false},
    {"(device-posture: book)", false},
    {"(device-posture: no-fold)", false},
    {nullptr, false}  // Do not remove the terminator line.
};

MediaQueryEvaluatorTestCase g_device_posture_tablet_cases[] = {
    {"(device-posture)", true},
    {"(device-posture: laptop)", false},
    {"(device-posture: flat)", false},
    {"(device-posture: tent)", false},
    {"(device-posture: tablet)", true},
    {"(device-posture: book)", false},
    {"(device-posture: no-fold)", false},
    {nullptr, false}  // Do not remove the terminator line.
};

MediaQueryEvaluatorTestCase g_device_posture_book_cases[] = {
    {"(device-posture)", true},
    {"(device-posture: laptop)", false},
    {"(device-posture: flat)", false},
    {"(device-posture: tent)", false},
    {"(device-posture: tablet)", false},
    {"(device-posture: book)", true},
    {"(device-posture: no-fold)", false},
    {nullptr, false}  // Do not remove the terminator line.
};

void TestMQEvaluator(MediaQueryEvaluatorTestCase* test_cases,
                     const MediaQueryEvaluator& media_query_evaluator,
                     CSSParserMode mode) {
  scoped_refptr<MediaQuerySet> query_set;
  for (unsigned i = 0; test_cases[i].input; ++i) {
    if (String(test_cases[i].input).IsEmpty()) {
      query_set = MediaQuerySet::Create();
    } else {
      query_set = MediaQueryParser::ParseMediaQuerySetInMode(
          CSSParserTokenRange(
              CSSTokenizer(test_cases[i].input).TokenizeToEOF()),
          mode, nullptr);
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
  data.primary_pointer_type = mojom::blink::PointerType::kPointerFineType;
  data.primary_hover_type = mojom::blink::HoverType::kHoverHoverType;
  data.default_font_size = 16;
  data.three_d_enabled = true;
  data.media_type = media_type_names::kScreen;
  data.strict_mode = true;
  data.display_mode = blink::mojom::DisplayMode::kBrowser;
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
  scoped_refptr<MediaQuerySet> query_set =
      MediaQuerySet::Create("foobar", nullptr);
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

TEST(MediaQueryEvaluatorTest, CachedPrefersContrast) {
  ScopedForcedColorsForTest forced_scoped_feature(true);
  ScopedPrefersContrastForTest contrast_scoped_feature(true);

  MediaValuesCached::MediaValuesCachedData data;
  data.forced_colors = ForcedColors::kNone;
  data.preferred_contrast = mojom::blink::PreferredContrast::kNoPreference;
  MediaValues* media_values = MakeGarbageCollected<MediaValuesCached>(data);

  // Prefers-contrast - no-preference.
  MediaQueryEvaluator media_query_evaluator(*media_values);
  TestMQEvaluator(g_preferscontrast_nopreference_cases, media_query_evaluator);

  // Prefers-contrast - more.
  {
    data.preferred_contrast = mojom::blink::PreferredContrast::kMore;
    MediaValues* media_values = MakeGarbageCollected<MediaValuesCached>(data);
    MediaQueryEvaluator media_query_evaluator(*media_values);
    TestMQEvaluator(g_preferscontrast_more_cases, media_query_evaluator);
  }

  // Prefers-contrast - less.
  {
    data.preferred_contrast = mojom::blink::PreferredContrast::kLess;
    MediaValues* media_values = MakeGarbageCollected<MediaValuesCached>(data);
    MediaQueryEvaluator media_query_evaluator(*media_values);
    TestMQEvaluator(g_preferscontrast_less_cases, media_query_evaluator);
  }

  // Prefers-contrast - forced.
  {
    data.preferred_contrast = mojom::blink::PreferredContrast::kNoPreference;
    data.forced_colors = ForcedColors::kActive;
    MediaValues* media_values = MakeGarbageCollected<MediaValuesCached>(data);
    MediaQueryEvaluator media_query_evaluator(*media_values);
    TestMQEvaluator(g_preferscontrast_forced_cases, media_query_evaluator);
  }

  // Prefers-contrast - forced and more.
  {
    data.preferred_contrast = mojom::blink::PreferredContrast::kMore;
    MediaValues* media_values = MakeGarbageCollected<MediaValuesCached>(data);
    MediaQueryEvaluator media_query_evaluator(*media_values);
    TestMQEvaluator(g_preferscontrast_forced_more_cases, media_query_evaluator);
  }

  // Prefers-contrast - forced and less.
  {
    data.preferred_contrast = mojom::blink::PreferredContrast::kLess;
    MediaValues* media_values = MakeGarbageCollected<MediaValuesCached>(data);
    MediaQueryEvaluator media_query_evaluator(*media_values);
    TestMQEvaluator(g_preferscontrast_forced_less_cases, media_query_evaluator);
  }
}

TEST(MediaQueryEvaluatorTest, CachedScreenSpanning) {
  ScopedCSSFoldablesForTest scoped_feature(true);

  MediaValuesCached::MediaValuesCachedData data;
  {
    data.screen_spanning = ScreenSpanning::kNone;
    MediaValues* media_values = MakeGarbageCollected<MediaValuesCached>(data);

    MediaQueryEvaluator media_query_evaluator(*media_values);
    TestMQEvaluator(g_screen_spanning_none_cases, media_query_evaluator);
  }

  {
    data.screen_spanning = ScreenSpanning::kSingleFoldVertical;
    MediaValues* media_values = MakeGarbageCollected<MediaValuesCached>(data);
    MediaQueryEvaluator media_query_evaluator(*media_values);
    TestMQEvaluator(g_screen_spanning_single_fold_vertical_cases,
                    media_query_evaluator);
  }

  {
    data.screen_spanning = ScreenSpanning::kSingleFoldHorizontal;
    MediaValues* media_values = MakeGarbageCollected<MediaValuesCached>(data);
    MediaQueryEvaluator media_query_evaluator(*media_values);
    TestMQEvaluator(g_screen_spanning_single_fold_horizontal_cases,
                    media_query_evaluator);
  }
}

TEST(MediaQueryEvaluatorTest, CachedDevicePosture) {
  ScopedDevicePostureForTest scoped_feature(true);

  MediaValuesCached::MediaValuesCachedData data;
  {
    data.device_posture = DevicePosture::kNoFold;
    MediaValues* media_values = MakeGarbageCollected<MediaValuesCached>(data);

    MediaQueryEvaluator media_query_evaluator(*media_values);
    TestMQEvaluator(g_device_posture_none_cases, media_query_evaluator);
  }

  {
    data.device_posture = DevicePosture::kLaptop;
    MediaValues* media_values = MakeGarbageCollected<MediaValuesCached>(data);
    MediaQueryEvaluator media_query_evaluator(*media_values);
    TestMQEvaluator(g_device_posture_laptop_cases, media_query_evaluator);
  }

  {
    data.device_posture = DevicePosture::kFlat;
    MediaValues* media_values = MakeGarbageCollected<MediaValuesCached>(data);
    MediaQueryEvaluator media_query_evaluator(*media_values);
    TestMQEvaluator(g_device_posture_flat_cases, media_query_evaluator);
  }

  {
    data.device_posture = DevicePosture::kTent;
    MediaValues* media_values = MakeGarbageCollected<MediaValuesCached>(data);
    MediaQueryEvaluator media_query_evaluator(*media_values);
    TestMQEvaluator(g_device_posture_tent_cases, media_query_evaluator);
  }

  {
    data.device_posture = DevicePosture::kTablet;
    MediaValues* media_values = MakeGarbageCollected<MediaValuesCached>(data);
    MediaQueryEvaluator media_query_evaluator(*media_values);
    TestMQEvaluator(g_device_posture_tablet_cases, media_query_evaluator);
  }

  {
    data.device_posture = DevicePosture::kBook;
    MediaValues* media_values = MakeGarbageCollected<MediaValuesCached>(data);
    MediaQueryEvaluator media_query_evaluator(*media_values);
    TestMQEvaluator(g_device_posture_book_cases, media_query_evaluator);
  }
}

}  // namespace blink
