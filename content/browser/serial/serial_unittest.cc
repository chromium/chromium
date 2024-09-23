// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/barrier_closure.h"
#include "base/command_line.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/repeating_test_future.h"
#include "base/test/test_future.h"
#include "content/browser/serial/serial_test_utils.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/navigation_simulator.h"
#include "content/test/test_render_view_host.h"
#include "content/test/test_web_contents.h"
#include "device/bluetooth/public/cpp/bluetooth_uuid.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/test_support/fake_message_dispatch_context.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "services/device/public/cpp/test/fake_serial_port_client.h"
#include "services/device/public/cpp/test/fake_serial_port_manager.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/serial/serial.mojom-blink.h"
#include "url/origin.h"

namespace content {

namespace {

using ::base::test::InvokeFuture;
using ::base::test::TestFuture;
using ::testing::_;
using ::testing::Invoke;
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
  MOCK_METHOD1(OnPortConnectedStateChanged,
               void(blink::mojom::SerialPortInfoPtr));

 private:
  mojo::Receiver<blink::mojom::SerialServiceClient> receiver_{this};
};

class SerialTest : public RenderViewHostImplTestHarness {
 public:
  SerialTest() {
    ON_CALL(delegate(), GetPortManager).WillByDefault(Return(&port_manager_));
    ON_CALL(delegate(), AddObserver)
        .WillByDefault(testing::SaveArg<1>(&observer_));
    ON_CALL(delegate(), RemoveObserver)
        .WillByDefault([&](RenderFrameHost*, SerialDelegate::Observer*) {
          observer_ = nullptr;
        });
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
  raw_ptr<SerialDelegate::Observer> observer_ = nullptr;
};

}  // namespace

TEST_F(SerialTest, GetPortsForAllDeviceTypes) {
  NavigateAndCommit(GURL(kTestUrl));

  mojo::Remote<blink::mojom::SerialService> service;
  contents()->GetPrimaryMainFrame()->BindSerialService(
      service.BindNewPipeAndPassReceiver());

  MockSerialServiceClient client;
  service->SetClient(client.BindNewPipeAndPassRemote());
  service.FlushForTesting();

  // Platform Serial port
  auto platform_token = base::UnguessableToken::Create();
  auto platform_port_info = device::mojom::SerialPortInfo::New();
  platform_port_info->token = platform_token;
  port_manager()->AddPort(std::move(platform_port_info));

  // USB Serial port
  auto usb_token = base::UnguessableToken::Create();
  auto usb_port_info = device::mojom::SerialPortInfo::New();
  usb_port_info->token = usb_token;
  usb_port_info->has_vendor_id = true;
  usb_port_info->vendor_id = 0x1111;
  usb_port_info->has_product_id = true;
  usb_port_info->product_id = 0x2222;
  port_manager()->AddPort(std::move(usb_port_info));

  // Bluetooth Serial port
  const device::BluetoothUUID kServiceClassId(
      "ac822b69-d7e9-4bab-8fa6-ce40c87e1ac4");
  auto bluetooth_token = base::UnguessableToken::Create();
  auto bluetooth_port_info = device::mojom::SerialPortInfo::New();
  bluetooth_port_info->token = bluetooth_token;
  bluetooth_port_info->bluetooth_service_class_id = kServiceClassId;
  port_manager()->AddPort(std::move(bluetooth_port_info));

  EXPECT_CALL(delegate(), HasPortPermission(_, _))
      .Times(3)
      .WillRepeatedly(Return(true));
  TestFuture<std::vector<blink::mojom::SerialPortInfoPtr>> future;
  service->GetPorts(future.GetCallback());

  auto ports = future.Take();
  // Assert as we need to access all three ports.
  ASSERT_EQ(ports.size(), 3u);
  // Ports are not in any particular order so we will loop through and check
  // according to the token.
  bool has_platform = false;
  bool has_usb = false;
  bool has_bluetooth = false;
  for (const auto& port : ports) {
    if (port->token == platform_token) {
      has_platform = true;
      EXPECT_EQ(port->has_usb_vendor_id, false);
      EXPECT_EQ(port->has_usb_product_id, false);
      EXPECT_FALSE(port->bluetooth_service_class_id.has_value());
    } else if (port->token == usb_token) {
      has_usb = true;
      EXPECT_EQ(port->has_usb_vendor_id, true);
      EXPECT_EQ(port->usb_vendor_id, 0x1111);
      EXPECT_EQ(port->has_usb_product_id, true);
      EXPECT_EQ(port->usb_product_id, 0x2222);
      EXPECT_FALSE(port->bluetooth_service_class_id.has_value());
    } else if (port->token == bluetooth_token) {
      has_bluetooth = true;
      EXPECT_EQ(port->has_usb_vendor_id, false);
      EXPECT_EQ(port->has_usb_product_id, false);
      EXPECT_EQ(port->bluetooth_service_class_id, kServiceClassId);
    } else {
      ADD_FAILURE() << "Unexpected port token: " << port->token;
    }
  }
  EXPECT_TRUE(has_platform);
  EXPECT_TRUE(has_usb);
  EXPECT_TRUE(has_bluetooth);
}

TEST_F(SerialTest, OpenAndClosePort) {
  NavigateAndCommit(GURL(kTestUrl));

  mojo::Remote<blink::mojom::SerialService> service;
  contents()->GetPrimaryMainFrame()->BindSerialService(
      service.BindNewPipeAndPassReceiver());

  auto token = base::UnguessableToken::Create();
  auto port_info = device::mojom::SerialPortInfo::New();
  port_info->token = token;
  port_manager()->AddPort(port_info->Clone());

  EXPECT_FALSE(contents()->IsConnectedToSerialPort());

  EXPECT_CALL(delegate(), GetPortInfo(_, _)).WillOnce(Return(port_info.get()));
  EXPECT_CALL(delegate(), HasPortPermission(_, _)).WillOnce(Return(true));

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

TEST_F(SerialTest, OpenWithoutPermission) {
  NavigateAndCommit(GURL(kTestUrl));

  mojo::Remote<blink::mojom::SerialService> service;
  contents()->GetPrimaryMainFrame()->BindSerialService(
      service.BindNewPipeAndPassReceiver());

  auto token = base::UnguessableToken::Create();
  auto port_info = device::mojom::SerialPortInfo::New();
  port_info->token = token;
  port_manager()->AddPort(port_info->Clone());

  EXPECT_FALSE(contents()->IsConnectedToSerialPort());

  EXPECT_CALL(delegate(), GetPortInfo(_, _)).WillOnce(Return(port_info.get()));
  EXPECT_CALL(delegate(), HasPortPermission(_, _)).WillOnce(Return(false));

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

TEST_F(SerialTest, OpenFailure) {
  NavigateAndCommit(GURL(kTestUrl));

  mojo::Remote<blink::mojom::SerialService> service;
  contents()->GetPrimaryMainFrame()->BindSerialService(
      service.BindNewPipeAndPassReceiver());

  auto token = base::UnguessableToken::Create();
  auto port_info = device::mojom::SerialPortInfo::New();
  port_info->token = token;
  port_manager()->AddPort(port_info->Clone());
  port_manager()->set_simulate_open_failure(true);

  EXPECT_FALSE(contents()->IsConnectedToSerialPort());

  EXPECT_CALL(delegate(), GetPortInfo(_, _)).WillOnce(Return(port_info.get()));
  EXPECT_CALL(delegate(), HasPortPermission(_, _)).WillOnce(Return(true));

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
  contents()->GetPrimaryMainFrame()->BindSerialService(
      service.BindNewPipeAndPassReceiver());

  auto token = base::UnguessableToken::Create();
  auto port_info = device::mojom::SerialPortInfo::New();
  port_info->token = token;
  port_manager()->AddPort(port_info->Clone());

  EXPECT_FALSE(contents()->IsConnectedToSerialPort());

  EXPECT_CALL(delegate(), GetPortInfo(_, _)).WillOnce(Return(port_info.get()));
  EXPECT_CALL(delegate(), HasPortPermission(_, _)).WillOnce(Return(true));

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

TEST_F(SerialTest, SameBluetoothSerialPortSameToken) {
  NavigateAndCommit(GURL(kTestUrl));
  mojo::Remote<blink::mojom::SerialService> service;
  contents()->GetPrimaryMainFrame()->BindSerialService(
      service.BindNewPipeAndPassReceiver());
  MockSerialServiceClient client;
  service->SetClient(client.BindNewPipeAndPassRemote());
  service.FlushForTesting();
  ASSERT_TRUE(observer());

  const device::BluetoothUUID kServiceClassId(
      "ac822b69-d7e9-4bab-8fa6-ce40c87e1ac4");
  base::UnguessableToken bluetooth_token;
  std::vector<device::mojom::SerialPortInfoPtr> ports;
  for (size_t i = 0; i < 2; i++) {
    auto port = device::mojom::SerialPortInfo::New();
    port->token = base::UnguessableToken::Create();
    port->bluetooth_service_class_id = kServiceClassId;
    ports.push_back(std::move(port));
    if (i == 0) {
      // Both SerialPortInfos describe the same port (same device address and
      // service UUID). When the ports are delivered to the renderer, the
      // second port reuses the token from the first port even though they were
      // created with different tokens.
      bluetooth_token = ports[i]->token;
    }
  }

  for (size_t i = 0; i < 2; i++) {
    TestFuture<blink::mojom::SerialPortInfoPtr> future;
    EXPECT_CALL(delegate(), HasPortPermission(_, _)).WillOnce(Return(true));
    EXPECT_CALL(client, OnPortConnectedStateChanged)
        .WillOnce(InvokeFuture(future));
    observer()->OnPortAdded(*ports[i]);
    EXPECT_EQ(future.Get()->token, bluetooth_token);
  }
}

TEST_F(SerialTest, AddAndRemovePorts) {
  NavigateAndCommit(GURL(kTestUrl));

  mojo::Remote<blink::mojom::SerialService> service;
  contents()->GetPrimaryMainFrame()->BindSerialService(
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
    EXPECT_CALL(client, OnPortConnectedStateChanged)
        .Times(2)
        .WillRepeatedly(base::test::RunClosure(closure));

    for (const auto& port : ports)
      observer()->OnPortAdded(*port);
    run_loop.Run();
  }

  {
    base::RunLoop run_loop;
    auto closure = base::BarrierClosure(2, run_loop.QuitClosure());
    EXPECT_CALL(client, OnPortConnectedStateChanged)
        .Times(2)
        .WillRepeatedly(base::test::RunClosure(closure));

    for (const auto& port : ports)
      observer()->OnPortRemoved(*port);
    run_loop.Run();
  }
}

TEST_F(SerialTest, PortConnectedState) {
  NavigateAndCommit(GURL(kTestUrl));

  mojo::Remote<blink::mojom::SerialService> service;
  contents()->GetPrimaryMainFrame()->BindSerialService(
      service.BindNewPipeAndPassReceiver());

  MockSerialServiceClient client;
  service->SetClient(client.BindNewPipeAndPassRemote());
  service.FlushForTesting();

  ASSERT_TRUE(observer());

  // Create a disconnected port.
  auto port = device::mojom::SerialPortInfo::New();
  port->token = base::UnguessableToken::Create();
  port->connected = false;

  EXPECT_CALL(delegate(), HasPortPermission).WillRepeatedly(Return(true));

  // Add the disconnected port. The client is not notified.
  EXPECT_CALL(client, OnPortConnectedStateChanged).Times(0);
  observer()->OnPortAdded(*port);
  base::RunLoop().RunUntilIdle();

  // Connect the port.
  TestFuture<blink::mojom::SerialPortInfoPtr> connect_future;
  EXPECT_CALL(client, OnPortConnectedStateChanged)
      .WillOnce(InvokeFuture(connect_future));
  port->connected = true;
  observer()->OnPortConnectedStateChanged(*port);
  EXPECT_EQ(connect_future.Get()->token, port->token);
  EXPECT_TRUE(connect_future.Get()->connected);

  // Disconnect the port.
  TestFuture<blink::mojom::SerialPortInfoPtr> disconnect_future;
  EXPECT_CALL(client, OnPortConnectedStateChanged)
      .WillOnce(InvokeFuture(disconnect_future));
  port->connected = false;
  observer()->OnPortConnectedStateChanged(*port);
  EXPECT_EQ(disconnect_future.Get()->token, port->token);
  EXPECT_FALSE(disconnect_future.Get()->connected);
}

TEST_F(SerialTest, OpenAndClosePortManagerConnection) {
  NavigateAndCommit(GURL(kTestUrl));

  mojo::Remote<blink::mojom::SerialService> service;
  contents()->GetPrimaryMainFrame()->BindSerialService(
      service.BindNewPipeAndPassReceiver());

  auto token = base::UnguessableToken::Create();
  auto port_info = device::mojom::SerialPortInfo::New();
  port_info->token = token;
  port_manager()->AddPort(port_info->Clone());

  EXPECT_FALSE(contents()->IsConnectedToSerialPort());

  EXPECT_CALL(delegate(), GetPortInfo(_, _)).WillOnce(Return(port_info.get()));
  EXPECT_CALL(delegate(), HasPortPermission(_, _)).WillOnce(Return(true));

  TestFuture<mojo::PendingRemote<device::mojom::SerialPort>> future;
  service->OpenPort(token, device::mojom::SerialConnectionOptions::New(),
                    device::FakeSerialPortClient::Create(),
                    future.GetCallback());
  mojo::Remote<device::mojom::SerialPort> port(future.Take());
  EXPECT_TRUE(port.is_connected());
  EXPECT_TRUE(contents()->IsConnectedToSerialPort());

  ASSERT_TRUE(observer());
  observer()->OnPortManagerConnectionError();
  EXPECT_FALSE(contents()->IsConnectedToSerialPort());
  port.FlushForTesting();
  EXPECT_FALSE(port.is_connected());
  service.FlushForTesting();
  EXPECT_FALSE(service.is_connected());
}

TEST_F(SerialTest, OpenAndRevokePermission) {
  NavigateAndCommit(GURL(kTestUrl));

  mojo::Remote<blink::mojom::SerialService> service;
  contents()->GetPrimaryMainFrame()->BindSerialService(
      service.BindNewPipeAndPassReceiver());

  auto token = base::UnguessableToken::Create();
  auto port_info = device::mojom::SerialPortInfo::New();
  port_info->token = token;
  port_manager()->AddPort(port_info->Clone());

  EXPECT_FALSE(contents()->IsConnectedToSerialPort());

  EXPECT_CALL(delegate(), GetPortInfo(_, _)).WillOnce(Return(port_info.get()));
  EXPECT_CALL(delegate(), HasPortPermission(_, _)).WillOnce(Return(true));

  TestFuture<mojo::PendingRemote<device::mojom::SerialPort>> future;
  service->OpenPort(token, device::mojom::SerialConnectionOptions::New(),
                    device::FakeSerialPortClient::Create(),
                    future.GetCallback());
  mojo::Remote<device::mojom::SerialPort> port(future.Take());
  EXPECT_TRUE(port.is_connected());
  EXPECT_TRUE(contents()->IsConnectedToSerialPort());

  EXPECT_CALL(delegate(), GetPortInfo(_, _)).WillOnce(Return(port_info.get()));
  EXPECT_CALL(delegate(), HasPortPermission(_, _)).WillOnce(Return(false));

  ASSERT_TRUE(observer());
  url::Origin origin = url::Origin::Create(GURL(kTestUrl));
  observer()->OnPermissionRevoked(origin);
  EXPECT_FALSE(contents()->IsConnectedToSerialPort());
  port.FlushForTesting();
  EXPECT_FALSE(port.is_connected());
  service.FlushForTesting();
  EXPECT_TRUE(service.is_connected());
}

TEST_F(SerialTest, OpenAndRevokePermissionOnDifferentOrigin) {
  NavigateAndCommit(GURL(kTestUrl));

  mojo::Remote<blink::mojom::SerialService> service;
  contents()->GetPrimaryMainFrame()->BindSerialService(
      service.BindNewPipeAndPassReceiver());

  auto token = base::UnguessableToken::Create();
  auto port_info = device::mojom::SerialPortInfo::New();
  port_info->token = token;
  port_manager()->AddPort(port_info->Clone());

  EXPECT_FALSE(contents()->IsConnectedToSerialPort());

  EXPECT_CALL(delegate(), GetPortInfo(_, _)).WillOnce(Return(port_info.get()));
  EXPECT_CALL(delegate(), HasPortPermission(_, _)).WillOnce(Return(true));

  TestFuture<mojo::PendingRemote<device::mojom::SerialPort>> future;
  service->OpenPort(token, device::mojom::SerialConnectionOptions::New(),
                    device::FakeSerialPortClient::Create(),
                    future.GetCallback());
  mojo::Remote<device::mojom::SerialPort> port(future.Take());
  EXPECT_TRUE(port.is_connected());
  EXPECT_TRUE(contents()->IsConnectedToSerialPort());

  ASSERT_TRUE(observer());
  url::Origin different_origin =
      url::Origin::Create(GURL("http://different-origin.com"));
  observer()->OnPermissionRevoked(different_origin);
  EXPECT_TRUE(contents()->IsConnectedToSerialPort());
  port.FlushForTesting();
  EXPECT_TRUE(port.is_connected());
  service.FlushForTesting();
  EXPECT_TRUE(service.is_connected());
}

TEST_F(SerialTest, OpenTwoPortsAndRevokePermission) {
  NavigateAndCommit(GURL(kTestUrl));

  mojo::Remote<blink::mojom::SerialService> service;
  contents()->GetPrimaryMainFrame()->BindSerialService(
      service.BindNewPipeAndPassReceiver());

  auto token1 = base::UnguessableToken::Create();
  auto port_info1 = device::mojom::SerialPortInfo::New();
  port_info1->token = token1;
  port_manager()->AddPort(port_info1->Clone());

  auto token2 = base::UnguessableToken::Create();
  auto port_info2 = device::mojom::SerialPortInfo::New();
  port_info2->token = token2;
  port_manager()->AddPort(port_info2->Clone());

  EXPECT_FALSE(contents()->IsConnectedToSerialPort());

  EXPECT_CALL(delegate(), GetPortInfo(_, _)).WillOnce(Return(port_info1.get()));
  EXPECT_CALL(delegate(), HasPortPermission(_, _)).WillOnce(Return(true));

  TestFuture<mojo::PendingRemote<device::mojom::SerialPort>> future1;
  service->OpenPort(token1, device::mojom::SerialConnectionOptions::New(),
                    device::FakeSerialPortClient::Create(),
                    future1.GetCallback());
  mojo::Remote<device::mojom::SerialPort> port1(future1.Take());
  EXPECT_TRUE(port1.is_connected());
  EXPECT_TRUE(contents()->IsConnectedToSerialPort());

  EXPECT_CALL(delegate(), GetPortInfo(_, _)).WillOnce(Return(port_info2.get()));
  EXPECT_CALL(delegate(), HasPortPermission(_, _)).WillOnce(Return(true));

  TestFuture<mojo::PendingRemote<device::mojom::SerialPort>> future2;
  service->OpenPort(token2, device::mojom::SerialConnectionOptions::New(),
                    device::FakeSerialPortClient::Create(),
                    future2.GetCallback());
  mojo::Remote<device::mojom::SerialPort> port2(future2.Take());
  EXPECT_TRUE(port2.is_connected());
  EXPECT_TRUE(contents()->IsConnectedToSerialPort());

  EXPECT_CALL(delegate(), GetPortInfo(_, token1))
      .WillOnce(Return(port_info1.get()));
  EXPECT_CALL(delegate(), GetPortInfo(_, token2))
      .WillOnce(Return(port_info2.get()));
  EXPECT_CALL(delegate(), HasPortPermission(_, _))
      .Times(2)
      .WillRepeatedly(testing::Invoke([&](auto rfh, auto port_info) {
        if (port_info.token == port_info1->token) {
          return false;
        } else {
          return true;
        }
      }));

  ASSERT_TRUE(observer());
  url::Origin origin = url::Origin::Create(GURL(kTestUrl));
  observer()->OnPermissionRevoked(origin);
  EXPECT_TRUE(contents()->IsConnectedToSerialPort());
  port1.FlushForTesting();
  EXPECT_FALSE(port1.is_connected());
  port2.FlushForTesting();
  EXPECT_TRUE(port2.is_connected());
  service.FlushForTesting();
  EXPECT_TRUE(service.is_connected());
}

TEST_F(SerialTest, RejectOpaqueOrigin) {
  // Create a fake dispatch context to trigger a bad message in.
  mojo::FakeMessageDispatchContext fake_dispatch_context;
  mojo::test::BadMessageObserver bad_message_observer;

  auto response_headers =
      base::MakeRefCounted<net::HttpResponseHeaders>(std::string());
  response_headers->SetHeader("Content-Security-Policy",
                              "sandbox allow-scripts");
  auto navigation_simulator = NavigationSimulator::CreateRendererInitiated(
      GURL("https://opaque.com"), main_test_rfh());
  navigation_simulator->SetResponseHeaders(response_headers);
  navigation_simulator->Start();
  navigation_simulator->Commit();

  mojo::Remote<blink::mojom::SerialService> service;
  main_test_rfh()->BindSerialService(service.BindNewPipeAndPassReceiver());
  EXPECT_EQ(bad_message_observer.WaitForBadMessage(),
            "Web Serial is not allowed when the top-level document has an "
            "opaque origin.");
}

TEST_F(SerialTest, RejectOpaqueOriginEmbeddedFrame) {
  // Create a fake dispatch context to trigger a bad message in.
  mojo::FakeMessageDispatchContext fake_dispatch_context;
  mojo::test::BadMessageObserver bad_message_observer;

  auto response_headers =
      base::MakeRefCounted<net::HttpResponseHeaders>(std::string());
  response_headers->SetHeader("Content-Security-Policy",
                              "sandbox allow-scripts");
  auto navigation_simulator = NavigationSimulator::CreateRendererInitiated(
      GURL("https://opaque.com"), main_test_rfh());
  navigation_simulator->SetResponseHeaders(response_headers);
  navigation_simulator->Start();
  navigation_simulator->Commit();

  const GURL kEmbeddedUrl("https://opaque.com");
  RenderFrameHost* embedded_rfh =
      RenderFrameHostTester::For(main_test_rfh())
          ->AppendChildWithPolicy(
              "embedded_frame",
              {{blink::mojom::PermissionsPolicyFeature::kSerial,
                /*allowed_origins=*/{},
                /*self_if_matches=*/url::Origin::Create(kEmbeddedUrl),
                /*matches_all_origins=*/false, /*matches_opaque_src=*/true}});
  embedded_rfh = NavigationSimulator::NavigateAndCommitFromDocument(
      kEmbeddedUrl, embedded_rfh);

  mojo::Remote<blink::mojom::SerialService> service;
  static_cast<TestRenderFrameHost*>(embedded_rfh)
      ->BindSerialService(service.BindNewPipeAndPassReceiver());
  EXPECT_EQ(bad_message_observer.WaitForBadMessage(),
            "Web Serial is not allowed when the top-level document has an "
            "opaque origin.");
}

}  // namespace content
