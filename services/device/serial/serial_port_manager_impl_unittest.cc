// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/serial/serial_port_manager_impl.h"

#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/threading/thread.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/public/cpp/bluetooth_uuid.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "device/bluetooth/test/mock_bluetooth_device.h"
#include "device/bluetooth/test/mock_bluetooth_socket.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "services/device/device_service_test_base.h"
#include "services/device/public/cpp/bluetooth/bluetooth_utils.h"
#include "services/device/public/cpp/serial/serial_switches.h"
#include "services/device/public/cpp/test/fake_serial_port_client.h"
#include "services/device/public/mojom/serial.mojom.h"
#include "services/device/serial/bluetooth_serial_device_enumerator.h"
#include "services/device/serial/fake_serial_device_enumerator.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::base::test::RunOnceCallback;
using ::testing::_;
using ::testing::Invoke;
using ::testing::Return;

namespace device {

namespace {

const base::FilePath kFakeDevicePath1(FILE_PATH_LITERAL("/dev/fakeserialmojo"));
const base::FilePath kFakeDevicePath2(FILE_PATH_LITERAL("\\\\COM800\\"));
constexpr char kDeviceAddress[] = "00:00:00:00:00:00";

class MockSerialPortManagerClient : public mojom::SerialPortManagerClient {
 public:
  MockSerialPortManagerClient() = default;
  MockSerialPortManagerClient(const MockSerialPortManagerClient&) = delete;
  MockSerialPortManagerClient& operator=(const MockSerialPortManagerClient&) =
      delete;
  ~MockSerialPortManagerClient() override = default;

  mojo::PendingRemote<mojom::SerialPortManagerClient>
  BindNewPipeAndPassRemote() {
    return receiver_.BindNewPipeAndPassRemote();
  }

  // mojom::SerialPortManagerClient
  MOCK_METHOD1(OnPortAdded, void(mojom::SerialPortInfoPtr));
  MOCK_METHOD1(OnPortRemoved, void(mojom::SerialPortInfoPtr));

 private:
  mojo::Receiver<mojom::SerialPortManagerClient> receiver_{this};
};

class TestingBluetoothAdapter : public device::MockBluetoothAdapter {
 public:
  bool IsInitialized() const override { return false; }
  MOCK_METHOD1(Initialize, void(base::OnceClosure callback));

 private:
  ~TestingBluetoothAdapter() override = default;
};

}  // namespace

class SerialPortManagerImplTest : public DeviceServiceTestBase {
 public:
  SerialPortManagerImplTest() {
    auto enumerator = std::make_unique<FakeSerialEnumerator>();
    enumerator_ = enumerator.get();
    enumerator_->AddDevicePath(kFakeDevicePath1);
    enumerator_->AddDevicePath(kFakeDevicePath2);

    manager_ = std::make_unique<SerialPortManagerImpl>(
        io_task_runner_, base::SingleThreadTaskRunner::GetCurrentDefault());
    manager_->SetSerialEnumeratorForTesting(std::move(enumerator));
  }

  SerialPortManagerImplTest(const SerialPortManagerImplTest&) = delete;
  SerialPortManagerImplTest& operator=(const SerialPortManagerImplTest&) =
      delete;

  ~SerialPortManagerImplTest() override = default;

  void TearDown() override {
    // Resetting `manager_` will delete the BluetoothSerialDeviceEnumerator
    // which will enqueue the deletion of a `SequenceBound` helper.
    manager_.reset();
    // Wait for any `SequenceBound` objects have been destroyed
    // to avoid tripping leak detection.
    base::RunLoop run_loop;
    adapter_task_runner()->PostTask(FROM_HERE, run_loop.QuitClosure());
    run_loop.Run();
  }

  // Since not all functions need to use a MockBluetoothAdapter, this function
  // is called at the beginning of test cases that do require a
  // MockBluetoothAdapter.
  void SetupBluetoothEnumerator() {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kEnableBluetoothSerialPortProfileInSerialApi);

    ON_CALL(*adapter_, GetDevices())
        .WillByDefault(
            Invoke(adapter_.get(), &MockBluetoothAdapter::GetConstMockDevices));
    device::BluetoothAdapterFactory::SetAdapterForTesting(adapter_);

    auto mock_device = std::make_unique<MockBluetoothDevice>(
        adapter_.get(), 0, "Test Device", kDeviceAddress, false, false);
    mock_device->AddUUID(GetSerialPortProfileUUID());
    adapter_->AddMockDevice(std::move(mock_device));

