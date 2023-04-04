// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_DEFAULT_CONSTRUCT_TAG_H_
#define MOJO_PUBLIC_CPP_BINDINGS_DEFAULT_CONSTRUCT_TAG_H_

namespace mojo {

namespace internal {
struct DefaultConstructTag;
}

// Mojo deserialization normally requires that types be default constructible.
// For a type where default construction does not make sense (e.g. a type that
// would prefer not to have a default invalid state), the type can instead
// expose a Mojo-specific "default" constructor that takes a
// `mojo::DefaultConstructTag` as its only argument:
//
// struct Nonce {
//  public:
//   explicit Nonce(absl::uint128 value);
//
//   // Constructs an uninitialized `Nonce` for Mojo deserialization to fill in.
//   explicit Nonce(mojo::DefaultConstructTag);
// };
//
// This is essentially a variant of `base::PassKey` that is specialized for the
// Mojo deserialization case.
struct DefaultConstruct {
 public:
  struct Tag {
   private:
    friend internal::DefaultConstructTag;
    // Intentionally not defaulted to prevent list initialization syntax from
    // bypassing the constructor visibility restrictions.
    constexpr Tag() {}
  };
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_DEFAULT_CONSTRUCT_TAG_H_
