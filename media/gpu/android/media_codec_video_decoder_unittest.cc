// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/android/media_codec_video_decoder.h"

#include "base/android/jni_android.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/run_loop.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "gpu/command_buffer/service/mock_texture_owner.h"
#include "gpu/config/gpu_preferences.h"
#include "media/base/android/media_codec_util.h"
#include "media/base/android/mock_android_overlay.h"
#include "media/base/android/mock_media_crypto_context.h"
#include "media/base/decoder_buffer.h"
#include "media/base/media_util.h"
#include "media/base/test_helpers.h"
#include "media/base/video_frame.h"
#include "media/gpu/android/android_video_surface_chooser_impl.h"
#include "media/gpu/android/fake_codec_allocator.h"
#include "media/gpu/android/mock_android_video_surface_chooser.h"
#include "media/gpu/android/mock_device_info.h"
#include "media/gpu/android/video_frame_factory.h"
#include "media/video/supported_video_decoder_config.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::test::RunCallback;
using testing::_;
using testing::InvokeWithoutArgs;
using testing::NiceMock;
using testing::NotNull;
using testing::Return;
using testing::ReturnRef;
using testing::SaveArg;

namespace media {
namespace {

void OutputCb(scoped_refptr<VideoFrame>* output,
              scoped_refptr<VideoFrame> frame) {
  *output = std::move(frame);
}

std::unique_ptr<AndroidOverlay> CreateAndroidOverlayCb(
    const base::UnguessableToken&,
    AndroidOverlayConfig) {
  return nullptr;
}

// Make MCVD's destruction observable for teardown tests.
struct DestructionObservableMCVD : public DestructionObservable,
                                   public MediaCodecVideoDecoder {
  using MediaCodecVideoDecoder::MediaCodecVideoDecoder;
};

}  // namespace

class MockVideoFrameFactory : public VideoFrameFactory {
 public:
  MOCK_METHOD2(Initialize, void(OverlayMode overlay_mode, InitCb init_cb));
  MOCK_METHOD1(MockSetSurfaceBundle, void(scoped_refptr<CodecSurfaceBundle>));
  MOCK_METHOD5(
      MockCreateVideoFrame,
      void(CodecOutputBuffer* raw_output_buffer,
           scoped_refptr<gpu::TextureOwner> texture_owner,
           base::TimeDelta timestamp,
           gfx::Size natural_size,
           PromotionHintAggregator::NotifyPromotionHintCB promotion_hint_cb));
  MOCK_METHOD1(MockRunAfterPendingVideoFrames,
               void(base::OnceClosure* closure));
  MOCK_METHOD0(CancelPendingCallbacks, void());

  void SetSurfaceBundle(
      scoped_refptr<CodecSurfaceBundle> surface_bundle) override {
    MockSetSurfaceBundle(surface_bundle);
    if (!surface_bundle) {
      texture_owner_ = nullptr;
    } else {
      texture_owner_ = surface_bundle->overlay()
                           ? nullptr
                           : surface_bundle->codec_buffer_wait_coordinator()
                                 ->texture_owner();
    }
  }

  void CreateVideoFrame(
      std::unique_ptr<CodecOutputBuffer> output_buffer,
      base::TimeDelta timestamp,
      gfx::Size natural_size,
      PromotionHintAggregator::NotifyPromotionHintCB promotion_hint_cb,
      VideoFrameFactory::OnceOutputCb output_cb) override {
    MockCreateVideoFrame(output_buffer.get(), texture_owner_, timestamp,
                         natural_size, promotion_hint_cb);
    last_output_buffer_ = std::move(output_buffer);
    std::move(output_cb).Run(VideoFrame::CreateBlackFrame(gfx::Size(10, 10)));
  }

  void RunAfterPendingVideoFrames(base::OnceClosure closure) override {
    last_closure_ = std::move(closure);
    MockRunAfterPendingVideoFrames(&last_closure_);
  }

  std::unique_ptr<CodecOutputBuffer> last_output_buffer_;
  scoped_refptr<gpu::TextureOwner> texture_owner_;
  base::OnceClosure last_closure_;
};

class MediaCodecVideoDecoderTest : public testing::TestWithParam<VideoCodec> {
 public:
  MediaCodecVideoDecoderTest() : codec_(GetParam()) {}

  void SetUp() override {
    uint8_t data = 0;
    fake_decoder_buffer_ = DecoderBuffer::CopyFrom(&data, 1);
    codec_allocator_ = std::make_unique<FakeCodecAllocator>(
        base::ThreadTaskRunnerHandle::Get());
    device_info_ = std::make_unique<NiceMock<MockDeviceInfo>>();
  }

  void TearDown() override {
    // For VP8, make MCVD skip the drain by resetting it.  Otherwise, it's hard
    // to finish the drain.
    if (mcvd_ && codec_ == kCodecVP8 && codec_allocator_->most_recent_codec)
      DoReset();

    // MCVD calls DeleteSoon() on itself, so we have to run a RunLoop.
    mcvd_.reset();
    base::RunLoop().RunUntilIdle();
  }

  void CreateMcvd() {
    auto surface_chooser =
        std::make_unique<NiceMock<MockAndroidVideoSurfaceChooser>>();
    surface_chooser_ = surface_chooser.get();

    auto texture_owner = base::MakeRefCounted<NiceMock<gpu::MockTextureOwner>>(
        0, nullptr, nullptr);
    texture_owner_ = texture_owner.get();

    auto video_frame_factory =
        std::make_unique<NiceMock<MockVideoFrameFactory>>();
    video_frame_factory_ = video_frame_factory.get();
    // Set up VFF to pass |texture_owner_| via its InitCb.
    ON_CALL(*video_frame_factory_, Initialize(ExpectedOverlayMode(), _))
        .WillByDefault(RunCallback<1>(texture_owner));

    auto* observable_mcvd = new DestructionObservableMCVD(
        gpu_preferences_, gpu_feature_info_, std::make_unique<NullMediaLog>(),
        device_info_.get(), codec_allocator_.get(), std::move(surface_chooser),
        base::BindRepeating(&CreateAndroidOverlayCb),
        base::Bind(&MediaCodecVideoDecoderTest::RequestOverlayInfoCb,
                   base::Unretained(this)),
        std::move(video_frame_factory));
    mcvd_.reset(observable_mcvd);
    mcvd_raw_ = observable_mcvd;
    destruction_observer_ = observable_mcvd->CreateDestructionObserver();
    // Ensure MCVD doesn't leak by default.
    destruction_observer_->ExpectDestruction();
  }

  VideoFrameFactory::OverlayMode ExpectedOverlayMode() const {
    const bool want_promotion_hint =
        device_info_->IsSetOutputSurfaceSupported();
    return want_promotion_hint
               ? VideoFrameFactory::OverlayMode::kRequestPromotionHints
               : VideoFrameFactory::OverlayMode::kDontRequestPromotionHints;
  }

