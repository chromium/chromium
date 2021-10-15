// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/barrier_closure.h"
#include "base/command_line.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/test_future.h"
#include "content/browser/serial/serial_test_utils.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "content/test/test_render_view_host.h"
#include "content/test/test_web_contents.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/device/public/cpp/test/fake_serial_port_client.h"
#include "services/device/public/cpp/test/fake_serial_port_manager.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

using ::base::test::TestFuture;
using ::testing::_;
using ::testing::Return;

const char kTestUrl[] = "https://www.google.com";
const char kCrossOriginTestUrl[] = "https://www.chromium.org";

class MockSerialServiceClient : public blink::mojom::SerialServiceClient {
 public:
  MockSerialServiceClient() = default;
  MockSerialServiceClient(const MockSerialServiceClient&) = delete;
  MockSerialServiceClient& operator=(const MockSerialServiceClient&) = delete;

  ~MockSerialServiceClient() override {
    // Flush the pipe to make sure there aren't any lingering events.
    receiver_.FlushForTesting();
  }

  mojo::PendingRemote<blink::mojom::SerialServiceClient>
  BindNewPipeAndPassRemote() {
    return receiver_.BindNewPipeAndPassRemote();
  }

  // blink::mojom::SerialPortManagerClient
  MOCK_METHOD1(OnPortAdded, void(blink::mojom::SerialPortInfoPtr));
  MOCK_METHOD1(OnPortRemoved, void(blink::mojom::SerialPortInfoPtr));

 private:
  mojo::Receiver<blink::mojom::SerialServiceClient> receiver_{this};
};

class SerialTest : public RenderViewHostImplTestHarness {
 public:
  SerialTest() {
    ON_CALL(delegate(), GetPortManager).WillByDefault(Return(&port_manager_));
    ON_CALL(delegate(), AddObserver)
        .WillByDefault(testing::SaveArg<1>(&observer_));
  }

  SerialTest(const SerialTest&) = delete;
  SerialTest& operator=(const SerialTest&) = delete;

  ~SerialTest() override = default;

  void SetUp() override {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kEnableExperimentalWebPlatformFeatures);
    original_client_ = SetBrowserClientForTesting(&test_client_);
    RenderViewHostTestHarness::SetUp();
  }

  void TearDown() override {
    RenderViewHostTestHarness::TearDown();
    if (original_client_)
      SetBrowserClientForTesting(original_client_);
  }

  MockSerialDelegate& delegate() { return test_client_.delegate(); }
  device::FakeSerialPortManager* port_manager() { return &port_manager_; }
  SerialDelegate::Observer* observer() { return observer_; }

 private:
  SerialTestContentBrowserClient test_client_;
  raw_ptr<ContentBrowserClient> original_client_ = nullptr;
  device::FakeSerialPortManager port_manager_;
  SerialDelegate::Observer* observer_ = nullptr;
};

}  // namespace

TEST_F(SerialTest, OpenAndClosePort) {
  NavigateAndCommit(GURL(kTestUrl));

  mojo::Remote<blink::mojom::SerialService> service;
  contents()->GetMainFrame()->BindSerialService(
      service.BindNewPipeAndPassReceiver());

  auto token = base::UnguessableToken::Create();
  auto port_info = device::mojom::SerialPortInfo::New();
  port_info->token = token;
  port_manager()->AddPort(std::move(port_info));

  EXPECT_FALSE(contents()->IsConnectedToSerialPort());

  TestFuture<mojo::PendingRemote<device::mojom::SerialPort>> future;
  service->OpenPort(token, device::mojom::SerialConnectionOptions::New(),
                    device::FakeSerialPortClient::Create(),
                    future.GetCallback());
  auto port = future.Take();
  EXPECT_TRUE(port.is_valid());
  EXPECT_TRUE(contents()->IsConnectedToSerialPort());

  port.reset();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(contents()->IsConnectedToSerialPort());
}

TEST_F(SerialTest, OpenFailure) {
  NavigateAndCommit(GURL(kTestUrl));

  mojo::Remote<blink::mojom::SerialService> service;
  contents()->GetMainFrame()->BindSerialService(
      service.BindNewPipeAndPassReceiver());

  auto token = base::UnguessableToken::Create();
  auto port_info = device::mojom::SerialPortInfo::New();
  port_info->token = token;
  port_manager()->AddPort(std::move(port_info));
  port_manager()->set_simulate_open_failure(true);

  EXPECT_FALSE(contents()->IsConnectedToSerialPort());

  TestFuture<mojo::PendingRemote<device::mojom::SerialPort>> future;
  service->OpenPort(token, device::mojom::SerialConnectionOptions::New(),
                    device::FakeSerialPortClient::Create(),
                    future.GetCallback());
  auto port = future.Take();
  EXPECT_FALSE(port.is_valid());

  // Allow extra time for the watcher connection failure to propagate.
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(contents()->IsConnectedToSerialPort());
}

