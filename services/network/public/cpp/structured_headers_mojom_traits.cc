// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/structured_headers_mojom_traits.h"

#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "net/http/structured_headers.h"

namespace mojo {

namespace {
using net::structured_headers::InnerListWrapper;
using net::structured_headers::Item;
using net::structured_headers::ParameterizedItem;
using net::structured_headers::ParameterizedMember;
using network::mojom::StructuredHeadersItemDataView;
using network::mojom::StructuredHeadersParameterizedMemberDataView;
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
                  ParameterizedItem>::
    Read(network::mojom::StructuredHeadersParameterizedItemDataView data,
         ParameterizedItem* out) {
  if (!data.ReadItem(&out->item))
    return false;

  if (!data.ReadParameters(&out->params))
    return false;

  return true;
}

// static
StructuredHeadersParameterizedMemberDataView::Tag
UnionTraits<StructuredHeadersParameterizedMemberDataView,
            ParameterizedMember>::GetTag(const ParameterizedMember& in) {
  if (in.GetWithParamsIfItem().has_value()) {
    return StructuredHeadersParameterizedMemberDataView::Tag::kItem;
  }
  if (in.GetWithParamsIfInnerList().has_value()) {
    return StructuredHeadersParameterizedMemberDataView::Tag::kInnerList;
  }
  return StructuredHeadersParameterizedMemberDataView::Tag::kEmpty;
}

// static
ParameterizedItem
UnionTraits<StructuredHeadersParameterizedMemberDataView,
            ParameterizedMember>::item(const ParameterizedMember& in) {
  auto pair = in.GetWithParamsIfItem();
  CHECK(pair.has_value());
  return {pair->first, pair->second};
}

// static
InnerListWrapper
UnionTraits<StructuredHeadersParameterizedMemberDataView,
            ParameterizedMember>::inner_list(const ParameterizedMember& in) {
  auto pair = in.GetWithParamsIfInnerList();
  CHECK(pair.has_value());
  return {pair->first, pair->second};
}

// static
bool UnionTraits<StructuredHeadersParameterizedMemberDataView,
                 ParameterizedMember>::
    Read(StructuredHeadersParameterizedMemberDataView data,
         ParameterizedMember* out) {
  switch (data.tag()) {
    case StructuredHeadersParameterizedMemberDataView::Tag::kEmpty:
      *out = ParameterizedMember();
      return true;
    case StructuredHeadersParameterizedMemberDataView::Tag::kItem: {
      ParameterizedItem item;
      if (!data.ReadItem(&item)) {
        return false;
      }
      *out = ParameterizedMember(std::move(item.item), std::move(item.params));
      return true;
    }
    case StructuredHeadersParameterizedMemberDataView::Tag::kInnerList: {
      InnerListWrapper inner_list;
      if (!data.ReadInnerList(&inner_list)) {
        return false;
      }
      *out = ParameterizedMember(std::move(inner_list.items),
                                 std::move(inner_list.params));
      return true;
    }
  }
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

  ParameterizedMember value;
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

// static
bool StructTraits<network::mojom::StructuredHeadersInnerListDataView,
                  InnerListWrapper>::
    Read(network::mojom::StructuredHeadersInnerListDataView data,
         InnerListWrapper* out) {
  return data.ReadItems(&out->items) && data.ReadParameters(&out->params);
}

}  // namespace mojo
