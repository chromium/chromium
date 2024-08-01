// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/midi/usb_midi_output_stream.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/strings/stringprintf.h"
#include "media/midi/usb_midi_device.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace midi {

namespace {

template<typename T, size_t N>
std::vector<T> ToVector(const T((&array)[N])) {
  return std::vector<T>(array, array + N);
}

class MockUsbMidiDevice : public UsbMidiDevice {
 public:
  MockUsbMidiDevice() = default;

  MockUsbMidiDevice(const MockUsbMidiDevice&) = delete;
  MockUsbMidiDevice& operator=(const MockUsbMidiDevice&) = delete;

  ~MockUsbMidiDevice() override = default;

  std::vector<uint8_t> GetDescriptors() override {
    return std::vector<uint8_t>();
  }
  std::string GetManufacturer() override { return std::string(); }
  std::string GetProductName() override { return std::string(); }
  std::string GetDeviceVersion() override { return std::string(); }

  void Send(int endpoint_number, const std::vector<uint8_t>& data) override {
    for (size_t i = 0; i < data.size(); ++i) {
      log_ += base::StringPrintf("0x%02x ", data[i]);
    }
    log_ += base::StringPrintf("(endpoint = %d)\n", endpoint_number);
  }

  const std::string& log() const { return log_; }

  void ClearLog() { log_ = ""; }

 private:
  std::string log_;
};

class UsbMidiOutputStreamTest : public ::testing::Test {
 public:
  UsbMidiOutputStreamTest(const UsbMidiOutputStreamTest&) = delete;
  UsbMidiOutputStreamTest& operator=(const UsbMidiOutputStreamTest&) = delete;

 protected:
  UsbMidiOutputStreamTest() {
    UsbMidiJack jack(&device_, 1, 2, 4);
    stream_ = std::make_unique<UsbMidiOutputStream>(jack);
  }

