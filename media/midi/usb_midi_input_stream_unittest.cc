// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/midi/usb_midi_input_stream.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "media/midi/usb_midi_device.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::TimeTicks;

namespace midi {

namespace {

class TestUsbMidiDevice : public UsbMidiDevice {
 public:
  TestUsbMidiDevice() = default;

  TestUsbMidiDevice(const TestUsbMidiDevice&) = delete;
  TestUsbMidiDevice& operator=(const TestUsbMidiDevice&) = delete;

  ~TestUsbMidiDevice() override = default;
  std::vector<uint8_t> GetDescriptors() override {
    return std::vector<uint8_t>();
  }
  std::string GetManufacturer() override { return std::string(); }
  std::string GetProductName() override { return std::string(); }
  std::string GetDeviceVersion() override { return std::string(); }
  void Send(int endpoint_number, const std::vector<uint8_t>& data) override {}
};

class MockDelegate : public UsbMidiInputStream::Delegate {
 public:
  MockDelegate() = default;

  MockDelegate(const MockDelegate&) = delete;
  MockDelegate& operator=(const MockDelegate&) = delete;

  ~MockDelegate() override = default;
  void OnReceivedData(size_t jack_index,
                      const uint8_t* data,
                      size_t size,
                      base::TimeTicks time) override {
    for (size_t i = 0; i < size; ++i)
      received_data_ += base::StringPrintf("0x%02x ", data[i]);
    received_data_ += "\n";
  }

  const std::string& received_data() const { return received_data_; }

 private:
  std::string received_data_;
};

class UsbMidiInputStreamTest : public ::testing::Test {
 public:
  UsbMidiInputStreamTest(const UsbMidiInputStreamTest&) = delete;
  UsbMidiInputStreamTest& operator=(const UsbMidiInputStreamTest&) = delete;

 protected:
  UsbMidiInputStreamTest() {
    stream_ = std::make_unique<UsbMidiInputStream>(&delegate_);

    stream_->Add(UsbMidiJack(&device1_,
                             84,  // jack_id
                             4,  // cable_number
                             135));  // endpoint_address
    stream_->Add(UsbMidiJack(&device2_,
                             85,
                             5,
                             137));
    stream_->Add(UsbMidiJack(&device2_,
                             84,
                             4,
                             135));
    stream_->Add(UsbMidiJack(&device1_,
                             85,
                             5,
                             135));
  }

  TestUsbMidiDevice device1_;
  TestUsbMidiDevice device2_;
  MockDelegate delegate_;
  std::unique_ptr<UsbMidiInputStream> stream_;
};

TEST_F(UsbMidiInputStreamTest, UnknownMessage) {
  uint8_t data[] = {
      0x40, 0xff, 0xff, 0xff, 0x41, 0xff, 0xff, 0xff,
  };

  stream_->OnReceivedData(&device1_, 7, data, std::size(data), TimeTicks());
  EXPECT_EQ("", delegate_.received_data());
}

TEST_F(UsbMidiInputStreamTest, SystemCommonMessage) {
  uint8_t data[] = {
      0x45, 0xf8, 0x00, 0x00, 0x42, 0xf3, 0x22, 0x00, 0x43, 0xf2, 0x33, 0x44,
  };

  stream_->OnReceivedData(&device1_, 7, data, std::size(data), TimeTicks());
  EXPECT_EQ("0xf8 \n"
            "0xf3 0x22 \n"
            "0xf2 0x33 0x44 \n", delegate_.received_data());
}

TEST_F(UsbMidiInputStreamTest, SystemExclusiveMessage) {
  uint8_t data[] = {
      0x44, 0xf0, 0x11, 0x22, 0x45, 0xf7, 0x00, 0x00,
      0x46, 0xf0, 0xf7, 0x00, 0x47, 0xf0, 0x33, 0xf7,
  };

  stream_->OnReceivedData(&device1_, 7, data, std::size(data), TimeTicks());
  EXPECT_EQ("0xf0 0x11 0x22 \n"
            "0xf7 \n"
            "0xf0 0xf7 \n"
            "0xf0 0x33 0xf7 \n", delegate_.received_data());
}

TEST_F(UsbMidiInputStreamTest, ChannelMessage) {
  uint8_t data[] = {
      0x48, 0x80, 0x11, 0x22, 0x49, 0x90, 0x33, 0x44, 0x4a, 0xa0,
      0x55, 0x66, 0x4b, 0xb0, 0x77, 0x88, 0x4c, 0xc0, 0x99, 0x00,
      0x4d, 0xd0, 0xaa, 0x00, 0x4e, 0xe0, 0xbb, 0xcc,
  };

  stream_->OnReceivedData(&device1_, 7, data, std::size(data), TimeTicks());
  EXPECT_EQ("0x80 0x11 0x22 \n"
            "0x90 0x33 0x44 \n"
            "0xa0 0x55 0x66 \n"
            "0xb0 0x77 0x88 \n"
            "0xc0 0x99 \n"
            "0xd0 0xaa \n"
            "0xe0 0xbb 0xcc \n", delegate_.received_data());
}

TEST_F(UsbMidiInputStreamTest, SingleByteMessage) {
  uint8_t data[] = {
      0x4f, 0xf8, 0x00, 0x00,
  };

  stream_->OnReceivedData(&device1_, 7, data, std::size(data), TimeTicks());
  EXPECT_EQ("0xf8 \n", delegate_.received_data());
}

TEST_F(UsbMidiInputStreamTest, DispatchForMultipleCables) {
  uint8_t data[] = {
      0x4f, 0xf8, 0x00, 0x00, 0x5f, 0xfa, 0x00, 0x00, 0x6f, 0xfb, 0x00, 0x00,
  };

  stream_->OnReceivedData(&device1_, 7, data, std::size(data), TimeTicks());
  EXPECT_EQ("0xf8 \n0xfa \n", delegate_.received_data());
}

TEST_F(UsbMidiInputStreamTest, DispatchForDevice2) {
  uint8_t data[] = {0x4f, 0xf8, 0x00, 0x00};

  stream_->OnReceivedData(&device2_, 7, data, std::size(data), TimeTicks());
  EXPECT_EQ("0xf8 \n", delegate_.received_data());
}

}  // namespace

}  // namespace midi
