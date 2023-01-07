// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/mojom/url_pattern_set_mojom_traits.h"

#include "extensions/common/mojom/url_pattern_set.mojom.h"
#include "extensions/common/url_pattern.h"
#include "extensions/common/url_pattern_set.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using mojo::test::SerializeAndDeserialize;

TEST(URLPatternSetMojomTraitsTest, BasicURLPattern) {
  URLPattern input(URLPattern::SCHEME_HTTP);
  EXPECT_EQ(URLPattern::ParseResult::kSuccess,
            input.Parse("http://*.foo:1234/bar"))
      << "Got unexpected error in the URLPattern parsing";

  URLPattern output;
  EXPECT_TRUE(
      SerializeAndDeserialize<extensions::mojom::URLPattern>(input, output));
  EXPECT_EQ(input, output);
  EXPECT_EQ(input.valid_schemes(), output.valid_schemes());
  EXPECT_EQ(input.scheme(), output.scheme());
  EXPECT_EQ(input.host(), output.host());
  EXPECT_EQ(input.port(), output.port());
  EXPECT_EQ(input.path(), output.path());
  EXPECT_EQ(input.match_all_urls(), output.match_all_urls());
  EXPECT_EQ(input.match_subdomains(), output.match_subdomains());
  EXPECT_EQ(input.GetAsString(), output.GetAsString());
}

TEST(URLPatternSetMojomTraitsTest, EmptyURLPatternSet) {
  extensions::URLPatternSet input;
  extensions::URLPatternSet output;

  EXPECT_TRUE(
      SerializeAndDeserialize<extensions::mojom::URLPatternSet>(input, output));
  EXPECT_TRUE(output.is_empty());
}

TEST(URLPatternSetMojomTraitsTest, BasicURLPatternSet) {
  URLPattern pattern1(URLPattern::SCHEME_ALL);
  EXPECT_EQ(URLPattern::ParseResult::kSuccess,
            pattern1.Parse("http://*.foo:1234/bar"))
      << "Got unexpected error in the URLPattern parsing";

  URLPattern pattern2(URLPattern::SCHEME_HTTPS);
  EXPECT_EQ(URLPattern::ParseResult::kSuccess,
            pattern2.Parse("https://www.google.com/foobar"))
      << "Got unexpected error in the URLPattern parsing";

  extensions::URLPatternSet input;
  input.AddPattern(pattern1);
  input.AddPattern(pattern2);

  extensions::URLPatternSet output;
  EXPECT_TRUE(
      SerializeAndDeserialize<extensions::mojom::URLPatternSet>(input, output));
  EXPECT_THAT(output.patterns(), testing::ElementsAre(pattern1, pattern2));
}
