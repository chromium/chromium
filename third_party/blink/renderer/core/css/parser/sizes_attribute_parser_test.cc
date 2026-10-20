// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/parser/sizes_attribute_parser.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/webpreferences/web_preferences.mojom-blink.h"
#include "third_party/blink/renderer/core/css/media_values_cached.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/media_type_names.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

namespace blink {

namespace {
MediaValues* GetTestMediaValues(const double viewport_width = 500) {
  MediaValuesCached::MediaValuesCachedData data;
  data.viewport_width = viewport_width;
  data.viewport_height = 600;
  data.device_width = 500;
  data.device_height = 500;
  data.device_pixel_ratio = 2.0;
  data.color_bits_per_component = 24;
  data.monochrome_bits_per_component = 0;
  data.primary_pointer_type = mojom::blink::PointerType::kPointerFineType;
  data.three_d_enabled = true;
  data.media_type = media_type_names::kScreen;
  data.strict_mode = true;
  data.display_mode = blink::mojom::DisplayMode::kBrowser;

  return MakeGarbageCollected<MediaValuesCached>(data);
}
}  // namespace

typedef struct {
  const char* input;
  const float effective_size;
} SizesAttributeParserTestCase;

class SizesAttributeParserTest : public PageTestBase {};

TEST_F(SizesAttributeParserTest, Basic) {
  SizesAttributeParserTestCase test_cases[] = {
      {"screen", 500},
      {"(min-width:500px)", 500},
      {"(min-width:500px) 200px", 200},
      {"(min-width:500px) 50vw", 250},
      {"(min-width:500px) 200px, 400px", 200},
      {"400px, (min-width:500px) 200px", 400},
      {"40vw, (min-width:500px) 201px", 200},
      {"(min-width:500px) 201px, 40vw", 201},
      {"(min-width:5000px) 40vw, 201px", 201},
      {"(min-width:500px) calc(201px), calc(40vw)", 201},
      {"(min-width:5000px) calc(40vw), calc(201px)", 201},
      {"(min-width:5000px) 200px, 400px", 400},
      {"(blalbadfsdf) 200px, 400px", 400},
      {"0", 0},
      {"-0", 0},
      {"1", 500},
      {"300px, 400px", 300},
      {"(min-width:5000px) 200px, (min-width:500px) 400px", 400},
      {"", 500},
      {"  ", 500},
      {" /**/ ", 500},
      {" /**/ 300px", 300},
      {"300px /**/ ", 300},
      {" /**/ (min-width:500px) /**/ 300px", 300},
      {"-100px, 200px", 200},
      {"-50vw, 20vw", 100},
      {"50asdf, 200px", 200},
      {"asdf, 200px", 200},
      {"(max-width: 3000px) 200w, 400w", 500},
      {",, , /**/ ,200px", 200},
      {"50vw", 250},
      {"50vh", 300},
      {"50vmin", 250},
      {"50vmax", 300},
      {"5em", 80},
      {"5rem", 80},
      {"calc(40vw*2)", 400},
      {"(min-width:5000px) calc(5000px/10), (min-width:500px) calc(1200px/3)",
       400},
      {"(min-width:500px) calc(1200/3)", 500},
      {"(min-width:500px) calc(1200px/(0px*14))", 500},
      {"(max-width: 3000px) 200px, 400px", 200},
      {"(max-width: 3000px) 20em, 40em", 320},
      {"(max-width: 3000px) 0, 40em", 0},
      {"(max-width: 3000px) 0px, 40em", 0},
      {"(max-width: 3000px) 50vw, 40em", 250},
      {"(max-width: 3000px) 50px, 40vw", 50},
      {"((),1px", 500},
      {"{{},1px", 500},
      {"[[],1px", 500},
      {"x(x(),1px", 500},
      {"(max-width: 3000px) 50.5px, 40vw", 50.5},
      {"not (blabla) 50px, 40vw", 200},
      {"not (max-width: 100px) 50px, 40vw", 50},
  };

  MediaValues* media_values = GetTestMediaValues();

  for (const SizesAttributeParserTestCase& test_case : test_cases) {
    SizesAttributeParser parser(media_values, test_case.input, nullptr);
    EXPECT_EQ(test_case.effective_size, parser.Size().width) << test_case.input;
  }
}

TEST_F(SizesAttributeParserTest, FloatViewportWidth) {
  SizesAttributeParserTestCase test_cases[] = {
      {"screen", 500.5},
      {"(min-width:500px)", 500.5},
      {"(min-width:500px) 200px", 200},
      {"(min-width:500px) 50vw", 250.25},
      {"(min-width:500px) 200px, 400px", 200},
      {"400px, (min-width:500px) 200px", 400},
      {"40vw, (min-width:500px) 201px", 200.2},
      {"(min-width:500px) 201px, 40vw", 201},
      {"(min-width:5000px) 40vw, 201px", 201},
      {"(min-width:500px) calc(201px), calc(40vw)", 201},
      {"(min-width:5000px) calc(40vw), calc(201px)", 201},
      {"(min-width:5000px) 200px, 400px", 400},
      {"(blalbadfsdf) 200px, 400px", 400},
      {"0", 0},
      {"-0", 0},
      {"1", 500.5},
      {"300px, 400px", 300},
      {"(min-width:5000px) 200px, (min-width:500px) 400px", 400},
      {"", 500.5},
      {"  ", 500.5},
      {" /**/ ", 500.5},
      {" /**/ 300px", 300},
      {"300px /**/ ", 300},
      {" /**/ (min-width:500px) /**/ 300px", 300},
      {"-100px, 200px", 200},
      {"-50vw, 20vw", 100.1},
      {"50asdf, 200px", 200},
      {"asdf, 200px", 200},
      {"(max-width: 3000px) 200w, 400w", 500.5},
      {",, , /**/ ,200px", 200},
      {"50vw", 250.25},
      {"50vh", 300},
      {"50vmin", 250.25},
      {"50vmax", 300},
      {"5em", 80},
      {"5rem", 80},
      {"calc(40vw*2)", 400.4},
      {"(min-width:5000px) calc(5000px/10), (min-width:500px) calc(1200px/3)",
       400},
      {"(min-width:500px) calc(1200/3)", 500.5},
      {"(min-width:500px) calc(1200px/(0px*14))", 500.5},
      {"(max-width: 3000px) 200px, 400px", 200},
      {"(max-width: 3000px) 20em, 40em", 320},
      {"(max-width: 3000px) 0, 40em", 0},
      {"(max-width: 3000px) 0px, 40em", 0},
      {"(max-width: 3000px) 50vw, 40em", 250.25},
      {"(max-width: 3000px) 50px, 40vw", 50},
      {"((),1px", 500.5},
      {"{{},1px", 500.5},
      {"[[],1px", 500.5},
      {"x(x(),1px", 500.5},
      {"(max-width: 3000px) 50.5px, 40vw", 50.5},
      {"not (blabla) 50px, 40vw", 200.2},
      {"not (max-width: 100px) 50px, 40vw", 50},
  };

  MediaValues* media_values = GetTestMediaValues(500.5);

  for (const SizesAttributeParserTestCase& test_case : test_cases) {
    SizesAttributeParser parser(media_values, test_case.input, nullptr);
    EXPECT_EQ(test_case.effective_size, parser.Size().width) << test_case.input;
  }
}

TEST_F(SizesAttributeParserTest, NegativeSourceSizesValue) {
  SizesAttributeParser parser(GetTestMediaValues(), "-10px", nullptr);

  ASSERT_TRUE(!parser.IsAuto());
  ASSERT_EQ(500, parser.Size().width);
}

TEST_F(SizesAttributeParserTest, ZeroSourceSizesValue) {
  SizesAttributeParser parser(GetTestMediaValues(), "0px", nullptr);

  ASSERT_TRUE(!parser.IsAuto());
  ASSERT_EQ(0, parser.Size().width);
}

TEST_F(SizesAttributeParserTest, PositiveSourceSizesValue) {
  SizesAttributeParser parser(GetTestMediaValues(), "27px", nullptr);

  ASSERT_TRUE(!parser.IsAuto());
  ASSERT_EQ(27, parser.Size().width);
}

TEST_F(SizesAttributeParserTest, EmptySizes) {
  SizesAttributeParser parser(GetTestMediaValues(), "", nullptr);

  ASSERT_TRUE(!parser.IsAuto());
  ASSERT_EQ(500, parser.Size().width);
}

TEST_F(SizesAttributeParserTest, AutoSizes) {
  SizesAttributeParser parser(GetTestMediaValues(), "auto", nullptr);

  ASSERT_TRUE(parser.IsAuto());
  ASSERT_EQ(500, parser.Size().width);
}

TEST_F(SizesAttributeParserTest, AutoSizesNonLazyImg) {
  SetBodyInnerHTML(R"HTML(
    <img id="target" width="5" height="3" sizes="auto">
  )HTML");

  auto* img = To<HTMLImageElement>(GetElementById("target"));

  SizesAttributeParser parser(GetTestMediaValues(), "auto", nullptr, img);

  ASSERT_TRUE(parser.IsAuto());
  ASSERT_EQ(500, parser.Size().width);
}

TEST_F(SizesAttributeParserTest, AutoSizesLazyImgNoWidth) {
  SetBodyInnerHTML(R"HTML(
    <img id="target" sizes="auto" loading="lazy">
  )HTML");

  auto* img = To<HTMLImageElement>(GetElementById("target"));

  SizesAttributeParser parser(GetTestMediaValues(), "auto", nullptr, img);

  ASSERT_TRUE(parser.IsAuto());

  // When img does not have a width and height defined the value from the UA
  // style sheet will be used
  ASSERT_EQ(300, parser.Size().width);
}

TEST_F(SizesAttributeParserTest, AutoSizesLazyImgZeroWidth) {
  SetBodyInnerHTML(R"HTML(
    <img id="target" loading="lazy" width="0px" height="0px" sizes="auto">
  )HTML");

  auto* img = To<HTMLImageElement>(GetElementById("target"));

  SizesAttributeParser parser(GetTestMediaValues(), "auto", nullptr, img);

  ASSERT_TRUE(parser.IsAuto());
  ASSERT_EQ(0, parser.Size().width);
}

TEST_F(SizesAttributeParserTest, AutoSizesLazyImgSmallPositiveWidth) {
  SetBodyInnerHTML(R"HTML(
    <img id="target" loading="lazy" width="5" height="3" sizes="auto">
  )HTML");

  auto* img = To<HTMLImageElement>(GetElementById("target"));

  SizesAttributeParser parser(GetTestMediaValues(), "auto", nullptr, img);

  ASSERT_TRUE(parser.IsAuto());
  ASSERT_EQ(5, parser.Size().width);
}

TEST_F(SizesAttributeParserTest, AutoSizesLazyImgLargePositiveWidth) {
  SetBodyInnerHTML(R"HTML(
    <img id="target" loading="lazy" width="531px" height="246px" sizes="auto">
  )HTML");

  auto* img = To<HTMLImageElement>(GetElementById("target"));

  SizesAttributeParser parser(GetTestMediaValues(), "auto", nullptr, img);

  ASSERT_TRUE(parser.IsAuto());
  ASSERT_EQ(531, parser.Size().width);
}

TEST_F(SizesAttributeParserTest, AutoSizesUsesExplicitFallbackWhenNotRendered) {
  SetBodyInnerHTML(R"HTML(
    <img id="target" loading="lazy" width="531px" height="246px"
         sizes="auto, 123px">
  )HTML");

  auto* img = To<HTMLImageElement>(GetElementById("target"));

  // Parsing while rendered stores the last auto-sizes width; removal resets
  // it, so the explicit fallback is used afterwards.
  SizesAttributeParser rendered_parser(GetTestMediaValues(), "auto, 123px",
                                       nullptr, img);
  ASSERT_EQ(531, rendered_parser.Size().width);

  img->remove();

  SizesAttributeParser parser(GetTestMediaValues(), "auto, 123px", nullptr,
                              img);

  ASSERT_TRUE(parser.IsAuto());
  EXPECT_EQ(123, parser.Size().width);
}

TEST_F(SizesAttributeParserTest, AutoSizesUsesLastWidthWhenNotRendered) {
  SetBodyInnerHTML(R"HTML(
    <img id="target" loading="lazy" width="531px" height="246px"
         sizes="auto, 123px">
  )HTML");

  auto* img = To<HTMLImageElement>(GetElementById("target"));

  SizesAttributeParser rendered_parser(GetTestMediaValues(), "auto, 123px",
                                       nullptr, img);
  ASSERT_EQ(531, rendered_parser.Size().width);

  img->setAttribute(html_names::kStyleAttr, AtomicString("display: none"));
  UpdateAllLifecyclePhasesForTest();
  ASSERT_FALSE(img->IsBeingRendered());

  SizesAttributeParser parser(GetTestMediaValues(), "auto, 123px", nullptr,
                              img);

  ASSERT_TRUE(parser.IsAuto());
  EXPECT_EQ(531, parser.Size().width);
}

TEST_F(SizesAttributeParserTest, AutoSizesNeverRenderedUsesFallback) {
  SetBodyInnerHTML(R"HTML(
    <div style="display: none">
      <img id="target" loading="lazy" width="531px" height="246px"
           sizes="auto, 123px">
    </div>
  )HTML");

  auto* img = To<HTMLImageElement>(GetElementById("target"));
  ASSERT_FALSE(img->IsBeingRendered());

  SizesAttributeParser parser(GetTestMediaValues(), "auto, 123px", nullptr,
                              img);

  ASSERT_TRUE(parser.IsAuto());
  EXPECT_EQ(123, parser.Size().width);
}

TEST_F(SizesAttributeParserTest, AutoSizesPrefersRenderedWidthToFallback) {
  SetBodyInnerHTML(R"HTML(
    <img id="target" loading="lazy" width="531px" height="246px"
         sizes="auto, 123px">
  )HTML");

  auto* img = To<HTMLImageElement>(GetElementById("target"));
  SizesAttributeParser parser(GetTestMediaValues(), "auto, 123px", nullptr,
                              img);

  ASSERT_TRUE(parser.IsAuto());
  EXPECT_EQ(531, parser.Size().width);
}

TEST_F(SizesAttributeParserTest, TwoDimensionalSizes) {
  ScopedSrcsetTwoDimensionalSizesForTest scoped_feature(true);
  using Fit = EObjectFit;
  // The viewport is 500px x 600px.
  struct {
    const char* input;
    TargetBoxSize expected;
  } test_cases[] = {
      // Bare two-dimensional sizes.
      {"contain 100px 50px", {100, 50, Fit::kContain}},
      {"cover 100px 50px", {100, 50, Fit::kCover}},
      {"COVER 100px 50px", {100, 50, Fit::kCover}},
      {"Contain 100px 50px", {100, 50, Fit::kContain}},
      {"  contain   50vw   50vh  ", {250, 300, Fit::kContain}},
      {"cover calc(100px + 1px) 0", {101, 0, Fit::kCover}},
      {"contain min(100px, 50vw) max(10px, 10vh)", {100, 60, Fit::kContain}},
      {"cover clamp(10px, 50vw, 1000px) 5em", {250, 80, Fit::kCover}},
      {"cover 100px 50px, 400px", {100, 50, Fit::kCover}},
      {"400px, cover 100px 50px", {400}},

      // Matching media condition with a two-dimensional size.
      {"(min-width: 400px) contain 100px 50px", {100, 50, Fit::kContain}},
      {"(min-width:500px) cover 100px 50px, 400px", {100, 50, Fit::kCover}},
      {"(min-width: 400px) cover 50vw 50vh, 10px", {250, 300, Fit::kCover}},
      // Non-matching media condition falls back to a one-dimensional size.
      {"(min-width:5000px) cover 100px 50px, 400px", {400}},
      // Non-matching media condition falls back to another media condition
      // with a two-dimensional size.
      {"(min-width: 5000px) contain 100px 50px, "
       "(min-width: 400px) cover 200px 100px, 300px",
       {200, 100, Fit::kCover}},
      // Non-matching media condition falls back to a bare 2D size.
      {"(min-width: 5000px) contain 100px 50px, cover 200px 100px",
       {200, 100, Fit::kCover}},
      // A one-dimensional size with a matching media condition wins over a
      // later two-dimensional size.
      {"(min-width: 400px) 200px, (min-width: 400px) cover 100px 50px", {200}},
      // ... and vice versa.
      {"(min-width: 400px) cover 200px 100px, (min-width: 400px) 200px, 100px",
       {200, 100, Fit::kCover}},
      // A two-dimensional size with a matching media condition after a
      // one-dimensional size with a non-matching media condition.
      {"(max-width: 100px) 200px, (min-width: 400px) contain 100px 50px",
       {100, 50, Fit::kContain}},
      // Complex media conditions.
      {"not (max-width: 100px) cover 100px 50px, 10px", {100, 50, Fit::kCover}},
      {"not (min-width: 100px) cover 100px 50px, 10px", {10}},
      {"(min-width: 100px) and (max-width: 1000px) contain 100px 50px, 10px",
       {100, 50, Fit::kContain}},
      {"(min-width: 100px) and (max-width: 200px) contain 100px 50px, 10px",
       {10}},
      {"(max-width: 100px) or (min-width: 400px) cover 100px 50px, 10px",
       {100, 50, Fit::kCover}},
      {"(width >= 400px) cover 100px 50px, 10px", {100, 50, Fit::kCover}},
      {"(400px <= width <= 1000px) contain calc(10vw + 10px) 50px, 10px",
       {60, 50, Fit::kContain}},
      {"(width < 400px) cover 100px 50px, contain 20px 10px",
       {20, 10, Fit::kContain}},
      {"(orientation: portrait) cover 100px 50px, 10px",
       {100, 50, Fit::kCover}},
      {"(orientation: landscape) cover 100px 50px, 10px", {10}},
      // `auto` without an img falls back to the next source size, which may be
      // a two-dimensional size with a media condition.
      {"auto, (min-width: 400px) cover 150px 75px, 300px",
       {150, 75, Fit::kCover}},
      {"auto, (min-width: 4000px) cover 150px 75px, 300px", {300}},

      // Invalid: skipped in favor of the next source size.
      {"contain 100px, 200px", {200}},
      {"contain 100px 50px 20px, 200px", {200}},
      {"contain -100px 50px, 200px", {200}},
      {"contain 100px -50px, 200px", {200}},
      {"contain 100 50, 200px", {200}},
      {"contain 100px 50%, 200px", {200}},
      {"fill 100px 50px, 200px", {200}},
      {"scale-down 100px 50px, 200px", {200}},
      {"100px 50px, 200px", {200}},
      {"100px contain 50px, 200px", {200}},
      {"contain cover 100px 50px, 200px", {200}},
      {"contain, 200px", {200}},
      {"(min-width: 400px) contain 100px, 200px", {200}},
      {"(min-width: 400px) contain 100px -50px, 200px", {200}},
      {"(min-width: 400px) contain 100px 50px 25px, 200px", {200}},
      {"(min-width: 400px) contain 100px 50px 25px, "
       "(min-width: 400px) cover 20px 10px",
       {20, 10, Fit::kCover}},
      {"contain 100px 50px 20px", {500}},
  };

  MediaValues* media_values = GetTestMediaValues();
  for (const auto& test_case : test_cases) {
    SizesAttributeParser parser(media_values, test_case.input, nullptr);
    EXPECT_EQ(test_case.expected, parser.Size()) << test_case.input;
  }
}

TEST_F(SizesAttributeParserTest, TwoDimensionalSizesDisabled) {
  ScopedSrcsetTwoDimensionalSizesForTest scoped_feature(false);
  const char* inputs[] = {
      "cover 100px 50px, 200px",
      "(min-width: 400px) cover 100px 50px, 200px",
      "(min-width: 400px) contain 100px 50px, (min-width: 400px) 200px",
  };
  for (const char* input : inputs) {
    SizesAttributeParser parser(GetTestMediaValues(), input, nullptr);
    EXPECT_EQ((TargetBoxSize{200}), parser.Size()) << input;
  }
}

TEST_F(SizesAttributeParserTest, TwoDimensionalSizesAutoFallbackNotRendered) {
  ScopedSrcsetTwoDimensionalSizesForTest scoped_feature(true);
  // A never-rendered auto-sized img uses the explicit fallback, which carries
  // its own fit rather than the img's computed 'object-fit'.
  SetBodyInnerHTML(R"HTML(
    <div style="display: none">
      <img id="target" loading="lazy" width="531" height="246"
           style="object-fit: cover">
    </div>
  )HTML");

  auto* img = To<HTMLImageElement>(GetElementById("target"));
  ASSERT_FALSE(img->IsBeingRendered());

  SizesAttributeParser parser(
      GetTestMediaValues(),
      "auto, (min-width: 5000px) cover 1px 1px, (min-width: 400px) contain "
      "123px 45px, 10px",
      nullptr, img);
  ASSERT_TRUE(parser.IsAuto());
  EXPECT_EQ((TargetBoxSize{123, 45, EObjectFit::kContain}), parser.Size());

  // A one-dimensional fallback is 'fill', even though the img has
  // 'object-fit: cover'.
  SizesAttributeParser parser2(GetTestMediaValues(), "auto, 123px", nullptr,
                               img);
  EXPECT_EQ((TargetBoxSize{123}), parser2.Size());
}

TEST_F(SizesAttributeParserTest, AutoSizesTwoDimensional) {
  ScopedSrcsetTwoDimensionalSizesForTest scoped_feature(true);
  SetBodyInnerHTML(R"HTML(
    <img id="target" loading="lazy" width="531" height="246" sizes="auto"
         style="object-fit: scale-down">
  )HTML");

  auto* img = To<HTMLImageElement>(GetElementById("target"));
  SizesAttributeParser parser(GetTestMediaValues(), "auto", nullptr, img);
  EXPECT_EQ((TargetBoxSize{531, 246, EObjectFit::kScaleDown}), parser.Size());

  img->setAttribute(html_names::kStyleAttr,
                    AtomicString("object-fit: cover; display: none"));
  GetDocument().UpdateStyleAndLayoutTree();
  ASSERT_FALSE(img->IsBeingRendered());

  // The last auto-sizes size and object-fit are used when not rendered.
  SizesAttributeParser parser2(GetTestMediaValues(), "auto", nullptr, img);
  EXPECT_EQ((TargetBoxSize{531, 246, EObjectFit::kScaleDown}), parser2.Size());

  img->setAttribute(html_names::kStyleAttr, AtomicString("object-fit: cover"));
  // Layout is needed so the new layout box has a content box size.
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);
  SizesAttributeParser parser3(GetTestMediaValues(), "auto", nullptr, img);
  EXPECT_EQ((TargetBoxSize{531, 246, EObjectFit::kCover}), parser3.Size());
}

TEST_F(SizesAttributeParserTest, AutoSizesTwoDimensionalDisabled) {
  ScopedSrcsetTwoDimensionalSizesForTest scoped_feature(false);
  SetBodyInnerHTML(R"HTML(
    <img id="target" loading="lazy" width="531" height="246" sizes="auto"
         style="object-fit: cover">
  )HTML");

  auto* img = To<HTMLImageElement>(GetElementById("target"));
  SizesAttributeParser parser(GetTestMediaValues(), "auto", nullptr, img);
  EXPECT_EQ((TargetBoxSize{531}), parser.Size());
}

}  // namespace blink
