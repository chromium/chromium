// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "content/browser/webui/test_webui_js_bridge_ui.h"
#include "content/public/test/test_web_ui.h"
#include "content/test/web_ui/webui_js_bridge_unittest.test-mojom-webui-js-bridge-impl.h"
#include "content/test/web_ui/webui_js_bridge_unittest2.test-mojom-webui-js-bridge-impl.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

class FooPageHandler : public mojom::FooPageHandler {
 public:
  FooPageHandler(mojo::PendingReceiver<mojom::FooPageHandler> receiver,
                 mojo::PendingRemote<mojom::FooPage> remote)
      : receiver_(this, std::move(receiver)), remote_(std::move(remote)) {}
  ~FooPageHandler() override = default;

  const mojo::Receiver<mojom::FooPageHandler>& receiver() { return receiver_; }
  const mojo::Remote<mojom::FooPage>& remote() { return remote_; }

 private:
  mojo::Receiver<mojom::FooPageHandler> receiver_;
  mojo::Remote<mojom::FooPage> remote_;
};

class FooPage : public mojom::FooPage {
 public:
  FooPage() = default;
  ~FooPage() override = default;

  mojo::Receiver<mojom::FooPage>& receiver() { return receiver_; }

 private:
  mojo::Receiver<mojom::FooPage> receiver_{this};
};

class Bar : public mojom::Bar {
 public:
  Bar() = default;
  ~Bar() override = default;

  void BindBar(mojo::PendingReceiver<mojom::Bar> receiver) {
    receivers_.Add(this, std::move(receiver));
  }

  void BindObserver(mojo::PendingRemote<mojom::BarObserver> remote) {
    observers_.Add(std::move(remote));
  }

  const mojo::ReceiverSet<mojom::Bar>& receivers() { return receivers_; }
  const mojo::RemoteSet<mojom::BarObserver>& observers() { return observers_; }

 private:
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

class WebUIJsBridgeTest : public testing::Test {
 public:
  WebUIJsBridgeTest() = default;
  ~WebUIJsBridgeTest() override = default;

  TestWebUI* web_ui() { return &test_web_ui; }

 private:
  base::test::TaskEnvironment task_environment_;
  TestWebUI test_web_ui;
};

// Tests binder methods are overridden and can be called. Calling them does
// nothing for now.
TEST_F(WebUIJsBridgeTest, Bind) {
  std::unique_ptr<FooPageHandler> page_handler;
  auto page_handler_binder = base::BindLambdaForTesting(
      [&page_handler](mojo::PendingReceiver<mojom::FooPageHandler> receiver,
                      mojo::PendingRemote<mojom::FooPage> remote) {
        page_handler = std::make_unique<FooPageHandler>(std::move(receiver),
                                                        std::move(remote));
      });
  Bar bar;

  TestWebUIJsBridgeUI controller(web_ui());
  mojom::FooWebUIJsBridgeImpl bridge(
      &controller, page_handler_binder,
      base::BindRepeating(&Bar::BindBar, base::Unretained(&bar)),
      base::BindRepeating(&Bar::BindObserver, base::Unretained(&bar)));

  mojo::Remote<mojom::FooPageHandler> page_handler_remote;
  FooPage page;
  bridge.BindFooPageHandler(page_handler_remote.BindNewPipeAndPassReceiver(),
                            page.receiver().BindNewPipeAndPassRemote());
  EXPECT_TRUE(page_handler_remote.is_bound());
  EXPECT_TRUE(page.receiver().is_bound());
  EXPECT_TRUE(page_handler->receiver().is_bound());
  EXPECT_TRUE(page_handler->remote().is_bound());

  mojo::Remote<mojom::Bar> bar_remote;
  bridge.BindBar(bar_remote.BindNewPipeAndPassReceiver());
  EXPECT_TRUE(bar_remote.is_bound());
  EXPECT_EQ(1u, bar.receivers().size());

  BarObserver observer;
  bridge.BindBarObserver(observer.receiver().BindNewPipeAndPassRemote());
  EXPECT_TRUE(observer.receiver().is_bound());
  EXPECT_EQ(1u, bar.observers().size());
}

// Tests we correctly generate a WebUIJsBridgeImpl for a interface that
// binds interfaces in a separate mojom.
TEST_F(WebUIJsBridgeTest, CrossModule) {
  TestWebUIJsBridgeUI controller(web_ui());
  mojom::TestWebUIJsBridge2Impl bridge(&controller, base::DoNothing());
  bridge.BindSecondaryInterface(mojo::NullReceiver());
}

// Tests that we crash if the wrong WebUIController is passed to the
// WebUIJsBridge.
TEST_F(WebUIJsBridgeTest, IncorrectWebUIControllerCrash) {
  TestWebUIJsBridgeIncorrectUI controller(web_ui());
  EXPECT_DEATH_IF_SUPPORTED(
      mojom::TestWebUIJsBridge2Impl bridge(&controller, base::DoNothing()), "");
}

}  // namespace content
