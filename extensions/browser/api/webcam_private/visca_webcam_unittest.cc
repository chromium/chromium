// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "extensions/browser/api/webcam_private/visca_webcam.h"

#include <vector>

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace {

using ::base::test::RunCallback;
using ::base::test::RunOnceCallback;
using ::testing::_;
using ::testing::DoAll;
using ::testing::Return;

class MockSerialConnection : public SerialConnection {
 public:
  explicit MockSerialConnection() : SerialConnection("dummy_id") {
    InitSerialPortForTesting();
  }
  ~MockSerialConnection() override = default;

  MockSerialConnection(const MockSerialConnection&) = delete;
  MockSerialConnection& operator=(const MockSerialConnection&) = delete;

  MOCK_METHOD4(Open,
               void(api::SerialPortManager* port_manager,
                    const std::string& path,
                    const api::serial::ConnectionOptions& options,
                    OpenCompleteCallback callback));
  MOCK_METHOD1(StartPolling, void(const ReceiveEventCallback& callback));
  MOCK_METHOD2(Send,
               void(const std::vector<uint8_t>& data,
                    SendCompleteCallback callback));
};

template <size_t N>
std::vector<uint8_t> ToByteVector(const uint8_t (&array)[N]) {
  return std::vector<uint8_t>(array, array + N);
}

}  // namespace

class ViscaWebcamTest : public testing::Test {
 protected:
  ViscaWebcamTest() {
    webcam_ = new ViscaWebcam;
    webcam_->OpenForTesting(std::make_unique<MockSerialConnection>());
  }
  ~ViscaWebcamTest() override {}

  Webcam* webcam() { return webcam_.get(); }

  MockSerialConnection* serial_connection() {
    return static_cast<MockSerialConnection*>(
        webcam_->GetSerialConnectionForTesting());
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  scoped_refptr<ViscaWebcam> webcam_;
};

TEST_F(ViscaWebcamTest, Zoom) {
  // Check getting the zoom.
  const uint8_t kGetZoomCommand[] = {0x81, 0x09, 0x04, 0x47, 0xFF};
  const uint8_t kGetZoomResponse[] = {0x00, 0x50, 0x01, 0x02, 0x03, 0x04, 0xFF};

  EXPECT_CALL(*serial_connection(), Send(ToByteVector(kGetZoomCommand), _))
      .WillOnce(RunOnceCallback<1>(sizeof(kGetZoomCommand),
                                   api::serial::SendError::kNone));
  EXPECT_CALL(*serial_connection(), StartPolling(_))
      .WillOnce(RunCallback<0>(ToByteVector(kGetZoomResponse),
                               api::serial::ReceiveError::kNone));

  {
    base::RunLoop loop;
    webcam()->GetZoom(base::BindLambdaForTesting(
        [&](bool success, int value, int min_value, int max_value) {
          EXPECT_TRUE(success);
          EXPECT_EQ(0x1234, value);

          // TODO(pbos): min/max values aren't currently supported. These
          // expectations should be updated when we do.
          EXPECT_EQ(0, min_value);
          EXPECT_EQ(0, max_value);

          loop.Quit();
        }));
    loop.Run();
  }

  // Check setting the zoom.
  const uint8_t kSetZoomCommand[] = {0x81, 0x01, 0x04, 0x47, 0x06,
                                     0x02, 0x05, 0x03, 0xFF};
  // Note: this is a valid, but empty value because nothing is checking it.
  const uint8_t kSetZoomResponse[] = {0x00, 0x50, 0x00, 0x00, 0x00, 0x00, 0xFF};

  EXPECT_CALL(*serial_connection(), Send(ToByteVector(kSetZoomCommand), _))
      .WillOnce(RunOnceCallback<1>(sizeof(kSetZoomCommand),
                                   api::serial::SendError::kNone));
  EXPECT_CALL(*serial_connection(), StartPolling(_))
      .WillOnce(RunCallback<0>(ToByteVector(kSetZoomResponse),
                               api::serial::ReceiveError::kNone));

  {
    base::RunLoop loop;
    webcam()->SetZoom(0x6253, base::BindLambdaForTesting([&](bool success) {
                        EXPECT_TRUE(success);
                        loop.Quit();
                      }));
    loop.Run();
  }
}

}  // namespace extensions
