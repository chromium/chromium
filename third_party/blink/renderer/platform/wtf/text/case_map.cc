// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/wtf/text/case_map.h"

#include <unicode/casemap.h>

#include "base/notreached.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/blink/renderer/platform/wtf/text/character_names.h"
#include "third_party/blink/renderer/platform/wtf/text/string_buffer.h"
#include "third_party/blink/renderer/platform/wtf/text/string_impl.h"
#include "third_party/blink/renderer/platform/wtf/text/string_view.h"
#include "third_party/blink/renderer/platform/wtf/text/text_offset_map.h"
#include "third_party/blink/renderer/platform/wtf/text/unicode.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

namespace {

// `lang` - Language code.  Now it is one of "tr", "az", "el", and "lt".
inline bool LocaleIdMatchesLang(const AtomicString& locale_id,
                                const StringView lang) {
  const wtf_size_t lang_length = lang.length();
  CHECK(lang_length == 2u || lang_length == 3u);
  const StringImpl* locale_id_impl = locale_id.Impl();
  if (!locale_id_impl || !locale_id_impl->StartsWithIgnoringASCIICase(lang)) {
    return false;
  }
  if (locale_id_impl->length() == lang_length) {
    return true;
  }
  const UChar maybe_delimiter = (*locale_id_impl)[lang_length];
  return maybe_delimiter == '-' || maybe_delimiter == '_' ||
         maybe_delimiter == '@';
}

enum class CaseMapType { kLower, kUpper, kTitle };

icu::Edits DecreaseFirstEditLength(const icu::Edits& edits) {
  icu::Edits new_edits;
  UErrorCode error = U_ZERO_ERROR;
  auto edit = edits.getFineIterator();

  edit.next(error);
  int32_t new_length = edit.oldLength() - 1;
  if (new_length > 0) {
    new_edits.addUnchanged(new_length);
  }

  while (edit.next(error)) {
    if (edit.hasChange()) {
      new_edits.addReplace(edit.oldLength(), edit.newLength());
    } else {
      new_edits.addUnchanged(edit.oldLength());
    }
  }
  DCHECK(U_SUCCESS(error));
  return new_edits;
}

scoped_refptr<StringImpl> CaseConvert(CaseMapType type,
                                      StringImpl* source,
                                      const char* locale,
                                      TextOffsetMap* offset_map = nullptr,
                                      UChar previous_character = 0) {
  DCHECK(source);
  CHECK_LE(source->length(),
           static_cast<wtf_size_t>(std::numeric_limits<int32_t>::max()));

  scoped_refptr<StringImpl> upconverted = source->UpconvertedString();
  const base::span<const UChar> source16 = upconverted->Span16();

  base::span<UChar> data16;
  scoped_refptr<StringImpl> output =
      StringImpl::CreateUninitialized(source16.size(), data16);
  wtf_size_t target_length = 0;
  while (true) {
    UErrorCode status = U_ZERO_ERROR;
    icu::Edits edits;
    switch (type) {
      case CaseMapType::kLower:
        target_length = icu::CaseMap::toLower(
            locale, /* options */ 0,
            reinterpret_cast<const char16_t*>(source16.data()), source16.size(),
            reinterpret_cast<char16_t*>(data16.data()), data16.size(), &edits,
            status);
        break;
      case CaseMapType::kUpper:
        target_length = icu::CaseMap::toUpper(
            locale, /* options */ 0,
            reinterpret_cast<const char16_t*>(source16.data()), source16.size(),
            reinterpret_cast<char16_t*>(data16.data()), data16.size(), &edits,
            status);
        break;
      case CaseMapType::kTitle: {
        unsigned source_length = source16.size();
        StringBuffer<UChar> string_buffer(source_length + 1);
        base::span<UChar> string_with_previous = string_buffer.Span();
        bool is_previous_alpha = u_isalpha(previous_character);
        // Use an 'A' as a previous character which is already capitalized to
        // make sure the titlecasing with previous character.
        string_with_previous[0] =
            is_previous_alpha ? u'A' : blink::uchar::kSpace;
        string_with_previous.subspan(1u).copy_from(source16);

        // Add a space of the previous character at the start.
        unsigned data_with_previous_length =
            (target_length ? target_length : source_length) + 1;
        StringBuffer<UChar> data_buffer(data_with_previous_length);
        base::span<UChar> data_with_previous = data_buffer.Span();

        target_length = icu::CaseMap::toTitle(
            locale, U_TITLECASE_NO_LOWERCASE, /* iter */ nullptr,
            reinterpret_cast<const char16_t*>(string_with_previous.data()),
            string_with_previous.size(),
            reinterpret_cast<char16_t*>(data_with_previous.data()),
            data_with_previous.size(), &edits, status);

        if (U_FAILURE(status)) {
          break;
        }

        // Remove the space of the previous character at the start.
        --target_length;
        // Copy the result without the previous character.
        data16.copy_from(data_with_previous.subspan(1u));
        // Since the text included the previous character, decrease length of
        // the first edit entry.
        edits = DecreaseFirstEditLength(edits);
        break;
      }
    }
    if (U_SUCCESS(status)) {
      if (!edits.hasChanges())
        return source;

      if (offset_map)
        offset_map->Append(edits);

      if (source16.size() == target_length) {
        return output;
      }
      return output->Substring(0, target_length);
    }

    // Expand the buffer and retry if the target is longer.
    if (status == U_BUFFER_OVERFLOW_ERROR) {
      output = StringImpl::CreateUninitialized(target_length, data16);
      continue;
    }

    NOTREACHED();
  }
}

}  // namespace

