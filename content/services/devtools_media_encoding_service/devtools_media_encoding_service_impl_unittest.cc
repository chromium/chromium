// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/services/devtools_media_encoding_service/devtools_media_encoding_service_impl.h"

#include <string_view>
#include <vector>

#include "base/run_loop.h"
#include "base/time/time.h"
#include "content/public/test/browser_task_environment.h"
#include "content/services/devtools_media_encoding_service/public/mojom/devtools_media_encoding_service.mojom.h"
#include "media/base/audio_buffer.h"
#include "media/base/audio_bus.h"
#include "media/base/test_helpers.h"
#include "media/base/video_frame.h"
#include "media/media_buildflags.h"
#include "media/mojo/common/media_type_converters.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/size.h"

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
    raw_data_.insert(raw_data_.end(), data.begin(), data.end());
  }

  void OnClosed() override {
    is_closed_ = true;
    if (quit_closure_) {
      std::move(quit_closure_).Run();
    }
  }

  void WaitUntilClosed() {
    if (is_closed_) {
      return;
    }
    base::RunLoop run_loop;
    quit_closure_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  bool ContainsBox(std::string_view box_type) const {
    if (raw_data_.size() < 4) {
      return false;
    }
    std::string_view str(reinterpret_cast<const char*>(raw_data_.data()),
                         raw_data_.size());
    return str.find(box_type) != std::string_view::npos;
  }

  size_t data_received() const { return data_received_; }
  bool is_closed() const { return is_closed_; }

 private:
  mojo::Receiver<
      devtools_media_encoding_service::mojom::DevToolsMediaEncodingServiceClient>
      receiver_;
  size_t data_received_ = 0;
  bool is_closed_ = false;
  std::vector<uint8_t> raw_data_;
  base::OnceClosure quit_closure_;
};

class DevToolsMediaEncodingServiceImplTest : public testing::Test {
 public:
  DevToolsMediaEncodingServiceImplTest() = default;

 protected:
  BrowserTaskEnvironment task_environment_;
};

#if BUILDFLAG(ENABLE_LIBAOM)

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

  client.WaitUntilClosed();
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

TEST_F(DevToolsMediaEncodingServiceImplTest, RecordVideoFrame) {
  mojo::Remote<
      devtools_media_encoding_service::mojom::DevToolsMediaEncodingService>
      server_remote;
  DevToolsMediaEncodingServiceImpl server(
      server_remote.BindNewPipeAndPassReceiver());

  mojo::Remote<devtools_media_encoding_service::mojom::
                   DevToolsMediaEncodingServiceClient>
      client_remote;
  DummyClient client(client_remote.BindNewPipeAndPassReceiver());

  server_remote->StartRecording(client_remote.Unbind(), 800, 600, 30, false);

  gfx::Size size(800, 600);
  base::TimeTicks timestamp = base::TimeTicks::Now();

  for (int i = 0; i < 5; ++i) {
    auto frame = media::VideoFrame::CreateBlackFrame(size);
    frame->set_timestamp(timestamp - base::TimeTicks() +
                         base::Milliseconds(i * 33));
    server_remote->RecordVideoFrame(frame);
  }

  server_remote->StopRecording();

  client.WaitUntilClosed();
  EXPECT_TRUE(client.is_closed());
  EXPECT_GT(client.data_received(), 0u);
  EXPECT_TRUE(client.ContainsBox("ftyp"));
  EXPECT_TRUE(client.ContainsBox("mdat"));
  EXPECT_TRUE(client.ContainsBox("moov"));
}

TEST_F(DevToolsMediaEncodingServiceImplTest, RecordAudioBuffer) {
  mojo::Remote<
      devtools_media_encoding_service::mojom::DevToolsMediaEncodingService>
      server_remote;
  DevToolsMediaEncodingServiceImpl server(
      server_remote.BindNewPipeAndPassReceiver());

  mojo::Remote<devtools_media_encoding_service::mojom::
                   DevToolsMediaEncodingServiceClient>
      client_remote;
  DummyClient client(client_remote.BindNewPipeAndPassReceiver());

  server_remote->StartRecording(client_remote.Unbind(), 800, 600, 30, true);

  for (int i = 0; i < 5; ++i) {
    auto buffer = media::MakeAudioBuffer<float>(
        media::kSampleFormatF32, media::CHANNEL_LAYOUT_STEREO, 2, 48000, 0.0f,
        0.0f, 480,
        base::TimeTicks::Now() - base::TimeTicks() +
            base::Milliseconds(i * 10));
    server_remote->RecordAudioBuffer(media::mojom::AudioBuffer::From(*buffer));
  }

  server_remote->StopRecording();

  client.WaitUntilClosed();
  EXPECT_TRUE(client.is_closed());
  EXPECT_GT(client.data_received(), 0u);
  EXPECT_TRUE(client.ContainsBox("ftyp"));
  EXPECT_TRUE(client.ContainsBox("mdat"));
  EXPECT_TRUE(client.ContainsBox("moov"));
}

