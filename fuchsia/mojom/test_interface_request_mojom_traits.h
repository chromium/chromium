// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_MOJOM_TEST_INTERFACE_REQUEST_MOJOM_TRAITS_H_
#define FUCHSIA_MOJOM_TEST_INTERFACE_REQUEST_MOJOM_TRAITS_H_

#include "fuchsia/mojom/fidl_interface_request_mojom_traits.h"

namespace mojo {

template <>
struct StructTraits<
    fuchsia::test::mojom::TestInterfaceRequestDataView,
    fidl::InterfaceRequest<base::fuchsia::testfidl::TestInterface>>
    : public FidlInterfaceRequestStructTraits<
          fuchsia::test::mojom::TestInterfaceRequestDataView,
          base::fuchsia::testfidl::TestInterface> {};

}  // namespace mojo

#endif  // FUCHSIA_MOJOM_TEST_INTERFACE_REQUEST_MOJOM_TRAITS_H_
