// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/text/hyphenation.h"

#include <CoreFoundation/CoreFoundation.h>
#include "base/mac/scoped_typeref.h"
#include "third_party/blink/renderer/platform/wtf/text/string_view.h"

namespace blink {

class HyphenationCF final : public Hyphenation {
 public:
  HyphenationCF(base::ScopedCFTypeRef<CFLocaleRef>& locale_cf)
      : locale_cf_(locale_cf) {
    DCHECK(locale_cf_);
  }

  wtf_size_t LastHyphenLocation(const StringView& text,
                                wtf_size_t before_index) const override {
    CFIndex result = CFStringGetHyphenationLocationBeforeIndex(
        text.ToString().Impl()->CreateCFString(), before_index,
        CFRangeMake(0, text.length()), 0, locale_cf_, 0);
    return result == kCFNotFound ? 0 : result;
  }

  // While Hyphenation::FirstHyphenLocation() works good, it computes all
  // locations and discards ones after |after_index|.
  // This version minimizes the computation for platforms that supports
  // LastHyphenLocation() but does not support HyphenLocations().
  wtf_size_t FirstHyphenLocation(const StringView& text,
                                 wtf_size_t after_index) const override {
    after_index = std::max(after_index,
                           static_cast<wtf_size_t>(kMinimumPrefixLength - 1));
    wtf_size_t hyphen_location = text.length();
    if (hyphen_location <= kMinimumSuffixLength)
      return 0;
    wtf_size_t max_hyphen_location = hyphen_location - kMinimumSuffixLength;
    hyphen_location = max_hyphen_location;
    for (;;) {
      wtf_size_t previous = LastHyphenLocation(text, hyphen_location);
      if (previous <= after_index)
        break;
      hyphen_location = previous;
    }
    return hyphen_location >= max_hyphen_location ? 0 : hyphen_location;
  }

 private:
  base::ScopedCFTypeRef<CFLocaleRef> locale_cf_;
};

scoped_refptr<Hyphenation> Hyphenation::PlatformGetHyphenation(
    const AtomicString& locale) {
  base::ScopedCFTypeRef<CFStringRef> locale_cf_string(
      locale.Impl()->CreateCFString());
  base::ScopedCFTypeRef<CFLocaleRef> locale_cf(
      CFLocaleCreate(kCFAllocatorDefault, locale_cf_string));
  if (!CFStringIsHyphenationAvailableForLocale(locale_cf))
    return nullptr;
  return base::AdoptRef(new HyphenationCF(locale_cf));
}

}  // namespace blink
