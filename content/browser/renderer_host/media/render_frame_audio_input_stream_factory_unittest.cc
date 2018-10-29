// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/render_frame_audio_input_stream_factory.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/task/post_task.h"
#include "content/browser/media/forwarding_audio_stream_factory.h"
#include "content/browser/renderer_host/media/audio_input_device_manager.h"
#include "content/browser/renderer_host/media/media_stream_manager.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_media_capture_id.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/test_service_manager_context.h"
#include "content/public/test/test_utils.h"
#include "media/audio/audio_system_impl.h"
#include "media/audio/fake_audio_log_factory.h"
#include "media/audio/fake_audio_manager.h"
#include "media/audio/test_audio_thread.h"
#include "media/base/audio_parameters.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "services/audio/public/cpp/fake_stream_factory.h"
#include "services/audio/public/mojom/constants.mojom.h"
#include "services/audio/public/mojom/stream_factory.mojom.h"
#include "services/service_manager/public/cpp/connector.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

class RenderFrameAudioInputStreamFactoryTest
    : public RenderViewHostTestHarness {
 public:
  RenderFrameAudioInputStreamFactoryTest()
      : RenderViewHostTestHarness(),
        test_service_manager_context_(
            std::make_unique<TestServiceManagerContext>()),
        audio_manager_(std::make_unique<media::TestAudioThread>(),
                       &log_factory_),
        audio_system_(media::AudioSystemImpl::CreateInstance()),
        media_stream_manager_(std::make_unique<MediaStreamManager>(
            audio_system_.get(),
            base::CreateSingleThreadTaskRunnerWithTraits(
                {BrowserThread::UI}))) {}

  ~RenderFrameAudioInputStreamFactoryTest() override {}

  void SetUp() override {
    RenderViewHostTestHarness::SetUp();
    RenderFrameHostTester::For(main_rfh())->InitializeRenderFrameIfNeeded();

    // Set up the ForwardingAudioStreamFactory.
    service_manager::Connector::TestApi connector_test_api(
        ForwardingAudioStreamFactory::ForFrame(main_rfh())
            ->core()
            ->get_connector_for_testing());
    connector_test_api.OverrideBinderForTesting(
        service_manager::Identity(audio::mojom::kServiceName),
        audio::mojom::StreamFactory::Name_,
        base::BindRepeating(
            &RenderFrameAudioInputStreamFactoryTest::BindFactory,
            base::Unretained(this)));

    base::RunLoop().RunUntilIdle();
  }

  void TearDown() override {
    audio_manager_.Shutdown();
    test_service_manager_context_.reset();
    RenderViewHostTestHarness::TearDown();
  }

  void BindFactory(mojo::ScopedMessagePipeHandle factory_request) {
    audio_service_stream_factory_.binding_.Bind(
        audio::mojom::StreamFactoryRequest(std::move(factory_request)));
  }

  class MockStreamFactory : public audio::FakeStreamFactory {
   public:
    MockStreamFactory() : binding_(this) {}
    ~MockStreamFactory() override {}

    void CreateInputStream(
        media::mojom::AudioInputStreamRequest stream_request,
        media::mojom::AudioInputStreamClientPtr client,
        media::mojom::AudioInputStreamObserverPtr observer,
        media::mojom::AudioLogPtr log,
        const std::string& device_id,
        const media::AudioParameters& params,
        uint32_t shared_memory_count,
        bool enable_agc,
        mojo::ScopedSharedBufferHandle key_press_count_buffer,
        audio::mojom::AudioProcessingConfigPtr processing_config,
        CreateInputStreamCallback created_callback) override {
      last_created_callback = std::move(created_callback);
    }

    void CreateLoopbackStream(
        media::mojom::AudioInputStreamRequest stream_request,
        media::mojom::AudioInputStreamClientPtr client,
        media::mojom::AudioInputStreamObserverPtr observer,
        const media::AudioParameters& params,
        uint32_t shared_memory_count,
        const base::UnguessableToken& group_id,
        CreateLoopbackStreamCallback created_callback) override {
      last_created_loopback_callback = std::move(created_callback);
    }

    CreateInputStreamCallback last_created_callback;
    CreateLoopbackStreamCallback last_created_loopback_callback;

    mojo::Binding<audio::mojom::StreamFactory> binding_;
  };

  class FakeRendererAudioInputStreamFactoryClient
      : public mojom::RendererAudioInputStreamFactoryClient {
   public:
    void StreamCreated(
        media::mojom::AudioInputStreamPtr stream,
        media::mojom::AudioInputStreamClientRequest client_request,
        media::mojom::ReadOnlyAudioDataPipePtr data_pipe,
        bool initially_muted,
        const base::Optional<base::UnguessableToken>& stream_id) override {}
  };

  AudioInputDeviceManager* audio_input_device_manager() {
    return media_stream_manager_->audio_input_device_manager();
  }

  class StreamOpenedWaiter : public MediaStreamProviderListener {
   public:
    explicit StreamOpenedWaiter(scoped_refptr<AudioInputDeviceManager> aidm,
                                base::OnceClosure callback)
        : aidm_(aidm), cb_(std::move(callback)) {
      aidm->RegisterListener(this);
    }

    ~StreamOpenedWaiter() override { aidm_->UnregisterListener(this); }

    void Opened(MediaStreamType stream_type, int capture_session_id) override {
      std::move(cb_).Run();
    }
    void Closed(MediaStreamType stream_type, int capture_session_id) override {}
    void Aborted(MediaStreamType stream_type, int capture_session_id) override {
    }

   private:
    scoped_refptr<AudioInputDeviceManager> aidm_;
    base::OnceClosure cb_;
  };

  void CallOpenWithTestDeviceAndStoreSessionIdOnIO(int* session_id) {
    *session_id = audio_input_device_manager()->Open(
        MediaStreamDevice(MEDIA_DEVICE_AUDIO_CAPTURE, kDeviceId, kDeviceName));
  }

  const media::AudioParameters kParams =
      media::AudioParameters::UnavailableDeviceParams();
  const std::string kDeviceId = "test id";
  const std::string kDeviceName = "test name";
  const bool kAGC = false;
  const uint32_t kSharedMemoryCount = 123;
  MockStreamFactory audio_service_stream_factory_;
  std::unique_ptr<TestServiceManagerContext> test_service_manager_context_;
  media::FakeAudioLogFactory log_factory_;
  media::FakeAudioManager audio_manager_;
  std::unique_ptr<media::AudioSystem> audio_system_;
  std::unique_ptr<MediaStreamManager> media_stream_manager_;
};

