// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/text/hyphenation.h"

#include "base/containers/adapters.h"
#include "third_party/blink/renderer/platform/wtf/text/string_view.h"

namespace blink {

void Hyphenation::Initialize(const AtomicString& locale) {
  // TODO(crbug.com/1318385): How to control hyphenating capitalized words is
  // still under discussion. https://github.com/w3c/csswg-drafts/issues/5157
  hyphenate_capitalized_word_ = !locale.StartsWithIgnoringASCIICase("en");
}

void Hyphenation::SetLimits(wtf_size_t min_prefix_length,
                            wtf_size_t min_suffix_length,
                            wtf_size_t min_word_length) {
  if (min_prefix_length) {
    // If the `prefix` is given but the `suffix` is missing, the `suffix` is the
    // same as the `prefix`.
    min_prefix_length_ = min_prefix_length;
    min_suffix_length_ =
        min_suffix_length ? min_suffix_length : min_prefix_length;
  } else {
    min_prefix_length_ = kDefaultMinPrefixLength;
    min_suffix_length_ =
        min_suffix_length ? min_suffix_length : kDefaultMinSuffixLength;
  }
  min_word_length_ =
      std::max(min_word_length ? min_word_length : kDefaultMinWordLength,
               min_prefix_length_ + min_suffix_length_);
  DCHECK_GE(min_prefix_length_, 1u);
  DCHECK_GE(min_suffix_length_, 1u);
  DCHECK_GE(min_word_length_, min_prefix_length_ + min_suffix_length_);
}

wtf_size_t Hyphenation::FirstHyphenLocation(const StringView& text,
                                            wtf_size_t after_index) const {
  DCHECK_GE(MinPrefixLength(), 1u);
  after_index =
      std::max(after_index, static_cast<wtf_size_t>(MinPrefixLength() - 1));
  const Vector<wtf_size_t, 8> hyphen_locations = HyphenLocations(text);
  for (const wtf_size_t index : base::Reversed(hyphen_locations)) {
    if (index > after_index)
      return index;
  }
  return 0;
}

Vector<wtf_size_t, 8> Hyphenation::HyphenLocations(
    const StringView& text) const {
  Vector<wtf_size_t, 8> hyphen_locations;
  const wtf_size_t word_len = text.length();
  if (word_len < MinWordLength())
    return hyphen_locations;

  const wtf_size_t min_prefix_len = MinPrefixLength();
  DCHECK_GT(word_len, MinSuffixLength());
  wtf_size_t hyphen_location = word_len - MinSuffixLength() + 1;
  while ((hyphen_location = LastHyphenLocation(text, hyphen_location)) >=
         min_prefix_len) {
    hyphen_locations.push_back(hyphen_location);
  }

  return hyphen_locations;
}

}  // namespace blink
