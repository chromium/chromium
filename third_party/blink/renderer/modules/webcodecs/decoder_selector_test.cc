// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webcodecs/decoder_selector.h"

#include <vector>

#include "media/base/demuxer_stream.h"
#include "media/base/media_util.h"
#include "media/base/mock_filters.h"
#include "media/base/status.h"
#include "media/base/test_helpers.h"
#include "media/base/video_decoder.h"
#include "media/filters/decoder_stream.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

using ::testing::_;
using ::testing::IsNull;
using ::testing::StrictMock;

namespace blink {

namespace {

enum DecoderCapability {
  kFail,
  kSucceed,
};

const int kNoDecoder = 0xdead;
const int kDecoder1 = 0xabc;
const int kDecoder2 = 0xdef;

// Specializations for the AUDIO version of the test.
class AudioDecoderSelectorTestParam {
 public:
  static constexpr media::DemuxerStream::Type kStreamType =
      media::DemuxerStream::AUDIO;

  using MockDecoderSelector = DecoderSelector<media::DemuxerStream::AUDIO>;
  using MockDecoder = media::MockAudioDecoder;
  using Output = media::AudioBuffer;
  using DecoderType = media::AudioDecoderType;

  static media::AudioDecoderConfig CreateConfig() {
    return media::TestAudioConfig::Normal();
  }

  // Create a config that won't match the return of CreateConfig().
  static media::AudioDecoderConfig CreateAlternateConfig() {
    return media::TestAudioConfig::NormalEncrypted();
  }

  // Decoder::Initialize() takes different parameters depending on the type.
  static void ExpectInitialize(MockDecoder* decoder,
                               DecoderCapability capability,
                               media::AudioDecoderConfig expected_config,
                               bool /*low_delay */) {
    EXPECT_CALL(*decoder, Initialize_(_, _, _, _, _))
        .WillRepeatedly([capability, expected_config](
                            const media::AudioDecoderConfig& config,
                            media::CdmContext*,
                            media::AudioDecoder::InitCB& init_cb,
                            const media::AudioDecoder::OutputCB&,
                            const media::WaitingCB&) {
          EXPECT_TRUE(config.Matches(expected_config));
          std::move(init_cb).Run(capability == kSucceed
                                     ? media::DecoderStatus::Codes::kOk
                                     : media::DecoderStatus::Codes::kFailed);
        });
  }
};

// Specializations for the VIDEO version of the test.
class VideoDecoderSelectorTestParam {
 public:
  static constexpr media::DemuxerStream::Type kStreamType =
      media::DemuxerStream::VIDEO;

  using MockDecoderSelector = DecoderSelector<media::DemuxerStream::VIDEO>;
  using MockDecoder = media::MockVideoDecoder;
  using Output = media::VideoFrame;
  using DecoderType = media::VideoDecoderType;

  static media::VideoDecoderConfig CreateConfig() {
    return media::TestVideoConfig::Normal();
  }

  // Create a config that won't match the return of CreateConfig().
  static media::VideoDecoderConfig CreateAlternateConfig() {
    return media::TestVideoConfig::LargeEncrypted();
  }

  static void ExpectInitialize(MockDecoder* decoder,
                               DecoderCapability capability,
                               media::VideoDecoderConfig expected_config,
                               bool low_delay) {
    EXPECT_CALL(*decoder, Initialize_(_, low_delay, _, _, _, _))
        .WillRepeatedly([capability, expected_config](
                            const media::VideoDecoderConfig& config,
                            bool low_delay, media::CdmContext*,
                            media::VideoDecoder::InitCB& init_cb,
                            const media::VideoDecoder::OutputCB&,
                            const media::WaitingCB&) {
          EXPECT_TRUE(config.Matches(expected_config));
          std::move(init_cb).Run(capability == kSucceed
                                     ? media::DecoderStatus::Codes::kOk
                                     : media::DecoderStatus::Codes::kFailed);
        });
  }
};

// Allocate storage for the member variables.
constexpr media::DemuxerStream::Type AudioDecoderSelectorTestParam::kStreamType;
constexpr media::DemuxerStream::Type VideoDecoderSelectorTestParam::kStreamType;

}  // namespace

// Note: The parameter is called TypeParam in the test cases regardless of what
// we call it here. It's been named the same for convenience.
// Note: The test fixtures inherit from this class. Inside the test cases the
// test fixture class is called TestFixture.
template <typename TypeParam>
class WebCodecsDecoderSelectorTest : public ::testing::Test {
 public:
  // Convenience aliases.
  using Self = WebCodecsDecoderSelectorTest<TypeParam>;
  using Decoder = typename TypeParam::MockDecoderSelector::Decoder;
  using DecoderConfig = typename TypeParam::MockDecoderSelector::DecoderConfig;
  using MockDecoder = typename TypeParam::MockDecoder;
  using Output = typename TypeParam::Output;
  using DecoderType = typename TypeParam::DecoderType;

  WebCodecsDecoderSelectorTest() { CreateDecoderSelector(); }

  WebCodecsDecoderSelectorTest(const WebCodecsDecoderSelectorTest&) = delete;
  WebCodecsDecoderSelectorTest& operator=(const WebCodecsDecoderSelectorTest&) =
      delete;