const char* CaseMap::Locale::turkic_or_azeri_ = "tr";
const char* CaseMap::Locale::greek_ = "el";
const char* CaseMap::Locale::lithuanian_ = "lt";
const char* CaseMap::Locale::dutch_ = "nl";

CaseMap::Locale::Locale(const AtomicString& locale) {
  // Use the more optimized code path most of the time.
  //
  // Only Turkic (tr and az) languages and Lithuanian requires
  // locale-specific lowercasing rules. Even though CLDR has el-Lower,
  // it's identical to the locale-agnostic lowercasing. Context-dependent
  // handling of Greek capital sigma is built into the common lowercasing
  // function in ICU.
  //
  // Only Turkic (tr and az) languages, Greek and Lithuanian require
  // locale-specific uppercasing rules.
  //
  // Only Dutch language requires locale-specific titlecasing rules.
  if (LocaleIdMatchesLang(locale, "tr") || LocaleIdMatchesLang(locale, "az"))
      [[unlikely]] {
    case_map_locale_ = turkic_or_azeri_;
  } else if (LocaleIdMatchesLang(locale, "el")) [[unlikely]] {
    case_map_locale_ = greek_;
  } else if (LocaleIdMatchesLang(locale, "lt")) [[unlikely]] {
    case_map_locale_ = lithuanian_;
  } else if (LocaleIdMatchesLang(locale, "nl")) [[unlikely]] {
    case_map_locale_ = dutch_;
  } else {
    case_map_locale_ = nullptr;
  }
}

