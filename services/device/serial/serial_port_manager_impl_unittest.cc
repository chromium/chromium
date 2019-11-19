// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/serial/serial_port_manager_impl.h"

#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/macros.h"
#include "base/task/post_task.h"
#include "base/test/bind_test_util.h"
#include "base/test/task_environment.h"
#include "base/threading/thread.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "services/device/device_service_test_base.h"
#include "services/device/public/mojom/constants.mojom.h"
#include "services/device/public/mojom/serial.mojom.h"
#include "services/device/serial/fake_serial_device_enumerator.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {

namespace {

const base::FilePath kFakeDevicePath1(FILE_PATH_LITERAL("/dev/fakeserialmojo"));
const base::FilePath kFakeDevicePath2(FILE_PATH_LITERAL("\\\\COM800\\"));

void CreateAndBindOnBlockableRunner(
    mojo::PendingReceiver<mojom::SerialPortManager> receiver,
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner) {
  auto manager = std::make_unique<SerialPortManagerImpl>(
      std::move(io_task_runner), std::move(ui_task_runner));
  auto fake_enumerator = std::make_unique<FakeSerialEnumerator>();
  fake_enumerator->AddDevicePath(kFakeDevicePath1);
  fake_enumerator->AddDevicePath(kFakeDevicePath2);
  manager->SetSerialEnumeratorForTesting(std::move(fake_enumerator));
  mojo::MakeSelfOwnedReceiver(std::move(manager), std::move(receiver));
}

}  // namespace

class SerialPortManagerImplTest : public DeviceServiceTestBase {
 public:
  SerialPortManagerImplTest() = default;
  ~SerialPortManagerImplTest() override = default;

 protected:
  void SetUp() override { DeviceServiceTestBase::SetUp(); }

  void BindSerialPortManager(
      mojo::PendingReceiver<mojom::SerialPortManager> receiver) {
    file_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&CreateAndBindOnBlockableRunner, std::move(receiver),
                       io_task_runner_, base::ThreadTaskRunnerHandle::Get()));
  }

  DISALLOW_COPY_AND_ASSIGN(SerialPortManagerImplTest);
};

// This is to simply test that we can enumerate devices on the platform without
// hanging or crashing.
TEST_F(SerialPortManagerImplTest, SimpleConnectTest) {
  mojo::Remote<mojom::SerialPortManager> port_manager;
  connector()->Connect(mojom::kServiceName,
                       port_manager.BindNewPipeAndPassReceiver());

  base::RunLoop loop;
  port_manager->GetDevices(base::BindLambdaForTesting(
      [&](std::vector<mojom::SerialPortInfoPtr> results) {
        for (auto& device : results) {
          mojo::Remote<mojom::SerialPort> serial_port;
          port_manager->GetPort(device->token,
                                serial_port.BindNewPipeAndPassReceiver(),
                                /*watcher=*/mojo::NullRemote());
          // Send a message on the pipe and wait for the response to make sure
          // that the interface request was bound successfully.
          serial_port.FlushForTesting();
          EXPECT_TRUE(serial_port.is_connected());
        }
        loop.Quit();
      }));
  loop.Run();
}

TEST_F(SerialPortManagerImplTest, GetDevices) {
  mojo::Remote<mojom::SerialPortManager> port_manager;
  BindSerialPortManager(port_manager.BindNewPipeAndPassReceiver());
  const std::set<base::FilePath> expected_paths = {kFakeDevicePath1,
                                                   kFakeDevicePath2};

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

TEST_F(SerialPortManagerImplTest, GetPort) {
  mojo::Remote<mojom::SerialPortManager> port_manager;
  BindSerialPortManager(port_manager.BindNewPipeAndPassReceiver());

  base::RunLoop loop;
  port_manager->GetDevices(base::BindLambdaForTesting(
      [&](std::vector<mojom::SerialPortInfoPtr> results) {
        EXPECT_GT(results.size(), 0u);

        mojo::Remote<mojom::SerialPort> serial_port;
        port_manager->GetPort(results[0]->token,
                              serial_port.BindNewPipeAndPassReceiver(),
                              /*watcher=*/mojo::NullRemote());
        // Send a message on the pipe and wait for the response to make sure
        // that the interface request was bound successfully.
        serial_port.FlushForTesting();
        EXPECT_TRUE(serial_port.is_connected());
        loop.Quit();
      }));
  loop.Run();
}

}  // namespace device
