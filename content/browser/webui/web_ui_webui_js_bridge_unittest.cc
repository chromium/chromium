// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/supports_user_data.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "content/browser/webui/test_webui_js_bridge_ui.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/document_user_data.h"
#include "content/public/browser/per_web_ui_browser_interface_broker.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/test_web_ui.h"
#include "content/test/web_ui/test_secondary_interface.test-mojom.h"
#include "content/test/web_ui/webui_js_bridge_unittest.test-mojom-webui-js-bridge-impl.h"
#include "content/test/web_ui/webui_js_bridge_unittest2.test-mojom-webui-js-bridge-impl.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content::js_bridge_unittest {

namespace {

// WebUI interface implementation that gets created when receiving an interface
// request. It's owned by the WebUI's document.
class FooPageHandler : public mojom::FooPageHandler,
                       public DocumentUserData<FooPageHandler> {
 public:
  static void Create(TestWebUIJsBridgeUI* controller,
                     mojo::PendingReceiver<mojom::FooPageHandler> receiver,
                     mojo::PendingRemote<mojom::FooPage> remote) {
    FooPageHandler::CreateForCurrentDocument(
        controller->web_ui()->GetRenderFrameHost(), std::move(receiver),
        std::move(remote));
  }

  static FooPageHandler& Get(content::WebUIController* controller) {
    return *FooPageHandler::GetForCurrentDocument(
        controller->web_ui()->GetRenderFrameHost());
  }

  FooPageHandler(RenderFrameHost* rfh,
                 mojo::PendingReceiver<mojom::FooPageHandler> receiver,
                 mojo::PendingRemote<mojom::FooPage> remote)
      : DocumentUserData(rfh),
        receiver_(this, std::move(receiver)),
        remote_(std::move(remote)) {}

  ~FooPageHandler() override = default;

  const mojo::Receiver<mojom::FooPageHandler>& receiver() { return receiver_; }
  const mojo::Remote<mojom::FooPage>& remote() { return remote_; }

 private:
  friend DocumentUserData;
  DOCUMENT_USER_DATA_KEY_DECL();

  mojo::Receiver<mojom::FooPageHandler> receiver_;
  mojo::Remote<mojom::FooPage> remote_;
};

DOCUMENT_USER_DATA_KEY_IMPL(FooPageHandler);

class FooPage : public mojom::FooPage {
 public:
  FooPage() = default;
  ~FooPage() override = default;

  mojo::Receiver<mojom::FooPage>& receiver() { return receiver_; }

 private:
  mojo::Receiver<mojom::FooPage> receiver_{this};
};

// WebUI interface implementation that is created and owned outside of the
// WebUI. In this case, it's owned by BrowserContext.
class Bar : public mojom::Bar, public base::SupportsUserData::Data {
 public:
  Bar() = default;
  ~Bar() override = default;

  static void Create(BrowserContext* browser_context) {
    browser_context->SetUserData("BarImpl", std::make_unique<Bar>());
  }

  static Bar& Get(BrowserContext* browser_context) {
    Bar* bar = static_cast<Bar*>(browser_context->GetUserData("BarImpl"));
    return *bar;
  }

  static void BindBar(TestWebUIJsBridgeUI* controller,
                      mojo::PendingReceiver<mojom::Bar> receiver) {
    Bar::Get(controller->web_ui()->GetWebContents()->GetBrowserContext())
        .BindBarImpl(std::move(receiver));
  }

  static void BindObserver(TestWebUIJsBridgeUI* controller,
                           mojo::PendingRemote<mojom::BarObserver> remote) {
    Bar::Get(controller->web_ui()->GetWebContents()->GetBrowserContext())
        .BindObserverImpl(std::move(remote));
  }

  const mojo::ReceiverSet<mojom::Bar>& receivers() { return receivers_; }
  const mojo::RemoteSet<mojom::BarObserver>& observers() { return observers_; }

 private:
  void BindBarImpl(mojo::PendingReceiver<mojom::Bar> receiver) {
    receivers_.Add(this, std::move(receiver));
  }

  void BindObserverImpl(mojo::PendingRemote<mojom::BarObserver> remote) {
    observers_.Add(std::move(remote));
  }

  mojo::ReceiverSet<mojom::Bar> receivers_;
  mojo::RemoteSet<mojom::BarObserver> observers_;
};

class BarObserver : public mojom::BarObserver {
 public:
  BarObserver() = default;
  ~BarObserver() override = default;

  mojo::Receiver<mojom::BarObserver>& receiver() { return receiver_; }

