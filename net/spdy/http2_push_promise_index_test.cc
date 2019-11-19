// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/spdy/http2_push_promise_index.h"

#include "net/base/host_port_pair.h"
#include "net/base/privacy_mode.h"
#include "net/socket/socket_tag.h"
#include "net/test/gtest_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

// For simplicity, these tests do not create SpdySession instances
// (necessary for a non-null WeakPtr<SpdySession>), instead they use nullptr.
// Streams are identified by spdy::SpdyStreamId only.

using ::testing::Return;
using ::testing::_;

namespace net {
namespace test {
namespace {

// Delegate implementation for tests that requires exact match of SpdySessionKey
// in ValidatePushedStream().  Note that SpdySession, unlike TestDelegate,
// allows cross-origin pooling.
class TestDelegate : public Http2PushPromiseIndex::Delegate {
 public:
  TestDelegate() = delete;
  explicit TestDelegate(const SpdySessionKey& key) : key_(key) {}
  ~TestDelegate() override {}

  bool ValidatePushedStream(spdy::SpdyStreamId stream_id,
                            const GURL& url,
                            const HttpRequestInfo& request_info,
                            const SpdySessionKey& key) const override {
    return key == key_;
  }

  base::WeakPtr<SpdySession> GetWeakPtrToSession() override { return nullptr; }

 private:
  SpdySessionKey key_;
};

}  // namespace

class Http2PushPromiseIndexPeer {
 public:
  using UnclaimedPushedStream = Http2PushPromiseIndex::UnclaimedPushedStream;
  using CompareByUrl = Http2PushPromiseIndex::CompareByUrl;
};

class Http2PushPromiseIndexTest : public testing::Test {
 protected:
  Http2PushPromiseIndexTest()
      : url1_("https://www.example.org"),
        url2_("https://mail.example.com"),
        key1_(HostPortPair::FromURL(url1_),
              ProxyServer::Direct(),
              PRIVACY_MODE_ENABLED,
              SpdySessionKey::IsProxySession::kFalse,
              SocketTag(),
              NetworkIsolationKey(),
              false /* disable_secure_dns */),
        key2_(HostPortPair::FromURL(url2_),
              ProxyServer::Direct(),
              PRIVACY_MODE_ENABLED,
              SpdySessionKey::IsProxySession::kFalse,
              SocketTag(),
              NetworkIsolationKey(),
              false /* disable_secure_dns */) {}

