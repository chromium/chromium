// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/decoder_selector.h"

#include <memory>
#include <tuple>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/notreached.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "media/base/callback_registry.h"
#include "media/base/demuxer_stream.h"
#include "media/base/media_switches.h"
#include "media/base/media_util.h"
#include "media/base/mock_filters.h"
#include "media/base/test_helpers.h"
#include "media/filters/decrypting_demuxer_stream.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if !BUILDFLAG(IS_ANDROID)
#include "media/filters/decrypting_audio_decoder.h"
#include "media/filters/decrypting_video_decoder.h"
#endif  // !BUILDFLAG(IS_ANDROID)

using ::base::test::RunCallback;
using ::base::test::RunOnceCallback;
using ::base::test::RunOnceCallbackRepeatedly;
using ::testing::_;
using ::testing::AnyNumber;
using ::testing::IsNull;
using ::testing::NiceMock;
using ::testing::NotNull;
using ::testing::Return;
using ::testing::StrictMock;

namespace media {

namespace {

enum DecryptorCapability {
  kNoDecryptor,
  kDecryptOnly,
  kDecryptAndDecode,
};

enum DecoderCapability {
  kAlwaysFail,
  kClearOnly,
  kEncryptedOnly,
  kAlwaysSucceed,
  kTooManyDecoders,
};

bool DecoderCapabilitySupportsDecryption(DecoderCapability capability) {
  switch (capability) {
    case kAlwaysFail:
      return false;
    case kClearOnly:
      return false;
    case kEncryptedOnly:
      return true;
    case kAlwaysSucceed:
      return true;
    case kTooManyDecoders:
      return true;
  }
}

DecoderStatus IsConfigSupported(DecoderCapability capability,
                                bool is_encrypted) {
  switch (capability) {
    case kAlwaysFail:
      return DecoderStatus::Codes::kFailed;
    case kClearOnly:
      return is_encrypted ? DecoderStatus::Codes::kUnsupportedEncryptionMode
                          : DecoderStatus::Codes::kOk;
    case kEncryptedOnly:
      return is_encrypted ? DecoderStatus::Codes::kOk
                          : DecoderStatus::Codes::kUnsupportedEncryptionMode;
    case kAlwaysSucceed:
      return DecoderStatus::Codes::kOk;
    case kTooManyDecoders:
      return DecoderStatus::Codes::kTooManyDecoders;
  }
}

const int kDecoder1 = 0xabc;
const int kDecoder2 = 0xdef;
const int kDecoder3 = 0x123;
const int kDecoder4 = 0x456;

// Specializations for the AUDIO version of the test.
class AudioDecoderSelectorTestParam {
 public:
  static constexpr DemuxerStream::Type kStreamType = DemuxerStream::AUDIO;
  using StreamTraits = DecoderStreamTraits<DemuxerStream::AUDIO>;
  using MockDecoder = MockAudioDecoder;
  using Output = AudioBuffer;
  using DecoderType = AudioDecoderType;

#if !BUILDFLAG(IS_ANDROID)
  using DecryptingDecoder = DecryptingAudioDecoder;
#endif  // !BUILDFLAG(IS_ANDROID)

  // StreamTraits() takes different parameters depending on the type.
  static std::unique_ptr<StreamTraits> CreateStreamTraits(MediaLog* media_log) {
    return std::make_unique<StreamTraits>(
        media_log, ChannelLayoutConfig::Stereo(), kSampleFormatPlanarF32);
  }

  static void UseNormalClearDecoderConfig(
      StrictMock<MockDemuxerStream>& stream) {
    stream.set_audio_decoder_config(TestAudioConfig::Normal());
  }
  static void UseNormalEncryptedDecoderConfig(
      StrictMock<MockDemuxerStream>& stream) {
    stream.set_audio_decoder_config(TestAudioConfig::NormalEncrypted());
  }

  // Decoder::Initialize() takes different parameters depending on the type.
  static void ExpectInitialize(MockDecoder* decoder,
                               DecoderCapability capability) {
    EXPECT_CALL(*decoder, Initialize_(_, _, _, _, _))
        .WillRepeatedly([capability](const AudioDecoderConfig& config,
                                     CdmContext*, AudioDecoder::InitCB& init_cb,
                                     const AudioDecoder::OutputCB&,
                                     const WaitingCB&) {
          std::move(init_cb).Run(
              IsConfigSupported(capability, config.is_encrypted()));
        });
  }

  static void ExpectNotInitialize(MockDecoder* decoder) {
    EXPECT_CALL(*decoder, Initialize_(_, _, _, _, _)).Times(0);
  }

  static void ExpectInitializeDeferred(MockDecoder* decoder,
                                       AudioDecoder::InitCB* captured_init_cb,
                                       base::RepeatingClosure on_initialize) {
    EXPECT_CALL(*decoder, Initialize_(_, _, _, _, _))
        .WillRepeatedly([captured_init_cb, on_initialize](
                            const AudioDecoderConfig&, CdmContext*,
                            AudioDecoder::InitCB& init_cb,
                            const AudioDecoder::OutputCB&, const WaitingCB&) {
          *captured_init_cb = std::move(init_cb);
          on_initialize.Run();
        });
  }
};

// Allocate storage for the member variables.
constexpr DemuxerStream::Type AudioDecoderSelectorTestParam::kStreamType;

// Specializations for the VIDEO version of the test.
class VideoDecoderSelectorTestParam {
 public:
  static constexpr DemuxerStream::Type kStreamType = DemuxerStream::VIDEO;

