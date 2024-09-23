// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/platform/json/json_parser.h"

#include "base/memory/raw_ptr.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "third_party/blink/renderer/platform/json/json_values.h"
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
      NOTREACHED_IN_MIGRATION();
      return "";
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
  return "Line: " + String::Number(line) +
         ", column: " + String::Number(column) + ", " + text;
}

// Note: all parsing functions take a |cursor| parameter which is
// where they start parsing from.
// If the parsing succeeds, |cursor| will point to the position
// right after the parsed value, "consuming" some portion of the input.
// If the parsing fails, |cursor| will point to the error position.

template <typename CharType>
struct Cursor {
  int line;
  raw_ptr<const CharType, AllowPtrArithmetic> line_start;
  raw_ptr<const CharType, AllowPtrArithmetic> pos;
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

template <typename CharType>
Error ParseConstToken(Cursor<CharType>* cursor,
                      const CharType* end,
                      const char* token) {
  const CharType* token_start = cursor->pos;
  while (cursor->pos < end && *token != '\0' && *(cursor->pos++) == *token++) {
  }
  if (*token != '\0') {
    cursor->pos = token_start;
    return Error::kSyntaxError;
  }
  return Error::kNoError;
}

template <typename CharType>
Error ReadInt(Cursor<CharType>* cursor,
              const CharType* end,
              bool can_have_leading_zeros) {
  if (cursor->pos == end)
    return Error::kSyntaxError;
  const CharType* start_ptr = cursor->pos;
  bool have_leading_zero = '0' == *(cursor->pos);
  int length = 0;
  while (cursor->pos < end && '0' <= *(cursor->pos) && *(cursor->pos) <= '9') {
    ++(cursor->pos);
    ++length;
  }
  if (!length)
    return Error::kSyntaxError;
  if (!can_have_leading_zeros && length > 1 && have_leading_zero) {
    cursor->pos = start_ptr + 1;
    return Error::kSyntaxError;
  }
  return Error::kNoError;
}

template <typename CharType>
Error ParseNumberToken(Cursor<CharType>* cursor, const CharType* end) {
  // We just grab the number here. We validate the size in DecodeNumber.
  // According to RFC4627, a valid number is: [minus] int [frac] [exp]
  if (cursor->pos == end)
    return Error::kSyntaxError;
  if (*(cursor->pos) == '-')
    ++(cursor->pos);

  Error error = ReadInt(cursor, end, false);
  if (error != Error::kNoError)
    return error;

  if (cursor->pos == end)
    return Error::kNoError;

  // Optional fraction part
  CharType c = *(cursor->pos);
  if ('.' == c) {
    ++(cursor->pos);
    error = ReadInt(cursor, end, true);
    if (error != Error::kNoError)
      return error;
    if (cursor->pos == end)
      return Error::kNoError;
    c = *(cursor->pos);
  }

  // Optional exponent part
  if ('e' == c || 'E' == c) {
    ++(cursor->pos);
    if (cursor->pos == end)
      return Error::kSyntaxError;
    c = *(cursor->pos);
    if ('-' == c || '+' == c) {
      ++(cursor->pos);
      if (cursor->pos == end)
        return Error::kSyntaxError;
    }
    error = ReadInt(cursor, end, true);
    if (error != Error::kNoError)
      return error;
  }

  return Error::kNoError;
}

template <typename CharType>
Error ReadHexDigits(Cursor<CharType>* cursor, const CharType* end, int digits) {
  const CharType* token_start = cursor->pos;
  if (end - cursor->pos < digits)
    return Error::kInvalidEscape;
  for (int i = 0; i < digits; ++i) {
    CharType c = *(cursor->pos)++;
    if (!(('0' <= c && c <= '9') || ('a' <= c && c <= 'f') ||
          ('A' <= c && c <= 'F'))) {
      cursor->pos = token_start;
      return Error::kInvalidEscape;
    }
  }
  return Error::kNoError;
}

template <typename CharType>
Error ParseStringToken(Cursor<CharType>* cursor, const CharType* end) {
  if (cursor->pos == end)
    return Error::kSyntaxError;
  if (*(cursor->pos) != '"')
    return Error::kSyntaxError;
  ++(cursor->pos);
  while (cursor->pos < end) {
    CharType c = *(cursor->pos)++;
    if ('\\' == c) {
      if (cursor->pos == end)
        return Error::kInvalidEscape;
      c = *(cursor->pos)++;
      // Make sure the escaped char is valid.
      switch (c) {
        case 'x': {
          Error error = ReadHexDigits(cursor, end, 2);
          if (error != Error::kNoError)
            return error;
          break;
        }
        case 'u': {
          Error error = ReadHexDigits(cursor, end, 4);
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
Error SkipComment(Cursor<CharType>* cursor, const CharType* end) {
  const CharType* pos = cursor->pos;
  if (pos == end)
    return Error::kSyntaxError;

  if (*pos != '/' || pos + 1 >= end)
    return Error::kSyntaxError;
  ++pos;

  if (*pos == '/') {
    // Single line comment, read to newline.
    for (++pos; pos < end; ++pos) {
      if (*pos == '\n') {
        cursor->line++;
        cursor->pos = pos + 1;
        cursor->line_start = cursor->pos;
        return Error::kNoError;
      }
    }
    cursor->pos = end;
    // Comment reaches end-of-input, which is fine.
    return Error::kNoError;
  }

  if (*pos == '*') {
    CharType previous = '\0';
    // Block comment, read until end marker.
    for (++pos; pos < end; previous = *pos++) {
      if (*pos == '\n') {
        cursor->line++;
        cursor->line_start = pos + 1;
      }
      if (previous == '*' && *pos == '/') {
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
Error SkipWhitespaceAndComments(Cursor<CharType>* cursor,
                                const CharType* end,
                                JSONCommentState& comment_state) {
  while (cursor->pos < end) {
    CharType c = *(cursor->pos);
    if (c == '\n') {
      cursor->line++;
      ++(cursor->pos);
      cursor->line_start = cursor->pos;
    } else if (c == ' ' || c == '\r' || c == '\t') {
      ++(cursor->pos);
    } else if (c == '/' && comment_state != JSONCommentState::kDisallowed) {
      comment_state = JSONCommentState::kAllowedAndPresent;
      Error error = SkipComment(cursor, end);
      if (error != Error::kNoError)
        return error;
    } else {
      break;
    }
  }
  return Error::kNoError;
}

template <typename CharType>
Error ParseToken(Cursor<CharType>* cursor,
                 const CharType* end,
                 Token* token,
                 Cursor<CharType>* token_start,
                 JSONCommentState& comment_state) {
  Error error = SkipWhitespaceAndComments(cursor, end, comment_state);
  if (error != Error::kNoError)
    return error;
  *token_start = *cursor;

  if (cursor->pos == end)
    return Error::kSyntaxError;

  switch (*(cursor->pos)) {
    case 'n':
      *token = kNullToken;
      return ParseConstToken(cursor, end, kJSONNullString);
    case 't':
      *token = kBoolTrue;
      return ParseConstToken(cursor, end, kJSONTrueString);
    case 'f':
      *token = kBoolFalse;
      return ParseConstToken(cursor, end, kJSONFalseString);
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
      return ParseNumberToken(cursor, end);
    case '"':
      *token = kStringLiteral;
      return ParseStringToken(cursor, end);
  }

  return Error::kSyntaxError;
}

template <typename CharType>
inline int HexToInt(CharType c) {
  if ('0' <= c && c <= '9')
    return c - '0';
  if ('A' <= c && c <= 'F')
    return c - 'A' + 10;
  if ('a' <= c && c <= 'f')
    return c - 'a' + 10;
  NOTREACHED_IN_MIGRATION();
  return 0;
}

template <typename CharType>
Error DecodeString(Cursor<CharType>* cursor,
                   const CharType* end,
                   String* output) {
  if (cursor->pos + 1 > end - 1)
    return Error::kSyntaxError;
  if (cursor->pos + 1 == end - 1) {
    *output = "";
    return Error::kNoError;
  }

  const CharType* string_start = cursor->pos;
  StringBuilder buffer;
  buffer.ReserveCapacity(static_cast<wtf_size_t>(end - cursor->pos - 2));

  cursor->pos++;
  while (cursor->pos < end - 1) {
    UChar c = *(cursor->pos)++;
    if (c == '\n') {
      cursor->line++;
      cursor->line_start = cursor->pos;
    }
    if ('\\' != c) {
      buffer.Append(c);
      continue;
    }
    if (cursor->pos == end - 1)
      return Error::kInvalidEscape;
    c = *(cursor->pos)++;

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
        c = (HexToInt(*(cursor->pos)) << 12) +
            (HexToInt(*(cursor->pos + 1)) << 8) +
            (HexToInt(*(cursor->pos + 2)) << 4) + HexToInt(*(cursor->pos + 3));
        cursor->pos += 4;
        break;
      default:
        return Error::kInvalidEscape;
    }
    buffer.Append(c);
  }
  *output = buffer.ToString();

  // Validate constructed utf16 string.
  if (output->Utf8(kStrictUTF8Conversion).empty()) {
    cursor->pos = string_start;
    return Error::kUnsupportedEncoding;
  }
  return Error::kNoError;
}

template <typename CharType>
Error BuildValue(Cursor<CharType>* cursor,
                 const CharType* end,
                 int max_depth,
                 JSONCommentState& comment_state,
                 std::unique_ptr<JSONValue>* result,
                 Vector<String>* duplicate_keys) {
  if (max_depth == 0)
    return Error::kTooMuchNesting;

  Cursor<CharType> token_start;
  Token token;
  Error error = ParseToken(cursor, end, &token, &token_start, comment_state);
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
          base::span<const CharType>(
              token_start.pos.get(),
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
      error = DecodeString(&token_start, cursor->pos.get(), &value);
      if (error != Error::kNoError) {
        *cursor = token_start;
        return error;
      }
      *result = std::make_unique<JSONString>(value);
      break;
    }
    case kArrayBegin: {
      auto array = std::make_unique<JSONArray>();
      Cursor<CharType> before_token = *cursor;
      error = ParseToken(cursor, end, &token, &token_start, comment_state);
      if (error != Error::kNoError)
        return error;
      while (token != kArrayEnd) {
        *cursor = before_token;
        std::unique_ptr<JSONValue> array_node;
        error = BuildValue(cursor, end, max_depth - 1, comment_state,
                           &array_node, duplicate_keys);
        if (error != Error::kNoError)
          return error;
        array->PushValue(std::move(array_node));

        // After a list value, we expect a comma or the end of the list.
        error = ParseToken(cursor, end, &token, &token_start, comment_state);
        if (error != Error::kNoError)
          return error;
        if (token == kListSeparator) {
          before_token = *cursor;
          error = ParseToken(cursor, end, &token, &token_start, comment_state);
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
      auto object = std::make_unique<JSONObject>();
      error = ParseToken(cursor, end, &token, &token_start, comment_state);
      if (error != Error::kNoError)
        return error;
      while (token != kObjectEnd) {
        if (token != kStringLiteral) {
          *cursor = token_start;
          return Error::kUnexpectedToken;
        }
        String key;
        error = DecodeString(&token_start, cursor->pos.get(), &key);
        if (error != Error::kNoError) {
          *cursor = token_start;
          return error;
        }

        error = ParseToken(cursor, end, &token, &token_start, comment_state);
        if (token != kObjectPairSeparator) {
          *cursor = token_start;
          return Error::kUnexpectedToken;
        }

        std::unique_ptr<JSONValue> value;
        error = BuildValue(cursor, end, max_depth - 1, comment_state, &value,
                           duplicate_keys);
        if (error != Error::kNoError)
          return error;
        if (!object->SetValue(key, std::move(value)) &&
            !duplicate_keys->Contains(key)) {
          duplicate_keys->push_back(key);
        }

        // After a key/value pair, we expect a comma or the end of the
        // object.
        error = ParseToken(cursor, end, &token, &token_start, comment_state);
        if (error != Error::kNoError)
          return error;
        if (token == kListSeparator) {
          error = ParseToken(cursor, end, &token, &token_start, comment_state);
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

  return SkipWhitespaceAndComments(cursor, end, comment_state);
}

template <typename CharType>
JSONParseError ParseJSONInternal(const CharType* start_ptr,
                                 unsigned length,
                                 int max_depth,
                                 JSONCommentState& comment_state,
                                 std::unique_ptr<JSONValue>* result) {
  Cursor<CharType> cursor;
  cursor.pos = start_ptr;
  cursor.line = 0;
  cursor.line_start = start_ptr;
  const CharType* end = start_ptr + length;
  JSONParseError error;
  error.type = BuildValue(&cursor, end, max_depth, comment_state, result,
                          &error.duplicate_keys);
  error.line = cursor.line;
  error.column = static_cast<int>(cursor.pos - cursor.line_start);
  if (error.type != Error::kNoError) {
    *result = nullptr;
  } else if (cursor.pos != end) {
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
  } else if (json.Is8Bit()) {
    error = ParseJSONInternal(json.Characters8(), json.length(), max_depth,
                              comment_state, &result);
  } else {
    error = ParseJSONInternal(json.Characters16(), json.length(), max_depth,
                              comment_state, &result);
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
