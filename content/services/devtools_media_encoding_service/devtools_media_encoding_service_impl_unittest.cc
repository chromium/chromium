// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/services/devtools_media_encoding_service/devtools_media_encoding_service_impl.h"

#include <vector>

#include "content/public/test/browser_task_environment.h"
#include "content/services/devtools_media_encoding_service/public/mojom/devtools_media_encoding_service.mojom.h"
#include "media/media_buildflags.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

class DummyClient : public devtools_media_encoding_service::mojom::
                        DevToolsMediaEncodingServiceClient {
 public:
  DummyClient(
      mojo::PendingReceiver<devtools_media_encoding_service::mojom::
                                DevToolsMediaEncodingServiceClient> receiver)
      : receiver_(this, std::move(receiver)) {}

  void OnData(const std::vector<uint8_t>& data) override {
    data_received_ += data.size();
  }

  void OnClosed() override { is_closed_ = true; }

  size_t data_received() const { return data_received_; }
  bool is_closed() const { return is_closed_; }

 private:
  mojo::Receiver<
      devtools_media_encoding_service::mojom::DevToolsMediaEncodingServiceClient>
      receiver_;
  size_t data_received_ = 0;
  bool is_closed_ = false;
};

class DevToolsMediaEncodingServiceImplTest : public testing::Test {
 public:
  DevToolsMediaEncodingServiceImplTest() = default;

 protected:
  BrowserTaskEnvironment task_environment_;
};

TEST_F(DevToolsMediaEncodingServiceImplTest, StartAndStopRecording) {
  mojo::Remote<
      devtools_media_encoding_service::mojom::DevToolsMediaEncodingService>
      server_remote;
  DevToolsMediaEncodingServiceImpl server(
      server_remote.BindNewPipeAndPassReceiver());

  mojo::Remote<
      devtools_media_encoding_service::mojom::DevToolsMediaEncodingServiceClient>
      client_remote;
  DummyClient client(client_remote.BindNewPipeAndPassReceiver());

  server_remote->StartRecording(client_remote.Unbind(), 800, 600, 30, false);
  server_remote->StopRecording();

  task_environment_.RunUntilIdle();
  EXPECT_TRUE(client.is_closed());
}

TEST_F(DevToolsMediaEncodingServiceImplTest, StartRecordingWhileActive_Rejects) {
  mojo::Remote<
      devtools_media_encoding_service::mojom::DevToolsMediaEncodingService>
      server_remote;
  DevToolsMediaEncodingServiceImpl server(
      server_remote.BindNewPipeAndPassReceiver());

  mojo::Remote<
      devtools_media_encoding_service::mojom::DevToolsMediaEncodingServiceClient>
      client_remote1;
  DummyClient client1(client_remote1.BindNewPipeAndPassReceiver());

  mojo::Remote<
      devtools_media_encoding_service::mojom::DevToolsMediaEncodingServiceClient>
      client_remote2;
  DummyClient client2(client_remote2.BindNewPipeAndPassReceiver());

  server_remote->StartRecording(client_remote1.Unbind(), 800, 600, 30, false);

  mojo::test::BadMessageObserver bad_message_observer;
  server_remote->StartRecording(client_remote2.Unbind(), 800, 600, 30, false);

  EXPECT_EQ(bad_message_observer.WaitForBadMessage(),
            "Recording is already active");
}

}  // namespace content
