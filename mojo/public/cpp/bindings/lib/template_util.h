// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_LIB_TEMPLATE_UTIL_H_
#define MOJO_PUBLIC_CPP_BINDINGS_LIB_TEMPLATE_UTIL_H_

#include <optional>
#include <type_traits>

#include "mojo/public/cpp/bindings/optional_as_pointer.h"

namespace mojo::internal {

// A helper template to determine if given type is non-const move-only-type,
// i.e. if a value of the given type should be passed via std::move() in a
// destructive way.
template <typename T>
struct IsMoveOnlyType {
  static const bool value =
      std::is_constructible_v<T, T&&> && !std::is_constructible_v<T, const T&>;
};

template <template <typename...> class Template, typename T>
struct IsSpecializationOf : std::false_type {};

template <template <typename...> class Template, typename... Args>
struct IsSpecializationOf<Template, Template<Args...>> : std::true_type {};

template <typename T>
using IsStdOptional = IsSpecializationOf<std::optional, std::decay_t<T>>;

template <typename T>
using IsOptionalAsPointer =
    IsSpecializationOf<mojo::OptionalAsPointer, std::decay_t<T>>;

}  // namespace mojo::internal

#endif  // MOJO_PUBLIC_CPP_BINDINGS_LIB_TEMPLATE_UTIL_H_
