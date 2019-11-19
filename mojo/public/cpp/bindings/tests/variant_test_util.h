// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_TESTS_VARIANT_TEST_UTIL_H_
#define MOJO_PUBLIC_CPP_BINDINGS_TESTS_VARIANT_TEST_UTIL_H_

#include <type_traits>

#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace mojo {
namespace test {

// Converts a PendingReceiver of Interface1 to a one of Interface0. Interface0
// and Interface1 must be two variants of the same mojom interface.
template <typename Interface0, typename Interface1>
PendingReceiver<Interface0> ConvertPendingReceiver(
    PendingReceiver<Interface1> receiver) {
  static_assert(std::is_base_of<typename Interface0::Base_, Interface1>::value,
                "Interface types are not variants of the same mojom interface");
  return PendingReceiver<Interface0>(receiver.PassPipe());
}

}  // namespace test
}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_TESTS_VARIANT_TEST_UTIL_H_
