// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web/model/choose_file/choose_file_util.h"

#import "base/strings/string_split.h"
#import "base/strings/string_util.h"

namespace {

// Returns true if `ch` is a valid RFC 2616 token character.
bool IsRFC2616TokenCharacter(char ch) {
  if (ch < 32 || ch > 127) {
    return false;
  }
  switch (ch) {
    case '(':
    case ')':
    case '<':
    case '>':
    case '@':
    case ',':
    case ';':
    case ':':
    case '\\':
    case '"':
    case '/':
    case '[':
    case ']':
    case '?':
    case '=':
    case '{':
    case '}':
    case 0x20:  // SP
    case 0x09:  // HT
    case 0x7f:  // DEL
      return false;
    default:
      return true;
  }
}

// Returns true if `type` is a valid MIME type.
bool IsValidMimeType(std::string_view type) {
  std::string_view::size_type slash_position = type.find('/');
  if (slash_position == std::string_view::npos || slash_position == 0 ||
      slash_position == type.length() - 1) {
    return false;
  }
  for (std::string_view::size_type i = 0; i < type.length(); ++i) {
    if (!IsRFC2616TokenCharacter(type[i]) && i != slash_position) {
      return false;
    }
  }
  return true;
}

// Returns true if `type` is a valid file extension.
bool IsValidFileExtension(std::string_view type) {
  if (type.length() < 2) {
    return false;
  }
  if (type[0] != '.') {
    return false;
  }
  for (const char ch : type) {
    if (!IsRFC2616TokenCharacter(ch)) {
      return false;
    }
  }
  return true;
}

// Returns accept attribute types that satisfy the `predicate` in
// `accept_string`.
std::vector<std::string> ParseAcceptAttribute(
    std::string_view accept_string,
    bool (*predicate)(std::string_view)) {
  std::vector<std::string> types;
  if (accept_string.empty()) {
    return types;
  }

  std::vector<std::string_view> split_types = base::SplitStringPiece(
      accept_string, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  for (std::string_view split_type : split_types) {
    if (!predicate(split_type)) {
      continue;
    }
    types.push_back(base::ToLowerASCII(split_type));
  }

  return types;
}

}  // namespace

std::vector<std::string> ParseAcceptAttributeMimeTypes(
    std::string_view accept_attribute) {
  return ParseAcceptAttribute(accept_attribute, IsValidMimeType);
}

std::vector<std::string> ParseAcceptAttributeFileExtensions(
    std::string_view accept_attribute) {
  return ParseAcceptAttribute(accept_attribute, IsValidFileExtension);
}
