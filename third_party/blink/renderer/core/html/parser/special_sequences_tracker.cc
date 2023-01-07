// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/parser/special_sequences_tracker.h"

#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {
namespace {

const LChar kCData[] = "<![CDATA[";
// Don't include the \0 in the count.
constexpr wtf_size_t kCDataLength =
    static_cast<wtf_size_t>(std::size(kCData) - 1);

}  // namespace

void SpecialSequencesTracker::UpdateIndices(const String& string) {
  if (special_sequence_index_ != kNoSpecialSequencesFound)
    return;

  UpdateIndexOfCDATA(string);
  special_sequence_index_ =
      std::min(IndexOfNullChar(string), special_sequence_index_);
  total_string_length_ += string.length();
}

unsigned SpecialSequencesTracker::IndexOfNullChar(const String& string) const {
  const wtf_size_t index = string.find(static_cast<UChar>('\0'));
  return index == kNotFound ? kNoSpecialSequencesFound
                            : (index + total_string_length_);
}

bool SpecialSequencesTracker::MatchPossibleCDataSection(
    const String& string,
    wtf_size_t start_string_index) {
  DCHECK_GT(num_matching_cdata_chars_, 0u);
  DCHECK_LT(start_string_index, kNotFound);
  const wtf_size_t string_length = string.length();
  wtf_size_t i = 0;
  // Iterate through `string` while it matches `kCData`. i starts at 0, but is
  // relative to `start_string_index`. `num_matching_cdata_chars_` is the
  // position into `kCData` to start the match from (portion of previous string
  // that matched).
  while (i + num_matching_cdata_chars_ < kCDataLength &&
         (i + start_string_index) < string_length &&
         string[i + start_string_index] ==
             kCData[i + num_matching_cdata_chars_]) {
    ++i;
  }
  if (i + num_matching_cdata_chars_ == kCDataLength) {
    // Matched all cdata.
    special_sequence_index_ =
        total_string_length_ + i + start_string_index - kCDataLength;
    return true;
  }
  if ((i + start_string_index) == string_length) {
    // This branch is hit in the case of matching all available data, but
    // more is required for a full match.
    num_matching_cdata_chars_ += i;
    return true;
  }
  return false;
}

void SpecialSequencesTracker::UpdateIndexOfCDATA(const String& string) {
  if (num_matching_cdata_chars_ != 0) {
    if (MatchPossibleCDataSection(string, 0))
      return;
    num_matching_cdata_chars_ = 0;
  }

  for (wtf_size_t next_possible_index = 0; next_possible_index != kNotFound;
       next_possible_index = string.find(kCData[0], next_possible_index)) {
    num_matching_cdata_chars_ = 1;
    if (MatchPossibleCDataSection(string, next_possible_index + 1))
      return;
    ++next_possible_index;
  }
  num_matching_cdata_chars_ = 0;
}

}  // namespace blink
