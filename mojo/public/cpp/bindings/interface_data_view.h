// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_INTERFACE_DATA_VIEW_H_
#define MOJO_PUBLIC_CPP_BINDINGS_INTERFACE_DATA_VIEW_H_

namespace mojo {

// They are used for type identification purpose only.
template <typename Interface>
class AssociatedInterfacePtrInfoDataView {};

template <typename Interface>
class AssociatedInterfaceRequestDataView {};

template <typename Interface>
class InterfacePtrDataView {};

template <typename Interface>
class InterfaceRequestDataView {};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_INTERFACE_DATA_VIEW_H_
