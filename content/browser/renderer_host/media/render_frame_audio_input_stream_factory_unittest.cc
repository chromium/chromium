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
#include "base/test/bind.h"
#include "base/test/run_until.h"
#include "build/build_config.h"
#include "content/browser/media/forwarding_audio_stream_factory.h"
#include "content/browser/renderer_host/media/audio_input_device_manager.h"
#include "content/browser/renderer_host/media/media_stream_manager.h"
#include "content/browser/renderer_host/media/media_stream_ui_proxy.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/desktop_media_id.h"
#include "content/public/browser/desktop_streams_registry.h"
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
        const base::UnguessableToken& group_id,
        uint32_t shared_memory_count,
        bool enable_agc,
        media::mojom::AudioProcessingConfigPtr processing_config,
        CreateInputStreamCallback created_callback) override {
      input_observers.emplace_back(std::move(observer));
      if (last_created_callback) {
        created_callbacks.push_back(std::move(last_created_callback));
      }
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
      loopback_observers.emplace_back(std::move(observer));
      last_created_loopback_callback = std::move(created_callback);
    }

    std::vector<mojo::Remote<media::mojom::AudioInputStreamObserver>>
        input_observers;
    std::vector<mojo::Remote<media::mojom::AudioInputStreamObserver>>
        loopback_observers;
    std::vector<CreateInputStreamCallback> created_callbacks;
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
        media::mojom::ReadWriteAudioDataPipePtr data_pipe,
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

  blink::MediaStreamDevice GenerateAudioStreamDevice(
      blink::mojom::MediaStreamType stream_type,
      const std::string& device_id,
      const url::Origin& origin,
      int page_request_id = 1,
      blink::mojom::StreamSelectionInfoPtr audio_stream_selection_info_ptr =
          blink::mojom::StreamSelectionInfo::NewSearchOnlyByDeviceId({})) {
    base::RunLoop run_loop;
    blink::MediaStreamDevice audio_device;

    blink::StreamControls controls(true /* request_audio */,
                                   false /* request_video */);
    controls.audio.stream_type = stream_type;

    std::string resolved_device_id =
        device_id.empty() ? "fake_default_mic_id" : device_id;
    blink::MediaStreamDevice fake_device(stream_type, resolved_device_id,
                                         "Fake Device");

    if (!device_id.empty()) {
      controls.audio.device_ids = {device_id};
    }

    media_stream_manager_->UseFakeUIFactoryForTests(base::BindRepeating(
        [](blink::MediaStreamDevice fake_device) {
          auto fake_ui = std::make_unique<FakeMediaStreamUIProxy>(
              /*tests_use_fake_render_frame_hosts=*/true);
          fake_ui->AddAvailableDevices({fake_device});
          return fake_ui;
        },
        fake_device));

    media_stream_manager_->GenerateStreams(
        main_rfh()->GetGlobalId(), /*requester_id=*/1, page_request_id,
        controls, MediaDeviceSaltAndOrigin("salt", origin),
        /*user_gesture=*/true, std::move(audio_stream_selection_info_ptr),
        base::BindOnce(
            [](base::RunLoop* run_loop, blink::MediaStreamDevice* audio_device,
               blink::mojom::MediaStreamRequestResult result,
               const std::string& label,
               blink::mojom::StreamDevicesSetPtr stream_devices_set,
               bool pan_tilt_zoom_allowed) {
              DCHECK_EQ(result, blink::mojom::MediaStreamRequestResult::OK);
              DCHECK_EQ(stream_devices_set->stream_devices.size(), 1u);
              DCHECK(stream_devices_set->stream_devices[0]
                         ->audio_device.has_value());
              *audio_device =
                  *stream_devices_set->stream_devices[0]->audio_device;
              run_loop->Quit();
            },
            &run_loop, &audio_device),
        base::DoNothing(), base::DoNothing(), base::DoNothing(),
        base::DoNothing(), base::DoNothing(), base::DoNothing());

    run_loop.Run();
    return audio_device;
  }

  base::UnguessableToken GenerateAudioStream(
      blink::mojom::MediaStreamType stream_type,
      const std::string& device_id,
      const url::Origin& origin,
      int page_request_id = 1,
      blink::mojom::StreamSelectionInfoPtr audio_stream_selection_info_ptr =
          blink::mojom::StreamSelectionInfo::NewSearchOnlyByDeviceId({})) {
    return GenerateAudioStreamDevice(stream_type, device_id, origin,
                                     page_request_id,
                                     std::move(audio_stream_selection_info_ptr))
        .session_id();
  }

  const media::AudioParameters kParams =
      media::AudioParameters::UnavailableDeviceParams();
  const std::string kDeviceId = "test id";
  const std::string kDeviceName = "test name";
  const bool kAGC = false;
  const uint32_t kSharedMemoryCount = 123;
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

  url::Origin origin = url::Origin::Create(GURL("https://test.com"));
  base::UnguessableToken session_id = GenerateAudioStream(
      blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE, "", origin);

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
  WebContentsMediaCaptureId capture_id(
      main_frame->GetProcess()->GetDeprecatedID(), main_frame->GetRoutingID());

  url::Origin origin = url::Origin::Create(GURL("https://test.com"));
  DesktopMediaID media_id(DesktopMediaID::TYPE_WEB_CONTENTS,
                          DesktopMediaID::kNullId, capture_id);
  std::string stream_id = DesktopStreamsRegistry::GetInstance()->RegisterStream(
      main_rfh()->GetProcess()->GetDeprecatedID(), main_rfh()->GetRoutingID(),
      origin, media_id, kRegistryStreamTypeTab);

  base::UnguessableToken session_id = GenerateAudioStream(
      blink::mojom::MediaStreamType::GUM_TAB_AUDIO_CAPTURE, stream_id, origin);

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
  WebContentsMediaCaptureId capture_id(
      main_frame->GetProcess()->GetDeprecatedID(), main_frame->GetRoutingID());

  url::Origin origin = url::Origin::Create(GURL("https://test.com"));
  DesktopMediaID media_id(DesktopMediaID::TYPE_WEB_CONTENTS,
                          DesktopMediaID::kNullId, capture_id);
  std::string stream_id = DesktopStreamsRegistry::GetInstance()->RegisterStream(
      main_rfh()->GetProcess()->GetDeprecatedID(), main_rfh()->GetRoutingID(),
      origin, media_id, kRegistryStreamTypeTab);

  base::UnguessableToken session_id = GenerateAudioStream(
      blink::mojom::MediaStreamType::GUM_TAB_AUDIO_CAPTURE, stream_id, origin);

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

