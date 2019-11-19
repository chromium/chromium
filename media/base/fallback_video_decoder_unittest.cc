// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <tuple>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/run_loop.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/task_environment.h"
#include "media/base/decoder_buffer.h"
#include "media/base/fallback_video_decoder.h"
#include "media/base/mock_filters.h"
#include "media/base/test_helpers.h"
#include "media/base/video_decoder.h"
#include "media/base/video_decoder_config.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest-param-test.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::base::test::RunOnceCallback;
using ::testing::_;
using ::testing::StrictMock;

namespace media {

class FallbackVideoDecoderUnittest : public ::testing::TestWithParam<bool> {
 public:
  FallbackVideoDecoderUnittest()
      : backup_decoder_(nullptr),
        preferred_decoder_(nullptr),
        fallback_decoder_(nullptr) {}

  ~FallbackVideoDecoderUnittest() override { Destroy(); }

  std::unique_ptr<VideoDecoder> MakeMockDecoderWithExpectations(
      bool is_fallback,
      bool preferred_should_succeed) {
    std::string n = is_fallback ? "Fallback" : "Preferred";
    StrictMock<MockVideoDecoder>* result = new StrictMock<MockVideoDecoder>(n);

    if (is_fallback && !preferred_should_succeed) {
      EXPECT_CALL(*result, Initialize_(_, _, _, _, _, _))
          .WillOnce(RunOnceCallback<3>(true));
    }

    if (!is_fallback) {
      preferred_decoder_ = result;
      EXPECT_CALL(*result, Initialize_(_, _, _, _, _, _))
          .WillOnce(RunOnceCallback<3>(preferred_should_succeed));
    } else {
      backup_decoder_ = result;
    }

    return std::unique_ptr<VideoDecoder>(result);
  }

  void Initialize(bool preferred_should_succeed) {
    fallback_decoder_ = new FallbackVideoDecoder(
        MakeMockDecoderWithExpectations(false, preferred_should_succeed),
        MakeMockDecoderWithExpectations(true, preferred_should_succeed));

    fallback_decoder_->Initialize(
        video_decoder_config_, false, nullptr,
        base::BindRepeating([](bool success) { EXPECT_TRUE(success); }),
        base::DoNothing(), base::DoNothing());
  }

 protected:
  void Destroy() { std::default_delete<VideoDecoder>()(fallback_decoder_); }

  bool PreferredShouldSucceed() { return GetParam(); }

  base::test::TaskEnvironment task_environment_;

  StrictMock<MockVideoDecoder>* backup_decoder_;
  StrictMock<MockVideoDecoder>* preferred_decoder_;
  VideoDecoder* fallback_decoder_;
  VideoDecoderConfig video_decoder_config_;

 private:
  DISALLOW_COPY_AND_ASSIGN(FallbackVideoDecoderUnittest);
};

INSTANTIATE_TEST_SUITE_P(DoesPreferredInitFail,
                         FallbackVideoDecoderUnittest,
                         testing::ValuesIn({true, false}));

#define EXPECT_ON_CORRECT_DECODER(method)     \
  if (PreferredShouldSucceed())               \
    EXPECT_CALL(*preferred_decoder_, method); \
  else                                        \
    EXPECT_CALL(*backup_decoder_, method)  // Intentionally leave off semicolon.

// Do not test the name lookup; it is NOTREACHED.
TEST_P(FallbackVideoDecoderUnittest, MethodsRedirectedAsExpected) {
  Initialize(PreferredShouldSucceed());

  EXPECT_ON_CORRECT_DECODER(Decode_(_, _));
  fallback_decoder_->Decode(nullptr, base::DoNothing());

  EXPECT_ON_CORRECT_DECODER(Reset_(_));
  fallback_decoder_->Reset(base::DoNothing());

  EXPECT_ON_CORRECT_DECODER(NeedsBitstreamConversion());
  fallback_decoder_->NeedsBitstreamConversion();

  EXPECT_ON_CORRECT_DECODER(CanReadWithoutStalling());
  fallback_decoder_->CanReadWithoutStalling();

  EXPECT_ON_CORRECT_DECODER(GetMaxDecodeRequests());
  fallback_decoder_->GetMaxDecodeRequests();
}

//               │    first initialization   │   second initialization   │
//   preferred   │  preferred  │   backup    │  preferred  │   backup    │
// will succeed  │ init called │ init called │ init called │ init called │
//───────────────┼─────────────┼─────────────┼─────────────┼─────────────┤
//     false     │      ✓      │      ✓      │       x     │      ✓      │
//     true      │      ✓      │      x      │       ✓     │      ✓      │
TEST_P(FallbackVideoDecoderUnittest, ReinitializeWithPreferredFailing) {
  Initialize(PreferredShouldSucceed());

  // If we succeedd the first time, it should still be alive.
  if (PreferredShouldSucceed()) {
    EXPECT_CALL(*preferred_decoder_, Initialize_(_, _, _, _, _, _))
        .WillOnce(RunOnceCallback<3>(false));  // fail initialization
  }
  EXPECT_CALL(*backup_decoder_, Initialize_(_, _, _, _, _, _))
      .WillOnce(RunOnceCallback<3>(true));

  fallback_decoder_->Initialize(
      video_decoder_config_, false, nullptr,
      base::BindRepeating([](bool success) { EXPECT_TRUE(success); }),
      base::DoNothing(), base::DoNothing());
}

//               │    first initialization   │   second initialization   │
//   preferred   │  preferred  │   backup    │  preferred  │   backup    │
// will succeed  │ init called │ init called │ init called │ init called │
//───────────────┼─────────────┼─────────────┼─────────────┼─────────────┤
//     false     │      ✓      │      ✓      │       x     │      ✓      │
//     true      │      ✓      │      x      │       ✓     │      x      │
TEST_P(FallbackVideoDecoderUnittest, ReinitializeWithPreferredSuccessful) {
  Initialize(PreferredShouldSucceed());

  // If we succeedd the first time, it should still be alive.
  if (PreferredShouldSucceed()) {
    EXPECT_CALL(*preferred_decoder_, Initialize_(_, _, _, _, _, _))
        .WillOnce(RunOnceCallback<3>(true));  // pass initialization
  } else {
    // Otherwise, preferred was deleted, and we only backup still exists.
    EXPECT_CALL(*backup_decoder_, Initialize_(_, _, _, _, _, _))
        .WillOnce(RunOnceCallback<3>(true));
  }

  fallback_decoder_->Initialize(
      video_decoder_config_, false, nullptr,
      base::BindOnce([](bool success) { EXPECT_TRUE(success); }),
      base::DoNothing(), base::DoNothing());
}

}  // namespace media
