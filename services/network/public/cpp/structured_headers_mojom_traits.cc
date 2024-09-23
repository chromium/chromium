// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/structured_headers_mojom_traits.h"

#include <string>
#include <utility>
#include <vector>

#include "net/http/structured_headers.h"

namespace mojo {

namespace {
using net::structured_headers::Item;
using network::mojom::StructuredHeadersItemDataView;
}  // namespace

// static
StructuredHeadersItemDataView::Tag
UnionTraits<StructuredHeadersItemDataView, Item>::GetTag(const Item& item) {
  switch (item.Type()) {
    case Item::kNullType:
      return StructuredHeadersItemDataView::Tag::kNullValue;
    case Item::kIntegerType:
      return StructuredHeadersItemDataView::Tag::kIntegerValue;
    case Item::kDecimalType:
      return StructuredHeadersItemDataView::Tag::kDecimalValue;
    case Item::kStringType:
      return StructuredHeadersItemDataView::Tag::kStringValue;
    case Item::kTokenType:
      return StructuredHeadersItemDataView::Tag::kTokenValue;
    case Item::kByteSequenceType:
      return StructuredHeadersItemDataView::Tag::kByteSequenceValue;
    case Item::kBooleanType:
      return StructuredHeadersItemDataView::Tag::kBooleanValue;
  }
}

// static
bool UnionTraits<StructuredHeadersItemDataView, Item>::Read(
    StructuredHeadersItemDataView data,
    net::structured_headers::Item* out) {
  switch (data.tag()) {
    case StructuredHeadersItemDataView::Tag::kNullValue:
      *out = Item();
      return true;
    case StructuredHeadersItemDataView::Tag::kIntegerValue:
      *out = Item(data.integer_value());
      return true;
    case StructuredHeadersItemDataView::Tag::kDecimalValue:
      *out = Item(data.decimal_value());
      return true;
    case StructuredHeadersItemDataView::Tag::kStringValue: {
      std::string value;
      if (!data.ReadStringValue(&value))
        return false;
      *out = Item(std::move(value), Item::kStringType);
      return true;
    }
    case StructuredHeadersItemDataView::Tag::kTokenValue: {
      std::string value;
      if (!data.ReadTokenValue(&value))
        return false;
      *out = Item(std::move(value), Item::kTokenType);
      return true;
    }
    case StructuredHeadersItemDataView::Tag::kByteSequenceValue: {
      std::string value;
      if (!data.ReadByteSequenceValue(&value))
        return false;
      *out = Item(std::move(value), Item::kByteSequenceType);
      return true;
    }
    case StructuredHeadersItemDataView::Tag::kBooleanValue:
      *out = Item(data.boolean_value());
      return true;
  }
}

// static
bool StructTraits<network::mojom::StructuredHeadersParameterDataView,
                  std::pair<std::string, Item>>::
    Read(network::mojom::StructuredHeadersParameterDataView data,
         std::pair<std::string, Item>* out) {
  if (!data.ReadKey(&out->first))
    return false;

  if (!data.ReadItem(&out->second))
    return false;

  return true;
}

// static
bool StructTraits<network::mojom::StructuredHeadersParameterizedItemDataView,
                  net::structured_headers::ParameterizedItem>::
    Read(network::mojom::StructuredHeadersParameterizedItemDataView data,
         net::structured_headers::ParameterizedItem* out) {
  if (!data.ReadItem(&out->item))
    return false;

  if (!data.ReadParameters(&out->params))
    return false;

  return true;
}

// static
bool StructTraits<network::mojom::StructuredHeadersParameterizedMemberDataView,
                  net::structured_headers::ParameterizedMember>::
    Read(network::mojom::StructuredHeadersParameterizedMemberDataView data,
         net::structured_headers::ParameterizedMember* out) {
  if (!data.ReadMember(&out->member)) {
    return false;
  }

  out->member_is_inner_list = data.member_is_inner_list();

  if (!data.ReadParameters(&out->params)) {
    return false;
  }

  return true;
}

// static
bool StructTraits<network::mojom::StructuredHeadersDictionaryMemberDataView,
                  net::structured_headers::DictionaryMember>::
    Read(network::mojom::StructuredHeadersDictionaryMemberDataView data,
         net::structured_headers::DictionaryMember* out) {
  std::string key;
  if (!data.ReadKey(&key)) {
    return false;
  }

  net::structured_headers::ParameterizedMember value;
  if (!data.ReadValue(&value)) {
    return false;
  }

  *out = std::make_pair(std::move(key), std::move(value));
  return true;
}

// static
std::vector<net::structured_headers::DictionaryMember>
StructTraits<network::mojom::StructuredHeadersDictionaryDataView,
             net::structured_headers::Dictionary>::
    members(const net::structured_headers::Dictionary& in) {
  return std::vector<net::structured_headers::DictionaryMember>(in.begin(),
                                                                in.end());
}

// static
bool StructTraits<network::mojom::StructuredHeadersDictionaryDataView,
                  net::structured_headers::Dictionary>::
    Read(network::mojom::StructuredHeadersDictionaryDataView data,
         net::structured_headers::Dictionary* out) {
  std::vector<net::structured_headers::DictionaryMember> members;
  if (!data.ReadMembers(&members)) {
    return false;
  }

  *out = net::structured_headers::Dictionary(std::move(members));
  return true;
}

}  // namespace mojo
