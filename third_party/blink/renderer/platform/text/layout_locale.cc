// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/text/layout_locale.h"

#include "base/compiler_specific.h"
#include "third_party/blink/renderer/platform/language.h"
#include "third_party/blink/renderer/platform/text/hyphenation.h"
#include "third_party/blink/renderer/platform/text/icu_error.h"
#include "third_party/blink/renderer/platform/text/locale_to_script_mapping.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string_hash.h"
#include "third_party/blink/renderer/platform/wtf/text/string_hash.h"
#include "third_party/blink/renderer/platform/wtf/thread_specific.h"

#include <hb.h>
#include <unicode/locid.h>

namespace blink {

namespace {

struct PerThreadData {
  HashMap<AtomicString, scoped_refptr<LayoutLocale>, CaseFoldingHash>
      locale_map;
  const LayoutLocale* default_locale = nullptr;
  const LayoutLocale* system_locale = nullptr;
  const LayoutLocale* default_locale_for_han = nullptr;
  bool default_locale_for_han_computed = false;
  String current_accept_languages;
};

PerThreadData& GetPerThreadData() {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(ThreadSpecific<PerThreadData>, data, ());
  return *data;
}

}  // namespace

static hb_language_t ToHarfbuzLanguage(const AtomicString& locale) {
  std::string locale_as_latin1 = locale.Latin1();
  return hb_language_from_string(locale_as_latin1.data(),
                                 locale_as_latin1.length());
}

// SkFontMgr requires script-based locale names, like "zh-Hant" and "zh-Hans",
// instead of "zh-CN" and "zh-TW".
static const char* ToSkFontMgrLocale(UScriptCode script) {
  switch (script) {
    case USCRIPT_KATAKANA_OR_HIRAGANA:
      return "ja-JP";
    case USCRIPT_HANGUL:
      return "ko-KR";
    case USCRIPT_SIMPLIFIED_HAN:
      return "zh-Hans";
    case USCRIPT_TRADITIONAL_HAN:
      return "zh-Hant";
    default:
      return nullptr;
  }
}

const char* LayoutLocale::LocaleForSkFontMgr() const {
  if (string_for_sk_font_mgr_.empty()) {
    const char* sk_font_mgr_locale = ToSkFontMgrLocale(script_);
    string_for_sk_font_mgr_ =
        sk_font_mgr_locale ? sk_font_mgr_locale : std::string();
    if (string_for_sk_font_mgr_.empty())
      string_for_sk_font_mgr_ = string_.Ascii();
  }
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
  if (UNLIKELY(!data.default_locale_for_han_computed)) {
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
  if (UNLIKELY(!data.default_locale)) {
    AtomicString language = DefaultLanguage();
    data.default_locale =
        LayoutLocale::Get(!language.IsEmpty() ? language : "en");
  }
  return *data.default_locale;
}

// static
const LayoutLocale& LayoutLocale::GetSystem() {
  PerThreadData& data = GetPerThreadData();
  if (UNLIKELY(!data.system_locale)) {
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

AtomicString LayoutLocale::LocaleWithBreakKeyword(
    LineBreakIteratorMode mode) const {
  if (string_.IsEmpty())
    return string_;

  // uloc_setKeywordValue_58 has a problem to handle "@" in the original
  // string. crbug.com/697859
  if (string_.Contains('@'))
    return string_;

  std::string utf8_locale = string_.Utf8();
  Vector<char> buffer(utf8_locale.length() + 11, 0);
  memcpy(buffer.data(), utf8_locale.c_str(), utf8_locale.length());

  const char* keyword_value = nullptr;
  switch (mode) {
    default:
      NOTREACHED();
      FALLTHROUGH;
    case LineBreakIteratorMode::kDefault:
      // nullptr will cause any existing values to be removed.
      break;
    case LineBreakIteratorMode::kNormal:
      keyword_value = "normal";
      break;
    case LineBreakIteratorMode::kStrict:
      keyword_value = "strict";
      break;
    case LineBreakIteratorMode::kLoose:
      keyword_value = "loose";
      break;
  }

  ICUError status;
  int32_t length_needed = uloc_setKeywordValue(
      "lb", keyword_value, buffer.data(), buffer.size(), &status);
  if (U_SUCCESS(status))
    return AtomicString::FromUTF8(buffer.data(), length_needed);

  if (status == U_BUFFER_OVERFLOW_ERROR) {
    buffer.Grow(length_needed + 1);
    memset(buffer.data() + utf8_locale.length(), 0,
           buffer.size() - utf8_locale.length());
    status = U_ZERO_ERROR;
    int32_t length_needed2 = uloc_setKeywordValue(
        "lb", keyword_value, buffer.data(), buffer.size(), &status);
    DCHECK_EQ(length_needed, length_needed2);
    if (U_SUCCESS(status) && length_needed == length_needed2)
      return AtomicString::FromUTF8(buffer.data(), length_needed);
  }

  NOTREACHED();
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
