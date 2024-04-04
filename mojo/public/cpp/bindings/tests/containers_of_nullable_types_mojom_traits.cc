// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/tests/containers_of_nullable_types_mojom_traits.h"

namespace mojo::test::containers_of_nullable_types {

NativeStruct::NativeStruct() = default;

NativeStruct::NativeStruct(const NativeStruct& other) = default;

NativeStruct::~NativeStruct() = default;

NativeStruct& NativeStruct::operator=(const NativeStruct& other) = default;

}  // namespace mojo::test::containers_of_nullable_types

namespace mojo {

using test::containers_of_nullable_types::NativeStruct;
using test::containers_of_nullable_types::mojom::RegularEnum;
using test::containers_of_nullable_types::mojom::TypemappedContainerDataView;

// statics
const std::vector<std::optional<RegularEnum>>&
StructTraits<TypemappedContainerDataView, NativeStruct>::enum_values(
    const NativeStruct& native) {
  return native.enum_values;
}

bool StructTraits<TypemappedContainerDataView, NativeStruct>::Read(
    TypemappedContainerDataView in,
    NativeStruct* out) {
  out->enum_values.clear();

  ArrayDataView<std::optional<RegularEnum>> enum_data_view;
  in.GetEnumValuesDataView(&enum_data_view);

  for (uint32_t i = 0; i < enum_data_view.size(); ++i) {
    out->enum_values.push_back(enum_data_view[i]);
  }

  return true;
}

}  // namespace mojo