TEST_F(SerialTest, OpenAndNavigateCrossOrigin) {
  NavigateAndCommit(GURL(kTestUrl));

  mojo::Remote<blink::mojom::SerialService> service;
  contents()->GetMainFrame()->BindSerialService(
      service.BindNewPipeAndPassReceiver());

  auto token = base::UnguessableToken::Create();
  auto port_info = device::mojom::SerialPortInfo::New();
  port_info->token = token;
  port_manager()->AddPort(std::move(port_info));

  EXPECT_FALSE(contents()->IsConnectedToSerialPort());

  TestFuture<mojo::PendingRemote<device::mojom::SerialPort>> future;
  service->OpenPort(token, device::mojom::SerialConnectionOptions::New(),
                    device::FakeSerialPortClient::Create(),
                    future.GetCallback());
  mojo::Remote<device::mojom::SerialPort> port(future.Take());
  EXPECT_TRUE(port.is_connected());
  EXPECT_TRUE(contents()->IsConnectedToSerialPort());

  NavigateAndCommit(GURL(kCrossOriginTestUrl));
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(contents()->IsConnectedToSerialPort());
  port.FlushForTesting();
  EXPECT_FALSE(port.is_connected());
}

TEST_F(SerialTest, AddAndRemovePorts) {
  mojo::Remote<blink::mojom::SerialService> service;
  contents()->GetMainFrame()->BindSerialService(
      service.BindNewPipeAndPassReceiver());

  MockSerialServiceClient client;
  service->SetClient(client.BindNewPipeAndPassRemote());
  service.FlushForTesting();

  ASSERT_TRUE(observer());

  // Three ports will be added and then removed. Only the 1st and 3rd will have
  // permission granted.
  std::vector<device::mojom::SerialPortInfoPtr> ports;
  for (size_t i = 0; i < 3; i++) {
    auto port = device::mojom::SerialPortInfo::New();
    port->token = base::UnguessableToken::Create();
    ports.push_back(std::move(port));
  }

  EXPECT_CALL(delegate(), HasPortPermission(_, _))
      .WillOnce(Return(true))
      .WillOnce(Return(false))
      .WillOnce(Return(true))
      .WillOnce(Return(true))
      .WillOnce(Return(false))
      .WillOnce(Return(true));

  {
    base::RunLoop run_loop;
    auto closure = base::BarrierClosure(2, run_loop.QuitClosure());
    EXPECT_CALL(client, OnPortAdded(_))
        .Times(2)
        .WillRepeatedly(base::test::RunClosure(closure));

    for (const auto& port : ports)
      observer()->OnPortAdded(*port);
    run_loop.Run();
  }

  {
    base::RunLoop run_loop;
    auto closure = base::BarrierClosure(2, run_loop.QuitClosure());
    EXPECT_CALL(client, OnPortRemoved(_))
        .Times(2)
        .WillRepeatedly(base::test::RunClosure(closure));

    for (const auto& port : ports)
      observer()->OnPortRemoved(*port);
    run_loop.Run();
  }
}

TEST_F(SerialTest, OpenAndClosePortManagerConnection) {
  NavigateAndCommit(GURL(kTestUrl));

  mojo::Remote<blink::mojom::SerialService> service;
  contents()->GetMainFrame()->BindSerialService(
      service.BindNewPipeAndPassReceiver());

  auto token = base::UnguessableToken::Create();
  auto port_info = device::mojom::SerialPortInfo::New();
  port_info->token = token;
  port_manager()->AddPort(std::move(port_info));

  EXPECT_FALSE(contents()->IsConnectedToSerialPort());

  TestFuture<mojo::PendingRemote<device::mojom::SerialPort>> future;
  service->OpenPort(token, device::mojom::SerialConnectionOptions::New(),
                    device::FakeSerialPortClient::Create(),
                    future.GetCallback());
  mojo::Remote<device::mojom::SerialPort> port(future.Take());
  EXPECT_TRUE(port.is_connected());
  EXPECT_TRUE(contents()->IsConnectedToSerialPort());

  ASSERT_TRUE(observer());
  observer()->OnPortManagerConnectionError();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(contents()->IsConnectedToSerialPort());
  port.FlushForTesting();
  EXPECT_FALSE(port.is_connected());
  service.FlushForTesting();
  EXPECT_FALSE(service.is_connected());
}

}  // namespace content
