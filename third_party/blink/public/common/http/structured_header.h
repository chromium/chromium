// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_HTTP_STRUCTURED_HEADER_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_HTTP_STRUCTURED_HEADER_H_

#include <algorithm>
#include <map>
#include <string>
#include <vector>

#include "base/optional.h"
#include "base/strings/string_piece.h"
#include "third_party/blink/public/common/common_export.h"

namespace blink {
namespace http_structured_header {

// This file implements parsing of HTTP structured headers, as defined in
// https://httpwg.org/http-extensions/draft-ietf-httpbis-header-structure.html.
//
// Both drafts 9 and 13 are currently supported. The major difference
// between the two drafts is in the various list formats: Draft 9 describes
// Parameterised lists and lists-of-lists, while draft 13 uses a single List
// syntax, whose members may be inner lists. There should be no ambiguity,
// however, as the code which calls this parser should be expecting only a
// single type for a given header.
//
// Currently supported data types are:
//  Item:
//   integer: 123
//   string: "abc"
//   token: abc
//   byte sequence: *YWJj*
//  Parameterised list: abc_123;a=1;b=2; cdef_456, ghi;q="9";r="w"
//  List-of-lists: "foo";"bar", "baz", "bat"; "one"
//  List: "foo", "bar", "It was the best of times."
//        ("foo" "bar"), ("baz"), ("bat" "one"), ()
//        abc;a=1;b=2; cde_456, (ghi jkl);q="9";r=w
//
// Functions are provided to parse each of these, which are intended to be
// called with the complete value of an HTTP header (that is, any
// sub-structure will be handled internally by the parser; the exported
// functions are not intended to be called on partial header strings.) Input
// values should be ASCII byte strings (non-ASCII characters should not be
// present in Structured Header values, and will cause the entire header to fail
// to parse.)
//
// Currently only limited types (non-negative integers, strings, tokens and
// byte sequences) are supported.
// TODO(1011101): Add support for other types.

class BLINK_COMMON_EXPORT Item {
 public:
  enum ItemType {
    kNullType,
    kIntegerType,
    kStringType,
    kTokenType,
    kByteSequenceType
  };
  Item();
  Item(int64_t value);

  // Constructors for string-like items: Strings, Tokens and Byte Sequences.
  Item(const std::string& value, Item::ItemType type = kStringType);
  Item(std::string&& value, Item::ItemType type = kStringType);

  BLINK_COMMON_EXPORT friend bool operator==(const Item& lhs, const Item& rhs);
  inline friend bool operator!=(const Item& lhs, const Item& rhs) {
    return !(lhs == rhs);
  }

  bool is_null() const { return type_ == kNullType; }
  bool is_integer() const { return type_ == kIntegerType; }
  bool is_string() const { return type_ == kStringType; }
  bool is_token() const { return type_ == kTokenType; }
  bool is_byte_sequence() const { return type_ == kByteSequenceType; }

  int64_t integer() const {
    DCHECK_EQ(type_, kIntegerType);
    return integer_value_;
  }
  // TODO(iclelland): Split up accessors for String, Token and Byte Sequence.
  const std::string& string() const {
    DCHECK(type_ == kStringType || type_ == kTokenType ||
           type_ == kByteSequenceType);
    return string_value_;
  }

 private:
  ItemType type_ = kNullType;
  // TODO(iclelland): Make this class more memory-efficient, replacing the
  // values here with a union or std::variant (when available).
  int64_t integer_value_ = 0;
  std::string string_value_;
};

// Holds a ParameterizedIdentifier (draft 9 only). The contained Item must be a
// Token, and there may be any number of parameters. Parameter ordering is not
// significant.
struct BLINK_COMMON_EXPORT ParameterisedIdentifier {
  using Parameters = std::map<std::string, Item>;

  Item identifier;
  Parameters params;

  ParameterisedIdentifier(const ParameterisedIdentifier&);
  ParameterisedIdentifier& operator=(const ParameterisedIdentifier&);
  ParameterisedIdentifier(Item, const Parameters&);
  ~ParameterisedIdentifier();
};

inline bool operator==(const ParameterisedIdentifier& lhs,
                       const ParameterisedIdentifier& rhs) {
  return lhs.identifier == rhs.identifier && lhs.params == rhs.params;
}

// Holds a ParameterizedMember, which may be either an Inner List, or a single
// Item, with any number of parameters. Parameter ordering is significant.
struct BLINK_COMMON_EXPORT ParameterizedMember {
  using Parameters = std::vector<std::pair<std::string, Item>>;

  std::vector<Item> member;
  // If false, then |member| should only hold one Item.
  bool member_is_inner_list;

  Parameters params;

  ParameterizedMember(const ParameterizedMember&);
  ParameterizedMember& operator=(const ParameterizedMember&);
  ParameterizedMember(std::vector<Item>, bool, const Parameters&);
  // Shorthand constructor for a member which is an inner list.
  ParameterizedMember(std::vector<Item>, const Parameters&);
  // Shorthand constructor for a member which is a single Item.
  ParameterizedMember(Item, const Parameters&);
  ~ParameterizedMember();
};

inline bool operator==(const ParameterizedMember& lhs,
                       const ParameterizedMember& rhs) {
  return lhs.member == rhs.member &&
         lhs.member_is_inner_list == rhs.member_is_inner_list &&
         lhs.params == rhs.params;
}

// Structured Headers Draft 09 Parameterised List.
using ParameterisedList = std::vector<ParameterisedIdentifier>;
// Structured Headers Draft 09 List of Lists.
using ListOfLists = std::vector<std::vector<Item>>;
// Structured Headers Draft 13 List.
using List = std::vector<ParameterizedMember>;

// Returns the result of parsing the header value as an Item, if it can be
// parsed as one, or nullopt if it cannot. Note that this uses the Draft 13
// parsing rules, and so applies tighter range limits to integers.
BLINK_COMMON_EXPORT base::Optional<Item> ParseItem(
    const base::StringPiece& str);

// Returns the result of parsing the header value as a Parameterised List, if it
// can be parsed as one, or nullopt if it cannot. Note that parameter keys will
// be returned as strings, which are guaranteed to be ASCII-encoded. List items,
// as well as parameter values, will be returned as Items. This method uses the
// Draft 09 parsing rules for Items, so integers have the 64-bit int range.
// Structured-Headers Draft 09 only.
BLINK_COMMON_EXPORT base::Optional<ParameterisedList> ParseParameterisedList(
    const base::StringPiece& str);

// Returns the result of parsing the header value as a List of Lists, if it can
// be parsed as one, or nullopt if it cannot. Inner list items will be returned
// as Items. This method uses the Draft 09 parsing rules for Items, so integers
// have the 64-bit int range.
// Structured-Headers Draft 09 only.
BLINK_COMMON_EXPORT base::Optional<ListOfLists> ParseListOfLists(
    const base::StringPiece& str);

// Returns the result of parsing the header value as a general List, if it can
// be parsed as one, or nullopt if it cannot.
// Structured-Headers Draft 13 only.
BLINK_COMMON_EXPORT base::Optional<List> ParseList(
    const base::StringPiece& str);

// Serialization is implemented for Structured-Headers Draft 13 only.
BLINK_COMMON_EXPORT base::Optional<std::string> SerializeItem(
    const Item& value);
BLINK_COMMON_EXPORT base::Optional<std::string> SerializeList(
    const List& value);

}  // namespace http_structured_header
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_HTTP_STRUCTURED_HEADER_H_
