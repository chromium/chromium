// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BASE_FUCHSIA_TEST_INTERFACE_REQUEST_MOJOM_TRAITS_H_
#define MOJO_PUBLIC_CPP_BASE_FUCHSIA_TEST_INTERFACE_REQUEST_MOJOM_TRAITS_H_

#include "mojo/public/cpp/base/fuchsia/fidl_interface_request_mojom_traits.h"

namespace mojo {

template <>
struct StructTraits<fuchsia::test::mojom::TestInterfaceRequestDataView,
                    fidl::InterfaceRequest<base::testfidl::TestInterface>>
    : public FidlInterfaceRequestStructTraits<
          fuchsia::test::mojom::TestInterfaceRequestDataView,
          base::testfidl::TestInterface> {};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BASE_FUCHSIA_TEST_INTERFACE_REQUEST_MOJOM_TRAITS_H_
