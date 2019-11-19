// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/http/structured_header.h"

#include <string>
#include <utility>

#include "base/base64.h"
#include "base/containers/flat_set.h"
#include "base/containers/span.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"

namespace blink {
namespace http_structured_header {

namespace {

#define DIGIT "0123456789"
#define LCALPHA "abcdefghijklmnopqrstuvwxyz"
#define UCALPHA "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
// https://tools.ietf.org/html/draft-ietf-httpbis-header-structure-09#section-3.9
// https://tools.ietf.org/html/draft-ietf-httpbis-header-structure-13#section-3.7
constexpr char kTokenChars[] = DIGIT UCALPHA LCALPHA "_-.:%*/";
// https://tools.ietf.org/html/draft-ietf-httpbis-header-structure-09#section-3.1
constexpr char kKeyChars09[] = DIGIT LCALPHA "_-";
// https://tools.ietf.org/html/draft-ietf-httpbis-header-structure-13#section-3.1
constexpr char kKeyChars13[] = DIGIT LCALPHA "_-*";
#undef DIGIT
#undef LCALPHA
#undef UCALPHA

// https://tools.ietf.org/html/draft-ietf-httpbis-header-structure-13#section-3.4
// TODO(1011101): Add support for negative integers.
constexpr int64_t kMaxInteger = 999999999999999L;
constexpr int64_t kMinInteger = 0L;

// https://tools.ietf.org/html/draft-ietf-httpbis-header-structure-09#section-3.8
// https://tools.ietf.org/html/draft-ietf-httpbis-header-structure-13#section-3.6
bool IsPrintableASCII(char c) {
  return ' ' <= c && c <= '~';  // 0x20 (' ') to 0x7E ('~')
}

// Parser for (a subset of) Structured Headers for HTTP defined in [SH09] and
// [SH13]. [SH09] compatibility is retained for use by Web Packaging, and can be
// removed once that spec is updated, and users have migrated to new headers.
// [SH09] https://tools.ietf.org/html/draft-ietf-httpbis-header-structure-09
// [SH13] https://tools.ietf.org/html/draft-ietf-httpbis-header-structure-13
class StructuredHeaderParser {
 public:
  enum DraftVersion {
    kDraft09,
    kDraft13,
  };
  explicit StructuredHeaderParser(const base::StringPiece& str,
                                  DraftVersion version)
      : input_(str), version_(version) {
    // [SH09] 4.2 Step 1.
    // [SH13] 4.2 Step 2.
    // Discard any leading OWS from input_string.
    SkipWhitespaces();
  }

  // Callers should call this after ReadSomething(), to check if parser has
  // consumed all the input successfully.
  bool FinishParsing() {
    // [SH09] 4.2 Step 7. [SH13] 4.2 Step 6.
    // Discard any leading OWS from input_string.
    SkipWhitespaces();
    // [SH09] 4.2 Step 8. [SH13] 4.2 Step 7.
    // If input_string is not empty, fail parsing.
    return input_.empty();
  }

  // Parses a List of Lists ([SH09] 4.2.4).
  base::Optional<ListOfLists> ReadListOfLists() {
    DCHECK_EQ(version_, kDraft09);
    ListOfLists result;
    while (true) {
      std::vector<Item> inner_list;
      while (true) {
        base::Optional<Item> item(ReadItem());
        if (!item)
          return base::nullopt;
        inner_list.push_back(std::move(*item));
        SkipWhitespaces();
        if (!ConsumeChar(';'))
          break;
        SkipWhitespaces();
      }
      result.push_back(std::move(inner_list));
      SkipWhitespaces();
      if (!ConsumeChar(','))
        break;
      SkipWhitespaces();
    }
    return result;
  }

  // Parses a List ([SH13] 4.2.1).
  base::Optional<List> ReadList() {
    DCHECK_EQ(version_, kDraft13);
    List members;
    while (!input_.empty()) {
      base::Optional<ParameterizedMember> member(ReadParameterizedMember());
      if (!member)
        return base::nullopt;
      members.push_back(std::move(*member));
      SkipWhitespaces();
      if (!ConsumeChar(','))
        break;
      SkipWhitespaces();
      if (input_.empty())
        return base::nullopt;
    }
    return members;
  }

