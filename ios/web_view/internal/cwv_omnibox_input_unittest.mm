// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/public/cwv_omnibox_input.h"

#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

namespace ios_web_view {

class CWVOmniboxInputTest : public PlatformTest {
 public:
  CWVOmniboxInputTest(const CWVOmniboxInputTest&) = delete;
  CWVOmniboxInputTest& operator=(const CWVOmniboxInputTest&) = delete;

 protected:
  CWVOmniboxInputTest() {}
};

TEST_F(CWVOmniboxInputTest, HasSameTextAsInput) {
  {
    CWVOmniboxInput* input = [[CWVOmniboxInput alloc] initWithText:@""
                                     shouldUseHTTPSAsDefaultScheme:NO];
    EXPECT_NSEQ(@"", input.text);
  }
  {
    CWVOmniboxInput* input = [[CWVOmniboxInput alloc] initWithText:@"foo"
                                     shouldUseHTTPSAsDefaultScheme:NO];
    EXPECT_NSEQ(@"foo", input.text);
  }
  {
    CWVOmniboxInput* input = [[CWVOmniboxInput alloc] initWithText:@"foo bar"
                                     shouldUseHTTPSAsDefaultScheme:NO];
    EXPECT_NSEQ(@"foo bar", input.text);
  }
  {
    CWVOmniboxInput* input = [[CWVOmniboxInput alloc] initWithText:@"foo.com"
                                     shouldUseHTTPSAsDefaultScheme:NO];
    EXPECT_NSEQ(@"foo.com", input.text);
  }
  {
    CWVOmniboxInput* input =
        [[CWVOmniboxInput alloc] initWithText:@"はじめよう.みんな"
                shouldUseHTTPSAsDefaultScheme:NO];
    EXPECT_NSEQ(@"はじめよう.みんな", input.text);
  }
}

TEST_F(CWVOmniboxInputTest, HasRightType) {
  // CWVOmniboxInput is a wrapper around AutocompleteInput. This test just
  // checks if the types are mapped correctly. It delegates comprehensive test
  // of various inputs to AutocompleteInputTest.
  {
    CWVOmniboxInput* input = [[CWVOmniboxInput alloc] initWithText:@""
                                     shouldUseHTTPSAsDefaultScheme:NO];
    EXPECT_EQ(CWVOmniboxInputTypeEmpty, input.type);
  }
  {
    CWVOmniboxInput* input = [[CWVOmniboxInput alloc] initWithText:@"foo"
                                     shouldUseHTTPSAsDefaultScheme:NO];
    EXPECT_EQ(CWVOmniboxInputTypeUnknown, input.type);
  }
  {
    CWVOmniboxInput* input = [[CWVOmniboxInput alloc] initWithText:@"foo bar"
                                     shouldUseHTTPSAsDefaultScheme:NO];
    EXPECT_EQ(CWVOmniboxInputTypeQuery, input.type);
  }
  {
    CWVOmniboxInput* input = [[CWVOmniboxInput alloc] initWithText:@"foo.com"
                                     shouldUseHTTPSAsDefaultScheme:NO];
    EXPECT_EQ(CWVOmniboxInputTypeURL, input.type);
  }
  {
    CWVOmniboxInput* input =
        [[CWVOmniboxInput alloc] initWithText:@"はじめよう.みんな"
                shouldUseHTTPSAsDefaultScheme:NO];
    EXPECT_EQ(CWVOmniboxInputTypeURL, input.type);
  }
}

TEST_F(CWVOmniboxInputTest, HasRightURL) {
  {
    CWVOmniboxInput* input = [[CWVOmniboxInput alloc] initWithText:@"foo.com"
                                     shouldUseHTTPSAsDefaultScheme:NO];
    NSURL* expected_url = [NSURL URLWithString:@"http://foo.com/"];
    EXPECT_NSEQ(expected_url, input.URL);
  }
  {
    CWVOmniboxInput* input =
        [[CWVOmniboxInput alloc] initWithText:@"http://foo.com"
                shouldUseHTTPSAsDefaultScheme:NO];
    NSURL* expected_url = [NSURL URLWithString:@"http://foo.com/"];
    EXPECT_NSEQ(expected_url, input.URL);
  }
  {
    CWVOmniboxInput* input = [[CWVOmniboxInput alloc]
                         initWithText:@"https://user:pass@foo.com/bar?baz#qux"
        shouldUseHTTPSAsDefaultScheme:NO];
    NSURL* expected_url =
        [NSURL URLWithString:@"https://user:pass@foo.com/bar?baz#qux"];
    EXPECT_NSEQ(expected_url, input.URL);
  }
  {
    CWVOmniboxInput* input =
        [[CWVOmniboxInput alloc] initWithText:@"はじめよう.みんな"
                shouldUseHTTPSAsDefaultScheme:NO];
    NSURL* expected_url =
        [NSURL URLWithString:@"http://xn--p8j9a0d9c9a.xn--q9jyb4c/"];
    EXPECT_NSEQ(expected_url, input.URL);
  }
}

TEST_F(CWVOmniboxInputTest, ShouldUseHTTPSAsDefaultSchemeIfRequested) {
  {
    CWVOmniboxInput* input = [[CWVOmniboxInput alloc] initWithText:@"foo.com"
                                     shouldUseHTTPSAsDefaultScheme:YES];
    NSURL* expected_url = [NSURL URLWithString:@"https://foo.com/"];
    EXPECT_NSEQ(expected_url, input.URL);
    EXPECT_TRUE(input.addedHTTPSToTypedURL);
  }
  {
    CWVOmniboxInput* input =
        [[CWVOmniboxInput alloc] initWithText:@"http://foo.com"
                shouldUseHTTPSAsDefaultScheme:YES];
    NSURL* expected_url = [NSURL URLWithString:@"http://foo.com/"];
    EXPECT_NSEQ(expected_url, input.URL);
    EXPECT_FALSE(input.addedHTTPSToTypedURL);
  }
  {
    CWVOmniboxInput* input =
        [[CWVOmniboxInput alloc] initWithText:@"https://foo.com"
                shouldUseHTTPSAsDefaultScheme:YES];
    NSURL* expected_url = [NSURL URLWithString:@"https://foo.com/"];
    EXPECT_NSEQ(expected_url, input.URL);
    EXPECT_FALSE(input.addedHTTPSToTypedURL);
  }
}

TEST_F(CWVOmniboxInputTest, ShouldNotUseHTTPSAsDefaultSchemeIfNotRequested) {
  {
    CWVOmniboxInput* input = [[CWVOmniboxInput alloc] initWithText:@"foo.com"
                                     shouldUseHTTPSAsDefaultScheme:NO];
    NSURL* expected_url = [NSURL URLWithString:@"http://foo.com/"];
    EXPECT_NSEQ(expected_url, input.URL);
    EXPECT_FALSE(input.addedHTTPSToTypedURL);
  }
  {
    CWVOmniboxInput* input =
        [[CWVOmniboxInput alloc] initWithText:@"http://foo.com"
                shouldUseHTTPSAsDefaultScheme:NO];
    NSURL* expected_url = [NSURL URLWithString:@"http://foo.com/"];
    EXPECT_NSEQ(expected_url, input.URL);
    EXPECT_FALSE(input.addedHTTPSToTypedURL);
  }
  {
    CWVOmniboxInput* input =
        [[CWVOmniboxInput alloc] initWithText:@"https://foo.com"
                shouldUseHTTPSAsDefaultScheme:NO];
    NSURL* expected_url = [NSURL URLWithString:@"https://foo.com/"];
    EXPECT_NSEQ(expected_url, input.URL);
    EXPECT_FALSE(input.addedHTTPSToTypedURL);
  }
}

TEST_F(CWVOmniboxInputTest, SupportsHTTPSchemes) {
  {
    CWVOmniboxInput* input =
        [[CWVOmniboxInput alloc] initWithText:@"http://foo.com"
                shouldUseHTTPSAsDefaultScheme:NO];
    EXPECT_EQ(CWVOmniboxInputTypeURL, input.type);
  }
  {
    CWVOmniboxInput* input =
        [[CWVOmniboxInput alloc] initWithText:@"https://foo.com"
                shouldUseHTTPSAsDefaultScheme:NO];
    EXPECT_EQ(CWVOmniboxInputTypeURL, input.type);
  }
}

TEST_F(CWVOmniboxInputTest, SupportsAppStoreSchemes) {
  {
    CWVOmniboxInput* input =
        [[CWVOmniboxInput alloc] initWithText:@"itms://foo.com"
                shouldUseHTTPSAsDefaultScheme:NO];
    EXPECT_EQ(CWVOmniboxInputTypeURL, input.type);
  }
  {
    CWVOmniboxInput* input =
        [[CWVOmniboxInput alloc] initWithText:@"itmss://foo.com"
                shouldUseHTTPSAsDefaultScheme:NO];
    EXPECT_EQ(CWVOmniboxInputTypeURL, input.type);
  }
  {
    CWVOmniboxInput* input =
        [[CWVOmniboxInput alloc] initWithText:@"itms-apps://foo.com"
                shouldUseHTTPSAsDefaultScheme:NO];
    EXPECT_EQ(CWVOmniboxInputTypeURL, input.type);
  }
  {
    CWVOmniboxInput* input =
        [[CWVOmniboxInput alloc] initWithText:@"itms-appss://foo.com"
                shouldUseHTTPSAsDefaultScheme:NO];
    EXPECT_EQ(CWVOmniboxInputTypeURL, input.type);
  }
  {
    CWVOmniboxInput* input =
        [[CWVOmniboxInput alloc] initWithText:@"itms-books://foo.com"
                shouldUseHTTPSAsDefaultScheme:NO];
    EXPECT_EQ(CWVOmniboxInputTypeURL, input.type);
  }
  {
    CWVOmniboxInput* input =
        [[CWVOmniboxInput alloc] initWithText:@"itms-bookss://foo.com"
                shouldUseHTTPSAsDefaultScheme:NO];
    EXPECT_EQ(CWVOmniboxInputTypeURL, input.type);
  }
}

TEST_F(CWVOmniboxInputTest, DoesNotSupportOtherSchemes) {
  {
    CWVOmniboxInput* input =
        [[CWVOmniboxInput alloc] initWithText:@"about:blank"
                shouldUseHTTPSAsDefaultScheme:NO];
    EXPECT_EQ(CWVOmniboxInputTypeUnknown, input.type);
  }
  {
    CWVOmniboxInput* input =
        [[CWVOmniboxInput alloc] initWithText:@"chrome://version"
                shouldUseHTTPSAsDefaultScheme:NO];
    EXPECT_EQ(CWVOmniboxInputTypeUnknown, input.type);
  }
  {
    CWVOmniboxInput* input =
        [[CWVOmniboxInput alloc] initWithText:@"javascript:foo"
                shouldUseHTTPSAsDefaultScheme:NO];
    EXPECT_EQ(CWVOmniboxInputTypeUnknown, input.type);
  }
  {
    CWVOmniboxInput* input =
        [[CWVOmniboxInput alloc] initWithText:@"file:///foo"
                shouldUseHTTPSAsDefaultScheme:NO];
    EXPECT_EQ(CWVOmniboxInputTypeQuery, input.type);
  }
  {
    CWVOmniboxInput* input =
        [[CWVOmniboxInput alloc] initWithText:@"data:,Hello%2C%20World%21"
                shouldUseHTTPSAsDefaultScheme:NO];
    EXPECT_EQ(CWVOmniboxInputTypeUnknown, input.type);
  }
  {
    CWVOmniboxInput* input =
        [[CWVOmniboxInput alloc] initWithText:@"ftp://foo.com"
                shouldUseHTTPSAsDefaultScheme:NO];
    EXPECT_EQ(CWVOmniboxInputTypeUnknown, input.type);
  }
}

}  // namespace ios_web_view
