// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/timing_allow_origin_parser.h"

#include <string>

#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using ::testing::ElementsAre;
using ::testing::IsEmpty;

namespace network {

TEST(TimingAllowOriginParserTest, Empty) {
  auto tao = ParseTimingAllowOrigin("");
  ASSERT_EQ(mojom::TimingAllowOrigin::Tag::kSerializedOrigins, tao->which());
  EXPECT_THAT(tao->get_serialized_origins(), IsEmpty());
}

// Underscores are not permitted in domain names. However, the parser shouldn't
// do anything clever, since the actual TAO check is implemented by comparing a
// serialized origin against a value parsed out of the header.
TEST(TimingAllowOriginParserTest, InvalidOrigin) {
  auto tao = ParseTimingAllowOrigin("_");
  ASSERT_EQ(mojom::TimingAllowOrigin::Tag::kSerializedOrigins, tao->which());
  EXPECT_THAT(tao->get_serialized_origins(), ElementsAre("_"));
}

TEST(TimingAllowOriginParserTest, InvalidOriginWithEmbeddedWhitespace) {
  auto tao = ParseTimingAllowOrigin("https://exa mple.com");
  ASSERT_EQ(mojom::TimingAllowOrigin::Tag::kSerializedOrigins, tao->which());
  // Garbage in, garbage out :)
  EXPECT_THAT(tao->get_serialized_origins(),
              ElementsAre("https://exa mple.com"));
}

TEST(TimingAllowedOriginParserTest, SingleOrigin) {
  auto tao = ParseTimingAllowOrigin("https://example.com");
  ASSERT_EQ(mojom::TimingAllowOrigin::Tag::kSerializedOrigins, tao->which());
  EXPECT_THAT(tao->get_serialized_origins(),
              ElementsAre("https://example.com"));
}

TEST(TimingAllowOriginParserTest, DuplicateOrigin) {}

TEST(TimingAllowOriginParserTest, SingleOriginWithWhitespacePadding) {
  {
    auto tao = ParseTimingAllowOrigin(" https://example.com");
    ASSERT_EQ(mojom::TimingAllowOrigin::Tag::kSerializedOrigins, tao->which());
    EXPECT_THAT(tao->get_serialized_origins(),
                ElementsAre("https://example.com"));
  }

  {
    auto tao = ParseTimingAllowOrigin("https://example.com ");
    ASSERT_EQ(mojom::TimingAllowOrigin::Tag::kSerializedOrigins, tao->which());
    EXPECT_THAT(tao->get_serialized_origins(),
                ElementsAre("https://example.com"));
  }

  {
    auto tao = ParseTimingAllowOrigin(" https://example.com ");
    ASSERT_EQ(mojom::TimingAllowOrigin::Tag::kSerializedOrigins, tao->which());
    EXPECT_THAT(tao->get_serialized_origins(),
                ElementsAre("https://example.com"));
  }
}

TEST(TimingAllowedOriginParserTest, MultipleOrigins) {
  auto tao =
      ParseTimingAllowOrigin("https://a.example.com,https://b.example.com");
  ASSERT_EQ(mojom::TimingAllowOrigin::Tag::kSerializedOrigins, tao->which());
  EXPECT_THAT(tao->get_serialized_origins(),
              ElementsAre("https://a.example.com", "https://b.example.com"));
}

TEST(TimingAllowOriginParserTest, MultipleOriginsWithWhitespacePadding) {
  static constexpr const char* kItems1[] = {
      "https://a.example.com",
      " https://a.example.com",
      "https://a.example.com ",
      " https://a.example.com ",
  };
  static constexpr const char* kItems2[] = {
      "https://b.example.com",
      " https://b.example.com",
      "https://b.example.com ",
      " https://b.example.com ",
  };

  for (const char* kItem1 : kItems1) {
    for (const char* kItem2 : kItems2) {
      std::string test_case = base::StrCat({kItem1, ",", kItem2});
      SCOPED_TRACE(test_case);
      auto tao = ParseTimingAllowOrigin(test_case);
      ASSERT_EQ(mojom::TimingAllowOrigin::Tag::kSerializedOrigins,
                tao->which());
      EXPECT_THAT(
          tao->get_serialized_origins(),
          ElementsAre("https://a.example.com", "https://b.example.com"));
    }
  }
}

TEST(TimingAllowOriginParserTest, AllOrigins) {
  EXPECT_EQ(mojom::TimingAllowOrigin::Tag::kAll,
            ParseTimingAllowOrigin("*")->which());
}

TEST(TimingAllowOriginParserTest, AllOriginsWithWhitespacePadding) {
  EXPECT_EQ(mojom::TimingAllowOrigin::Tag::kAll,
            ParseTimingAllowOrigin(" *")->which());

  EXPECT_EQ(mojom::TimingAllowOrigin::Tag::kAll,
            ParseTimingAllowOrigin("* ")->which());

  EXPECT_EQ(mojom::TimingAllowOrigin::Tag::kAll,
            ParseTimingAllowOrigin(" * ")->which());
}

TEST(TimingAllowOriginParserTest, DuplicateWildcard) {
  EXPECT_EQ(mojom::TimingAllowOrigin::Tag::kAll,
            ParseTimingAllowOrigin("*,*")->which());
}

// *, combined with any other origin, should still result in all origins
// allowed.
TEST(TimingAllowOriginParserTest, WildcardWithOtherOrigins) {
  EXPECT_EQ(mojom::TimingAllowOrigin::Tag::kAll,
            ParseTimingAllowOrigin("*,https://example.com")->which());

  EXPECT_EQ(mojom::TimingAllowOrigin::Tag::kAll,
            ParseTimingAllowOrigin("https://example.com,*")->which());
}

TEST(TimingAllowOriginParserTest,
     WildcardWithOtherOriginsAndWhitespacePadding) {
  static constexpr const char* kItems1[] = {
      "*",
      " *",
      "* ",
      " * ",
  };
  static constexpr const char* kItems2[] = {
      "https://example.com",
      " https://example.com",
      "https://example.com ",
      " https://example.com ",
  };

  for (const char* kItem1 : kItems1) {
    for (const char* kItem2 : kItems2) {
      {
        std::string test_case = base::StrCat({kItem1, ",", kItem2});
        SCOPED_TRACE(test_case);
        EXPECT_EQ(mojom::TimingAllowOrigin::Tag::kAll,
                  ParseTimingAllowOrigin(test_case)->which());
      }

      {
        std::string test_case = base::StrCat({kItem2, ",", kItem1});
        SCOPED_TRACE(test_case);
        EXPECT_EQ(mojom::TimingAllowOrigin::Tag::kAll,
                  ParseTimingAllowOrigin(test_case)->which());
      }
    }
  }
}

TEST(TimingAllowOriginParserTest, WildcardWithInvalidOrigins) {
  EXPECT_EQ(mojom::TimingAllowOrigin::Tag::kAll,
            ParseTimingAllowOrigin("*,_")->which());

  EXPECT_EQ(mojom::TimingAllowOrigin::Tag::kAll,
            ParseTimingAllowOrigin("_,*")->which());
}

static bool TAOCheck(const std::string& tao, const std::string& url) {
  return TimingAllowOriginCheck(ParseTimingAllowOrigin(tao),
                                url::Origin::Create(GURL(url)));
}

TEST(TimingAllowOriginParserTest, TAOCheck) {
  EXPECT_TRUE(TAOCheck("http://example1.com", "http://example1.com"));
  EXPECT_TRUE(
      TAOCheck("http://example1.com,example2.com", "http://example1.com"));
  EXPECT_TRUE(TAOCheck("http://example1.com,  http://example2.com",
                       "http://example2.com"));
  EXPECT_TRUE(TAOCheck("http://example1.com,  http://example2.com",
                       "http://example1.com"));
  EXPECT_TRUE(TAOCheck("*", "https://example1.com"));
  EXPECT_FALSE(TAOCheck("example1.com,  example2.com", "example1.com"));
  EXPECT_FALSE(TAOCheck(std::string(), "http://example1.com"));
  EXPECT_FALSE(TAOCheck(std::string("2342invalid-"), "http://example1.com"));
  EXPECT_FALSE(TAOCheck(std::string("example2.com"), "http://example1.com"));
}

}  // namespace network