TEST_F(MAYBE_RenderFrameAudioInputStreamFactoryTest,
       CloseSession_ClosesCreatedInputStream) {
  mojo::Remote<blink::mojom::RendererAudioInputStreamFactory> factory_remote;
  RenderFrameAudioInputStreamFactory factory(
      factory_remote.BindNewPipeAndPassReceiver(), media_stream_manager_.get(),
      main_rfh());

  url::Origin origin = url::Origin::Create(GURL("https://test.com"));
  base::UnguessableToken session_id = GenerateAudioStream(
      blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE, "", origin);

  FakeRendererAudioInputStreamFactoryClient fake_client;
  mojo::Receiver<blink::mojom::RendererAudioInputStreamFactoryClient>
      client_receiver(&fake_client);
  factory_remote->CreateStream(client_receiver.BindNewPipeAndPassRemote(),
                               session_id, kParams, kAGC, kSharedMemoryCount,
                               nullptr);

  ASSERT_TRUE(base::test::RunUntil([&]() {
    return audio_service_stream_factory_.input_observers.size() == 1u;
  }));
  EXPECT_TRUE(audio_service_stream_factory_.input_observers[0].is_connected());

  audio_input_device_manager()->Close(session_id);
  audio_service_stream_factory_.input_observers[0].FlushForTesting();

  EXPECT_FALSE(audio_service_stream_factory_.input_observers[0].is_connected());
}