  MockUsbMidiDevice device_;
  std::unique_ptr<UsbMidiOutputStream> stream_;
};

TEST_F(UsbMidiOutputStreamTest, SendEmpty) {
  stream_->Send(std::vector<uint8_t>());

  EXPECT_EQ("", device_.log());
}

TEST_F(UsbMidiOutputStreamTest, SendNoteOn) {
  uint8_t data[] = {0x90, 0x45, 0x7f};

  stream_->Send(ToVector(data));
  EXPECT_EQ("0x29 0x90 0x45 0x7f (endpoint = 4)\n", device_.log());
}

TEST_F(UsbMidiOutputStreamTest, SendNoteOnPending) {
  stream_->Send(std::vector<uint8_t>(1, 0x90));
  stream_->Send(std::vector<uint8_t>(1, 0x45));
  EXPECT_EQ("", device_.log());

  stream_->Send(std::vector<uint8_t>(1, 0x7f));
  EXPECT_EQ("0x29 0x90 0x45 0x7f (endpoint = 4)\n", device_.log());
  device_.ClearLog();

  stream_->Send(std::vector<uint8_t>(1, 0x90));
  stream_->Send(std::vector<uint8_t>(1, 0x45));
  EXPECT_EQ("", device_.log());
}

TEST_F(UsbMidiOutputStreamTest, SendNoteOnBurst) {
  uint8_t data1[] = {
      0x90,
  };
  uint8_t data2[] = {
      0x45, 0x7f, 0x90, 0x45, 0x71, 0x90, 0x45, 0x72, 0x90,
  };

  stream_->Send(ToVector(data1));
  stream_->Send(ToVector(data2));
  EXPECT_EQ("0x29 0x90 0x45 0x7f "
            "0x29 0x90 0x45 0x71 "
            "0x29 0x90 0x45 0x72 (endpoint = 4)\n", device_.log());
}

TEST_F(UsbMidiOutputStreamTest, SendNoteOff) {
  uint8_t data[] = {
      0x80, 0x33, 0x44,
  };

  stream_->Send(ToVector(data));
  EXPECT_EQ("0x28 0x80 0x33 0x44 (endpoint = 4)\n", device_.log());
}

TEST_F(UsbMidiOutputStreamTest, SendPolyphonicKeyPress) {
  uint8_t data[] = {
      0xa0, 0x33, 0x44,
  };

  stream_->Send(ToVector(data));
  EXPECT_EQ("0x2a 0xa0 0x33 0x44 (endpoint = 4)\n", device_.log());
}

TEST_F(UsbMidiOutputStreamTest, SendControlChange) {
  uint8_t data[] = {
      0xb7, 0x33, 0x44,
  };

  stream_->Send(ToVector(data));
  EXPECT_EQ("0x2b 0xb7 0x33 0x44 (endpoint = 4)\n", device_.log());
}

TEST_F(UsbMidiOutputStreamTest, SendProgramChange) {
  uint8_t data[] = {
      0xc2, 0x33,
  };

  stream_->Send(ToVector(data));
  EXPECT_EQ("0x2c 0xc2 0x33 0x00 (endpoint = 4)\n", device_.log());
}

TEST_F(UsbMidiOutputStreamTest, SendChannelPressure) {
  uint8_t data[] = {
      0xd1, 0x33, 0x44,
  };

  stream_->Send(ToVector(data));
  EXPECT_EQ("0x2d 0xd1 0x33 0x44 (endpoint = 4)\n", device_.log());
}

TEST_F(UsbMidiOutputStreamTest, SendPitchWheelChange) {
  uint8_t data[] = {
      0xe4, 0x33, 0x44,
  };

  stream_->Send(ToVector(data));
  EXPECT_EQ("0x2e 0xe4 0x33 0x44 (endpoint = 4)\n", device_.log());
}

TEST_F(UsbMidiOutputStreamTest, SendTwoByteSysEx) {
  uint8_t data[] = {
      0xf0, 0xf7,
  };

  stream_->Send(ToVector(data));
  EXPECT_EQ("0x26 0xf0 0xf7 0x00 (endpoint = 4)\n", device_.log());
}

TEST_F(UsbMidiOutputStreamTest, SendThreeByteSysEx) {
  uint8_t data[] = {
      0xf0, 0x4f, 0xf7,
  };

  stream_->Send(ToVector(data));
  EXPECT_EQ("0x27 0xf0 0x4f 0xf7 (endpoint = 4)\n", device_.log());
}

TEST_F(UsbMidiOutputStreamTest, SendFourByteSysEx) {
  uint8_t data[] = {
      0xf0, 0x00, 0x01, 0xf7,
  };

  stream_->Send(ToVector(data));
  EXPECT_EQ("0x24 0xf0 0x00 0x01 "
            "0x25 0xf7 0x00 0x00 (endpoint = 4)\n", device_.log());
}

TEST_F(UsbMidiOutputStreamTest, SendFiveByteSysEx) {
  uint8_t data[] = {
      0xf0, 0x00, 0x01, 0x02, 0xf7,
  };

  stream_->Send(ToVector(data));
  EXPECT_EQ("0x24 0xf0 0x00 0x01 "
            "0x26 0x02 0xf7 0x00 (endpoint = 4)\n", device_.log());
}

TEST_F(UsbMidiOutputStreamTest, SendSixByteSysEx) {
  uint8_t data[] = {
      0xf0, 0x00, 0x01, 0x02, 0x03, 0xf7,
  };

  stream_->Send(ToVector(data));
  EXPECT_EQ("0x24 0xf0 0x00 0x01 "
            "0x27 0x02 0x03 0xf7 (endpoint = 4)\n", device_.log());
}

TEST_F(UsbMidiOutputStreamTest, SendPendingSysEx) {
  uint8_t data1[] = {
      0xf0, 0x33,
  };
  uint8_t data2[] = {
      0x44, 0x55, 0x66,
  };
  uint8_t data3[] = {
      0x77, 0x88, 0x99, 0xf7,
  };

  stream_->Send(ToVector(data1));
  EXPECT_EQ("", device_.log());

  stream_->Send(ToVector(data2));
  EXPECT_EQ("0x24 0xf0 0x33 0x44 (endpoint = 4)\n", device_.log());
  device_.ClearLog();

  stream_->Send(ToVector(data3));
  EXPECT_EQ("0x24 0x55 0x66 0x77 0x27 0x88 0x99 0xf7 (endpoint = 4)\n",
            device_.log());
}

TEST_F(UsbMidiOutputStreamTest, SendNoteOnAfterSysEx) {
  uint8_t data[] = {
      0xf0, 0x00, 0x01, 0x02, 0x03, 0xf7, 0x90, 0x44, 0x33,
  };

  stream_->Send(ToVector(data));
  EXPECT_EQ("0x24 0xf0 0x00 0x01 "
            "0x27 0x02 0x03 0xf7 "
            "0x29 0x90 0x44 0x33 (endpoint = 4)\n", device_.log());
}

TEST_F(UsbMidiOutputStreamTest, SendTimeCodeQuarterFrame) {
  uint8_t data[] = {
      0xf1, 0x22,
  };

  stream_->Send(ToVector(data));
  EXPECT_EQ("0x22 0xf1 0x22 0x00 (endpoint = 4)\n", device_.log());
}

TEST_F(UsbMidiOutputStreamTest, SendSongPositionPointer) {
  uint8_t data[] = {
      0xf2, 0x22, 0x33,
  };

  stream_->Send(ToVector(data));
  EXPECT_EQ("0x23 0xf2 0x22 0x33 (endpoint = 4)\n", device_.log());
}

TEST_F(UsbMidiOutputStreamTest, SendSongSelect) {
  uint8_t data[] = {
      0xf3, 0x22,
  };

  stream_->Send(ToVector(data));
  EXPECT_EQ("0x22 0xf3 0x22 0x00 (endpoint = 4)\n", device_.log());
}

TEST_F(UsbMidiOutputStreamTest, TuneRequest) {
  uint8_t data[] = {
      0xf6,
  };

  stream_->Send(ToVector(data));
  EXPECT_EQ("0x25 0xf6 0x00 0x00 (endpoint = 4)\n", device_.log());
}

TEST_F(UsbMidiOutputStreamTest, SendSongPositionPointerPending) {
  uint8_t data1[] = {
      0xf2, 0x22,
  };
  uint8_t data2[] = {
      0x33,
  };

  stream_->Send(ToVector(data1));
  EXPECT_EQ("", device_.log());

  stream_->Send(ToVector(data2));
  EXPECT_EQ("0x23 0xf2 0x22 0x33 (endpoint = 4)\n", device_.log());
}

TEST_F(UsbMidiOutputStreamTest, SendRealTimeMessages) {
  uint8_t data[] = {
      0xf8, 0xfa, 0xfb, 0xfc, 0xfe, 0xff,
  };

  stream_->Send(ToVector(data));
  EXPECT_EQ("0x25 0xf8 0x00 0x00 "
            "0x25 0xfa 0x00 0x00 "
            "0x25 0xfb 0x00 0x00 "
            "0x25 0xfc 0x00 0x00 "
            "0x25 0xfe 0x00 0x00 "
            "0x25 0xff 0x00 0x00 (endpoint = 4)\n", device_.log());
}

TEST_F(UsbMidiOutputStreamTest, SendRealTimeInSysExMessage) {
  uint8_t data[] = {
      0xf0, 0x00, 0x01, 0x02, 0xf8, 0xfa, 0x03, 0xf7,
  };

  stream_->Send(ToVector(data));
  EXPECT_EQ("0x24 0xf0 0x00 0x01 "
            "0x25 0xf8 0x00 0x00 "
            "0x25 0xfa 0x00 0x00 "
            "0x27 0x02 0x03 0xf7 (endpoint = 4)\n", device_.log());
}

}  // namespace

}  // namespace midi
