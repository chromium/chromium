// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/service_manager/public/cpp/standalone_connector_impl.h"

#include <optional>

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "mojo/public/cpp/bindings/generic_pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/service_manager/public/cpp/standalone_connector_impl_unittest.test-mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace service_manager {
namespace standalone_connector_impl_unittest {

class TestConnectorDelegate : public StandaloneConnectorImpl::Delegate {
 public:
  template <typename Handler>
  TestConnectorDelegate(Handler handler)
      : TestConnectorDelegate(base::BindLambdaForTesting(handler)) {}

  TestConnectorDelegate(const TestConnectorDelegate&) = delete;
  TestConnectorDelegate& operator=(const TestConnectorDelegate&) = delete;

  ~TestConnectorDelegate() override = default;

 private:
  using Callback = base::RepeatingCallback<void(const std::string&,
                                                mojo::GenericPendingReceiver)>;

  explicit TestConnectorDelegate(Callback callback)
      : callback_(std::move(callback)) {}

  // StandaloneConnectorImpl::Delegate implementation:
  void OnConnect(const std::string& service_name,
                 mojo::GenericPendingReceiver receiver) override {
    callback_.Run(service_name, std::move(receiver));
  }

  const Callback callback_;
};

class StandaloneConnectorImplTest : public testing::Test {
 public:
  StandaloneConnectorImplTest() = default;

 private:
  base::test::TaskEnvironment task_environment_;
};

TEST_F(StandaloneConnectorImplTest, Connect) {
  const std::string kFooServiceName = "foo_service";
  const std::string kBarServiceName = "bar_service";

  int requests_processed = 0;
  std::optional<base::RunLoop> loop;
  TestConnectorDelegate delegate([&](const std::string& service_name,
                                     mojo::GenericPendingReceiver receiver) {
    ASSERT_TRUE(receiver);
    if (service_name == kFooServiceName) {
      EXPECT_EQ(mojom::Foo::Name_, *receiver.interface_name());
    } else {
      EXPECT_EQ(kBarServiceName, service_name);
      EXPECT_EQ(mojom::Bar::Name_, *receiver.interface_name());
    }

    ++requests_processed;
    loop->Quit();
  });

  StandaloneConnectorImpl impl(&delegate);
  Connector connector(impl.MakeRemote());

  mojo::Remote<mojom::Foo> foo;
  connector.Connect(kFooServiceName, foo.BindNewPipeAndPassReceiver());
  loop.emplace();
  loop->Run();
  EXPECT_EQ(1, requests_processed);

  mojo::Remote<mojom::Bar> bar;
  connector.Connect(kBarServiceName, bar.BindNewPipeAndPassReceiver());
  loop.emplace();
  loop->Run();
  EXPECT_EQ(2, requests_processed);
}

TEST_F(StandaloneConnectorImplTest, Clone) {
  const std::string kFooServiceName = "foo_service";
  base::RunLoop loop;
  TestConnectorDelegate delegate([&](const std::string& service_name,
                                     mojo::GenericPendingReceiver receiver) {
    ASSERT_TRUE(receiver);
    EXPECT_EQ(kFooServiceName, service_name);
    EXPECT_EQ(mojom::Foo::Name_, *receiver.interface_name());
    loop.Quit();
  });
  StandaloneConnectorImpl impl(&delegate);
  Connector connector(impl.MakeRemote());

  auto clone = connector.Clone();
  mojo::Remote<mojom::Foo> foo;
  clone->Connect(kFooServiceName, foo.BindNewPipeAndPassReceiver());
  loop.Run();
}

}  // namespace standalone_connector_impl_unittest
}  // namespace service_manager
