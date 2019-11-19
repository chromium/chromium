// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <tuple>

#include "base/bind.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "media/base/demuxer_stream.h"
#include "media/base/media_util.h"
#include "media/base/mock_filters.h"
#include "media/base/test_helpers.h"
#include "media/filters/decoder_selector.h"
#include "media/filters/decrypting_demuxer_stream.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if !defined(OS_ANDROID)
#include "media/filters/decrypting_audio_decoder.h"
#include "media/filters/decrypting_video_decoder.h"
#endif  // !defined(OS_ANDROID)

using ::base::test::RunCallback;
using ::testing::_;
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
};

bool IsConfigSupported(DecoderCapability capability, bool is_encrypted) {
  switch (capability) {
    case kAlwaysFail:
      return false;
    case kClearOnly:
      return !is_encrypted;
    case kEncryptedOnly:
      return is_encrypted;
    case kAlwaysSucceed:
      return true;
  }
}

const char kNoDecoder[] = "";
const char kDecoder1[] = "Decoder1";
const char kDecoder2[] = "Decoder2";

// Specializations for the AUDIO version of the test.
class AudioDecoderSelectorTestParam {
 public:
  static constexpr DemuxerStream::Type kStreamType = DemuxerStream::AUDIO;

  using StreamTraits = DecoderStreamTraits<DemuxerStream::AUDIO>;
  using MockDecoder = MockAudioDecoder;
  using Output = AudioBuffer;

#if !defined(OS_ANDROID)
  static constexpr char kDecryptingDecoder[] = "DecryptingAudioDecoder";
  using DecryptingDecoder = DecryptingAudioDecoder;
#endif  // !defined(OS_ANDROID)

  // StreamTraits() takes different parameters depending on the type.
  static std::unique_ptr<StreamTraits> CreateStreamTraits(MediaLog* media_log) {
    return std::make_unique<StreamTraits>(media_log, CHANNEL_LAYOUT_STEREO);
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
};

// Allocate storage for the member variables.
constexpr DemuxerStream::Type AudioDecoderSelectorTestParam::kStreamType;
#if !defined(OS_ANDROID)
constexpr char AudioDecoderSelectorTestParam::kDecryptingDecoder[];
#endif  // !defined(OS_ANDROID)

// Specializations for the VIDEO version of the test.
class VideoDecoderSelectorTestParam {
 public:
  static constexpr DemuxerStream::Type kStreamType = DemuxerStream::VIDEO;

  using StreamTraits = DecoderStreamTraits<DemuxerStream::VIDEO>;
  using MockDecoder = MockVideoDecoder;
  using Output = VideoFrame;

#if !defined(OS_ANDROID)
  static constexpr char kDecryptingDecoder[] = "DecryptingVideoDecoder";
  using DecryptingDecoder = DecryptingVideoDecoder;
#endif  // !defined(OS_ANDROID)

  static std::unique_ptr<StreamTraits> CreateStreamTraits(MediaLog* media_log) {
    return std::make_unique<StreamTraits>(media_log);
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
};

// Allocate storate for the member variables.
constexpr DemuxerStream::Type VideoDecoderSelectorTestParam::kStreamType;
#if !defined(OS_ANDROID)
constexpr char VideoDecoderSelectorTestParam::kDecryptingDecoder[];
#endif  // !defined(OS_ANDROID)

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

  DecoderSelectorTest()
      : traits_(TypeParam::CreateStreamTraits(&media_log_)),
        demuxer_stream_(TypeParam::kStreamType) {}

  void OnWaiting(WaitingReason reason) { NOTREACHED(); }
  void OnOutput(scoped_refptr<Output> output) { NOTREACHED(); }

  MOCK_METHOD2_T(OnDecoderSelected,
                 void(std::string, std::unique_ptr<DecryptingDemuxerStream>));

