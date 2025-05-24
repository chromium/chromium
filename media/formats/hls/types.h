// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FORMATS_HLS_TYPES_H_
#define MEDIA_FORMATS_HLS_TYPES_H_

#include <cstdint>
#include <optional>

#include "base/containers/span.h"
#include "base/memory/raw_span.h"
#include "base/time/time.h"
#include "media/formats/hls/parse_status.h"
#include "media/formats/hls/source_string.h"
#include "media/formats/hls/variable_dictionary.h"

namespace media::hls::types {

namespace {

template <size_t count, typename impl, typename... types>
struct repeat_t {
  using type = repeat_t<count - 1, impl, impl, types...>::type;
};

template <typename impl, typename... types>
struct repeat_t<0, impl, types...> {
  using type = std::tuple<types...>;
};

}  // namespace

// A `DecimalInteger` is an unsigned integer value.
// https://datatracker.ietf.org/doc/html/draft-pantos-hls-rfc8216bis#:~:text=of%20the%20following%3A%0A%0A%20%20%20o-,decimal%2Dinteger,-%3A%20an%20unquoted%20string
using DecimalInteger = uint64_t;

namespace parsing {

// A substituting parser functions as a super-struct parser which provides the
// entry points for raw SourceStrings to either be consumed raw or resolved by
// the sub-struct's Parse method. It is specialized with:
// Subtype: The substruct implementation, which provides the method
//          `static T Parse(ResolvedSourceString, ParseArgs...)`
// T: The type that should result from successful parsing.
// ParseArgs...: Additional arguments that might be passed to the Parse
//               function declared on `Subtype`. For example, some quoted
//               strings are required to be non-empty, so the quoted string
//               parser should have an optional bool parameter to require it.
template <typename Subtype, typename T, typename... ParseArgs>
struct MEDIA_EXPORT SubstitutingParser {
  using ParseInto = T;

  static ParseStatus::Or<ParseInto> ParseWithSubstitution(
      SourceString str,
      const VariableDictionary& variable_dict,
      VariableDictionary::SubstitutionBuffer& sub_buffer,
      ParseArgs&&... args) {
    return variable_dict.Resolve(str, sub_buffer)
        .MapValue([... args = std::forward<ParseArgs>(args)](
                      ResolvedSourceString str) {
          return Subtype::Parse(str, std::forward<ParseArgs>(args)...);
        });
  }

  static ParseStatus::Or<ParseInto> ParseWithoutSubstitution(
      SourceString str,
      ParseArgs&&... args) {
    return Subtype::Parse(str.SkipVariableSubstitution(),
                          std::forward<ParseArgs>(args)...);
  }
};

// A wrapping parser that will parse some other type T which is contained
// withing quotation marks. Quoted<RawStr>::ParseWithoutSubstitution will ensure
// that the SourceString starts and ends with quotation marks, and will return
// a ResolvedSourceString representing the content inside those quotes.
template <typename T>
struct MEDIA_EXPORT Quoted
    : public SubstitutingParser<Quoted<T>, typename T::ParseInto> {
  static ParseStatus::Or<typename T::ParseInto> Parse(
      ResolvedSourceString str,
      bool allow_empty = false) {
    if (str.Size() < 2) {
      return ParseStatusCode::kFailedToParseQuotedString;
    }
    if (*str.Str().begin() != '"') {
      return ParseStatusCode::kFailedToParseQuotedString;
    }
    if (*str.Str().rbegin() != '"') {
      return ParseStatusCode::kFailedToParseQuotedString;
    }

    ResolvedSourceString unquoted = str.Substr(1, str.Size() - 2);
    if (!allow_empty && unquoted.Empty()) {
      return ParseStatusCode::kFailedToParseQuotedString;
    }

    return T::Parse(unquoted);
  }
};

// Parser struct for a plain ResolvedSourceString. This is usually used
// for things like URIs.
struct MEDIA_EXPORT RawStr : SubstitutingParser<RawStr, ResolvedSourceString> {
  static ParseStatus::Or<ResolvedSourceString> Parse(ResolvedSourceString str);
};

struct MEDIA_EXPORT YesOrNo : SubstitutingParser<YesOrNo, bool> {
  static ParseStatus::Or<bool> Parse(ResolvedSourceString str);
};

// Parser struct for floating point representations of TimeDelta instances.
struct MEDIA_EXPORT TimeDelta : SubstitutingParser<TimeDelta, base::TimeDelta> {
  static ParseStatus::Or<base::TimeDelta> Parse(ResolvedSourceString str);
};

// A `ByteRangeExpression` represents the 'length[@offset]' syntax that appears
// in tags describing byte ranges of a resource.
// https://datatracker.ietf.org/doc/html/draft-pantos-hls-rfc8216bis#section-4.4.4.2
struct MEDIA_EXPORT ByteRangeExpression
    : public SubstitutingParser<ByteRangeExpression, ByteRangeExpression> {
  static ParseStatus::Or<ByteRangeExpression> Parse(
      ResolvedSourceString source_str);

  // The length of the sub-range, in bytes.
  types::DecimalInteger length;

  // If present, the offset in bytes from the beginning of the resource.
  // If not present, the sub-range begins at the next byte following that of the
  // previous segment. The previous segment must be a subrange of the same
  // resource.
  std::optional<types::DecimalInteger> offset;
};

// Calculate chunk sizes for the hex parser. This needs to be separated so that
// the SubstitutingParser template can be specialized on the proper return type
// of HexRepr::Parse.
template <int64_t bits>
struct HexPackInfo {
  // Must be a multiple of 8!
  static_assert((bits % 0x8) == 0);