  // Parses an Item ([SH09] 4.2.7, [SH13] 4.2.3).
  // Currently only limited types (non-negative integers, strings, tokens and
  // byte sequences) are supported.
  // TODO(1011101): Add support for other types.
  base::Optional<Item> ReadItem() {
    if (input_.empty()) {
      DVLOG(1) << "ReadItem: unexpected EOF";
      return base::nullopt;
    }
    switch (input_.front()) {
      case '"':
        return ReadString();
      case '*':
        return ReadByteSequence();
      default:
        if (base::IsAsciiDigit(input_.front()))
          return ReadNumber();
        else
          return ReadToken();
    }
  }

  // Parses a Parameterised List ([SH09] 4.2.5).
  base::Optional<ParameterisedList> ReadParameterisedList() {
    DCHECK_EQ(version_, kDraft09);
    ParameterisedList items;
    while (true) {
      base::Optional<ParameterisedIdentifier> item =
          ReadParameterisedIdentifier();
      if (!item)
        return base::nullopt;
      items.push_back(std::move(*item));
      SkipWhitespaces();
      if (!ConsumeChar(','))
        return items;
      SkipWhitespaces();
    }
  }

 private:
  // Parses a Parameterised Identifier ([SH09] 4.2.6).
  base::Optional<ParameterisedIdentifier> ReadParameterisedIdentifier() {
    DCHECK_EQ(version_, kDraft09);
    base::Optional<Item> primary_identifier = ReadToken();
    if (!primary_identifier)
      return base::nullopt;

    ParameterisedIdentifier::Parameters parameters;

    SkipWhitespaces();
    while (ConsumeChar(';')) {
      SkipWhitespaces();

      base::Optional<std::string> name = ReadKey();
      if (!name)
        return base::nullopt;

      Item value;
      if (ConsumeChar('=')) {
        auto item = ReadItem();
        if (!item)
          return base::nullopt;
        value = std::move(*item);
      }
      if (!parameters.emplace(*name, value).second) {
        DVLOG(1) << "ReadParameterisedIdentifier: duplicated parameter: "
                 << *name;
        return base::nullopt;
      }
      SkipWhitespaces();
    }
    return ParameterisedIdentifier(std::move(*primary_identifier),
                                   std::move(parameters));
  }

  // Parses a Parameterized Member ([SH13] 4.2.1.1).
  base::Optional<ParameterizedMember> ReadParameterizedMember() {
    DCHECK_EQ(version_, kDraft13);
    std::vector<Item> member;
    bool member_is_inner_list = ConsumeChar('(');
    if (member_is_inner_list) {
      base::Optional<std::vector<Item>> inner_list = ReadInnerList();
      if (!inner_list)
        return base::nullopt;
      member = std::move(*inner_list);
    } else {
      base::Optional<Item> item = ReadItem();
      if (!item)
        return base::nullopt;
      member.push_back(std::move(*item));
    }

    ParameterizedMember::Parameters parameters;
    base::flat_set<std::string> keys;

    SkipWhitespaces();
    while (ConsumeChar(';')) {
      SkipWhitespaces();

      base::Optional<std::string> name = ReadKey();
      if (!name)
        return base::nullopt;
      bool is_duplicate_key = !keys.insert(*name).second;
      if (is_duplicate_key) {
        DVLOG(1) << "ReadParameterizedMember: duplicated parameter: " << *name;
        return base::nullopt;
      }

      Item value;
      if (ConsumeChar('=')) {
        auto item = ReadItem();
        if (!item)
          return base::nullopt;
        value = std::move(*item);
      }
      parameters.emplace_back(std::move(*name), std::move(value));
      SkipWhitespaces();
    }
    return ParameterizedMember(std::move(member), member_is_inner_list,
                               std::move(parameters));
  }

  // Parses an Inner List ([SH13] 4.2.1.2).
  // Note that the initial '(' character should already have been consumed by
  // the caller to determine that this is in fact an inner list.
  base::Optional<std::vector<Item>> ReadInnerList() {
    DCHECK_EQ(version_, kDraft13);
    std::vector<Item> inner_list;
    while (true) {
      SkipWhitespaces();
      if (ConsumeChar(')')) {
        return inner_list;
      }
      auto item = ReadItem();
      if (!item)
        return base::nullopt;
      inner_list.push_back(std::move(*item));
      if (input_.empty() || (input_.front() != ' ' && input_.front() != ')'))
        return base::nullopt;
    }
    NOTREACHED();
    return base::nullopt;
  }