  using StreamTraits = DecoderStreamTraits<DemuxerStream::VIDEO>;
  using MockDecoder = MockVideoDecoder;
  using Output = VideoFrame;
  using DecoderType = VideoDecoderType;

#if !BUILDFLAG(IS_ANDROID)
  using DecryptingDecoder = DecryptingVideoDecoder;
#endif  // !BUILDFLAG(IS_ANDROID)

  static std::unique_ptr<StreamTraits> CreateStreamTraits(MediaLog* media_log) {
    return std::make_unique<StreamTraits>(media_log);
  }

  static void UseNormalClearDecoderConfig(
      StrictMock<MockDemuxerStream>& stream) {
    stream.set_video_decoder_config(TestVideoConfig::Normal());
  }
  static void UseNormalEncryptedDecoderConfig(
      StrictMock<MockDemuxerStream>& stream) {
    stream.set_video_decoder_config(TestVideoConfig::NormalEncrypted());
  }

  static void ExpectInitialize(MockDecoder* decoder,
                               DecoderCapability capability) {
    EXPECT_CALL(*decoder, Initialize_(_, _, _, _, _, _))
        .WillRepeatedly(
            [capability](const VideoDecoderConfig& config, bool low_delay,
                         CdmContext*, VideoDecoder::InitCB& init_cb,
                         const VideoDecoder::OutputCB&, const WaitingCB&) {
              std::move(init_cb).Run(
                  IsConfigSupported(capability, config.is_encrypted()));
            });
  }

  static void ExpectNotInitialize(MockDecoder* decoder) {
    EXPECT_CALL(*decoder, Initialize_(_, _, _, _, _, _)).Times(0);
  }

  static void ExpectInitializeDeferred(MockDecoder* decoder,
                                       VideoDecoder::InitCB* captured_init_cb,
                                       base::RepeatingClosure on_initialize) {
    EXPECT_CALL(*decoder, Initialize_(_, _, _, _, _, _))
        .WillRepeatedly([captured_init_cb, on_initialize](
                            const VideoDecoderConfig&, bool, CdmContext*,
                            VideoDecoder::InitCB& init_cb,
                            const VideoDecoder::OutputCB&, const WaitingCB&) {
          *captured_init_cb = std::move(init_cb);
          on_initialize.Run();
        });
  }
};

// Mock decoder wrapper that tracks live instances to verify synchronous
// destruction when DecoderSelector is destroyed.
template <typename BaseMockDecoder>
class LiveTrackingMockDecoder : public BaseMockDecoder {
 public:
  LiveTrackingMockDecoder(bool is_platform_decoder,
                          bool supports_decryption,
                          int decoder_id,
                          int* live_count)
      : BaseMockDecoder(is_platform_decoder, supports_decryption, decoder_id),
        live_count_(live_count) {
    ++(*live_count_);
  }

  ~LiveTrackingMockDecoder() override { --(*live_count_); }

 private:
  raw_ptr<int> live_count_;
};

// CallbackRegistration wrapper that notifies when unregistered while still
// delegating to a real CallbackRegistry registration.
class TrackingCallbackRegistration : public CallbackRegistration {
 public:
  TrackingCallbackRegistration(std::unique_ptr<CallbackRegistration> inner,
                               bool* destroyed)
      : inner_(std::move(inner)), destroyed_(destroyed) {}

  ~TrackingCallbackRegistration() override {
    inner_.reset();
    *destroyed_ = true;
  }

 private:
  std::unique_ptr<CallbackRegistration> inner_;
  raw_ptr<bool> destroyed_;
};

// Allocate storate for the member variables.
constexpr DemuxerStream::Type VideoDecoderSelectorTestParam::kStreamType;

}  // namespace

// Note: The parameter is called TypeParam in the test cases regardless of what
// we call it here. It's been named the same for convenience.
// Note: The test fixtures inherit from this class. Inside the test cases the
// test fixture class is called TestFixture.
template <typename TypeParam>
class DecoderSelectorTest : public ::testing::Test {
 public:
  // Convenience aliases.
  using Self = DecoderSelectorTest<TypeParam>;
  using StreamTraits = typename TypeParam::StreamTraits;
  using Decoder = typename StreamTraits::DecoderType;
  using MockDecoder = typename TypeParam::MockDecoder;
  using Output = typename TypeParam::Output;
  using DecoderType = typename TypeParam::DecoderType;
  using Selector = DecoderSelector<TypeParam::kStreamType>;

  struct MockDecoderArgs {
    static MockDecoderArgs Create(int decoder_id,
                                  DecoderCapability capability) {
      MockDecoderArgs result;
      result.decoder_id = decoder_id;
      result.capability = capability;
      result.supports_decryption =
          DecoderCapabilitySupportsDecryption(capability);
      result.is_platform_decoder = false;
      result.expect_not_initialized = false;
      return result;
    }

    int decoder_id;
    DecoderCapability capability;
    bool supports_decryption;
    bool is_platform_decoder;
    bool expect_not_initialized;
  };

  DecoderSelectorTest()
      : traits_(TypeParam::CreateStreamTraits(&media_log_)),
        demuxer_stream_(TypeParam::kStreamType) {}

  DecoderSelectorTest(const DecoderSelectorTest&) = delete;
  DecoderSelectorTest& operator=(const DecoderSelectorTest&) = delete;

  void OnWaiting(WaitingReason reason) { NOTREACHED(); }
  void OnOutput(scoped_refptr<Output> output) { NOTREACHED(); }

  MOCK_METHOD1_T(NoDecoderSelected, void(DecoderStatus::Codes));
  MOCK_METHOD1_T(OnDecoderSelected, void(int));
  MOCK_METHOD1_T(OnDecoderSelected, void(DecoderType));
  MOCK_METHOD1_T(OnDemuxerStreamSelected,
                 void(std::unique_ptr<DecryptingDemuxerStream>));

