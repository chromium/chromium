// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/tests/bindings_test_base.h"
#include "mojo/public/cpp/bindings/tests/js_interface_binder_unittest.test-mojom-js-interface-binder-impl.h"

namespace mojo::test::js_interface_binder {

class JsInterfaceBinderTest : public BindingsTestBase {
 public:
  JsInterfaceBinderTest() = default;
  ~JsInterfaceBinderTest() override = default;
};

// Test to ensure generated impl file can be included.
TEST_P(JsInterfaceBinderTest, Stub) {}

INSTANTIATE_MOJO_BINDINGS_TEST_SUITE_P(JsInterfaceBinderTest);

}  // namespace mojo::test::js_interface_binder
