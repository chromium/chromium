// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/render_frame_audio_output_stream_factory.h"

#include <memory>
#include <string>
#include <tuple>
#include <utility>

#include "base/dcheck_is_on.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/unguessable_token.h"
#include "build/build_config.h"
#include "content/browser/media/forwarding_audio_stream_factory.h"
#include "content/browser/preloading/prerender/prerender_final_status.h"
#include "content/browser/renderer_host/media/audio_output_authorization_handler.h"
#include "content/browser/renderer_host/media/media_stream_manager.h"
#include "content/browser/renderer_host/media/mock_preferred_audio_output_device_manager.h"
#include "content/browser/renderer_host/media/preferred_audio_output_device_manager.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/prerender_test_util.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "media/audio/audio_system_impl.h"
#include "media/audio/fake_audio_log_factory.h"
#include "media/audio/fake_audio_manager.h"
#include "media/audio/test_audio_thread.h"
#include "media/base/audio_parameters.h"
#include "media/mojo/mojom/audio_output_stream.mojom.h"
#include "media/mojo/mojom/audio_stream_factory.mojom.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/audio/public/cpp/fake_stream_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

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
      : audio_manager_(std::make_unique<media::TestAudioThread>(),
                       &log_factory_),
        audio_system_(media::AudioSystemImpl::CreateInstance()),
        media_stream_manager_(
            std::make_unique<MediaStreamManager>(audio_system_.get())) {}

  ~RenderFrameAudioOutputStreamFactoryTest() override {}

  void SetUp() override {
    RenderViewHostTestHarness::SetUp();
    RenderFrameHostTester::For(main_rfh())->InitializeRenderFrameIfNeeded();

    // Set up the ForwardingAudioStreamFactory.
    ForwardingAudioStreamFactory::OverrideAudioStreamFactoryBinderForTesting(
        base::BindRepeating(
            &RenderFrameAudioOutputStreamFactoryTest::BindFactory,
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

  // Requests authorization of the default device through `factory_remote` and
  // waits for it to succeed. Returns the remote of the authorized provider.
  mojo::Remote<media::mojom::AudioOutputStreamProvider> AuthorizeDefaultDevice(
      mojo::Remote<blink::mojom::RendererAudioOutputStreamFactory>&
          factory_remote) {
    mojo::Remote<media::mojom::AudioOutputStreamProvider> provider_remote;
    base::test::TestFuture<media::OutputDeviceStatus,
                           const media::AudioParameters&, const std::string&>
        auth_future;
    factory_remote->RequestDeviceAuthorization(
        provider_remote.BindNewPipeAndPassReceiver(), std::nullopt,
        kDefaultDeviceId, auth_future.GetCallback());
    EXPECT_EQ(auth_future.Get<0>(), media::OUTPUT_DEVICE_STATUS_OK);
    return provider_remote;
  }

  void FlushTaskRunner(scoped_refptr<base::SequencedTaskRunner> task_runner) {
    base::test::TestFuture<void> future;
    task_runner->PostTaskAndReply(FROM_HERE, base::DoNothing(),
                                  future.GetCallback());
    EXPECT_TRUE(future.Wait());
  }

  // Calls Acquire() on `provider_remote` and waits until the request has been
  // handled, including any UI thread task it triggers (e.g. the prerender
  // cancellation).
  void AcquireAndWait(
      mojo::Remote<media::mojom::AudioOutputStreamProvider>& provider_remote) {
    mojo::PendingRemote<media::mojom::AudioOutputStreamProviderClient> client;
    std::ignore = client.InitWithNewPipeAndPassReceiver();
    provider_remote->Acquire(kParams, std::move(client));
    provider_remote.FlushForTesting();
    FlushTaskRunner(GetUIThreadTaskRunner({}));
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
        CreateOutputStreamCallback created_callback) override {
      last_created_callback_ = std::move(created_callback);
    }

    void CreateSwitchableOutputStream(
        mojo::PendingReceiver<media::mojom::AudioOutputStream> stream,
        mojo::PendingReceiver<media::mojom::DeviceSwitchInterface>
            device_switch_receiver,
        mojo::PendingAssociatedRemote<media::mojom::AudioOutputStreamObserver>
            observer,
        mojo::PendingRemote<media::mojom::AudioLog> log,
        const std::string& device_id,
        const media::AudioParameters& params,
        const base::UnguessableToken& group_id,
        CreateOutputStreamCallback created_callback) override {
      last_created_callback_ = std::move(created_callback);
    }

    bool last_created_callback() const {
      return !last_created_callback_.is_null();
    }

   private:
    CreateOutputStreamCallback last_created_callback_;
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
  media::FakeAudioLogFactory log_factory_;
  media::FakeAudioManager audio_manager_;
  std::unique_ptr<media::AudioSystem> audio_system_;
  std::unique_ptr<MediaStreamManager> media_stream_manager_;
};

TEST_F(RenderFrameAudioOutputStreamFactoryTest, ConstructDestruct) {
  mojo::Remote<blink::mojom::RendererAudioOutputStreamFactory> factory_remote;
  RenderFrameAudioOutputStreamFactory factory(
      main_rfh(), audio_system_.get(), media_stream_manager_.get(),
      factory_remote.BindNewPipeAndPassReceiver());
}

TEST_F(RenderFrameAudioOutputStreamFactoryTest,
       RequestDeviceAuthorizationForDefaultDevice_StatusOk) {
  mojo::Remote<blink::mojom::RendererAudioOutputStreamFactory> factory_remote;
  RenderFrameAudioOutputStreamFactory factory(
      main_rfh(), audio_system_.get(), media_stream_manager_.get(),
      factory_remote.BindNewPipeAndPassReceiver());

  mojo::Remote<media::mojom::AudioOutputStreamProvider> provider_remote;
  MockAuthorizationCallback mock_callback;
  factory_remote->RequestDeviceAuthorization(
      provider_remote.BindNewPipeAndPassReceiver(), std::nullopt,
      kDefaultDeviceId, mock_callback.Get());

  EXPECT_CALL(mock_callback,
              Run(media::OUTPUT_DEVICE_STATUS_OK, _, std::string()));

  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(1u, factory.CurrentNumberOfProvidersForTesting());
}

TEST_F(
    RenderFrameAudioOutputStreamFactoryTest,
    RequestDeviceAuthorizationForDefaultDeviceAndDestroyProviderPtr_CleansUp) {
  mojo::Remote<blink::mojom::RendererAudioOutputStreamFactory> factory_remote;
  RenderFrameAudioOutputStreamFactory factory(
      main_rfh(), audio_system_.get(), media_stream_manager_.get(),
      factory_remote.BindNewPipeAndPassReceiver());

  mojo::Remote<media::mojom::AudioOutputStreamProvider> provider_remote;
  MockAuthorizationCallback mock_callback;
  factory_remote->RequestDeviceAuthorization(
      provider_remote.BindNewPipeAndPassReceiver(), std::nullopt,
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
  mojo::Remote<blink::mojom::RendererAudioOutputStreamFactory> factory_remote;
  RenderFrameAudioOutputStreamFactory factory(
      main_rfh(), audio_system_.get(), media_stream_manager_.get(),
      factory_remote.BindNewPipeAndPassReceiver());

  mojo::Remote<media::mojom::AudioOutputStreamProvider> provider_remote;
  MockAuthorizationCallback mock_callback;
  factory_remote->RequestDeviceAuthorization(
      provider_remote.BindNewPipeAndPassReceiver(), std::nullopt, kDeviceId,
      mock_callback.Get());

  EXPECT_CALL(mock_callback,
              Run(Ne(media::OUTPUT_DEVICE_STATUS_OK), _, std::string()));

  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(0u, factory.CurrentNumberOfProvidersForTesting());
}

TEST_F(RenderFrameAudioOutputStreamFactoryTest,
       CreateStream_CreatesStreamAndFreesProvider) {
  mojo::Remote<blink::mojom::RendererAudioOutputStreamFactory> factory_remote;
  RenderFrameAudioOutputStreamFactory factory(
      main_rfh(), audio_system_.get(), media_stream_manager_.get(),
      factory_remote.BindNewPipeAndPassReceiver());

  mojo::Remote<media::mojom::AudioOutputStreamProvider> provider_remote;
  MockAuthorizationCallback mock_callback;
  factory_remote->RequestDeviceAuthorization(
      provider_remote.BindNewPipeAndPassReceiver(), std::nullopt,
      kDefaultDeviceId, mock_callback.Get());
  {
    mojo::PendingRemote<media::mojom::AudioOutputStreamProviderClient> client;
    std::ignore = client.InitWithNewPipeAndPassReceiver();
    provider_remote->Acquire(kParams, std::move(client));
  }

  media::mojom::AudioStreamFactory::CreateOutputStreamCallback created_callback;
  EXPECT_CALL(mock_callback,
              Run(media::OUTPUT_DEVICE_STATUS_OK, _, std::string()));

  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(!!audio_service_stream_factory_.last_created_callback());
  EXPECT_EQ(0u, factory.CurrentNumberOfProvidersForTesting());
}

TEST_F(RenderFrameAudioOutputStreamFactoryTest,
       CreateStreamAfterFactoryDestruction_Fails) {
  mojo::Remote<media::mojom::AudioOutputStreamProvider> provider_remote;
  MockAuthorizationCallback mock_callback;

  {
    mojo::Remote<blink::mojom::RendererAudioOutputStreamFactory> factory_remote;
    RenderFrameAudioOutputStreamFactory factory(
        main_rfh(), audio_system_.get(), media_stream_manager_.get(),
        factory_remote.BindNewPipeAndPassReceiver());

    factory_remote->RequestDeviceAuthorization(
        provider_remote.BindNewPipeAndPassReceiver(), std::nullopt,
        kDefaultDeviceId, mock_callback.Get());

    media::mojom::AudioStreamFactory::CreateOutputStreamCallback
        created_callback;
    EXPECT_CALL(mock_callback,
                Run(media::OUTPUT_DEVICE_STATUS_OK, _, std::string()));
    base::RunLoop().RunUntilIdle();
  }
  // Now factory is destructed. Trying to create a stream should fail.
  {
    mojo::PendingRemote<media::mojom::AudioOutputStreamProviderClient> client;
    std::ignore = client.InitWithNewPipeAndPassReceiver();
    provider_remote->Acquire(kParams, std::move(client));
  }

  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(!!audio_service_stream_factory_.last_created_callback());
}

namespace {

class ScopedDisallowAudioOutputFeatureList {
 protected:
  explicit ScopedDisallowAudioOutputFeatureList(bool enabled) {
    feature_list_.InitWithFeatureState(kDisallowAudioOutputDuringPrerendering,
                                       enabled);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

}  // namespace

class RenderFrameAudioOutputStreamFactoryPrerenderTest
    : private test::ScopedPrerenderFeatureList,
      private ScopedDisallowAudioOutputFeatureList,
      public RenderFrameAudioOutputStreamFactoryTest {
 public:
  explicit RenderFrameAudioOutputStreamFactoryPrerenderTest(
      bool enable_feature = true)
      : ScopedDisallowAudioOutputFeatureList(enable_feature) {}

  void SetUp() override {
    RenderFrameAudioOutputStreamFactoryTest::SetUp();
    web_contents_delegate_ =
        std::make_unique<test::ScopedPrerenderWebContentsDelegate>(
            *web_contents());
    NavigateAndCommit(GURL("https://example.com"));
  }

 protected:
  RenderFrameHost* StartPrerender() {
    RenderFrameHost* prerender_rfh =
        WebContentsTester::For(web_contents())
            ->AddPrerenderAndCommitNavigation(kPrerenderUrl);
    EXPECT_TRUE(prerender_rfh);
    EXPECT_TRUE(prerender_rfh->IsInLifecycleState(
        RenderFrameHost::LifecycleState::kPrerendering));
    return prerender_rfh;
  }

  std::unique_ptr<RenderFrameAudioOutputStreamFactory> CreateFactory(
      RenderFrameHost* rfh) {
    return std::make_unique<RenderFrameAudioOutputStreamFactory>(
        rfh, audio_system_.get(), media_stream_manager_.get(),
        factory_remote_.BindNewPipeAndPassReceiver());
  }

  const GURL kPrerenderUrl{"https://example.com/prerender"};
  mojo::Remote<blink::mojom::RendererAudioOutputStreamFactory> factory_remote_;

 private:
  std::unique_ptr<test::ScopedPrerenderWebContentsDelegate>
      web_contents_delegate_;
};

class RenderFrameAudioOutputStreamFactoryPrerenderFeatureDisabledTest
    : public RenderFrameAudioOutputStreamFactoryPrerenderTest {
 public:
  RenderFrameAudioOutputStreamFactoryPrerenderFeatureDisabledTest()
      : RenderFrameAudioOutputStreamFactoryPrerenderTest(
            /*enable_feature=*/false) {}
};

TEST_F(RenderFrameAudioOutputStreamFactoryPrerenderTest,
       CreateStreamWhilePrerendering_CancelsPrerendering) {
  base::HistogramTester histogram_tester;
  RenderFrameHost* prerender_rfh = StartPrerender();
  ASSERT_TRUE(prerender_rfh);
  test::PrerenderHostObserver prerender_observer(*web_contents(),
                                                 kPrerenderUrl);
  auto factory = CreateFactory(prerender_rfh);

  // Default device authorization (for querying AudioParameters during `new
  // AudioContext()`) is allowed while prerendering and does not cancel it.
  auto provider_remote = AuthorizeDefaultDevice(factory_remote_);
  EXPECT_EQ(1u, factory->CurrentNumberOfProvidersForTesting());
  EXPECT_TRUE(prerender_rfh->IsInLifecycleState(
      RenderFrameHost::LifecycleState::kPrerendering));

  // Acquiring an actual audio output stream while prerendering must be blocked
  // and must cancel the prerender.
  AcquireAndWait(provider_remote);
  prerender_observer.WaitForDestroyed();
  EXPECT_FALSE(prerender_observer.was_activated());
  EXPECT_FALSE(audio_service_stream_factory_.last_created_callback());
  EXPECT_EQ(0u, factory->CurrentNumberOfProvidersForTesting());
  histogram_tester.ExpectUniqueSample(
      "Prerender.Experimental.PrerenderHostFinalStatus.SpeculationRule",
      PrerenderFinalStatus::kAudioOutput, 1);
}

TEST_F(RenderFrameAudioOutputStreamFactoryPrerenderTest,
       CreateStreamAfterPrerenderActivation_Succeeds) {
  RenderFrameHost* prerender_rfh = StartPrerender();
  ASSERT_TRUE(prerender_rfh);
  auto factory = CreateFactory(prerender_rfh);

  // Authorize a provider while prerendering, then activate the page and verify
  // that acquiring the stream after activation succeeds.
  auto provider_remote = AuthorizeDefaultDevice(factory_remote_);
  EXPECT_EQ(1u, factory->CurrentNumberOfProvidersForTesting());

  WebContentsTester::For(web_contents())
      ->ActivatePrerenderedPage(kPrerenderUrl);
  factory->ReleaseRestriction();
  ASSERT_TRUE(prerender_rfh->IsActive());

  AcquireAndWait(provider_remote);
  audio_service_stream_factory_.receiver_.FlushForTesting();

  EXPECT_TRUE(audio_service_stream_factory_.last_created_callback());
  EXPECT_EQ(0u, factory->CurrentNumberOfProvidersForTesting());
}

// Simulates the renderer's first Acquire() after activation reaching the IO
// thread before the ReleaseRestriction() notification. Prerendering can no
// longer be cancelled, so the deferred request must be created once
// ReleaseRestriction() arrives instead of being dropped.
TEST_F(RenderFrameAudioOutputStreamFactoryPrerenderTest,
       CreateStreamRacingWithActivation_CreatedOnReleaseRestriction) {
  RenderFrameHost* prerender_rfh = StartPrerender();
  ASSERT_TRUE(prerender_rfh);
  auto factory = CreateFactory(prerender_rfh);
  auto provider_remote = AuthorizeDefaultDevice(factory_remote_);

  // Activate the page without notifying `factory` yet.
  WebContentsTester::For(web_contents())
      ->ActivatePrerenderedPage(kPrerenderUrl);
  ASSERT_TRUE(prerender_rfh->IsActive());

  // The request is deferred (not dropped), and canceling the already activated
  // page is a no-op.
  AcquireAndWait(provider_remote);
  EXPECT_FALSE(audio_service_stream_factory_.last_created_callback());
  EXPECT_EQ(0u, factory->CurrentNumberOfProvidersForTesting());
  EXPECT_TRUE(prerender_rfh->IsActive());

  // The deferred stream is created once the restriction is released on the IO
  // thread.
  factory->ReleaseRestriction();
  FlushTaskRunner(GetIOThreadTaskRunner({}));
  audio_service_stream_factory_.receiver_.FlushForTesting();
  EXPECT_TRUE(audio_service_stream_factory_.last_created_callback());
}

// On macOS, RenderFrameHostImpl::GetAudibleMediaStreamType() returns
// kPlayingAudioStream, so creating a stream while prerendering with the kill
// switch disabled triggers the pre-existing
// DCHECK_NE(lifecycle_state(), LifecycleStateImpl::kPrerendering) in
// RenderFrameHostImpl::OnMediaStreamAdded.
#if BUILDFLAG(IS_MAC) && DCHECK_IS_ON()
#define MAYBE_CreateStreamWhilePrerendering_FeatureDisabled \
  DISABLED_CreateStreamWhilePrerendering_FeatureDisabled
#else
#define MAYBE_CreateStreamWhilePrerendering_FeatureDisabled \
  CreateStreamWhilePrerendering_FeatureDisabled
#endif
TEST_F(RenderFrameAudioOutputStreamFactoryPrerenderFeatureDisabledTest,
       MAYBE_CreateStreamWhilePrerendering_FeatureDisabled) {
  RenderFrameHost* prerender_rfh = StartPrerender();
  ASSERT_TRUE(prerender_rfh);
  auto factory = CreateFactory(prerender_rfh);

  auto provider_remote = AuthorizeDefaultDevice(factory_remote_);
  EXPECT_EQ(1u, factory->CurrentNumberOfProvidersForTesting());

  // With the kill switch disabled, acquiring a stream while prerendering is not
  // blocked by the active frame check.
  AcquireAndWait(provider_remote);
  audio_service_stream_factory_.receiver_.FlushForTesting();

  EXPECT_TRUE(audio_service_stream_factory_.last_created_callback());
  EXPECT_EQ(0u, factory->CurrentNumberOfProvidersForTesting());
}

}  // namespace content