  // The pack size should be whatever is the largest integer type with a size
  // that is a factor of `bits`. Since there is no builtin integer type larger
  // than uint64_t, this number is also capped.
  static constexpr int64_t UnitSize = std::min<int64_t>(64, bits & -bits);

  // Select repeating unit.
  using Unit = std::conditional_t<
      (UnitSize == 64),
      uint64_t,
      std::conditional_t<
          (UnitSize == 32),
          uint32_t,
          std::conditional_t<(UnitSize == 16), uint16_t, uint8_t>>>;

  // A tuple of repeating `Unit`s
  using Container = repeat_t<bits / UnitSize, Unit>::type;
};

// Represents a hexadecimal string in the form 0xXXXXXX...
// where this might require more than sizeof(size_t) bytes.
// HexRepr<X>::Container can be used to get the tuple format for the
// underlying container that HexRepr<X>::Parse should return.
template <int64_t bits>
struct MEDIA_EXPORT HexRepr
    : public SubstitutingParser<HexRepr<bits>,
                                typename HexPackInfo<bits>::Container> {
  using Container = HexPackInfo<bits>::Container;
  using Unit = HexPackInfo<bits>::Unit;
  static constexpr int64_t UnitSize = HexPackInfo<bits>::UnitSize;

  template <size_t reps>
  struct ParserImpl {
    static ParseStatus::Or<typename repeat_t<reps, Unit>::type> Parse(
        ResolvedSourceString str,
        bool extrapolate_leading_zeros) {
      if constexpr (reps == 0) {
        if (str.Size() != 0) {
          return ParseStatusCode::kFailedToParseHexadecimalString;
        }
        return std::make_tuple();
      } else {
        Unit chunk = 0;
        for (size_t i = 0; i < sizeof(Unit) * 2; i++) {
          if (!extrapolate_leading_zeros && !str.Size()) {
            return ParseStatusCode::kFailedToParseHexadecimalString;
          }
          if (str.Size()) {
            auto bits4 = str.Str()[str.Size() - 1];
            Unit num4 = 0;
            if (bits4 >= '0' && bits4 <= '9') {
              num4 = bits4 - '0';
            } else if (bits4 >= 'A' && bits4 <= 'F') {
              num4 = bits4 - 'A' + 10;
            } else if (bits4 >= 'a' && bits4 <= 'f') {
              num4 = bits4 - 'a' + 10;
            } else {
              return ParseStatusCode::kFailedToParseHexadecimalString;
            }
            chunk += (num4 << (4 * i));
            str = str.Substr(0, str.Size() - 1);
          }
        }
        auto rest = ParserImpl<reps - 1>::Parse(str, extrapolate_leading_zeros);
        if (!rest.has_value()) {
          return std::move(rest).error();
        }
        return std::tuple_cat(std::move(rest).value(), std::make_tuple(chunk));
      }
    }
  };

  static ParseStatus::Or<Container> Parse(
      ResolvedSourceString str,
      bool extrapolate_leading_zeros = false,
      bool has_prefix = true) {
    if (has_prefix && str.Consume(2).Str() != "0x") {
      return ParseStatusCode::kFailedToParseHexadecimalString;
    }
    if (!str.Size()) {
      return ParseStatusCode::kFailedToParseHexadecimalString;
    }
    return ParserImpl<bits / UnitSize>::Parse(str, extrapolate_leading_zeros);
  }
};

}  // namespace parsing

MEDIA_EXPORT ParseStatus::Or<DecimalInteger> ParseDecimalInteger(
    ResolvedSourceString source_str);

// A `DecimalFloatingPoint` is an unsigned floating-point value.
// https://datatracker.ietf.org/doc/html/draft-pantos-hls-rfc8216bis#:~:text=on%20its%20AttributeNames.%0A%0A%20%20%20o-,decimal%2Dfloating%2Dpoint,-%3A%20an%20unquoted%20string
using DecimalFloatingPoint = double;

MEDIA_EXPORT ParseStatus::Or<DecimalFloatingPoint> ParseDecimalFloatingPoint(
    ResolvedSourceString source_str);

// A `SignedDecimalFloatingPoint` is a signed floating-point value.
// https://datatracker.ietf.org/doc/html/draft-pantos-hls-rfc8216bis#:~:text=decimal%20positional%20notation.%0A%0A%20%20%20o-,signed%2Ddecimal%2Dfloating%2Dpoint,-%3A%20an%20unquoted%20string
using SignedDecimalFloatingPoint = double;

MEDIA_EXPORT ParseStatus::Or<SignedDecimalFloatingPoint>
ParseSignedDecimalFloatingPoint(ResolvedSourceString source_str);

// A `DecimalResolution` is a set of two `DecimalInteger`s describing width and
// height.
// https://datatracker.ietf.org/doc/html/draft-pantos-hls-rfc8216bis#:~:text=enumerated%2Dstring%2Dlist.%0A%0A%20%20%20o-,decimal%2Dresolution,-%3A%20two%20decimal%2Dintegers
struct MEDIA_EXPORT DecimalResolution {
  static ParseStatus::Or<DecimalResolution> Parse(
      ResolvedSourceString source_str);