    auto bluetooth_enumerator =
        std::make_unique<BluetoothSerialDeviceEnumerator>(
            adapter_task_runner());
    bluetooth_enumerator_ = bluetooth_enumerator.get();

    manager_->SetBluetoothSerialEnumeratorForTesting(
        std::move(bluetooth_enumerator));

    base::RunLoop run_loop;
    bluetooth_enumerator_->OnGotAdapterForTesting(run_loop.QuitClosure());
    run_loop.Run();
  }

  void SetupBluetoothEnumeratorWithExpectations() {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kEnableBluetoothSerialPortProfileInSerialApi);

    ON_CALL(*adapter_, GetDevices())
        .WillByDefault(
            Invoke(adapter_.get(), &MockBluetoothAdapter::GetConstMockDevices));
    device::BluetoothAdapterFactory::SetAdapterForTesting(adapter_);

    auto mock_device = std::make_unique<MockBluetoothDevice>(
        adapter_.get(), 0, "Test Device", kDeviceAddress, false, false);
    mock_device->AddUUID(GetSerialPortProfileUUID());
    MockBluetoothDevice* mock_device_ptr = mock_device.get();
    adapter_->AddMockDevice(std::move(mock_device));

    EXPECT_CALL(*adapter_, GetDevice(kDeviceAddress))
        .WillOnce(Return(mock_device_ptr));
    EXPECT_CALL(*mock_device_ptr,
                ConnectToService(GetSerialPortProfileUUID(), _, _))
        .WillOnce(RunOnceCallback<1>(mock_socket_));

    auto bluetooth_enumerator =
        std::make_unique<BluetoothSerialDeviceEnumerator>(
            adapter_task_runner());
    bluetooth_enumerator_ = bluetooth_enumerator.get();

    manager_->SetBluetoothSerialEnumeratorForTesting(
        std::move(bluetooth_enumerator));

    base::RunLoop run_loop;
    bluetooth_enumerator_->OnGotAdapterForTesting(run_loop.QuitClosure());
    run_loop.Run();
  }

 protected:
  scoped_refptr<base::SingleThreadTaskRunner> adapter_task_runner() {
    return base::SingleThreadTaskRunner::GetCurrentDefault();
  }

  raw_ptr<FakeSerialEnumerator> enumerator_;
  raw_ptr<BluetoothSerialDeviceEnumerator> bluetooth_enumerator_;
  scoped_refptr<MockBluetoothAdapter> adapter_ =
      base::MakeRefCounted<MockBluetoothAdapter>();
  scoped_refptr<MockBluetoothSocket> mock_socket_ =
      base::MakeRefCounted<MockBluetoothSocket>();

  void Bind(mojo::PendingReceiver<mojom::SerialPortManager> receiver) {
    manager_->Bind(std::move(receiver));
  }

 private:
  std::unique_ptr<SerialPortManagerImpl> manager_;
};

// This is to simply test that we can enumerate devices on the platform without
// hanging or crashing.
TEST_F(SerialPortManagerImplTest, SimpleEnumerationTest) {
  // DeviceService has its own instance of SerialPortManagerImpl that is used to
  // bind the receiver over the one created for this test.
  mojo::Remote<mojom::SerialPortManager> port_manager;
  device_service()->BindSerialPortManager(
      port_manager.BindNewPipeAndPassReceiver());

  base::RunLoop loop;
  port_manager->GetDevices(base::BindLambdaForTesting(
      [&](std::vector<mojom::SerialPortInfoPtr> results) { loop.Quit(); }));
  loop.Run();
}

TEST_F(SerialPortManagerImplTest, GetDevices) {
  SetupBluetoothEnumerator();
  mojo::Remote<mojom::SerialPortManager> port_manager;
  Bind(port_manager.BindNewPipeAndPassReceiver());
  const std::set<base::FilePath> expected_paths = {
      kFakeDevicePath1, kFakeDevicePath2,
      base::FilePath::FromASCII(kDeviceAddress)};

  base::RunLoop loop;
  port_manager->GetDevices(base::BindLambdaForTesting(
      [&](std::vector<mojom::SerialPortInfoPtr> results) {
        EXPECT_EQ(expected_paths.size(), results.size());
        std::set<base::FilePath> actual_paths;
        for (size_t i = 0; i < results.size(); ++i)
          actual_paths.insert(results[i]->path);
        EXPECT_EQ(expected_paths, actual_paths);
        loop.Quit();
      }));
  loop.Run();
}

