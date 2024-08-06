// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/platform/wtf/text/case_map.h"

#include <unicode/casemap.h>

#include "base/notreached.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/blink/renderer/platform/wtf/text/character_names.h"
#include "third_party/blink/renderer/platform/wtf/text/string_impl.h"
#include "third_party/blink/renderer/platform/wtf/text/string_view.h"
#include "third_party/blink/renderer/platform/wtf/text/text_offset_map.h"
#include "third_party/blink/renderer/platform/wtf/text/unicode.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace WTF {

namespace {

inline bool LocaleIdMatchesLang(const AtomicString& locale_id,
                                const StringView& lang) {
  CHECK_GE(lang.length(), 2u);
  CHECK_LE(lang.length(), 3u);
  if (!locale_id.Impl() || !locale_id.Impl()->StartsWithIgnoringCase(lang))
    return false;
  if (locale_id.Impl()->length() == lang.length())
    return true;
  const UChar maybe_delimiter = (*locale_id.Impl())[lang.length()];
  return maybe_delimiter == '-' || maybe_delimiter == '_' ||
         maybe_delimiter == '@';
}

enum class CaseMapType { kLower, kUpper };

scoped_refptr<StringImpl> CaseConvert(CaseMapType type,
                                      StringImpl* source,
                                      const char* locale,
                                      TextOffsetMap* offset_map = nullptr) {
  DCHECK(source);
  CHECK_LE(source->length(),
           static_cast<wtf_size_t>(std::numeric_limits<int32_t>::max()));
  const wtf_size_t source_length = source->length();

  scoped_refptr<StringImpl> upconverted = source->UpconvertedString();
  const UChar* source16 = upconverted->Characters16();

  UChar* data16;
  wtf_size_t target_length = source_length;
  scoped_refptr<StringImpl> output =
      StringImpl::CreateUninitialized(target_length, data16);
  while (true) {
    UErrorCode status = U_ZERO_ERROR;
    icu::Edits edits;
    switch (type) {
      case CaseMapType::kLower:
        target_length = icu::CaseMap::toLower(
            locale, /* options */ 0,
            reinterpret_cast<const char16_t*>(source16), source_length,
            reinterpret_cast<char16_t*>(data16), target_length, &edits, status);
        break;
      case CaseMapType::kUpper:
        target_length = icu::CaseMap::toUpper(
            locale, /* options */ 0,
            reinterpret_cast<const char16_t*>(source16), source_length,
            reinterpret_cast<char16_t*>(data16), target_length, &edits, status);
        break;
    }
    if (U_SUCCESS(status)) {
      if (!edits.hasChanges())
        return source;

      if (offset_map)
        offset_map->Append(edits);

      if (source_length == target_length)
        return output;
      return output->Substring(0, target_length);
    }

    // Expand the buffer and retry if the target is longer.
    if (status == U_BUFFER_OVERFLOW_ERROR) {
      output = StringImpl::CreateUninitialized(target_length, data16);
      continue;
    }

    NOTREACHED_IN_MIGRATION();
    return source;
  }
}

}  // namespace

