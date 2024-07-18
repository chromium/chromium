// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/spdy/header_coalescer.h"

#include <string>
#include <string_view>
#include <vector>

#include "net/log/net_log.h"
#include "net/log/test_net_log.h"
#include "net/log/test_net_log_util.h"
#include "net/spdy/spdy_test_util_common.h"
#include "net/third_party/quiche/src/quiche/common/http/http_header_block.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::ElementsAre;
using ::testing::Pair;

namespace net::test {

class HeaderCoalescerTest : public ::testing::Test {
 public:
  HeaderCoalescerTest()
      : header_coalescer_(kMaxHeaderListSizeForTest, net_log_with_source_) {}

  void ExpectEntry(std::string_view expected_header_name,
                   std::string_view expected_header_value,
                   std::string_view expected_error_message) {
    auto entry_list = net_log_observer_.GetEntries();
    ASSERT_EQ(1u, entry_list.size());
    EXPECT_EQ(entry_list[0].type,
              NetLogEventType::HTTP2_SESSION_RECV_INVALID_HEADER);
    EXPECT_EQ(entry_list[0].source.id, net_log_with_source_.source().id);
    std::string value;
    EXPECT_EQ(expected_header_name,
              GetStringValueFromParams(entry_list[0], "header_name"));
    EXPECT_EQ(expected_header_value,
              GetStringValueFromParams(entry_list[0], "header_value"));
    EXPECT_EQ(expected_error_message,
              GetStringValueFromParams(entry_list[0], "error"));
  }

 protected:
  NetLogWithSource net_log_with_source_{
      NetLogWithSource::Make(NetLog::Get(), NetLogSourceType::NONE)};
  RecordingNetLogObserver net_log_observer_;
  HeaderCoalescer header_coalescer_;
};

TEST_F(HeaderCoalescerTest, CorrectHeaders) {
  header_coalescer_.OnHeader(":foo", "bar");
  header_coalescer_.OnHeader("baz", "qux");
  EXPECT_FALSE(header_coalescer_.error_seen());

  quiche::HttpHeaderBlock header_block = header_coalescer_.release_headers();
  EXPECT_THAT(header_block,
              ElementsAre(Pair(":foo", "bar"), Pair("baz", "qux")));
}

TEST_F(HeaderCoalescerTest, EmptyHeaderKey) {
  header_coalescer_.OnHeader("", "foo");
  EXPECT_TRUE(header_coalescer_.error_seen());
  ExpectEntry("", "foo", "Header name must not be empty.");
}

TEST_F(HeaderCoalescerTest, HeaderBlockTooLarge) {
  // key + value + overhead = 3 + kMaxHeaderListSizeForTest - 40 + 32
  // = kMaxHeaderListSizeForTest - 5
  std::string data(kMaxHeaderListSizeForTest - 40, 'a');
  header_coalescer_.OnHeader("foo", data);
  EXPECT_FALSE(header_coalescer_.error_seen());

  // Another 3 + 4 + 32 bytes: too large.
  header_coalescer_.OnHeader("bar", "abcd");
  EXPECT_TRUE(header_coalescer_.error_seen());
  ExpectEntry("bar", "abcd", "Header list too large.");
}

TEST_F(HeaderCoalescerTest, PseudoHeadersMustNotFollowRegularHeaders) {
  header_coalescer_.OnHeader("foo", "bar");
  EXPECT_FALSE(header_coalescer_.error_seen());
  header_coalescer_.OnHeader(":baz", "qux");
  EXPECT_TRUE(header_coalescer_.error_seen());
  ExpectEntry(":baz", "qux", "Pseudo header must not follow regular headers.");
}

TEST_F(HeaderCoalescerTest, Append) {
  header_coalescer_.OnHeader("foo", "bar");
  header_coalescer_.OnHeader("cookie", "baz");
  header_coalescer_.OnHeader("foo", "quux");
  header_coalescer_.OnHeader("cookie", "qux");
  EXPECT_FALSE(header_coalescer_.error_seen());

  quiche::HttpHeaderBlock header_block = header_coalescer_.release_headers();
  EXPECT_THAT(header_block,
              ElementsAre(Pair("foo", std::string_view("bar\0quux", 8)),
                          Pair("cookie", "baz; qux")));
}

TEST_F(HeaderCoalescerTest, HeaderNameNotValid) {
  std::string_view header_name("\x1\x7F\x80\xFF");
  header_coalescer_.OnHeader(header_name, "foo");
  EXPECT_TRUE(header_coalescer_.error_seen());
  ExpectEntry("%ESCAPED:\xE2\x80\x8B \x1\x7F%80%FF", "foo",
              "Invalid character in header name.");
}

// RFC 7540 Section 8.1.2.6. Uppercase in header name is invalid.
TEST_F(HeaderCoalescerTest, HeaderNameHasUppercase) {
  std::string_view header_name("ABCDEFGHIJKLMNOPQRSTUVWXYZ");
  header_coalescer_.OnHeader(header_name, "foo");
  EXPECT_TRUE(header_coalescer_.error_seen());
  ExpectEntry("ABCDEFGHIJKLMNOPQRSTUVWXYZ", "foo",
              "Upper case characters in header name.");
}

// RFC 7230 Section 3.2. Valid header name is defined as:
// field-name     = token
// token          = 1*tchar
// tchar          = "!" / "#" / "$" / "%" / "&" / "'" / "*" / "+" / "-" / "." /
//                  "^" / "_" / "`" / "|" / "~" / DIGIT / ALPHA
TEST_F(HeaderCoalescerTest, HeaderNameValid) {
  // Due to RFC 7540 Section 8.1.2.6. Uppercase characters are not included.
  std::string_view header_name(
      "abcdefghijklmnopqrstuvwxyz0123456789!#$%&'*+-."
      "^_`|~");
  header_coalescer_.OnHeader(header_name, "foo");
  EXPECT_FALSE(header_coalescer_.error_seen());
  quiche::HttpHeaderBlock header_block = header_coalescer_.release_headers();
  EXPECT_THAT(header_block, ElementsAre(Pair(header_name, "foo")));
}

// According to RFC 7540 Section 10.3 and RFC 7230 Section 3.2, allowed
// characters in header values are '\t', '  ', 0x21 to 0x7E, and 0x80 to 0xFF.
TEST_F(HeaderCoalescerTest, HeaderValueValid) {
  header_coalescer_.OnHeader("foo", " bar \x21 \x7e baz\tqux\x80\xff ");
  EXPECT_FALSE(header_coalescer_.error_seen());
}

TEST_F(HeaderCoalescerTest, HeaderValueContainsLF) {
  header_coalescer_.OnHeader("foo", "bar\nbaz");
  EXPECT_TRUE(header_coalescer_.error_seen());
  ExpectEntry("foo", "bar\nbaz", "Invalid character 0x0A in header value.");
}

TEST_F(HeaderCoalescerTest, HeaderValueContainsCR) {
  header_coalescer_.OnHeader("foo", "bar\rbaz");
  EXPECT_TRUE(header_coalescer_.error_seen());
  ExpectEntry("foo", "bar\rbaz", "Invalid character 0x0D in header value.");
}

TEST_F(HeaderCoalescerTest, HeaderValueContains0x7f) {
  header_coalescer_.OnHeader("foo", "bar\x7f baz");
  EXPECT_TRUE(header_coalescer_.error_seen());
  ExpectEntry("foo", "bar\x7F baz", "Invalid character 0x7F in header value.");
}

}  // namespace net::test