  void OnDecoderSelectedThunk(
      std::unique_ptr<Decoder> decoder,
      std::unique_ptr<DecryptingDemuxerStream> decrypting_demuxer_stream) {
    // Report only the name of the decoder, since that's what the tests care
    // about. The decoder will be destructed immediately.
    OnDecoderSelected(decoder ? decoder->GetDisplayName() : kNoDecoder,
                      std::move(decrypting_demuxer_stream));
  }

  void AddDecryptingDecoder() {
    // Require the DecryptingDecoder to be first, because that's easier to
    // implement.
    DCHECK(mock_decoders_to_create_.empty());
    DCHECK(!use_decrypting_decoder_);
    use_decrypting_decoder_ = true;
  }

  void AddMockDecoder(const std::string& decoder_name,
                      DecoderCapability capability) {
    // Actual decoders are created in CreateDecoders(), which may be called
    // multiple times by the DecoderSelector.
    mock_decoders_to_create_.emplace_back(decoder_name, capability);
  }

  std::vector<std::unique_ptr<Decoder>> CreateDecoders() {
    std::vector<std::unique_ptr<Decoder>> decoders;

#if !defined(OS_ANDROID)
    if (use_decrypting_decoder_) {
      decoders.push_back(
          std::make_unique<typename TypeParam::DecryptingDecoder>(
              task_environment_.GetMainThreadTaskRunner(), &media_log_));
    }
#endif  // !defined(OS_ANDROID)

    for (const auto& info : mock_decoders_to_create_) {
      std::unique_ptr<StrictMock<MockDecoder>> decoder =
          std::make_unique<StrictMock<MockDecoder>>(info.first);
      TypeParam::ExpectInitialize(decoder.get(), info.second);
      decoders.push_back(std::move(decoder));
    }

    return decoders;
  }

  void CreateCdmContext(DecryptorCapability capability) {
    DCHECK(!decoder_selector_);

    cdm_context_ = std::make_unique<StrictMock<MockCdmContext>>();

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
            .WillRepeatedly(RunCallback<1>(capability == kDecryptAndDecode));
        break;
      case DemuxerStream::VIDEO:
        EXPECT_CALL(*decryptor_, InitializeVideoDecoder(_, _))
            .WillRepeatedly(RunCallback<1>(capability == kDecryptAndDecode));
        break;
      default:
        NOTREACHED();
    }
  }

  void CreateDecoderSelector() {
    decoder_selector_ =
        std::make_unique<DecoderSelector<TypeParam::kStreamType>>(
            task_environment_.GetMainThreadTaskRunner(),
            base::BindRepeating(&Self::CreateDecoders, base::Unretained(this)),
            &media_log_);
    decoder_selector_->Initialize(
        traits_.get(), &demuxer_stream_, cdm_context_.get(),
        base::BindRepeating(&Self::OnWaiting, base::Unretained(this)));
  }

  void UseClearDecoderConfig() {
    switch (TypeParam::kStreamType) {
      case DemuxerStream::AUDIO:
        demuxer_stream_.set_audio_decoder_config(TestAudioConfig::Normal());
        break;
      case DemuxerStream::VIDEO:
        demuxer_stream_.set_video_decoder_config(TestVideoConfig::Normal());
        break;
      default:
        NOTREACHED();
    }
  }

  void UseEncryptedDecoderConfig() {
    switch (TypeParam::kStreamType) {
      case DemuxerStream::AUDIO:
        demuxer_stream_.set_audio_decoder_config(
            TestAudioConfig::NormalEncrypted());
        break;
      case DemuxerStream::VIDEO:
        demuxer_stream_.set_video_decoder_config(
            TestVideoConfig::NormalEncrypted());
        break;
      default:
        NOTREACHED();
    }
  }

  void SelectDecoder() {
    decoder_selector_->SelectDecoder(
        base::BindOnce(&Self::OnDecoderSelectedThunk, base::Unretained(this)),
        base::BindRepeating(&Self::OnOutput, base::Unretained(this)));
    RunUntilIdle();
  }

  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

