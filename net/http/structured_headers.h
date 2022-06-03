// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_STRUCTURED_HEADERS_H_
#define NET_HTTP_STRUCTURED_HEADERS_H_

#include <algorithm>
#include <map>
#include <string>
#include <tuple>
#include <vector>

#include "base/strings/string_piece.h"
#include "net/base/net_export.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace net {
namespace structured_headers {

// This file implements parsing of HTTP structured headers, as defined in
// https://httpwg.org/http-extensions/draft-ietf-httpbis-header-structure.html.
//
// Both drafts 9 and 15 are currently supported. The major difference
// between the two drafts is in the various list formats: Draft 9 describes
// Parameterised lists and lists-of-lists, while draft 15 uses a single List
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
//  Dictionary: a=(1 2), b=3, c=4;aa=bb, d=(5 6);valid=?0
//
// Functions are provided to parse each of these, which are intended to be
// called with the complete value of an HTTP header (that is, any
// sub-structure will be handled internally by the parser; the exported
// functions are not intended to be called on partial header strings.) Input
// values should be ASCII byte strings (non-ASCII characters should not be
// present in Structured Header values, and will cause the entire header to fail
// to parse.)

class NET_EXPORT Item {
 public:
  enum ItemType {
    kNullType,
    kIntegerType,
    kDecimalType,
    kStringType,
    kTokenType,
    kByteSequenceType,
    kBooleanType
  };
  Item();
  explicit Item(int64_t value);
  explicit Item(double value);
  explicit Item(bool value);

  // Constructors for string-like items: Strings, Tokens and Byte Sequences.
  Item(const char* value, Item::ItemType type = kStringType);
  //  Item(StringPiece value, Item::ItemType type = kStringType);
  Item(const std::string& value, Item::ItemType type = kStringType);
  Item(std::string&& value, Item::ItemType type = kStringType);

  NET_EXPORT friend bool operator==(const Item& lhs, const Item& rhs);
  inline friend bool operator!=(const Item& lhs, const Item& rhs) {
    return !(lhs == rhs);
  }

  bool is_null() const { return type_ == kNullType; }
  bool is_integer() const { return type_ == kIntegerType; }
  bool is_decimal() const { return type_ == kDecimalType; }
  bool is_string() const { return type_ == kStringType; }
  bool is_token() const { return type_ == kTokenType; }
  bool is_byte_sequence() const { return type_ == kByteSequenceType; }
  bool is_boolean() const { return type_ == kBooleanType; }

  int64_t GetInteger() const {
    DCHECK_EQ(type_, kIntegerType);
    return integer_value_;
  }
  double GetDecimal() const {
    DCHECK_EQ(type_, kDecimalType);
    return decimal_value_;
  }
  bool GetBoolean() const {
    DCHECK_EQ(type_, kBooleanType);
    return boolean_value_;
  }
  // TODO(iclelland): Split up accessors for String, Token and Byte Sequence.
  const std::string& GetString() const {
    DCHECK(type_ == kStringType || type_ == kTokenType ||
           type_ == kByteSequenceType);
    return string_value_;
  }

  ItemType Type() const { return type_; }

 private:
  ItemType type_ = kNullType;
  // TODO(iclelland): Make this class more memory-efficient, replacing the
  // values here with a union or std::variant (when available).
  int64_t integer_value_ = 0;
  std::string string_value_;
  double decimal_value_;
  bool boolean_value_;
};

// Holds a ParameterizedIdentifier (draft 9 only). The contained Item must be a
// Token, and there may be any number of parameters. Parameter ordering is not
// significant.
struct NET_EXPORT ParameterisedIdentifier {
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
  return std::tie(lhs.identifier, lhs.params) ==
         std::tie(rhs.identifier, rhs.params);
}

using Parameters = std::vector<std::pair<std::string, Item>>;

struct NET_EXPORT ParameterizedItem {
  Item item;
  Parameters params;

  ParameterizedItem(const ParameterizedItem&);
  ParameterizedItem& operator=(const ParameterizedItem&);
  ParameterizedItem(Item, const Parameters&);
  ~ParameterizedItem();
};

inline bool operator==(const ParameterizedItem& lhs,
                       const ParameterizedItem& rhs) {
  return std::tie(lhs.item, lhs.params) == std::tie(rhs.item, rhs.params);
}

inline bool operator!=(const ParameterizedItem& lhs,
                       const ParameterizedItem& rhs) {
  return !(lhs == rhs);
}

// Holds a ParameterizedMember, which may be either an single Item, or an Inner
// List of ParameterizedItems, along with any number of parameters. Parameter
// ordering is significant.
struct NET_EXPORT ParameterizedMember {
  std::vector<ParameterizedItem> member;
  // If false, then |member| should only hold one Item.
  bool member_is_inner_list = false;

  Parameters params;