TEST_F(MAYBE_RenderFrameAudioInputStreamFactoryTest,
       CloseSession_ClosesMultipleCreatedInputStreams) {
  mojo::Remote<blink::mojom::RendererAudioInputStreamFactory> factory_remote;
  RenderFrameAudioInputStreamFactory factory(
      factory_remote.BindNewPipeAndPassReceiver(), media_stream_manager_.get(),
      main_rfh());

  url::Origin origin = url::Origin::Create(GURL("https://test.com"));
  base::UnguessableToken session_id = GenerateAudioStream(
      blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE, "", origin);

  FakeRendererAudioInputStreamFactoryClient fake_client_1;
  mojo::Receiver<blink::mojom::RendererAudioInputStreamFactoryClient>
      client_receiver_1(&fake_client_1);
  factory_remote->CreateStream(client_receiver_1.BindNewPipeAndPassRemote(),
                               session_id, kParams, kAGC, kSharedMemoryCount,
                               nullptr);

  FakeRendererAudioInputStreamFactoryClient fake_client_2;
  mojo::Receiver<blink::mojom::RendererAudioInputStreamFactoryClient>
      client_receiver_2(&fake_client_2);
  factory_remote->CreateStream(client_receiver_2.BindNewPipeAndPassRemote(),
                               session_id, kParams, kAGC, kSharedMemoryCount,
                               nullptr);

  ASSERT_TRUE(base::test::RunUntil([&]() {
    return audio_service_stream_factory_.input_observers.size() == 2u;
  }));
  EXPECT_TRUE(audio_service_stream_factory_.input_observers[0].is_connected());
  EXPECT_TRUE(audio_service_stream_factory_.input_observers[1].is_connected());

  audio_input_device_manager()->Close(session_id);
  audio_service_stream_factory_.input_observers[0].FlushForTesting();
  audio_service_stream_factory_.input_observers[1].FlushForTesting();

  EXPECT_FALSE(audio_service_stream_factory_.input_observers[0].is_connected());
  EXPECT_FALSE(audio_service_stream_factory_.input_observers[1].is_connected());
}

TEST_F(MAYBE_RenderFrameAudioInputStreamFactoryTest,
       CloseOneOfTwoSessionsOnSameDevice_ClosesOnlyMatchingStream) {
  mojo::Remote<blink::mojom::RendererAudioInputStreamFactory> factory_remote;
  RenderFrameAudioInputStreamFactory factory(
      factory_remote.BindNewPipeAndPassReceiver(), media_stream_manager_.get(),
      main_rfh());

  url::Origin origin = url::Origin::Create(GURL("https://test.com"));
  base::UnguessableToken session_1 = GenerateAudioStream(
      blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE, "", origin,
      /*page_request_id=*/1);
  base::UnguessableToken session_2 = GenerateAudioStream(
      blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE, "", origin,
      /*page_request_id=*/2,
      blink::mojom::StreamSelectionInfo::NewSearchBySessionId(
          blink::mojom::SearchBySessionId::New()));
  ASSERT_NE(session_1, session_2);

  FakeRendererAudioInputStreamFactoryClient fake_client_1;
  mojo::Receiver<blink::mojom::RendererAudioInputStreamFactoryClient>
      client_receiver_1(&fake_client_1);
  factory_remote->CreateStream(client_receiver_1.BindNewPipeAndPassRemote(),
                               session_1, kParams, kAGC, kSharedMemoryCount,
                               nullptr);

  FakeRendererAudioInputStreamFactoryClient fake_client_2;
  mojo::Receiver<blink::mojom::RendererAudioInputStreamFactoryClient>
      client_receiver_2(&fake_client_2);
  factory_remote->CreateStream(client_receiver_2.BindNewPipeAndPassRemote(),
                               session_2, kParams, kAGC, kSharedMemoryCount,
                               nullptr);

  ASSERT_TRUE(base::test::RunUntil([&]() {
    return audio_service_stream_factory_.input_observers.size() == 2u;
  }));
  EXPECT_TRUE(audio_service_stream_factory_.input_observers[0].is_connected());
  EXPECT_TRUE(audio_service_stream_factory_.input_observers[1].is_connected());

  audio_input_device_manager()->Close(session_1);
  audio_service_stream_factory_.input_observers[0].FlushForTesting();
  audio_service_stream_factory_.input_observers[1].FlushForTesting();

  EXPECT_FALSE(audio_service_stream_factory_.input_observers[0].is_connected());
  EXPECT_TRUE(audio_service_stream_factory_.input_observers[1].is_connected());
}

