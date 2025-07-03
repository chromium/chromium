// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is a "no compile" test. It's not supposed to build.

#include "mojo/public/cpp/bindings/binder_map.h"

#include <string>

#include "mojo/public/cpp/bindings/tests/binder_map_unittest.test-mojom.h"

namespace mojo::test::binder_map_unittest {

void NonStaticString() {
  {
    std::string foo = "foo";
    auto static_str = internal::StaticString(foo.c_str()); // expected-error {{call to consteval function 'mojo::internal::StaticString::StaticString' is not a constant expression}}
  }
  {
    const char kFoo[] = "foo";
    auto static_str = internal::StaticString(kFoo); // expected-error {{call to consteval function 'mojo::internal::StaticString::StaticString' is not a constant expression}}
  }
  {
    constexpr char kFoo[] = "foo";
    auto static_str = internal::StaticString(kFoo); // expected-error {{call to consteval function 'mojo::internal::StaticString::StaticString' is not a constant expression}}
  }
}

void AddCapturingLambda() {
  BinderMap map;
  int captured = 42;

  map.Add<mojom::TestInterface1>(  // expected-error {{no matching member function for call to 'Add'}}
    [&captured](PendingReceiver<mojom::TestInterface1> receiver) {
      captured++;
    }
  );
}

}  // namespace mojo::test::binder_map_unittest