  types::DecimalInteger width;
  types::DecimalInteger height;

  types::DecimalInteger Area() const { return width * height; }
  types::DecimalInteger Szudzik() const;
};

// This is similar to `ByteRangeExpression`, but with a stronger contract:
// - `length` is non-zero
// - `offset` is non-optional
// - `offset+length` may not overflow `types::DecimalInteger`
class MEDIA_EXPORT ByteRange {
 public:
  // Validates that the range given by `[offset,offset+length)` is non-empty and
  // that `GetEnd()` would not exceed the max value representable by a
  // `DecimalInteger`.
  static std::optional<ByteRange> Validate(DecimalInteger length,
                                           DecimalInteger offset);

  DecimalInteger GetLength() const { return length_; }
  DecimalInteger GetOffset() const { return offset_; }
  DecimalInteger GetEnd() const { return offset_ + length_; }

 private:
  ByteRange(DecimalInteger length, DecimalInteger offset)
      : length_(length), offset_(offset) {}

  DecimalInteger length_;
  DecimalInteger offset_;
};

// Parses a string surrounded by double-quotes ("), returning the inner string.
// These appear in the context of attribute-lists, and are subject to variable
// substitution. `sub_buffer` must outlive the returned string.
// `allow_empty` determines whether an empty quoted string is accepted, (after
// variable substitution) which isn't the case for most attributes.
MEDIA_EXPORT ParseStatus::Or<ResolvedSourceString> ParseQuotedString(
    SourceString source_str,
    const VariableDictionary& variable_dict,
    VariableDictionary::SubstitutionBuffer& sub_buffer,
    bool allow_empty = false);

// Parses a string surrounded by double-quotes ("), returning the interior
// string. These appear in the context of attribute-lists, however certain tags
// disallow variable substitution so this function exists to serve those.
// `allow_empty` determines whether an empty quoted string is accepted, which
// isn't the case for most attributes.
MEDIA_EXPORT ParseStatus::Or<SourceString> ParseQuotedStringWithoutSubstitution(
    SourceString source_str,
    bool allow_empty = false);

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
  using Item = std::pair<std::string_view, std::optional<SourceString>>;

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
    return {{{keys, std::nullopt}...}};
  }

  ~AttributeMap();

 private:
  base::raw_span<Item> items_;
};

