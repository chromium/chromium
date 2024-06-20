// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/stream_parser_factory.h"

#include "base/test/scoped_feature_list.h"
#include "media/base/media_switches.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

TEST(StreamParserFactoryTest, HlsProbeParserTest) {
  EXPECT_NE(StreamParserFactory::CreateRelaxedParser(
                RelaxedParserSupportedType::kMP2T),
            nullptr);
  EXPECT_NE(StreamParserFactory::CreateRelaxedParser(
                RelaxedParserSupportedType::kAAC),
            nullptr);

  // These are feature gated!
  EXPECT_EQ(StreamParserFactory::CreateRelaxedParser(
                RelaxedParserSupportedType::kMP4),
            nullptr);

  {
    base::test::ScopedFeatureList enable_mp4{kBuiltInHlsMP4};
    EXPECT_NE(StreamParserFactory::CreateRelaxedParser(
                  RelaxedParserSupportedType::kMP4),
              nullptr);
  }
}

}  // namespace media
