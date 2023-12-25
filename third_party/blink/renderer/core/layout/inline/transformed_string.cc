// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/inline/transformed_string.h"

#include "third_party/blink/renderer/platform/wtf/text/text_offset_map.h"

namespace blink {

// static
// Convert TextOffsetMap to a form we can split easily.
Vector<uint8_t> TransformedString::CreateLengthMap(
    unsigned dom_length,
    unsigned transformed_length,
    const TextOffsetMap& offset_map) {
  Vector<uint8_t> map;
  if (offset_map.IsEmpty()) {
    return map;
  }
  map.reserve(transformed_length);
  unsigned dom_offset = 0;
  unsigned transformed_offset = 0;
  for (const auto& entry : offset_map.Entries()) {
    unsigned dom_chunk_length = entry.source - dom_offset;
    unsigned transformed_chunk_length = entry.target - transformed_offset;
    if (dom_chunk_length < transformed_chunk_length) {
      unsigned i = 0;
      for (; i < dom_chunk_length; ++i) {
        map.push_back(1u);
      }
      for (; i < transformed_chunk_length; ++i) {
        map.push_back(0u);
      }
    } else if (dom_chunk_length > transformed_chunk_length) {
      CHECK_GE(transformed_chunk_length, 1u);
      for (unsigned i = 0; i < transformed_chunk_length - 1; ++i) {
        map.push_back(1u);
      }
      unsigned length = 1u + (dom_chunk_length - transformed_chunk_length);
      CHECK_LE(length, std::numeric_limits<uint8_t>::max());
      map.push_back(length);
    } else {
      for (unsigned i = 0; i < transformed_chunk_length; ++i) {
        map.push_back(1u);
      }
    }
    dom_offset = entry.source;
    transformed_offset = entry.target;
  }
  DCHECK_EQ(dom_length - dom_offset, transformed_length - transformed_offset);
  // TODO(layout-dev): We may drop this trailing '1' sequence to save memory.
  for (; transformed_offset < transformed_length; ++transformed_offset) {
    map.push_back(1u);
  }
  DCHECK_EQ(map.size(), transformed_length);
  return map;
}

TransformedString TransformedString::Substring(unsigned start,
                                               unsigned length) const {
  StringView sub_view = StringView(view_, start, length);
  if (length_map_.empty()) {
    return TransformedString(sub_view);
  }
  return TransformedString(sub_view, length_map_.subspan(start, length));
}

}  // namespace blink
