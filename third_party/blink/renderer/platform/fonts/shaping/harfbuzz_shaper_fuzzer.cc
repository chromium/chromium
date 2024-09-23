// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/shaping/harfbuzz_shaper.h"

#include <stddef.h>
#include <stdint.h>
#include <unicode/ustring.h>

#include "base/command_line.h"
#include "third_party/blink/renderer/platform/fonts/font.h"
#include "third_party/blink/renderer/platform/fonts/font_cache.h"
#include "third_party/blink/renderer/platform/fonts/shaping/caching_word_shaper.h"
#include "third_party/blink/renderer/platform/fonts/shaping/shape_result_bloberizer.h"
#include "third_party/blink/renderer/platform/fonts/shaping/shape_result_view.h"
#include "third_party/blink/renderer/platform/fonts/text_fragment_paint_info.h"
#include "third_party/blink/renderer/platform/fonts/text_run_paint_info.h"
#include "third_party/blink/renderer/platform/testing/blink_fuzzer_test_support.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

namespace blink {

constexpr size_t kMaxInputLength = 256;

// TODO crbug.com/771901: BlinkFuzzerTestSupport should also initialize the
// custom fontconfig configuration that we use for content_shell.
int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  static BlinkFuzzerTestSupport fuzzer_support = BlinkFuzzerTestSupport();
  test::TaskEnvironment task_environment;

  if ((false)) {  // Add extra parenthesis to disable dead code warning.
    // The fuzzer driver does not pass along command line arguments, so add any
    // useful debugging command line arguments manually here.
    base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
    if (!command_line->HasSwitch("vmodule")) {
      command_line->AppendSwitchASCII("vmodule", "shape_result_bloberizer=4");
      logging::InitLogging(logging::LoggingSettings());
    }
  }

  FontCachePurgePreventer font_cache_purge_preventer;
  FontDescription font_description;
  Font font(font_description);
  // Set font size to something other than the default 0 size in
  // FontDescription, 16 matches the default text size in HTML.
  // We don't use a FontSelector here. Only look for system fonts for now.
  font_description.SetComputedSize(16.0f);

  String string(reinterpret_cast<const UChar*>(data),
                std::min(kMaxInputLength, size / sizeof(UChar)));
  HarfBuzzShaper shaper(string);
  const ShapeResult* result = shaper.Shape(&font, TextDirection::kLtr);

  // BloberizeNG
  ShapeResultView* result_view = ShapeResultView::Create(result);
  TextFragmentPaintInfo text_info{StringView(string), 0, string.length(),
                                  result_view};
  ShapeResultBloberizer::FillGlyphsNG bloberizer_ng(
      font.GetFontDescription(), text_info.text, text_info.from, text_info.to,
      text_info.shape_result, ShapeResultBloberizer::Type::kEmitText);
  bloberizer_ng.Blobs();

  // Bloberize
  CachingWordShaper word_shaper(font);
  TextRun text_run(string);
  constexpr unsigned word_length = 7;
  unsigned state = 0;
  for (unsigned from = 0; from < text_run.length(); from += word_length) {
    unsigned to = std::min(from + word_length, text_run.length());
    bool is_rtl = state & 0x2;
    bool is_override = state & 0x4;
    ++state;

    TextRun subrun = text_run.SubRun(from, to - from);
    subrun.SetDirection(is_rtl ? TextDirection::kRtl : TextDirection::kLtr);
    subrun.SetDirectionalOverride(is_override);

    TextRunPaintInfo subrun_info(subrun);
    ShapeResultBuffer buffer;
    word_shaper.FillResultBuffer(subrun_info, &buffer);
    ShapeResultBloberizer::FillGlyphs bloberizer(
        font.GetFontDescription(), subrun_info, buffer,
        ShapeResultBloberizer::Type::kEmitText);
    bloberizer.Blobs();
  }

  return 0;
}

}  // namespace blink

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  return blink::LLVMFuzzerTestOneInput(data, size);
}