TEST_F(SerialPortManagerImplTest, OpenUnknownPort) {
  mojo::Remote<mojom::SerialPortManager> port_manager;
  Bind(port_manager.BindNewPipeAndPassReceiver());

  base::RunLoop loop;
  port_manager->OpenPort(
      base::UnguessableToken::Create(),
      /*use_alternate_path=*/false, mojom::SerialConnectionOptions::New(),
      FakeSerialPortClient::Create(),
      /*watcher=*/mojo::NullRemote(),
      base::BindLambdaForTesting(
          [&](mojo::PendingRemote<mojom::SerialPort> pending_remote) {
            EXPECT_FALSE(pending_remote.is_valid());
            loop.Quit();
          }));
  loop.Run();
}

TEST_F(SerialPortManagerImplTest, PortRemovedAndAdded) {
  SetupBluetoothEnumerator();
  mojo::Remote<mojom::SerialPortManager> port_manager;
  Bind(port_manager.BindNewPipeAndPassReceiver());

  MockSerialPortManagerClient client;
  port_manager->SetClient(client.BindNewPipeAndPassRemote());

  base::UnguessableToken port1_token;
  {
    base::RunLoop run_loop;
    port_manager->GetDevices(base::BindLambdaForTesting(
        [&](std::vector<mojom::SerialPortInfoPtr> results) {
          for (const auto& port : results) {
            if (port->path == kFakeDevicePath1) {
              port1_token = port->token;
              break;
            }
          }
          run_loop.Quit();
        }));
    run_loop.Run();
  }
  ASSERT_FALSE(port1_token.is_empty());

  enumerator_->RemoveDevicePath(kFakeDevicePath1);
  {
    base::RunLoop run_loop;
    EXPECT_CALL(client, OnPortRemoved(_))
        .WillOnce(Invoke([&](mojom::SerialPortInfoPtr port) {
          EXPECT_EQ(port1_token, port->token);
          EXPECT_EQ(kFakeDevicePath1, port->path);
          run_loop.Quit();
        }));
    run_loop.Run();
  }

  enumerator_->AddDevicePath(kFakeDevicePath1);
  {
    base::RunLoop run_loop;
    EXPECT_CALL(client, OnPortAdded(_))
        .WillOnce(Invoke([&](mojom::SerialPortInfoPtr port) {
          EXPECT_NE(port1_token, port->token);
          EXPECT_EQ(kFakeDevicePath1, port->path);
          run_loop.Quit();
        }));
    run_loop.Run();
  }
}

TEST_F(SerialPortManagerImplTest, OpenBluetoothDevicePort) {
  SetupBluetoothEnumeratorWithExpectations();
  mojo::Remote<mojom::SerialPortManager> port_manager;
  Bind(port_manager.BindNewPipeAndPassReceiver());
  mojo::PendingRemote<mojom::SerialPortConnectionWatcher> watcher_remote;
  mojo::SelfOwnedReceiverRef<mojom::SerialPortConnectionWatcher> watcher =
      mojo::MakeSelfOwnedReceiver(
          std::make_unique<mojom::SerialPortConnectionWatcher>(),
          watcher_remote.InitWithNewPipeAndPassReceiver());

  // Since we only want to use devices enumerated by the Bluetooth
  // enumerator, we can remove the devices that are not.
  enumerator_->RemoveDevicePath(kFakeDevicePath1);
  enumerator_->RemoveDevicePath(kFakeDevicePath2);

  const std::set<base::FilePath> expected_paths = {
      base::FilePath::FromASCII(kDeviceAddress)};

  mojo::Remote<mojom::SerialPort> serial_port;
  base::RunLoop loop;
  port_manager->GetDevices(base::BindLambdaForTesting(
      [&](std::vector<mojom::SerialPortInfoPtr> results) {
        EXPECT_EQ(expected_paths.size(), results.size());
        std::set<base::FilePath> actual_paths;
        for (size_t i = 0; i < results.size(); ++i)
          actual_paths.insert(results[i]->path);
        EXPECT_EQ(expected_paths, actual_paths);

        port_manager->OpenPort(
            results[0]->token,
            /*use_alternate_path=*/false, mojom::SerialConnectionOptions::New(),
            FakeSerialPortClient::Create(), std::move(watcher_remote),
            base::BindLambdaForTesting(
                [&](mojo::PendingRemote<mojom::SerialPort> pending_remote) {
                  serial_port.Bind(std::move(pending_remote));
                  EXPECT_TRUE(serial_port.is_connected());
                  loop.Quit();
                }));
      }));

  loop.Run();

  base::RunLoop disconnect_loop;
  watcher->set_connection_error_handler(disconnect_loop.QuitClosure());

  serial_port.reset();
  disconnect_loop.Run();
}