scoped_refptr<StringImpl> CaseMap::TryFastToLowerInvariant(StringImpl* source) {
  DCHECK(source);

  // Note: This is a hot function in the Dromaeo benchmark, specifically the
  // no-op code path up through the first 'return' statement.

  // First scan the string for uppercase and non-ASCII characters:
  if (source->Is8Bit()) {
    const base::span<const LChar> source8 = source->Span8();
    size_t first_index_to_be_lowered = source8.size();
    for (size_t i = 0; i < source8.size(); ++i) {
      const LChar ch = source8[i];
      if (IsASCIIUpper(ch) || ch & ~0x7F) [[unlikely]] {
        first_index_to_be_lowered = i;
        break;
      }
    }

    // Nothing to do if the string is all ASCII with no uppercase.
    if (first_index_to_be_lowered == source8.size()) {
      return source;
    }

    base::span<LChar> data8;
    scoped_refptr<StringImpl> new_impl =
        StringImpl::CreateUninitialized(source8.size(), data8);

    auto [source8_already_lowercase, source8_tail] =
        source8.split_at(first_index_to_be_lowered);
    auto [data8_already_lowercase, data8_tail] =
        data8.split_at(first_index_to_be_lowered);

    data8_already_lowercase.copy_from(source8_already_lowercase);

    for (size_t i = 0; i < source8_tail.size(); ++i) {
      const LChar ch = source8_tail[i];
      LChar lowered_ch;
      if (ch & ~0x7F) [[unlikely]] {
        lowered_ch = static_cast<LChar>(blink::unicode::ToLower(ch));
      } else {
        lowered_ch = ToASCIILower(ch);
      }
      data8_tail[i] = lowered_ch;
    }
    return new_impl;
  }

  bool no_upper = true;
  UChar ored = 0;

  const base::span<const UChar> source16 = source->Span16();
  for (size_t i = 0; i < source16.size(); ++i) {
    const UChar ch = source16[i];
    if (IsASCIIUpper(ch)) [[unlikely]] {
      no_upper = false;
    }
    ored |= ch;
  }
  // Nothing to do if the string is all ASCII with no uppercase.
  if (no_upper && !(ored & ~0x7F))
    return source;

  CHECK_LE(source16.size(),
           static_cast<wtf_size_t>(std::numeric_limits<int32_t>::max()));

  if (!(ored & ~0x7F)) {
    base::span<UChar> data16;
    scoped_refptr<StringImpl> new_impl =
        StringImpl::CreateUninitialized(source16.size(), data16);

    for (size_t i = 0; i < source16.size(); ++i) {
      data16[i] = ToASCIILower(source16[i]);
    }
    return new_impl;
  }

  // The fast code path was not able to handle this case.
  return nullptr;
}

scoped_refptr<StringImpl> CaseMap::FastToLowerInvariant(StringImpl* source) {
  // Note: This is a hot function in the Dromaeo benchmark.
  DCHECK(source);
  if (scoped_refptr<StringImpl> result = TryFastToLowerInvariant(source))
    return result;
  const char* locale = "";  // "" = root locale.
  return CaseConvert(CaseMapType::kLower, source, locale);
}

scoped_refptr<StringImpl> CaseMap::ToLowerInvariant(StringImpl* source,
                                                    TextOffsetMap* offset_map) {
  DCHECK(source);
  DCHECK(!offset_map || offset_map->IsEmpty());
  if (scoped_refptr<StringImpl> result = TryFastToLowerInvariant(source))
    return result;
  const char* locale = "";  // "" = root locale.
  return CaseConvert(CaseMapType::kLower, source, locale, offset_map);
}