TEST_F(MAYBE_RenderFrameAudioInputStreamFactoryTest,
       CancelRequest_ClosesCreatedInputStream) {
  mojo::Remote<blink::mojom::RendererAudioInputStreamFactory> factory_remote;
  RenderFrameAudioInputStreamFactory factory(
      factory_remote.BindNewPipeAndPassReceiver(), media_stream_manager_.get(),
      main_rfh());

  url::Origin origin = url::Origin::Create(GURL("https://test.com"));
  base::UnguessableToken session_id = GenerateAudioStream(
      blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE, "", origin,
      /*page_request_id=*/1);

  FakeRendererAudioInputStreamFactoryClient fake_client;
  mojo::Receiver<blink::mojom::RendererAudioInputStreamFactoryClient>
      client_receiver(&fake_client);
  factory_remote->CreateStream(client_receiver.BindNewPipeAndPassRemote(),
                               session_id, kParams, kAGC, kSharedMemoryCount,
                               nullptr);

  ASSERT_TRUE(base::test::RunUntil([&]() {
    return audio_service_stream_factory_.input_observers.size() == 1u;
  }));
  EXPECT_TRUE(audio_service_stream_factory_.input_observers[0].is_connected());

  media_stream_manager_->CancelRequest(main_rfh()->GetGlobalId(),
                                       /*requester_id=*/1,
                                       /*page_request_id=*/1);
  audio_service_stream_factory_.input_observers[0].FlushForTesting();

  EXPECT_FALSE(audio_service_stream_factory_.input_observers[0].is_connected());
}

TEST_F(MAYBE_RenderFrameAudioInputStreamFactoryTest,
       StopStreamDevice_ClosesCreatedInputStream) {
  mojo::Remote<blink::mojom::RendererAudioInputStreamFactory> factory_remote;
  RenderFrameAudioInputStreamFactory factory(
      factory_remote.BindNewPipeAndPassReceiver(), media_stream_manager_.get(),
      main_rfh());

  url::Origin origin = url::Origin::Create(GURL("https://test.com"));
  blink::MediaStreamDevice audio_device = GenerateAudioStreamDevice(
      blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE, "", origin,
      /*page_request_id=*/1);

  FakeRendererAudioInputStreamFactoryClient fake_client;
  mojo::Receiver<blink::mojom::RendererAudioInputStreamFactoryClient>
      client_receiver(&fake_client);
  factory_remote->CreateStream(client_receiver.BindNewPipeAndPassRemote(),
                               audio_device.session_id(), kParams, kAGC,
                               kSharedMemoryCount, nullptr);

  ASSERT_TRUE(base::test::RunUntil([&]() {
    return audio_service_stream_factory_.input_observers.size() == 1u;
  }));
  EXPECT_TRUE(audio_service_stream_factory_.input_observers[0].is_connected());

  media_stream_manager_->StopStreamDevice(main_rfh()->GetGlobalId(),
                                          /*requester_id=*/1, audio_device.id,
                                          audio_device.session_id());
  audio_service_stream_factory_.input_observers[0].FlushForTesting();

  EXPECT_FALSE(audio_service_stream_factory_.input_observers[0].is_connected());
}

