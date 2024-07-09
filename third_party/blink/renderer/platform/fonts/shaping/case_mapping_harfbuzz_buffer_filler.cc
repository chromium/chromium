// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/platform/fonts/shaping/case_mapping_harfbuzz_buffer_filler.h"

#include <unicode/utf16.h>

#include "third_party/blink/renderer/platform/wtf/text/case_map.h"

namespace blink {

static const uint16_t* ToUint16(const UChar* src) {
  // FIXME: This relies on undefined behavior however it works on the
  // current versions of all compilers we care about and avoids making
  // a copy of the string.
  static_assert(sizeof(UChar) == sizeof(uint16_t),
                "UChar should be the same size as uint16_t");
  return reinterpret_cast<const uint16_t*>(src);
}

CaseMappingHarfBuzzBufferFiller::CaseMappingHarfBuzzBufferFiller(
    CaseMapIntend case_map_intend,
    const AtomicString& locale,
    hb_buffer_t* harfbuzz_buffer,
    const String& text,
    unsigned start_index,
    unsigned num_characters)
    : harfbuzz_buffer_(harfbuzz_buffer) {
  if (case_map_intend == CaseMapIntend::kKeepSameCase) {
    if (text.Is8Bit()) {
      hb_buffer_add_latin1(harfbuzz_buffer_, text.Characters8(), text.length(),
                           start_index, num_characters);
    } else {
      hb_buffer_add_utf16(harfbuzz_buffer_, ToUint16(text.Characters16()),
                          text.length(), start_index, num_characters);
    }
  } else {
    CaseMap case_map(locale);
    String case_mapped_text = case_map_intend == CaseMapIntend::kUpperCase
                                  ? case_map.ToUpper(text)
                                  : case_map.ToLower(text);
    case_mapped_text.Ensure16Bit();

    if (case_mapped_text.length() != text.length()) {
      String original_text = text;
      original_text.Ensure16Bit();
      FillSlowCase(case_map_intend, locale, original_text.Characters16(),
                   original_text.length(), start_index, num_characters);
      return;
    }

    DCHECK_EQ(case_mapped_text.length(), text.length());
    DCHECK(!case_mapped_text.Is8Bit());
    hb_buffer_add_utf16(harfbuzz_buffer_,
                        ToUint16(case_mapped_text.Characters16()),
                        text.length(), start_index, num_characters);
  }
}

// TODO(drott): crbug.com/623940 Fix lack of context sensitive case mapping
// here.
void CaseMappingHarfBuzzBufferFiller::FillSlowCase(
    CaseMapIntend case_map_intend,
    const AtomicString& locale,
    const UChar* buffer,
    unsigned buffer_length,
    unsigned start_index,
    unsigned num_characters) {
  // Record pre-context.
  hb_buffer_add_utf16(harfbuzz_buffer_, ToUint16(buffer), buffer_length,
                      start_index, 0);

  CaseMap case_map(locale);
  for (unsigned char_index = start_index;
       char_index < start_index + num_characters;) {
    unsigned new_char_index = char_index;
    U16_FWD_1(buffer, new_char_index, num_characters);
    String char_by_char(&buffer[char_index], new_char_index - char_index);
    String case_mapped_char;
    if (case_map_intend == CaseMapIntend::kUpperCase)
      case_mapped_char = case_map.ToUpper(char_by_char);
    else
      case_mapped_char = case_map.ToLower(char_by_char);

    for (unsigned j = 0; j < case_mapped_char.length();) {
      UChar32 codepoint = 0;
      U16_NEXT(case_mapped_char.Characters16(), j, case_mapped_char.length(),
               codepoint);
      // Add all characters of the case mapping result at the same cluster
      // position.
      hb_buffer_add(harfbuzz_buffer_, codepoint, char_index);
    }
    char_index = new_char_index;
  }

  // Record post-context
  hb_buffer_add_utf16(harfbuzz_buffer_, ToUint16(buffer), buffer_length,
                      start_index + num_characters, 0);
}

}  // namespace blink
