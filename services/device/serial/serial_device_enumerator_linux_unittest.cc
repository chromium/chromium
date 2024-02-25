// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/serial/serial_device_enumerator_linux.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/task_environment.h"
#include "device/udev_linux/fake_udev_loader.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {

constexpr char kSerialDriverInfo[] =
    R"(/dev/tty             /dev/tty        5       0 system:/dev/tty
/dev/console         /dev/console    5       1 system:console
/dev/ptmx            /dev/ptmx       5       2 system
/dev/vc/0            /dev/vc/0       4       0 system:vtmaster
rfcomm               /dev/rfcomm   216 0-255 serial
acm                  /dev/ttyACM   166 0-255 serial
ttyAMA               /dev/ttyAMA   204 64-77 serial
ttyprintk            /dev/ttyprintk   5       3 console
max310x              /dev/ttyMAX   204 209-224 serial
serial               /dev/ttyS       4      64 serial
pty_slave            /dev/pts      136 0-1048575 pty:slave
pty_master           /dev/ptm      128 0-1048575 pty:master
pty_slave            /dev/ttyp       3 0-255 pty:slave
pty_master           /dev/pty        2 0-255 pty:master
unknown              /dev/tty        4 1-63 console)";

class SerialDeviceEnumeratorLinuxTest : public testing::Test {
 public:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    drivers_file_ = temp_dir_.GetPath().Append("drivers");
    ASSERT_TRUE(base::WriteFile(drivers_file_, kSerialDriverInfo));
  }

  std::unique_ptr<SerialDeviceEnumeratorLinux> CreateEnumerator() {
    return std::make_unique<SerialDeviceEnumeratorLinux>(drivers_file_);
  }

 private:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::IO};
  base::ScopedTempDir temp_dir_;
  base::FilePath drivers_file_;
};

TEST_F(SerialDeviceEnumeratorLinuxTest, EnumerateUsb) {
  testing::FakeUdevLoader fake_udev;
  fake_udev.AddFakeDevice(/*name=*/"ttyACM0",
                          /*syspath=*/"/sys/class/tty/ttyACM0",
                          /*subsystem=*/"tty", /*devnode=*/std::nullopt,
                          /*devtype=*/std::nullopt, /*sysattrs=*/{},
                          /*properties=*/
                          {
                              {"DEVNAME", "/dev/ttyACM0"},
                              {"MAJOR", "166"},
                              {"MINOR", "0"},
                              {"ID_VENDOR_ID", "2341"},
                              {"ID_MODEL_ID", "0043"},
                              {"ID_MODEL_ENC", "Arduino\\x20Uno"},
                              {"ID_SERIAL_SHORT", "000001"},
                          });

  std::unique_ptr<SerialDeviceEnumeratorLinux> enumerator = CreateEnumerator();
  std::vector<mojom::SerialPortInfoPtr> devices = enumerator->GetDevices();
  ASSERT_EQ(devices.size(), 1u);
  EXPECT_EQ(devices[0]->serial_number, "000001");
  EXPECT_EQ(devices[0]->path, base::FilePath("/dev/ttyACM0"));
  EXPECT_TRUE(devices[0]->has_vendor_id);
  EXPECT_EQ(devices[0]->vendor_id, 0x2341);
  EXPECT_TRUE(devices[0]->has_product_id);
  EXPECT_EQ(devices[0]->product_id, 0x0043);
  EXPECT_EQ(devices[0]->display_name, "Arduino Uno");
}

TEST_F(SerialDeviceEnumeratorLinuxTest, EnumerateRfcomm) {
  testing::FakeUdevLoader fake_udev;
  fake_udev.AddFakeDevice(/*name=*/"rfcomm0",
                          /*syspath=*/"/sys/class/tty/rfcomm0",
                          /*subsystem=*/"tty", /*devnode=*/std::nullopt,
                          /*devtype=*/std::nullopt, /*sysattrs=*/{},
                          /*properties=*/
                          {
                              {"DEVNAME", "/dev/rfcomm0"},
                              {"MAJOR", "216"},
                              {"MINOR", "0"},
                          });

  std::unique_ptr<SerialDeviceEnumeratorLinux> enumerator = CreateEnumerator();
  std::vector<mojom::SerialPortInfoPtr> devices = enumerator->GetDevices();
  ASSERT_EQ(devices.size(), 1u);
  EXPECT_FALSE(devices[0]->serial_number.has_value());
  EXPECT_EQ(devices[0]->path, base::FilePath("/dev/rfcomm0"));
  EXPECT_FALSE(devices[0]->has_vendor_id);
  EXPECT_FALSE(devices[0]->has_product_id);
  EXPECT_FALSE(devices[0]->display_name.has_value());
}

}  // namespace device
