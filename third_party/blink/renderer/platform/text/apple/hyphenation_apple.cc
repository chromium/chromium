// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/text/hyphenation.h"

#include <CoreFoundation/CoreFoundation.h>

#include "base/apple/scoped_typeref.h"
#include "third_party/blink/renderer/platform/wtf/text/string_view.h"
#include "third_party/blink/renderer/platform/wtf/text/unicode.h"

namespace blink {

class HyphenationCF final : public Hyphenation {
 public:
  HyphenationCF(base::apple::ScopedCFTypeRef<CFLocaleRef>& locale_cf)
      : locale_cf_(locale_cf) {
    DCHECK(locale_cf_);
  }

  wtf_size_t LastHyphenLocation(const StringView& text,
                                wtf_size_t before_index) const override {
    if (!ShouldHyphenateWord(text)) {
      return 0;
    }
    DCHECK_GE(text.length(), MinWordLength());

    DCHECK_GT(text.length(), MinSuffixLength());
    before_index = std::min<wtf_size_t>(before_index,
                                        text.length() - MinSuffixLength() + 1);

    const CFIndex result = CFStringGetHyphenationLocationBeforeIndex(
        text.ToString().Impl()->CreateCFString().get(), before_index,
        CFRangeMake(0, text.length()), 0, locale_cf_.get(), 0);
    if (result == kCFNotFound) {
      return 0;
    }
    DCHECK_GE(result, 0);
    DCHECK_LT(result, before_index);
    if (result < MinPrefixLength()) {
      return 0;
    }
    return static_cast<wtf_size_t>(result);
  }

  // While Hyphenation::FirstHyphenLocation() works good, it computes all
  // locations and discards ones after |after_index|.
  // This version minimizes the computation for platforms that supports
  // LastHyphenLocation() but does not support HyphenLocations().
  wtf_size_t FirstHyphenLocation(const StringView& text,
                                 wtf_size_t after_index) const override {
    if (!ShouldHyphenateWord(text)) {
      return 0;
    }
    DCHECK_GE(text.length(), MinWordLength());

    DCHECK_GE(MinPrefixLength(), 1u);
    after_index =
        std::max(after_index, static_cast<wtf_size_t>(MinPrefixLength() - 1));

    const wtf_size_t word_len = text.length();
    DCHECK_GE(word_len, MinWordLength());
    DCHECK_GE(word_len, MinSuffixLength());
    const wtf_size_t max_hyphen_location = word_len - MinSuffixLength();
    wtf_size_t hyphen_location = max_hyphen_location + 1;
    for (;;) {
      wtf_size_t previous = LastHyphenLocation(text, hyphen_location);
      if (previous <= after_index) {
        break;
      }
      hyphen_location = previous;
    }
    return hyphen_location > max_hyphen_location ? 0 : hyphen_location;
  }

 private:
  base::apple::ScopedCFTypeRef<CFLocaleRef> locale_cf_;
};

scoped_refptr<Hyphenation> Hyphenation::PlatformGetHyphenation(
    const AtomicString& locale) {
  base::apple::ScopedCFTypeRef<CFStringRef> locale_cf_string(
      locale.Impl()->CreateCFString());
  base::apple::ScopedCFTypeRef<CFLocaleRef> locale_cf(
      CFLocaleCreate(kCFAllocatorDefault, locale_cf_string.get()));
  if (!CFStringIsHyphenationAvailableForLocale(locale_cf.get())) {
    return nullptr;
  }
  scoped_refptr<Hyphenation> hyphenation(
      base::AdoptRef(new HyphenationCF(locale_cf)));
  hyphenation->Initialize(locale);
  return hyphenation;
}

}  // namespace blink