  // Parses a Key ([SH09] 4.2.2, [SH13] 4.2.1.3).
  base::Optional<std::string> ReadKey() {
    if (input_.empty() || !base::IsAsciiLower(input_.front())) {
      LogParseError("ReadKey", "lcalpha");
      return base::nullopt;
    }
    const char* allowed_chars =
        (version_ == kDraft09 ? kKeyChars09 : kKeyChars13);
    size_t len = input_.find_first_not_of(allowed_chars);
    if (len == base::StringPiece::npos)
      len = input_.size();
    std::string key(input_.substr(0, len));
    input_.remove_prefix(len);
    return key;
  }

  // Parses a Token ([SH09] 4.2.10, [SH13] 4.2.6).
  base::Optional<Item> ReadToken() {
    if (input_.empty() || !base::IsAsciiAlpha(input_.front())) {
      LogParseError("ReadToken", "ALPHA");
      return base::nullopt;
    }
    size_t len = input_.find_first_not_of(kTokenChars);
    if (len == base::StringPiece::npos)
      len = input_.size();
    std::string token(input_.substr(0, len));
    input_.remove_prefix(len);
    return Item(std::move(token), Item::kTokenType);
  }

  // Parses a Number ([SH09] 4.2.8, [SH13] 4.2.4).
  // Currently only supports non-negative integers.
  base::Optional<Item> ReadNumber() {
    size_t i = 0;
    for (; i < input_.size(); ++i) {
      if (!base::IsAsciiDigit(input_[i]))
        break;
    }
    if (i == 0) {
      LogParseError("ReadNumber", "DIGIT");
      return base::nullopt;
    }
    std::string output_number_string(input_.substr(0, i));
    input_.remove_prefix(i);

    // Convert to a 64-bit signed integer, and return if the conversion is
    // successful.
    int64_t n;
    if (!base::StringToInt64(output_number_string, &n))
      return base::nullopt;
    // [SH13] restricts the range of integers further.
    if (version_ == kDraft13 && n > kMaxInteger)
      return base::nullopt;
    return Item(n);
  }

  // Parses a String ([SH09] 4.2.9, [SH13] 4.2.5).
  base::Optional<Item> ReadString() {
    std::string s;
    if (!ConsumeChar('"')) {
      LogParseError("ReadString", "'\"'");
      return base::nullopt;
    }
    while (!ConsumeChar('"')) {
      size_t i = 0;
      for (; i < input_.size(); ++i) {
        if (!IsPrintableASCII(input_[i])) {
          DVLOG(1) << "ReadString: non printable-ASCII character";
          return base::nullopt;
        }
        if (input_[i] == '"' || input_[i] == '\\')
          break;
      }
      if (i == input_.size()) {
        DVLOG(1) << "ReadString: missing closing '\"'";
        return base::nullopt;
      }
      s.append(std::string(input_.substr(0, i)));
      input_.remove_prefix(i);
      if (ConsumeChar('\\')) {
        if (input_.empty()) {
          DVLOG(1) << "ReadString: backslash at string end";
          return base::nullopt;
        }
        if (input_[0] != '"' && input_[0] != '\\') {
          DVLOG(1) << "ReadString: invalid escape";
          return base::nullopt;
        }
        s.push_back(input_.front());
        input_.remove_prefix(1);
      }
    }
    return s;
  }

  // Parses a Byte Sequence ([SH09] 4.2.11, [SH13] 4.2.7).
  base::Optional<Item> ReadByteSequence() {
    if (!ConsumeChar('*')) {
      LogParseError("ReadByteSequence", "'*'");
      return base::nullopt;
    }
    size_t len = input_.find('*');
    if (len == base::StringPiece::npos) {
      DVLOG(1) << "ReadByteSequence: missing closing '*'";
      return base::nullopt;
    }
    std::string base64(input_.substr(0, len));
    // Append the necessary padding characters.
    base64.resize((base64.size() + 3) / 4 * 4, '=');

    std::string binary;
    if (!base::Base64Decode(base64, &binary)) {
      DVLOG(1) << "ReadByteSequence: failed to decode base64: " << base64;
      return base::nullopt;
    }
    input_.remove_prefix(len);
    ConsumeChar('*');
    return Item(std::move(binary), Item::kByteSequenceType);
  }