TEST_F(DevToolsMediaEncodingServiceImplTest, RecordVideoAndAudio) {
  mojo::Remote<
      devtools_media_encoding_service::mojom::DevToolsMediaEncodingService>
      server_remote;
  DevToolsMediaEncodingServiceImpl server(
      server_remote.BindNewPipeAndPassReceiver());

  mojo::Remote<devtools_media_encoding_service::mojom::
                   DevToolsMediaEncodingServiceClient>
      client_remote;
  DummyClient client(client_remote.BindNewPipeAndPassReceiver());

  server_remote->StartRecording(client_remote.Unbind(), 800, 600, 30, true);

  gfx::Size size(800, 600);
  base::TimeTicks timestamp = base::TimeTicks::Now();

  for (int i = 0; i < 5; ++i) {
    auto frame = media::VideoFrame::CreateBlackFrame(size);
    frame->set_timestamp(timestamp - base::TimeTicks() +
                         base::Milliseconds(i * 33));
    server_remote->RecordVideoFrame(frame);

    auto buffer = media::MakeAudioBuffer<float>(
        media::kSampleFormatF32, media::CHANNEL_LAYOUT_STEREO, 2, 48000, 0.0f,
        0.0f, 480, timestamp - base::TimeTicks() + base::Milliseconds(i * 10));
    server_remote->RecordAudioBuffer(media::mojom::AudioBuffer::From(*buffer));
  }

  server_remote->StopRecording();

  client.WaitUntilClosed();
  EXPECT_TRUE(client.is_closed());
  EXPECT_GT(client.data_received(), 0u);
  EXPECT_TRUE(client.ContainsBox("ftyp"));
  EXPECT_TRUE(client.ContainsBox("mdat"));
  EXPECT_TRUE(client.ContainsBox("moov"));
}

TEST_F(DevToolsMediaEncodingServiceImplTest, ChangeVideoSurfaceSize) {
  mojo::Remote<
      devtools_media_encoding_service::mojom::DevToolsMediaEncodingService>
      server_remote;
  DevToolsMediaEncodingServiceImpl server(
      server_remote.BindNewPipeAndPassReceiver());

  mojo::Remote<devtools_media_encoding_service::mojom::
                   DevToolsMediaEncodingServiceClient>
      client_remote;
  DummyClient client(client_remote.BindNewPipeAndPassReceiver());

  server_remote->StartRecording(client_remote.Unbind(), 800, 600, 30, false);

  base::TimeTicks timestamp = base::TimeTicks::Now();

  for (int i = 0; i < 3; ++i) {
    auto frame = media::VideoFrame::CreateBlackFrame(gfx::Size(800, 600));
    frame->set_timestamp(timestamp - base::TimeTicks() +
                         base::Milliseconds(i * 33));
    server_remote->RecordVideoFrame(frame);
  }

  // Change size to trigger flush
  for (int i = 3; i < 6; ++i) {
    auto frame = media::VideoFrame::CreateBlackFrame(gfx::Size(400, 300));
    frame->set_timestamp(timestamp - base::TimeTicks() +
                         base::Milliseconds(i * 33));
    server_remote->RecordVideoFrame(frame);
  }

  server_remote->StopRecording();

  client.WaitUntilClosed();
  EXPECT_TRUE(client.is_closed());
  EXPECT_GT(client.data_received(), 0u);
  EXPECT_TRUE(client.ContainsBox("ftyp"));
  EXPECT_TRUE(client.ContainsBox("mdat"));
  EXPECT_TRUE(client.ContainsBox("moov"));
}

#else

TEST_F(DevToolsMediaEncodingServiceImplTest, VideoEncodingNotSupported) {
  mojo::Remote<
      devtools_media_encoding_service::mojom::DevToolsMediaEncodingService>
      server_remote;
  DevToolsMediaEncodingServiceImpl server(
      server_remote.BindNewPipeAndPassReceiver());

  mojo::Remote<devtools_media_encoding_service::mojom::
                   DevToolsMediaEncodingServiceClient>
      client_remote;
  DummyClient client(client_remote.BindNewPipeAndPassReceiver());

  mojo::test::BadMessageObserver bad_message_observer;
  server_remote->StartRecording(client_remote.Unbind(), 800, 600, 30, false);

  EXPECT_EQ(bad_message_observer.WaitForBadMessage(),
            "Video encoding is not supported");
}

#endif

}  // namespace content
