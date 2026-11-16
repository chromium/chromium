// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/navigation/commit_url.h"

#include <string>

#include "base/test/metrics/histogram_tester.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/url_constants.h"

namespace blink {
namespace {

constexpr char kHistogramName[] = "Navigation.Renderer.BlockedForFilterURL";

TEST(CommitUrlTest, ValidUrls) {
  base::HistogramTester histogram_tester;

  EXPECT_TRUE(IsValidCommitUrl(GURL("https://example.com/")));
  EXPECT_TRUE(IsValidCommitUrl(GURL("http://localhost:8080/path?q=1#frag")));
  EXPECT_TRUE(IsValidCommitUrl(GURL("data:text/html,hello")));
  EXPECT_TRUE(IsValidCommitUrl(GURL(url::kAboutBlankURL)));
  EXPECT_TRUE(IsValidCommitUrl(GURL("about:blank#foo")));
  EXPECT_TRUE(IsValidCommitUrl(GURL(kBlockedURL)));
  EXPECT_TRUE(IsValidCommitUrl(GURL(url::kAboutSrcdocURL)));
  EXPECT_TRUE(IsValidCommitUrl(GURL("about:srcdoc#frag")));

  // Exact maximum URL length should be accepted.
  std::string max_len_url = "https://example.com/";
  max_len_url.append(url::kMaxURLChars - max_len_url.length(), 'a');
  ASSERT_EQ(max_len_url.length(), url::kMaxURLChars);
  EXPECT_TRUE(IsValidCommitUrl(GURL(max_len_url)));

  histogram_tester.ExpectTotalCount(kHistogramName, 0);
}

TEST(CommitUrlTest, InvalidUrl) {
  base::HistogramTester histogram_tester;

  EXPECT_FALSE(IsValidCommitUrl(GURL()));
  EXPECT_FALSE(IsValidCommitUrl(GURL("not a valid url")));

  histogram_tester.ExpectUniqueSample(kHistogramName,
                                      RendererBlockedURLReason::kInvalidURL, 2);
}

TEST(CommitUrlTest, TooLongUrl) {
  base::HistogramTester histogram_tester;

  std::string too_long_url = "https://example.com/";
  too_long_url.append(url::kMaxURLChars - too_long_url.length() + 1, 'a');
  ASSERT_GT(too_long_url.length(), url::kMaxURLChars);

  EXPECT_FALSE(IsValidCommitUrl(GURL(too_long_url)));

  histogram_tester.ExpectUniqueSample(kHistogramName,
                                      RendererBlockedURLReason::kTooLongURL, 1);
}

TEST(CommitUrlTest, BadAboutUrl) {
  base::HistogramTester histogram_tester;

  EXPECT_FALSE(IsValidCommitUrl(GURL("about:mumble")));
  EXPECT_FALSE(IsValidCommitUrl(GURL("about://mumble")));
  EXPECT_FALSE(IsValidCommitUrl(GURL("about:version")));

  histogram_tester.ExpectUniqueSample(
      kHistogramName, RendererBlockedURLReason::kBadAboutURL, 3);
}

}  // namespace
}  // namespace blink
