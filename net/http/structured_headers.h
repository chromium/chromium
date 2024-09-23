// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_STRUCTURED_HEADERS_H_
#define NET_HTTP_STRUCTURED_HEADERS_H_

#include <optional>
#include <string>
#include <string_view>

#include "net/third_party/quiche/src/quiche/common/structured_headers.h"

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

inline std::optional<ParameterizedItem> ParseItem(std::string_view str) {
  return quiche::structured_headers::ParseItem(str);
}
inline std::optional<Item> ParseBareItem(std::string_view str) {
  return quiche::structured_headers::ParseBareItem(str);
}
inline std::optional<ParameterisedList> ParseParameterisedList(
    std::string_view str) {
  return quiche::structured_headers::ParseParameterisedList(str);
}
inline std::optional<ListOfLists> ParseListOfLists(std::string_view str) {
  return quiche::structured_headers::ParseListOfLists(str);
}
inline std::optional<List> ParseList(std::string_view str) {
  return quiche::structured_headers::ParseList(str);
}
inline std::optional<Dictionary> ParseDictionary(std::string_view str) {
  return quiche::structured_headers::ParseDictionary(str);
}

inline std::optional<std::string> SerializeItem(const Item& value) {
  return quiche::structured_headers::SerializeItem(value);
}
inline std::optional<std::string> SerializeItem(
    const ParameterizedItem& value) {
  return quiche::structured_headers::SerializeItem(value);
}
inline std::optional<std::string> SerializeList(const List& value) {
  return quiche::structured_headers::SerializeList(value);
}
inline std::optional<std::string> SerializeDictionary(const Dictionary& value) {
  return quiche::structured_headers::SerializeDictionary(value);
}

inline std::string_view ItemTypeToString(
    structured_headers::Item::ItemType type) {
  return quiche::structured_headers::ItemTypeToString(type);
}

}  // namespace net::structured_headers

#endif  // NET_HTTP_STRUCTURED_HEADERS_H_
