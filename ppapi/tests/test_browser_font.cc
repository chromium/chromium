// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ppapi/tests/test_browser_font.h"

#include <stdint.h>
#include <string.h>

#include "ppapi/cpp/image_data.h"
#include "ppapi/cpp/trusted/browser_font_trusted.h"
#include "ppapi/tests/test_utils.h"
#include "ppapi/tests/testing_instance.h"

REGISTER_TEST_CASE(BrowserFont);

bool TestBrowserFont::Init() {
  return true;
}

void TestBrowserFont::RunTests(const std::string& filter) {
  RUN_TEST(FontFamilies, filter);
  RUN_TEST(Measure, filter);
  RUN_TEST(MeasureRTL, filter);
  RUN_TEST(CharPos, filter);
  // This test is disabled. It doesn't currently pass. See the
  // CharacterOffsetForPixel API.
  //RUN_TEST(CharPosRTL, filter);
  RUN_TEST(Draw, filter);
}

// Just tests that GetFontFamilies is hooked up & returns something.
std::string TestBrowserFont::TestFontFamilies() {
  // This function is only supported out-of-process.
  const PPB_Testing_Private* testing_interface = GetTestingInterface();
  if (testing_interface && !testing_interface->IsOutOfProcess())
    PASS();

  pp::Var families = pp::BrowserFont_Trusted::GetFontFamilies(instance_);

  ASSERT_TRUE(families.is_string());
  ASSERT_TRUE(!families.AsString().empty());
  PASS();
}

// Tests that measuring text behaves reasonably. We aren't sure if the browser
// will be doing kerning or something for the particular default font, so we
// just make a string that we're pretty sure should be more than twice as long
// as another one, and verify that condition.
std::string TestBrowserFont::TestMeasure() {
  pp::BrowserFontDescription desc;
  pp::BrowserFont_Trusted font(instance_, desc);

  int32_t length1 = font.MeasureText(pp::BrowserFontTextRun("WWW"));
  ASSERT_TRUE(length1 > 0);
  int32_t length2 = font.MeasureText(pp::BrowserFontTextRun("WWWWWWWW"));
  ASSERT_TRUE(length2 >= length1 * 2);

  // Fallback font test.
  int32_t length3 = font.MeasureText(pp::BrowserFontTextRun("こんにちは"));
  ASSERT_TRUE(length3 > 0);

  PASS();
}

std::string TestBrowserFont::TestMeasureRTL() {
  pp::BrowserFontDescription desc;
  pp::BrowserFont_Trusted font(instance_, desc);

  // Mixed string, two chars of LTR, two of RTL, then two of LTR.
  // Note this is in UTF-8 so has more than 6 bytes.
  std::string mixed("AB\xd7\x94\xd7\x97ZZ");
  const int kNumChars = 6;
  pp::BrowserFontTextRun run(mixed);

  // Note that since this is UTF-8, the two RTL chars are two bytes each.
  int32_t len[kNumChars];
  len[0] = font.PixelOffsetForCharacter(run, 0);
  len[1] = font.PixelOffsetForCharacter(run, 1);
  len[2] = font.PixelOffsetForCharacter(run, 2);
  len[3] = font.PixelOffsetForCharacter(run, 3);
  len[4] = font.PixelOffsetForCharacter(run, 4);
  len[5] = font.PixelOffsetForCharacter(run, 5);

  // First three chars should be increasing.
  ASSERT_TRUE(len[0] >= 0);
  ASSERT_TRUE(len[1] > len[0]);
  ASSERT_TRUE(len[3] > len[1]);
  ASSERT_TRUE(len[2] > len[3]);
  ASSERT_TRUE(len[4] > len[2]);
  ASSERT_TRUE(len[5] > len[4]);

  // Test the same sequence with force LTR. The offsets should appear in
  // sequence.
  pp::BrowserFontTextRun forced_run(mixed, false, true);
  len[0] = font.PixelOffsetForCharacter(forced_run, 0);
  len[1] = font.PixelOffsetForCharacter(forced_run, 1);
  len[2] = font.PixelOffsetForCharacter(forced_run, 2);
  len[3] = font.PixelOffsetForCharacter(forced_run, 3);
  len[4] = font.PixelOffsetForCharacter(forced_run, 4);
  len[5] = font.PixelOffsetForCharacter(forced_run, 5);
  for (int i = 1; i < kNumChars; i++)
    ASSERT_TRUE(len[i] > len[i - 1]);

  PASS();
}