 private:
  mojo::Receiver<mojom::BarObserver> receiver_{this};
};

}  // namespace

class WebUIJsBridgeTest : public RenderViewHostTestHarness {
 public:
  WebUIJsBridgeTest() = default;
  ~WebUIJsBridgeTest() override = default;

  void SetUp() override {
    RenderViewHostTestHarness::SetUp();
    Bar::Create(browser_context());
    test_web_ui.set_web_contents(web_contents());
    test_web_ui.set_render_frame_host(main_rfh());
  }

  void TearDown() override {
    test_web_ui.set_web_contents(nullptr);
    RenderViewHostTestHarness::TearDown();
  }

  TestWebUI* web_ui() { return &test_web_ui; }

 private:
  TestWebUI test_web_ui;
};

// Tests binder methods are overridden and can be called.
TEST_F(WebUIJsBridgeTest, Bind) {
  mojom::FooWebUIJsBridgeBinderInitializer binder_initializer;
  binder_initializer
      .AddBinder<mojom::FooPageHandler, mojom::FooPage>(FooPageHandler::Create)
      .AddBinder<mojom::Bar>(Bar::BindBar)
      .AddBinder<mojom::BarObserver>(Bar::BindObserver);

  TestWebUIJsBridgeUI controller(web_ui());
  mojo::Remote<mojom::FooWebUIJsBridge> js_bridge_remote;

  auto binder = binder_initializer.GetWebUIJsBridgeBinder();
  binder.Run(&controller, js_bridge_remote.BindNewPipeAndPassReceiver());

  mojo::Remote<mojom::FooPageHandler> page_handler_remote;
  FooPage page;
  js_bridge_remote->BindFooPageHandler(
      page_handler_remote.BindNewPipeAndPassReceiver(),
      page.receiver().BindNewPipeAndPassRemote());
  js_bridge_remote.FlushForTesting();

  auto& page_handler = FooPageHandler::Get(&controller);
  EXPECT_TRUE(page_handler_remote.is_bound());
  EXPECT_TRUE(page.receiver().is_bound());
  EXPECT_TRUE(page_handler.receiver().is_bound());
  EXPECT_TRUE(page_handler.remote().is_bound());

  mojo::Remote<mojom::Bar> bar_remote;
  js_bridge_remote->BindBar(bar_remote.BindNewPipeAndPassReceiver());
  js_bridge_remote.FlushForTesting();

  auto& bar = Bar::Get(browser_context());
  EXPECT_TRUE(bar_remote.is_bound());
  EXPECT_EQ(1u, bar.receivers().size());

  BarObserver observer;
  js_bridge_remote->BindBarObserver(
      observer.receiver().BindNewPipeAndPassRemote());
  js_bridge_remote.FlushForTesting();

  EXPECT_TRUE(observer.receiver().is_bound());
  EXPECT_EQ(1u, bar.observers().size());
}

// Making this a lambda causes a compile error.
static void EmptyBinder(
    TestWebUIJsBridgeUI2*,
    mojo::PendingReceiver<secondary::mojom::SecondaryInterface>) {}

// Tests we correctly generate a WebUIJsBridgeBinderInitializer for an interface
// that binds interfaces in a separate mojom.
TEST_F(WebUIJsBridgeTest, CrossModule) {
  mojom::TestWebUIJsBridge2BinderInitializer initializer;
  initializer.AddBinder<secondary::mojom::SecondaryInterface>(&EmptyBinder);
}

// Tests that we crash if the wrong WebUIController is passed to the
// WebUIJsBridge.
TEST_F(WebUIJsBridgeTest, IncorrectWebUIControllerCrash) {
  mojom::TestWebUIJsBridge2BinderInitializer initializer;
  initializer.AddBinder<secondary::mojom::SecondaryInterface>(&EmptyBinder);

  // The `TestWebUIJsBridge2` binder below expects a `TestWebUIJsBridgeUI2`
  // WebUIController, but we pass it a `TestWebUIJsBridgeUI` controller, which
  // should cause a crash.
  TestWebUIJsBridgeUI controller(web_ui());

  mojo::Remote<mojom::TestWebUIJsBridge2> js_bridge_remote;
  auto binder = initializer.GetWebUIJsBridgeBinder();

  EXPECT_DEATH_IF_SUPPORTED(
      binder.Run(&controller, js_bridge_remote.BindNewPipeAndPassReceiver()),
      "");
}

}  // namespace content::js_bridge_unittest
