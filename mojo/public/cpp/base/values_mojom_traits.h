// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BASE_VALUES_MOJOM_TRAITS_H_
#define MOJO_PUBLIC_CPP_BASE_VALUES_MOJOM_TRAITS_H_

#include <vector>

#include "base/component_export.h"
#include "base/values.h"
#include "mojo/public/cpp/bindings/map_traits.h"
#include "mojo/public/cpp/bindings/struct_traits.h"
#include "mojo/public/cpp/bindings/union_traits.h"
#include "mojo/public/mojom/base/values.mojom-shared.h"

namespace mojo {

template <>
struct MapTraits<base::Value> {
  using Key = std::string;
  using Value = base::Value;
  using Iterator = base::Value::const_dict_iterator_proxy::const_iterator;

  static size_t GetSize(const base::Value& input) {
    DCHECK(input.is_dict());
    return static_cast<const base::DictionaryValue&>(input).size();
  }

  static Iterator GetBegin(const base::Value& input) {
    DCHECK(input.is_dict());
    return input.DictItems().cbegin();
  }

  static void AdvanceIterator(Iterator& iterator) { ++iterator; }

  static const Key& GetKey(const Iterator& iterator) { return iterator->first; }

  static const Value& GetValue(const Iterator& iterator) {
    return iterator->second;
  }
};

template <>
struct COMPONENT_EXPORT(MOJO_BASE_SHARED_TRAITS)
    StructTraits<mojo_base::mojom::DictionaryValueDataView, base::Value> {
  static const base::Value& storage(const base::Value& value) {
    DCHECK(value.is_dict());
    return value;
  }

  static bool Read(mojo_base::mojom::DictionaryValueDataView data,
                   base::Value* value);
};

template <>
struct COMPONENT_EXPORT(MOJO_BASE_SHARED_TRAITS)
    StructTraits<mojo_base::mojom::ListValueDataView, base::Value> {
  static base::span<const base::Value> storage(const base::Value& value) {
    DCHECK(value.is_list());
    return value.GetList();
  }

  static bool Read(mojo_base::mojom::ListValueDataView data,
                   base::Value* value);
};

template <>
struct COMPONENT_EXPORT(MOJO_BASE_SHARED_TRAITS)
    UnionTraits<mojo_base::mojom::ValueDataView, base::Value> {
  static mojo_base::mojom::ValueDataView::Tag GetTag(const base::Value& data) {
    switch (data.type()) {
      case base::Value::Type::NONE:
        return mojo_base::mojom::ValueDataView::Tag::NULL_VALUE;
      case base::Value::Type::BOOLEAN:
        return mojo_base::mojom::ValueDataView::Tag::BOOL_VALUE;
      case base::Value::Type::INTEGER:
        return mojo_base::mojom::ValueDataView::Tag::INT_VALUE;
      case base::Value::Type::DOUBLE:
        return mojo_base::mojom::ValueDataView::Tag::DOUBLE_VALUE;
      case base::Value::Type::STRING:
        return mojo_base::mojom::ValueDataView::Tag::STRING_VALUE;
      case base::Value::Type::BINARY:
        return mojo_base::mojom::ValueDataView::Tag::BINARY_VALUE;
      case base::Value::Type::DICTIONARY:
        return mojo_base::mojom::ValueDataView::Tag::DICTIONARY_VALUE;
      case base::Value::Type::LIST:
        return mojo_base::mojom::ValueDataView::Tag::LIST_VALUE;
      // TODO(crbug.com/859477): Remove after root cause is found.
      case base::Value::Type::DEAD:
        CHECK(false);
        return mojo_base::mojom::ValueDataView::Tag::NULL_VALUE;
    }
    // TODO(crbug.com/859477): Revert to NOTREACHED() after root cause is found.
    CHECK(false);
    return mojo_base::mojom::ValueDataView::Tag::NULL_VALUE;
  }

  static uint8_t null_value(const base::Value& value) { return 0; }

  static bool bool_value(const base::Value& value) { return value.GetBool(); }

  static int32_t int_value(const base::Value& value) { return value.GetInt(); }

  static double double_value(const base::Value& value) {
    return value.GetDouble();
  }

  static base::StringPiece string_value(const base::Value& value) {
    return value.GetString();
  }

  static const std::vector<uint8_t>& binary_value(const base::Value& value) {
    return value.GetBlob();
  }

  static const base::Value& list_value(const base::Value& value) {
    DCHECK(value.is_list());
    return value;
  }
  static const base::Value& dictionary_value(const base::Value& value) {
    DCHECK(value.is_dict());
    return value;
  }

  static bool Read(mojo_base::mojom::ValueDataView view,
                   base::Value* value_out);
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BASE_VALUES_MOJOM_TRAITS_H_
