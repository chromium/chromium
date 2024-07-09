// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_LIB_SERIALIZATION_FORWARD_H_
#define MOJO_PUBLIC_CPP_BINDINGS_LIB_SERIALIZATION_FORWARD_H_

#include <optional>
#include <type_traits>

#include "mojo/public/cpp/bindings/array_traits.h"
#include "mojo/public/cpp/bindings/enum_traits.h"
#include "mojo/public/cpp/bindings/lib/buffer.h"
#include "mojo/public/cpp/bindings/lib/default_construct_tag_internal.h"
#include "mojo/public/cpp/bindings/lib/message_fragment.h"
#include "mojo/public/cpp/bindings/lib/template_util.h"
#include "mojo/public/cpp/bindings/map_traits.h"
#include "mojo/public/cpp/bindings/optional_as_pointer.h"
#include "mojo/public/cpp/bindings/string_traits.h"
#include "mojo/public/cpp/bindings/struct_traits.h"
#include "mojo/public/cpp/bindings/union_traits.h"

// This file is included by serialization implementation files to avoid circular
// includes.
// Users of the serialization funtions should include serialization.h (and also
// wtf_serialization.h if necessary).

namespace mojo {
namespace internal {

template <typename MojomType, typename MaybeConstUserType>
struct Serializer;

template <typename T>
using IsAbslOptional = IsSpecializationOf<std::optional, std::decay_t<T>>;

template <typename T>
using IsOptionalAsPointer =
    IsSpecializationOf<mojo::OptionalAsPointer, std::decay_t<T>>;

template <typename MojomType, typename InputUserType, typename... Args>
void Serialize(InputUserType&& input, Args&&... args) {
  if constexpr (IsAbslOptional<InputUserType>::value) {
    if (!input)
      return;
    Serialize<MojomType>(*input, std::forward<Args>(args)...);
  } else if constexpr (IsOptionalAsPointer<InputUserType>::value) {
    if (!input.has_value())
      return;
    Serialize<MojomType>(input.value(), std::forward<Args>(args)...);
  } else {
    Serializer<MojomType, std::remove_reference_t<InputUserType>>::Serialize(
        std::forward<InputUserType>(input), std::forward<Args>(args)...);
  }
}

template <typename MojomType,
          typename DataType,
          typename InputUserType,
          typename... Args>
bool Deserialize(DataType&& input, InputUserType* output, Args&&... args) {
  if constexpr (IsAbslOptional<InputUserType>::value) {
    if (!input) {
      *output = std::nullopt;
      return true;
    }
    if (!*output) {
      if constexpr (std::is_constructible_v<typename InputUserType::value_type,
                                            ::mojo::DefaultConstruct::Tag>) {
        output->emplace(mojo::internal::DefaultConstructTag());
      } else {
        output->emplace(typename InputUserType::value_type());
      }
    }
    return Deserialize<MojomType>(std::forward<DataType>(input),
                                  &output->value(),
                                  std::forward<Args>(args)...);
  } else {
    return Serializer<MojomType, InputUserType>::Deserialize(
        std::forward<DataType>(input), output, std::forward<Args>(args)...);
  }
}

}  // namespace internal
}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_LIB_SERIALIZATION_FORWARD_H_