  void OnDecoderSelectedThunk(
      typename Selector::DecoderOrError decoder,
      std::unique_ptr<DecryptingDemuxerStream> decrypting_demuxer_stream) {
    // Report only the type or id of the decoder, since that's what the tests
    // care about. The decoder will be destructed immediately.
    if (decoder.has_value() &&
        decoder.value()->GetDecoderType() == DecoderType::kTesting) {
      OnDecoderSelected(
          static_cast<MockDecoder*>(std::move(decoder).value().get())
              ->GetDecoderId());
    } else if (decoder.has_value()) {
      OnDecoderSelected(decoder.value()->GetDecoderType());
    } else {
      NoDecoderSelected(std::move(decoder).error().code());
    }

    if (decrypting_demuxer_stream)
      OnDemuxerStreamSelected(std::move(decrypting_demuxer_stream));
  }

  void AddDecryptingDecoder() {
    // Require the DecryptingDecoder to be first, because that's easier to
    // implement.
    DCHECK(mock_decoders_to_create_.empty());
    DCHECK(!use_decrypting_decoder_);
    use_decrypting_decoder_ = true;
  }

  void AddMockDecoder(int decoder_id, DecoderCapability capability) {
    auto args = MockDecoderArgs::Create(decoder_id, capability);
    AddMockDecoder(std::move(args));
  }

  void AddMockPlatformDecoder(int decoder_id, DecoderCapability capability) {
    auto args = MockDecoderArgs::Create(std::move(decoder_id), capability);
    args.is_platform_decoder = true;
    AddMockDecoder(std::move(args));
  }

  void AddMockDecoder(MockDecoderArgs args) {
    // Actual decoders are created in CreateDecoders(), which may be called
    // multiple times by the DecoderSelector.
    mock_decoders_to_create_.push_back(std::move(args));
  }

  std::vector<std::unique_ptr<Decoder>> CreateDecoders() {
    if (custom_create_decoders_cb_) {
      return custom_create_decoders_cb_.Run();
    }

    std::vector<std::unique_ptr<Decoder>> decoders;

#if !BUILDFLAG(IS_ANDROID)
    if (use_decrypting_decoder_) {
      decoders.push_back(
          std::make_unique<typename TypeParam::DecryptingDecoder>(
              task_environment_.GetMainThreadTaskRunner(), &media_log_));
    }
#endif  // !BUILDFLAG(IS_ANDROID)

    for (const auto& args : mock_decoders_to_create_) {
      std::unique_ptr<StrictMock<MockDecoder>> decoder =
          std::make_unique<StrictMock<MockDecoder>>(args.is_platform_decoder,
                                                    args.supports_decryption,
                                                    args.decoder_id);
      if (args.expect_not_initialized) {
        TypeParam::ExpectNotInitialize(decoder.get());
      } else {
        TypeParam::ExpectInitialize(decoder.get(), args.capability);
      }
      decoders.push_back(std::move(decoder));
    }

    return decoders;
  }

  void CreateCdmContext(DecryptorCapability capability) {
    DCHECK(!decoder_selector_);

    cdm_context_ = std::make_unique<StrictMock<MockCdmContext>>();

    EXPECT_CALL(*cdm_context_, RegisterEventCB(_)).Times(AnyNumber());

    if (capability == kNoDecryptor) {
      EXPECT_CALL(*cdm_context_, GetDecryptor())
          .WillRepeatedly(Return(nullptr));
      return;
    }

    decryptor_ = std::make_unique<NiceMock<MockDecryptor>>();
    EXPECT_CALL(*cdm_context_, GetDecryptor())
        .WillRepeatedly(Return(decryptor_.get()));
    switch (TypeParam::kStreamType) {
      case DemuxerStream::AUDIO:
        EXPECT_CALL(*decryptor_, InitializeAudioDecoder(_, _))
            .WillRepeatedly(
                RunOnceCallbackRepeatedly<1>(capability == kDecryptAndDecode));
        break;
      case DemuxerStream::VIDEO:
        EXPECT_CALL(*decryptor_, InitializeVideoDecoder(_, _))
            .WillRepeatedly(
                RunOnceCallbackRepeatedly<1>(capability == kDecryptAndDecode));
        break;
      default:
        NOTREACHED();
    }
  }

  void CreateDecoderSelector(bool supports_config_changes = false,
                             bool enable_priority_based_selection = true) {
    EXPECT_CALL(demuxer_stream_, SupportsConfigChanges())
        .WillRepeatedly(Return(supports_config_changes));
    decoder_selector_ = std::make_unique<Selector>(
        task_environment_.GetMainThreadTaskRunner(),
        base::BindRepeating(&Self::CreateDecoders, base::Unretained(this)),
        &media_log_, enable_priority_based_selection);
    decoder_selector_->Initialize(
        traits_.get(), &demuxer_stream_, cdm_context_.get(),
        base::BindRepeating(&Self::OnWaiting, base::Unretained(this)));
  }

  void UseClearDecoderConfig() {
    TypeParam::UseNormalClearDecoderConfig(demuxer_stream_);
  }
  void UseEncryptedDecoderConfig() {
    TypeParam::UseNormalEncryptedDecoderConfig(demuxer_stream_);
  }

