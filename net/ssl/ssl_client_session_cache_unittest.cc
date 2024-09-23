// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/ssl/ssl_client_session_cache.h"

#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/simple_test_clock.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/trace_event/memory_allocator_dump.h"
#include "base/trace_event/process_memory_dump.h"
#include "net/base/network_anonymization_key.h"
#include "net/base/schemeful_site.h"
#include "net/base/tracing.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/boringssl/src/include/openssl/ssl.h"
#include "url/gurl.h"

using testing::ByRef;
using testing::Contains;
using testing::Eq;
using testing::Field;

namespace net {

namespace {

std::unique_ptr<base::SimpleTestClock> MakeTestClock() {
  std::unique_ptr<base::SimpleTestClock> clock =
      std::make_unique<base::SimpleTestClock>();
  // SimpleTestClock starts at the null base::Time which converts to and from
  // time_t confusingly.
  clock->SetNow(base::Time::FromTimeT(1000000000));
  return clock;
}

SSLClientSessionCache::Key MakeTestKey(const std::string& str) {
  SSLClientSessionCache::Key key;
  key.server = HostPortPair(str, 443);
  return key;
}

class SSLClientSessionCacheTest : public testing::Test {
 public:
  SSLClientSessionCacheTest() : ssl_ctx_(SSL_CTX_new(TLS_method())) {}

 protected:
  bssl::UniquePtr<SSL_SESSION> NewSSLSession(
      uint16_t version = TLS1_2_VERSION) {
    SSL_SESSION* session = SSL_SESSION_new(ssl_ctx_.get());
    if (!SSL_SESSION_set_protocol_version(session, version))
      return nullptr;
    return bssl::UniquePtr<SSL_SESSION>(session);
  }

  bssl::UniquePtr<SSL_SESSION> MakeTestSession(base::Time now,
                                               base::TimeDelta timeout) {
    bssl::UniquePtr<SSL_SESSION> session = NewSSLSession();
    SSL_SESSION_set_time(session.get(), now.ToTimeT());
    SSL_SESSION_set_timeout(session.get(), timeout.InSeconds());
    return session;
  }

