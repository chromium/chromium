// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/parser/sizes_attribute_parser.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/css/media_values_cached.h"
#include "third_party/blink/renderer/core/media_type_names.h"

namespace blink {

typedef struct {
  const char* input;
  const float effective_size;
} SizesAttributeParserTestCase;

TEST(SizesAttributeParserTest, Basic) {
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
      {nullptr, 0}  // Do not remove the terminator line.
  };

  MediaValuesCached::MediaValuesCachedData data;
  data.viewport_width = 500;
  data.viewport_height = 600;
  data.device_width = 500;
  data.device_height = 500;
  data.device_pixel_ratio = 2.0;
  data.color_bits_per_component = 24;
  data.monochrome_bits_per_component = 0;
  data.primary_pointer_type = kPointerTypeFine;
  data.default_font_size = 16;
  data.three_d_enabled = true;
  data.media_type = media_type_names::kScreen;
  data.strict_mode = true;
  data.display_mode = blink::mojom::DisplayMode::kBrowser;
  auto* media_values = MakeGarbageCollected<MediaValuesCached>(data);

  for (unsigned i = 0; test_cases[i].input; ++i) {
    SizesAttributeParser parser(media_values, test_cases[i].input);
    ASSERT_EQ(test_cases[i].effective_size, parser.length());
  }
}

TEST(SizesAttributeParserTest, FloatViewportWidth) {
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
      {nullptr, 0}  // Do not remove the terminator line.
  };

  MediaValuesCached::MediaValuesCachedData data;
  data.viewport_width = 500.5;
  data.viewport_height = 600;
  data.device_width = 500;
  data.device_height = 500;
  data.device_pixel_ratio = 2.0;
  data.color_bits_per_component = 24;
  data.monochrome_bits_per_component = 0;
  data.primary_pointer_type = kPointerTypeFine;
  data.default_font_size = 16;
  data.three_d_enabled = true;
  data.media_type = media_type_names::kScreen;
  data.strict_mode = true;
  data.display_mode = blink::mojom::DisplayMode::kBrowser;
  auto* media_values = MakeGarbageCollected<MediaValuesCached>(data);

  for (unsigned i = 0; test_cases[i].input; ++i) {
    SizesAttributeParser parser(media_values, test_cases[i].input);
    ASSERT_EQ(test_cases[i].effective_size, parser.length());
  }
}

}  // namespace blink
