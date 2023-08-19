// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "content/browser/webui/test_webui_js_bridge_ui.h"
#include "content/browser/webui/web_ui_managed_interface.h"
#include "content/public/browser/web_ui_browser_interface_broker_registry.h"
#include "content/public/browser/webui_config_map.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_content_browser_client.h"
#include "content/shell/browser/shell.h"
#include "content/test/data/web_ui_managed_interface_test.test-mojom-webui-js-bridge-impl.h"
#include "content/test/data/web_ui_managed_interface_test.test-mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

class FooImpl : public mojom::Foo, public WebUIManagedInterfaceBase {
 public:
  static void Create(TestWebUIJsBridgeUI* controller,
                     mojo::PendingReceiver<mojom::Foo> pending_receiver) {
    auto foo =
        base::WrapUnique(new FooImpl(controller, std::move(pending_receiver)));
    SaveWebUIManagedInterfaceInDocument(controller, std::move(foo));
  }

  ~FooImpl() override = default;

  // mojom::Foo
  void GetFoo(GetFooCallback callback) override {
    std::move(callback).Run("foo-success");
  }

 private:
  FooImpl(TestWebUIJsBridgeUI* controller,
          mojo::PendingReceiver<mojom::Foo> pending_receiver)
      : receiver_(this, std::move(pending_receiver)) {}

  mojo::Receiver<mojom::Foo> receiver_;
};

class FooBarImpl : public mojom::Foo, public WebUIManagedInterfaceBase {
 public:
  static void Create(TestWebUIJsBridgeUI* controller,
                     mojo::PendingReceiver<mojom::Foo> pending_receiver,
                     mojo::PendingRemote<mojom::Bar> pending_remote) {
    auto foo_bar = base::WrapUnique(new FooBarImpl(
        controller, std::move(pending_receiver), std::move(pending_remote)));
    SaveWebUIManagedInterfaceInDocument(controller, std::move(foo_bar));
  }

  ~FooBarImpl() override = default;

  // mojom::Foo
  void GetFoo(GetFooCallback callback) override {
    std::move(callback).Run("foo-success");
  }

 private:
  FooBarImpl(TestWebUIJsBridgeUI* controller,
             mojo::PendingReceiver<mojom::Foo> pending_receiver,
             mojo::PendingRemote<mojom::Bar> pending_remote)
      : receiver_(this, std::move(pending_receiver)),
        remote_(std::move(pending_remote)) {
    remote_->GetBar(base::BindRepeating(
        [](const std::string& value) { EXPECT_EQ("bar-success", value); }));
  }

  mojo::Receiver<mojom::Foo> receiver_;
  mojo::Remote<mojom::Bar> remote_;
};

class Baz : public WebUIManagedInterfaceBase {
 public:
  static void Create(TestWebUIJsBridgeUI* controller,
                     mojo::PendingRemote<mojom::Baz> pending_remote) {
    auto baz = base::WrapUnique(new Baz(controller, std::move(pending_remote)));
    SaveWebUIManagedInterfaceInDocument(controller, std::move(baz));
  }

  ~Baz() override = default;

 private:
  Baz(TestWebUIJsBridgeUI* controller,
      mojo::PendingRemote<mojom::Baz> pending_remote)
      : remote_(std::move(pending_remote)) {
    remote_->GetBaz(base::BindRepeating(
        [](const std::string& value) { EXPECT_EQ("baz-success", value); }));
  }

  mojo::Remote<mojom::Baz> remote_;
};

