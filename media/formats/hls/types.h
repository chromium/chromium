// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FORMATS_HLS_TYPES_H_
#define MEDIA_FORMATS_HLS_TYPES_H_

#include <cstdint>
#include "base/containers/span.h"
#include "media/formats/hls/parse_status.h"
#include "media/formats/hls/source_string.h"
#include "media/formats/hls/variable_dictionary.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace media::hls::types {

// A `DecimalInteger` is an unsigned integer value.
// https://datatracker.ietf.org/doc/html/draft-pantos-hls-rfc8216bis#:~:text=of%20the%20following%3A%0A%0A%20%20%20o-,decimal%2Dinteger,-%3A%20an%20unquoted%20string
using DecimalInteger = uint64_t;

MEDIA_EXPORT ParseStatus::Or<DecimalInteger> ParseDecimalInteger(
    SourceString source_str);

// A `DecimalFloatingPoint` is an unsigned floating-point value.
// https://datatracker.ietf.org/doc/html/draft-pantos-hls-rfc8216bis#:~:text=on%20its%20AttributeNames.%0A%0A%20%20%20o-,decimal%2Dfloating%2Dpoint,-%3A%20an%20unquoted%20string
using DecimalFloatingPoint = double;

MEDIA_EXPORT ParseStatus::Or<DecimalFloatingPoint> ParseDecimalFloatingPoint(
    SourceString source_str);

// A `SignedDecimalFloatingPoint` is a signed floating-point value.
// https://datatracker.ietf.org/doc/html/draft-pantos-hls-rfc8216bis#:~:text=decimal%20positional%20notation.%0A%0A%20%20%20o-,signed%2Ddecimal%2Dfloating%2Dpoint,-%3A%20an%20unquoted%20string
using SignedDecimalFloatingPoint = double;

MEDIA_EXPORT ParseStatus::Or<SignedDecimalFloatingPoint>
ParseSignedDecimalFloatingPoint(SourceString source_str);

// A `DecimalResolution` is a set of two `DecimalInteger`s describing width and
// height.
// https://datatracker.ietf.org/doc/html/draft-pantos-hls-rfc8216bis#:~:text=enumerated%2Dstring%2Dlist.%0A%0A%20%20%20o-,decimal%2Dresolution,-%3A%20two%20decimal%2Dintegers
struct MEDIA_EXPORT DecimalResolution {
  types::DecimalInteger width;
  types::DecimalInteger height;

  static ParseStatus::Or<DecimalResolution> Parse(SourceString source_str);
};

// Parses a string surrounded by double-quotes ("), returning the inner string.
// These appear in the context of attribute-lists, and are subject to variable
// substitution. `sub_buffer` must outlive the returned string.
MEDIA_EXPORT ParseStatus::Or<base::StringPiece> ParseQuotedString(
    SourceString source_str,
    const VariableDictionary& variable_dict,
    VariableDictionary::SubstitutionBuffer& sub_buffer);

// Parses a string surrounded by double-quotes ("), returning the interior
// string. These appear in the context of attribute-lists, however certain tags
// disallow variable substitution so this function exists to serve those.
MEDIA_EXPORT ParseStatus::Or<SourceString> ParseQuotedStringWithoutSubstitution(
    SourceString source_str);

// Provides an iterator-style interface over attribute-lists.
// Since the number of attributes expected in an attribute-list for a tag varies
// (most have 2-4, the highest has 15), rather than prescribing a specific data
// structure to use, callers can iterate over the list and build their own.
// `AttributeMap` exists which can match items against a pre-determined set of
// keys, which may be stored on the stack.
struct MEDIA_EXPORT AttributeListIterator {
  struct Item {
    SourceString name;
    SourceString value;
  };

  explicit AttributeListIterator(SourceString content);

  // Parses the next item in the attribute-list, and returns it, or an error.
  // Returns `ParseStatusCode::kReachedEOF` if no further items exist.
  ParseStatus::Or<Item> Next();

 private:
  SourceString remaining_content_;
};

// Represents a map of attributes with a fixed set of keys.
// This is essentially a `base::fixed_flat_map`, with the advantage of erasing
// the size of the map from its type.
struct MEDIA_EXPORT AttributeMap {
  using Item = std::pair<base::StringPiece, absl::optional<SourceString>>;

  // Constructs an AttributeMap using the given span to store the keys and
  // values. The keys present must be unique and sorted in alphabetical order.
  explicit AttributeMap(base::span<Item> sorted_items);

  // Fills this map with the given iterator until one of the following occurs:
  // - iter->Next() returns a error. The error will be forwarded to the caller.
  // - iter->Next() returns an Item with an unrecognized name. The item will be
  // forwarded to the caller.
  // - iter->Next() returns an Item with a name that has already been seen.
  // `ParseStatusCode::kAttributeListHasDuplicateNames` will be returned to the
  // caller, and the iterator will be left pointing at the duplicate entry.
  // As with `AttributeListIterator::Next()`, when there is no more data this
  // function will return `kReachedEOF`. The caller may then verify that
  // required keys have been filled, and mutually exclusive keys have not been
  // simultaneously filled.
  ParseStatus::Or<AttributeListIterator::Item> Fill(
      AttributeListIterator* iter);

  // Like `Fill`, but doesn't stop to report unknown keys to the caller.
  ParseStatus FillUntilError(AttributeListIterator* iter);

  // Helper for creating backing storage for an AttributeMap on the stack.
  // `keys` must be a set of unique key strings sorted in alphabetical order.
  template <typename... T>
  static constexpr std::array<Item, sizeof...(T)> MakeStorage(T... keys) {
    return {{{keys, absl::nullopt}...}};
  }

 private:
  base::span<Item> items_;
};

// Represents a string that is guaranteed to be a non-empty, and consisting only
// of characters in the set {[a-z], [A-Z], [0-9], _, -}. Variable names are
// case-sensitive.
class MEDIA_EXPORT VariableName {
 public:
  static ParseStatus::Or<VariableName> Parse(SourceString source_str);

  base::StringPiece GetName() const { return name_; }

 private:
  explicit VariableName(base::StringPiece name) : name_(name) {}

  base::StringPiece name_;
};

}  // namespace media::hls::types

#endif  // MEDIA_FORMATS_HLS_TYPES_H_