  base::test::TaskEnvironment task_environment_;
  NullMediaLog media_log_;

  std::unique_ptr<StreamTraits> traits_;
  StrictMock<MockDemuxerStream> demuxer_stream_;
  std::unique_ptr<StrictMock<MockCdmContext>> cdm_context_;
  std::unique_ptr<NiceMock<MockDecryptor>> decryptor_;

  std::unique_ptr<DecoderSelector<TypeParam::kStreamType>> decoder_selector_;

  bool use_decrypting_decoder_ = false;
  std::vector<std::pair<std::string, DecoderCapability>>
      mock_decoders_to_create_;

 private:
  DISALLOW_COPY_AND_ASSIGN(DecoderSelectorTest);
};

using DecoderSelectorTestParams =
    ::testing::Types<AudioDecoderSelectorTestParam,
                     VideoDecoderSelectorTestParam>;
TYPED_TEST_SUITE(DecoderSelectorTest, DecoderSelectorTestParams);

// Tests for clear streams. CDM will not be used for clear streams so
// DecryptorCapability doesn't really matter.

TYPED_TEST(DecoderSelectorTest, ClearStream_NoDecoders) {
  this->UseClearDecoderConfig();
  this->CreateDecoderSelector();

  EXPECT_CALL(*this, OnDecoderSelected(kNoDecoder, IsNull()));
  this->SelectDecoder();
}

TYPED_TEST(DecoderSelectorTest, ClearStream_NoClearDecoder) {
  this->AddDecryptingDecoder();
  this->UseClearDecoderConfig();
  this->CreateDecoderSelector();

  EXPECT_CALL(*this, OnDecoderSelected(kNoDecoder, IsNull()));
  this->SelectDecoder();
}

TYPED_TEST(DecoderSelectorTest, ClearStream_OneClearDecoder) {
  this->AddMockDecoder(kDecoder1, kClearOnly);
  this->UseClearDecoderConfig();
  this->CreateDecoderSelector();

  EXPECT_CALL(*this, OnDecoderSelected(kDecoder1, IsNull()));
  this->SelectDecoder();
}

TYPED_TEST(DecoderSelectorTest, ClearStream_InternalFallback) {
  this->AddMockDecoder(kDecoder1, kAlwaysFail);
  this->AddMockDecoder(kDecoder2, kClearOnly);
  this->UseClearDecoderConfig();
  this->CreateDecoderSelector();

  EXPECT_CALL(*this, OnDecoderSelected(kDecoder2, IsNull()));
  this->SelectDecoder();
}

TYPED_TEST(DecoderSelectorTest, ClearStream_ExternalFallback) {
  this->AddMockDecoder(kDecoder1, kClearOnly);
  this->AddMockDecoder(kDecoder2, kClearOnly);
  this->UseClearDecoderConfig();
  this->CreateDecoderSelector();

  EXPECT_CALL(*this, OnDecoderSelected(kDecoder1, IsNull()));
  this->SelectDecoder();

  EXPECT_CALL(*this, OnDecoderSelected(kDecoder2, IsNull()));
  this->SelectDecoder();

  EXPECT_CALL(*this, OnDecoderSelected(kNoDecoder, IsNull()));
  this->SelectDecoder();
}

TYPED_TEST(DecoderSelectorTest, ClearStream_FinalizeDecoderSelection) {
  this->AddMockDecoder(kDecoder1, kClearOnly);
  this->AddMockDecoder(kDecoder2, kClearOnly);
  this->UseClearDecoderConfig();
  this->CreateDecoderSelector();

  EXPECT_CALL(*this, OnDecoderSelected(kDecoder1, IsNull()));
  this->SelectDecoder();

  this->decoder_selector_->FinalizeDecoderSelection();

  EXPECT_CALL(*this, OnDecoderSelected(kDecoder1, IsNull()));
  this->SelectDecoder();
}

