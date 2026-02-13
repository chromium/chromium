// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_TEXT_STRING_TO_NUMBER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_TEXT_STRING_TO_NUMBER_H_

#include "base/containers/span.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"
#include "third_party/blink/renderer/platform/wtf/text/number_parsing_options.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_uchar.h"
#include "third_party/blink/renderer/platform/wtf/wtf_export.h"

namespace blink {

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
WTF_EXPORT int CharactersToInt(base::span<const LChar>,
                               NumberParsingOptions,
                               bool* ok);
WTF_EXPORT int CharactersToInt(base::span<const UChar>,
                               NumberParsingOptions,
                               bool* ok);

// string -> unsigned.
WTF_EXPORT unsigned HexCharactersToUInt(base::span<const LChar>,
                                        NumberParsingOptions,
                                        bool* ok);
WTF_EXPORT unsigned HexCharactersToUInt(base::span<const UChar>,
                                        NumberParsingOptions,
                                        bool* ok);
WTF_EXPORT uint64_t HexCharactersToUInt64(base::span<const UChar>,
                                          NumberParsingOptions,
                                          bool* ok);
WTF_EXPORT uint64_t HexCharactersToUInt64(base::span<const LChar>,
                                          NumberParsingOptions,
                                          bool* ok);
WTF_EXPORT unsigned CharactersToUInt(base::span<const LChar>,
                                     NumberParsingOptions,
                                     bool* ok);
WTF_EXPORT unsigned CharactersToUInt(base::span<const UChar>,
                                     NumberParsingOptions,
                                     bool* ok);

// NumberParsingResult versions of CharactersToUInt. They can detect
// overflow. |NumberParsingResult*| should not be nullptr;
WTF_EXPORT unsigned CharactersToUInt(base::span<const LChar>,
                                     NumberParsingOptions,
                                     NumberParsingResult*);
WTF_EXPORT unsigned CharactersToUInt(base::span<const UChar>,
                                     NumberParsingOptions,
                                     NumberParsingResult*);

// string -> int64_t.
WTF_EXPORT int64_t CharactersToInt64(base::span<const LChar>,
                                     NumberParsingOptions,
                                     bool* ok);
WTF_EXPORT int64_t CharactersToInt64(base::span<const UChar>,
                                     NumberParsingOptions,
                                     bool* ok);

// string -> uint64_t.
WTF_EXPORT uint64_t CharactersToUInt64(base::span<const LChar>,
                                       NumberParsingOptions,
                                       bool* ok);
WTF_EXPORT uint64_t CharactersToUInt64(base::span<const UChar>,
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
WTF_EXPORT double CharactersToDouble(base::span<const LChar>, bool* ok);
WTF_EXPORT double CharactersToDouble(base::span<const UChar>, bool* ok);

// |parsed_length| will have the length of characters which was parsed as a
// double number. It will be 0 if the input string isn't a number. It will be
// smaller than |length| if the input string contains trailing
// whiespace/garbage.
WTF_EXPORT double CharactersToDouble(base::span<const LChar>,
                                     size_t& parsed_length);
WTF_EXPORT double CharactersToDouble(base::span<const UChar>,
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
WTF_EXPORT float CharactersToFloat(base::span<const LChar>, bool* ok);
WTF_EXPORT float CharactersToFloat(base::span<const UChar>, bool* ok);

// |parsed_length| will have the length of characters which was parsed as a
// flaot number. It will be 0 if the input string isn't a number. It will be
// smaller than |length| if the input string contains trailing
// whiespace/garbage.
WTF_EXPORT float CharactersToFloat(base::span<const LChar>,
                                   size_t& parsed_length);
WTF_EXPORT float CharactersToFloat(base::span<const UChar>,
                                   size_t& parsed_length);

// Parse `input` as a signed decimal number.
// If the input string is not acceptable, std::nullopt is returned.
WTF_EXPORT std::optional<int32_t> StringToInt(const StringView& input,
                                              NumberParsingOptions);
// Parse `input` as an unsigned decimal number.
// If the input string is not acceptable, std::nullopt is returned.
WTF_EXPORT std::optional<uint32_t> StringToUint(const StringView& input,
                                                NumberParsingOptions);
// Parse `input` as a signed decimal number.
// If the input string is not acceptable, std::nullopt is returned.
WTF_EXPORT std::optional<int64_t> StringToInt64(const StringView& input,
                                                NumberParsingOptions);
// Parse `input` as an unsigned decimal number.
// If the input string is not acceptable, std::nullopt is returned.
WTF_EXPORT std::optional<uint64_t> StringToUint64(const StringView& input,
                                                  NumberParsingOptions);
// Parse `input` as a hexadecimal number.
// If the input string is not acceptable, std::nullopt is returned.
WTF_EXPORT std::optional<uint32_t> HexStringToUint(const StringView& input,
                                                   NumberParsingOptions);
// Parse `input` as a hexadecimal number.
// If the input string is not acceptable, std::nullopt is returned.
WTF_EXPORT std::optional<uint64_t> HexStringToUint64(const StringView& input,
                                                     NumberParsingOptions);

// The following StringTo*Strict() and StringTo*Loose() exist for a historical
// reason. We should not use them for new code.

// The following StringToFooStrict functions accept:
//  - leading '+'
//  - leading Unicode whitespace
//  - trailing Unicode whitespace
//  - no "-0" (only for StringToUintStrict)
//  - no out-of-range numbers which the resultant type can't represent
//
// If the input string is not acceptable, std::nullopt is returned.
//
// We can use these functions to implement a Web Platform feature only if the
// input string is already valid according to the specification of the
// feature.
WTF_EXPORT std::optional<int32_t> StringToIntStrict(const StringView& input);
WTF_EXPORT std::optional<uint32_t> StringToUintStrict(const StringView& input);

// The following StringToFooLoose() functions accept:
//  - leading '+'
//  - leading Unicode whitespace
//  - trailing garbage
//  - no "-0" (only for StringToUint)
//  - no out-of-range numbers which the resultant type can't represent
//
// If the input string is not acceptable, std::nullopt is returned.
//
// We can use these functions to implement a Web Platform feature only if the
// input string is already valid according to the specification of the
// feature.
WTF_EXPORT std::optional<int32_t> StringToIntLoose(const StringView& input);
WTF_EXPORT std::optional<uint32_t> StringToUintLoose(const StringView& input);

// StringToDouble() and StringToFloat() functions accept:
//  - leading '+'
//  - numbers without leading zeros such as ".5"
//  - numbers ending with "." such as "3."
//  - scientific notation
//  - leading whitespace (IsASCIISpace, not IsHTMLSpace)
//  - no trailing whitespace
//  - no trailing garbage
//  - no numbers such as "NaN" "Infinity"
//
// A huge absolute number which a double/float can't represent is accepted,
// and +Infinity or -Infinity is returned.
//
// A small absolute numbers which a double/float can't represent is accepted,
// and 0 is returned
//
// If the input string is not acceptable, std::nullopt is returned.
//
// We can use these functions to implement a Web Platform feature only if the
// input string is already valid according to the specification of the
// feature.
WTF_EXPORT std::optional<double> StringToDouble(const StringView& input);
WTF_EXPORT std::optional<float> StringToFloat(const StringView& input);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_TEXT_STRING_TO_NUMBER_H_
