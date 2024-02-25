// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/wtf/text/text_offset_map.h"

#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace WTF {

namespace {

// Returns a negative value:
//     The specified entry represents a removal of characters.
// Returns a positive value:
//     The specified entry represents an addition of characters.
// Returns zero:
//     The specified entry is redundant.
int ChunkLengthDifference(const Vector<TextOffsetMap::Entry>& entries,
                          wtf_size_t index) {
  wtf_size_t previous_source = 0;
  wtf_size_t previous_target = 0;
  if (index > 0) {
    previous_source = entries[index - 1].source;
    previous_target = entries[index - 1].target;
  }
  const TextOffsetMap::Entry& entry = entries[index];
  return (entry.target - previous_target) - (entry.source - previous_source);
}

}  // namespace

std::ostream& operator<<(std::ostream& stream,
                         const TextOffsetMap::Entry& entry) {
  return stream << "{" << entry.source << ", " << entry.target << "}";
}

std::ostream& operator<<(std::ostream& stream,
                         const Vector<TextOffsetMap::Entry>& entries) {
  stream << "{";
  for (wtf_size_t i = 0; i < entries.size(); ++i) {
    if (i > 0) {
      stream << ", ";
    }
    stream << entries[i];
  }
  return stream << "}";
}

TextOffsetMap::TextOffsetMap(const TextOffsetMap& map12,
                             const TextOffsetMap& map23) {
  if (map12.IsEmpty()) {
    entries_ = map23.entries_;
    return;
  }
  if (map23.IsEmpty()) {
    entries_ = map12.entries_;
    return;
  }

  const wtf_size_t size12 = map12.entries_.size();
  const wtf_size_t size23 = map23.entries_.size();
  wtf_size_t index12 = 0, index23 = 0;
  int offset_diff_12 = 0, offset_diff_23 = 0;
  while (index12 < size12 && index23 < size23) {
    const Entry& entry12 = map12.entries_[index12];
    const Entry& entry23 = map23.entries_[index23];
    int chunk_length_diff_12 = ChunkLengthDifference(map12.entries_, index12);
    int chunk_length_diff_23 = ChunkLengthDifference(map23.entries_, index23);
    if (chunk_length_diff_12 < 0 && chunk_length_diff_23 < 0 &&
        entry12.target + offset_diff_23 == entry23.target) {
      // No need to handle entry12 because it was overwritten by entry23.
      offset_diff_12 = entry12.target - entry12.source;
      ++index12;
    } else if (chunk_length_diff_12 > 0 && chunk_length_diff_23 > 0 &&
               entry12.source == entry23.source - offset_diff_12) {
      offset_diff_12 = entry12.target - entry12.source;
      offset_diff_23 = entry23.target - entry23.source;
      Append(entry12.source, entry23.target + chunk_length_diff_12);
      ++index12;
      ++index23;

    } else if (entry12.target < entry23.source) {
      Append(entry12.source, entry12.target + offset_diff_23);
      offset_diff_12 = entry12.target - entry12.source;
      ++index12;
    } else if (entry12.target == entry23.source) {
      Append(entry12.source, entry23.target);
      offset_diff_12 = entry12.target - entry12.source;
      offset_diff_23 = entry23.target - entry23.source;
      ++index12;
      ++index23;
    } else {
      Append(entry23.source - offset_diff_12, entry23.target);
      offset_diff_23 = entry23.target - entry23.source;
      ++index23;
    }
  }
  for (; index12 < size12; ++index12) {
    const Entry& entry12 = map12.entries_[index12];
    Append(entry12.source, entry12.target + offset_diff_23);
  }
  for (; index23 < size23; ++index23) {
    const Entry& entry23 = map23.entries_[index23];
    Append(entry23.source - offset_diff_12, entry23.target);
  }
}

void TextOffsetMap::Append(wtf_size_t source, wtf_size_t target) {
  DCHECK(IsEmpty() ||
         (source > entries_.back().source && target > entries_.back().target));
  entries_.emplace_back(source, target);
}

void TextOffsetMap::Append(const icu::Edits& edits) {
  DCHECK(IsEmpty());

  UErrorCode error = U_ZERO_ERROR;
  auto edit = edits.getFineChangesIterator();
  while (edit.next(error)) {
    if (!edit.hasChange() || edit.oldLength() == edit.newLength())
      continue;

    entries_.emplace_back(edit.sourceIndex() + edit.oldLength(),
                          edit.destinationIndex() + edit.newLength());
  }
  DCHECK(U_SUCCESS(error));
}

}  // namespace WTF
