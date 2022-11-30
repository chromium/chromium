// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_RAW_PTR_IMPL_REF_TRAITS_H_
#define MOJO_PUBLIC_CPP_BINDINGS_RAW_PTR_IMPL_REF_TRAITS_H_

namespace mojo {

// Default traits for a binding's implementation reference type. This
// corresponds to a raw pointer.
template <typename Interface>
struct RawPtrImplRefTraits {
  using PointerType = Interface*;

  static bool IsNull(PointerType ptr) { return !ptr; }
  static Interface* GetRawPointer(PointerType* ptr) { return *ptr; }
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_RAW_PTR_IMPL_REF_TRAITS_H_