 private:
  bssl::UniquePtr<SSL_CTX> ssl_ctx_;
};

}  // namespace

// These tests rely on memory corruption detectors to verify that
// SSL_SESSION reference counts were correctly managed and no sessions
// leaked or were accessed after free.

// Test basic insertion and lookup operations.
TEST_F(SSLClientSessionCacheTest, Basic) {
  SSLClientSessionCache::Config config;
  SSLClientSessionCache cache(config);

  bssl::UniquePtr<SSL_SESSION> session1 = NewSSLSession();
  bssl::UniquePtr<SSL_SESSION> session2 = NewSSLSession();
  bssl::UniquePtr<SSL_SESSION> session3 = NewSSLSession();

  EXPECT_EQ(nullptr, cache.Lookup(MakeTestKey("key1")).get());
  EXPECT_EQ(nullptr, cache.Lookup(MakeTestKey("key2")).get());
  EXPECT_EQ(0u, cache.size());

  cache.Insert(MakeTestKey("key1"), bssl::UpRef(session1));
  EXPECT_EQ(session1.get(), cache.Lookup(MakeTestKey("key1")).get());
  EXPECT_EQ(nullptr, cache.Lookup(MakeTestKey("key2")).get());
  EXPECT_EQ(1u, cache.size());

  cache.Insert(MakeTestKey("key2"), bssl::UpRef(session2));
  EXPECT_EQ(session1.get(), cache.Lookup(MakeTestKey("key1")).get());
  EXPECT_EQ(session2.get(), cache.Lookup(MakeTestKey("key2")).get());
  EXPECT_EQ(2u, cache.size());

  cache.Insert(MakeTestKey("key1"), bssl::UpRef(session3));
  EXPECT_EQ(session3.get(), cache.Lookup(MakeTestKey("key1")).get());
  EXPECT_EQ(session2.get(), cache.Lookup(MakeTestKey("key2")).get());
  EXPECT_EQ(2u, cache.size());

  cache.Flush();
  EXPECT_EQ(nullptr, cache.Lookup(MakeTestKey("key1")).get());
  EXPECT_EQ(nullptr, cache.Lookup(MakeTestKey("key2")).get());
  EXPECT_EQ(nullptr, cache.Lookup(MakeTestKey("key3")).get());
  EXPECT_EQ(0u, cache.size());
}

// Test basic insertion and lookup operations with single-use sessions.
TEST_F(SSLClientSessionCacheTest, BasicSingleUse) {
  SSLClientSessionCache::Config config;
  SSLClientSessionCache cache(config);

  bssl::UniquePtr<SSL_SESSION> session1 = NewSSLSession(TLS1_3_VERSION);
  bssl::UniquePtr<SSL_SESSION> session2 = NewSSLSession(TLS1_3_VERSION);
  bssl::UniquePtr<SSL_SESSION> session3 = NewSSLSession(TLS1_3_VERSION);

  EXPECT_EQ(nullptr, cache.Lookup(MakeTestKey("key1")).get());
  EXPECT_EQ(nullptr, cache.Lookup(MakeTestKey("key2")).get());
  EXPECT_EQ(0u, cache.size());

  cache.Insert(MakeTestKey("key1"), bssl::UpRef(session1));
  EXPECT_EQ(session1.get(), cache.Lookup(MakeTestKey("key1")).get());
  EXPECT_EQ(nullptr, cache.Lookup(MakeTestKey("key2")).get());
  EXPECT_EQ(0u, cache.size());

  EXPECT_EQ(nullptr, cache.Lookup(MakeTestKey("key1")).get());

  cache.Insert(MakeTestKey("key1"), bssl::UpRef(session1));
  cache.Insert(MakeTestKey("key1"), bssl::UpRef(session1));
  cache.Insert(MakeTestKey("key2"), bssl::UpRef(session2));

  EXPECT_EQ(session1.get(), cache.Lookup(MakeTestKey("key1")).get());
  EXPECT_EQ(session2.get(), cache.Lookup(MakeTestKey("key2")).get());
  EXPECT_EQ(1u, cache.size());

  EXPECT_EQ(session1.get(), cache.Lookup(MakeTestKey("key1")).get());
  EXPECT_EQ(nullptr, cache.Lookup(MakeTestKey("key2")).get());

  cache.Insert(MakeTestKey("key1"), bssl::UpRef(session1));
  cache.Insert(MakeTestKey("key1"), bssl::UpRef(session3));
  cache.Insert(MakeTestKey("key2"), bssl::UpRef(session2));
  EXPECT_EQ(session3.get(), cache.Lookup(MakeTestKey("key1")).get());
  EXPECT_EQ(session1.get(), cache.Lookup(MakeTestKey("key1")).get());
  EXPECT_EQ(session2.get(), cache.Lookup(MakeTestKey("key2")).get());
  EXPECT_EQ(0u, cache.size());

  cache.Flush();
  EXPECT_EQ(nullptr, cache.Lookup(MakeTestKey("key1")).get());
  EXPECT_EQ(nullptr, cache.Lookup(MakeTestKey("key2")).get());
  EXPECT_EQ(nullptr, cache.Lookup(MakeTestKey("key3")).get());
  EXPECT_EQ(0u, cache.size());

  cache.Insert(MakeTestKey("key1"), bssl::UpRef(session1));
  cache.Insert(MakeTestKey("key1"), bssl::UpRef(session2));
  cache.Insert(MakeTestKey("key1"), bssl::UpRef(session3));
  EXPECT_EQ(session3.get(), cache.Lookup(MakeTestKey("key1")).get());
  EXPECT_EQ(session2.get(), cache.Lookup(MakeTestKey("key1")).get());
  EXPECT_EQ(nullptr, cache.Lookup(MakeTestKey("key1")).get());
}

// Test insertion and lookup operations with both single-use and reusable
// sessions.
TEST_F(SSLClientSessionCacheTest, MixedUse) {
  SSLClientSessionCache::Config config;
  SSLClientSessionCache cache(config);

  bssl::UniquePtr<SSL_SESSION> session_single = NewSSLSession(TLS1_3_VERSION);
  bssl::UniquePtr<SSL_SESSION> session_reuse = NewSSLSession(TLS1_2_VERSION);

  EXPECT_EQ(nullptr, cache.Lookup(MakeTestKey("key1")).get());
  EXPECT_EQ(0u, cache.size());

  cache.Insert(MakeTestKey("key1"), bssl::UpRef(session_reuse));
  EXPECT_EQ(session_reuse.get(), cache.Lookup(MakeTestKey("key1")).get());
  EXPECT_EQ(1u, cache.size());

  cache.Insert(MakeTestKey("key1"), bssl::UpRef(session_single));
  EXPECT_EQ(session_single.get(), cache.Lookup(MakeTestKey("key1")).get());
  EXPECT_EQ(nullptr, cache.Lookup(MakeTestKey("key1")).get());
  EXPECT_EQ(0u, cache.size());

  EXPECT_EQ(nullptr, cache.Lookup(MakeTestKey("key2")).get());
  EXPECT_EQ(0u, cache.size());

  cache.Insert(MakeTestKey("key2"), bssl::UpRef(session_single));
  cache.Insert(MakeTestKey("key2"), bssl::UpRef(session_single));
  EXPECT_EQ(1u, cache.size());

  EXPECT_EQ(session_single.get(), cache.Lookup(MakeTestKey("key2")).get());
  EXPECT_EQ(session_single.get(), cache.Lookup(MakeTestKey("key2")).get());
  EXPECT_EQ(nullptr, cache.Lookup(MakeTestKey("key2")).get());
  EXPECT_EQ(0u, cache.size());

  cache.Insert(MakeTestKey("key2"), bssl::UpRef(session_single));
  cache.Insert(MakeTestKey("key2"), bssl::UpRef(session_reuse));
  EXPECT_EQ(session_reuse.get(), cache.Lookup(MakeTestKey("key2")).get());
  EXPECT_EQ(session_reuse.get(), cache.Lookup(MakeTestKey("key2")).get());
  EXPECT_EQ(1u, cache.size());
}

// Test that a session may be inserted at two different keys. This should never
// be necessary, but the API doesn't prohibit it.
TEST_F(SSLClientSessionCacheTest, DoubleInsert) {
  SSLClientSessionCache::Config config;
  SSLClientSessionCache cache(config);

  bssl::UniquePtr<SSL_SESSION> session = NewSSLSession();

  EXPECT_EQ(nullptr, cache.Lookup(MakeTestKey("key1")).get());
  EXPECT_EQ(nullptr, cache.Lookup(MakeTestKey("key2")).get());
  EXPECT_EQ(0u, cache.size());

  cache.Insert(MakeTestKey("key1"), bssl::UpRef(session));
  EXPECT_EQ(session.get(), cache.Lookup(MakeTestKey("key1")).get());
  EXPECT_EQ(nullptr, cache.Lookup(MakeTestKey("key2")).get());
  EXPECT_EQ(1u, cache.size());

  cache.Insert(MakeTestKey("key2"), bssl::UpRef(session));
  EXPECT_EQ(session.get(), cache.Lookup(MakeTestKey("key1")).get());
  EXPECT_EQ(session.get(), cache.Lookup(MakeTestKey("key2")).get());
  EXPECT_EQ(2u, cache.size());

  cache.Flush();
  EXPECT_EQ(nullptr, cache.Lookup(MakeTestKey("key1")).get());
  EXPECT_EQ(nullptr, cache.Lookup(MakeTestKey("key2")).get());
  EXPECT_EQ(0u, cache.size());
}

// Tests that the session cache's size is correctly bounded.
TEST_F(SSLClientSessionCacheTest, MaxEntries) {
  SSLClientSessionCache::Config config;
  config.max_entries = 3;
  SSLClientSessionCache cache(config);

  bssl::UniquePtr<SSL_SESSION> session1 = NewSSLSession();
  bssl::UniquePtr<SSL_SESSION> session2 = NewSSLSession();
  bssl::UniquePtr<SSL_SESSION> session3 = NewSSLSession();
  bssl::UniquePtr<SSL_SESSION> session4 = NewSSLSession();

  // Insert three entries.
  cache.Insert(MakeTestKey("key1"), bssl::UpRef(session1));
  cache.Insert(MakeTestKey("key2"), bssl::UpRef(session2));
  cache.Insert(MakeTestKey("key3"), bssl::UpRef(session3));
  EXPECT_EQ(session1.get(), cache.Lookup(MakeTestKey("key1")).get());
  EXPECT_EQ(session2.get(), cache.Lookup(MakeTestKey("key2")).get());
  EXPECT_EQ(session3.get(), cache.Lookup(MakeTestKey("key3")).get());
  EXPECT_EQ(3u, cache.size());

  // On insertion of a fourth, the first is removed.
  cache.Insert(MakeTestKey("key4"), bssl::UpRef(session4));
  EXPECT_EQ(nullptr, cache.Lookup(MakeTestKey("key1")).get());
  EXPECT_EQ(session4.get(), cache.Lookup(MakeTestKey("key4")).get());
  EXPECT_EQ(session3.get(), cache.Lookup(MakeTestKey("key3")).get());
  EXPECT_EQ(session2.get(), cache.Lookup(MakeTestKey("key2")).get());
  EXPECT_EQ(3u, cache.size());

  // Despite being newest, the next to be removed is session4 as it was accessed
  // least. recently.
  cache.Insert(MakeTestKey("key1"), bssl::UpRef(session1));
  EXPECT_EQ(session1.get(), cache.Lookup(MakeTestKey("key1")).get());
  EXPECT_EQ(session2.get(), cache.Lookup(MakeTestKey("key2")).get());
  EXPECT_EQ(session3.get(), cache.Lookup(MakeTestKey("key3")).get());
  EXPECT_EQ(nullptr, cache.Lookup(MakeTestKey("key4")).get());
  EXPECT_EQ(3u, cache.size());
}

// Tests that session expiration works properly.
TEST_F(SSLClientSessionCacheTest, Expiration) {
  const size_t kNumEntries = 20;
  const size_t kExpirationCheckCount = 10;
  const base::TimeDelta kTimeout = base::Seconds(1000);

  SSLClientSessionCache::Config config;
  config.expiration_check_count = kExpirationCheckCount;
  std::unique_ptr<base::SimpleTestClock> clock = MakeTestClock();
  SSLClientSessionCache cache(config);
  cache.SetClockForTesting(clock.get());

  // Add |kNumEntries - 1| entries.
  for (size_t i = 0; i < kNumEntries - 1; i++) {
    bssl::UniquePtr<SSL_SESSION> session =
        MakeTestSession(clock->Now(), kTimeout);
    cache.Insert(MakeTestKey(base::NumberToString(i)), bssl::UpRef(session));
  }
  EXPECT_EQ(kNumEntries - 1, cache.size());

  // Expire all the previous entries and insert one more entry.
  clock->Advance(kTimeout * 2);
  bssl::UniquePtr<SSL_SESSION> session =
      MakeTestSession(clock->Now(), kTimeout);
  cache.Insert(MakeTestKey("key"), bssl::UpRef(session));

  // All entries are still in the cache.
  EXPECT_EQ(kNumEntries, cache.size());

  // Perform one fewer lookup than needed to trigger the expiration check. This
  // shall not expire any session.
  for (size_t i = 0; i < kExpirationCheckCount - 1; i++)
    cache.Lookup(MakeTestKey("key"));

  // All entries are still in the cache.
  EXPECT_EQ(kNumEntries, cache.size());

  // Perform one more lookup. This will expire all sessions but the last one.
  cache.Lookup(MakeTestKey("key"));
  EXPECT_EQ(1u, cache.size());
  EXPECT_EQ(session.get(), cache.Lookup(MakeTestKey("key")).get());
  for (size_t i = 0; i < kNumEntries - 1; i++) {
    SCOPED_TRACE(i);
    EXPECT_EQ(nullptr, cache.Lookup(MakeTestKey(base::NumberToString(i))));
  }
}

// Tests that Lookup performs an expiration check before returning a cached
// session.
TEST_F(SSLClientSessionCacheTest, LookupExpirationCheck) {
  // kExpirationCheckCount is set to a suitably large number so the automated
  // pruning never triggers.
  const size_t kExpirationCheckCount = 1000;
  const base::TimeDelta kTimeout = base::Seconds(1000);

  SSLClientSessionCache::Config config;
  config.expiration_check_count = kExpirationCheckCount;
  std::unique_ptr<base::SimpleTestClock> clock = MakeTestClock();
  SSLClientSessionCache cache(config);
  cache.SetClockForTesting(clock.get());

  // Insert an entry into the session cache.
  bssl::UniquePtr<SSL_SESSION> session =
      MakeTestSession(clock->Now(), kTimeout);
  cache.Insert(MakeTestKey("key"), bssl::UpRef(session));
  EXPECT_EQ(session.get(), cache.Lookup(MakeTestKey("key")).get());
  EXPECT_EQ(1u, cache.size());

  // Expire the session.
  clock->Advance(kTimeout * 2);

  // The entry has not been removed yet.
  EXPECT_EQ(1u, cache.size());

  // But it will not be returned on lookup and gets pruned at that point.
  EXPECT_EQ(nullptr, cache.Lookup(MakeTestKey("key")).get());
  EXPECT_EQ(0u, cache.size());

  // Re-inserting a session does not refresh the lifetime. The expiration
  // information in the session is used.
  cache.Insert(MakeTestKey("key"), bssl::UpRef(session));
  EXPECT_EQ(nullptr, cache.Lookup(MakeTestKey("key")).get());
  EXPECT_EQ(0u, cache.size());

  // Re-insert a fresh copy of the session.
  session = MakeTestSession(clock->Now(), kTimeout);
  cache.Insert(MakeTestKey("key"), bssl::UpRef(session));
  EXPECT_EQ(session.get(), cache.Lookup(MakeTestKey("key")).get());
  EXPECT_EQ(1u, cache.size());

  // Sessions also are treated as expired if the clock rewinds.
  clock->Advance(base::Seconds(-2));
  EXPECT_EQ(nullptr, cache.Lookup(MakeTestKey("key")).get());
  EXPECT_EQ(0u, cache.size());
}

// Test that SSL cache is flushed on low memory notifications
TEST_F(SSLClientSessionCacheTest, TestFlushOnMemoryNotifications) {
  base::test::TaskEnvironment task_environment;

  // kExpirationCheckCount is set to a suitably large number so the automated
  // pruning never triggers.
  const size_t kExpirationCheckCount = 1000;
  const base::TimeDelta kTimeout = base::Seconds(1000);

  SSLClientSessionCache::Config config;
  config.expiration_check_count = kExpirationCheckCount;
  std::unique_ptr<base::SimpleTestClock> clock = MakeTestClock();
  SSLClientSessionCache cache(config);
  cache.SetClockForTesting(clock.get());

  // Insert an entry into the session cache.
  bssl::UniquePtr<SSL_SESSION> session1 =
      MakeTestSession(clock->Now(), kTimeout);
  cache.Insert(MakeTestKey("key1"), bssl::UpRef(session1));
  EXPECT_EQ(session1.get(), cache.Lookup(MakeTestKey("key1")).get());
  EXPECT_EQ(1u, cache.size());

  // Expire the session.
  clock->Advance(kTimeout * 2);
  // Add one more session.
  bssl::UniquePtr<SSL_SESSION> session2 =
      MakeTestSession(clock->Now(), kTimeout);
  cache.Insert(MakeTestKey("key2"), bssl::UpRef(session2));
  EXPECT_EQ(2u, cache.size());

  // Fire a notification that will flush expired sessions.
  base::MemoryPressureListener::NotifyMemoryPressure(
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE);
  base::RunLoop().RunUntilIdle();

  // Expired session's cache should be flushed.
  // Lookup returns nullptr, when cache entry not found.
  EXPECT_FALSE(cache.Lookup(MakeTestKey("key1")));
  EXPECT_TRUE(cache.Lookup(MakeTestKey("key2")));
  EXPECT_EQ(1u, cache.size());

  // Fire notification that will flush everything.
  base::MemoryPressureListener::NotifyMemoryPressure(
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0u, cache.size());
}

TEST_F(SSLClientSessionCacheTest, FlushForServer) {
  SSLClientSessionCache::Config config;
  SSLClientSessionCache cache(config);

  const SchemefulSite kSiteA(GURL("https://a.test"));
  const SchemefulSite kSiteB(GURL("https://b.test"));

  // Insert a number of cache entries.
  SSLClientSessionCache::Key key1;
  key1.server = HostPortPair("a.test", 443);
  auto session1 = NewSSLSession();
  cache.Insert(key1, bssl::UpRef(session1));

  SSLClientSessionCache::Key key2;
  key2.server = HostPortPair("a.test", 443);
  key2.dest_ip_addr = IPAddress::IPv4Localhost();
  key2.network_anonymization_key =
      NetworkAnonymizationKey::CreateSameSite(kSiteB);
  key2.privacy_mode = PRIVACY_MODE_ENABLED;
  auto session2 = NewSSLSession();
  cache.Insert(key2, bssl::UpRef(session2));

  SSLClientSessionCache::Key key3;
  key3.server = HostPortPair("a.test", 444);
  auto session3 = NewSSLSession();
  cache.Insert(key3, bssl::UpRef(session3));

  SSLClientSessionCache::Key key4;
  key4.server = HostPortPair("b.test", 443);
  auto session4 = NewSSLSession();
  cache.Insert(key4, bssl::UpRef(session4));

  SSLClientSessionCache::Key key5;
  key5.server = HostPortPair("b.test", 443);
  key5.network_anonymization_key =
      NetworkAnonymizationKey::CreateSameSite(kSiteA);
  auto session5 = NewSSLSession();
  cache.Insert(key5, bssl::UpRef(session5));

  // Flush an unrelated server. The cache should be unaffected.
  cache.FlushForServers({HostPortPair("c.test", 443)});
  EXPECT_EQ(5u, cache.size());
  EXPECT_EQ(session1.get(), cache.Lookup(key1).get());
  EXPECT_EQ(session2.get(), cache.Lookup(key2).get());
  EXPECT_EQ(session3.get(), cache.Lookup(key3).get());
  EXPECT_EQ(session4.get(), cache.Lookup(key4).get());
  EXPECT_EQ(session5.get(), cache.Lookup(key5).get());

  // Flush a.test:443. |key1| and |key2| should match, but not the others.
  cache.FlushForServers({HostPortPair("a.test", 443)});
  EXPECT_EQ(3u, cache.size());
  EXPECT_EQ(nullptr, cache.Lookup(key1).get());
  EXPECT_EQ(nullptr, cache.Lookup(key2).get());
  EXPECT_EQ(session3.get(), cache.Lookup(key3).get());
  EXPECT_EQ(session4.get(), cache.Lookup(key4).get());
  EXPECT_EQ(session5.get(), cache.Lookup(key5).get());

  // Flush b.test:443. |key4| and |key5| match, but not |key3|.
  cache.FlushForServers({HostPortPair("b.test", 443)});
  EXPECT_EQ(1u, cache.size());
  EXPECT_EQ(nullptr, cache.Lookup(key1).get());
  EXPECT_EQ(nullptr, cache.Lookup(key2).get());
  EXPECT_EQ(session3.get(), cache.Lookup(key3).get());
  EXPECT_EQ(nullptr, cache.Lookup(key4).get());
  EXPECT_EQ(nullptr, cache.Lookup(key5).get());

  // Flush the last host, a.test:444.
  cache.FlushForServers({HostPortPair("a.test", 444)});
  EXPECT_EQ(0u, cache.size());
  EXPECT_EQ(nullptr, cache.Lookup(key1).get());
  EXPECT_EQ(nullptr, cache.Lookup(key2).get());
  EXPECT_EQ(nullptr, cache.Lookup(key3).get());
  EXPECT_EQ(nullptr, cache.Lookup(key4).get());
  EXPECT_EQ(nullptr, cache.Lookup(key5).get());
}

TEST_F(SSLClientSessionCacheTest, FlushForServers) {
  SSLClientSessionCache::Config config;
  SSLClientSessionCache cache(config);

  const SchemefulSite kSiteA(GURL("https://a.test"));
  const SchemefulSite kSiteB(GURL("https://b.test"));

  // Insert a number of cache entries.
  SSLClientSessionCache::Key key1;
  key1.server = HostPortPair("a.test", 443);
  auto session1 = NewSSLSession();
  cache.Insert(key1, bssl::UpRef(session1));

  SSLClientSessionCache::Key key2;
  key2.server = HostPortPair("a.test", 443);
  key2.dest_ip_addr = IPAddress::IPv4Localhost();
  key2.network_anonymization_key =
      NetworkAnonymizationKey::CreateSameSite(kSiteB);
  key2.privacy_mode = PRIVACY_MODE_ENABLED;
  auto session2 = NewSSLSession();
  cache.Insert(key2, bssl::UpRef(session2));

  SSLClientSessionCache::Key key3;
  key3.server = HostPortPair("a.test", 444);
  auto session3 = NewSSLSession();
  cache.Insert(key3, bssl::UpRef(session3));

  SSLClientSessionCache::Key key4;
  key4.server = HostPortPair("b.test", 443);
  auto session4 = NewSSLSession();
  cache.Insert(key4, bssl::UpRef(session4));

  SSLClientSessionCache::Key key5;
  key5.server = HostPortPair("b.test", 443);
  key5.network_anonymization_key =
      NetworkAnonymizationKey::CreateSameSite(kSiteA);
  auto session5 = NewSSLSession();
  cache.Insert(key5, bssl::UpRef(session5));

  cache.FlushForServers({
      // Unrelated server. Should have no effect.
      HostPortPair("c.test", 443),
      // Flush a.test:443. |key1| and |key2| should match, but not the others.
      HostPortPair("a.test", 443),
      // Flush b.test:443. |key4| and |key5| match, but not |key3|.
      HostPortPair("b.test", 443),
  });
  EXPECT_EQ(1u, cache.size());
  EXPECT_EQ(nullptr, cache.Lookup(key1).get());
  EXPECT_EQ(nullptr, cache.Lookup(key2).get());
  EXPECT_EQ(session3.get(), cache.Lookup(key3).get());
  EXPECT_EQ(nullptr, cache.Lookup(key4).get());
  EXPECT_EQ(nullptr, cache.Lookup(key5).get());
}

}  // namespace net
