// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_STRUCTURED_HEADERS_MOJOM_TRAITS_H_
#define SERVICES_NETWORK_PUBLIC_CPP_STRUCTURED_HEADERS_MOJOM_TRAITS_H_

#include <stdint.h>

#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/component_export.h"
#include "mojo/public/cpp/base/byte_string_mojom_traits.h"
#include "mojo/public/cpp/bindings/struct_traits.h"
#include "mojo/public/cpp/bindings/union_traits.h"
#include "net/http/structured_headers.h"
#include "services/network/public/mojom/structured_headers.mojom-shared.h"

namespace mojo {

template <>
struct COMPONENT_EXPORT(NETWORK_CPP_STRUCTURED_HEADERS)
    UnionTraits<network::mojom::StructuredHeadersItemDataView,
                net::structured_headers::Item> {
  static network::mojom::StructuredHeadersItemDataView::Tag GetTag(
      const net::structured_headers::Item&);

  static uint8_t null_value(const net::structured_headers::Item&) { return 0; }

  static int64_t integer_value(const net::structured_headers::Item& item) {
    return item.GetInteger();
  }

  static double decimal_value(const net::structured_headers::Item& item) {
    return item.GetDecimal();
  }

  static std::string_view string_value(
      const net::structured_headers::Item& item) {
    return item.GetString();
  }

  static std::string_view token_value(
      const net::structured_headers::Item& item) {
    return item.GetString();
  }

  static const std::string& byte_sequence_value(
      const net::structured_headers::Item& item) {
    return item.GetString();
  }

  static bool boolean_value(const net::structured_headers::Item& item) {
    return item.GetBoolean();
  }

  static bool Read(network::mojom::StructuredHeadersItemDataView,
                   net::structured_headers::Item* out);
};

template <>
struct COMPONENT_EXPORT(NETWORK_CPP_STRUCTURED_HEADERS)
    StructTraits<network::mojom::StructuredHeadersParameterDataView,
                 std::pair<std::string, net::structured_headers::Item>> {
  static std::string_view key(
      const std::pair<std::string, net::structured_headers::Item>& param) {
    return param.first;
  }

  static const net::structured_headers::Item& item(
      const std::pair<std::string, net::structured_headers::Item>& param) {
    return param.second;
  }

  static bool Read(network::mojom::StructuredHeadersParameterDataView,
                   std::pair<std::string, net::structured_headers::Item>* out);
};

template <>
struct COMPONENT_EXPORT(NETWORK_CPP_STRUCTURED_HEADERS)
    StructTraits<network::mojom::StructuredHeadersParameterizedItemDataView,
                 net::structured_headers::ParameterizedItem> {
  static const net::structured_headers::Item& item(
      const net::structured_headers::ParameterizedItem& item) {
    return item.item;
  }

  static const std::vector<
      std::pair<std::string, net::structured_headers::Item>>&
  parameters(const net::structured_headers::ParameterizedItem& item) {
    return item.params;
  }

  static bool Read(network::mojom::StructuredHeadersParameterizedItemDataView,
                   net::structured_headers::ParameterizedItem* out);
};

template <>
struct COMPONENT_EXPORT(NETWORK_CPP_STRUCTURED_HEADERS)
    StructTraits<network::mojom::StructuredHeadersParameterizedMemberDataView,
                 net::structured_headers::ParameterizedMember> {
  static const std::vector<net::structured_headers::ParameterizedItem>& member(
      const net::structured_headers::ParameterizedMember& in) {
    return in.member;
  }

  static bool member_is_inner_list(
      const net::structured_headers::ParameterizedMember& in) {
    return in.member_is_inner_list;
  }

  static const std::vector<
      std::pair<std::string, net::structured_headers::Item>>&
  parameters(const net::structured_headers::ParameterizedMember& in) {
    return in.params;
  }

  static bool Read(network::mojom::StructuredHeadersParameterizedMemberDataView,
                   net::structured_headers::ParameterizedMember* out);
};

template <>
struct COMPONENT_EXPORT(NETWORK_CPP_STRUCTURED_HEADERS)
    StructTraits<network::mojom::StructuredHeadersDictionaryMemberDataView,
                 net::structured_headers::DictionaryMember> {
  static const std::string& key(
      const net::structured_headers::DictionaryMember& in) {
    return in.first;
  }

  static const net::structured_headers::ParameterizedMember& value(
      const net::structured_headers::DictionaryMember& in) {
    return in.second;
  }

  static bool Read(network::mojom::StructuredHeadersDictionaryMemberDataView,
                   net::structured_headers::DictionaryMember* out);
};

template <>
struct COMPONENT_EXPORT(NETWORK_CPP_STRUCTURED_HEADERS)
    StructTraits<network::mojom::StructuredHeadersDictionaryDataView,
                 net::structured_headers::Dictionary> {
  static std::vector<net::structured_headers::DictionaryMember> members(
      const net::structured_headers::Dictionary& in);

  static bool Read(network::mojom::StructuredHeadersDictionaryDataView,
                   net::structured_headers::Dictionary* out);
};

}  // namespace mojo

#endif  // SERVICES_NETWORK_PUBLIC_CPP_STRUCTURED_HEADERS_MOJOM_TRAITS_H_
