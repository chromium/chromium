// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_ENUM_UTILS_H_
#define MOJO_PUBLIC_CPP_BINDINGS_ENUM_UTILS_H_

#include <optional>
#include <type_traits>

#include "base/numerics/safe_conversions.h"

namespace mojo {

// Converts |int_value| to |TMojoEnum|.  If |int_value| represents a known enum
// value, then a corresponding |TMojoEnum| value will be returned.  Returns
// |std::nullopt| otherwise.
//
// Using base::StrictNumeric as the parameter type prevents callers from
// accidentally using an implicit narrowing conversion when calling this
// function (e.g. calling it with an int64_t argument, when the enum's
// underlying type is int32_t).
template <typename TMojoEnum>
std::optional<TMojoEnum> ConvertIntToMojoEnum(
    base::StrictNumeric<int32_t> int_value) {
  // Today all mojo enums use |int32_t| as the underlying type, so the code
  // can simply use |int32_t| rather than |std::underlying_type_t<TMojoEnum>|.
  static_assert(std::is_same<int32_t, std::underlying_type_t<TMojoEnum>>::value,
                "Assumming that all mojo enums use int32_t as the underlying "
                "type");

  // The static cast from int32_t to TMojoEnum should be safe from undefined
  // behavior.  In particular, TMojoEnums have a fixed underlying type
  // (int32_t), so only the first part of the following spec snippet applies:
  // http://www.eel.is/c++draft/expr.static.cast#10:
  //     [...] If the enumeration type has a fixed underlying type, the value is
  //     first converted to that type by integral conversion, if necessary, and
  //     then to the enumeration type. If the enumeration type does not have a
  //     fixed underlying type, the value is unchanged if the original value is
  //     within the range of the enumeration values ([dcl.enum]), and otherwise,
  //     the behavior is undefined. [...]
  //
  // Also, note that using a list initializer to covert an integer to an enum
  // value is explicitly called out as safe in C++17 - see
  // http://www.eel.is/c++draft/dcl.init.list#3.8:
  //     enum class Handle : std::uint32_t { Invalid = 0 };
  //     Handle h { 42 }; // OK as of C++17
  // We may want to switch to this syntax in the future (once C++17 is adopted).
  TMojoEnum enum_value =
      static_cast<TMojoEnum>(static_cast<int32_t>(int_value));

  // Verify whether |int_value| was one of known enum values.
  //
  // IsKnownEnumValue comes from code generated from .mojom files and is present
  // in somenamespace::mojom namespace (the same namespace as the namespace of
  // TMojoEnum and |enum_value|) - we rely on ADL (argument-dependent lookup) to
  // find the right overload below.
  if (!IsKnownEnumValue(enum_value))
    return std::nullopt;

  return enum_value;
}

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_ENUM_UTILS_H_
