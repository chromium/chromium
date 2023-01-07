// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/history_util.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"

namespace blink {

class HistoryUtilTest : public testing::Test {};

TEST_F(HistoryUtilTest, CanChangeToURL) {
  struct TestCase {
    const char* url;
    const char* document_url;
    bool expected;
  } cases[] = {
      {"http://example.com/", "http://example.com/", true},
      {"http://example.com/#hash", "http://example.com/", true},
      {"http://example.com/path", "http://example.com/", true},
      {"http://example.com/path#hash", "http://example.com/", true},
      {"http://example.com/path?query", "http://example.com/", true},
      {"http://example.com/path?query#hash", "http://example.com/", true},
      {"http://example.com:80/", "http://example.com/", true},
      {"http://example.com:80/#hash", "http://example.com/", true},
      {"http://example.com:80/path", "http://example.com/", true},
      {"http://example.com:80/path#hash", "http://example.com/", true},
      {"http://example.com:80/path?query", "http://example.com/", true},
      {"http://example.com:80/path?query#hash", "http://example.com/", true},
      {"http://not-example.com:80/", "http://example.com/", false},
      {"http://not-example.com:80/#hash", "http://example.com/", false},
      {"http://not-example.com:80/path", "http://example.com/", false},
      {"http://not-example.com:80/path#hash", "http://example.com/", false},
      {"http://not-example.com:80/path?query", "http://example.com/", false},
      {"http://not-example.com:80/path?query#hash", "http://example.com/",
       false},
      {"http://example.com:81/", "http://example.com/", false},
      {"http://example.com:81/#hash", "http://example.com/", false},
      {"http://example.com:81/path", "http://example.com/", false},
      {"http://example.com:81/path#hash", "http://example.com/", false},
      {"http://example.com:81/path?query", "http://example.com/", false},
      {"http://example.com:81/path?query#hash", "http://example.com/", false},
  };

  for (const auto& test : cases) {
    KURL url(test.url);
    KURL document_url(test.document_url);
    scoped_refptr<const SecurityOrigin> document_origin =
        SecurityOrigin::Create(document_url);
    EXPECT_EQ(test.expected, CanChangeToUrlForHistoryApi(
                                 url, document_origin.get(), document_url));
  }
}

TEST_F(HistoryUtilTest, CanChangeToURLInFileOrigin) {
  struct TestCase {
    const char* url;
    const char* document_url;
    bool expected;
  } cases[] = {
      {"file:///path/to/file/", "file:///path/to/file/", true},
      {"file:///path/to/file/#hash", "file:///path/to/file/", true},
      {"file:///path/to/file/path", "file:///path/to/file/", false},
      {"file:///path/to/file/path#hash", "file:///path/to/file/", false},
      {"file:///path/to/file/path?query", "file:///path/to/file/", false},
      {"file:///path/to/file/path?query#hash", "file:///path/to/file/", false},
  };

  for (const auto& test : cases) {
    KURL url(test.url);
    KURL document_url(test.document_url);
    scoped_refptr<const SecurityOrigin> document_origin =
        SecurityOrigin::Create(document_url);
    EXPECT_EQ(test.expected, CanChangeToUrlForHistoryApi(
                                 url, document_origin.get(), document_url));
  }
}

TEST_F(HistoryUtilTest, CanChangeToURLInUniqueOrigin) {
  struct TestCase {
    const char* url;
    const char* document_url;
    bool expected;
  } cases[] = {
      {"http://example.com/", "http://example.com/", true},
      {"http://example.com/#hash", "http://example.com/", true},
      {"http://example.com/path", "http://example.com/", false},
      {"http://example.com/path#hash", "http://example.com/", false},
      {"http://example.com/path?query", "http://example.com/", false},
      {"http://example.com/path?query#hash", "http://example.com/", false},
      {"http://example.com:80/", "http://example.com/", true},
      {"http://example.com:80/#hash", "http://example.com/", true},
      {"http://example.com:80/path", "http://example.com/", false},
      {"http://example.com:80/path#hash", "http://example.com/", false},
      {"http://example.com:80/path?query", "http://example.com/", false},
      {"http://example.com:80/path?query#hash", "http://example.com/", false},
      {"http://example.com:81/", "http://example.com/", false},
      {"http://example.com:81/#hash", "http://example.com/", false},
      {"http://example.com:81/path", "http://example.com/", false},
      {"http://example.com:81/path#hash", "http://example.com/", false},
      {"http://example.com:81/path?query", "http://example.com/", false},
      {"http://example.com:81/path?query#hash", "http://example.com/", false},
  };

  for (const auto& test : cases) {
    KURL url(test.url);
    KURL document_url(test.document_url);
    scoped_refptr<const SecurityOrigin> document_origin =
        SecurityOrigin::CreateUniqueOpaque();
    EXPECT_EQ(test.expected, CanChangeToUrlForHistoryApi(
                                 url, document_origin.get(), document_url));
  }
}

}  // namespace blink
