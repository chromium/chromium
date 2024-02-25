// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/render_frame_audio_input_stream_factory.h"

#include <string>
#include <tuple>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
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
#include "content/public/test/test_utils.h"
#include "media/audio/audio_system_impl.h"
#include "media/audio/fake_audio_log_factory.h"
#include "media/audio/fake_audio_manager.h"
#include "media/audio/test_audio_thread.h"
#include "media/base/audio_parameters.h"
#include "media/mojo/mojom/audio_data_pipe.mojom.h"
#include "media/mojo/mojom/audio_processing.mojom.h"
#include "media/mojo/mojom/audio_stream_factory.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/audio/public/cpp/fake_stream_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/lacros/lacros_test_helper.h"
#endif

namespace content {

// RenderViewHostTestHarness works poorly on Android.
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_RenderFrameAudioInputStreamFactoryTest \
  DISABLED_RenderFrameAudioInputStreamFactoryTest
#else
#define MAYBE_RenderFrameAudioInputStreamFactoryTest \
  RenderFrameAudioInputStreamFactoryTest
#endif

class MAYBE_RenderFrameAudioInputStreamFactoryTest
    : public RenderViewHostTestHarness {
 public:
  MAYBE_RenderFrameAudioInputStreamFactoryTest()
      : RenderViewHostTestHarness(),
        audio_manager_(std::make_unique<media::TestAudioThread>(),
                       &log_factory_),
        audio_system_(media::AudioSystemImpl::CreateInstance()),
        media_stream_manager_(
            std::make_unique<MediaStreamManager>(audio_system_.get())) {}

  ~MAYBE_RenderFrameAudioInputStreamFactoryTest() override {}

  void SetUp() override {
    RenderViewHostTestHarness::SetUp();
    RenderFrameHostTester::For(main_rfh())->InitializeRenderFrameIfNeeded();

    // Set up the ForwardingAudioStreamFactory.
    ForwardingAudioStreamFactory::OverrideAudioStreamFactoryBinderForTesting(
        base::BindRepeating(
            &MAYBE_RenderFrameAudioInputStreamFactoryTest::BindFactory,
            base::Unretained(this)));

    base::RunLoop().RunUntilIdle();
  }

  void TearDown() override {
    ForwardingAudioStreamFactory::OverrideAudioStreamFactoryBinderForTesting(
        base::NullCallback());
    audio_manager_.Shutdown();
    RenderViewHostTestHarness::TearDown();
  }

  void BindFactory(
      mojo::PendingReceiver<media::mojom::AudioStreamFactory> receiver) {
    audio_service_stream_factory_.receiver_.Bind(std::move(receiver));
  }

  class MockStreamFactory : public audio::FakeStreamFactory {
   public:
    MockStreamFactory() = default;
    ~MockStreamFactory() override = default;

    void CreateInputStream(
        mojo::PendingReceiver<media::mojom::AudioInputStream> stream_receiver,
        mojo::PendingRemote<media::mojom::AudioInputStreamClient> client,
        mojo::PendingRemote<media::mojom::AudioInputStreamObserver> observer,
        mojo::PendingRemote<media::mojom::AudioLog> log,
        const std::string& device_id,
        const media::AudioParameters& params,
        uint32_t shared_memory_count,
        bool enable_agc,
        base::ReadOnlySharedMemoryRegion key_press_count_buffer,
        media::mojom::AudioProcessingConfigPtr processing_config,
        CreateInputStreamCallback created_callback) override {
      last_created_callback = std::move(created_callback);
    }

    void CreateLoopbackStream(
        mojo::PendingReceiver<media::mojom::AudioInputStream> receiver,
        mojo::PendingRemote<media::mojom::AudioInputStreamClient> client,
        mojo::PendingRemote<media::mojom::AudioInputStreamObserver> observer,
        const media::AudioParameters& params,
        uint32_t shared_memory_count,
        const base::UnguessableToken& group_id,
        CreateLoopbackStreamCallback created_callback) override {
      last_created_loopback_callback = std::move(created_callback);
    }

    CreateInputStreamCallback last_created_callback;
    CreateLoopbackStreamCallback last_created_loopback_callback;

    mojo::Receiver<media::mojom::AudioStreamFactory> receiver_{this};
  };

  class FakeRendererAudioInputStreamFactoryClient
      : public blink::mojom::RendererAudioInputStreamFactoryClient {
   public:
    void StreamCreated(
        mojo::PendingRemote<media::mojom::AudioInputStream> stream,
        mojo::PendingReceiver<media::mojom::AudioInputStreamClient>
            client_receiver,
        media::mojom::ReadOnlyAudioDataPipePtr data_pipe,
        bool initially_muted,
        const std::optional<base::UnguessableToken>& stream_id) override {}
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

    void Opened(blink::mojom::MediaStreamType stream_type,
                const base::UnguessableToken& capture_session_id) override {
      std::move(cb_).Run();
    }
    void Closed(blink::mojom::MediaStreamType stream_type,
                const base::UnguessableToken& capture_session_id) override {}
    void Aborted(blink::mojom::MediaStreamType stream_type,
                 const base::UnguessableToken& capture_session_id) override {}

   private:
    scoped_refptr<AudioInputDeviceManager> aidm_;
    base::OnceClosure cb_;
  };

  void CallOpenWithTestDeviceAndStoreSessionIdOnIO(
      base::UnguessableToken* session_id) {
    *session_id = audio_input_device_manager()->Open(blink::MediaStreamDevice(
        blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE, kDeviceId,
        kDeviceName));
  }

  const media::AudioParameters kParams =
      media::AudioParameters::UnavailableDeviceParams();
  const std::string kDeviceId = "test id";
  const std::string kDeviceName = "test name";
  const bool kAGC = false;
  const uint32_t kSharedMemoryCount = 123;
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // Instantiate LacrosService for WakeLock support.
  chromeos::ScopedLacrosServiceTestHelper scoped_lacros_service_test_helper_;
#endif
  MockStreamFactory audio_service_stream_factory_;
  media::FakeAudioLogFactory log_factory_;
  media::FakeAudioManager audio_manager_;
  std::unique_ptr<media::AudioSystem> audio_system_;
  std::unique_ptr<MediaStreamManager> media_stream_manager_;
};

TEST_F(MAYBE_RenderFrameAudioInputStreamFactoryTest, ConstructDestruct) {
  mojo::Remote<blink::mojom::RendererAudioInputStreamFactory> factory_remote;
  RenderFrameAudioInputStreamFactory factory(
      factory_remote.BindNewPipeAndPassReceiver(), media_stream_manager_.get(),
      main_rfh());
}

TEST_F(MAYBE_RenderFrameAudioInputStreamFactoryTest,
       CreateOpenedStream_ForwardsCall) {
  mojo::Remote<blink::mojom::RendererAudioInputStreamFactory> factory_remote;
  RenderFrameAudioInputStreamFactory factory(
      factory_remote.BindNewPipeAndPassReceiver(), media_stream_manager_.get(),
      main_rfh());

  base::UnguessableToken session_id =
      audio_input_device_manager()->Open(blink::MediaStreamDevice(
          blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE, kDeviceId,
          kDeviceName));
  base::RunLoop().RunUntilIdle();

  mojo::PendingRemote<blink::mojom::RendererAudioInputStreamFactoryClient>
      client;
  std::ignore = client.InitWithNewPipeAndPassReceiver();
  factory_remote->CreateStream(std::move(client), session_id, kParams, kAGC,
                               kSharedMemoryCount, nullptr);

  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(!!audio_service_stream_factory_.last_created_callback);
}

TEST_F(MAYBE_RenderFrameAudioInputStreamFactoryTest,
       CreateWebContentsCapture_ForwardsCall) {
  std::unique_ptr<WebContents> source_contents = CreateTestWebContents();
  mojo::Remote<blink::mojom::RendererAudioInputStreamFactory> factory_remote;
  RenderFrameAudioInputStreamFactory factory(
      factory_remote.BindNewPipeAndPassReceiver(), media_stream_manager_.get(),
      main_rfh());

  RenderFrameHost* main_frame = source_contents->GetPrimaryMainFrame();
  WebContentsMediaCaptureId capture_id(main_frame->GetProcess()->GetID(),
                                       main_frame->GetRoutingID());
  base::UnguessableToken session_id =
      audio_input_device_manager()->Open(blink::MediaStreamDevice(
          blink::mojom::MediaStreamType::GUM_TAB_AUDIO_CAPTURE,
          capture_id.ToString(), kDeviceName));
  base::RunLoop().RunUntilIdle();

  mojo::PendingRemote<blink::mojom::RendererAudioInputStreamFactoryClient>
      client;
  std::ignore = client.InitWithNewPipeAndPassReceiver();
  factory_remote->CreateStream(std::move(client), session_id, kParams, kAGC,
                               kSharedMemoryCount, nullptr);

  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(!!audio_service_stream_factory_.last_created_loopback_callback);
}

TEST_F(MAYBE_RenderFrameAudioInputStreamFactoryTest,
       CreateWebContentsCaptureAfterCaptureSourceDestructed_Fails) {
  std::unique_ptr<WebContents> source_contents = CreateTestWebContents();
  mojo::Remote<blink::mojom::RendererAudioInputStreamFactory> factory_remote;
  RenderFrameAudioInputStreamFactory factory(
      factory_remote.BindNewPipeAndPassReceiver(), media_stream_manager_.get(),
      main_rfh());

  RenderFrameHost* main_frame = source_contents->GetPrimaryMainFrame();
  WebContentsMediaCaptureId capture_id(main_frame->GetProcess()->GetID(),
                                       main_frame->GetRoutingID());
  base::UnguessableToken session_id =
      audio_input_device_manager()->Open(blink::MediaStreamDevice(
          blink::mojom::MediaStreamType::GUM_TAB_AUDIO_CAPTURE,
          capture_id.ToString(), kDeviceName));
  base::RunLoop().RunUntilIdle();

  source_contents.reset();
  mojo::PendingRemote<blink::mojom::RendererAudioInputStreamFactoryClient>
      client;
  std::ignore = client.InitWithNewPipeAndPassReceiver();
  factory_remote->CreateStream(std::move(client), session_id, kParams, kAGC,
                               kSharedMemoryCount, nullptr);

  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(!!audio_service_stream_factory_.last_created_loopback_callback);
}

TEST_F(MAYBE_RenderFrameAudioInputStreamFactoryTest,
       CreateStreamWithoutValidSessionId_Fails) {
  mojo::Remote<blink::mojom::RendererAudioInputStreamFactory> factory_remote;
  RenderFrameAudioInputStreamFactory factory(
      factory_remote.BindNewPipeAndPassReceiver(), media_stream_manager_.get(),
      main_rfh());

  base::UnguessableToken session_id = base::UnguessableToken::Create();
  mojo::PendingRemote<blink::mojom::RendererAudioInputStreamFactoryClient>
      client;
  std::ignore = client.InitWithNewPipeAndPassReceiver();
  factory_remote->CreateStream(std::move(client), session_id, kParams, kAGC,
                               kSharedMemoryCount, nullptr);

  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(!!audio_service_stream_factory_.last_created_callback);
}

}  // namespace content
