// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/shaping/harfbuzz_shaper.h"

#include <stddef.h>
#include <stdint.h>
#include <unicode/ustring.h>

#include "base/command_line.h"
#include "base/logging/logging_settings.h"
#include "third_party/blink/renderer/platform/fonts/font.h"
#include "third_party/blink/renderer/platform/fonts/plain_text_node.h"
#include "third_party/blink/renderer/platform/fonts/shaping/shape_result_bloberizer.h"
#include "third_party/blink/renderer/platform/fonts/shaping/shape_result_view.h"
#include "third_party/blink/renderer/platform/fonts/text_fragment_paint_info.h"
#include "third_party/blink/renderer/platform/testing/blink_fuzzer_test_support.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/text/text_run.h"

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

  FontDescription font_description;
  Font font(font_description);
  // Set font size to something other than the default 0 size in
  // FontDescription, 16 matches the default text size in HTML.
  // We don't use a FontSelector here. Only look for system fonts for now.
  font_description.SetComputedSize(16.0f);

  // SAFETY: Just make a span from the function arguments provided by libfuzzer.
  String string(UNSAFE_BUFFERS(
      base::span(reinterpret_cast<const UChar*>(data),
                 std::min(kMaxInputLength, size / sizeof(UChar)))));
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
  constexpr unsigned word_length = 7;
  unsigned state = 0;
  for (unsigned from = 0; from < string.length(); from += word_length) {
    unsigned to = std::min(from + word_length, string.length());
    bool is_rtl = state & 0x2;
    bool is_override = state & 0x4;
    ++state;

    TextRun subrun(StringView(string, from, to - from),
                   is_rtl ? TextDirection::kRtl : TextDirection::kLtr,
                   is_override);

    PlainTextNode* node = MakeGarbageCollected<PlainTextNode>(
        subrun, /* normalize_space */ false, font, /* supports_bidi */ true,
        nullptr);
    if (!node->ContainsRtlItems()) {
      ShapeResultBloberizer::FillGlyphs bloberizer(
          font.GetFontDescription(), *node,
          ShapeResultBloberizer::Type::kEmitText);
      bloberizer.Blobs();
    } else {
      for (const PlainTextItem& item : node->ItemList()) {
        ShapeResultBloberizer::FillGlyphsNG bloberizer(
            font.GetFontDescription(), item.Text(), 0, item.Length(),
            item.EnsureView(), ShapeResultBloberizer::Type::kEmitText);
        bloberizer.Blobs();
      }
    }
  }

  return 0;
}

}  // namespace blink

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  return blink::LLVMFuzzerTestOneInput(data, size);
}