// Tests for encrypted streams.

TYPED_TEST(DecoderSelectorTest, EncryptedStream_NoDecryptor_OneClearDecoder) {
  this->AddMockDecoder(kDecoder1, kClearOnly);
  this->CreateCdmContext(kNoDecryptor);
  this->UseEncryptedDecoderConfig();
  this->CreateDecoderSelector();

  EXPECT_CALL(*this, OnDecoderSelected(kNoDecoder, IsNull()));
  this->SelectDecoder();
}

TYPED_TEST(DecoderSelectorTest, EncryptedStream_NoDecryptor_InternalFallback) {
  this->AddMockDecoder(kDecoder1, kClearOnly);
  this->AddMockDecoder(kDecoder2, kEncryptedOnly);
  this->CreateCdmContext(kNoDecryptor);
  this->UseEncryptedDecoderConfig();
  this->CreateDecoderSelector();

  EXPECT_CALL(*this, OnDecoderSelected(kDecoder2, IsNull()));
  this->SelectDecoder();
}

TYPED_TEST(DecoderSelectorTest, EncryptedStream_NoDecryptor_ExternalFallback) {
  this->AddMockDecoder(kDecoder1, kEncryptedOnly);
  this->AddMockDecoder(kDecoder2, kEncryptedOnly);
  this->CreateCdmContext(kNoDecryptor);
  this->UseEncryptedDecoderConfig();
  this->CreateDecoderSelector();

  EXPECT_CALL(*this, OnDecoderSelected(kDecoder1, IsNull()));
  this->SelectDecoder();

  EXPECT_CALL(*this, OnDecoderSelected(kDecoder2, IsNull()));
  this->SelectDecoder();
}

TYPED_TEST(DecoderSelectorTest,
           EncryptedStream_NoDecryptor_FinalizeDecoderSelection) {
  this->AddMockDecoder(kDecoder1, kEncryptedOnly);
  this->AddMockDecoder(kDecoder2, kEncryptedOnly);
  this->CreateCdmContext(kNoDecryptor);
  this->UseEncryptedDecoderConfig();
  this->CreateDecoderSelector();

  EXPECT_CALL(*this, OnDecoderSelected(kDecoder1, IsNull()));
  this->SelectDecoder();

  this->decoder_selector_->FinalizeDecoderSelection();

  EXPECT_CALL(*this, OnDecoderSelected(kDecoder1, IsNull()));
  this->SelectDecoder();
}

TYPED_TEST(DecoderSelectorTest, EncryptedStream_DecryptOnly_NoDecoder) {
  this->CreateCdmContext(kDecryptOnly);
  this->UseEncryptedDecoderConfig();
  this->CreateDecoderSelector();

  EXPECT_CALL(*this, OnDecoderSelected(kNoDecoder, IsNull()));
  this->SelectDecoder();
}

TYPED_TEST(DecoderSelectorTest, EncryptedStream_DecryptOnly_OneClearDecoder) {
  this->AddMockDecoder(kDecoder1, kClearOnly);
  this->CreateCdmContext(kDecryptOnly);
  this->UseEncryptedDecoderConfig();
  this->CreateDecoderSelector();

  EXPECT_CALL(*this, OnDecoderSelected(kDecoder1, NotNull()));
  this->SelectDecoder();
}

TYPED_TEST(DecoderSelectorTest, EncryptedStream_DecryptOnly_InternalFallback) {
  this->AddMockDecoder(kDecoder1, kAlwaysFail);
  this->AddMockDecoder(kDecoder2, kClearOnly);
  this->CreateCdmContext(kDecryptOnly);
  this->UseEncryptedDecoderConfig();
  this->CreateDecoderSelector();

  EXPECT_CALL(*this, OnDecoderSelected(kDecoder2, NotNull()));
  this->SelectDecoder();
}

