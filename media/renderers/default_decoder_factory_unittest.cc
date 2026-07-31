// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/renderers/default_decoder_factory.h"

#include <algorithm>
#include <memory>
#include <vector>

#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "media/base/audio_decoder.h"
#include "media/base/media_switches.h"
#include "media/base/media_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

class DefaultDecoderFactoryTest : public testing::Test {
 protected:
  bool CreatesIamfAudioDecoder() {
    DefaultDecoderFactory factory(/*external_decoder_factory=*/nullptr);
    std::vector<std::unique_ptr<AudioDecoder>> decoders;
    factory.CreateAudioDecoders(task_environment_.GetMainThreadTaskRunner(),
                                &media_log_, &decoders);
    return std::ranges::any_of(decoders, [](const auto& decoder) {
      return decoder->GetDecoderType() == AudioDecoderType::kIamf;
    });
  }

 private:
  base::test::TaskEnvironment task_environment_;
  NullMediaLog media_log_;
};

TEST_F(DefaultDecoderFactoryTest, IamfDecoderTracksFeatureState) {
  {
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitAndDisableFeature(kIamfAudioDecoding);
    EXPECT_FALSE(CreatesIamfAudioDecoder());
  }

  {
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitAndEnableFeature(kIamfAudioDecoding);
    EXPECT_TRUE(CreatesIamfAudioDecoder());
  }
}

}  // namespace media