  void SelectNextDecoder() {
    if (is_selecting_) {
      decoder_selector_->ResumeDecoderSelection(
          base::BindOnce(&Self::OnDecoderSelectedThunk, base::Unretained(this)),
          base::BindRepeating(&Self::OnOutput, base::Unretained(this)),
          DecoderStatus::Codes::kFailed);
    } else {
      decoder_selector_->BeginDecoderSelection(
          base::BindOnce(&Self::OnDecoderSelectedThunk, base::Unretained(this)),
          base::BindRepeating(&Self::OnOutput, base::Unretained(this)));
    }
    is_selecting_ = true;
    RunUntilIdle();
  }

  void FinalizeDecoderSelection() {
    decoder_selector_->FinalizeDecoderSelection();
    is_selecting_ = false;
  }

  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

  base::test::TaskEnvironment task_environment_;
  NullMediaLog media_log_;

  std::unique_ptr<StreamTraits> traits_;
  StrictMock<MockDemuxerStream> demuxer_stream_;
  std::unique_ptr<StrictMock<MockCdmContext>> cdm_context_;
  std::unique_ptr<NiceMock<MockDecryptor>> decryptor_;

  std::unique_ptr<Selector> decoder_selector_;

  bool use_decrypting_decoder_ = false;
  bool is_selecting_ = false;
  std::vector<MockDecoderArgs> mock_decoders_to_create_;
  typename Selector::CreateDecodersCB custom_create_decoders_cb_;
  base::WeakPtrFactory<Self> weak_factory_{this};
};

using VideoDecoderSelectorTest =
    DecoderSelectorTest<VideoDecoderSelectorTestParam>;

using DecoderSelectorTestParams =
    ::testing::Types<AudioDecoderSelectorTestParam,
                     VideoDecoderSelectorTestParam>;
TYPED_TEST_SUITE(DecoderSelectorTest, DecoderSelectorTestParams);

// Tests for clear streams. CDM will not be used for clear streams so
// DecryptorCapability doesn't really matter.

TYPED_TEST(DecoderSelectorTest, ClearStream_NoDecoders) {
  this->UseClearDecoderConfig();
  this->CreateDecoderSelector();

  EXPECT_CALL(*this,
              NoDecoderSelected(DecoderStatus::Codes::kUnsupportedConfig));
  this->SelectNextDecoder();
}

TYPED_TEST(DecoderSelectorTest, ClearStream_TooManyDecoders) {
  this->AddMockDecoder(kDecoder1, kTooManyDecoders);
  this->UseClearDecoderConfig();
  this->CreateDecoderSelector();

  EXPECT_CALL(*this, NoDecoderSelected(DecoderStatus::Codes::kTooManyDecoders));
  this->SelectNextDecoder();
}

TYPED_TEST(DecoderSelectorTest, ClearStream_NoClearDecoder) {
  this->AddDecryptingDecoder();
  this->UseClearDecoderConfig();
  this->CreateDecoderSelector();

  EXPECT_CALL(*this,
              NoDecoderSelected(DecoderStatus::Codes::kUnsupportedConfig));
  this->SelectNextDecoder();
}

TYPED_TEST(DecoderSelectorTest, ClearStream_OneClearDecoder) {
  this->AddMockDecoder(kDecoder1, kClearOnly);
  this->UseClearDecoderConfig();
  this->CreateDecoderSelector();

  EXPECT_CALL(*this, OnDecoderSelected(kDecoder1));
  this->SelectNextDecoder();
}

TYPED_TEST(DecoderSelectorTest, ClearStream_InternalFallback) {
  this->AddMockDecoder(kDecoder1, kAlwaysFail);
  this->AddMockDecoder(kDecoder2, kClearOnly);
  this->UseClearDecoderConfig();
  this->CreateDecoderSelector();

  EXPECT_CALL(*this, OnDecoderSelected(kDecoder2));
  this->SelectNextDecoder();
}

TYPED_TEST(DecoderSelectorTest, ClearStream_ExternalFallback) {
  this->AddMockDecoder(kDecoder1, kClearOnly);
  this->AddMockDecoder(kDecoder2, kClearOnly);
  this->UseClearDecoderConfig();
  this->CreateDecoderSelector();

  EXPECT_CALL(*this, OnDecoderSelected(kDecoder1));
  this->SelectNextDecoder();

  EXPECT_CALL(*this, OnDecoderSelected(kDecoder2));
  this->SelectNextDecoder();

  EXPECT_CALL(*this, NoDecoderSelected(DecoderStatus::Codes::kFailed));
  this->SelectNextDecoder();
}

TYPED_TEST(DecoderSelectorTest, ClearStream_FinalizeDecoderSelection) {
  this->AddMockDecoder(kDecoder1, kClearOnly);
  this->AddMockDecoder(kDecoder2, kClearOnly);
  this->UseClearDecoderConfig();
  this->CreateDecoderSelector();

  EXPECT_CALL(*this, OnDecoderSelected(kDecoder1));
  this->SelectNextDecoder();

  this->FinalizeDecoderSelection();

  EXPECT_CALL(*this, OnDecoderSelected(kDecoder1));
  this->SelectNextDecoder();
}

// Tests the production predicate for `DecoderSelector<DemuxerStream::VIDEO>`
TEST_F(VideoDecoderSelectorTest, ClearStream_PrioritizeSoftwareDecoders) {
  this->AddMockPlatformDecoder(kDecoder1, kClearOnly);
  this->AddMockDecoder(kDecoder2, kClearOnly);
  this->AddMockPlatformDecoder(kDecoder3, kAlwaysSucceed);
  this->AddMockDecoder(kDecoder4, kAlwaysSucceed);

  // Create a clear config that will cause software decoders to be
  // prioritized on any platform.
  this->demuxer_stream_.set_video_decoder_config(
      TestVideoConfig::Custom(gfx::Size(64, 64)));
  this->CreateDecoderSelector();

  EXPECT_CALL(*this, OnDecoderSelected(kDecoder2));
  this->SelectNextDecoder();
  EXPECT_CALL(*this, OnDecoderSelected(kDecoder4));
  this->SelectNextDecoder();
  EXPECT_CALL(*this, OnDecoderSelected(kDecoder1));
  this->SelectNextDecoder();
  EXPECT_CALL(*this, OnDecoderSelected(kDecoder3));
  this->SelectNextDecoder();
  EXPECT_CALL(*this, NoDecoderSelected(DecoderStatus::Codes::kFailed));
  this->SelectNextDecoder();
}

// Test decoders are not prioritized when config changes are supported.
TEST_F(VideoDecoderSelectorTest,
       ClearStream_PrioritizeSoftwareDecodersDisabled) {
  this->AddMockPlatformDecoder(kDecoder1, kClearOnly);
  this->AddMockDecoder(kDecoder2, kClearOnly);
  this->AddMockPlatformDecoder(kDecoder3, kAlwaysSucceed);
  this->AddMockDecoder(kDecoder4, kAlwaysSucceed);

  // Create a clear config that will cause software decoders to be
  // prioritized on any platform.
  this->demuxer_stream_.set_video_decoder_config(
      TestVideoConfig::Custom(gfx::Size(64, 64)));
  this->CreateDecoderSelector(/*supports_config_changes=*/true,
                              /*enable_priority_based_selection=*/true);

  EXPECT_CALL(*this, OnDecoderSelected(kDecoder1));
  this->SelectNextDecoder();
  EXPECT_CALL(*this, OnDecoderSelected(kDecoder2));
  this->SelectNextDecoder();
  EXPECT_CALL(*this, OnDecoderSelected(kDecoder3));
  this->SelectNextDecoder();
  EXPECT_CALL(*this, OnDecoderSelected(kDecoder4));
  this->SelectNextDecoder();
  EXPECT_CALL(*this, NoDecoderSelected(DecoderStatus::Codes::kFailed));
  this->SelectNextDecoder();
}

TEST_F(VideoDecoderSelectorTest,
       ClearStream_PrioritizeSoftwareDecodersFullyDisabled) {
  this->AddMockPlatformDecoder(kDecoder1, kClearOnly);
  this->AddMockDecoder(kDecoder2, kClearOnly);
  this->AddMockPlatformDecoder(kDecoder3, kAlwaysSucceed);
  this->AddMockDecoder(kDecoder4, kAlwaysSucceed);

  // Create a clear config that will cause software decoders to be
  // prioritized on any platform.
  this->demuxer_stream_.set_video_decoder_config(
      TestVideoConfig::Custom(gfx::Size(64, 64)));
  this->CreateDecoderSelector(/*supports_config_changes=*/false,
                              /*enable_priority_based_selection=*/false);

  EXPECT_CALL(*this, OnDecoderSelected(kDecoder1));
  this->SelectNextDecoder();
  EXPECT_CALL(*this, OnDecoderSelected(kDecoder2));
  this->SelectNextDecoder();
  EXPECT_CALL(*this, OnDecoderSelected(kDecoder3));
  this->SelectNextDecoder();
  EXPECT_CALL(*this, OnDecoderSelected(kDecoder4));
  this->SelectNextDecoder();
  EXPECT_CALL(*this, NoDecoderSelected(DecoderStatus::Codes::kFailed));
  this->SelectNextDecoder();
}

// Tests the production predicate for `DecoderSelector<DemuxerStream::VIDEO>`
TEST_F(VideoDecoderSelectorTest, ClearStream_PrioritizePlatformDecoders) {
  this->AddMockPlatformDecoder(kDecoder1, kClearOnly);
  this->AddMockDecoder(kDecoder2, kClearOnly);
  this->AddMockPlatformDecoder(kDecoder3, kAlwaysSucceed);
  this->AddMockDecoder(kDecoder4, kAlwaysSucceed);

  // Create a clear config that will cause hardware decoders to be prioritized
  // on any platform.
  this->demuxer_stream_.set_video_decoder_config(
      TestVideoConfig::Custom(gfx::Size(4096, 4096)));
  this->CreateDecoderSelector();

  EXPECT_CALL(*this, OnDecoderSelected(kDecoder1));
  this->SelectNextDecoder();
  EXPECT_CALL(*this, OnDecoderSelected(kDecoder3));
  this->SelectNextDecoder();
  EXPECT_CALL(*this, OnDecoderSelected(kDecoder2));
  this->SelectNextDecoder();
  EXPECT_CALL(*this, OnDecoderSelected(kDecoder4));
  this->SelectNextDecoder();
  EXPECT_CALL(*this, NoDecoderSelected(DecoderStatus::Codes::kFailed));
  this->SelectNextDecoder();
}

// Tests for encrypted streams.

// Tests that non-decrypting decoders are filtered out by DecoderSelector
// before being initialized.
TYPED_TEST(DecoderSelectorTest,
           EncryptedStream_NoDecryptor_DecodersNotInitialized) {
  using MockDecoderArgs =
      typename DecoderSelectorTest<TypeParam>::MockDecoderArgs;

  auto args = MockDecoderArgs::Create(kDecoder1, kClearOnly);
  args.expect_not_initialized = true;
  this->AddMockDecoder(std::move(args));

  args = MockDecoderArgs::Create(kDecoder2, kClearOnly);
  args.expect_not_initialized = true;
  this->AddMockDecoder(std::move(args));

  this->UseEncryptedDecoderConfig();
  this->CreateDecoderSelector();

  EXPECT_CALL(*this,
              NoDecoderSelected(DecoderStatus::Codes::kUnsupportedConfig));
  this->SelectNextDecoder();
}

TYPED_TEST(DecoderSelectorTest, EncryptedStream_NoDecryptor_OneClearDecoder) {
  this->AddMockDecoder(kDecoder1, kClearOnly);
  this->CreateCdmContext(kNoDecryptor);
  this->UseEncryptedDecoderConfig();
  this->CreateDecoderSelector();

  EXPECT_CALL(*this, NoDecoderSelected(
                         DecoderStatus::Codes::kUnsupportedEncryptionMode));
  this->SelectNextDecoder();
}

TYPED_TEST(DecoderSelectorTest, EncryptedStream_NoDecryptor_InternalFallback) {
  this->AddMockDecoder(kDecoder1, kClearOnly);
  this->AddMockDecoder(kDecoder2, kEncryptedOnly);
  this->CreateCdmContext(kNoDecryptor);
  this->UseEncryptedDecoderConfig();
  this->CreateDecoderSelector();

  EXPECT_CALL(*this, OnDecoderSelected(kDecoder2));
  this->SelectNextDecoder();
}

TYPED_TEST(DecoderSelectorTest, EncryptedStream_NoDecryptor_ExternalFallback) {
  this->AddMockDecoder(kDecoder1, kEncryptedOnly);
  this->AddMockDecoder(kDecoder2, kEncryptedOnly);
  this->CreateCdmContext(kNoDecryptor);
  this->UseEncryptedDecoderConfig();
  this->CreateDecoderSelector();

  EXPECT_CALL(*this, OnDecoderSelected(kDecoder1));
  this->SelectNextDecoder();

  EXPECT_CALL(*this, OnDecoderSelected(kDecoder2));
  this->SelectNextDecoder();
}

TYPED_TEST(DecoderSelectorTest,
           EncryptedStream_NoDecryptor_FinalizeDecoderSelection) {
  this->AddMockDecoder(kDecoder1, kEncryptedOnly);
  this->AddMockDecoder(kDecoder2, kEncryptedOnly);
  this->CreateCdmContext(kNoDecryptor);
  this->UseEncryptedDecoderConfig();
  this->CreateDecoderSelector();

  EXPECT_CALL(*this, OnDecoderSelected(kDecoder1));
  this->SelectNextDecoder();

  this->FinalizeDecoderSelection();

  EXPECT_CALL(*this, OnDecoderSelected(kDecoder1));
  this->SelectNextDecoder();
}

TYPED_TEST(DecoderSelectorTest, EncryptedStream_DecryptOnly_NoDecoder) {
  this->CreateCdmContext(kDecryptOnly);
  this->UseEncryptedDecoderConfig();
  this->CreateDecoderSelector();

  EXPECT_CALL(*this,
              NoDecoderSelected(DecoderStatus::Codes::kUnsupportedConfig));
  this->SelectNextDecoder();
}

TYPED_TEST(DecoderSelectorTest, EncryptedStream_DecryptOnly_OneClearDecoder) {
  this->AddMockDecoder(kDecoder1, kClearOnly);
  this->CreateCdmContext(kDecryptOnly);
  this->UseEncryptedDecoderConfig();
  this->CreateDecoderSelector();

  EXPECT_CALL(*this, OnDecoderSelected(kDecoder1));
  EXPECT_CALL(*this, OnDemuxerStreamSelected(NotNull()));
  this->SelectNextDecoder();
}

TYPED_TEST(DecoderSelectorTest, EncryptedStream_DecryptOnly_InternalFallback) {
  this->AddMockDecoder(kDecoder1, kAlwaysFail);
  this->AddMockDecoder(kDecoder2, kClearOnly);
  this->CreateCdmContext(kDecryptOnly);
  this->UseEncryptedDecoderConfig();
  this->CreateDecoderSelector();

  EXPECT_CALL(*this, OnDecoderSelected(kDecoder2));
  EXPECT_CALL(*this, OnDemuxerStreamSelected(NotNull()));

  this->SelectNextDecoder();
}

TYPED_TEST(DecoderSelectorTest,
           EncryptedStream_DecryptOnly_FinalizeDecoderSelection) {
  this->AddMockDecoder(kDecoder1, kClearOnly);
  this->AddMockDecoder(kDecoder2, kClearOnly);
  this->CreateCdmContext(kDecryptOnly);
  this->UseEncryptedDecoderConfig();
  this->CreateDecoderSelector();

  std::unique_ptr<DecryptingDemuxerStream> saved_dds;
  EXPECT_CALL(*this, OnDecoderSelected(kDecoder1));
  EXPECT_CALL(*this, OnDemuxerStreamSelected(NotNull()))
      .WillOnce([&](std::unique_ptr<DecryptingDemuxerStream> dds) {
        saved_dds = std::move(dds);
      });

  this->SelectNextDecoder();

  this->FinalizeDecoderSelection();

  // DDS is reused.
  EXPECT_CALL(*this, OnDecoderSelected(kDecoder1));
  this->SelectNextDecoder();
}

TYPED_TEST(DecoderSelectorTest, EncryptedStream_DecryptAndDecode) {
  this->AddDecryptingDecoder();
  this->AddMockDecoder(kDecoder1, kClearOnly);
  this->CreateCdmContext(kDecryptAndDecode);
  this->UseEncryptedDecoderConfig();
  this->CreateDecoderSelector();

#if !BUILDFLAG(IS_ANDROID)
  // A DecryptingVideoDecoder will be created and selected. The clear decoder
  // should not be touched at all. No DecryptingDemuxerStream should be
  // created.
  EXPECT_CALL(*this, OnDecoderSelected(TestFixture::DecoderType::kDecrypting));
#else
  // A DecryptingDemuxerStream will be created. The clear decoder will be
  // initialized and returned.
  EXPECT_CALL(*this, OnDecoderSelected(kDecoder1));
  EXPECT_CALL(*this, OnDemuxerStreamSelected(NotNull()));
#endif  // !BUILDFLAG(IS_ANDROID)

  this->SelectNextDecoder();
}

TYPED_TEST(DecoderSelectorTest,
           EncryptedStream_DecryptAndDecode_ExternalFallback) {
  this->AddDecryptingDecoder();
  this->AddMockDecoder(kDecoder1, kClearOnly);
  this->AddMockDecoder(kDecoder2, kClearOnly);
  this->CreateCdmContext(kDecryptAndDecode);
  this->UseEncryptedDecoderConfig();
  this->CreateDecoderSelector();

#if !BUILDFLAG(IS_ANDROID)
  // DecryptingDecoder is selected immediately.
  EXPECT_CALL(*this, OnDecoderSelected(TestFixture::DecoderType::kDecrypting));
  this->SelectNextDecoder();
#endif  // !BUILDFLAG(IS_ANDROID)

  // On fallback, a DecryptingDemuxerStream will be created.
  std::unique_ptr<DecryptingDemuxerStream> saved_dds;
  EXPECT_CALL(*this, OnDecoderSelected(kDecoder1));
  EXPECT_CALL(*this, OnDemuxerStreamSelected(NotNull()))
      .WillOnce([&](std::unique_ptr<DecryptingDemuxerStream> dds) {
        saved_dds = std::move(dds);
      });
  this->SelectNextDecoder();

  // The DecryptingDemuxerStream should be reused.
  EXPECT_CALL(*this, OnDecoderSelected(kDecoder2));
  this->SelectNextDecoder();
}

TYPED_TEST(DecoderSelectorTest, ClearToEncryptedStream_DecryptOnly) {
  this->AddMockDecoder(kDecoder1, kClearOnly);
  this->CreateCdmContext(kDecryptOnly);
  this->UseClearDecoderConfig();
  this->CreateDecoderSelector();

  EXPECT_CALL(*this, OnDecoderSelected(kDecoder1));
  this->SelectNextDecoder();

  this->FinalizeDecoderSelection();
  this->UseEncryptedDecoderConfig();

  EXPECT_CALL(*this, OnDecoderSelected(kDecoder1));
  EXPECT_CALL(*this, OnDemuxerStreamSelected(NotNull()));
  this->SelectNextDecoder();
}

// Tests the production predicate for `DecoderSelector<DemuxerStream::VIDEO>`
TEST_F(VideoDecoderSelectorTest, EncryptedStream_PrioritizeSoftwareDecoders) {
  this->AddMockPlatformDecoder(kDecoder1, kClearOnly);
  this->AddMockDecoder(kDecoder2, kClearOnly);
  this->AddMockPlatformDecoder(kDecoder3, kAlwaysSucceed);
  this->AddMockDecoder(kDecoder4, kAlwaysSucceed);

  // Create an encrypted config that will cause software decoders to be
  // prioritized on any platform.
  this->demuxer_stream_.set_video_decoder_config(
      TestVideoConfig::CustomEncrypted(gfx::Size(64, 64)));
  this->CreateDecoderSelector();

  EXPECT_CALL(*this, OnDecoderSelected(kDecoder4));
  this->SelectNextDecoder();
  EXPECT_CALL(*this, OnDecoderSelected(kDecoder3));
  this->SelectNextDecoder();
  EXPECT_CALL(*this, NoDecoderSelected(DecoderStatus::Codes::kFailed));
  this->SelectNextDecoder();
}

// Tests the production predicate for `DecoderSelector<DemuxerStream::VIDEO>`
TEST_F(VideoDecoderSelectorTest, EncryptedStream_PrioritizePlatformDecoders) {
  this->AddMockPlatformDecoder(kDecoder1, kClearOnly);
  this->AddMockDecoder(kDecoder2, kClearOnly);
  this->AddMockPlatformDecoder(kDecoder3, kAlwaysSucceed);
  this->AddMockDecoder(kDecoder4, kAlwaysSucceed);

  // Create an encrypted config that will cause hardware decoders to be
  // prioritized on any platform.
  this->demuxer_stream_.set_video_decoder_config(
      TestVideoConfig::CustomEncrypted(gfx::Size(4096, 4096)));
  this->CreateDecoderSelector();

  EXPECT_CALL(*this, OnDecoderSelected(kDecoder3));
  this->SelectNextDecoder();
  EXPECT_CALL(*this, OnDecoderSelected(kDecoder4));
  this->SelectNextDecoder();
  EXPECT_CALL(*this, NoDecoderSelected(DecoderStatus::Codes::kFailed));
  this->SelectNextDecoder();
}

// Regression test for https://crbug.com/589800510: destroying DecoderSelector
// after a decoder finishes initializing (while delivery of SelectDecoderCB is
// still pending on the task runner) must synchronously destroy both the
// selected Decoder and DecryptingDemuxerStream so they cannot outlive
// CdmContext / CallbackRegistry.
TYPED_TEST(DecoderSelectorTest,
           DestroyWhileDeliveryPending_DecryptingDemuxerStream) {
  using Decoder = typename TestFixture::Decoder;
  using MockDecoder = typename TestFixture::MockDecoder;
  using TrackedDecoder = StrictMock<LiveTrackingMockDecoder<MockDecoder>>;

  int live_decoders = 0;
  typename Decoder::InitCB captured_init_cb;
  base::RunLoop init_run_loop;
  this->custom_create_decoders_cb_ =
      base::BindLambdaForTesting([&live_decoders, &captured_init_cb,
                                  quit_cb = init_run_loop.QuitClosure()]() {
        std::vector<std::unique_ptr<Decoder>> decoders;
        auto decoder = std::make_unique<TrackedDecoder>(
            /*is_platform_decoder=*/false, /*supports_decryption=*/false,
            kDecoder1, &live_decoders);
        TypeParam::ExpectInitializeDeferred(decoder.get(), &captured_init_cb,
                                            quit_cb);
        decoders.push_back(std::move(decoder));
        return decoders;
      });

  auto event_callbacks =
      std::make_unique<CallbackRegistry<CdmContext::EventCB::RunType>>();
  bool registration_destroyed = false;

  this->CreateCdmContext(kDecryptOnly);
  EXPECT_CALL(*this->cdm_context_, RegisterEventCB(_))
      .WillOnce([&](CdmContext::EventCB cb) {
        return std::make_unique<TrackingCallbackRegistration>(
            event_callbacks->Register(std::move(cb)), &registration_destroyed);
      });
  EXPECT_CALL(*this->decryptor_, CancelDecrypt(_));

  this->UseEncryptedDecoderConfig();
  this->CreateDecoderSelector();

  // Mirror DecoderStream, which binds OnDecoderSelected with a WeakPtr.
  this->decoder_selector_->BeginDecoderSelection(
      base::BindOnce(&TestFixture::OnDecoderSelectedThunk,
                     this->weak_factory_.GetWeakPtr()),
      base::BindRepeating(&TestFixture::OnOutput, base::Unretained(this)));
  init_run_loop.Run();

  // DecryptingDemuxerStream initialized and the clear decoder is awaiting
  // initialization completion.
  ASSERT_TRUE(captured_init_cb);
  EXPECT_EQ(live_decoders, 1);
  EXPECT_FALSE(registration_destroyed);

  // Complete decoder initialization; this posts delivery of the selected
  // decoder and DecryptingDemuxerStream to the task runner.
  std::move(captured_init_cb).Run(DecoderStatus::Codes::kOk);

  // Simulate pipeline teardown (e.g. ~DecoderStream) running before the posted
  // delivery task executes.
  this->weak_factory_.InvalidateWeakPtrs();
  this->decoder_selector_.reset();

  // Both the selected decoder and DecryptingDemuxerStream must already be
  // destroyed before CdmContext / CallbackRegistry are torn down.
  EXPECT_EQ(live_decoders, 0);
  EXPECT_TRUE(registration_destroyed);
  testing::Mock::VerifyAndClearExpectations(this->decryptor_.get());

  // Simulate DestructionHelper destroying CdmContext / AesDecryptor while the
  // cancelled delivery task is still queued on the media task runner.
  event_callbacks.reset();
  this->cdm_context_.reset();
  this->decryptor_.reset();

  // Drain the queued cancelled delivery task; it must not touch the destroyed
  // CallbackRegistry or Decryptor.
  base::RunLoop flush_run_loop;
  this->task_environment_.GetMainThreadTaskRunner()->PostTask(
      FROM_HERE, flush_run_loop.QuitClosure());
  flush_run_loop.Run();
}

// Verifies that destroying DecoderSelector while delivery of a selected
// decoder is pending destroys the decoder synchronously and aborts selection
// with kFailed if the caller's callback is still active.
TYPED_TEST(DecoderSelectorTest, DestroyWhileDeliveryPending_AbortsSelection) {
  using Decoder = typename TestFixture::Decoder;
  using MockDecoder = typename TestFixture::MockDecoder;
  using TrackedDecoder = StrictMock<LiveTrackingMockDecoder<MockDecoder>>;

  int live_decoders = 0;
  typename Decoder::InitCB captured_init_cb;
  base::RunLoop init_run_loop;
  this->custom_create_decoders_cb_ =
      base::BindLambdaForTesting([&live_decoders, &captured_init_cb,
                                  quit_cb = init_run_loop.QuitClosure()]() {
        std::vector<std::unique_ptr<Decoder>> decoders;
        auto decoder = std::make_unique<TrackedDecoder>(
            /*is_platform_decoder=*/false, /*supports_decryption=*/false,
            kDecoder1, &live_decoders);
        TypeParam::ExpectInitializeDeferred(decoder.get(), &captured_init_cb,
                                            quit_cb);
        decoders.push_back(std::move(decoder));
        return decoders;
      });

  this->UseClearDecoderConfig();
  this->CreateDecoderSelector();

  this->decoder_selector_->BeginDecoderSelection(
      base::BindOnce(&TestFixture::OnDecoderSelectedThunk,
                     base::Unretained(this)),
      base::BindRepeating(&TestFixture::OnOutput, base::Unretained(this)));
  init_run_loop.Run();

  ASSERT_TRUE(captured_init_cb);
  EXPECT_EQ(live_decoders, 1);

  std::move(captured_init_cb).Run(DecoderStatus::Codes::kOk);

  // Destroy DecoderSelector before the posted delivery task runs.
  this->decoder_selector_.reset();
  EXPECT_EQ(live_decoders, 0);

  base::RunLoop abort_run_loop;
  EXPECT_CALL(*this, NoDecoderSelected(DecoderStatus::Codes::kFailed))
      .WillOnce(base::test::RunClosure(abort_run_loop.QuitClosure()));
  abort_run_loop.Run();
}

}  // namespace media