TEST_F(RenderFrameAudioInputStreamFactoryTest, ConstructDestruct) {
  mojom::RendererAudioInputStreamFactoryPtr factory_ptr;
  RenderFrameAudioInputStreamFactory factory(
      mojo::MakeRequest(&factory_ptr), media_stream_manager_.get(), main_rfh());
}

TEST_F(RenderFrameAudioInputStreamFactoryTest,
       CreateOpenedStream_ForwardsCall) {
  mojom::RendererAudioInputStreamFactoryPtr factory_ptr;
  RenderFrameAudioInputStreamFactory factory(
      mojo::MakeRequest(&factory_ptr), media_stream_manager_.get(), main_rfh());

  int session_id = audio_input_device_manager()->Open(
      MediaStreamDevice(MEDIA_DEVICE_AUDIO_CAPTURE, kDeviceId, kDeviceName));
  base::RunLoop().RunUntilIdle();

  mojom::RendererAudioInputStreamFactoryClientPtr client;
  mojo::MakeRequest(&client);
  factory_ptr->CreateStream(std::move(client), session_id, kParams, kAGC,
                            kSharedMemoryCount, nullptr);

  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(!!audio_service_stream_factory_.last_created_callback);
}

TEST_F(RenderFrameAudioInputStreamFactoryTest,
       CreateWebContentsCapture_ForwardsCall) {
  std::unique_ptr<WebContents> source_contents = CreateTestWebContents();
  mojom::RendererAudioInputStreamFactoryPtr factory_ptr;
  RenderFrameAudioInputStreamFactory factory(
      mojo::MakeRequest(&factory_ptr), media_stream_manager_.get(), main_rfh());

  RenderFrameHost* main_frame = source_contents->GetMainFrame();
  WebContentsMediaCaptureId capture_id(main_frame->GetProcess()->GetID(),
                                       main_frame->GetRoutingID());
  int session_id = audio_input_device_manager()->Open(MediaStreamDevice(
      MEDIA_GUM_TAB_AUDIO_CAPTURE, capture_id.ToString(), kDeviceName));
  base::RunLoop().RunUntilIdle();

  mojom::RendererAudioInputStreamFactoryClientPtr client;
  mojo::MakeRequest(&client);
  factory_ptr->CreateStream(std::move(client), session_id, kParams, kAGC,
                            kSharedMemoryCount, nullptr);

  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(!!audio_service_stream_factory_.last_created_loopback_callback);
}

TEST_F(RenderFrameAudioInputStreamFactoryTest,
       CreateWebContentsCaptureAfterCaptureSourceDestructed_Fails) {
  std::unique_ptr<WebContents> source_contents = CreateTestWebContents();
  mojom::RendererAudioInputStreamFactoryPtr factory_ptr;
  RenderFrameAudioInputStreamFactory factory(
      mojo::MakeRequest(&factory_ptr), media_stream_manager_.get(), main_rfh());

  RenderFrameHost* main_frame = source_contents->GetMainFrame();
  WebContentsMediaCaptureId capture_id(main_frame->GetProcess()->GetID(),
                                       main_frame->GetRoutingID());
  int session_id = audio_input_device_manager()->Open(MediaStreamDevice(
      MEDIA_GUM_TAB_AUDIO_CAPTURE, capture_id.ToString(), kDeviceName));
  base::RunLoop().RunUntilIdle();

  source_contents.reset();
  mojom::RendererAudioInputStreamFactoryClientPtr client;
  mojo::MakeRequest(&client);
  factory_ptr->CreateStream(std::move(client), session_id, kParams, kAGC,
                            kSharedMemoryCount, nullptr);

  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(!!audio_service_stream_factory_.last_created_loopback_callback);
}

TEST_F(RenderFrameAudioInputStreamFactoryTest,
       CreateStreamWithoutValidSessionId_Fails) {
  mojom::RendererAudioInputStreamFactoryPtr factory_ptr;
  RenderFrameAudioInputStreamFactory factory(
      mojo::MakeRequest(&factory_ptr), media_stream_manager_.get(), main_rfh());

  int session_id = 123;
  mojom::RendererAudioInputStreamFactoryClientPtr client;
  mojo::MakeRequest(&client);

  factory_ptr->CreateStream(std::move(client), session_id, kParams, kAGC,
                            kSharedMemoryCount, nullptr);

  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(!!audio_service_stream_factory_.last_created_callback);
}

}  // namespace content
