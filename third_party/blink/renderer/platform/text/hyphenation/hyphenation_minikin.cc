// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/platform/text/hyphenation/hyphenation_minikin.h"

#include <algorithm>
#include <utility>

#include "base/files/file.h"
#include "base/files/memory_mapped_file.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/timer/elapsed_timer.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/common/thread_safe_browser_interface_broker_proxy.h"
#include "third_party/blink/public/mojom/hyphenation/hyphenation.mojom-blink.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/platform/text/character.h"
#include "third_party/blink/renderer/platform/text/hyphenation/hyphenator_aosp.h"
#include "third_party/blink/renderer/platform/text/layout_locale.h"
#include "third_party/blink/renderer/platform/wtf/text/case_folding_hash.h"

namespace blink {

namespace {

inline bool ShouldSkipLeadingChar(UChar32 c) {
  if (Character::TreatAsSpace(c))
    return true;
  // Strip leading punctuation, defined as OP and QU line breaking classes,
  // see UAX #14.
  const int32_t lb = u_getIntPropertyValue(c, UCHAR_LINE_BREAK);
  if (lb == U_LB_OPEN_PUNCTUATION || lb == U_LB_QUOTATION)
    return true;
  return false;
}

inline bool ShouldSkipTrailingChar(UChar32 c) {
  // Strip trailing spaces, punctuation and control characters.
  const int32_t gc_mask = U_GET_GC_MASK(c);
  return gc_mask & (U_GC_ZS_MASK | U_GC_P_MASK | U_GC_CC_MASK);
}

}  // namespace

using Hyphenator = android::Hyphenator;

static mojo::Remote<mojom::blink::Hyphenation> ConnectToRemoteService() {
  mojo::Remote<mojom::blink::Hyphenation> service;
  Platform::Current()->GetBrowserInterfaceBroker()->GetInterface(
      service.BindNewPipeAndPassReceiver());
  return service;
}

static mojom::blink::Hyphenation* GetService() {
  DEFINE_STATIC_LOCAL(mojo::Remote<mojom::blink::Hyphenation>, service,
                      (ConnectToRemoteService()));
  return service.get();
}

bool HyphenationMinikin::OpenDictionary(const AtomicString& locale) {
  mojom::blink::Hyphenation* service = GetService();
  base::File file;
  base::ElapsedTimer timer;
  service->OpenDictionary(locale, &file);
  UMA_HISTOGRAM_TIMES("Hyphenation.Open", timer.Elapsed());

  return OpenDictionary(std::move(file));
}

bool HyphenationMinikin::OpenDictionary(base::File file) {
  if (!file.IsValid())
    return false;
  if (!file_.Initialize(std::move(file))) {
    DLOG(ERROR) << "mmap failed";
    return false;
  }

  hyphenator_ = base::WrapUnique(Hyphenator::loadBinary(file_.data()));

  return true;
}

StringView HyphenationMinikin::WordToHyphenate(
    const StringView& text,
    unsigned* num_leading_chars_out) {
  if (text.Is8Bit()) {
    const LChar* begin = text.Characters8();
    const LChar* end = begin + text.length();
    while (begin != end && ShouldSkipLeadingChar(*begin))
      ++begin;
    while (begin != end && ShouldSkipTrailingChar(end[-1]))
      --end;
    *num_leading_chars_out = static_cast<unsigned>(begin - text.Characters8());
    CHECK_GE(end, begin);
    return StringView(begin, static_cast<unsigned>(end - begin));
  }
  const UChar* begin = text.Characters16();
  int index = 0;
  int len = text.length();
  while (index < len) {
    int next_index = index;
    UChar32 c;
    U16_NEXT(begin, next_index, len, c);
    if (!ShouldSkipLeadingChar(c))
      break;
    index = next_index;
  }
  while (index < len) {
    int prev_len = len;
    UChar32 c;
    U16_PREV(begin, index, prev_len, c);
    if (!ShouldSkipTrailingChar(c))
      break;
    len = prev_len;
  }
  *num_leading_chars_out = index;
  CHECK_GE(len, index);
  return StringView(begin + index, len - index);
}

Vector<uint8_t> HyphenationMinikin::Hyphenate(const StringView& text) const {
  DCHECK(ShouldHyphenateWord(text));
  DCHECK_GE(text.length(), MinWordLength());
  Vector<uint8_t> result;
  if (text.Is8Bit()) {
    String text16_bit = text.ToString();
    text16_bit.Ensure16Bit();
    hyphenator_->hyphenate(
        &result, reinterpret_cast<const uint16_t*>(text16_bit.Characters16()),
        text16_bit.length());
  } else {
    hyphenator_->hyphenate(
        &result, reinterpret_cast<const uint16_t*>(text.Characters16()),
        text.length());
  }
  return result;
}

wtf_size_t HyphenationMinikin::LastHyphenLocation(
    const StringView& text,
    wtf_size_t before_index) const {
  unsigned num_leading_chars;
  const StringView word = WordToHyphenate(text, &num_leading_chars);
  if (before_index <= num_leading_chars || !ShouldHyphenateWord(word))
    return 0;
  DCHECK_GE(word.length(), MinWordLength());

  DCHECK_GT(word.length(), MinSuffixLength());
  before_index = std::min<wtf_size_t>(before_index - num_leading_chars,
                                      word.length() - MinSuffixLength() + 1);
  const wtf_size_t min_prefix_len = MinPrefixLength();
  if (before_index <= min_prefix_len)
    return 0;

  Vector<uint8_t> result = Hyphenate(word);
  CHECK_LE(before_index, result.size());
  CHECK_GE(before_index, 1u);
  DCHECK_GE(min_prefix_len, 1u);
  for (wtf_size_t i = before_index - 1; i >= min_prefix_len; i--) {
    if (result[i])
      return i + num_leading_chars;
  }
  return 0;
}

Vector<wtf_size_t, 8> HyphenationMinikin::HyphenLocations(
    const StringView& text) const {
  unsigned num_leading_chars;
  StringView word = WordToHyphenate(text, &num_leading_chars);

  Vector<wtf_size_t, 8> hyphen_locations;
  if (!ShouldHyphenateWord(word))
    return hyphen_locations;
  DCHECK_GE(word.length(), MinWordLength());

  Vector<uint8_t> result = Hyphenate(word);
  const wtf_size_t min_prefix_len = MinPrefixLength();
  DCHECK_GE(min_prefix_len, 1u);
  DCHECK_GT(word.length(), MinSuffixLength());
  for (wtf_size_t i = word.length() - MinSuffixLength(); i >= min_prefix_len;
       --i) {
    if (result[i])
      hyphen_locations.push_back(i + num_leading_chars);
  }
  return hyphen_locations;
}

struct HyphenatorLocaleData {
  const char* locale = nullptr;
  const char* locale_for_exact_match = nullptr;
};

using LocaleMap = HashMap<AtomicString,
                          const HyphenatorLocaleData*,
                          CaseFoldingHashTraits<AtomicString>>;

static LocaleMap CreateLocaleFallbackMap() {
  // This data is from CLDR, compiled by AOSP.
  // https://android.googlesource.com/platform/frameworks/base/+/master/core/jni/android_text_Hyphenator.cpp
  struct LocaleFallback {
    const char* locale;
    HyphenatorLocaleData data;
  };
  static LocaleFallback locale_fallback_data[] = {
      // English locales that fall back to en-US. The data is from CLDR. It's
      // all English locales,
      // minus the locales whose parent is en-001 (from supplementalData.xml,
      // under <parentLocales>).
      {"en-AS", {"en-us"}},  // English (American Samoa)
      {"en-GU", {"en-us"}},  // English (Guam)
      {"en-MH", {"en-us"}},  // English (Marshall Islands)
      {"en-MP", {"en-us"}},  // English (Northern Mariana Islands)
      {"en-PR", {"en-us"}},  // English (Puerto Rico)
      {"en-UM", {"en-us"}},  // English (United States Minor Outlying Islands)
      {"en-VI", {"en-us"}},  // English (Virgin Islands)
      // All English locales other than those falling back to en-US are mapped
      // to en-GB, except that "en" is mapped to "en-us" for interoperability
      // with other browsers.
      {"en", {"en-gb", "en-us"}},
      // For German, we're assuming the 1996 (and later) orthography by default.
      {"de", {"de-1996"}},
      // Liechtenstein uses the Swiss hyphenation rules for the 1901
      // orthography.
      {"de-LI-1901", {"de-ch-1901"}},
      // Norwegian is very probably Norwegian Bokmål.
      {"no", {"nb"}},
      // Use mn-Cyrl. According to CLDR's likelySubtags.xml, mn is most likely
      // to be mn-Cyrl.
      {"mn", {"mn-cyrl"}},  // Mongolian
      // Fall back to Ethiopic script for languages likely to be written in
      // Ethiopic.
      // Data is from CLDR's likelySubtags.xml.
      {"am", {"und-ethi"}},   // Amharic
      {"byn", {"und-ethi"}},  // Blin
      {"gez", {"und-ethi"}},  // Geʻez
      {"ti", {"und-ethi"}},   // Tigrinya
      {"wal", {"und-ethi"}},  // Wolaytta
      // Use Hindi as a fallback hyphenator for all languages written in
      // Devanagari, etc. This makes
      // sense because our Indic patterns are not really linguistic, but
      // script-based.
      {"und-Beng", {"bn"}},  // Bengali
      {"und-Deva", {"hi"}},  // Devanagari -> Hindi
      {"und-Gujr", {"gu"}},  // Gujarati
      {"und-Guru", {"pa"}},  // Gurmukhi -> Punjabi
      {"und-Knda", {"kn"}},  // Kannada
      {"und-Mlym", {"ml"}},  // Malayalam
      {"und-Orya", {"or"}},  // Oriya
      {"und-Taml", {"ta"}},  // Tamil
      {"und-Telu", {"te"}},  // Telugu

      // List of locales with hyphens not to fall back.
      {"de-1901", {"de-1901"}},
      {"de-1996", {"de-1996"}},
      {"de-ch-1901", {"de-ch-1901"}},
      {"en-gb", {"en-gb"}},
      {"en-us", {"en-us"}},
      {"mn-cyrl", {"mn-cyrl"}},
      {"und-ethi", {"und-ethi"}},
  };
  LocaleMap map;
  for (const auto& it : locale_fallback_data)
    map.insert(AtomicString(it.locale), &it.data);
  return map;
}

// static
AtomicString HyphenationMinikin::MapLocale(const AtomicString& locale) {
  DEFINE_STATIC_LOCAL(LocaleMap, locale_fallback, (CreateLocaleFallbackMap()));
  for (AtomicString mapped_locale = locale;;) {
    const auto& it = locale_fallback.find(mapped_locale);
    if (it != locale_fallback.end()) {
      if (it->value->locale_for_exact_match && locale == mapped_locale)
        return AtomicString(it->value->locale_for_exact_match);
      return AtomicString(it->value->locale);
    }
    const wtf_size_t last_hyphen = mapped_locale.ReverseFind('-');
    if (last_hyphen == kNotFound || !last_hyphen)
      return mapped_locale;
    mapped_locale = AtomicString(mapped_locale.GetString().Left(last_hyphen));
  }
}

scoped_refptr<Hyphenation> Hyphenation::PlatformGetHyphenation(
    const AtomicString& locale) {
  const AtomicString mapped_locale = HyphenationMinikin::MapLocale(locale);
  if (!EqualIgnoringASCIICase(mapped_locale, locale))
    return LayoutLocale::Get(mapped_locale)->GetHyphenation();

  scoped_refptr<HyphenationMinikin> hyphenation(
      base::AdoptRef(new HyphenationMinikin));
  const AtomicString lower_ascii_locale = locale.LowerASCII();
  if (!hyphenation->OpenDictionary(lower_ascii_locale))
    return nullptr;
  hyphenation->Initialize(lower_ascii_locale);
  return hyphenation;
}

scoped_refptr<HyphenationMinikin> HyphenationMinikin::FromFileForTesting(
    const AtomicString& locale,
    base::File file) {
  scoped_refptr<HyphenationMinikin> hyphenation(
      base::AdoptRef(new HyphenationMinikin));
  if (!hyphenation->OpenDictionary(std::move(file)))
    return nullptr;
  hyphenation->Initialize(locale);
  return hyphenation;
}

}  // namespace blink
