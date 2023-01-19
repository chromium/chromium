// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/tests/bindings_test_base.h"
#include "mojo/public/cpp/bindings/tests/js_interface_binder_unittest.test-mojom-js-interface-binder-impl.h"

namespace mojo::test::js_interface_binder {

class JsInterfaceBinderTest : public BindingsTestBase {
 public:
  JsInterfaceBinderTest() = default;
  ~JsInterfaceBinderTest() override = default;
};

// Tests binder methods are overridden and can be called. Calling them does
// nothing for now.
TEST_P(JsInterfaceBinderTest, Bind) {
  mojom::FooJsInterfaceBinderImpl binder;

  binder.BindFooPageHandler(mojo::PendingReceiver<mojom::FooPageHandler>(),
                            mojo::PendingRemote<mojom::FooPage>());

  binder.BindBar(mojo::PendingReceiver<mojom::Bar>());
  binder.BindBarObserver(mojo::PendingRemote<mojom::BarObserver>());
}

INSTANTIATE_MOJO_BINDINGS_TEST_SUITE_P(JsInterfaceBinderTest);

}  // namespace mojo::test::js_interface_binder
