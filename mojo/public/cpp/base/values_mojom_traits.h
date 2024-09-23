// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BASE_VALUES_MOJOM_TRAITS_H_
#define MOJO_PUBLIC_CPP_BASE_VALUES_MOJOM_TRAITS_H_

#include <string_view>
#include <vector>

#include "base/component_export.h"
#include "base/notreached.h"
#include "base/values.h"
#include "mojo/public/cpp/bindings/map_traits.h"
#include "mojo/public/cpp/bindings/struct_traits.h"
#include "mojo/public/cpp/bindings/union_traits.h"
#include "mojo/public/mojom/base/values.mojom-shared.h"

namespace mojo {

template <>
struct MapTraits<base::Value::Dict> {
  using Key = std::string;
  using Value = base::Value;
  using Iterator = base::Value::Dict::const_iterator;

  static size_t GetSize(const base::Value::Dict& in) { return in.size(); }

  static Iterator GetBegin(const base::Value::Dict& in) { return in.cbegin(); }

  static void AdvanceIterator(Iterator& it) { ++it; }

  static const Key& GetKey(const Iterator& it) { return it->first; }

  static const Value& GetValue(const Iterator& it) { return it->second; }
};

template <>
struct COMPONENT_EXPORT(MOJO_BASE_SHARED_TRAITS)
    StructTraits<mojo_base::mojom::DictionaryValueDataView, base::Value::Dict> {
  static const base::Value::Dict& storage(const base::Value::Dict& in) {
    return in;
  }

  static bool Read(mojo_base::mojom::DictionaryValueDataView data,
                   base::Value::Dict* out);
};

template <>
struct ArrayTraits<base::Value::List> {
  using Element = base::Value;

  static size_t GetSize(const base::Value::List& in) { return in.size(); }

  static const base::Value& GetAt(const base::Value::List& in, size_t index) {
    return in[index];
  }

  static base::Value& GetAt(base::Value::List& in, size_t index) {
    return in[index];
  }

  static bool Resize(base::Value::List& in, size_t size) {
    in.resize(size);
    return true;
  }
};

template <>
struct COMPONENT_EXPORT(MOJO_BASE_SHARED_TRAITS)
    StructTraits<mojo_base::mojom::ListValueDataView, base::Value::List> {
  static const base::Value::List& storage(const base::Value::List& in) {
    return in;
  }

  static bool Read(mojo_base::mojom::ListValueDataView data,
                   base::Value::List* out);
};

template <>
struct COMPONENT_EXPORT(MOJO_BASE_SHARED_TRAITS)
    UnionTraits<mojo_base::mojom::ValueDataView, base::Value> {
  static mojo_base::mojom::ValueDataView::Tag GetTag(const base::Value& data) {
    switch (data.type()) {
      case base::Value::Type::NONE:
        return mojo_base::mojom::ValueDataView::Tag::kNullValue;
      case base::Value::Type::BOOLEAN:
        return mojo_base::mojom::ValueDataView::Tag::kBoolValue;
      case base::Value::Type::INTEGER:
        return mojo_base::mojom::ValueDataView::Tag::kIntValue;
      case base::Value::Type::DOUBLE:
        return mojo_base::mojom::ValueDataView::Tag::kDoubleValue;
      case base::Value::Type::STRING:
        return mojo_base::mojom::ValueDataView::Tag::kStringValue;
      case base::Value::Type::BINARY:
        return mojo_base::mojom::ValueDataView::Tag::kBinaryValue;
      case base::Value::Type::DICT:
        return mojo_base::mojom::ValueDataView::Tag::kDictionaryValue;
      case base::Value::Type::LIST:
        return mojo_base::mojom::ValueDataView::Tag::kListValue;
    }
    NOTREACHED();
  }

  static uint8_t null_value(const base::Value& value) { return 0; }

  static bool bool_value(const base::Value& value) { return value.GetBool(); }

  static int32_t int_value(const base::Value& value) { return value.GetInt(); }

  static double double_value(const base::Value& value) {
    return value.GetDouble();
  }

  static std::string_view string_value(const base::Value& value) {
    return value.GetString();
  }

  static const std::vector<uint8_t>& binary_value(const base::Value& value) {
    return value.GetBlob();
  }

  static const base::Value::Dict& dictionary_value(const base::Value& value) {
    return value.GetDict();
  }

  static const base::Value::List& list_value(const base::Value& value) {
    return value.GetList();
  }

  static bool Read(mojo_base::mojom::ValueDataView view,
                   base::Value* value_out);
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BASE_VALUES_MOJOM_TRAITS_H_