class WebUIBrowserInterfaceBrokerRegistryBrowserTest
    : public ContentBrowserTest {
 public:
  WebUIBrowserInterfaceBrokerRegistryBrowserTest() = default;

  void SetUpOnMainThread() override {
    ContentBrowserTest::SetUpOnMainThread();
    test_content_browser_client_ = std::make_unique<TestContentBrowserClient>();

    WebUIConfigMap::GetInstance().AddWebUIConfig(
        std::make_unique<TestWebUIJsBridgeUIConfig>());
  }

  void TearDownOnMainThread() override { test_content_browser_client_.reset(); }

 private:
  class TestContentBrowserClient
      : public ContentBrowserTestContentBrowserClient {
   public:
    TestContentBrowserClient() = default;
    TestContentBrowserClient(const TestContentBrowserClient&) = delete;
    TestContentBrowserClient& operator=(const TestContentBrowserClient&) =
        delete;
    ~TestContentBrowserClient() override = default;

    void RegisterWebUIInterfaceBrokers(
        WebUIBrowserInterfaceBrokerRegistry& registry) override {
      registry.ForWebUIWithJsBridge<TestWebUIJsBridgeUI>()
          .AddBinder<mojom::Foo>(FooImpl::Create)
          .AddBinder<mojom::Foo, mojom::Bar>(FooBarImpl::Create)
          .AddBinder<mojom::Baz>(Baz::Create);
    }
  };

  std::unique_ptr<TestContentBrowserClient> test_content_browser_client_;
};

}  // namespace

// Tests that the registered Receiver-only binder works.
IN_PROC_BROWSER_TEST_F(WebUIBrowserInterfaceBrokerRegistryBrowserTest,
                       BindReceiverOnly) {
  WebContents* web_contents = shell()->web_contents();
  ASSERT_TRUE(NavigateToURL(web_contents, GURL(kTestWebUIJsBridgeUIUrl)));

  static constexpr char kScript[] = R"(
(async () => {
  let jsBridgeRemote = window.TestWebUIJsBridge.getRemote();
  let fooRemote = new FooRemote();
  jsBridgeRemote.bindFoo(fooRemote.$.bindNewPipeAndPassReceiver());

  return (await fooRemote.getFoo()).value;
})();)";

  EXPECT_EQ("foo-success", EvalJs(web_contents, kScript));
}

// Tests that the registered Receiver + Remote binder works.
IN_PROC_BROWSER_TEST_F(WebUIBrowserInterfaceBrokerRegistryBrowserTest,
                       BindReceiverAndRemote) {
  WebContents* web_contents = shell()->web_contents();
  ASSERT_TRUE(NavigateToURL(web_contents, GURL(kTestWebUIJsBridgeUIUrl)));

  static constexpr char kScript[] = R"(
(async () => {
  let jsBridgeRemote = window.TestWebUIJsBridge.getRemote();
  let fooRemote = new FooRemote();
  let barCallbackRouter = new BarCallbackRouter();
  let listenerPromise = new Promise(resolve => {
    barCallbackRouter.getBar.addListener(() => {
      /* Resolve the promise after the response is sent. */
      setTimeout(resolve, 0);
      return { value : 'bar-success' };
    });
  });

  jsBridgeRemote.bindFooBar(
    fooRemote.$.bindNewPipeAndPassReceiver(),
    barCallbackRouter.$.bindNewPipeAndPassRemote());

  /* Wait for the listener to be called. */
  await listenerPromise;
  /* Wait for the response to get to the browser. */
  await barCallbackRouter.$.flush();

  return (await fooRemote.getFoo()).value;
})();)";

  EXPECT_EQ("foo-success", EvalJs(web_contents, kScript));
}

// Tests that the registered Remote-only binder works.
IN_PROC_BROWSER_TEST_F(WebUIBrowserInterfaceBrokerRegistryBrowserTest,
                       BindRemote) {
  WebContents* web_contents = shell()->web_contents();
  ASSERT_TRUE(NavigateToURL(web_contents, GURL(kTestWebUIJsBridgeUIUrl)));

  static constexpr char kScript[] = R"(
(async () => {
  let jsBridgeRemote = window.TestWebUIJsBridge.getRemote();
  let bazCallbackRouter = new BazCallbackRouter();
  let listenerPromise = new Promise(resolve => {
    bazCallbackRouter.getBaz.addListener(() => {
      /* Resolve the promise after the response is sent. */
      setTimeout(resolve, 0);
      return { value: 'baz-success' };
    });
  });

  jsBridgeRemote.bindBaz(
    bazCallbackRouter.$.bindNewPipeAndPassRemote());

  /* Wait for the listener to be called. */
  await listenerPromise;
  /* Wait for the response to get to the browser. */
  await bazCallbackRouter.$.flush();
})();)";

  EXPECT_TRUE(ExecJs(web_contents, kScript));
}

}  // namespace content
