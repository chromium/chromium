// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_MOJOM_FIDL_INTERFACE_REQUEST_MOJOM_TRAITS_H_
#define FUCHSIA_MOJOM_FIDL_INTERFACE_REQUEST_MOJOM_TRAITS_H_

#include <lib/fidl/cpp/interface_request.h>

#include "mojo/public/cpp/platform/platform_handle.h"

namespace mojo {

// Implementation of StructTratis<DataView, fidl::InterfaceRequest<Interface>>.
// Different Interface still needs to define its own StructTraits by subclassing
// this struct. Read test_request_interface_mojom_traits.h for an example.
template <typename DataView, typename Interface>
struct FidlInterfaceRequestStructTraits {
  static PlatformHandle request(fidl::InterfaceRequest<Interface>& request) {
    DCHECK(request.is_valid());
    return PlatformHandle(request.TakeChannel());
  }

  static bool Read(DataView input, fidl::InterfaceRequest<Interface>* output) {
    PlatformHandle handle = input.TakeRequest();
    if (!handle.is_valid_handle())
      return false;

    output->set_channel(zx::channel(handle.TakeHandle()));
    return true;
  }
};

}  // namespace mojo

#endif  // FUCHSIA_MOJOM_FIDL_INTERFACE_REQUEST_MOJOM_TRAITS_H_
