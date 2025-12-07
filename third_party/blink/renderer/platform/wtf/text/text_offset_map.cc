// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/wtf/text/text_offset_map.h"

#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

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

TextOffsetMap::TextOffsetMap(wtf_size_t length1,
                             const TextOffsetMap& map12,
                             wtf_size_t length2,
                             const TextOffsetMap& map23,
                             wtf_size_t length3) {
  if (map12.IsEmpty()) {
    entries_ = map23.entries_;
    return;
  }
  if (map23.IsEmpty()) {
    entries_ = map12.entries_;
    return;
  }

  Vector<Length> length_map12 = map12.CreateLengthMap(length1, length2);
  Vector<Length> length_map = map23.CreateLengthMap(length2, length3);
  wtf_size_t index12 = 0;
  for (wtf_size_t index23 = 0; index23 < length_map.size(); ++index23) {
    Length length = length_map[index23];
    Length sum = 0;
    bool is_starting_at_middle = length > 0 && length_map12[index12] == 0;
    for (wtf_size_t i = 0; i < length; ++i) {
      sum += length_map12[index12++];
    }
    length_map[index23] = sum;
    if (is_starting_at_middle) {
      length_map[index23] = 0;
      length_map[index23 - 1] += sum;
    }
  }

  wtf_size_t source_index = 0;
  for (wtf_size_t dest_index = 0; dest_index < length_map.size();
       ++dest_index) {
    wtf_size_t source_length = length_map[dest_index];
    source_index += source_length;
    wtf_size_t dest_length = 1;
    if (source_length == 1) {
      while (dest_index + 1 < length_map.size() &&
             length_map[dest_index + 1] == 0u) {
        ++dest_index;
        ++dest_length;
      }
    }
    if (source_length != dest_length) {
      Append(source_index, dest_index + 1);
    }
  }
}

void TextOffsetMap::Append(wtf_size_t source, wtf_size_t target) {
  CHECK(IsEmpty() ||
        (source >= entries_.back().source && target >= entries_.back().target));
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

// Convert this TextOffsetMap to a form we can split easily.
Vector<TextOffsetMap::Length> TextOffsetMap::CreateLengthMap(
    wtf_size_t old_length,
    wtf_size_t new_length) const {
  Vector<Length> map;
  if (IsEmpty()) {
    return map;
  }
  map.reserve(new_length);
  unsigned old_offset = 0;
  unsigned new_offset = 0;
  for (const auto& entry : Entries()) {
    unsigned old_chunk_length = entry.source - old_offset;
    unsigned new_chunk_length = entry.target - new_offset;
    if (old_chunk_length < new_chunk_length) {
      unsigned i = 0;
      for (; i < old_chunk_length; ++i) {
        map.push_back(1u);
      }
      for (; i < new_chunk_length; ++i) {
        map.push_back(0u);
      }
    } else if (old_chunk_length > new_chunk_length) {
      CHECK_GE(new_chunk_length, 1u);
      for (unsigned i = 0; i < new_chunk_length - 1; ++i) {
        map.push_back(1u);
      }
      unsigned length = 1u + (old_chunk_length - new_chunk_length);
      map.push_back(length);
    } else {
      for (unsigned i = 0; i < new_chunk_length; ++i) {
        map.push_back(1u);
      }
    }
    old_offset = entry.source;
    new_offset = entry.target;
  }
  DCHECK_EQ(old_length - old_offset, new_length - new_offset);
  // TODO(layout-dev): We may drop this trailing '1' sequence to save memory.
  for (; new_offset < new_length; ++new_offset) {
    map.push_back(1u);
  }
  DCHECK_EQ(map.size(), new_length);
  return map;
}

}  // namespace blink
