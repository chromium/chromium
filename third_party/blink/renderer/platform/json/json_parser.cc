// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/json/json_parser.h"

#include "base/memory/raw_ptr.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "third_party/blink/renderer/platform/json/json_values.h"
#include "third_party/blink/renderer/platform/wtf/text/ascii_ctype.h"
#include "third_party/blink/renderer/platform/wtf/text/character_visitor.h"
#include "third_party/blink/renderer/platform/wtf/text/strcat.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/text/string_to_number.h"

namespace blink {

namespace {

const int kMaxStackLimit = 1000;

using Error = JSONParseErrorType;

String FormatErrorMessage(Error error, int line, int column) {
  String text;
  switch (error) {
    case Error::kNoError:
      NOTREACHED();
    case Error::kUnexpectedToken:
      text = "Unexpected token.";
      break;
    case Error::kSyntaxError:
      text = "Syntax error.";
      break;
    case Error::kInvalidEscape:
      text = "Invalid escape sequence.";
      break;
    case Error::kTooMuchNesting:
      text = "Too much nesting.";
      break;
    case Error::kUnexpectedDataAfterRoot:
      text = "Unexpected data after root element.";
      break;
    case Error::kUnsupportedEncoding:
      text =
          "Unsupported encoding. JSON and all string literals must contain "
          "valid Unicode characters.";
      break;
  }
  return StrCat({"Line: ", String::Number(line),
                 ", column: ", String::Number(column), ", ", text});
}

// Note: all parsing functions take a |cursor| parameter which is
// where they start parsing from.
// If the parsing succeeds, |cursor| will point to the position
// right after the parsed value, "consuming" some portion of the input.
// If the parsing fails, |cursor| will point to the error position.

struct Cursor {
  int line;
  size_t line_start;
  size_t pos;
};

enum Token {
  kObjectBegin,
  kObjectEnd,
  kArrayBegin,
  kArrayEnd,
  kStringLiteral,
  kNumber,
  kBoolTrue,
  kBoolFalse,
  kNullToken,
  kListSeparator,
  kObjectPairSeparator,
};

template <typename CharType, size_t N>
Error ParseConstToken(Cursor* cursor,
                      base::span<const CharType> data,
                      const char (&token)[N]) {
  constexpr size_t kTokenLength = N - 1;
  if (data.size() - cursor->pos < kTokenLength) {
    return Error::kSyntaxError;
  }
  auto span_to_match = data.subspan(cursor->pos).template first<kTokenLength>();
  if (span_to_match != base::span(token).template first<kTokenLength>()) {
    return Error::kSyntaxError;
  }
  cursor->pos += kTokenLength;
  return Error::kNoError;
}

template <typename CharType>
Error ReadInt(Cursor* cursor,
              base::span<const CharType> data,
              bool can_have_leading_zeros) {
  if (cursor->pos == data.size()) {
    return Error::kSyntaxError;
  }
  const size_t start_pos = cursor->pos;
  bool have_leading_zero = '0' == data[cursor->pos];
  while (cursor->pos < data.size() && IsASCIIDigit(data[cursor->pos])) {
    ++(cursor->pos);
  }
  const size_t length = cursor->pos - start_pos;
  if (!length) {
    return Error::kSyntaxError;
  }
  if (!can_have_leading_zeros && length > 1 && have_leading_zero) {
    cursor->pos = start_pos + 1;
    return Error::kSyntaxError;
  }
  return Error::kNoError;
}

template <typename CharType>
Error ParseNumberToken(Cursor* cursor, base::span<const CharType> data) {
  // We just grab the number here. We validate the size in DecodeNumber.
  // According to RFC4627, a valid number is: [minus] int [frac] [exp]
  if (cursor->pos == data.size()) {
    return Error::kSyntaxError;
  }
  if (data[cursor->pos] == '-') {
    ++(cursor->pos);
  }

  Error error = ReadInt(cursor, data, false);
  if (error != Error::kNoError)
    return error;

  if (cursor->pos == data.size()) {
    return Error::kNoError;
  }

  // Optional fraction part
  CharType c = data[cursor->pos];
  if ('.' == c) {
    ++(cursor->pos);
    error = ReadInt(cursor, data, true);
    if (error != Error::kNoError)
      return error;
    if (cursor->pos == data.size()) {
      return Error::kNoError;
    }
    c = data[cursor->pos];
  }

  // Optional exponent part
  if ('e' == c || 'E' == c) {
    ++(cursor->pos);
    if (cursor->pos == data.size()) {
      return Error::kSyntaxError;
    }
    c = data[cursor->pos];
    if ('-' == c || '+' == c) {
      ++(cursor->pos);
      if (cursor->pos == data.size()) {
        return Error::kSyntaxError;
      }
    }
    error = ReadInt(cursor, data, true);
    if (error != Error::kNoError)
      return error;
  }

  return Error::kNoError;
}

template <typename CharType>
Error ReadHexDigits(Cursor* cursor,
                    base::span<const CharType> data,
                    size_t digits) {
  const size_t token_start = cursor->pos;
  if (data.size() - cursor->pos < digits) {
    return Error::kInvalidEscape;
  }
  for (size_t i = 0; i < digits; ++i) {
    if (!IsASCIIHexDigit(data[cursor->pos++])) {
      cursor->pos = token_start;
      return Error::kInvalidEscape;
    }
  }
  return Error::kNoError;
}

template <typename CharType>
Error ParseStringToken(Cursor* cursor, base::span<const CharType> data) {
  if (cursor->pos == data.size()) {
    return Error::kSyntaxError;
  }
  if (data[cursor->pos] != '"') {
    return Error::kSyntaxError;
  }
  ++(cursor->pos);
  while (cursor->pos < data.size()) {
    CharType c = data[cursor->pos++];
    if ('\\' == c) {
      if (cursor->pos == data.size()) {
        return Error::kInvalidEscape;
      }
      c = data[cursor->pos++];
      // Make sure the escaped char is valid.
      switch (c) {
        case 'x': {
          Error error = ReadHexDigits(cursor, data, 2);
          if (error != Error::kNoError)
            return error;
          break;
        }
        case 'u': {
          Error error = ReadHexDigits(cursor, data, 4);
          if (error != Error::kNoError)
            return error;
          break;
        }
        case '\\':
        case '/':
        case 'b':
        case 'f':
        case 'n':
        case 'r':
        case 't':
        case 'v':
        case '"':
          break;
        default:
          return Error::kInvalidEscape;
      }
    } else if (c < 0x20) {
      return Error::kSyntaxError;
    } else if ('"' == c) {
      return Error::kNoError;
    }
  }
  return Error::kSyntaxError;
}

template <typename CharType>
Error SkipComment(Cursor* cursor, base::span<const CharType> data) {
  size_t pos = cursor->pos;
  if (pos == data.size()) {
    return Error::kSyntaxError;
  }

  if (data[pos] != '/' || pos + 1 >= data.size()) {
    return Error::kSyntaxError;
  }
  ++pos;

  if (data[pos] == '/') {
    // Single line comment, read to newline.
    for (++pos; pos < data.size(); ++pos) {
      if (data[pos] == '\n') {
        cursor->line++;
        cursor->pos = pos + 1;
        cursor->line_start = cursor->pos;
        return Error::kNoError;
      }
    }
    cursor->pos = data.size();
    // Comment reaches end-of-input, which is fine.
    return Error::kNoError;
  }

  if (data[pos] == '*') {
    CharType previous = '\0';
    // Block comment, read until end marker.
    for (++pos; pos < data.size(); previous = data[pos++]) {
      if (data[pos] == '\n') {
        cursor->line++;
        cursor->line_start = pos + 1;
      }
      if (previous == '*' && data[pos] == '/') {
        cursor->pos = pos + 1;
        return Error::kNoError;
      }
    }
    // Block comment must close before end-of-input.
    return Error::kSyntaxError;
  }

  return Error::kSyntaxError;
}

template <typename CharType>
Error SkipWhitespaceAndComments(Cursor* cursor,
                                base::span<const CharType> data,
                                JSONCommentState& comment_state) {
  while (cursor->pos < data.size()) {
    const CharType c = data[cursor->pos];
    if (c == '\n') {
      cursor->line++;
      ++(cursor->pos);
      cursor->line_start = cursor->pos;
    } else if (c == ' ' || c == '\r' || c == '\t') {
      ++(cursor->pos);
    } else if (c == '/' && comment_state != JSONCommentState::kDisallowed) {
      comment_state = JSONCommentState::kAllowedAndPresent;
      Error error = SkipComment(cursor, data);
      if (error != Error::kNoError)
        return error;
    } else {
      break;
    }
  }
  return Error::kNoError;
}

template <typename CharType>
Error ParseToken(Cursor* cursor,
                 base::span<const CharType> data,
                 Token* token,
                 Cursor* token_start,
                 JSONCommentState& comment_state) {
  Error error = SkipWhitespaceAndComments(cursor, data, comment_state);
  if (error != Error::kNoError)
    return error;
  *token_start = *cursor;

  if (cursor->pos == data.size()) {
    return Error::kSyntaxError;
  }

  switch (data[cursor->pos]) {
    case 'n':
      *token = kNullToken;
      return ParseConstToken(cursor, data, kJSONNullString);
    case 't':
      *token = kBoolTrue;
      return ParseConstToken(cursor, data, kJSONTrueString);
    case 'f':
      *token = kBoolFalse;
      return ParseConstToken(cursor, data, kJSONFalseString);
    case '[':
      ++(cursor->pos);
      *token = kArrayBegin;
      return Error::kNoError;
    case ']':
      ++(cursor->pos);
      *token = kArrayEnd;
      return Error::kNoError;
    case ',':
      ++(cursor->pos);
      *token = kListSeparator;
      return Error::kNoError;
    case '{':
      ++(cursor->pos);
      *token = kObjectBegin;
      return Error::kNoError;
    case '}':
      ++(cursor->pos);
      *token = kObjectEnd;
      return Error::kNoError;
    case ':':
      ++(cursor->pos);
      *token = kObjectPairSeparator;
      return Error::kNoError;
    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
    case '-':
      *token = kNumber;
      return ParseNumberToken(cursor, data);
    case '"':
      *token = kStringLiteral;
      return ParseStringToken(cursor, data);
  }

  return Error::kSyntaxError;
}

template <typename CharType>
Error DecodeString(Cursor* cursor,
                   base::span<const CharType> data,
                   size_t string_end,
                   String* output) {
  if (cursor->pos + 1 > string_end - 1) {
    return Error::kSyntaxError;
  }
  if (cursor->pos + 1 == string_end - 1) {
    *output = g_empty_string;
    return Error::kNoError;
  }

  const size_t string_start = cursor->pos;
  StringBuilder buffer;
  buffer.ReserveCapacity(static_cast<wtf_size_t>(string_end - cursor->pos - 2));

  cursor->pos++;
  while (cursor->pos < string_end - 1) {
    UChar c = data[cursor->pos++];
    if (c == '\n') {
      cursor->line++;
      cursor->line_start = cursor->pos;
    }
    if ('\\' != c) {
      buffer.Append(c);
      continue;
    }
    if (cursor->pos == string_end - 1) {
      return Error::kInvalidEscape;
    }
    c = data[cursor->pos++];

    if (c == 'x') {
      // \x is not supported.
      return Error::kInvalidEscape;
    }

    switch (c) {
      case '"':
      case '/':
      case '\\':
        break;
      case 'b':
        c = '\b';
        break;
      case 'f':
        c = '\f';
        break;
      case 'n':
        c = '\n';
        break;
      case 'r':
        c = '\r';
        break;
      case 't':
        c = '\t';
        break;
      case 'v':
        c = '\v';
        break;
      case 'u':
        c = (ToASCIIHexValue(data[cursor->pos]) << 12) +
            (ToASCIIHexValue(data[cursor->pos + 1]) << 8) +
            (ToASCIIHexValue(data[cursor->pos + 2]) << 4) +
            ToASCIIHexValue(data[cursor->pos + 3]);
        cursor->pos += 4;
        break;
      default:
        return Error::kInvalidEscape;
    }
    buffer.Append(c);
  }
  *output = buffer.ToString();

  // Validate constructed UTF-16 string.
  if (output->Utf8(Utf8ConversionMode::kStrict).empty()) {
    cursor->pos = string_start;
    return Error::kUnsupportedEncoding;
  }
  return Error::kNoError;
}

template <typename CharType>
Error BuildValue(Cursor* cursor,
                 base::span<const CharType> data,
                 int max_depth,
                 JSONCommentState& comment_state,
                 std::unique_ptr<JSONValue>* result,
                 Vector<String>* duplicate_keys) {
  if (max_depth == 0)
    return Error::kTooMuchNesting;

  Cursor token_start;
  Token token;
  Error error = ParseToken(cursor, data, &token, &token_start, comment_state);
  if (error != Error::kNoError)
    return error;

  switch (token) {
    case kNullToken:
      *result = JSONValue::Null();
      break;
    case kBoolTrue:
      *result = std::make_unique<JSONBasicValue>(true);
      break;
    case kBoolFalse:
      *result = std::make_unique<JSONBasicValue>(false);
      break;
    case kNumber: {
      bool ok;
      double value = CharactersToDouble(
          data.subspan(token_start.pos,
                       static_cast<size_t>(cursor->pos - token_start.pos)),
          &ok);
      if (!ok || std::isinf(value)) {
        *cursor = token_start;
        return Error::kSyntaxError;
      }
      if (base::IsValueInRangeForNumericType<int>(value) &&
          static_cast<int>(value) == value)
        *result = std::make_unique<JSONBasicValue>(static_cast<int>(value));
      else
        *result = std::make_unique<JSONBasicValue>(value);
      break;
    }
    case kStringLiteral: {
      String value;
      error = DecodeString(&token_start, data, cursor->pos, &value);
      if (error != Error::kNoError) {
        *cursor = token_start;
        return error;
      }
      *result = std::make_unique<JSONString>(value);
      break;
    }
    case kArrayBegin: {
      Cursor before_token = *cursor;
      error = ParseToken(cursor, data, &token, &token_start, comment_state);
      if (error != Error::kNoError)
        return error;
      auto array = std::make_unique<JSONArray>();
      while (token != kArrayEnd) {
        *cursor = before_token;
        std::unique_ptr<JSONValue> array_node;
        error = BuildValue(cursor, data, max_depth - 1, comment_state,
                           &array_node, duplicate_keys);
        if (error != Error::kNoError)
          return error;
        array->PushValue(std::move(array_node));

        // After a list value, we expect a comma or the end of the list.
        error = ParseToken(cursor, data, &token, &token_start, comment_state);
        if (error != Error::kNoError)
          return error;
        if (token == kListSeparator) {
          before_token = *cursor;
          error = ParseToken(cursor, data, &token, &token_start, comment_state);
          if (error != Error::kNoError)
            return error;
          if (token == kArrayEnd) {
            *cursor = token_start;
            return Error::kUnexpectedToken;
          }
        } else if (token != kArrayEnd) {
          // Unexpected value after list value. Bail out.
          *cursor = token_start;
          return Error::kUnexpectedToken;
        }
      }
      if (token != kArrayEnd) {
        *cursor = token_start;
        return Error::kUnexpectedToken;
      }
      *result = std::move(array);
      break;
    }
    case kObjectBegin: {
      error = ParseToken(cursor, data, &token, &token_start, comment_state);
      if (error != Error::kNoError)
        return error;
      auto object = std::make_unique<JSONObject>();
      while (token != kObjectEnd) {
        if (token != kStringLiteral) {
          *cursor = token_start;
          return Error::kUnexpectedToken;
        }
        String key;
        error = DecodeString(&token_start, data, cursor->pos, &key);
        if (error != Error::kNoError) {
          *cursor = token_start;
          return error;
        }

        error = ParseToken(cursor, data, &token, &token_start, comment_state);
        if (token != kObjectPairSeparator) {
          *cursor = token_start;
          return Error::kUnexpectedToken;
        }

        std::unique_ptr<JSONValue> value;
        error = BuildValue(cursor, data, max_depth - 1, comment_state, &value,
                           duplicate_keys);
        if (error != Error::kNoError)
          return error;
        if (!object->SetValue(key, std::move(value)) &&
            !duplicate_keys->Contains(key)) {
          duplicate_keys->push_back(key);
        }

        // After a key/value pair, we expect a comma or the end of the
        // object.
        error = ParseToken(cursor, data, &token, &token_start, comment_state);
        if (error != Error::kNoError)
          return error;
        if (token == kListSeparator) {
          error = ParseToken(cursor, data, &token, &token_start, comment_state);
          if (error != Error::kNoError)
            return error;
          if (token == kObjectEnd) {
            *cursor = token_start;
            return Error::kUnexpectedToken;
          }
        } else if (token != kObjectEnd) {
          // Unexpected value after last object value. Bail out.
          *cursor = token_start;
          return Error::kUnexpectedToken;
        }
      }
      if (token != kObjectEnd) {
        *cursor = token_start;
        return Error::kUnexpectedToken;
      }
      *result = std::move(object);
      break;
    }

    default:
      // We got a token that's not a value.
      *cursor = token_start;
      return Error::kUnexpectedToken;
  }

  return SkipWhitespaceAndComments(cursor, data, comment_state);
}

template <typename CharType>
JSONParseError ParseJSONInternal(base::span<const CharType> json,
                                 int max_depth,
                                 JSONCommentState& comment_state,
                                 std::unique_ptr<JSONValue>* result) {
  Cursor cursor;
  cursor.pos = 0;
  cursor.line = 0;
  cursor.line_start = 0;

  JSONParseError error;
  error.type = BuildValue(&cursor, json, max_depth, comment_state, result,
                          &error.duplicate_keys);
  error.line = cursor.line;
  error.column = static_cast<int>(cursor.pos - cursor.line_start);
  if (error.type != Error::kNoError) {
    *result = nullptr;
  } else if (cursor.pos != json.size()) {
    error.type = Error::kUnexpectedDataAfterRoot;
    *result = nullptr;
  }
  return error;
}

}  // anonymous namespace

std::unique_ptr<JSONValue> ParseJSON(const String& json,
                                     JSONParseError* opt_error) {
  JSONCommentState comments = JSONCommentState::kDisallowed;
  auto result = ParseJSON(json, comments, kMaxStackLimit, opt_error);
  DCHECK_EQ(comments, JSONCommentState::kDisallowed);
  return result;
}

std::unique_ptr<JSONValue> ParseJSONWithCommentsDeprecated(
    const String& json,
    JSONParseError* opt_error,
    bool* opt_has_comments) {
  JSONCommentState comment_state = JSONCommentState::kAllowedButAbsent;
  auto result = ParseJSON(json, comment_state, kMaxStackLimit, opt_error);
  if (opt_has_comments) {
    *opt_has_comments = (comment_state == JSONCommentState::kAllowedAndPresent);
  }
  return result;
}

std::unique_ptr<JSONValue> ParseJSON(const String& json,
                                     JSONCommentState& comment_state,
                                     int max_depth,
                                     JSONParseError* opt_error) {
  if (max_depth < 0)
    max_depth = 0;
  if (max_depth > kMaxStackLimit)
    max_depth = kMaxStackLimit;

  std::unique_ptr<JSONValue> result;
  JSONParseError error;

  if (json.empty()) {
    error.type = Error::kSyntaxError;
    error.line = 0;
    error.column = 0;
  } else {
    error = VisitCharacters(json, [&](auto chars) {
      return ParseJSONInternal(chars, max_depth, comment_state, &result);
    });
  }

  if (opt_error) {
    error.line++;
    error.column++;
    if (error.type != Error::kNoError)
      error.message = FormatErrorMessage(error.type, error.line, error.column);
    *opt_error = error;
  }
  return result;
}

}  // namespace blink
