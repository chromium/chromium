// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/text/hyphenation/hyphenation_minikin.h"

#include <algorithm>
#include <utility>

#include "base/files/file.h"
#include "base/files/memory_mapped_file.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/mojom/hyphenation/hyphenation.mojom-blink.h"
#include "third_party/blink/public/platform/interface_provider.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/platform/text/character.h"
#include "third_party/blink/renderer/platform/text/hyphenation/hyphenator_aosp.h"
#include "third_party/blink/renderer/platform/text/layout_locale.h"

namespace blink {

namespace {

template <typename CharType>
StringView SkipLeadingSpaces(const CharType* text,
                             unsigned length,
                             unsigned* num_leading_spaces_out) {
  const CharType* begin = text;
  const CharType* end = text + length;
  while (text != end && Character::TreatAsSpace(*text))
    text++;
  *num_leading_spaces_out = text - begin;
  return StringView(text, static_cast<unsigned>(end - text));
}

StringView SkipLeadingSpaces(const StringView& text,
                             unsigned* num_leading_spaces_out) {
  if (text.Is8Bit()) {
    return SkipLeadingSpaces(text.Characters8(), text.length(),
                             num_leading_spaces_out);
  }
  return SkipLeadingSpaces(text.Characters16(), text.length(),
                           num_leading_spaces_out);
}

}  // namespace

using Hyphenator = android::Hyphenator;

static mojo::Remote<mojom::blink::Hyphenation> ConnectToRemoteService() {
  mojo::Remote<mojom::blink::Hyphenation> service;
  Platform::Current()->GetInterfaceProvider()->GetInterface(
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
  service->OpenDictionary(locale, &file);
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

Vector<uint8_t> HyphenationMinikin::Hyphenate(const StringView& text) const {
  Vector<uint8_t> result;
  if (text.Is8Bit()) {
    String text16_bit = text.ToString();
    text16_bit.Ensure16Bit();
    hyphenator_->hyphenate(&result, text16_bit.Characters16(),
                           text16_bit.length());
  } else {
    hyphenator_->hyphenate(&result, text.Characters16(), text.length());
  }
  return result;
}

wtf_size_t HyphenationMinikin::LastHyphenLocation(
    const StringView& text,
    wtf_size_t before_index) const {
  unsigned num_leading_spaces;
  StringView word = SkipLeadingSpaces(text, &num_leading_spaces);
  if (before_index <= num_leading_spaces)
    return 0;
  before_index = std::min<wtf_size_t>(before_index - num_leading_spaces,
                                      word.length() - kMinimumSuffixLength);

  if (word.length() < kMinimumPrefixLength + kMinimumSuffixLength ||
      before_index <= kMinimumPrefixLength)
    return 0;

  Vector<uint8_t> result = Hyphenate(word);
  CHECK_LE(before_index, result.size());
  CHECK_GE(before_index, 1u);
  static_assert(kMinimumPrefixLength >= 1, "|beforeIndex - 1| can underflow");
  for (wtf_size_t i = before_index - 1; i >= kMinimumPrefixLength; i--) {
    if (result[i])
      return i + num_leading_spaces;
  }
  return 0;
}

Vector<wtf_size_t, 8> HyphenationMinikin::HyphenLocations(
    const StringView& text) const {
  unsigned num_leading_spaces;
  StringView word = SkipLeadingSpaces(text, &num_leading_spaces);

  Vector<wtf_size_t, 8> hyphen_locations;
  if (word.length() < kMinimumPrefixLength + kMinimumSuffixLength)
    return hyphen_locations;

  Vector<uint8_t> result = Hyphenate(word);
  static_assert(kMinimumPrefixLength >= 1,
                "Change the 'if' above if this fails");
  for (wtf_size_t i = word.length() - kMinimumSuffixLength - 1;
       i >= kMinimumPrefixLength; i--) {
    if (result[i])
      hyphen_locations.push_back(i + num_leading_spaces);
  }
  return hyphen_locations;
}

using LocaleMap = HashMap<AtomicString, AtomicString, CaseFoldingHash>;

static LocaleMap CreateLocaleFallbackMap() {
  // This data is from CLDR, compiled by AOSP.
  // https://android.googlesource.com/platform/frameworks/base/+/master/core/java/android/text/Hyphenator.java
  using LocaleFallback = const char * [2];
  static LocaleFallback locale_fallback_data[] = {
      {"en-AS", "en-us"},  // English (American Samoa)
      {"en-GU", "en-us"},  // English (Guam)
      {"en-MH", "en-us"},  // English (Marshall Islands)
      {"en-MP", "en-us"},  // English (Northern Mariana Islands)
      {"en-PR", "en-us"},  // English (Puerto Rico)
      {"en-UM", "en-us"},  // English (United States Minor Outlying Islands)
      {"en-VI", "en-us"},  // English (Virgin Islands)
      // All English locales other than those falling back to en-US are mapped
      // to en-GB.
      {"en", "en-gb"},
      // For German, we're assuming the 1996 (and later) orthography by default.
      {"de", "de-1996"},
      // Liechtenstein uses the Swiss hyphenation rules for the 1901
      // orthography.
      {"de-LI-1901", "de-ch-1901"},
      // Norwegian is very probably Norwegian Bokmål.
      {"no", "nb"},
      {"mn", "mn-cyrl"},    // Mongolian
      {"am", "und-ethi"},   // Amharic
      {"byn", "und-ethi"},  // Blin
      {"gez", "und-ethi"},  // Geʻez
      {"ti", "und-ethi"},   // Tigrinya
      {"wal", "und-ethi"},  // Wolaytta
  };
  LocaleMap map;
  for (const auto& it : locale_fallback_data)
    map.insert(AtomicString(it[0]), it[1]);
  return map;
}

scoped_refptr<Hyphenation> Hyphenation::PlatformGetHyphenation(
    const AtomicString& locale) {
  scoped_refptr<HyphenationMinikin> hyphenation(
      base::AdoptRef(new HyphenationMinikin));
  if (hyphenation->OpenDictionary(locale.LowerASCII()))
    return hyphenation;
  hyphenation = nullptr;

  DEFINE_STATIC_LOCAL(LocaleMap, locale_fallback, (CreateLocaleFallbackMap()));
  const auto& it = locale_fallback.find(locale);
  if (it != locale_fallback.end())
    return LayoutLocale::Get(it->value)->GetHyphenation();

  return nullptr;
}

scoped_refptr<HyphenationMinikin> HyphenationMinikin::FromFileForTesting(
    base::File file) {
  scoped_refptr<HyphenationMinikin> hyphenation(
      base::AdoptRef(new HyphenationMinikin));
  if (hyphenation->OpenDictionary(std::move(file)))
    return hyphenation;
  return nullptr;
}

}  // namespace blink