// Tests that the character/pixel offset functions correctly round-trip.
std::string TestBrowserFont::TestCharPos() {
  pp::BrowserFontDescription desc;
  pp::BrowserFont_Trusted font(instance_, desc);

  pp::BrowserFontTextRun run("Hello, world");
  uint32_t original_char = 3;
  uint32_t pixel_offset = font.PixelOffsetForCharacter(run, original_char);
  ASSERT_TRUE(pixel_offset > 0);

  uint32_t computed_char = font.CharacterOffsetForPixel(
      run, static_cast<int32_t>(pixel_offset));
  ASSERT_TRUE(computed_char == original_char);

  PASS();
}

// Tests that we can get character positions in a mixed LTR/RTL run.
std::string TestBrowserFont::TestCharPosRTL() {
  pp::BrowserFontDescription desc;
  pp::BrowserFont_Trusted font(instance_, desc);

  // Mixed string, two chars of LTR, two of RTL, than two of LTR.
  // Note this is in UTF-8 so has more than 6 bytes.
  std::string mixed("AB\xd7\x94\xd7\x97ZZ");

  pp::BrowserFontTextRun run(mixed);
  static const int kNumChars = 6;
  int expected_char_sequence[kNumChars] = { 0, 1, 3, 2, 4, 5 };

  // Check that the characters appear in the order we expect.
  int pixel_width = font.MeasureText(pp::BrowserFontTextRun(mixed));
  int last_sequence = 0;  // Index into expected_char_sequence.
  for (int x = 0; x < pixel_width; x++) {
    int cur_char = font.CharacterOffsetForPixel(run, x);
    if (cur_char != expected_char_sequence[last_sequence]) {
      // This pixel has a different character. It should be the next one in
      // the sequence for it to be correct.
      last_sequence++;
      ASSERT_TRUE(last_sequence < kNumChars);
      ASSERT_TRUE(cur_char == expected_char_sequence[last_sequence]);
    }
  }

  // Try the same string with force LTR. The characters should all appear in
  // sequence.
  pp::BrowserFontTextRun forced_run(mixed, false, true);
  int last_forced_char = 0;  // Char index into the forced sequence.
  for (int x = 0; x < pixel_width; x++) {
    int cur_char = font.CharacterOffsetForPixel(forced_run, x);
    if (cur_char != last_forced_char) {
      last_forced_char++;
      ASSERT_TRUE(cur_char == last_forced_char);
    }
  }

  PASS();
}

// Tests that drawing some text produces "some" output.
std::string TestBrowserFont::TestDraw() {
  pp::BrowserFontDescription desc;
  desc.set_family(PP_BROWSERFONT_TRUSTED_FAMILY_MONOSPACE);
  desc.set_size(10);
  pp::BrowserFont_Trusted font(instance_, desc);

  const pp::Size kSize(30, 10);
  pp::ImageData image(instance_,
                      PP_IMAGEDATAFORMAT_BGRA_PREMUL,  // 0xAARRGGBB
                      kSize,
                      false);  // init_to_zero
  ASSERT_FALSE(image.is_null());

  // Draw black text on white canvas.
  memset(image.data(), 0xFF, 4 * kSize.GetArea());
  font.DrawSimpleText(&image,
                      "Hello",
                      pp::Point(0, 10),  // Baseline position.
                      0xFF000000,  // Black text.
                      true);  // image_data_is_opaque.

  // Expect that at least a few pixels are non-white (text).
  // Due to blending, there may be rounding errors and
  // checking for exact black may not be correct.
  // Also expect that all pixels are opaque.
  const uint32_t kRGBMask = 0x00FFFFFF;
  const uint32_t kAlphaMask = 0xFF000000;
  int text_pixels = 0, opaque_pixels = 0;
  const uint32_t* pixels = static_cast<const uint32_t*>(image.data());
  for (int i = 0; i < kSize.GetArea(); ++i) {
    if ((pixels[i] & kRGBMask) != kRGBMask)
      ++text_pixels;
    if ((pixels[i] & kAlphaMask) == kAlphaMask)
      ++opaque_pixels;
  }
  ASSERT_GT(text_pixels, 0);
  ASSERT_EQ(opaque_pixels, kSize.GetArea());
  PASS();
}