  const GURL url1_;
  const GURL url2_;
  const SpdySessionKey key1_;
  const SpdySessionKey key2_;
  Http2PushPromiseIndex index_;
};

// RegisterUnclaimedPushedStream() returns false
// if there is already a registered entry with same delegate and URL.
TEST_F(Http2PushPromiseIndexTest, CannotRegisterSameEntryTwice) {
  TestDelegate delegate(key1_);
  EXPECT_TRUE(index_.RegisterUnclaimedPushedStream(url1_, 2, &delegate));
  EXPECT_FALSE(index_.RegisterUnclaimedPushedStream(url1_, 4, &delegate));
  // Unregister first entry so that DCHECK() does not fail in destructor.
  EXPECT_TRUE(index_.UnregisterUnclaimedPushedStream(url1_, 2, &delegate));
}

// UnregisterUnclaimedPushedStream() returns false
// if there is no identical entry registered.
// Case 1: no streams for the given URL.
TEST_F(Http2PushPromiseIndexTest, CannotUnregisterNonexistingEntry) {
  TestDelegate delegate(key1_);
  EXPECT_FALSE(index_.UnregisterUnclaimedPushedStream(url1_, 2, &delegate));
}

// UnregisterUnclaimedPushedStream() returns false
// if there is no identical entry registered.
// Case 2: there is a stream for the given URL with the same Delegate,
// but the stream ID does not match.
TEST_F(Http2PushPromiseIndexTest, CannotUnregisterEntryIfStreamIdDoesNotMatch) {
  TestDelegate delegate(key1_);
  EXPECT_TRUE(index_.RegisterUnclaimedPushedStream(url1_, 2, &delegate));
  EXPECT_FALSE(index_.UnregisterUnclaimedPushedStream(url1_, 4, &delegate));
  // Unregister first entry so that DCHECK() does not fail in destructor.
  EXPECT_TRUE(index_.UnregisterUnclaimedPushedStream(url1_, 2, &delegate));
}

// UnregisterUnclaimedPushedStream() returns false
// if there is no identical entry registered.
// Case 3: there is a stream for the given URL with the same stream ID,
// but the delegate does not match.
TEST_F(Http2PushPromiseIndexTest, CannotUnregisterEntryIfDelegateDoesNotMatch) {
  TestDelegate delegate1(key1_);
  TestDelegate delegate2(key2_);
  EXPECT_TRUE(index_.RegisterUnclaimedPushedStream(url1_, 2, &delegate1));
  EXPECT_FALSE(index_.UnregisterUnclaimedPushedStream(url1_, 2, &delegate2));
  // Unregister first entry so that DCHECK() does not fail in destructor.
  EXPECT_TRUE(index_.UnregisterUnclaimedPushedStream(url1_, 2, &delegate1));
}

TEST_F(Http2PushPromiseIndexTest, CountStreamsForSession) {
  TestDelegate delegate1(key1_);
  TestDelegate delegate2(key2_);

  EXPECT_EQ(0u, index_.CountStreamsForSession(&delegate1));
  EXPECT_EQ(0u, index_.CountStreamsForSession(&delegate2));

  EXPECT_TRUE(index_.RegisterUnclaimedPushedStream(url1_, 2, &delegate1));

  EXPECT_EQ(1u, index_.CountStreamsForSession(&delegate1));
  EXPECT_EQ(0u, index_.CountStreamsForSession(&delegate2));

  EXPECT_TRUE(index_.RegisterUnclaimedPushedStream(url2_, 4, &delegate1));

  EXPECT_EQ(2u, index_.CountStreamsForSession(&delegate1));
  EXPECT_EQ(0u, index_.CountStreamsForSession(&delegate2));

  EXPECT_TRUE(index_.RegisterUnclaimedPushedStream(url1_, 6, &delegate2));

  EXPECT_EQ(2u, index_.CountStreamsForSession(&delegate1));
  EXPECT_EQ(1u, index_.CountStreamsForSession(&delegate2));

  EXPECT_TRUE(index_.UnregisterUnclaimedPushedStream(url1_, 2, &delegate1));

  EXPECT_EQ(1u, index_.CountStreamsForSession(&delegate1));
  EXPECT_EQ(1u, index_.CountStreamsForSession(&delegate2));

  EXPECT_TRUE(index_.UnregisterUnclaimedPushedStream(url2_, 4, &delegate1));

  EXPECT_EQ(0u, index_.CountStreamsForSession(&delegate1));
  EXPECT_EQ(1u, index_.CountStreamsForSession(&delegate2));

  EXPECT_TRUE(index_.UnregisterUnclaimedPushedStream(url1_, 6, &delegate2));

  EXPECT_EQ(0u, index_.CountStreamsForSession(&delegate1));
  EXPECT_EQ(0u, index_.CountStreamsForSession(&delegate2));
}

TEST_F(Http2PushPromiseIndexTest, FindStream) {
  TestDelegate delegate1(key1_);
  TestDelegate delegate2(key2_);

  EXPECT_EQ(kNoPushedStreamFound, index_.FindStream(url1_, &delegate1));
  EXPECT_EQ(kNoPushedStreamFound, index_.FindStream(url2_, &delegate1));
  EXPECT_EQ(kNoPushedStreamFound, index_.FindStream(url1_, &delegate2));
  EXPECT_EQ(kNoPushedStreamFound, index_.FindStream(url2_, &delegate2));

  EXPECT_TRUE(index_.RegisterUnclaimedPushedStream(url1_, 2, &delegate1));

  EXPECT_EQ(2u, index_.FindStream(url1_, &delegate1));
  EXPECT_EQ(kNoPushedStreamFound, index_.FindStream(url2_, &delegate1));
  EXPECT_EQ(kNoPushedStreamFound, index_.FindStream(url1_, &delegate2));
  EXPECT_EQ(kNoPushedStreamFound, index_.FindStream(url2_, &delegate2));

  EXPECT_TRUE(index_.RegisterUnclaimedPushedStream(url2_, 4, &delegate1));

  EXPECT_EQ(2u, index_.FindStream(url1_, &delegate1));
  EXPECT_EQ(4u, index_.FindStream(url2_, &delegate1));
  EXPECT_EQ(kNoPushedStreamFound, index_.FindStream(url1_, &delegate2));
  EXPECT_EQ(kNoPushedStreamFound, index_.FindStream(url2_, &delegate2));

  EXPECT_TRUE(index_.RegisterUnclaimedPushedStream(url1_, 6, &delegate2));

  EXPECT_EQ(2u, index_.FindStream(url1_, &delegate1));
  EXPECT_EQ(4u, index_.FindStream(url2_, &delegate1));
  EXPECT_EQ(6u, index_.FindStream(url1_, &delegate2));
  EXPECT_EQ(kNoPushedStreamFound, index_.FindStream(url2_, &delegate2));

  EXPECT_TRUE(index_.UnregisterUnclaimedPushedStream(url1_, 2, &delegate1));

  EXPECT_EQ(kNoPushedStreamFound, index_.FindStream(url1_, &delegate1));
  EXPECT_EQ(4u, index_.FindStream(url2_, &delegate1));
  EXPECT_EQ(6u, index_.FindStream(url1_, &delegate2));
  EXPECT_EQ(kNoPushedStreamFound, index_.FindStream(url2_, &delegate2));

  EXPECT_TRUE(index_.UnregisterUnclaimedPushedStream(url2_, 4, &delegate1));

  EXPECT_EQ(kNoPushedStreamFound, index_.FindStream(url1_, &delegate1));
  EXPECT_EQ(kNoPushedStreamFound, index_.FindStream(url2_, &delegate1));
  EXPECT_EQ(6u, index_.FindStream(url1_, &delegate2));
  EXPECT_EQ(kNoPushedStreamFound, index_.FindStream(url2_, &delegate2));

  EXPECT_TRUE(index_.UnregisterUnclaimedPushedStream(url1_, 6, &delegate2));

  EXPECT_EQ(kNoPushedStreamFound, index_.FindStream(url1_, &delegate1));
  EXPECT_EQ(kNoPushedStreamFound, index_.FindStream(url2_, &delegate1));
  EXPECT_EQ(kNoPushedStreamFound, index_.FindStream(url1_, &delegate2));
  EXPECT_EQ(kNoPushedStreamFound, index_.FindStream(url2_, &delegate2));
}

// If |index_| is empty, then ClaimPushedStream() should set its |stream_id|
// outparam to kNoPushedStreamFound for any values of inparams.
TEST_F(Http2PushPromiseIndexTest, Empty) {
  base::WeakPtr<SpdySession> session;
  spdy::SpdyStreamId stream_id = 2;
  index_.ClaimPushedStream(key1_, url1_, HttpRequestInfo(), &session,
                           &stream_id);
  EXPECT_EQ(kNoPushedStreamFound, stream_id);

  stream_id = 2;
  index_.ClaimPushedStream(key1_, url2_, HttpRequestInfo(), &session,
                           &stream_id);
  EXPECT_EQ(kNoPushedStreamFound, stream_id);

  stream_id = 2;
  index_.ClaimPushedStream(key1_, url2_, HttpRequestInfo(), &session,
                           &stream_id);
  EXPECT_EQ(kNoPushedStreamFound, stream_id);

  stream_id = 2;
  index_.ClaimPushedStream(key2_, url2_, HttpRequestInfo(), &session,
                           &stream_id);
  EXPECT_EQ(kNoPushedStreamFound, stream_id);
}

// Create two entries, both with a delegate that requires |key| to be equal to
// |key1_|.  Register the two entries with different URLs.  Check that they can
// be found by their respective URLs.
TEST_F(Http2PushPromiseIndexTest, FindMultipleStreamsWithDifferentUrl) {
  // Register first entry.
  TestDelegate delegate1(key1_);
  EXPECT_TRUE(index_.RegisterUnclaimedPushedStream(url1_, 2, &delegate1));

  // No entry found for |url2_|.
  base::WeakPtr<SpdySession> session;
  spdy::SpdyStreamId stream_id = 2;
  index_.ClaimPushedStream(key1_, url2_, HttpRequestInfo(), &session,
                           &stream_id);
  EXPECT_EQ(kNoPushedStreamFound, stream_id);

  // Claim first entry.
  stream_id = kNoPushedStreamFound;
  index_.ClaimPushedStream(key1_, url1_, HttpRequestInfo(), &session,
                           &stream_id);
  EXPECT_EQ(2u, stream_id);

  // ClaimPushedStream() unregistered first entry, cannot claim it again.
  stream_id = 2;
  index_.ClaimPushedStream(key1_, url1_, HttpRequestInfo(), &session,
                           &stream_id);
  EXPECT_EQ(kNoPushedStreamFound, stream_id);

  // Register two entries.  Second entry uses same key.
  EXPECT_TRUE(index_.RegisterUnclaimedPushedStream(url1_, 2, &delegate1));
  TestDelegate delegate2(key1_);
  EXPECT_TRUE(index_.RegisterUnclaimedPushedStream(url2_, 4, &delegate2));

  // Retrieve each entry by their respective URLs.
  stream_id = kNoPushedStreamFound;
  index_.ClaimPushedStream(key1_, url1_, HttpRequestInfo(), &session,
                           &stream_id);
  EXPECT_EQ(2u, stream_id);

  stream_id = kNoPushedStreamFound;
  index_.ClaimPushedStream(key1_, url2_, HttpRequestInfo(), &session,
                           &stream_id);
  EXPECT_EQ(4u, stream_id);

  // ClaimPushedStream() calls unregistered both entries,
  // cannot claim them again.
  stream_id = 2;
  index_.ClaimPushedStream(key1_, url1_, HttpRequestInfo(), &session,
                           &stream_id);
  EXPECT_EQ(kNoPushedStreamFound, stream_id);

  stream_id = 2;
  index_.ClaimPushedStream(key1_, url2_, HttpRequestInfo(), &session,
                           &stream_id);
  EXPECT_EQ(kNoPushedStreamFound, stream_id);

  EXPECT_FALSE(index_.UnregisterUnclaimedPushedStream(url1_, 2, &delegate1));
  EXPECT_FALSE(index_.UnregisterUnclaimedPushedStream(url2_, 4, &delegate2));
}

// Create two entries with delegates that validate different SpdySessionKeys.
// Register the two entries with the same URL.  Check that they can be found by
// their respective SpdySessionKeys.
TEST_F(Http2PushPromiseIndexTest, MultipleStreamsWithDifferentKeys) {
  // Register first entry.
  TestDelegate delegate1(key1_);
  EXPECT_TRUE(index_.RegisterUnclaimedPushedStream(url1_, 2, &delegate1));

  // No entry found for |key2_|.
  base::WeakPtr<SpdySession> session;
  spdy::SpdyStreamId stream_id = 2;
  index_.ClaimPushedStream(key2_, url1_, HttpRequestInfo(), &session,
                           &stream_id);
  EXPECT_EQ(kNoPushedStreamFound, stream_id);

  // Claim first entry.
  stream_id = kNoPushedStreamFound;
  index_.ClaimPushedStream(key1_, url1_, HttpRequestInfo(), &session,
                           &stream_id);
  EXPECT_EQ(2u, stream_id);

  // ClaimPushedStream() unregistered first entry, cannot claim it again.
  stream_id = 2;
  index_.ClaimPushedStream(key1_, url1_, HttpRequestInfo(), &session,
                           &stream_id);
  EXPECT_EQ(kNoPushedStreamFound, stream_id);

  // Register two entries.  Second entry uses same URL.
  EXPECT_TRUE(index_.RegisterUnclaimedPushedStream(url1_, 2, &delegate1));
  TestDelegate delegate2(key2_);
  EXPECT_TRUE(index_.RegisterUnclaimedPushedStream(url1_, 4, &delegate2));

  // Retrieve each entry by their respective SpdySessionKeys.
  stream_id = kNoPushedStreamFound;
  index_.ClaimPushedStream(key1_, url1_, HttpRequestInfo(), &session,
                           &stream_id);
  EXPECT_EQ(2u, stream_id);

  stream_id = kNoPushedStreamFound;
  index_.ClaimPushedStream(key2_, url1_, HttpRequestInfo(), &session,
                           &stream_id);
  EXPECT_EQ(4u, stream_id);

  // ClaimPushedStream() calls unregistered both entries,
  // cannot claim them again.
  stream_id = 2;
  index_.ClaimPushedStream(key1_, url1_, HttpRequestInfo(), &session,
                           &stream_id);
  EXPECT_EQ(kNoPushedStreamFound, stream_id);

  stream_id = 2;
  index_.ClaimPushedStream(key2_, url1_, HttpRequestInfo(), &session,
                           &stream_id);
  EXPECT_EQ(kNoPushedStreamFound, stream_id);

  EXPECT_FALSE(index_.UnregisterUnclaimedPushedStream(url1_, 2, &delegate1));
  EXPECT_FALSE(index_.UnregisterUnclaimedPushedStream(url1_, 4, &delegate2));
}

TEST_F(Http2PushPromiseIndexTest, MultipleMatchingStreams) {
  // Register two entries with identical URLs that have delegates that accept
  // the same SpdySessionKey.
  TestDelegate delegate1(key1_);
  TestDelegate delegate2(key1_);
  EXPECT_TRUE(index_.RegisterUnclaimedPushedStream(url1_, 2, &delegate1));
  EXPECT_TRUE(index_.RegisterUnclaimedPushedStream(url1_, 4, &delegate2));

  // Test that ClaimPushedStream() returns one of the two entries.
  // ClaimPushedStream() makes no guarantee about which entry it returns if
  // there are multiple matches.
  base::WeakPtr<SpdySession> session;
  spdy::SpdyStreamId stream_id1 = kNoPushedStreamFound;
  index_.ClaimPushedStream(key1_, url1_, HttpRequestInfo(), &session,
                           &stream_id1);
  EXPECT_NE(kNoPushedStreamFound, stream_id1);

  // First call to ClaimPushedStream() unregistered one of the entries.
  // Second call to ClaimPushedStream() must return the other entry.
  spdy::SpdyStreamId stream_id2 = kNoPushedStreamFound;
  index_.ClaimPushedStream(key1_, url1_, HttpRequestInfo(), &session,
                           &stream_id2);
  EXPECT_NE(kNoPushedStreamFound, stream_id2);
  EXPECT_NE(stream_id1, stream_id2);

  // Two calls to ClaimPushedStream() unregistered both entries.
  spdy::SpdyStreamId stream_id3 = 2;
  index_.ClaimPushedStream(key1_, url1_, HttpRequestInfo(), &session,
                           &stream_id3);
  EXPECT_EQ(kNoPushedStreamFound, stream_id3);

  EXPECT_FALSE(index_.UnregisterUnclaimedPushedStream(url1_, 2, &delegate1));
  EXPECT_FALSE(index_.UnregisterUnclaimedPushedStream(url1_, 4, &delegate2));
}

// Test that an entry is equivalent to itself.
TEST(Http2PushPromiseIndexCompareByUrlTest, Reflexivity) {
  // Test with two entries: with and without a pushed stream.
  Http2PushPromiseIndexPeer::UnclaimedPushedStream entry1{GURL(), nullptr, 2};
  Http2PushPromiseIndexPeer::UnclaimedPushedStream entry2{GURL(), nullptr,
                                                          kNoPushedStreamFound};

  // For "Compare", it is a requirement that comp(A, A) == false, see
  // http://en.cppreference.com/w/cpp/concept/Compare.  This will in fact imply
  // that equiv(A, A) == true.
  EXPECT_FALSE(Http2PushPromiseIndexPeer::CompareByUrl()(entry1, entry1));
  EXPECT_FALSE(Http2PushPromiseIndexPeer::CompareByUrl()(entry2, entry2));

  std::set<Http2PushPromiseIndexPeer::UnclaimedPushedStream,
           Http2PushPromiseIndexPeer::CompareByUrl>
      entries;
  bool success;
  std::tie(std::ignore, success) = entries.insert(entry1);
  EXPECT_TRUE(success);

  // Test that |entry1| is considered equivalent to itself by ensuring that
  // a second insertion fails.
  std::tie(std::ignore, success) = entries.insert(entry1);
  EXPECT_FALSE(success);

  // Test that |entry1| and |entry2| are not equivalent.
  std::tie(std::ignore, success) = entries.insert(entry2);
  EXPECT_TRUE(success);

  // Test that |entry2| is equivalent to an existing entry
  // (which then must be |entry2|).
  std::tie(std::ignore, success) = entries.insert(entry2);
  EXPECT_FALSE(success);
}

TEST(Http2PushPromiseIndexCompareByUrlTest, LookupByURL) {
  const GURL url1("https://example.com:1");
  const GURL url2("https://example.com:2");
  const GURL url3("https://example.com:3");
  // This test relies on the order of these GURLs.
  ASSERT_LT(url1, url2);
  ASSERT_LT(url2, url3);

  // Create four entries, two for the middle URL, with distinct stream IDs not
  // in ascending order.
  Http2PushPromiseIndexPeer::UnclaimedPushedStream entry1{url1, nullptr, 8};
  Http2PushPromiseIndexPeer::UnclaimedPushedStream entry2{url2, nullptr, 4};
  Http2PushPromiseIndexPeer::UnclaimedPushedStream entry3{url2, nullptr, 6};
  Http2PushPromiseIndexPeer::UnclaimedPushedStream entry4{url3, nullptr, 2};

  // Fill up a set.
  std::set<Http2PushPromiseIndexPeer::UnclaimedPushedStream,
           Http2PushPromiseIndexPeer::CompareByUrl>
      entries;
  entries.insert(entry1);
  entries.insert(entry2);
  entries.insert(entry3);
  entries.insert(entry4);
  ASSERT_EQ(4u, entries.size());

  // Test that entries are ordered by URL first, not stream ID.
  auto it = entries.begin();
  EXPECT_EQ(8u, it->stream_id);
  ++it;
  EXPECT_EQ(4u, it->stream_id);
  ++it;
  EXPECT_EQ(6u, it->stream_id);
  ++it;
  EXPECT_EQ(2u, it->stream_id);
  ++it;
  EXPECT_TRUE(it == entries.end());

  // Test that kNoPushedStreamFound can be used to look up the first entry for a
  // given URL.  In particular, the first entry with |url2| is |entry2|.
  EXPECT_TRUE(
      entries.lower_bound(Http2PushPromiseIndexPeer::UnclaimedPushedStream{
          url2, nullptr, kNoPushedStreamFound}) == entries.find(entry2));
}

}  // namespace test
}  // namespace net
