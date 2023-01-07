// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_PARSER_SPECIAL_SEQUENCES_TRACKER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_PARSER_SPECIAL_SEQUENCES_TRACKER_H_

#include <limits>

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/wtf/wtf_size_t.h"

namespace WTF {
class String;
}

namespace blink {

// Used to track sequences that can not be handled the
// BackgroundHTMLTokenProducer.
class CORE_EXPORT SpecialSequencesTracker {
 public:
  static constexpr unsigned kNoSpecialSequencesFound =
      std::numeric_limits<unsigned>::max();

  void UpdateIndices(const WTF::String& string);

  // Returns the index of the first sequence that can't be handled. Returns
  // the max unsized value if no special sequences have been encountered.
  unsigned index_of_first_special_sequence() const {
    return special_sequence_index_;
  }

 private:
  unsigned IndexOfNullChar(const WTF::String& string) const;

  // Returns true on success, or the section matches but the end of input is
  // reached (partial match).
  bool MatchPossibleCDataSection(const WTF::String& string,
                                 wtf_size_t start_string_index);

  void UpdateIndexOfCDATA(const WTF::String& string);

  // Length of all strings supplied to UpdateIndices() (until a special sequence
  // has been encountered).
  unsigned total_string_length_ = 0;

  // Index of the special sequence.
  unsigned special_sequence_index_ = std::numeric_limits<unsigned>::max();

  // If non-zero a partial match of CDATA has been encountered.
  wtf_size_t num_matching_cdata_chars_ = 0;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_PARSER_SPECIAL_SEQUENCES_TRACKER_H_