  void CreateCdm(bool has_media_crypto_context,
                 bool require_secure_video_decoder) {
    cdm_ = std::make_unique<MockMediaCryptoContext>(has_media_crypto_context);
    require_secure_video_decoder_ = require_secure_video_decoder;

    // We need to send an object as the media crypto, but MCVD shouldn't
    // use it for anything.  Just send in some random java object, so that
    // it's not null.
    media_crypto_ = base::android::ScopedJavaGlobalRef<jobject>(
        gl::SurfaceTexture::Create(0)->j_surface_texture());
  }

  // Just call Initialize(). MCVD will be waiting for a call to Decode() before
  // continuining initialization.
  bool Initialize(VideoDecoderConfig config) {
    if (!mcvd_)
      CreateMcvd();
    bool result = false;
    auto init_cb = [](bool* result_out, bool result) { *result_out = result; };
    mcvd_->Initialize(
        config, false, cdm_.get(), base::BindOnce(init_cb, &result),
        base::BindRepeating(&OutputCb, &most_recent_frame_), base::DoNothing());
    base::RunLoop().RunUntilIdle();

    // If there is a CDM available, then we expect that MCVD will be waiting
    // for the media crypto object.
    // TODO(liberato): why does CreateJavaObjectPtr() not link?
    if (cdm_ && cdm_->media_crypto_ready_cb) {
      std::move(cdm_->media_crypto_ready_cb)
          .Run(std::make_unique<base::android::ScopedJavaGlobalRef<jobject>>(
                   media_crypto_),
               require_secure_video_decoder_);
      // The callback is consumed, mark that we ran it so tests can verify.
      cdm_->ran_media_crypto_ready_cb = true;
      base::RunLoop().RunUntilIdle();
    }

    return result;
  }

  // Call Initialize() and Decode() to start lazy init. MCVD will be waiting for
  // a codec and have one decode pending.
  MockAndroidOverlay* InitializeWithOverlay_OneDecodePending(
      VideoDecoderConfig config) {
    Initialize(config);
    mcvd_->Decode(fake_decoder_buffer_, decode_cb_.Get());
    OverlayInfo info;
    info.routing_token = base::UnguessableToken::Deserialize(1, 2);
    provide_overlay_info_cb_.Run(info);
    auto overlay_ptr = std::make_unique<MockAndroidOverlay>();
    auto* overlay = overlay_ptr.get();

    if (!java_surface_) {
      java_surface_ = base::android::ScopedJavaGlobalRef<jobject>(
          gl::SurfaceTexture::Create(0)->j_surface_texture());
    }
    EXPECT_CALL(*overlay, GetJavaSurface())
        .WillRepeatedly(ReturnRef(java_surface_));

    surface_chooser_->ProvideOverlay(std::move(overlay_ptr));
    return overlay;
  }

  // Call Initialize() and Decode() to start lazy init. MCVD will be waiting for
  // a codec and have one decode pending.
  void InitializeWithTextureOwner_OneDecodePending(VideoDecoderConfig config) {
    Initialize(config);
    mcvd_->Decode(fake_decoder_buffer_, decode_cb_.Get());
    provide_overlay_info_cb_.Run(OverlayInfo());
    surface_chooser_->ProvideTextureOwner();
  }

  // Fully initializes MCVD and returns the codec it's configured with. MCVD
  // will have one decode pending.
  MockMediaCodecBridge* InitializeFully_OneDecodePending(
      VideoDecoderConfig config) {
    InitializeWithTextureOwner_OneDecodePending(config);
    return codec_allocator_->ProvideMockCodecAsync();
  }

  // Provide access to MCVD's private PumpCodec() to drive the state transitions
  // that depend on queueing and dequeueing buffers. It uses |mcvd_raw_| so that
  // it can be called after |mcvd_| is reset.
  void PumpCodec() { mcvd_raw_->PumpCodec(false); }

  // Start and finish a reset.
  void DoReset() {
    bool reset_complete = false;
    mcvd_->Reset(base::BindRepeating(
        [](bool* reset_complete) { *reset_complete = true; }, &reset_complete));
    base::RunLoop().RunUntilIdle();
    if (!reset_complete) {
      // Note that there might be more pending decodes, and this will arrive
      // out of order.  We assume that MCVD doesn't care.
      codec_allocator_->most_recent_codec->ProduceOneOutput(
          MockMediaCodecBridge::kEos);
      PumpCodec();
      EXPECT_TRUE(reset_complete);
    }
  }

  void RequestOverlayInfoCb(
      bool restart_for_transitions,
      const ProvideOverlayInfoCB& provide_overlay_info_cb) {
    restart_for_transitions_ = restart_for_transitions;
    provide_overlay_info_cb_ = provide_overlay_info_cb;
  }

 protected:
  const VideoCodec codec_;
  base::test::SingleThreadTaskEnvironment task_environment_;
  base::android::ScopedJavaGlobalRef<jobject> java_surface_;
  scoped_refptr<DecoderBuffer> fake_decoder_buffer_;
  std::unique_ptr<MockDeviceInfo> device_info_;
  std::unique_ptr<FakeCodecAllocator> codec_allocator_;
  MockAndroidVideoSurfaceChooser* surface_chooser_;
  gpu::MockTextureOwner* texture_owner_;
  MockVideoFrameFactory* video_frame_factory_;
  NiceMock<base::MockCallback<VideoDecoder::DecodeCB>> decode_cb_;
  std::unique_ptr<DestructionObserver> destruction_observer_;
  ProvideOverlayInfoCB provide_overlay_info_cb_;
  bool restart_for_transitions_;
  gpu::GpuPreferences gpu_preferences_;
  gpu::GpuFeatureInfo gpu_feature_info_;
  scoped_refptr<VideoFrame> most_recent_frame_;

  // This is not an actual media crypto object.
  base::android::ScopedJavaGlobalRef<jobject> media_crypto_;
  bool require_secure_video_decoder_ = false;

  // This must outlive |mcvd_| .
  std::unique_ptr<MockMediaCryptoContext> cdm_;