  void SkipWhitespaces() {
    input_ = base::TrimWhitespaceASCII(input_, base::TRIM_LEADING);
  }

  bool ConsumeChar(char expected) {
    if (!input_.empty() && input_.front() == expected) {
      input_.remove_prefix(1);
      return true;
    }
    return false;
  }

  void LogParseError(const char* func, const char* expected) {
    DVLOG(1) << func << ": " << expected << " expected, got "
             << (input_.empty() ? "'" + input_.substr(0, 1).as_string() + "'"
                                : "EOS");
  }

  base::StringPiece input_;
  DraftVersion version_;

  DISALLOW_COPY_AND_ASSIGN(StructuredHeaderParser);
};

// Serializer for (a subset of) Structured Headers for HTTP defined in [SH13].
// [SH13] https://tools.ietf.org/html/draft-ietf-httpbis-header-structure-13
class StructuredHeaderSerializer {
 public:
  StructuredHeaderSerializer() = default;
  ~StructuredHeaderSerializer() = default;
  StructuredHeaderSerializer(const StructuredHeaderSerializer&) = delete;
  StructuredHeaderSerializer& operator=(const StructuredHeaderSerializer&) =
      delete;

  std::string Output() { return output_.str(); }

  bool WriteList(const List& value) {
    // Serializes a List ([SH13] 4.1.1).
    bool first = true;
    for (const auto& member : value) {
      if (!first)
        output_ << ", ";
      if (!WriteParameterizedMember(member))
        return false;
      first = false;
    }
    return true;
  }

  bool WriteItem(const Item& value) {
    // Serializes an Item ([SH13] 4.1.6).
    if (value.is_string()) {
      output_ << "\"";
      for (const char& c : value.string()) {
        if (!IsPrintableASCII(c))
          return false;
        if (c == '\\' || c == '\"')
          output_ << "\\";
        output_ << c;
      }
      output_ << "\"";
      return true;
    }
    if (value.is_token()) {
      if (!value.string().size() || !base::IsAsciiAlpha(value.string().front()))
        return false;
      if (value.string().find_first_not_of(kTokenChars) != std::string::npos)
        return false;
      output_ << value.string();
      return true;
    }
    if (value.is_byte_sequence()) {
      output_ << "*";
      output_ << base::Base64Encode(
          base::as_bytes(base::make_span(value.string())));
      output_ << "*";
      return true;
    }
    if (value.is_integer()) {
      if (value.integer() > kMaxInteger || value.integer() < kMinInteger)
        return false;
      output_ << value.integer();
      return true;
    }
    return false;
  }

 private:
  bool WriteParameterizedMember(const ParameterizedMember& value) {
    // Serializes a parameterized member ([SH13] 4.1.1).
    if (value.member_is_inner_list) {
      if (!WriteInnerList(value.member))
        return false;
    } else {
      DCHECK_EQ(value.member.size(), 1UL);
      if (!WriteItem(value.member[0]))
        return false;
    }
    return WriteParameters(value.params);
  }

  bool WriteInnerList(const std::vector<Item>& value) {
    // Serializes an inner list ([SH13] 4.1.1.1).
    output_ << "(";
    bool first = true;
    for (const Item& member : value) {
      if (!first)
        output_ << " ";
      if (!WriteItem(member))
        return false;
      first = false;
    }
    output_ << ")";
    return true;
  }

  bool WriteParameters(const ParameterizedMember::Parameters& value) {
    // Serializes a parameter list ([SH13] 4.1.1.2).
    for (const auto& param_name_and_value : value) {
      const std::string& param_name = param_name_and_value.first;
      const Item& param_value = param_name_and_value.second;
      output_ << ";";
      if (!WriteKey(param_name))
        return false;
      if (!param_value.is_null()) {
        output_ << "=";
        if (!WriteItem(param_value))
          return false;
      }
    }
    return true;
  }

