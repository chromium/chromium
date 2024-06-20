// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "net/base/lookup_string_in_fixed_set.h"

#include <cstdint>

#include "base/check.h"
#include "base/containers/span.h"

namespace net {

namespace {

// Read next offset from `bytes`, increment `offset_bytes` by that amount, and
// increment `bytes` either to point to the start of the next encoded offset in
// its node, or set it to an empty span, if there are no remaining offsets.
//
// Returns true if an offset could be read; false otherwise.
inline bool GetNextOffset(base::span<const uint8_t>* bytes,
                          base::span<const uint8_t>* offset_bytes) {
  if (!bytes->size()) {
    return false;
  }

  size_t bytes_consumed;
  switch (bytes->front() & 0x60) {
    case 0x60:  // Read three byte offset
      *offset_bytes = offset_bytes->subspan(((bytes->front() & 0x1F) << 16) |
                                            ((*bytes)[1] << 8) | (*bytes)[2]);
      bytes_consumed = 3;
      break;
    case 0x40:  // Read two byte offset
      *offset_bytes =
          offset_bytes->subspan(((bytes->front() & 0x1F) << 8) | (*bytes)[1]);
      bytes_consumed = 2;
      break;
    default:
      *offset_bytes = offset_bytes->subspan(bytes->front() & 0x3F);
      bytes_consumed = 1;
  }
  if ((bytes->front() & 0x80) != 0) {
    *bytes = base::span<const uint8_t>();
  } else {
    *bytes = bytes->subspan(bytes_consumed);
  }
  return true;
}

// Check if byte at `byte` is last in label.
bool IsEOL(uint8_t byte) {
  return (byte & 0x80) != 0;
}

// Check if byte at `byte` matches key. This version matches both end-of-label
// chars and not-end-of-label chars.
bool IsMatch(uint8_t byte, char key) {
  return (byte & 0x7F) == key;
}

// Read return value at `byte`, if it is a return value. Returns true if a
// return value could be read, false otherwise.
bool GetReturnValue(uint8_t byte, int* return_value) {
  // Return values are always encoded as end-of-label chars (so the high bit is
  // set). So byte values in the inclusive range [0x80, 0x9F] encode the return
  // values 0 through 31 (though make_dafsa.py doesn't currently encode values
  // higher than 7). The following code does that translation.
  if ((byte & 0xE0) == 0x80) {
    *return_value = byte & 0x1F;
    return true;
  }
  return false;
}

}  // namespace

FixedSetIncrementalLookup::FixedSetIncrementalLookup(
    base::span<const uint8_t> graph)
    : bytes_(graph), original_bytes_(graph) {}

FixedSetIncrementalLookup::FixedSetIncrementalLookup(
    const FixedSetIncrementalLookup& other) = default;

FixedSetIncrementalLookup& FixedSetIncrementalLookup::operator=(
    const FixedSetIncrementalLookup& other) = default;

FixedSetIncrementalLookup::~FixedSetIncrementalLookup() = default;

bool FixedSetIncrementalLookup::Advance(char input) {
  if (bytes_.empty()) {
    // A previous input exhausted the graph, so there are no possible matches.
    return false;
  }

  // Only ASCII printable chars are supported by the current DAFSA format -- the
  // high bit (values 0x80-0xFF) is reserved as a label-end signifier, and the
  // low values (values 0x00-0x1F) are reserved to encode the return values. So
  // values outside this range will never be in the dictionary.
  if (input >= 0x20) {
    if (bytes_starts_with_label_character_) {
      // Currently processing a label, so it is only necessary to check the byte
      // pointed by `bytes_` to see if it encodes a character matching `input`.
      bool is_last_char_in_label = IsEOL(bytes_.front());
      bool is_match = IsMatch(bytes_.front(), input);
      if (is_match) {
        // If this is not the last character in the label, the next byte should
        // be interpreted as a character or return value. Otherwise, the next
        // byte should be interpreted as a list of child node offsets.
        bytes_ = bytes_.subspan(1);
        DCHECK(!bytes_.empty());
        bytes_starts_with_label_character_ = !is_last_char_in_label;
        return true;
      }
    } else {
      base::span<const uint8_t> offset_bytes = bytes_;
      // Read offsets from `bytes_` until the label of the child node at
      // `offset_bytes` matches `input`, or until there are no more offsets.
      while (GetNextOffset(&bytes_, &offset_bytes)) {
        DCHECK(!offset_bytes.empty());

        // `offset_bytes` points to a DAFSA node that is a child of the original
        // node.
        //
        // The low 7 bits of a node encodes a character value; the high bit
        // indicates whether it's the last character in the label.
        //
        // Note that `*offset_bytes` could also be a result code value, but
        // these are really just out-of-range ASCII values, encoded the same way
        // as characters. Since `input` was already validated as a printable
        // ASCII value, IsMatch will never return true if `offset_bytes` is a
        // result code.
        bool is_last_char_in_label = IsEOL(offset_bytes.front());
        bool is_match = IsMatch(offset_bytes.front(), input);

        if (is_match) {
          // If this is not the last character in the label, the next byte
          // should be interpreted as a character or return value. Otherwise,
          // the next byte should be interpreted as a list of child node
          // offsets.
          bytes_ = offset_bytes.subspan(1);
          DCHECK(!bytes_.empty());
          bytes_starts_with_label_character_ = !is_last_char_in_label;
          return true;
        }
      }
    }
  }

  // If no match was found, then end of the DAFSA has been reached.
  bytes_ = base::span<const uint8_t>();
  bytes_starts_with_label_character_ = false;
  return false;
}

int FixedSetIncrementalLookup::GetResultForCurrentSequence() const {
  int value = kDafsaNotFound;
  // Look to see if there is a next character that's a return value.
  if (bytes_starts_with_label_character_) {
    // Currently processing a label, so it is only necessary to check the byte
    // at `bytes_` to see if encodes a return value.
    GetReturnValue(bytes_.front(), &value);
  } else {
    // Otherwise, `bytes_` is an offset list. Explore the list of child nodes
    // (given by their offsets) to find one whose label is a result code.
    //
    // This search uses a temporary copy of `bytes_`, since mutating `bytes_`
    // could skip over a node that would be important to a subsequent Advance()
    // call.
    base::span<const uint8_t> temp_bytes = bytes_;

    // Read offsets from `temp_bytes` until either `temp_bytes` is exhausted or
    // until the byte at `offset_bytes` contains a result code (encoded as an
    // ASCII character below 0x20).
    base::span<const uint8_t> offset_bytes = bytes_;
    while (GetNextOffset(&temp_bytes, &offset_bytes)) {
      DCHECK(!offset_bytes.empty());
      if (GetReturnValue(offset_bytes.front(), &value)) {
        break;
      }
    }
  }
  return value;
}

int LookupStringInFixedSet(base::span<const uint8_t> graph,
                           const char* key,
                           size_t key_length) {
  // Do an incremental lookup until either the end of the graph is reached, or
  // until every character in |key| is consumed.
  FixedSetIncrementalLookup lookup(graph);
  const char* key_end = key + key_length;
  while (key != key_end) {
    if (!lookup.Advance(*key))
      return kDafsaNotFound;
    key++;
  }
  // The entire input was consumed without reaching the end of the graph. Return
  // the result code (if present) for the current position, or kDafsaNotFound.
  return lookup.GetResultForCurrentSequence();
}

// This function is only used by GetRegistryLengthInStrippedHost(), but is
// implemented here to allow inlining of
// LookupStringInFixedSet::GetResultForCurrentSequence() and
// LookupStringInFixedSet::Advance() at compile time. Tests on x86_64 linux
// indicated about 10% increased runtime cost for GetRegistryLength() in average
// if the implementation of this function was separated from the lookup methods.
int LookupSuffixInReversedSet(base::span<const uint8_t> graph,
                              bool include_private,
                              std::string_view host,
                              size_t* suffix_length) {
  FixedSetIncrementalLookup lookup(graph);
  *suffix_length = 0;
  int result = kDafsaNotFound;
  std::string_view::const_iterator pos = host.end();
  // Look up host from right to left.
  while (pos != host.begin() && lookup.Advance(*--pos)) {
    // Only host itself or a part that follows a dot can match.
    if (pos == host.begin() || *(pos - 1) == '.') {
      int value = lookup.GetResultForCurrentSequence();
      if (value != kDafsaNotFound) {
        // Break if private and private rules should be excluded.
        if ((value & kDafsaPrivateRule) && !include_private)
          break;
        // Save length and return value. Since hosts are looked up from right to
        // left, the last saved values will be from the longest match.
        *suffix_length = host.end() - pos;
        result = value;
      }
    }
  }
  return result;
}

}  // namespace net
