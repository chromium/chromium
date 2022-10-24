// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_STRUCTURED_HEADERS_H_
#define NET_HTTP_STRUCTURED_HEADERS_H_

#include <string>

#include "base/strings/abseil_string_conversions.h"
#include "base/strings/string_piece.h"
#include "net/third_party/quiche/src/quiche/common/structured_headers.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace net::structured_headers {

using Item = quiche::structured_headers::Item;
using ParameterisedIdentifier =
    quiche::structured_headers::ParameterisedIdentifier;
using ParameterizedItem = quiche::structured_headers::ParameterizedItem;
using ParameterizedMember = quiche::structured_headers::ParameterizedMember;
using DictionaryMember = quiche::structured_headers::DictionaryMember;
using Dictionary = quiche::structured_headers::Dictionary;
using ParameterisedList = quiche::structured_headers::ParameterisedList;
using ListOfLists = quiche::structured_headers::ListOfLists;
using List = quiche::structured_headers::List;
using Parameters = quiche::structured_headers::Parameters;

inline absl::optional<ParameterizedItem> ParseItem(base::StringPiece str) {
  return quiche::structured_headers::ParseItem(
      base::StringPieceToStringView(str));
}
inline absl::optional<Item> ParseBareItem(base::StringPiece str) {
  return quiche::structured_headers::ParseBareItem(
      base::StringPieceToStringView(str));
}
inline absl::optional<ParameterisedList> ParseParameterisedList(
    base::StringPiece str) {
  return quiche::structured_headers::ParseParameterisedList(
      base::StringPieceToStringView(str));
}
inline absl::optional<ListOfLists> ParseListOfLists(base::StringPiece str) {
  return quiche::structured_headers::ParseListOfLists(
      base::StringPieceToStringView(str));
}
inline absl::optional<List> ParseList(base::StringPiece str) {
  return quiche::structured_headers::ParseList(
      base::StringPieceToStringView(str));
}
inline absl::optional<Dictionary> ParseDictionary(base::StringPiece str) {
  return quiche::structured_headers::ParseDictionary(
      base::StringPieceToStringView(str));
}

inline absl::optional<std::string> SerializeItem(const Item& value) {
  return quiche::structured_headers::SerializeItem(value);
}
inline absl::optional<std::string> SerializeItem(
    const ParameterizedItem& value) {
  return quiche::structured_headers::SerializeItem(value);
}
inline absl::optional<std::string> SerializeList(const List& value) {
  return quiche::structured_headers::SerializeList(value);
}
inline absl::optional<std::string> SerializeDictionary(
    const Dictionary& value) {
  return quiche::structured_headers::SerializeDictionary(value);
}

}  // namespace net::structured_headers

#endif  // NET_HTTP_STRUCTURED_HEADERS_H_
