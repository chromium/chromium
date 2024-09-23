// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/text/layout_locale.h"

#include <hb.h>
#include <unicode/locid.h>
#include <unicode/ulocdata.h>

#include <array>

#include "base/compiler_specific.h"
#include "base/memory/raw_ptr.h"
#include "base/notreached.h"
#include "third_party/blink/renderer/platform/language.h"
#include "third_party/blink/renderer/platform/text/hyphenation.h"
#include "third_party/blink/renderer/platform/text/icu_error.h"
#include "third_party/blink/renderer/platform/text/locale_to_script_mapping.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string_hash.h"
#include "third_party/blink/renderer/platform/wtf/text/case_folding_hash.h"
#include "third_party/blink/renderer/platform/wtf/thread_specific.h"

namespace blink {

namespace {

struct PerThreadData {
  HashMap<AtomicString,
          scoped_refptr<LayoutLocale>,
          CaseFoldingHashTraits<AtomicString>>
      locale_map;
  raw_ptr<const LayoutLocale> default_locale = nullptr;
  raw_ptr<const LayoutLocale> system_locale = nullptr;
  raw_ptr<const LayoutLocale> default_locale_for_han = nullptr;
  bool default_locale_for_han_computed = false;
  String current_accept_languages;
};

PerThreadData& GetPerThreadData() {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(ThreadSpecific<PerThreadData>, data, ());
  return *data;
}

struct DelimiterConfig {
  ULocaleDataDelimiterType type;
  raw_ptr<UChar> result;
};
// Use  ICU ulocdata to find quote delimiters for an ICU locale
// https://unicode-org.github.io/icu-docs/apidoc/dev/icu4c/ulocdata_8h.html#a0bf1fdd1a86918871ae2c84b5ce8421f
scoped_refptr<QuotesData> GetQuotesDataForLanguage(const char* locale) {
  UErrorCode status = U_ZERO_ERROR;
  // Expect returned buffer size is 1 to match QuotesData type
  constexpr int ucharDelimMaxLength = 1;

  ULocaleData* uld = ulocdata_open(locale, &status);
  if (U_FAILURE(status)) {
    ulocdata_close(uld);
    return nullptr;
  }
  std::array<UChar, ucharDelimMaxLength> open1, close1, open2, close2;

  int32_t delimResultLength;
  struct DelimiterConfig delimiters[] = {
      {ULOCDATA_QUOTATION_START, open1.data()},
      {ULOCDATA_QUOTATION_END, close1.data()},
      {ULOCDATA_ALT_QUOTATION_START, open2.data()},
      {ULOCDATA_ALT_QUOTATION_END, close2.data()},
  };
  for (DelimiterConfig delim : delimiters) {
    delimResultLength = ulocdata_getDelimiter(uld, delim.type, delim.result,
                                              ucharDelimMaxLength, &status);
    if (U_FAILURE(status) || delimResultLength != 1) {
      ulocdata_close(uld);
      return nullptr;
    }
  }
  ulocdata_close(uld);

  return QuotesData::Create(open1[0], close1[0], open2[0], close2[0]);
}

// Returns the Unicode Line Break Style Identifier (key "lb") value.
// https://www.unicode.org/reports/tr35/#UnicodeLineBreakStyleIdentifier
inline const char* LbValueFromStrictness(LineBreakStrictness strictness) {
  switch (strictness) {
    case LineBreakStrictness::kDefault:
      return nullptr;  // nullptr removes any existing values.
    case LineBreakStrictness::kNormal:
      return "normal";
    case LineBreakStrictness::kStrict:
      return "strict";
    case LineBreakStrictness::kLoose:
      return "loose";
  }
  NOTREACHED_IN_MIGRATION();
  return nullptr;
}

}  // namespace

static hb_language_t ToHarfbuzLanguage(const AtomicString& locale) {
  std::string locale_as_latin1 = locale.Latin1();
  return hb_language_from_string(locale_as_latin1.data(),
                                 static_cast<int>(locale_as_latin1.length()));
}

// SkFontMgr uses two/three-letter language code with an optional ISO 15924
// four-letter script code, in POSIX style (with '-' as the separator,) such as
// "zh-Hant" and "zh-Hans". See `fonts.xml`.
static const char* ToSkFontMgrLocale(UScriptCode script) {
  switch (script) {
    case USCRIPT_KATAKANA_OR_HIRAGANA:
      return "ja";
    case USCRIPT_HANGUL:
      return "ko";
    case USCRIPT_SIMPLIFIED_HAN:
      return "zh-Hans";
    case USCRIPT_TRADITIONAL_HAN:
      return "zh-Hant";
    default:
      return nullptr;
  }
}

const char* LayoutLocale::LocaleForSkFontMgr() const {
  if (!string_for_sk_font_mgr_.empty())
    return string_for_sk_font_mgr_.c_str();

  if (const char* sk_font_mgr_locale = ToSkFontMgrLocale(script_)) {
    string_for_sk_font_mgr_ = sk_font_mgr_locale;
    DCHECK(!string_for_sk_font_mgr_.empty());
    return string_for_sk_font_mgr_.c_str();
  }

  const icu::Locale locale(Ascii().c_str());
  const char* language = locale.getLanguage();
  string_for_sk_font_mgr_ = language && *language ? language : "und";
  const char* script = locale.getScript();
  if (script && *script)
    string_for_sk_font_mgr_ = string_for_sk_font_mgr_ + "-" + script;
  DCHECK(!string_for_sk_font_mgr_.empty());
  return string_for_sk_font_mgr_.c_str();
}

void LayoutLocale::ComputeScriptForHan() const {
  if (IsUnambiguousHanScript(script_)) {
    script_for_han_ = script_;
    has_script_for_han_ = true;
    return;
  }

  script_for_han_ = ScriptCodeForHanFromSubtags(string_);
  if (script_for_han_ == USCRIPT_COMMON)
    script_for_han_ = USCRIPT_SIMPLIFIED_HAN;
  else
    has_script_for_han_ = true;
  DCHECK(IsUnambiguousHanScript(script_for_han_));
}

UScriptCode LayoutLocale::GetScriptForHan() const {
  if (script_for_han_ == USCRIPT_COMMON)
    ComputeScriptForHan();
  return script_for_han_;
}

bool LayoutLocale::HasScriptForHan() const {
  if (script_for_han_ == USCRIPT_COMMON)
    ComputeScriptForHan();
  return has_script_for_han_;
}

// static
const LayoutLocale* LayoutLocale::LocaleForHan(
    const LayoutLocale* content_locale) {
  if (content_locale && content_locale->HasScriptForHan())
    return content_locale;

  PerThreadData& data = GetPerThreadData();
  if (!data.default_locale_for_han_computed) [[unlikely]] {
    // Use the first acceptLanguages that can disambiguate.
    Vector<String> languages;
    data.current_accept_languages.Split(',', languages);
    for (String token : languages) {
      token = token.StripWhiteSpace();
      const LayoutLocale* locale = LayoutLocale::Get(AtomicString(token));
      if (locale->HasScriptForHan()) {
        data.default_locale_for_han = locale;
        break;
      }
    }
    if (!data.default_locale_for_han) {
      const LayoutLocale& default_locale = GetDefault();
      if (default_locale.HasScriptForHan())
        data.default_locale_for_han = &default_locale;
    }
    if (!data.default_locale_for_han) {
      const LayoutLocale& system_locale = GetSystem();
      if (system_locale.HasScriptForHan())
        data.default_locale_for_han = &system_locale;
    }
    data.default_locale_for_han_computed = true;
  }
  return data.default_locale_for_han;
}

const char* LayoutLocale::LocaleForHanForSkFontMgr() const {
  const char* locale = ToSkFontMgrLocale(GetScriptForHan());
  DCHECK(locale);
  return locale;
}

void LayoutLocale::ComputeCaseMapLocale() const {
  DCHECK(!case_map_computed_);
  case_map_computed_ = true;
  locale_for_case_map_ = CaseMap::Locale(LocaleString());
}

LayoutLocale::LayoutLocale(const AtomicString& locale)
    : string_(locale),
      harfbuzz_language_(ToHarfbuzLanguage(locale)),
      script_(LocaleToScriptCodeForFontSelection(locale)),
      script_for_han_(USCRIPT_COMMON),
      has_script_for_han_(false),
      hyphenation_computed_(false),
      quotes_data_computed_(false),
      case_map_computed_(false) {}

// static
const LayoutLocale* LayoutLocale::Get(const AtomicString& locale) {
  if (locale.IsNull())
    return nullptr;

  auto result = GetPerThreadData().locale_map.insert(locale, nullptr);
  if (result.is_new_entry)
    result.stored_value->value = base::AdoptRef(new LayoutLocale(locale));
  return result.stored_value->value.get();
}

// static
const LayoutLocale& LayoutLocale::GetDefault() {
  PerThreadData& data = GetPerThreadData();
  if (!data.default_locale) [[unlikely]] {
    AtomicString language = DefaultLanguage();
    data.default_locale =
        LayoutLocale::Get(!language.empty() ? language : AtomicString("en"));
  }
  return *data.default_locale;
}

// static
const LayoutLocale& LayoutLocale::GetSystem() {
  PerThreadData& data = GetPerThreadData();
  if (!data.system_locale) [[unlikely]] {
    // Platforms such as Windows can give more information than the default
    // locale, such as "en-JP" for English speakers in Japan.
    String name = icu::Locale::getDefault().getName();
    data.system_locale =
        LayoutLocale::Get(AtomicString(name.Replace('_', '-')));
  }
  return *data.system_locale;
}

scoped_refptr<LayoutLocale> LayoutLocale::CreateForTesting(
    const AtomicString& locale) {
  return base::AdoptRef(new LayoutLocale(locale));
}

Hyphenation* LayoutLocale::GetHyphenation() const {
  if (hyphenation_computed_)
    return hyphenation_.get();

  hyphenation_computed_ = true;
  hyphenation_ = Hyphenation::PlatformGetHyphenation(LocaleString());
  return hyphenation_.get();
}

void LayoutLocale::SetHyphenationForTesting(
    const AtomicString& locale_string,
    scoped_refptr<Hyphenation> hyphenation) {
  const LayoutLocale& locale = ValueOrDefault(Get(locale_string));
  locale.hyphenation_computed_ = true;
  locale.hyphenation_ = std::move(hyphenation);
}

scoped_refptr<QuotesData> LayoutLocale::GetQuotesData() const {
  if (quotes_data_computed_)
    return quotes_data_;
  quotes_data_computed_ = true;

  // BCP 47 uses '-' as the delimiter but ICU uses '_'.
  // https://tools.ietf.org/html/bcp47
  String normalized_lang = LocaleString();
  normalized_lang.Replace('-', '_');

  UErrorCode status = U_ZERO_ERROR;
  // Use uloc_openAvailableByType() to find all CLDR recognized locales
  // https://unicode-org.github.io/icu-docs/apidoc/dev/icu4c/uloc_8h.html#aa0332857185774f3e0520a0823c14d16
  UEnumeration* ulocales =
      uloc_openAvailableByType(ULOC_AVAILABLE_DEFAULT, &status);
  if (U_FAILURE(status)) {
    uenum_close(ulocales);
    return nullptr;
  }

  // Try to find exact match
  while (const char* loc = uenum_next(ulocales, nullptr, &status)) {
    if (U_FAILURE(status)) {
      uenum_close(ulocales);
      return nullptr;
    }
    if (EqualIgnoringASCIICase(loc, normalized_lang)) {
      quotes_data_ = GetQuotesDataForLanguage(loc);
      uenum_close(ulocales);
      return quotes_data_;
    }
  }
  uenum_close(ulocales);

  // No exact match, try to find without subtags.
  wtf_size_t hyphen_offset = normalized_lang.ReverseFind('_');
  if (hyphen_offset == kNotFound)
    return nullptr;
  normalized_lang = normalized_lang.Substring(0, hyphen_offset);
  ulocales = uloc_openAvailableByType(ULOC_AVAILABLE_DEFAULT, &status);
  if (U_FAILURE(status)) {
    uenum_close(ulocales);
    return nullptr;
  }
  while (const char* loc = uenum_next(ulocales, nullptr, &status)) {
    if (U_FAILURE(status)) {
      uenum_close(ulocales);
      return nullptr;
    }
    if (EqualIgnoringASCIICase(loc, normalized_lang)) {
      quotes_data_ = GetQuotesDataForLanguage(loc);
      uenum_close(ulocales);
      return quotes_data_;
    }
  }
  uenum_close(ulocales);
  return nullptr;
}

AtomicString LayoutLocale::LocaleWithBreakKeyword(
    LineBreakStrictness strictness,
    bool use_phrase) const {
  if (string_.empty())
    return string_;

  // uloc_setKeywordValue_58 has a problem to handle "@" in the original
  // string. crbug.com/697859
  if (string_.Contains('@'))
    return string_;

  constexpr wtf_size_t kMaxLbValueLen = 6;
  constexpr wtf_size_t kMaxKeywordsLen =
      /* strlen("@lb=") */ 4 + kMaxLbValueLen + /* strlen("@lw=phrase") */ 10;
  class ULocaleKeywordBuilder {
   public:
    explicit ULocaleKeywordBuilder(const std::string& utf8_locale)
        : length_(base::saturated_cast<wtf_size_t>(utf8_locale.length())),
          buffer_(length_ + kMaxKeywordsLen + 1, 0) {
      // The `buffer_` is initialized to 0 above.
      memcpy(buffer_.data(), utf8_locale.c_str(), length_);
    }
    explicit ULocaleKeywordBuilder(const String& locale)
        : ULocaleKeywordBuilder(locale.Utf8()) {}

    AtomicString ToAtomicString() const {
      return AtomicString::FromUTF8(buffer_.data(), length_);
    }

    bool SetStrictness(LineBreakStrictness strictness) {
      const char* const lb_value = LbValueFromStrictness(strictness);
      DCHECK(!lb_value || strlen(lb_value) <= kMaxLbValueLen);
      return SetKeywordValue("lb", lb_value);
    }

    bool SetKeywordValue(const char* keyword_name, const char* value) {
      ICUError status;
      int32_t length_needed = uloc_setKeywordValue(
          keyword_name, value, buffer_.data(), buffer_.size(), &status);
      if (U_SUCCESS(status)) {
        DCHECK_GE(length_needed, 0);
        length_ = length_needed;
        DCHECK_LT(length_, buffer_.size());
        return true;
      }
      DCHECK_NE(status, U_BUFFER_OVERFLOW_ERROR);
      return false;
    }

   private:
    wtf_size_t length_;
    Vector<char> buffer_;
  } builder(string_);

  if (builder.SetStrictness(strictness) &&
      (!use_phrase || builder.SetKeywordValue("lw", "phrase"))) {
    return builder.ToAtomicString();
  }
  NOTREACHED_IN_MIGRATION();
  return string_;
}

// static
void LayoutLocale::AcceptLanguagesChanged(const String& accept_languages) {
  PerThreadData& data = GetPerThreadData();
  if (data.current_accept_languages == accept_languages)
    return;

  data.current_accept_languages = accept_languages;
  data.default_locale_for_han = nullptr;
  data.default_locale_for_han_computed = false;
}

// static
void LayoutLocale::ClearForTesting() {
  GetPerThreadData() = PerThreadData();
}

}  // namespace blink
