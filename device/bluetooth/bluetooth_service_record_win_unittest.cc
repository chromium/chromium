// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include "base/strings/string_number_conversions.h"
#include "device/bluetooth/bluetooth_service_record_win.h"
#include "device/bluetooth/public/cpp/bluetooth_uuid.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kTestNoRfcommSdpBytes[] =
    "35510900000a00010001090001350319110a09000435103506190100090019350619001909"
    "010209000535031910020900093508350619110d090102090100250c417564696f20536f75"
    "726365090311090001";
const device::BluetoothUUID kTestNoRfcommSdpUuid("110a");

const char kTestRfcommSdpBytes[] =
    "354b0900000a000100030900013506191112191203090004350c3503190100350519000308"
    "0b090005350319100209000935083506191108090100090100250d566f6963652047617465"
    "776179";
const device::BluetoothUUID kTestRfcommSdpUuid("1112");
const int kTestRfcommChannel = 11;

}  // namespace

namespace device {

class BluetoothServiceRecordWinTest : public testing::Test {
 protected:
  void ConvertSdpBytes(const char* sdp_hex_char,
                       std::vector<uint8_t>* sdp_bytes_vector) {
    base::HexStringToBytes(sdp_hex_char, sdp_bytes_vector);
  }
};

TEST_F(BluetoothServiceRecordWinTest, NoRfcommSdp) {
  std::vector<uint8_t> sdp_bytes_array;
  ConvertSdpBytes(kTestNoRfcommSdpBytes, &sdp_bytes_array);
  BluetoothServiceRecordWin service_record(
      "01:02:03:0A:10:A0", "NoRfcommSdp", sdp_bytes_array, BluetoothUUID());
  EXPECT_EQ(kTestNoRfcommSdpUuid, service_record.uuid());
  EXPECT_FALSE(service_record.SupportsRfcomm());
}


TEST_F(BluetoothServiceRecordWinTest, RfcommSdp) {
  std::vector<uint8_t> sdp_bytes_array;
  ConvertSdpBytes(kTestRfcommSdpBytes, &sdp_bytes_array);
  BluetoothServiceRecordWin service_record(
      "01:02:03:0A:10:A0", "RfcommSdp", sdp_bytes_array, BluetoothUUID());
  EXPECT_EQ(kTestRfcommSdpUuid, service_record.uuid());
  EXPECT_TRUE(service_record.SupportsRfcomm());
  EXPECT_EQ(kTestRfcommChannel, service_record.rfcomm_channel());
}

TEST_F(BluetoothServiceRecordWinTest, BthAddr) {
  std::vector<uint8_t> sdp_bytes_array;
  ConvertSdpBytes(kTestRfcommSdpBytes, &sdp_bytes_array);
  BluetoothServiceRecordWin service_record(
      "01:02:03:0A:10:A0", "Sdp", sdp_bytes_array, BluetoothUUID());
  EXPECT_EQ(1108152553632ull, service_record.device_bth_addr());
}

}  // namespace device