  ParameterizedMember();
  ParameterizedMember(const ParameterizedMember&);
  ParameterizedMember& operator=(const ParameterizedMember&);
  ParameterizedMember(std::vector<ParameterizedItem>,
                      bool member_is_inner_list,
                      const Parameters&);
  // Shorthand constructor for a member which is an inner list.
  ParameterizedMember(std::vector<ParameterizedItem>, const Parameters&);
  // Shorthand constructor for a member which is a single Item.
  ParameterizedMember(Item, const Parameters&);
  ~ParameterizedMember();
};

inline bool operator==(const ParameterizedMember& lhs,
                       const ParameterizedMember& rhs) {
  return std::tie(lhs.member, lhs.member_is_inner_list, lhs.params) ==
         std::tie(rhs.member, rhs.member_is_inner_list, rhs.params);
}

using DictionaryMember = std::pair<std::string, ParameterizedMember>;

// Structured Headers Draft 15 Dictionary.
class NET_EXPORT Dictionary {
 public:
  using iterator = std::vector<DictionaryMember>::iterator;
  using const_iterator = std::vector<DictionaryMember>::const_iterator;

  Dictionary();
  Dictionary(const Dictionary&);
  explicit Dictionary(std::vector<DictionaryMember> members);
  ~Dictionary();
  Dictionary& operator=(const Dictionary&) = default;
  iterator begin();
  const_iterator begin() const;
  iterator end();
  const_iterator end() const;

  // operator[](size_t) and at(size_t) will both abort the program in case of
  // out of bounds access.
  ParameterizedMember& operator[](std::size_t idx);
  const ParameterizedMember& operator[](std::size_t idx) const;
  ParameterizedMember& at(std::size_t idx);
  const ParameterizedMember& at(std::size_t idx) const;

  // Consistent with std::map, if |key| does not exist in the Dictionary, then
  // operator[](base::StringPiece) will create an entry for it, but at() will
  // abort the entire program.
  ParameterizedMember& operator[](base::StringPiece key);
  ParameterizedMember& at(base::StringPiece key);
  const ParameterizedMember& at(base::StringPiece key) const;

  bool empty() const;
  std::size_t size() const;
  bool contains(base::StringPiece key) const;
  friend bool operator==(const Dictionary& lhs, const Dictionary& rhs);
  friend bool operator!=(const Dictionary& lhs, const Dictionary& rhs);

 private:
  // Uses a vector to hold pairs of key and dictionary member. This makes
  // look up by index and serialization much easier.
  std::vector<DictionaryMember> members_;
};

inline bool operator==(const Dictionary& lhs, const Dictionary& rhs) {
  return lhs.members_ == rhs.members_;
}

inline bool operator!=(const Dictionary& lhs, const Dictionary& rhs) {
  return !(lhs == rhs);
}

// Structured Headers Draft 09 Parameterised List.
using ParameterisedList = std::vector<ParameterisedIdentifier>;
// Structured Headers Draft 09 List of Lists.
using ListOfLists = std::vector<std::vector<Item>>;
// Structured Headers Draft 15 List.
using List = std::vector<ParameterizedMember>;

// Returns the result of parsing the header value as an Item, if it can be
// parsed as one, or nullopt if it cannot. Note that this uses the Draft 15
// parsing rules, and so applies tighter range limits to integers.
NET_EXPORT absl::optional<ParameterizedItem> ParseItem(base::StringPiece str);

// Returns the result of parsing the header value as an Item with no parameters,
// or nullopt if it cannot. Note that this uses the Draft 15 parsing rules, and
// so applies tighter range limits to integers.
NET_EXPORT absl::optional<Item> ParseBareItem(base::StringPiece str);

// Returns the result of parsing the header value as a Parameterised List, if it
// can be parsed as one, or nullopt if it cannot. Note that parameter keys will
// be returned as strings, which are guaranteed to be ASCII-encoded. List items,
// as well as parameter values, will be returned as Items. This method uses the
// Draft 09 parsing rules for Items, so integers have the 64-bit int range.
// Structured-Headers Draft 09 only.
NET_EXPORT absl::optional<ParameterisedList> ParseParameterisedList(
    base::StringPiece str);

// Returns the result of parsing the header value as a List of Lists, if it can
// be parsed as one, or nullopt if it cannot. Inner list items will be returned
// as Items. This method uses the Draft 09 parsing rules for Items, so integers
// have the 64-bit int range.
// Structured-Headers Draft 09 only.
NET_EXPORT absl::optional<ListOfLists> ParseListOfLists(base::StringPiece str);

// Returns the result of parsing the header value as a general List, if it can
// be parsed as one, or nullopt if it cannot.
// Structured-Headers Draft 15 only.
NET_EXPORT absl::optional<List> ParseList(base::StringPiece str);

// Returns the result of parsing the header value as a general Dictionary, if it
// can be parsed as one, or nullopt if it cannot. Structured-Headers Draft 15
// only.
NET_EXPORT absl::optional<Dictionary> ParseDictionary(
    const base::StringPiece& str);

// Serialization is implemented for Structured-Headers Draft 15 only.
NET_EXPORT absl::optional<std::string> SerializeItem(const Item& value);
NET_EXPORT absl::optional<std::string> SerializeItem(
    const ParameterizedItem& value);
NET_EXPORT absl::optional<std::string> SerializeList(const List& value);
NET_EXPORT absl::optional<std::string> SerializeDictionary(
    const Dictionary& value);

}  // namespace structured_headers
}  // namespace net

#endif  // NET_HTTP_STRUCTURED_HEADERS_H_
