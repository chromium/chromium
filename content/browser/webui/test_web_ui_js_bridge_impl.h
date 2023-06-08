// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBUI_TEST_WEB_UI_JS_BRIDGE_IMPL_H_
#define CONTENT_BROWSER_WEBUI_TEST_WEB_UI_JS_BRIDGE_IMPL_H_

#include <type_traits>

#include "content/browser/webui/test_webui_js_bridge_ui.h"
#include "content/browser/webui/web_ui_managed_interface.h"
#include "content/public/browser/web_ui_js_bridge_traits.h"
#include "content/test/data/web_ui_managed_interface_test.test-mojom.h"

// This file is a hand-written implementation of a WebUIJsBridge.
// TODO(crbug.com/1407936): Remove once WebUIJsBridges are generated.

namespace content {

class WebUIController;

namespace mojom {

class TestWebUIJsBridgeImpl : public mojom::TestWebUIJsBridge,
                              public WebUIManagedInterfaceBase {
 public:
  using WebUIControllerSubclass = TestWebUIJsBridgeUI;

  using FooBinder = void (*)(WebUIControllerSubclass*,
                             mojo::PendingReceiver<mojom::Foo>);
  using FooBarBinder = void (*)(WebUIControllerSubclass*,
                                mojo::PendingReceiver<mojom::Foo>,
                                mojo::PendingRemote<mojom::Bar>);
  using BazBinder = void (*)(WebUIControllerSubclass*,
                             mojo::PendingRemote<mojom::Baz>);

  static void Create(FooBinder foo_binder,
                     FooBarBinder foo_bar_binder,
                     BazBinder baz_binder,
                     WebUIController* controller,
                     mojo::PendingReceiver<mojom::TestWebUIJsBridge> receiver);

  ~TestWebUIJsBridgeImpl() override;

  // mojom::TestWebUIJsBridge:
  void BindFoo(mojo::PendingReceiver<mojom::Foo> foo_receiver) override;

  void BindFooBar(mojo::PendingReceiver<mojom::Foo> foo_receiver,
                  mojo::PendingRemote<mojom::Bar> bar_remote) override;

  void BindBaz(mojo::PendingRemote<mojom::Baz> baz_remote) override;

 private:
  explicit TestWebUIJsBridgeImpl(
      WebUIControllerSubclass* controller,
      mojo::PendingReceiver<mojom::TestWebUIJsBridge> receiver,
      FooBinder foo_binder,
      FooBarBinder foo_bar_binder,
      BazBinder baz_binder);

  raw_ptr<WebUIControllerSubclass> controller_;
  mojo::Receiver<mojom::TestWebUIJsBridge> receiver_;

  FooBinder foo_binder_;
  FooBarBinder foo_bar_binder_;
  BazBinder baz_binder_;
};

// `TestWebUIJsBridgeBinderInitializer` is a helper class that holds all WebUI
// interface binders used by the WebUIJsBridge. It has a
// `GetWebUIJsBridgeBinder()` method that returns a binder which constructs
// a WebUIJsBridgeImpl with the registered WebUI interface binders. This
// class is used by WebUIBrowserInterfaceBrokerRegistry.
class TestWebUIJsBridgeBinderInitializer {
 public:
  using WebUIControllerSubclass = TestWebUIJsBridgeUI;

  template <typename ReceiverInterface>
  using ReceiverBinder = void (*)(WebUIControllerSubclass*,
                                  mojo::PendingReceiver<ReceiverInterface>);

  template <typename ReceiverInterface, typename RemoteInterface>
  using ReceiverAndRemoteBinder =
      void (*)(WebUIControllerSubclass*,
               mojo::PendingReceiver<ReceiverInterface>,
               mojo::PendingRemote<RemoteInterface>);

  template <typename RemoteInterface>
  using RemoteBinder = void (*)(WebUIControllerSubclass*,
                                mojo::PendingRemote<RemoteInterface>);

  using WebUIJsBridgeBinder = base::RepeatingCallback<
      void(WebUIController*, mojo::PendingReceiver<mojom::TestWebUIJsBridge>)>;

  explicit TestWebUIJsBridgeBinderInitializer() = default;
  ~TestWebUIJsBridgeBinderInitializer() = default;

  // Method that returns an interface binder for mojom::WebUIJsBridge.
  WebUIJsBridgeBinder GetWebUIJsBridgeBinder() {
    return base::BindRepeating(&TestWebUIJsBridgeImpl::Create, foo_binder_,
                               foo_bar_binder_, baz_binder_);
  }

  // AddBinder() methods don't need to use templates, but making them
  // templated allows AddBinder callers to specify which interfaces are being
  // bound e.g.:
  //
  // initializer.AddBinder<mojom::Foo>(...);
  // initializer.AddBinder<mojom::Bar, mojom::BarClient>(...);
  //
  // We also use std::common_type_t<> to disable automatic type deduction and
  // force callers to specify the template.
  template <typename ReceiverInterface>
  TestWebUIJsBridgeBinderInitializer& AddBinder(
      std::common_type_t<ReceiverBinder<ReceiverInterface>>);
  template <typename ReceiverInterface, typename RemoteInterface>
  TestWebUIJsBridgeBinderInitializer& AddBinder(
      std::common_type_t<
          ReceiverAndRemoteBinder<ReceiverInterface, RemoteInterface>>);
  template <typename RemoteInterface>
  TestWebUIJsBridgeBinderInitializer& AddBinder(
      std::common_type_t<RemoteBinder<RemoteInterface>>);

  template <>
  TestWebUIJsBridgeBinderInitializer& AddBinder<mojom::Foo>(
      ReceiverBinder<mojom::Foo> foo_binder) {
    CHECK(!foo_binder_);
    foo_binder_ = foo_binder;
    return *this;
  }

  template <>
  TestWebUIJsBridgeBinderInitializer& AddBinder<mojom::Foo, mojom::Bar>(
      ReceiverAndRemoteBinder<mojom::Foo, mojom::Bar> foo_bar_binder) {
    CHECK(!foo_bar_binder_);
    foo_bar_binder_ = foo_bar_binder;
    return *this;
  }

  template <>
  TestWebUIJsBridgeBinderInitializer& AddBinder<mojom::Baz>(
      RemoteBinder<mojom::Baz> baz_binder) {
    CHECK(!baz_binder_);
    baz_binder_ = baz_binder;
    return *this;
  }

 private:
  ReceiverBinder<mojom::Foo> foo_binder_;
  ReceiverAndRemoteBinder<mojom::Foo, mojom::Bar> foo_bar_binder_;
  RemoteBinder<mojom::Baz> baz_binder_;
};

}  // namespace mojom

}  // namespace content

namespace content {
template <>
struct JsBridgeTraits<content::TestWebUIJsBridgeUI> {
  using Interface = content::mojom::TestWebUIJsBridge;
  using Implementation = content::mojom::TestWebUIJsBridgeImpl;
  using BinderInitializer = content::mojom::TestWebUIJsBridgeBinderInitializer;
};
}  // namespace content

#endif  // CONTENT_BROWSER_WEBUI_TEST_WEB_UI_JS_BRIDGE_IMPL_H_
