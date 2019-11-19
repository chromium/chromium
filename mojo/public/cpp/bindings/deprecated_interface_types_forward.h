// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_DEPRECATED_INTERFACE_TYPES_FORWARD_H_
#define MOJO_PUBLIC_CPP_BINDINGS_DEPRECATED_INTERFACE_TYPES_FORWARD_H_

// Forward declarations which foo.mojom-forward.h requires.
namespace mojo {

template <typename Interface>
class InterfacePtr;
template <typename Interface>
class InterfacePtrInfo;
template <typename Interface>
class InterfaceRequest;
template <typename Interface>
class AssociatedInterfacePtr;
template <typename Interface>
class AssociatedInterfacePtrInfo;
template <typename Interface>
class AssociatedInterfaceRequest;

template <typename InterfacePtrType>
class ThreadSafeInterfacePtrBase;
template <typename Interface>
using ThreadSafeAssociatedInterfacePtr =
    ThreadSafeInterfacePtrBase<AssociatedInterfacePtr<Interface>>;
template <typename Interface>
using ThreadSafeInterfacePtr =
    ThreadSafeInterfacePtrBase<InterfacePtr<Interface>>;

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_DEPRECATED_INTERFACE_TYPES_FORWARD_H_