  // |mcvd_raw_| lets us call PumpCodec() even after |mcvd_| is dropped, for
  // testing the teardown path.
  MediaCodecVideoDecoder* mcvd_raw_;
  std::unique_ptr<MediaCodecVideoDecoder> mcvd_;
};

// Tests which only work for a single codec.
class MediaCodecVideoDecoderAV1Test : public MediaCodecVideoDecoderTest {};
class MediaCodecVideoDecoderH264Test : public MediaCodecVideoDecoderTest {};
class MediaCodecVideoDecoderVp8Test : public MediaCodecVideoDecoderTest {};
class MediaCodecVideoDecoderVp9Test : public MediaCodecVideoDecoderTest {};

TEST_P(MediaCodecVideoDecoderTest, UnknownCodecIsRejected) {
  ASSERT_FALSE(Initialize(TestVideoConfig::Invalid()));
}

TEST_P(MediaCodecVideoDecoderH264Test, H264IsSupported) {
  ASSERT_TRUE(Initialize(TestVideoConfig::NormalH264()));
}

TEST_P(MediaCodecVideoDecoderVp8Test, SmallVp8IsRejected) {
  auto configs = MediaCodecVideoDecoder::GetSupportedConfigs();
  auto small_vp8_config = TestVideoConfig::Normal();
  for (const auto& c : configs)
    ASSERT_FALSE(c.Matches(small_vp8_config));
}

TEST_P(MediaCodecVideoDecoderAV1Test, Av1IsSupported) {
  EXPECT_CALL(*device_info_, IsAv1DecoderAvailable()).WillOnce(Return(true));
  ASSERT_TRUE(Initialize(TestVideoConfig::Normal(kCodecAV1)));
}

TEST_P(MediaCodecVideoDecoderTest, InitializeDoesntInitSurfaceOrCodec) {
  CreateMcvd();
  EXPECT_CALL(*video_frame_factory_, Initialize(ExpectedOverlayMode(), _))
      .Times(0);
  EXPECT_CALL(*surface_chooser_, MockUpdateState()).Times(0);
  EXPECT_CALL(*codec_allocator_, MockCreateMediaCodecAsync()).Times(0);
  Initialize(TestVideoConfig::Large(codec_));
}

TEST_P(MediaCodecVideoDecoderTest, FirstDecodeTriggersFrameFactoryInit) {
  Initialize(TestVideoConfig::Large(codec_));
  EXPECT_CALL(*video_frame_factory_, Initialize(ExpectedOverlayMode(), _));
  mcvd_->Decode(fake_decoder_buffer_, decode_cb_.Get());
}

TEST_P(MediaCodecVideoDecoderTest,
       FirstDecodeTriggersOverlayInfoRequestIfSupported) {
  Initialize(TestVideoConfig::Large(codec_));
  // Requesting overlay info sets this cb.
  ASSERT_FALSE(provide_overlay_info_cb_);
  mcvd_->Decode(fake_decoder_buffer_, decode_cb_.Get());
  ASSERT_TRUE(provide_overlay_info_cb_);
}

TEST_P(MediaCodecVideoDecoderTest,
       OverlayInfoIsNotRequestedIfOverlaysNotSupported) {
  Initialize(TestVideoConfig::Large(codec_));
  ON_CALL(*device_info_, SupportsOverlaySurfaces())
      .WillByDefault(Return(false));
  mcvd_->Decode(fake_decoder_buffer_, decode_cb_.Get());
  ASSERT_FALSE(provide_overlay_info_cb_);
}

TEST_P(MediaCodecVideoDecoderTest, RestartForOverlayTransitionsFlagIsCorrect) {
  ON_CALL(*device_info_, IsSetOutputSurfaceSupported())
      .WillByDefault(Return(true));
  Initialize(TestVideoConfig::Large(codec_));
  mcvd_->Decode(fake_decoder_buffer_, decode_cb_.Get());
  ASSERT_FALSE(restart_for_transitions_);
}

TEST_P(MediaCodecVideoDecoderTest,
       OverlayInfoIsNotRequestedIfThreadedTextureMailboxesEnabled) {
  gpu_preferences_.enable_threaded_texture_mailboxes = true;
  Initialize(TestVideoConfig::Large(codec_));
  mcvd_->Decode(fake_decoder_buffer_, decode_cb_.Get());
  ASSERT_FALSE(provide_overlay_info_cb_);
}

TEST_P(MediaCodecVideoDecoderTest, OverlayInfoDuringInitUpdatesSurfaceChooser) {
  InitializeWithTextureOwner_OneDecodePending(TestVideoConfig::Large(codec_));
  EXPECT_CALL(*surface_chooser_, MockUpdateState());
  provide_overlay_info_cb_.Run(OverlayInfo());
}

TEST_P(MediaCodecVideoDecoderTest, CodecIsCreatedAfterSurfaceChosen) {
  Initialize(TestVideoConfig::Large(codec_));
  mcvd_->Decode(fake_decoder_buffer_, decode_cb_.Get());
  provide_overlay_info_cb_.Run(OverlayInfo());
  EXPECT_CALL(*codec_allocator_, MockCreateMediaCodecAsync());
  surface_chooser_->ProvideTextureOwner();
}

TEST_P(MediaCodecVideoDecoderTest, FrameFactoryInitFailureIsAnError) {
  Initialize(TestVideoConfig::Large(codec_));
  ON_CALL(*video_frame_factory_, Initialize(ExpectedOverlayMode(), _))
      .WillByDefault(RunCallback<1>(nullptr));
  EXPECT_CALL(decode_cb_, Run(DecodeStatus::DECODE_ERROR)).Times(1);
  EXPECT_CALL(*surface_chooser_, MockUpdateState()).Times(0);
  mcvd_->Decode(fake_decoder_buffer_, decode_cb_.Get());
}

TEST_P(MediaCodecVideoDecoderTest, CodecCreationFailureIsAnError) {
  InitializeWithTextureOwner_OneDecodePending(TestVideoConfig::Large(codec_));
  mcvd_->Decode(fake_decoder_buffer_, decode_cb_.Get());
  EXPECT_CALL(decode_cb_, Run(DecodeStatus::DECODE_ERROR)).Times(2);
  // Failing to create a codec should put MCVD into an error state.
  codec_allocator_->ProvideNullCodecAsync();
}

TEST_P(MediaCodecVideoDecoderTest, CodecFailuresAreAnError) {
  auto* codec =
      InitializeFully_OneDecodePending(TestVideoConfig::Large(codec_));
  EXPECT_CALL(*codec, DequeueInputBuffer(_, _))
      .WillOnce(Return(MEDIA_CODEC_ERROR));
  EXPECT_CALL(decode_cb_, Run(DecodeStatus::DECODE_ERROR));
  PumpCodec();
}

TEST_P(MediaCodecVideoDecoderTest, AfterInitCompletesTheCodecIsPolled) {
  auto* codec =
      InitializeFully_OneDecodePending(TestVideoConfig::Large(codec_));
  // Run a RunLoop until the first time the codec is polled for an available
  // input buffer.
  base::RunLoop loop;
  EXPECT_CALL(*codec, DequeueInputBuffer(_, _))
      .WillOnce(InvokeWithoutArgs([&loop]() {
        loop.Quit();
        return MEDIA_CODEC_TRY_AGAIN_LATER;
      }));
  loop.Run();
}

TEST_P(MediaCodecVideoDecoderTest, CodecIsReleasedOnDestruction) {
  auto* codec =
      InitializeFully_OneDecodePending(TestVideoConfig::Large(codec_));
  EXPECT_CALL(*codec_allocator_, MockReleaseMediaCodec(codec));
}

TEST_P(MediaCodecVideoDecoderTest, SurfaceChooserIsUpdatedOnOverlayChanges) {
  InitializeWithTextureOwner_OneDecodePending(TestVideoConfig::Large(codec_));

  EXPECT_CALL(*surface_chooser_, MockReplaceOverlayFactory(_)).Times(2);
  OverlayInfo info;
  info.routing_token = base::UnguessableToken::Deserialize(1, 2);
  provide_overlay_info_cb_.Run(info);
  ASSERT_TRUE(surface_chooser_->factory_);
  info.routing_token = base::UnguessableToken::Deserialize(3, 4);
  provide_overlay_info_cb_.Run(info);
  ASSERT_TRUE(surface_chooser_->factory_);
}

TEST_P(MediaCodecVideoDecoderTest, OverlayInfoUpdatesAreIgnoredInStateError) {
  InitializeWithTextureOwner_OneDecodePending(TestVideoConfig::Large(codec_));
  // Enter the error state.
  codec_allocator_->ProvideNullCodecAsync();

  EXPECT_CALL(*surface_chooser_, MockUpdateState()).Times(0);
  OverlayInfo info;
  info.routing_token = base::UnguessableToken::Deserialize(1, 2);
  provide_overlay_info_cb_.Run(info);
}

TEST_P(MediaCodecVideoDecoderTest, DuplicateOverlayInfoUpdatesAreIgnored) {
  InitializeWithTextureOwner_OneDecodePending(TestVideoConfig::Large(codec_));

  // The second overlay info update should be ignored.
  EXPECT_CALL(*surface_chooser_, MockReplaceOverlayFactory(_)).Times(1);
  OverlayInfo info;
  info.routing_token = base::UnguessableToken::Deserialize(1, 2);
  provide_overlay_info_cb_.Run(info);
  provide_overlay_info_cb_.Run(info);
}

TEST_P(MediaCodecVideoDecoderTest, CodecIsCreatedWithChosenOverlay) {
  EXPECT_CALL(*codec_allocator_, MockCreateMediaCodecAsync());
  InitializeWithOverlay_OneDecodePending(TestVideoConfig::Large(codec_));
  EXPECT_TRUE(base::android::AttachCurrentThread()->IsSameObject(
      java_surface_.obj(),
      codec_allocator_->most_recent_config->surface.obj()));
}

TEST_P(MediaCodecVideoDecoderTest,
       CodecCreationWeakPtrIsInvalidatedBySurfaceDestroyed) {
  ON_CALL(*device_info_, IsSetOutputSurfaceSupported())
      .WillByDefault(Return(false));
  auto* overlay =
      InitializeWithOverlay_OneDecodePending(TestVideoConfig::Large(codec_));
  overlay->OnSurfaceDestroyed();

  // MCVD handles release of the MediaCodec after WeakPtr invalidation.
  EXPECT_CALL(*codec_allocator_, MockReleaseMediaCodec(NotNull()));
  auto* codec = codec_allocator_->ProvideMockCodecAsync();
  ASSERT_TRUE(!!codec);
}

TEST_P(MediaCodecVideoDecoderTest, SurfaceChangedWhileCodecCreationPending) {
  auto* overlay =
      InitializeWithOverlay_OneDecodePending(TestVideoConfig::Large(codec_));
  overlay->OnSurfaceDestroyed();
  auto codec = std::make_unique<NiceMock<MockMediaCodecBridge>>();

  // SetSurface() is called as soon as the codec is created to switch away from
  // the destroyed surface.
  EXPECT_CALL(*codec, SetSurface(_)).WillOnce(Return(true));
  codec_allocator_->ProvideMockCodecAsync(std::move(codec));
}

TEST_P(MediaCodecVideoDecoderTest, SurfaceDestroyedDoesSyncSurfaceTransition) {
  auto* overlay =
      InitializeWithOverlay_OneDecodePending(TestVideoConfig::Large(codec_));
  auto* codec = codec_allocator_->ProvideMockCodecAsync();

  // MCVD must synchronously switch the codec's surface (to surface
  // texture), and delete the overlay.
  EXPECT_CALL(*codec, SetSurface(_)).WillOnce(Return(true));
  auto observer = overlay->CreateDestructionObserver();
  observer->ExpectDestruction();
  overlay->OnSurfaceDestroyed();
}

TEST_P(MediaCodecVideoDecoderTest,
       SurfaceDestroyedReleasesCodecIfSetSurfaceIsNotSupported) {
  ON_CALL(*device_info_, IsSetOutputSurfaceSupported())
      .WillByDefault(Return(false));
  auto* overlay =
      InitializeWithOverlay_OneDecodePending(TestVideoConfig::Large(codec_));
  auto* codec = codec_allocator_->ProvideMockCodecAsync();

  // MCVD must synchronously release the codec.
  EXPECT_CALL(*codec, SetSurface(_)).Times(0);
  EXPECT_CALL(*codec_allocator_, MockReleaseMediaCodec(codec));
  overlay->OnSurfaceDestroyed();
  // Verify expectations before we delete the MCVD.
  testing::Mock::VerifyAndClearExpectations(codec_allocator_.get());
}

TEST_P(MediaCodecVideoDecoderTest, PumpCodecPerformsPendingSurfaceTransitions) {
  InitializeWithOverlay_OneDecodePending(TestVideoConfig::Large(codec_));
  auto* codec = codec_allocator_->ProvideMockCodecAsync();

  // Set a pending surface transition and then call PumpCodec().
  surface_chooser_->ProvideTextureOwner();
  EXPECT_CALL(*codec, SetSurface(_)).WillOnce(Return(true));
  PumpCodec();
}

TEST_P(MediaCodecVideoDecoderTest,
       SetSurfaceFailureReleasesTheCodecAndSignalsError) {
  InitializeWithOverlay_OneDecodePending(TestVideoConfig::Large(codec_));
  auto* codec = codec_allocator_->ProvideMockCodecAsync();

  surface_chooser_->ProvideTextureOwner();
  EXPECT_CALL(*codec, SetSurface(_)).WillOnce(Return(false));
  EXPECT_CALL(decode_cb_, Run(DecodeStatus::DECODE_ERROR)).Times(2);
  EXPECT_CALL(*codec_allocator_, MockReleaseMediaCodec(codec));
  mcvd_->Decode(fake_decoder_buffer_, decode_cb_.Get());
  // Verify expectations before we delete the MCVD.
  testing::Mock::VerifyAndClearExpectations(codec_allocator_.get());
}

TEST_P(MediaCodecVideoDecoderTest, SurfaceTransitionsCanBeCanceled) {
  InitializeWithTextureOwner_OneDecodePending(TestVideoConfig::Large(codec_));
  auto* codec = codec_allocator_->ProvideMockCodecAsync();

  // Set a pending transition to an overlay, and then back to a texture owner.
  // They should cancel each other out and leave the codec as-is.
  EXPECT_CALL(*codec, SetSurface(_)).Times(0);
  auto overlay = std::make_unique<MockAndroidOverlay>();
  auto observer = overlay->CreateDestructionObserver();
  surface_chooser_->ProvideOverlay(std::move(overlay));

  // Switching back to texture owner should delete the pending overlay.
  observer->ExpectDestruction();
  surface_chooser_->ProvideTextureOwner();
  observer.reset();

  // Verify that Decode() does not transition the surface
  mcvd_->Decode(fake_decoder_buffer_, decode_cb_.Get());
}

TEST_P(MediaCodecVideoDecoderTest, TransitionToSameSurfaceIsIgnored) {
  InitializeWithTextureOwner_OneDecodePending(TestVideoConfig::Large(codec_));
  auto* codec = codec_allocator_->ProvideMockCodecAsync();
  EXPECT_CALL(*codec, SetSurface(_)).Times(0);
  surface_chooser_->ProvideTextureOwner();
  mcvd_->Decode(fake_decoder_buffer_, decode_cb_.Get());
}

TEST_P(MediaCodecVideoDecoderTest,
       ResetBeforeCodecInitializedSucceedsImmediately) {
  InitializeWithTextureOwner_OneDecodePending(TestVideoConfig::Large(codec_));
  base::MockCallback<base::Closure> reset_cb;
  EXPECT_CALL(reset_cb, Run());
  mcvd_->Reset(reset_cb.Get());
  testing::Mock::VerifyAndClearExpectations(&reset_cb);
}

TEST_P(MediaCodecVideoDecoderTest, ResetAbortsPendingDecodes) {
  InitializeWithTextureOwner_OneDecodePending(TestVideoConfig::Large(codec_));
  EXPECT_CALL(decode_cb_, Run(DecodeStatus::ABORTED));
  DoReset();
  testing::Mock::VerifyAndClearExpectations(&decode_cb_);
}

TEST_P(MediaCodecVideoDecoderTest, ResetAbortsPendingEosDecode) {
  // EOS is treated differently by MCVD. This verifies that it's also aborted.
  auto* codec =
      InitializeFully_OneDecodePending(TestVideoConfig::Large(codec_));
  base::MockCallback<VideoDecoder::DecodeCB> eos_decode_cb;
  mcvd_->Decode(DecoderBuffer::CreateEOSBuffer(), eos_decode_cb.Get());

  // Accept the two pending decodes.
  codec->AcceptOneInput();
  PumpCodec();
  codec->AcceptOneInput(MockMediaCodecBridge::kEos);
  PumpCodec();

  EXPECT_CALL(eos_decode_cb, Run(DecodeStatus::ABORTED));
  DoReset();
  // Should be run before |mcvd_| is destroyed.
  testing::Mock::VerifyAndClearExpectations(&eos_decode_cb);
}

TEST_P(MediaCodecVideoDecoderTest, ResetDoesNotFlushAnAlreadyFlushedCodec) {
  auto* codec =
      InitializeFully_OneDecodePending(TestVideoConfig::Large(codec_));

  // The codec is still in the flushed state so Reset() doesn't need to flush.
  EXPECT_CALL(*codec, Flush()).Times(0);
  base::MockCallback<base::Closure> reset_cb;
  EXPECT_CALL(reset_cb, Run());
  mcvd_->Reset(reset_cb.Get());
  testing::Mock::VerifyAndClearExpectations(&decode_cb_);
}

TEST_P(MediaCodecVideoDecoderVp8Test, ResetDrainsVP8CodecsBeforeFlushing) {
  auto* codec =
      InitializeFully_OneDecodePending(TestVideoConfig::Large(codec_));
  // Accept the first decode to transition out of the flushed state.
  codec->AcceptOneInput();
  PumpCodec();

  // The reset should not complete immediately because the codec needs to be
  // drained.
  EXPECT_CALL(*codec, Flush()).Times(0);
  base::MockCallback<base::Closure> reset_cb;
  EXPECT_CALL(reset_cb, Run()).Times(0);
  mcvd_->Reset(reset_cb.Get());

  // The next input should be an EOS.
  codec->AcceptOneInput(MockMediaCodecBridge::kEos);
  PumpCodec();
  testing::Mock::VerifyAndClearExpectations(codec);

  // After the EOS is dequeued, the reset should complete.
  EXPECT_CALL(reset_cb, Run());
  codec->ProduceOneOutput(MockMediaCodecBridge::kEos);
  PumpCodec();
  testing::Mock::VerifyAndClearExpectations(&reset_cb);
}

// Makes sure UnregisterPlayer() works with async decoder destruction.
// Uses VP8 because this is the only codec that could trigger async destruction.
// See https://crbug.com/893498
TEST_P(MediaCodecVideoDecoderVp8Test, UnregisterPlayerBeforeAsyncDestruction) {
  CreateCdm(true, false);
  EXPECT_CALL(*cdm_, RegisterPlayer(_, _));
  auto* codec = InitializeFully_OneDecodePending(
      TestVideoConfig::NormalEncrypted(codec_));

  // Accept the first decode to transition out of the flushed state. This is
  // necessary to make sure the decoder is destructed asynchronously.
  codec->AcceptOneInput();
  PumpCodec();

  // When |mcvd_| is reset, expect that it will unregister itself immediately,
  // before the decoder is actually destructed, asynchronously.
  EXPECT_CALL(*cdm_, UnregisterPlayer(MockMediaCryptoContext::kRegistrationId));
  mcvd_.reset();

  // Make sure the decoder has not been destroyed yet.
  destruction_observer_->DoNotAllowDestruction();
}

// A reference test for UnregisterPlayerBeforeAsyncDestruction.
TEST_P(MediaCodecVideoDecoderVp8Test, UnregisterPlayerBeforeSyncDestruction) {
  CreateCdm(true, false);
  EXPECT_CALL(*cdm_, RegisterPlayer(_, _));
  InitializeFully_OneDecodePending(TestVideoConfig::NormalEncrypted(codec_));

  // Do not attempt any decode to keep the decoder in a clean state. This is
  // necessary to make sure the decoder is destructed synchronously.

  // When |mcvd_| is reset, expect that it will unregister itself immediately.
  EXPECT_CALL(*cdm_, UnregisterPlayer(MockMediaCryptoContext::kRegistrationId));
  mcvd_.reset();

  // Make sure the decoder is now destroyed.
  destruction_observer_->ExpectDestruction();
}

TEST_P(MediaCodecVideoDecoderVp8Test, ResetDoesNotDrainVp8WithAsyncApi) {
  EXPECT_CALL(*device_info_, IsAsyncApiSupported())
      .WillRepeatedly(Return(true));

  auto* codec =
      InitializeFully_OneDecodePending(TestVideoConfig::Large(codec_));
  // Accept the first decode to transition out of the flushed state.
  codec->AcceptOneInput();
  PumpCodec();

  // The reset should complete immediately because the codec is not VP8 so
  // it doesn't need draining.  We don't expect a call to Flush on the codec
  // since it will be deferred until the first decode after the reset.
  base::MockCallback<base::Closure> reset_cb;
  EXPECT_CALL(reset_cb, Run());
  mcvd_->Reset(reset_cb.Get());
  // The reset should complete before destroying the codec, since TearDown will
  // complete the drain for VP8.  It still might not call reset since a drain
  // for destroy probably doesn't, but either way we expect it before the drain.
  testing::Mock::VerifyAndClearExpectations(&reset_cb);
}

TEST_P(MediaCodecVideoDecoderH264Test, ResetDoesNotDrainNonVp8Codecs) {
  auto* codec =
      InitializeFully_OneDecodePending(TestVideoConfig::Large(codec_));
  // Accept the first decode to transition out of the flushed state.
  codec->AcceptOneInput();
  PumpCodec();

  // The reset should complete immediately because the codec is not VP8 so
  // it doesn't need draining.  We don't expect a call to Flush on the codec
  // since it will be deferred until the first decode after the reset.
  base::MockCallback<base::Closure> reset_cb;
  EXPECT_CALL(reset_cb, Run());
  mcvd_->Reset(reset_cb.Get());
  // The reset should complete before destroying the codec, since TearDown will
  // complete the drain for VP8.  It still might not call reset since a drain
  // for destroy probably doesn't, but either way we expect it before the drain.
  testing::Mock::VerifyAndClearExpectations(&reset_cb);
}

TEST_P(MediaCodecVideoDecoderVp8Test, TeardownCompletesPendingReset) {
  auto* codec =
      InitializeFully_OneDecodePending(TestVideoConfig::Large(codec_));

  // Accept the first decode to transition out of the flushed state.
  codec->AcceptOneInput();
  PumpCodec();

  base::MockCallback<base::Closure> reset_cb;
  EXPECT_CALL(reset_cb, Run()).Times(0);
  mcvd_->Reset(reset_cb.Get());
  EXPECT_CALL(reset_cb, Run());
  mcvd_.reset();

  // VP8 codecs requiring draining for teardown to complete (tested below).
  codec->ProduceOneOutput(MockMediaCodecBridge::kEos);
  PumpCodec();
}

TEST_P(MediaCodecVideoDecoderTest, CodecFlushIsDeferredAfterDraining) {
  auto* codec =
      InitializeFully_OneDecodePending(TestVideoConfig::Large(codec_));
  mcvd_->Decode(DecoderBuffer::CreateEOSBuffer(), decode_cb_.Get());

  // Produce one output that VFF will hold onto.
  codec->AcceptOneInput();
  codec->ProduceOneOutput();
  PumpCodec();

  // Drain the codec.
  EXPECT_CALL(*codec, Flush()).Times(0);
  codec->AcceptOneInput(MockMediaCodecBridge::kEos);
  codec->ProduceOneOutput(MockMediaCodecBridge::kEos);
  PumpCodec();

  // Create a pending decode. The codec should still not be flushed because
  // there is an unrendered output buffer.
  mcvd_->Decode(fake_decoder_buffer_, decode_cb_.Get());
  PumpCodec();

  // Releasing the output buffer should now trigger a flush.
  video_frame_factory_->last_output_buffer_.reset();
  EXPECT_CALL(*codec, Flush());
  PumpCodec();
}

TEST_P(MediaCodecVideoDecoderTest, EosDecodeCbIsRunAfterEosIsDequeued) {
  auto* codec =
      InitializeFully_OneDecodePending(TestVideoConfig::Large(codec_));
  codec->AcceptOneInput();
  PumpCodec();

  base::MockCallback<VideoDecoder::DecodeCB> eos_decode_cb;
  EXPECT_CALL(eos_decode_cb, Run(_)).Times(0);
  mcvd_->Decode(DecoderBuffer::CreateEOSBuffer(), eos_decode_cb.Get());
  codec->AcceptOneInput(MockMediaCodecBridge::kEos);
  PumpCodec();

  // On dequeueing EOS, MCVD will post a closure to run eos_decode_cb after
  // pending video frames.
  EXPECT_CALL(*video_frame_factory_, MockRunAfterPendingVideoFrames(_));
  codec->ProduceOneOutput(MockMediaCodecBridge::kEos);
  PumpCodec();

  EXPECT_CALL(eos_decode_cb, Run(DecodeStatus::OK));
  std::move(video_frame_factory_->last_closure_).Run();
}

TEST_P(MediaCodecVideoDecoderTest, TeardownBeforeInitWorks) {
  // Since we assert that MCVD is destructed by default, this test verifies that
  // MCVD is destructed safely before Initialize().
}

TEST_P(MediaCodecVideoDecoderTest, TeardownInvalidatesCodecCreationWeakPtr) {
  InitializeWithTextureOwner_OneDecodePending(TestVideoConfig::Large(codec_));
  destruction_observer_->DoNotAllowDestruction();
  mcvd_.reset();
  // DeleteSoon() is now pending. Ensure it's safe if the codec creation
  // completes before it runs.
  destruction_observer_->ExpectDestruction();
  EXPECT_CALL(*codec_allocator_, MockReleaseMediaCodec(NotNull()));
  ASSERT_TRUE(codec_allocator_->ProvideMockCodecAsync());
}

TEST_P(MediaCodecVideoDecoderTest,
       TeardownInvalidatesCodecCreationWeakPtrButDoesNotCallReleaseMediaCodec) {
  InitializeWithTextureOwner_OneDecodePending(TestVideoConfig::Large(codec_));
  destruction_observer_->DoNotAllowDestruction();
  mcvd_.reset();
  // DeleteSoon() is now pending. Ensure it's safe if the codec creation
  // completes before it runs.
  destruction_observer_->ExpectDestruction();

  // A null codec should not be released via ReleaseMediaCodec().
  EXPECT_CALL(*codec_allocator_, MockReleaseMediaCodec(_)).Times(0);
  codec_allocator_->ProvideNullCodecAsync();
}

TEST_P(MediaCodecVideoDecoderTest, TeardownDoesNotDrainFlushedCodecs) {
  InitializeFully_OneDecodePending(TestVideoConfig::Large(codec_));
  // Since we assert that MCVD is destructed by default, this test verifies that
  // MCVD is destructed without requiring the codec to output an EOS buffer.

  // We assert this since, otherwise, we'll complete the drain for VP8 codecs in
  // TearDown.  This guarantees that we won't, so any drain started by MCVD
  // won't complete.  Otherwise, this tests nothing.  Note that 'Drained' here
  // is a bit of a misnomer; the mock codec doesn't track flushed.
  ASSERT_TRUE(codec_allocator_->most_recent_codec->IsDrained());
}

TEST_P(MediaCodecVideoDecoderH264Test, TeardownDoesNotDrainNonVp8Codecs) {
  auto* codec =
      InitializeFully_OneDecodePending(TestVideoConfig::Large(codec_));
  // Accept the first decode to transition out of the flushed state.
  codec->AcceptOneInput();
  PumpCodec();
  // Since we assert that MCVD is destructed by default, this test verifies that
  // MCVD is destructed without requiring the codec to output an EOS buffer.
  // Remember that we do not complete the drain for non-VP8 codecs in TearDown.
}

TEST_P(MediaCodecVideoDecoderVp8Test,
       TeardownDrainsVp8CodecsBeforeDestruction) {
  auto* codec =
      InitializeFully_OneDecodePending(TestVideoConfig::Large(codec_));
  // Accept the first decode to transition out of the flushed state.
  codec->AcceptOneInput();
  PumpCodec();

  // MCVD should not be destructed immediately.
  destruction_observer_->DoNotAllowDestruction();
  mcvd_.reset();
  base::RunLoop().RunUntilIdle();

  // It should be destructed after draining completes.
  codec->AcceptOneInput(MockMediaCodecBridge::kEos);
  codec->ProduceOneOutput(MockMediaCodecBridge::kEos);
  EXPECT_CALL(*codec, Flush()).Times(0);
  destruction_observer_->ExpectDestruction();
  PumpCodec();
  base::RunLoop().RunUntilIdle();
}

TEST_P(MediaCodecVideoDecoderTest, CdmInitializationWorksForL3) {
  // Make sure that MCVD uses the cdm, and sends it along to the codec.
  CreateCdm(true, false);
  EXPECT_CALL(*cdm_, RegisterPlayer(_, _));
  InitializeWithOverlay_OneDecodePending(
      TestVideoConfig::NormalEncrypted(codec_));
  ASSERT_TRUE(!!cdm_->new_key_cb);
  ASSERT_TRUE(!!cdm_->cdm_unset_cb);
  ASSERT_TRUE(!!cdm_->ran_media_crypto_ready_cb);
  ASSERT_EQ(surface_chooser_->current_state_.is_secure, true);
  ASSERT_EQ(surface_chooser_->current_state_.is_required, false);
  ASSERT_EQ(codec_allocator_->most_recent_config->codec_type, CodecType::kAny);
  // We can't check for equality safely, but verify that something was provided.
  ASSERT_TRUE(codec_allocator_->most_recent_config->media_crypto);

  // When |mcvd_| is destroyed, expect that it will unregister itself.
  EXPECT_CALL(*cdm_, UnregisterPlayer(MockMediaCryptoContext::kRegistrationId));
}

TEST_P(MediaCodecVideoDecoderTest, CdmInitializationWorksForL1) {
  // Make sure that MCVD uses the cdm, and sends it along to the codec.
  CreateCdm(true, true);
  EXPECT_CALL(*cdm_, RegisterPlayer(_, _));
  InitializeWithOverlay_OneDecodePending(
      TestVideoConfig::NormalEncrypted(codec_));
  ASSERT_TRUE(!!cdm_->new_key_cb);
  ASSERT_TRUE(!!cdm_->cdm_unset_cb);
  ASSERT_TRUE(!!cdm_->ran_media_crypto_ready_cb);
  ASSERT_EQ(surface_chooser_->current_state_.is_secure, true);
  ASSERT_EQ(surface_chooser_->current_state_.is_required, true);
  ASSERT_EQ(codec_allocator_->most_recent_config->codec_type,
            CodecType::kSecure);
  ASSERT_TRUE(codec_allocator_->most_recent_config->media_crypto);

  // When |mcvd_| is destroyed, expect that it will unregister itself.
  EXPECT_CALL(*cdm_, UnregisterPlayer(MockMediaCryptoContext::kRegistrationId));
}

TEST_P(MediaCodecVideoDecoderTest, CdmIsSetEvenForClearStream) {
  // Make sure that MCVD uses the cdm, and sends it along to the codec.
  CreateCdm(true, false);
  EXPECT_CALL(*cdm_, RegisterPlayer(_, _));
  // We use the Large config, since VPx can be rejected if it's too small, in
  // favor of software decode, since this is unencrypted.
  InitializeWithOverlay_OneDecodePending(TestVideoConfig::Large(codec_));
  ASSERT_TRUE(!!cdm_->new_key_cb);
  ASSERT_TRUE(!!cdm_->cdm_unset_cb);
  ASSERT_TRUE(!!cdm_->ran_media_crypto_ready_cb);
  ASSERT_EQ(surface_chooser_->current_state_.is_secure, true);
  ASSERT_EQ(surface_chooser_->current_state_.is_required, false);
  ASSERT_NE(codec_allocator_->most_recent_config->codec_type,
            CodecType::kSecure);
  // We can't check for equality safely, but verify that something was provided.
  ASSERT_TRUE(codec_allocator_->most_recent_config->media_crypto);

  // When |mcvd_| is destroyed, expect that it will unregister itself.
  EXPECT_CALL(*cdm_, UnregisterPlayer(MockMediaCryptoContext::kRegistrationId));
}

TEST_P(MediaCodecVideoDecoderTest, NoMediaCryptoContext_ClearStream) {
  // Make sure that MCVD initializes for clear stream when MediaCryptoContext
  // is not available.
  CreateCdm(false, false);
  InitializeWithOverlay_OneDecodePending(TestVideoConfig::Normal(codec_));
  ASSERT_FALSE(!!cdm_->new_key_cb);
  ASSERT_FALSE(!!cdm_->cdm_unset_cb);
  ASSERT_FALSE(!!cdm_->media_crypto_ready_cb);
  ASSERT_FALSE(!!cdm_->ran_media_crypto_ready_cb);
  ASSERT_EQ(surface_chooser_->current_state_.is_secure, false);
  ASSERT_EQ(surface_chooser_->current_state_.is_required, false);
  ASSERT_NE(codec_allocator_->most_recent_config->codec_type,
            CodecType::kSecure);
  ASSERT_FALSE(codec_allocator_->most_recent_config->media_crypto);
}

TEST_P(MediaCodecVideoDecoderTest, NoMediaCryptoContext_EncryptedStream) {
  // Make sure that MCVD fails to initialize for encrypted stream when
  // MediaCryptoContext is not available.
  CreateCdm(false, false);
  ASSERT_FALSE(Initialize(TestVideoConfig::NormalEncrypted(codec_)));
}

TEST_P(MediaCodecVideoDecoderTest, MissingMediaCryptoFailsInit) {
  // Encrypted media that doesn't get a mediacrypto should fail to init.
  CreateCdm(true, true);
  media_crypto_ = nullptr;
  ASSERT_FALSE(Initialize(TestVideoConfig::NormalEncrypted(codec_)));
}

TEST_P(MediaCodecVideoDecoderTest, MissingCdmFailsInit) {
  // MCVD should fail init if we don't provide a cdm with an encrypted config.
  ASSERT_FALSE(Initialize(TestVideoConfig::NormalEncrypted(codec_)));
}

TEST_P(MediaCodecVideoDecoderTest, VideoFramesArePowerEfficient) {
  // MCVD should mark video frames as POWER_EFFICIENT.
  auto* codec =
      InitializeFully_OneDecodePending(TestVideoConfig::Large(codec_));

  // Produce one output.
  codec->AcceptOneInput();
  codec->ProduceOneOutput();
  EXPECT_CALL(*video_frame_factory_, MockCreateVideoFrame(_, _, _, _, _));
  PumpCodec();
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(!!most_recent_frame_);
  bool power_efficient = false;
  EXPECT_TRUE(most_recent_frame_->metadata()->GetBoolean(
      VideoFrameMetadata::POWER_EFFICIENT, &power_efficient));
  EXPECT_TRUE(power_efficient);
}

TEST_P(MediaCodecVideoDecoderH264Test, CsdIsIncludedInCodecConfig) {
  // Make sure that any CSD is included in the CodecConfig that MCVD uses to
  // allocate the codec.
  VideoDecoderConfig config = TestVideoConfig::NormalH264();

  // Csd, excluding '0 0 0 1'.
  std::vector<uint8_t> csd0 = {103, 77,  64, 30,  232, 128, 80, 23,
                               252, 184, 8,  128, 0,   0,   3,  0,
                               128, 0,   0,  30,  7,   139, 22, 137};
  std::vector<uint8_t> csd1 = {104, 235, 239, 32};
  std::vector<uint8_t> extra_data_separator = {1, 0, 4};
  std::vector<uint8_t> extra_data = {1, 77, 64, 30, 255, 225, 0, 24};
  extra_data.insert(extra_data.end(), csd0.begin(), csd0.end());
  extra_data.insert(extra_data.end(), extra_data_separator.begin(),
                    extra_data_separator.end());
  extra_data.insert(extra_data.end(), csd1.begin(), csd1.end());
  config.SetExtraData(extra_data);

  EXPECT_TRUE(InitializeFully_OneDecodePending(config));

  // Prepend the headers and check for equality.
  std::vector<uint8_t> csd_header = {0, 0, 0, 1};
  csd0.insert(csd0.begin(), csd_header.begin(), csd_header.end());
  EXPECT_EQ(csd0, codec_allocator_->most_recent_config->csd0);
  csd1.insert(csd1.begin(), csd_header.begin(), csd_header.end());
  EXPECT_EQ(csd1, codec_allocator_->most_recent_config->csd1);
}

TEST_P(MediaCodecVideoDecoderVp9Test, ColorSpaceIsIncludedInCodecConfig) {
  VideoColorSpace color_space(VideoColorSpace::PrimaryID::BT2020,
                              VideoColorSpace::TransferID::SMPTEST2084,
                              VideoColorSpace::MatrixID::BT2020_CL,
                              gfx::ColorSpace::RangeID::LIMITED);
  VideoDecoderConfig config =
      TestVideoConfig::NormalWithColorSpace(kCodecVP9, color_space);
  EXPECT_TRUE(InitializeFully_OneDecodePending(config));

  EXPECT_EQ(color_space,
            codec_allocator_->most_recent_config->container_color_space);
}

TEST_P(MediaCodecVideoDecoderVp9Test, HdrMetadataIsIncludedInCodecConfig) {
  VideoDecoderConfig config = TestVideoConfig::Normal(kCodecVP9);
  HDRMetadata hdr_metadata;
  hdr_metadata.max_frame_average_light_level = 123;
  hdr_metadata.max_content_light_level = 456;
  hdr_metadata.mastering_metadata.primary_r.set_x(0.1f);
  hdr_metadata.mastering_metadata.primary_r.set_y(0.2f);
  hdr_metadata.mastering_metadata.primary_g.set_x(0.3f);
  hdr_metadata.mastering_metadata.primary_g.set_y(0.4f);
  hdr_metadata.mastering_metadata.primary_b.set_x(0.5f);
  hdr_metadata.mastering_metadata.primary_b.set_y(0.6f);
  hdr_metadata.mastering_metadata.white_point.set_x(0.7f);
  hdr_metadata.mastering_metadata.white_point.set_y(0.8f);
  hdr_metadata.mastering_metadata.luminance_max = 1000;
  hdr_metadata.mastering_metadata.luminance_min = 0;

  config.set_hdr_metadata(hdr_metadata);

  EXPECT_TRUE(InitializeFully_OneDecodePending(config));

  EXPECT_EQ(hdr_metadata, codec_allocator_->most_recent_config->hdr_metadata);
}

static std::vector<VideoCodec> GetTestList() {
  std::vector<VideoCodec> test_codecs;

#if BUILDFLAG(USE_PROPRIETARY_CODECS)
  if (MediaCodecUtil::IsMediaCodecAvailable())
    test_codecs.push_back(kCodecH264);
#endif

  if (MediaCodecUtil::IsVp8DecoderAvailable())
    test_codecs.push_back(kCodecVP8);
  if (MediaCodecUtil::IsVp9DecoderAvailable())
    test_codecs.push_back(kCodecVP9);
  return test_codecs;
}

#if BUILDFLAG(USE_PROPRIETARY_CODECS)
static std::vector<VideoCodec> GetH264IfAvailable() {
  return MediaCodecUtil::IsMediaCodecAvailable()
             ? std::vector<VideoCodec>(1, kCodecH264)
             : std::vector<VideoCodec>();
}
#endif

static std::vector<VideoCodec> GetVp8IfAvailable() {
  return MediaCodecUtil::IsVp8DecoderAvailable()
             ? std::vector<VideoCodec>(1, kCodecVP8)
             : std::vector<VideoCodec>();
}

INSTANTIATE_TEST_SUITE_P(MediaCodecVideoDecoderTest,
                         MediaCodecVideoDecoderTest,
                         testing::ValuesIn(GetTestList()));

#if BUILDFLAG(USE_PROPRIETARY_CODECS)
INSTANTIATE_TEST_SUITE_P(MediaCodecVideoDecoderH264Test,
                         MediaCodecVideoDecoderH264Test,
                         testing::ValuesIn(GetH264IfAvailable()));
#endif

INSTANTIATE_TEST_SUITE_P(MediaCodecVideoDecoderVp8Test,
                         MediaCodecVideoDecoderVp8Test,
                         testing::ValuesIn(GetVp8IfAvailable()));

}  // namespace media