  void OnOutput(scoped_refptr<Output> output) { NOTREACHED_IN_MIGRATION(); }

  MOCK_METHOD1_T(OnDecoderSelected, void(int));

  void OnDecoderSelectedThunk(std::unique_ptr<Decoder> decoder) {
    // Report only the id of the mock, since that's what the tests care
    // about. The decoder will be destructed immediately.
    OnDecoderSelected(
        decoder ? static_cast<MockDecoder*>(decoder.get())->GetDecoderId()
                : kNoDecoder);
  }

  void AddMockDecoder(int decoder_id, DecoderCapability capability) {
    // Actual decoders are created in CreateDecoders(), which may be called
    // multiple times by the DecoderSelector.
    mock_decoders_to_create_.emplace_back(decoder_id, capability);
  }

  std::vector<std::unique_ptr<Decoder>> CreateDecoders() {
    std::vector<std::unique_ptr<Decoder>> decoders;

    for (const auto& info : mock_decoders_to_create_) {
      std::unique_ptr<StrictMock<MockDecoder>> decoder =
          std::make_unique<StrictMock<MockDecoder>>(
              /*is_platform_decoder=*/false, /*supports_decryption=*/true,
              info.first);
      TypeParam::ExpectInitialize(decoder.get(), info.second,
                                  last_set_decoder_config_, low_delay_);
      decoders.push_back(std::move(decoder));
    }

    return decoders;
  }

  void CreateDecoderSelector() {
    decoder_selector_ =
        std::make_unique<DecoderSelector<TypeParam::kStreamType>>(
            scheduler::GetSingleThreadTaskRunnerForTesting(),
            WTF::BindRepeating(&Self::CreateDecoders, base::Unretained(this)),
            WTF::BindRepeating(&Self::OnOutput, base::Unretained(this)));
  }

  void SelectDecoder(DecoderConfig config = TypeParam::CreateConfig()) {
    last_set_decoder_config_ = config;
    decoder_selector_->SelectDecoder(
        config, low_delay_,
        WTF::BindOnce(&Self::OnDecoderSelectedThunk, base::Unretained(this)));
    RunUntilIdle();
  }

  void RunUntilIdle() { platform_->RunUntilIdle(); }

  test::TaskEnvironment task_environment_;
  ScopedTestingPlatformSupport<TestingPlatformSupport> platform_;
  media::NullMediaLog media_log_;

  DecoderConfig last_set_decoder_config_;

  std::unique_ptr<DecoderSelector<TypeParam::kStreamType>> decoder_selector_;

  std::vector<std::pair<int, DecoderCapability>> mock_decoders_to_create_;

  bool low_delay_ = false;
};

using WebCodecsDecoderSelectorTestParams =
    ::testing::Types<AudioDecoderSelectorTestParam,
                     VideoDecoderSelectorTestParam>;
TYPED_TEST_SUITE(WebCodecsDecoderSelectorTest,
                 WebCodecsDecoderSelectorTestParams);

TYPED_TEST(WebCodecsDecoderSelectorTest, NoDecoders) {
  EXPECT_CALL(*this, OnDecoderSelected(kNoDecoder));
  this->SelectDecoder();
}

TYPED_TEST(WebCodecsDecoderSelectorTest, OneDecoder) {
  this->AddMockDecoder(kDecoder1, kSucceed);

  EXPECT_CALL(*this, OnDecoderSelected(kDecoder1));
  this->SelectDecoder();
}

TYPED_TEST(WebCodecsDecoderSelectorTest, LowDelay) {
  this->low_delay_ = true;
  this->AddMockDecoder(kDecoder1, kSucceed);

  EXPECT_CALL(*this, OnDecoderSelected(kDecoder1));
  this->SelectDecoder();
}

TYPED_TEST(WebCodecsDecoderSelectorTest, TwoDecoders) {
  this->AddMockDecoder(kDecoder1, kFail);
  this->AddMockDecoder(kDecoder2, kSucceed);

  EXPECT_CALL(*this, OnDecoderSelected(kDecoder2));
  this->SelectDecoder();
}

TYPED_TEST(WebCodecsDecoderSelectorTest, TwoDecoders_SelectAgain) {
  this->AddMockDecoder(kDecoder1, kSucceed);
  this->AddMockDecoder(kDecoder2, kSucceed);

  EXPECT_CALL(*this, OnDecoderSelected(kDecoder1));
  this->SelectDecoder();

  // Selecting again should give (a new instance of) the same decoder.
  EXPECT_CALL(*this, OnDecoderSelected(kDecoder1));
  this->SelectDecoder();
}

TYPED_TEST(WebCodecsDecoderSelectorTest, TwoDecoders_NewConfigSelectAgain) {
  this->AddMockDecoder(kDecoder1, kSucceed);
  this->AddMockDecoder(kDecoder2, kSucceed);

  EXPECT_CALL(*this, OnDecoderSelected(kDecoder1));
  this->SelectDecoder(TypeParam::CreateConfig());

  // Selecting again should give (a new instance of) the same decoder.
  EXPECT_CALL(*this, OnDecoderSelected(kDecoder1));
  // Select again with a different config. Expected config verified during
  // CreateDecoders() the SelectDecoder() call.
  this->SelectDecoder(TypeParam::CreateAlternateConfig());
}

}  // namespace blink