// Represents a string that is guaranteed to be a non-empty, and consisting only
// of characters in the set {[a-z], [A-Z], [0-9], _, -}. Variable names are
// case-sensitive.
class MEDIA_EXPORT VariableName {
 public:
  static ParseStatus::Or<VariableName> Parse(SourceString source_str);

  std::string_view GetName() const { return name_; }

 private:
  explicit VariableName(std::string_view name) : name_(name) {}

  std::string_view name_;
};

// Represents a string that is guaranteed to be non-empty, and consisting only
// of characters in the set {[a-z], [A-Z], [0-9], +, /, =, ., -, _}.
// This is used in the 'STABLE-VARIANT-ID' and 'STABLE-RENDITION-ID' attributes
// of the EXT-X-STREAM-INF and EXT-X-MEDIA tags, respectively.
class MEDIA_EXPORT StableId {
 public:
  static ParseStatus::Or<StableId> Parse(ResolvedSourceString str);
  static StableId CreateForTesting(std::string_view str) {
    return Parse(ResolvedSourceString::CreateForTesting(str)).value();
  }

  const std::string& Str() const { return id_; }

 private:
  explicit StableId(std::string id) : id_(std::move(id)) {}

  std::string id_;
};

inline bool operator==(const StableId& lhs, const StableId& rhs) {
  return lhs.Str() == rhs.Str();
}
inline bool operator!=(const StableId& lhs, const StableId& rhs) {
  return lhs.Str() != rhs.Str();
}
inline bool operator<(const StableId& lhs, const StableId& rhs) {
  return lhs.Str() < rhs.Str();
}
inline bool operator>(const StableId& lhs, const StableId& rhs) {
  return lhs.Str() > rhs.Str();
}

// Represents the contents of the 'INSTREAM-ID' attribute on the 'EXT-X-MEDIA'
// tag.
class MEDIA_EXPORT InstreamId {
 public:
  enum class Type {
    kCc,
    kService,
  };

  static ParseStatus::Or<InstreamId> Parse(ResolvedSourceString);

  Type GetType() const { return type_; }
  uint8_t GetNumber() const { return number_; }

 private:
  InstreamId(Type type, uint8_t number) : type_(type), number_(number) {}

  Type type_;
  uint8_t number_;
};

// Represents the contents of the 'CHANNELS' attribute on the 'EXT-X-MEDIA' tag
// for an audio stream.
// https://datatracker.ietf.org/doc/html/draft-pantos-hls-rfc8216bis#section-4.4.6.1:~:text=If%20the%20TYPE%20attribute%20is%20AUDIO%2C%20then%20the%20first%20parameter%20is%20a
class MEDIA_EXPORT AudioChannels {
 public:
  AudioChannels(const AudioChannels&);
  AudioChannels(AudioChannels&&);
  AudioChannels& operator=(const AudioChannels&);
  AudioChannels& operator=(AudioChannels&&);
  ~AudioChannels();

  static ParseStatus::Or<AudioChannels> Parse(ResolvedSourceString);

  // Returns the max number of independent, simultaneous audio channels present
  // in any media segment in the associated rendition.
  DecimalInteger GetMaxChannels() const { return max_channels_; }

  // Returns the list of audio coding identifiers, which are strings of
  // characters in the set [A-Z], [0-9], '-'. This list may be empty, or may
  // only contain "-", indicating that the audio is only channel-based.
  const std::vector<std::string>& GetAudioCodingIdentifiers() const {
    return audio_coding_identifiers_;
  }

 private:
  AudioChannels(DecimalInteger max_channels,
                std::vector<std::string> audio_coding_identifiers);

  DecimalInteger max_channels_;
  std::vector<std::string> audio_coding_identifiers_;
};

}  // namespace media::hls::types

#endif  // MEDIA_FORMATS_HLS_TYPES_H_
