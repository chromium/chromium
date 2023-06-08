// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webui/test_web_ui_js_bridge_impl.h"

namespace content::mojom {

// static
void TestWebUIJsBridgeImpl::Create(
    FooBinder foo_binder,
    FooBarBinder foo_bar_binder,
    BazBinder baz_binder,
    WebUIController* controller,
    mojo::PendingReceiver<mojom::TestWebUIJsBridge> receiver) {
  auto* controller_subclass = controller->GetAs<WebUIControllerSubclass>();
  CHECK(controller_subclass);

  auto js_bridge_impl = base::WrapUnique(
      new TestWebUIJsBridgeImpl(controller_subclass, std::move(receiver),
                                foo_binder, foo_bar_binder, baz_binder));
  SaveWebUIManagedInterfaceInDocument(controller, std::move(js_bridge_impl));
}

TestWebUIJsBridgeImpl::TestWebUIJsBridgeImpl(
    WebUIControllerSubclass* controller,
    mojo::PendingReceiver<mojom::TestWebUIJsBridge> receiver,
    FooBinder foo_binder,
    FooBarBinder foo_bar_binder,
    BazBinder baz_binder)
    : controller_(controller),
      receiver_(this, std::move(receiver)),
      foo_binder_(foo_binder),
      foo_bar_binder_(foo_bar_binder),
      baz_binder_(baz_binder) {}

TestWebUIJsBridgeImpl::~TestWebUIJsBridgeImpl() = default;

void TestWebUIJsBridgeImpl::BindFoo(
    mojo::PendingReceiver<mojom::Foo> foo_receiver) {
  foo_binder_(controller_, std::move(foo_receiver));
}

void TestWebUIJsBridgeImpl::BindFooBar(
    mojo::PendingReceiver<mojom::Foo> foo_receiver,
    mojo::PendingRemote<mojom::Bar> bar_remote) {
  foo_bar_binder_(controller_, std::move(foo_receiver), std::move(bar_remote));
}

void TestWebUIJsBridgeImpl::BindBaz(
    mojo::PendingRemote<mojom::Baz> baz_remote) {
  baz_binder_(controller_, std::move(baz_remote));
}

}  // namespace content::mojom