  bool WriteKey(const std::string& value) {
    // Serializes a Key ([SH13] 4.1.1.3).
    if (!value.size())
      return false;
    if (value.find_first_not_of(kKeyChars13) != std::string::npos)
      return false;
    output_ << value;
    return true;
  }

  std::ostringstream output_;
};

}  // namespace

Item::Item() {}
Item::Item(const std::string& value, Item::ItemType type)
    : type_(type), string_value_(value) {}
Item::Item(std::string&& value, Item::ItemType type)
    : type_(type), string_value_(std::move(value)) {
  DCHECK(type_ == kStringType || type_ == kTokenType ||
         type_ == kByteSequenceType);
}
Item::Item(int64_t value) : type_(kIntegerType), integer_value_(value) {}

bool operator==(const Item& lhs, const Item& rhs) {
  if (lhs.type_ != rhs.type_)
    return false;
  switch (lhs.type_) {
    case Item::kNullType:
      return true;
    case Item::kStringType:
    case Item::kTokenType:
    case Item::kByteSequenceType:
      return lhs.string_value_ == rhs.string_value_;
    case Item::kIntegerType:
      return lhs.integer_value_ == rhs.integer_value_;
    default:
      NOTREACHED();
      return false;
  }
}

ParameterizedMember::ParameterizedMember(const ParameterizedMember&) = default;
ParameterizedMember& ParameterizedMember::operator=(
    const ParameterizedMember&) = default;
ParameterizedMember::ParameterizedMember(std::vector<Item> id,
                                         bool member_is_inner_list,
                                         const Parameters& ps)
    : member(std::move(id)),
      member_is_inner_list(member_is_inner_list),
      params(ps) {}
ParameterizedMember::ParameterizedMember(std::vector<Item> id,
                                         const Parameters& ps)
    : member(std::move(id)), member_is_inner_list(true), params(ps) {}
ParameterizedMember::ParameterizedMember(Item id, const Parameters& ps)
    : member({std::move(id)}), member_is_inner_list(false), params(ps) {}
ParameterizedMember::~ParameterizedMember() = default;

ParameterisedIdentifier::ParameterisedIdentifier(
    const ParameterisedIdentifier&) = default;
ParameterisedIdentifier& ParameterisedIdentifier::operator=(
    const ParameterisedIdentifier&) = default;
ParameterisedIdentifier::ParameterisedIdentifier(Item id, const Parameters& ps)
    : identifier(std::move(id)), params(ps) {}
ParameterisedIdentifier::~ParameterisedIdentifier() = default;

base::Optional<Item> ParseItem(const base::StringPiece& str) {
  StructuredHeaderParser parser(str, StructuredHeaderParser::kDraft13);
  base::Optional<Item> item = parser.ReadItem();
  if (item && parser.FinishParsing())
    return item;
  return base::nullopt;
}

base::Optional<ParameterisedList> ParseParameterisedList(
    const base::StringPiece& str) {
  StructuredHeaderParser parser(str, StructuredHeaderParser::kDraft09);
  base::Optional<ParameterisedList> param_list = parser.ReadParameterisedList();
  if (param_list && parser.FinishParsing())
    return param_list;
  return base::nullopt;
}

base::Optional<ListOfLists> ParseListOfLists(const base::StringPiece& str) {
  StructuredHeaderParser parser(str, StructuredHeaderParser::kDraft09);
  base::Optional<ListOfLists> list_of_lists = parser.ReadListOfLists();
  if (list_of_lists && parser.FinishParsing())
    return list_of_lists;
  return base::nullopt;
}

base::Optional<List> ParseList(const base::StringPiece& str) {
  StructuredHeaderParser parser(str, StructuredHeaderParser::kDraft13);
  base::Optional<List> list = parser.ReadList();
  if (list && parser.FinishParsing())
    return list;
  return base::nullopt;
}

base::Optional<std::string> SerializeItem(const Item& value) {
  StructuredHeaderSerializer s;
  if (s.WriteItem(value))
    return s.Output();
  return base::nullopt;
}

base::Optional<std::string> SerializeList(const List& value) {
  StructuredHeaderSerializer s;
  if (s.WriteList(value))
    return s.Output();
  return base::nullopt;
}

}  // namespace http_structured_header
}  // namespace blink