TEST_F(SerialPortManagerImplTest, BluetoothPortRemovedAndAdded) {
  SetupBluetoothEnumerator();
  mojo::Remote<mojom::SerialPortManager> port_manager;
  Bind(port_manager.BindNewPipeAndPassReceiver());

  MockSerialPortManagerClient client;
  port_manager->SetClient(client.BindNewPipeAndPassRemote());

  base::UnguessableToken port1_token;
  {
    base::RunLoop run_loop;
    port_manager->GetDevices(base::BindLambdaForTesting(
        [&](std::vector<mojom::SerialPortInfoPtr> results) {
          for (const auto& port : results) {
            if (port->path == base::FilePath::FromASCII(kDeviceAddress)) {
              port1_token = port->token;
              break;
            }
          }
          run_loop.Quit();
        }));
    run_loop.Run();
  }
  ASSERT_FALSE(port1_token.is_empty());

  bluetooth_enumerator_->DeviceRemoved(kDeviceAddress);
  {
    base::RunLoop run_loop;
    EXPECT_CALL(client, OnPortRemoved(_))
        .WillOnce(Invoke([&](mojom::SerialPortInfoPtr port) {
          EXPECT_EQ(port1_token, port->token);
          EXPECT_EQ(port->path, base::FilePath::FromASCII(kDeviceAddress));
          EXPECT_EQ(mojom::DeviceType::SPP_DEVICE, port->type);
          run_loop.Quit();
        }));
    run_loop.Run();
  }

  auto mock_device = std::make_unique<MockBluetoothDevice>(
      adapter_.get(), 0, "Test Device", kDeviceAddress, false, false);
  static const BluetoothUUID kSerialPortProfileUUID("1101");
  mock_device->AddUUID(kSerialPortProfileUUID);
  MockBluetoothDevice* mock_device_ptr = mock_device.get();
  adapter_->AddMockDevice(std::move(mock_device));

  bluetooth_enumerator_->DeviceAddedForTesting(adapter_.get(), mock_device_ptr);
  {
    base::RunLoop run_loop;
    EXPECT_CALL(client, OnPortAdded(_))
        .WillOnce(Invoke([&](mojom::SerialPortInfoPtr port) {
          EXPECT_NE(port1_token, port->token);
          EXPECT_EQ(port->path, base::FilePath::FromASCII(kDeviceAddress));
          EXPECT_EQ(mojom::DeviceType::SPP_DEVICE, port->type);
          run_loop.Quit();
        }));
    run_loop.Run();
  }
}

TEST_F(SerialPortManagerImplTest,
       BluetoothSerialDeviceEnumerator_DeleteBeforeAdapterInit) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableBluetoothSerialPortProfileInSerialApi);

  auto adapter = base::MakeRefCounted<TestingBluetoothAdapter>();
  BluetoothAdapterFactory::SetAdapterForTesting(adapter);

  // BluetoothAdapterFactory does not initialize the test adapter. Instead it
  // holds on to the GetAdapter callback until adapter initialization is
  // complete.
  EXPECT_CALL(*adapter, Initialize).Times(0);

  // Create the enumerator, which calls GetAdapter(), which is blocked waiting
  // on adapter initialization.
  auto enumerator =
      std::make_unique<BluetoothSerialDeviceEnumerator>(adapter_task_runner());

  // Delete the enumerator before adapter initialization completes.
  // Explicitly delete its helper. This workaround is needed because this test
  // does not mock out the BluetoothAdapterFactory singleton, which (on Linux)
  // calls bluez::BluezDBusManager::Get() - failing a CHECK.
  enumerator->SynchronouslyResetHelperForTesting();
  enumerator.reset();

  // Directly call the adapter initialization callback, which calls any saved
  // GetAdapter() callbacks - i.e. the one made by the
  // BluetoothSerialDeviceEnumerator constructor.
  BluetoothAdapterFactory::Get()->AdapterInitialized();

  // We didn't crash? yay \o/ test passed.
}

}  // namespace device