scoped_refptr<StringImpl> CaseMap::ToUpperInvariant(StringImpl* source,
                                                    TextOffsetMap* offset_map) {
  DCHECK(source);
  DCHECK(!offset_map || offset_map->IsEmpty());

  // This function could be optimized for no-op cases the way LowerUnicode() is,
  // but in empirical testing, few actual calls to UpperUnicode() are no-ops, so
  // it wouldn't be worth the extra time for pre-scanning.

  CHECK_LE(source->length(),
           static_cast<wtf_size_t>(std::numeric_limits<int32_t>::max()));

  if (source->Is8Bit()) {
    const base::span<const LChar> source8 = source->Span8();
    base::span<LChar> data8;
    scoped_refptr<StringImpl> new_impl =
        StringImpl::CreateUninitialized(source8.size(), data8);

    // Do a faster loop for the case where all the characters are ASCII.
    LChar ored = 0;
    for (size_t i = 0; i < source8.size(); ++i) {
      const LChar c = source8[i];
      ored |= c;
      data8[i] = ToASCIIUpper(c);
    }
    if (!(ored & ~0x7F))
      return new_impl;

    // Do a slower implementation for cases that include non-ASCII Latin-1
    // characters.
    size_t count_sharp_s_characters = 0;

    // There are two special cases.
    //  1. latin-1 characters when converted to upper case are 16 bit
    //     characters.
    //  2. Lower case sharp-S converts to "SS" (two characters)
    for (size_t i = 0; i < source8.size(); ++i) {
      const LChar c = source8[i];
      if (c == blink::uchar::kLatinSmallLetterSharpS) [[unlikely]] {
        ++count_sharp_s_characters;
      }
      const UChar upper = static_cast<UChar>(blink::unicode::ToUpper(c));
      if (upper > 0xff) [[unlikely]] {
        // Since this upper-cased character does not fit in an 8-bit string, we
        // need to take the 16-bit path.
        goto upconvert;
      }
      data8[i] = static_cast<LChar>(upper);
    }

    if (!count_sharp_s_characters) {
      return new_impl;
    }

    // We have numberSSCharacters sharp-s characters, but none of the other
    // special characters.
    new_impl = StringImpl::CreateUninitialized(
        source8.size() + count_sharp_s_characters, data8);

    size_t dest_index = 0;
    for (size_t i = 0; i < source8.size(); ++i) {
      const LChar c = source8[i];
      if (c == blink::uchar::kLatinSmallLetterSharpS) {
        data8[dest_index++] = 'S';
        data8[dest_index++] = 'S';
        if (offset_map)
          offset_map->Append(i + 1, dest_index);
      } else {
        data8[dest_index++] = static_cast<LChar>(blink::unicode::ToUpper(c));
      }
    }
    return new_impl;
  }

upconvert:
  scoped_refptr<StringImpl> upconverted = source->UpconvertedString();
  base::span<const UChar> source16 = upconverted->Span16();

  base::span<UChar> data16;
  scoped_refptr<StringImpl> new_impl =
      StringImpl::CreateUninitialized(source16.size(), data16);

  // Do a faster loop for the case where all the characters are ASCII.
  UChar ored = 0;
  for (size_t i = 0; i < source16.size(); ++i) {
    const UChar c = source16[i];
    ored |= c;
    data16[i] = ToASCIIUpper(c);
  }
  if (!(ored & ~0x7F))
    return new_impl;

  // Do a slower implementation for cases that include non-ASCII characters.
  const char* locale = "";  // "" = root locale.
  return CaseConvert(CaseMapType::kUpper, source, locale, offset_map);
}

scoped_refptr<StringImpl> CaseMap::ToLower(StringImpl* source,
                                           TextOffsetMap* offset_map) const {
  DCHECK(source);
  DCHECK(!offset_map || offset_map->IsEmpty());

  if (!case_map_locale_)
    return ToLowerInvariant(source, offset_map);
  return CaseConvert(CaseMapType::kLower, source, case_map_locale_, offset_map);
}

scoped_refptr<StringImpl> CaseMap::ToUpper(StringImpl* source,
                                           TextOffsetMap* offset_map) const {
  DCHECK(source);
  DCHECK(!offset_map || offset_map->IsEmpty());

  if (!case_map_locale_)
    return ToUpperInvariant(source, offset_map);
  return CaseConvert(CaseMapType::kUpper, source, case_map_locale_, offset_map);
}

String CaseMap::ToLower(const String& source, TextOffsetMap* offset_map) const {
  DCHECK(!offset_map || offset_map->IsEmpty());

  if (StringImpl* impl = source.Impl())
    return ToLower(impl, offset_map);
  return String();
}

String CaseMap::ToUpper(const String& source, TextOffsetMap* offset_map) const {
  DCHECK(!offset_map || offset_map->IsEmpty());

  if (StringImpl* impl = source.Impl())
    return ToUpper(impl, offset_map);
  return String();
}

String CaseMap::ToTitle(const String& source,
                        TextOffsetMap* offset_map,
                        UChar previous_character) const {
  DCHECK(!offset_map || offset_map->IsEmpty());

  if (StringImpl* impl = source.Impl()) {
    return CaseConvert(CaseMapType::kTitle, impl, case_map_locale_, offset_map,
                       previous_character);
  }
  return String();
}

}  // namespace blink
