// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/webcam_private/visca_webcam.h"

#include <vector>

#include "base/bind.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "content/public/test/browser_task_environment.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace {

class TestSerialConnection : public SerialConnection {
 public:
  explicit TestSerialConnection(
      mojo::PendingRemote<device::mojom::SerialPort> port)
      : SerialConnection("dummy_id", std::move(port)) {}
  ~TestSerialConnection() override {}

  void SetReceiveBuffer(const std::vector<uint8_t>& receive_buffer) {
    receive_buffer_ = receive_buffer;
  }

  void CheckSendBufferAndClear(const std::vector<uint8_t>& expectations) {
    EXPECT_EQ(send_buffer_, expectations);
    send_buffer_.clear();
  }

 private:
  // SerialConnection:
  void Open(const api::serial::ConnectionOptions& options,
            OpenCompleteCallback callback) override {
    NOTREACHED();
  }

  void StartPolling(const ReceiveEventCallback& callback) override {
    SetPaused(false);
    callback.Run(std::move(receive_buffer_), api::serial::RECEIVE_ERROR_NONE);
    receive_buffer_.clear();
  }

  bool Send(const std::vector<uint8_t>& data,
            SendCompleteCallback callback) override {
    send_buffer_.insert(send_buffer_.end(), data.begin(), data.end());
    std::move(callback).Run(data.size(), api::serial::SEND_ERROR_NONE);
    return true;
  }

  std::vector<uint8_t> receive_buffer_;
  std::vector<uint8_t> send_buffer_;

  DISALLOW_COPY_AND_ASSIGN(TestSerialConnection);
};

class GetPTZExpectations {
 public:
  GetPTZExpectations(bool expected_success, int expected_value)
      : expected_success_(expected_success), expected_value_(expected_value) {}

  void OnCallback(bool success, int value, int min_value, int max_value) {
    EXPECT_EQ(expected_success_, success);
    EXPECT_EQ(expected_value_, value);

    // TODO(pbos): min/max values aren't currently supported. These expectations
    // should be updated when we do.
    EXPECT_EQ(0, min_value);
    EXPECT_EQ(0, max_value);
  }

 private:
  const bool expected_success_;
  const int expected_value_;

  DISALLOW_COPY_AND_ASSIGN(GetPTZExpectations);
};

class SetPTZExpectations {
 public:
  explicit SetPTZExpectations(bool expected_success)
      : expected_success_(expected_success) {}

  void OnCallback(bool success) { EXPECT_EQ(expected_success_, success); }

 private:
  const bool expected_success_;

  DISALLOW_COPY_AND_ASSIGN(SetPTZExpectations);
};

template <size_t N>
std::vector<uint8_t> ToByteVector(const char (&array)[N]) {
  return std::vector<uint8_t>(array, array + N);
}

}  // namespace

class ViscaWebcamTest : public testing::Test {
 protected:
  ViscaWebcamTest() {
    mojo::PendingRemote<device::mojom::SerialPort> port;
    ignore_result(port.InitWithNewPipeAndPassReceiver());
    webcam_ = new ViscaWebcam;
    webcam_->OpenForTesting(
        std::make_unique<TestSerialConnection>(std::move(port)));
  }
  ~ViscaWebcamTest() override {}

  Webcam* webcam() { return webcam_.get(); }

  TestSerialConnection* serial_connection() {
    return static_cast<TestSerialConnection*>(
        webcam_->GetSerialConnectionForTesting());
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  scoped_refptr<ViscaWebcam> webcam_;
};

TEST_F(ViscaWebcamTest, Zoom) {
  // Check getting the zoom.
  const char kGetZoomCommand[] = {0x81, 0x09, 0x04, 0x47, 0xFF};
  const char kGetZoomResponse[] = {0x00, 0x50, 0x01, 0x02, 0x03, 0x04, 0xFF};
  serial_connection()->SetReceiveBuffer(ToByteVector(kGetZoomResponse));
  Webcam::GetPTZCompleteCallback receive_callback =
      base::Bind(&GetPTZExpectations::OnCallback,
                 base::Owned(new GetPTZExpectations(true, 0x1234)));
  webcam()->GetZoom(receive_callback);
  base::RunLoop().RunUntilIdle();
  serial_connection()->CheckSendBufferAndClear(ToByteVector(kGetZoomCommand));

  // Check setting the zoom.
  const char kSetZoomCommand[] = {0x81, 0x01, 0x04, 0x47, 0x06,
                                  0x02, 0x05, 0x03, 0xFF};
  // Note: this is a valid, but empty value because nothing is checking it.
  const char kSetZoomResponse[] = {0x00, 0x50, 0x00, 0x00, 0x00, 0x00, 0xFF};
  serial_connection()->SetReceiveBuffer(ToByteVector(kSetZoomResponse));
  Webcam::SetPTZCompleteCallback send_callback =
      base::Bind(&SetPTZExpectations::OnCallback,
                 base::Owned(new SetPTZExpectations(true)));
  serial_connection()->SetReceiveBuffer(ToByteVector(kSetZoomResponse));
  webcam()->SetZoom(0x6253, send_callback);
  base::RunLoop().RunUntilIdle();
  serial_connection()->CheckSendBufferAndClear(ToByteVector(kSetZoomCommand));
}

}  // namespace extensions
