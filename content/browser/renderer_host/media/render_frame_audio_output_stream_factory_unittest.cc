// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/render_frame_audio_output_stream_factory.h"

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/task/post_task.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "base/unguessable_token.h"
#include "content/browser/media/forwarding_audio_stream_factory.h"
#include "content/browser/renderer_host/media/media_stream_manager.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/test_service_manager_context.h"
#include "media/audio/audio_system_impl.h"
#include "media/audio/fake_audio_log_factory.h"
#include "media/audio/fake_audio_manager.h"
#include "media/audio/test_audio_thread.h"
#include "media/base/audio_parameters.h"
#include "media/mojo/mojom/audio_output_stream.mojom.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/audio/public/cpp/fake_stream_factory.h"
#include "services/audio/public/mojom/constants.mojom.h"
#include "services/audio/public/mojom/stream_factory.mojom.h"
#include "services/service_manager/public/cpp/connector.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Test;
using ::testing::Mock;
using ::testing::Ne;
using ::testing::StrictMock;

namespace content {

class RenderFrameAudioOutputStreamFactoryTest
    : public RenderViewHostTestHarness {
 public:
  RenderFrameAudioOutputStreamFactoryTest()
      : test_service_manager_context_(
            std::make_unique<TestServiceManagerContext>()),
        audio_manager_(std::make_unique<media::TestAudioThread>(),
                       &log_factory_),
        audio_system_(media::AudioSystemImpl::CreateInstance()),
        media_stream_manager_(std::make_unique<MediaStreamManager>(
            audio_system_.get(),
            base::CreateSingleThreadTaskRunner({BrowserThread::UI}))) {}

  ~RenderFrameAudioOutputStreamFactoryTest() override {}

  void SetUp() override {
    RenderViewHostTestHarness::SetUp();
    RenderFrameHostTester::For(main_rfh())->InitializeRenderFrameIfNeeded();

    // Set up the ForwardingAudioStreamFactory.
    service_manager::Connector* connector =
        ForwardingAudioStreamFactory::ForFrame(main_rfh())
            ->core()
            ->get_connector_for_testing();
    connector->OverrideBinderForTesting(
        service_manager::ServiceFilter::ByName(audio::mojom::kServiceName),
        audio::mojom::StreamFactory::Name_,
        base::BindRepeating(
            &RenderFrameAudioOutputStreamFactoryTest::BindFactory,
            base::Unretained(this)));

    base::RunLoop().RunUntilIdle();
  }

  void TearDown() override {
    audio_manager_.Shutdown();
    test_service_manager_context_.reset();
    RenderViewHostTestHarness::TearDown();
  }

  void BindFactory(mojo::ScopedMessagePipeHandle factory_receiver) {
    audio_service_stream_factory_.receiver_.Bind(
        mojo::PendingReceiver<audio::mojom::StreamFactory>(
            std::move(factory_receiver)));
  }

  class MockStreamFactory : public audio::FakeStreamFactory {
   public:
    MockStreamFactory() {}
    ~MockStreamFactory() override {}

    void CreateOutputStream(
        mojo::PendingReceiver<media::mojom::AudioOutputStream> stream_receiver,
        mojo::PendingAssociatedRemote<media::mojom::AudioOutputStreamObserver>
            observer,
        mojo::PendingRemote<media::mojom::AudioLog> log,
        const std::string& output_device_id,
        const media::AudioParameters& params,
        const base::UnguessableToken& group_id,
        const base::Optional<base::UnguessableToken>& processing_id,
        CreateOutputStreamCallback created_callback) override {
      last_created_callback = std::move(created_callback);
    }

    CreateOutputStreamCallback last_created_callback;
  };

  using MockAuthorizationCallback = StrictMock<
      base::MockCallback<base::OnceCallback<void(media::OutputDeviceStatus,
                                                 const media::AudioParameters&,
                                                 const std::string&)>>>;

  const char* kDefaultDeviceId = "default";
  const char* kDeviceId =
      "111122223333444455556666777788889999aaaabbbbccccddddeeeeffff";
  const media::AudioParameters kParams =
      media::AudioParameters::UnavailableDeviceParams();
  MockStreamFactory audio_service_stream_factory_;
  std::unique_ptr<TestServiceManagerContext> test_service_manager_context_;
  media::FakeAudioLogFactory log_factory_;
  media::FakeAudioManager audio_manager_;
  std::unique_ptr<media::AudioSystem> audio_system_;
  std::unique_ptr<MediaStreamManager> media_stream_manager_;
};

TEST_F(RenderFrameAudioOutputStreamFactoryTest, ConstructDestruct) {
  mojo::Remote<mojom::RendererAudioOutputStreamFactory> factory_remote;
  RenderFrameAudioOutputStreamFactory factory(
      main_rfh(), audio_system_.get(), media_stream_manager_.get(),
      factory_remote.BindNewPipeAndPassReceiver());
}

TEST_F(RenderFrameAudioOutputStreamFactoryTest,
       RequestDeviceAuthorizationForDefaultDevice_StatusOk) {
  mojo::Remote<mojom::RendererAudioOutputStreamFactory> factory_remote;
  RenderFrameAudioOutputStreamFactory factory(
      main_rfh(), audio_system_.get(), media_stream_manager_.get(),
      factory_remote.BindNewPipeAndPassReceiver());

  mojo::Remote<media::mojom::AudioOutputStreamProvider> provider_remote;
  MockAuthorizationCallback mock_callback;
  factory_remote->RequestDeviceAuthorization(
      provider_remote.BindNewPipeAndPassReceiver(), base::nullopt,
      kDefaultDeviceId, mock_callback.Get());

  EXPECT_CALL(mock_callback,
              Run(media::OUTPUT_DEVICE_STATUS_OK, _, std::string()));

  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(1u, factory.CurrentNumberOfProvidersForTesting());
}

TEST_F(
    RenderFrameAudioOutputStreamFactoryTest,
    RequestDeviceAuthorizationForDefaultDeviceAndDestroyProviderPtr_CleansUp) {
  mojo::Remote<mojom::RendererAudioOutputStreamFactory> factory_remote;
  RenderFrameAudioOutputStreamFactory factory(
      main_rfh(), audio_system_.get(), media_stream_manager_.get(),
      factory_remote.BindNewPipeAndPassReceiver());

  mojo::Remote<media::mojom::AudioOutputStreamProvider> provider_remote;
  MockAuthorizationCallback mock_callback;
  factory_remote->RequestDeviceAuthorization(
      provider_remote.BindNewPipeAndPassReceiver(), base::nullopt,
      kDefaultDeviceId, mock_callback.Get());
  provider_remote.reset();

  EXPECT_CALL(mock_callback,
              Run(media::OUTPUT_DEVICE_STATUS_OK, _, std::string()));

  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(0u, factory.CurrentNumberOfProvidersForTesting());
}

TEST_F(
    RenderFrameAudioOutputStreamFactoryTest,
    RequestDeviceAuthorizationForNondefaultDeviceWithoutAuthorization_Fails) {
  mojo::Remote<mojom::RendererAudioOutputStreamFactory> factory_remote;
  RenderFrameAudioOutputStreamFactory factory(
      main_rfh(), audio_system_.get(), media_stream_manager_.get(),
      factory_remote.BindNewPipeAndPassReceiver());

  mojo::Remote<media::mojom::AudioOutputStreamProvider> provider_remote;
  MockAuthorizationCallback mock_callback;
  factory_remote->RequestDeviceAuthorization(
      provider_remote.BindNewPipeAndPassReceiver(), base::nullopt, kDeviceId,
      mock_callback.Get());

  EXPECT_CALL(mock_callback,
              Run(Ne(media::OUTPUT_DEVICE_STATUS_OK), _, std::string()));

  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(0u, factory.CurrentNumberOfProvidersForTesting());
}

TEST_F(RenderFrameAudioOutputStreamFactoryTest,
       CreateStream_CreatesStreamAndFreesProvider) {
  mojo::Remote<mojom::RendererAudioOutputStreamFactory> factory_remote;
  RenderFrameAudioOutputStreamFactory factory(
      main_rfh(), audio_system_.get(), media_stream_manager_.get(),
      factory_remote.BindNewPipeAndPassReceiver());

  mojo::Remote<media::mojom::AudioOutputStreamProvider> provider_remote;
  MockAuthorizationCallback mock_callback;
  factory_remote->RequestDeviceAuthorization(
      provider_remote.BindNewPipeAndPassReceiver(), base::nullopt,
      kDefaultDeviceId, mock_callback.Get());
  {
    mojo::PendingRemote<media::mojom::AudioOutputStreamProviderClient> client;
    ignore_result(client.InitWithNewPipeAndPassReceiver());
    provider_remote->Acquire(kParams, std::move(client), base::nullopt);
  }

  audio::mojom::StreamFactory::CreateOutputStreamCallback created_callback;
  EXPECT_CALL(mock_callback,
              Run(media::OUTPUT_DEVICE_STATUS_OK, _, std::string()));

  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(!!audio_service_stream_factory_.last_created_callback);
  EXPECT_EQ(0u, factory.CurrentNumberOfProvidersForTesting());
}

TEST_F(RenderFrameAudioOutputStreamFactoryTest,
       CreateStreamAfterFactoryDestruction_Fails) {
  mojo::Remote<media::mojom::AudioOutputStreamProvider> provider_remote;
  MockAuthorizationCallback mock_callback;

  {
    mojo::Remote<mojom::RendererAudioOutputStreamFactory> factory_remote;
    RenderFrameAudioOutputStreamFactory factory(
        main_rfh(), audio_system_.get(), media_stream_manager_.get(),
        factory_remote.BindNewPipeAndPassReceiver());

    factory_remote->RequestDeviceAuthorization(
        provider_remote.BindNewPipeAndPassReceiver(), base::nullopt,
        kDefaultDeviceId, mock_callback.Get());

    audio::mojom::StreamFactory::CreateOutputStreamCallback created_callback;
    EXPECT_CALL(mock_callback,
                Run(media::OUTPUT_DEVICE_STATUS_OK, _, std::string()));
    base::RunLoop().RunUntilIdle();
  }
  // Now factory is destructed. Trying to create a stream should fail.
  {
    mojo::PendingRemote<media::mojom::AudioOutputStreamProviderClient> client;
    ignore_result(client.InitWithNewPipeAndPassReceiver());
    provider_remote->Acquire(kParams, std::move(client), base::nullopt);
  }

  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(!!audio_service_stream_factory_.last_created_callback);
}

}  // namespace content
