// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_TESTS_CONTAINERS_OF_NULLABLE_TYPES_MOJOM_TRAITS_H_
#define MOJO_PUBLIC_CPP_BINDINGS_TESTS_CONTAINERS_OF_NULLABLE_TYPES_MOJOM_TRAITS_H_

#include "mojo/public/interfaces/bindings/tests/containers_of_nullable_types.mojom.h"

namespace mojo::test::containers_of_nullable_types {

struct NativeStruct {
  NativeStruct();
  ~NativeStruct();
  NativeStruct(const NativeStruct&);
  NativeStruct& operator=(const NativeStruct& other);

  std::vector<std::optional<mojom::RegularEnum>> enum_values;
};

}  // namespace mojo::test::containers_of_nullable_types

namespace mojo {

using test::containers_of_nullable_types::NativeStruct;
using test::containers_of_nullable_types::mojom::RegularEnum;
using test::containers_of_nullable_types::mojom::TypemappedContainerDataView;

template <>
struct StructTraits<TypemappedContainerDataView, NativeStruct> {
  static const std::vector<std::optional<RegularEnum>>& enum_values(
      const NativeStruct& native);

  static bool Read(TypemappedContainerDataView in, NativeStruct* out);
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_TESTS_CONTAINERS_OF_NULLABLE_TYPES_MOJOM_TRAITS_H_
