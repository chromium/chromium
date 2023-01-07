// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_TEXT_STRING_TO_NUMBER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_TEXT_STRING_TO_NUMBER_H_

#include "third_party/blink/renderer/platform/wtf/text/number_parsing_options.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_uchar.h"
#include "third_party/blink/renderer/platform/wtf/wtf_export.h"

namespace WTF {

enum class NumberParsingResult {
  kSuccess,
  kError,
  // For UInt functions, kOverflowMin never happens. Negative numbers are
  // treated as kError. This behavior matches to the HTML standard.
  // https://html.spec.whatwg.org/C/#rules-for-parsing-non-negative-integers
  kOverflowMin,
  kOverflowMax,
};

// string -> int.
WTF_EXPORT int CharactersToInt(const LChar*,
                               size_t,
                               NumberParsingOptions,
                               bool* ok);
WTF_EXPORT int CharactersToInt(const UChar*,
                               size_t,
                               NumberParsingOptions,
                               bool* ok);

// string -> unsigned.
WTF_EXPORT unsigned HexCharactersToUInt(const LChar*,
                                        size_t,
                                        NumberParsingOptions,
                                        bool* ok);
WTF_EXPORT unsigned HexCharactersToUInt(const UChar*,
                                        size_t,
                                        NumberParsingOptions,
                                        bool* ok);
WTF_EXPORT uint64_t HexCharactersToUInt64(const UChar*,
                                          size_t,
                                          NumberParsingOptions,
                                          bool* ok);
WTF_EXPORT uint64_t HexCharactersToUInt64(const LChar*,
                                          size_t,
                                          NumberParsingOptions,
                                          bool* ok);
WTF_EXPORT unsigned CharactersToUInt(const LChar*,
                                     size_t,
                                     NumberParsingOptions,
                                     bool* ok);
WTF_EXPORT unsigned CharactersToUInt(const UChar*,
                                     size_t,
                                     NumberParsingOptions,
                                     bool* ok);

// NumberParsingResult versions of CharactersToUInt. They can detect
// overflow. |NumberParsingResult*| should not be nullptr;
WTF_EXPORT unsigned CharactersToUInt(const LChar*,
                                     size_t,
                                     NumberParsingOptions,
                                     NumberParsingResult*);
WTF_EXPORT unsigned CharactersToUInt(const UChar*,
                                     size_t,
                                     NumberParsingOptions,
                                     NumberParsingResult*);

// string -> int64_t.
WTF_EXPORT int64_t CharactersToInt64(const LChar*,
                                     size_t,
                                     NumberParsingOptions,
                                     bool* ok);
WTF_EXPORT int64_t CharactersToInt64(const UChar*,
                                     size_t,
                                     NumberParsingOptions,
                                     bool* ok);

// string -> uint64_t.
WTF_EXPORT uint64_t CharactersToUInt64(const LChar*,
                                       size_t,
                                       NumberParsingOptions,
                                       bool* ok);
WTF_EXPORT uint64_t CharactersToUInt64(const UChar*,
                                       size_t,
                                       NumberParsingOptions,
                                       bool* ok);

// FIXME: Like the strict functions above, these give false for "ok" when there
// is trailing garbage.  Like the non-strict functions above, these return the
// value when there is trailing garbage.  It would be better if these were more
// consistent with the above functions instead.

// string -> double.
//
// These functions accepts:
//  - leading '+'
//  - numbers without leading zeros such as ".5"
//  - numbers ending with "." such as "3."
//  - scientific notation
//  - leading whitespace (IsASCIISpace, not IsHTMLSpace)
//  - no trailing whitespace
//  - no trailing garbage
//  - no numbers such as "NaN" "Infinity"
//
// A huge absolute number which a double can't represent is accepted, and
// +Infinity or -Infinity is returned.
//
// A small absolute numbers which a double can't represent is accepted, and
// 0 is returned
WTF_EXPORT double CharactersToDouble(const LChar*, size_t, bool* ok);
WTF_EXPORT double CharactersToDouble(const UChar*, size_t, bool* ok);

// |parsed_length| will have the length of characters which was parsed as a
// double number. It will be 0 if the input string isn't a number. It will be
// smaller than |length| if the input string contains trailing
// whiespace/garbage.
WTF_EXPORT double CharactersToDouble(const LChar*,
                                     size_t length,
                                     size_t& parsed_length);
WTF_EXPORT double CharactersToDouble(const UChar*,
                                     size_t length,
                                     size_t& parsed_length);

// string -> float.
//
// These functions accepts:
//  - leading '+'
//  - numbers without leading zeros such as ".5"
//  - numbers ending with "." such as "3."
//  - scientific notation
//  - leading whitespace (IsASCIISpace, not IsHTMLSpace)
//  - no trailing whitespace
//  - no trailing garbage
//  - no numbers such as "NaN" "Infinity"
//
// A huge absolute number which a float can't represent is accepted, and
// +Infinity or -Infinity is returned.
//
// A small absolute numbers which a float can't represent is accepted, and
// 0 is returned
WTF_EXPORT float CharactersToFloat(const LChar*, size_t, bool* ok);
WTF_EXPORT float CharactersToFloat(const UChar*, size_t, bool* ok);

// |parsed_length| will have the length of characters which was parsed as a
// flaot number. It will be 0 if the input string isn't a number. It will be
// smaller than |length| if the input string contains trailing
// whiespace/garbage.
WTF_EXPORT float CharactersToFloat(const LChar*,
                                   size_t length,
                                   size_t& parsed_length);
WTF_EXPORT float CharactersToFloat(const UChar*,
                                   size_t length,
                                   size_t& parsed_length);

}  // namespace WTF

using WTF::CharactersToInt;
using WTF::CharactersToUInt;
using WTF::CharactersToInt64;
using WTF::CharactersToUInt64;
using WTF::CharactersToDouble;
using WTF::CharactersToFloat;

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_TEXT_STRING_TO_NUMBER_H_
