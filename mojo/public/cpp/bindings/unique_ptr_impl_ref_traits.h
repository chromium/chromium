// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_UNIQUE_PTR_IMPL_REF_TRAITS_H_
#define MOJO_PUBLIC_CPP_BINDINGS_UNIQUE_PTR_IMPL_REF_TRAITS_H_

namespace mojo {

// Traits for a binding's implementation reference type.
// This corresponds to a unique_ptr reference type.
template <typename Interface, typename Deleter = std::default_delete<Interface>>
struct UniquePtrImplRefTraits {
  using PointerType = std::unique_ptr<Interface, Deleter>;

  static bool IsNull(const PointerType& ptr) { return !ptr; }
  static Interface* GetRawPointer(PointerType* ptr) { return ptr->get(); }
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_UNIQUE_PTR_IMPL_REF_TRAITS_H_