const char* CaseMap::Locale::turkic_or_azeri_ = "tr";
const char* CaseMap::Locale::greek_ = "el";
const char* CaseMap::Locale::lithuanian_ = "lt";

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
  if (LocaleIdMatchesLang(locale, "tr") || LocaleIdMatchesLang(locale, "az"))
      [[unlikely]] {
    case_map_locale_ = turkic_or_azeri_;
  } else if (LocaleIdMatchesLang(locale, "el")) [[unlikely]] {
    case_map_locale_ = greek_;
  } else if (LocaleIdMatchesLang(locale, "lt")) [[unlikely]] {
    case_map_locale_ = lithuanian_;
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
    wtf_size_t first_index_to_be_lowered = source->length();
    for (wtf_size_t i = 0; i < source->length(); ++i) {
      LChar ch = source->Characters8()[i];
      if (IsASCIIUpper(ch) || ch & ~0x7F) [[unlikely]] {
        first_index_to_be_lowered = i;
        break;
      }
    }

    // Nothing to do if the string is all ASCII with no uppercase.
    if (first_index_to_be_lowered == source->length())
      return source;

    LChar* data8;
    scoped_refptr<StringImpl> new_impl =
        StringImpl::CreateUninitialized(source->length(), data8);
    memcpy(data8, source->Characters8(), first_index_to_be_lowered);

    for (wtf_size_t i = first_index_to_be_lowered; i < source->length(); ++i) {
      LChar ch = source->Characters8()[i];
      if (ch & ~0x7F) [[unlikely]] {
        data8[i] = static_cast<LChar>(unicode::ToLower(ch));
      } else {
        data8[i] = ToASCIILower(ch);
      }
    }

    return new_impl;
  }

  bool no_upper = true;
  UChar ored = 0;

  const UChar* end = source->Characters16() + source->length();
  for (const UChar* chp = source->Characters16(); chp != end; ++chp) {
    if (IsASCIIUpper(*chp)) [[unlikely]] {
      no_upper = false;
    }
    ored |= *chp;
  }
  // Nothing to do if the string is all ASCII with no uppercase.
  if (no_upper && !(ored & ~0x7F))
    return source;

  CHECK_LE(source->length(),
           static_cast<wtf_size_t>(std::numeric_limits<int32_t>::max()));
  int32_t length = source->length();

  if (!(ored & ~0x7F)) {
    UChar* data16;
    scoped_refptr<StringImpl> new_impl =
        StringImpl::CreateUninitialized(source->length(), data16);

    for (int32_t i = 0; i < length; ++i) {
      UChar c = source->Characters16()[i];
      data16[i] = ToASCIILower(c);
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
  int32_t length = source->length();

  if (source->Is8Bit()) {
    LChar* data8;
    scoped_refptr<StringImpl> new_impl =
        StringImpl::CreateUninitialized(source->length(), data8);

    // Do a faster loop for the case where all the characters are ASCII.
    LChar ored = 0;
    for (int i = 0; i < length; ++i) {
      LChar c = source->Characters8()[i];
      ored |= c;
      data8[i] = ToASCIIUpper(c);
    }
    if (!(ored & ~0x7F))
      return new_impl;

    // Do a slower implementation for cases that include non-ASCII Latin-1
    // characters.
    int number_sharp_s_characters = 0;

    // There are two special cases.
    //  1. latin-1 characters when converted to upper case are 16 bit
    //     characters.
    //  2. Lower case sharp-S converts to "SS" (two characters)
    for (int32_t i = 0; i < length; ++i) {
      LChar c = source->Characters8()[i];
      if (c == kSmallLetterSharpSCharacter) [[unlikely]] {
        ++number_sharp_s_characters;
      }
      UChar upper = static_cast<UChar>(unicode::ToUpper(c));
      if (upper > 0xff) [[unlikely]] {
        // Since this upper-cased character does not fit in an 8-bit string, we
        // need to take the 16-bit path.
        goto upconvert;
      }
      data8[i] = static_cast<LChar>(upper);
    }

    if (!number_sharp_s_characters)
      return new_impl;

    // We have numberSSCharacters sharp-s characters, but none of the other
    // special characters.
    new_impl = StringImpl::CreateUninitialized(
        source->length() + number_sharp_s_characters, data8);

    LChar* dest = data8;

    for (int32_t i = 0; i < length; ++i) {
      LChar c = source->Characters8()[i];
      if (c == kSmallLetterSharpSCharacter) {
        *dest++ = 'S';
        *dest++ = 'S';
        if (offset_map)
          offset_map->Append(i + 1, dest - data8);
      } else {
        *dest++ = static_cast<LChar>(unicode::ToUpper(c));
      }
    }

    return new_impl;
  }

upconvert:
  scoped_refptr<StringImpl> upconverted = source->UpconvertedString();
  const UChar* source16 = upconverted->Characters16();

  UChar* data16;
  scoped_refptr<StringImpl> new_impl =
      StringImpl::CreateUninitialized(source->length(), data16);

  // Do a faster loop for the case where all the characters are ASCII.
  UChar ored = 0;
  for (int i = 0; i < length; ++i) {
    UChar c = source16[i];
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

}  // namespace WTF
