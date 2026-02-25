// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/base/values_mojom_traits.h"

#include <memory>
#include <utility>

#include "base/features.h"

namespace mojo {

bool StructTraits<mojo_base::mojom::DictionaryValueDataView, base::DictValue>::
    Read(mojo_base::mojom::DictionaryValueDataView data, base::DictValue* out) {
  mojo::MapDataView<mojo::StringDataView, mojo_base::mojom::ValueDataView> view;
  data.GetStorageDataView(&view);

  if (base::features::IsReducePPMsEnabled()) {
    out->reserve(view.size());
  }

  for (size_t i = 0; i < view.size(); ++i) {
    std::string_view key;
    base::Value value;
    if (!view.keys().Read(i, &key) || !view.values().Read(i, &value)) {
      return false;
    }
    if (base::features::IsReducePPMsEnabled()) {
      out->Set_HintAtEnd(key, std::move(value));
    } else {
      out->Set(key, std::move(value));
    }
  }
  return true;
}

bool StructTraits<mojo_base::mojom::ListValueDataView, base::ListValue>::Read(
    mojo_base::mojom::ListValueDataView data,
    base::ListValue* out) {
  mojo::ArrayDataView<mojo_base::mojom::ValueDataView> view;
  data.GetStorageDataView(&view);

  if (base::features::IsReducePPMsEnabled()) {
    out->reserve(view.size());
  }

  base::Value element;
  for (size_t i = 0; i < view.size(); ++i) {
    if (!view.Read(i, &element)) {
      return false;
    }
    out->Append(std::move(element));
  }
  return true;
}

bool UnionTraits<mojo_base::mojom::ValueDataView, base::Value>::Read(
    mojo_base::mojom::ValueDataView data,
    base::Value* value_out) {
  switch (data.tag()) {
    case mojo_base::mojom::ValueDataView::Tag::kNullValue: {
      *value_out = base::Value();
      return true;
    }
    case mojo_base::mojom::ValueDataView::Tag::kBoolValue: {
      *value_out = base::Value(data.bool_value());
      return true;
    }
    case mojo_base::mojom::ValueDataView::Tag::kIntValue: {
      *value_out = base::Value(data.int_value());
      return true;
    }
    case mojo_base::mojom::ValueDataView::Tag::kDoubleValue: {
      *value_out = base::Value(data.double_value());
      return true;
    }
    case mojo_base::mojom::ValueDataView::Tag::kStringValue: {
      std::string_view string_piece;
      if (!data.ReadStringValue(&string_piece)) {
        return false;
      }
      *value_out = base::Value(string_piece);
      return true;
    }
    case mojo_base::mojom::ValueDataView::Tag::kBinaryValue: {
      mojo::ArrayDataView<uint8_t> binary_data_view;
      data.GetBinaryValueDataView(&binary_data_view);
      base::Value::BlobStorage blob_storage(std::from_range, binary_data_view);
      *value_out = base::Value(std::move(blob_storage));
      return true;
    }
    case mojo_base::mojom::ValueDataView::Tag::kDictionaryValue: {
      base::DictValue dict;
      if (!data.ReadDictionaryValue(&dict)) {
        return false;
      }
      *value_out = base::Value(std::move(dict));
      return true;
    }
    case mojo_base::mojom::ValueDataView::Tag::kListValue: {
      base::ListValue list;
      if (!data.ReadListValue(&list)) {
        return false;
      }
      *value_out = base::Value(std::move(list));
      return true;
    }
  }
  return false;
}

}  // namespace mojo
