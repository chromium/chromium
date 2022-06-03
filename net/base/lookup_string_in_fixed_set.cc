// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/lookup_string_in_fixed_set.h"

#include "base/check.h"

namespace net {

namespace {

// Read next offset from |pos|, increment |offset| by that amount, and increment
// |pos| either to point to the start of the next encoded offset in its node, or
// nullptr, if there are no remaining offsets.
//
// Returns true if an offset could be read; false otherwise.
inline bool GetNextOffset(const unsigned char** pos,
                          const unsigned char** offset) {
  if (*pos == nullptr)
    return false;

  size_t bytes_consumed;
  switch (**pos & 0x60) {
    case 0x60:  // Read three byte offset
      *offset += (((*pos)[0] & 0x1F) << 16) | ((*pos)[1] << 8) | (*pos)[2];
      bytes_consumed = 3;
      break;
    case 0x40:  // Read two byte offset
      *offset += (((*pos)[0] & 0x1F) << 8) | (*pos)[1];
      bytes_consumed = 2;
      break;
    default:
      *offset += (*pos)[0] & 0x3F;
      bytes_consumed = 1;
  }
  if ((**pos & 0x80) != 0) {
    *pos = nullptr;
  } else {
    *pos += bytes_consumed;
  }
  return true;
}

// Check if byte at |offset| is last in label.
bool IsEOL(const unsigned char* offset) {
  return (*offset & 0x80) != 0;
}

// Check if byte at |offset| matches key. This version matches both end-of-label
// chars and not-end-of-label chars.
bool IsMatch(const unsigned char* offset, char key) {
  return (*offset & 0x7F) == key;
}

// Read return value at |offset|, if it is a return value. Returns true if a
// return value could be read, false otherwise.
bool GetReturnValue(const unsigned char* offset, int* return_value) {
  // Return values are always encoded as end-of-label chars (so the high bit is
  // set). So byte values in the inclusive range [0x80, 0x9F] encode the return
  // values 0 through 31 (though make_dafsa.py doesn't currently encode values
  // higher than 7). The following code does that translation.
  if ((*offset & 0xE0) == 0x80) {
    *return_value = *offset & 0x1F;
    return true;
  }
  return false;
}

}  // namespace

FixedSetIncrementalLookup::FixedSetIncrementalLookup(const unsigned char* graph,
                                                     size_t length)
    : pos_(graph), end_(graph + length), pos_is_label_character_(false) {}

FixedSetIncrementalLookup::FixedSetIncrementalLookup(
    const FixedSetIncrementalLookup& other) = default;

FixedSetIncrementalLookup& FixedSetIncrementalLookup::operator=(
    const FixedSetIncrementalLookup& other) = default;

FixedSetIncrementalLookup::~FixedSetIncrementalLookup() = default;

bool FixedSetIncrementalLookup::Advance(char input) {
  if (!pos_) {
    // A previous input exhausted the graph, so there are no possible matches.
    return false;
  }

  // Only ASCII printable chars are supported by the current DAFSA format -- the
  // high bit (values 0x80-0xFF) is reserved as a label-end signifier, and the
  // low values (values 0x00-0x1F) are reserved to encode the return values. So
  // values outside this range will never be in the dictionary.
  if (input >= 0x20) {
    if (pos_is_label_character_) {
      // Currently processing a label, so it is only necessary to check the byte
      // at |pos_| to see if it encodes a character matching |input|.
      bool is_last_char_in_label = IsEOL(pos_);
      bool is_match = IsMatch(pos_, input);
      if (is_match) {
        // If this is not the last character in the label, the next byte should
        // be interpreted as a character or return value. Otherwise, the next
        // byte should be interpreted as a list of child node offsets.
        ++pos_;
        DCHECK(pos_ < end_);
        pos_is_label_character_ = !is_last_char_in_label;
        return true;
      }
    } else {
      const unsigned char* offset = pos_;
      // Read offsets from |pos_| until the label of the child node at |offset|
      // matches |input|, or until there are no more offsets.
      while (GetNextOffset(&pos_, &offset)) {
        DCHECK(offset < end_);
        DCHECK((pos_ == nullptr) || (pos_ < end_));

        // |offset| points to a DAFSA node that is a child of the original node.
        //
        // The low 7 bits of a node encodes a character value; the high bit
        // indicates whether it's the last character in the label.
        //
        // Note that |*offset| could also be a result code value, but these are
        // really just out-of-range ASCII values, encoded the same way as
        // characters. Since |input| was already validated as a printable ASCII
        // value ASCII value, IsMatch will never return true if |offset| is a
        // result code.
        bool is_last_char_in_label = IsEOL(offset);
        bool is_match = IsMatch(offset, input);

        if (is_match) {
          // If this is not the last character in the label, the next byte
          // should be interpreted as a character or return value. Otherwise,
          // the next byte should be interpreted as a list of child node
          // offsets.
          pos_ = offset + 1;
          DCHECK(pos_ < end_);
          pos_is_label_character_ = !is_last_char_in_label;
          return true;
        }
      }
    }
  }

  // If no match was found, then end of the DAFSA has been reached.
  pos_ = nullptr;
  pos_is_label_character_ = false;
  return false;
}

int FixedSetIncrementalLookup::GetResultForCurrentSequence() const {
  int value = kDafsaNotFound;
  // Look to see if there is a next character that's a return value.
  if (pos_is_label_character_) {
    // Currently processing a label, so it is only necessary to check the byte
    // at |pos_| to see if encodes a return value.
    GetReturnValue(pos_, &value);
  } else {
    // Otherwise, |pos_| is an offset list (or nullptr). Explore the list of
    // child nodes (given by their offsets) to find one whose label is a result
    // code.
    //
    // This search uses a temporary copy of |pos_|, since mutating |pos_| could
    // skip over a node that would be important to a subsequent Advance() call.
    const unsigned char* temp_pos = pos_;

    // Read offsets from |temp_pos| until either |temp_pos| is nullptr or until
    // the byte at |offset| contains a result code (encoded as an ASCII
    // character below 0x20).
    const unsigned char* offset = pos_;
    while (GetNextOffset(&temp_pos, &offset)) {
      DCHECK(offset < end_);
      DCHECK((temp_pos == nullptr) || temp_pos < end_);
      if (GetReturnValue(offset, &value))
        break;
    }
  }
  return value;
}

int LookupStringInFixedSet(const unsigned char* graph,
                           size_t length,
                           const char* key,
                           size_t key_length) {
  // Do an incremental lookup until either the end of the graph is reached, or
  // until every character in |key| is consumed.
  FixedSetIncrementalLookup lookup(graph, length);
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
int LookupSuffixInReversedSet(const unsigned char* graph,
                              size_t length,
                              bool include_private,
                              base::StringPiece host,
                              size_t* suffix_length) {
  FixedSetIncrementalLookup lookup(graph, length);
  *suffix_length = 0;
  int result = kDafsaNotFound;
  base::StringPiece::const_iterator pos = host.end();
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