TYPED_TEST(DecoderSelectorTest,
           EncryptedStream_DecryptOnly_FinalizeDecoderSelection) {
  this->AddMockDecoder(kDecoder1, kClearOnly);
  this->AddMockDecoder(kDecoder2, kClearOnly);
  this->CreateCdmContext(kDecryptOnly);
  this->UseEncryptedDecoderConfig();
  this->CreateDecoderSelector();

  std::unique_ptr<DecryptingDemuxerStream> saved_dds;
  EXPECT_CALL(*this, OnDecoderSelected(kDecoder1, NotNull()))
      .WillOnce([&](std::string decoder_name,
                    std::unique_ptr<DecryptingDemuxerStream> dds) {
        saved_dds = std::move(dds);
      });
  this->SelectDecoder();

  this->decoder_selector_->FinalizeDecoderSelection();

  // DDS is reused.
  EXPECT_CALL(*this, OnDecoderSelected(kDecoder1, IsNull()));
  this->SelectDecoder();
}

TYPED_TEST(DecoderSelectorTest, EncryptedStream_DecryptAndDecode) {
  this->AddDecryptingDecoder();
  this->AddMockDecoder(kDecoder1, kClearOnly);
  this->CreateCdmContext(kDecryptAndDecode);
  this->UseEncryptedDecoderConfig();
  this->CreateDecoderSelector();

#if !defined(OS_ANDROID)
  // A DecryptingVideoDecoder will be created and selected. The clear decoder
  // should not be touched at all. No DecryptingDemuxerStream should be
  // created.
  EXPECT_CALL(*this,
              OnDecoderSelected(TypeParam::kDecryptingDecoder, IsNull()));
#else
  // A DecryptingDemuxerStream will be created. The clear decoder will be
  // initialized and returned.
  EXPECT_CALL(*this, OnDecoderSelected(kDecoder1, NotNull()));
#endif  // !defined(OS_ANDROID)

  this->SelectDecoder();
}

TYPED_TEST(DecoderSelectorTest,
           EncryptedStream_DecryptAndDecode_ExternalFallback) {
  this->AddDecryptingDecoder();
  this->AddMockDecoder(kDecoder1, kClearOnly);
  this->AddMockDecoder(kDecoder2, kClearOnly);
  this->CreateCdmContext(kDecryptAndDecode);
  this->UseEncryptedDecoderConfig();
  this->CreateDecoderSelector();

#if !defined(OS_ANDROID)
  // DecryptingDecoder is selected immediately.
  EXPECT_CALL(*this,
              OnDecoderSelected(TypeParam::kDecryptingDecoder, IsNull()));
  this->SelectDecoder();
#endif  // !defined(OS_ANDROID)

  // On fallback, a DecryptingDemuxerStream will be created.
  std::unique_ptr<DecryptingDemuxerStream> saved_dds;
  EXPECT_CALL(*this, OnDecoderSelected(kDecoder1, NotNull()))
      .WillOnce([&](std::string decoder_name,
                    std::unique_ptr<DecryptingDemuxerStream> dds) {
        saved_dds = std::move(dds);
      });
  this->SelectDecoder();

  // The DecryptingDemuxerStream should be reused.
  EXPECT_CALL(*this, OnDecoderSelected(kDecoder2, IsNull()));
  this->SelectDecoder();
}

TYPED_TEST(DecoderSelectorTest, ClearToEncryptedStream_DecryptOnly) {
  this->AddMockDecoder(kDecoder1, kClearOnly);
  this->CreateCdmContext(kDecryptOnly);
  this->UseClearDecoderConfig();
  this->CreateDecoderSelector();

  EXPECT_CALL(*this, OnDecoderSelected(kDecoder1, IsNull()));
  this->SelectDecoder();

  this->decoder_selector_->FinalizeDecoderSelection();
  this->UseEncryptedDecoderConfig();

  EXPECT_CALL(*this, OnDecoderSelected(kDecoder1, NotNull()));
  this->SelectDecoder();
}

}  // namespace media