TEST_F(MAYBE_RenderFrameAudioInputStreamFactoryTest,
       CloseSession_ClosesCreatedWebContentsCaptureStream) {
  std::unique_ptr<WebContents> source_contents = CreateTestWebContents();
  mojo::Remote<blink::mojom::RendererAudioInputStreamFactory> factory_remote;
  RenderFrameAudioInputStreamFactory factory(
      factory_remote.BindNewPipeAndPassReceiver(), media_stream_manager_.get(),
      main_rfh());

  RenderFrameHost* main_frame = source_contents->GetPrimaryMainFrame();
  WebContentsMediaCaptureId capture_id(
      main_frame->GetProcess()->GetDeprecatedID(), main_frame->GetRoutingID());

  url::Origin origin = url::Origin::Create(GURL("https://test.com"));
  DesktopMediaID media_id(DesktopMediaID::TYPE_WEB_CONTENTS,
                          DesktopMediaID::kNullId, capture_id);
  std::string stream_id = DesktopStreamsRegistry::GetInstance()->RegisterStream(
      main_rfh()->GetProcess()->GetDeprecatedID(), main_rfh()->GetRoutingID(),
      origin, media_id, kRegistryStreamTypeTab);

  base::UnguessableToken session_id = GenerateAudioStream(
      blink::mojom::MediaStreamType::GUM_TAB_AUDIO_CAPTURE, stream_id, origin);

  FakeRendererAudioInputStreamFactoryClient fake_client;
  mojo::Receiver<blink::mojom::RendererAudioInputStreamFactoryClient>
      client_receiver(&fake_client);
  factory_remote->CreateStream(client_receiver.BindNewPipeAndPassRemote(),
                               session_id, kParams, kAGC, kSharedMemoryCount,
                               nullptr);

  ASSERT_TRUE(base::test::RunUntil([&]() {
    return audio_service_stream_factory_.loopback_observers.size() == 1u;
  }));
  EXPECT_TRUE(
      audio_service_stream_factory_.loopback_observers[0].is_connected());

  audio_input_device_manager()->Close(session_id);
  audio_service_stream_factory_.loopback_observers[0].FlushForTesting();

  EXPECT_FALSE(
      audio_service_stream_factory_.loopback_observers[0].is_connected());
}

TEST_F(MAYBE_RenderFrameAudioInputStreamFactoryTest,
       CloseSessionBeforeWebContentsCaptureCreated_Fails) {
  std::unique_ptr<WebContents> source_contents = CreateTestWebContents();
  mojo::Remote<blink::mojom::RendererAudioInputStreamFactory> factory_remote;
  RenderFrameAudioInputStreamFactory factory(
      factory_remote.BindNewPipeAndPassReceiver(), media_stream_manager_.get(),
      main_rfh());

  RenderFrameHost* main_frame = source_contents->GetPrimaryMainFrame();
  WebContentsMediaCaptureId capture_id(
      main_frame->GetProcess()->GetDeprecatedID(), main_frame->GetRoutingID());

  url::Origin origin = url::Origin::Create(GURL("https://test.com"));
  DesktopMediaID media_id(DesktopMediaID::TYPE_WEB_CONTENTS,
                          DesktopMediaID::kNullId, capture_id);
  std::string stream_id = DesktopStreamsRegistry::GetInstance()->RegisterStream(
      main_rfh()->GetProcess()->GetDeprecatedID(), main_rfh()->GetRoutingID(),
      origin, media_id, kRegistryStreamTypeTab);

  base::UnguessableToken session_id = GenerateAudioStream(
      blink::mojom::MediaStreamType::GUM_TAB_AUDIO_CAPTURE, stream_id, origin);

  // Ensure the factory receiver is bound before sending CreateStream.
  factory_remote.FlushForTesting();

  FakeRendererAudioInputStreamFactoryClient fake_client;
  mojo::Receiver<blink::mojom::RendererAudioInputStreamFactoryClient>
      client_receiver(&fake_client);
  factory_remote->CreateStream(client_receiver.BindNewPipeAndPassRemote(),
                               session_id, kParams, kAGC, kSharedMemoryCount,
                               nullptr);
  bool client_disconnected = false;
  client_receiver.set_disconnect_handler(
      base::BindLambdaForTesting([&]() { client_disconnected = true; }));

  // Schedule Close() so that it executes after CreateStream() dispatches the
  // UI-thread loopback lookup, but before the reply runs CreateLoopbackStream.
  GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&AudioInputDeviceManager::Close,
                                base::Unretained(audio_input_device_manager()),
                                session_id));
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return client_disconnected ||
           !!audio_service_stream_factory_.last_created_loopback_callback;
  }));

  EXPECT_TRUE(client_disconnected);
  EXPECT_FALSE(!!audio_service_stream_factory_.last_created_loopback_callback);
}

}  // namespace content
