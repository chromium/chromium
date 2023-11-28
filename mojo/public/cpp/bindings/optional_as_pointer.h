// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_OPTIONAL_AS_POINTER_H_
#define MOJO_PUBLIC_CPP_BINDINGS_OPTIONAL_AS_POINTER_H_

#include <cstddef>

#include "base/memory/raw_ptr.h"

namespace mojo {

// Simple wrapper around a pointer to allow zero-copy serialization of a
// nullable type.
//
// Traits for nullable fields typically return `const std::optional<T>&` or
// `std::optional<T>&`. However, if the field is not already an
// `std::optional`, this can be inefficient:
//
//   static std::optional<std::string> nullable_field_getter(
//       const MyType& input) {
//     // Bad: copies input.data() to populate `std::optional`.
//     return std::make_optional(
//         input.has_valid_data() ? input.data() : std::nullopt);
//   }
//
// Using this wrapper allows this to be serialized without additional copies:
//
//   static mojo::OptionalAsPointer<std::string> nullable_field_getter(
//       const MyType& input) {
//     return mojo::OptionalAsPointer(
//         input.has_valid_data() ? &input.data() : nullptr);
//   }
//
// N.B. The original prototype for reducing copies in serialization attempted to
// use C++ pointers directly; unfortunately, some Windows SDK opaque handle
// types are actually defined as a pointer to a struct, which confused the Mojo
// serialization traits. While it is possible to block the problematic types,
// having an actual type makes the intent more explicit.
template <typename T>
class OptionalAsPointer {
 public:
  explicit OptionalAsPointer(T* ptr) : value_(ptr) {}
  OptionalAsPointer(std::nullptr_t) {}

  // Allows for conversions between compatible pointer types (e.g. from `T*` to
  // `const T*`). For simplicity, this does not bother with using SFINAE to
  // restrict conversions: assignment of the underlying pointer will give a
  // compile error that is hopefully "good enough".
  template <typename U>
  OptionalAsPointer(const OptionalAsPointer<U>& other) : value_(other.value_) {}

  bool has_value() const { return value_ != nullptr; }
  T& value() { return *value_; }
  const T& value() const { return *value_; }

 private:
  template <typename U>
  friend class OptionalAsPointer;

  raw_ptr<T> value_ = nullptr;
};

template <typename T>
OptionalAsPointer(T*) -> OptionalAsPointer<T>;

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_OPTIONAL_AS_POINTER_H_
