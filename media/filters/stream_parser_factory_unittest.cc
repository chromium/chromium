// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/stream_parser_factory.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace media {

struct TestExpectation {
  std::string container;
  std::string codecs;
  bool is_created;
};

static const TestExpectation kHlsProbeParserExpectations[] = {
    {"video/webm", "vp9", true},
    {"audio/webm", "opus", true},
    {"video/mp4", "mp4a", true},
    {"video/mp4", "avc1.420000", true},
    {"audio/aac", "aac",
#if BUILDFLAG(USE_PROPRIETARY_CODECS)
     true
#else
     false
#endif
    },
    {"video/mp2t", "avc1.420000",
#if BUILDFLAG(USE_PROPRIETARY_CODECS) && \
    BUILDFLAG(ENABLE_MSE_MPEG2TS_STREAM_PARSER)
     true
#else
     false
#endif
    },
};

TEST(StreamParserFactoryTest, HlsProbeParserTest) {
  for (const auto& expectation : kHlsProbeParserExpectations) {
    const std::string codecs[]{expectation.codecs};
    auto parser_ptr = StreamParserFactory::CreateHLSProbeParser(
        expectation.container, codecs);
    EXPECT_EQ(expectation.is_created, (parser_ptr != nullptr));
  }
}

}  // namespace media
