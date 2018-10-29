// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/strings/string16.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/simple_test_clock.h"
#include "base/test/simple_test_tick_clock.h"
#include "net/base/net_errors.h"
#include "net/http/http_auth_cache.h"
#include "net/http/http_auth_handler.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::ASCIIToUTF16;

namespace net {

namespace {

class MockAuthHandler : public HttpAuthHandler {
 public:
  MockAuthHandler(HttpAuth::Scheme scheme,
                  const std::string& realm,
                  HttpAuth::Target target) {
    // Can't use initializer list since these are members of the base class.
    auth_scheme_ = scheme;
    realm_ = realm;
    score_ = 1;
    target_ = target;
    properties_ = 0;
  }

  HttpAuth::AuthorizationResult HandleAnotherChallenge(
      HttpAuthChallengeTokenizer* challenge) override {
    return HttpAuth::AUTHORIZATION_RESULT_REJECT;
  }

 protected:
  bool Init(HttpAuthChallengeTokenizer* challenge,
            const SSLInfo& ssl_info) override {
    return false;  // Unused.
  }

  int GenerateAuthTokenImpl(const AuthCredentials*,
                            const HttpRequestInfo*,
                            CompletionOnceCallback callback,
                            std::string* auth_token) override {
    *auth_token = "mock-credentials";
    return OK;
  }


 private:
  ~MockAuthHandler() override = default;
};

const char kRealm1[] = "Realm1";
const char kRealm2[] = "Realm2";
const char kRealm3[] = "Realm3";
const char kRealm4[] = "Realm4";
const char kRealm5[] = "Realm5";
const base::string16 k123(ASCIIToUTF16("123"));
const base::string16 k1234(ASCIIToUTF16("1234"));
const base::string16 kAdmin(ASCIIToUTF16("admin"));
const base::string16 kAlice(ASCIIToUTF16("alice"));
const base::string16 kAlice2(ASCIIToUTF16("alice2"));
const base::string16 kPassword(ASCIIToUTF16("password"));
const base::string16 kRoot(ASCIIToUTF16("root"));
const base::string16 kUsername(ASCIIToUTF16("username"));
const base::string16 kWileCoyote(ASCIIToUTF16("wilecoyote"));

AuthCredentials CreateASCIICredentials(const char* username,
                                       const char* password) {
  return AuthCredentials(ASCIIToUTF16(username), ASCIIToUTF16(password));
}

}  // namespace

// Test adding and looking-up cache entries (both by realm and by path).
TEST(HttpAuthCacheTest, Basic) {
  GURL origin("http://www.google.com");
  GURL origin2("http://www.foobar.com");
  HttpAuthCache cache;
  HttpAuthCache::Entry* entry;

  // Add cache entries for 4 realms: "Realm1", "Realm2", "Realm3" and
  // "Realm4"

  std::unique_ptr<HttpAuthHandler> realm1_handler(new MockAuthHandler(
      HttpAuth::AUTH_SCHEME_BASIC, kRealm1, HttpAuth::AUTH_SERVER));
  cache.Add(origin, realm1_handler->realm(), realm1_handler->auth_scheme(),
            "Basic realm=Realm1",
            CreateASCIICredentials("realm1-user", "realm1-password"),
            "/foo/bar/index.html");

  std::unique_ptr<HttpAuthHandler> realm2_handler(new MockAuthHandler(
      HttpAuth::AUTH_SCHEME_BASIC, kRealm2, HttpAuth::AUTH_SERVER));
  cache.Add(origin, realm2_handler->realm(), realm2_handler->auth_scheme(),
            "Basic realm=Realm2",
            CreateASCIICredentials("realm2-user", "realm2-password"),
            "/foo2/index.html");

  std::unique_ptr<HttpAuthHandler> realm3_basic_handler(new MockAuthHandler(
      HttpAuth::AUTH_SCHEME_BASIC, kRealm3, HttpAuth::AUTH_PROXY));
  cache.Add(
      origin,
      realm3_basic_handler->realm(),
      realm3_basic_handler->auth_scheme(),
      "Basic realm=Realm3",
      CreateASCIICredentials("realm3-basic-user", "realm3-basic-password"),
      std::string());

  std::unique_ptr<HttpAuthHandler> realm3_digest_handler(new MockAuthHandler(
      HttpAuth::AUTH_SCHEME_DIGEST, kRealm3, HttpAuth::AUTH_PROXY));
  cache.Add(origin, realm3_digest_handler->realm(),
            realm3_digest_handler->auth_scheme(), "Digest realm=Realm3",
            CreateASCIICredentials("realm3-digest-user",
                                   "realm3-digest-password"),
            "/baz/index.html");

  std::unique_ptr<HttpAuthHandler> realm4_basic_handler(new MockAuthHandler(
      HttpAuth::AUTH_SCHEME_BASIC, kRealm4, HttpAuth::AUTH_SERVER));
  cache.Add(origin, realm4_basic_handler->realm(),
            realm4_basic_handler->auth_scheme(), "Basic realm=Realm4",
            CreateASCIICredentials("realm4-basic-user",
                                   "realm4-basic-password"),
            "/");

  std::unique_ptr<HttpAuthHandler> origin2_realm5_handler(new MockAuthHandler(
      HttpAuth::AUTH_SCHEME_BASIC, kRealm5, HttpAuth::AUTH_SERVER));
  cache.Add(origin2, origin2_realm5_handler->realm(),
            origin2_realm5_handler->auth_scheme(), "Basic realm=Realm5",
            CreateASCIICredentials("realm5-user", "realm5-password"), "/");
  cache.Add(
      origin2, realm3_basic_handler->realm(),
      realm3_basic_handler->auth_scheme(), "Basic realm=Realm3",
      CreateASCIICredentials("realm3-basic-user", "realm3-basic-password"),
      std::string());

  // There is no Realm5 in origin
  entry = cache.Lookup(origin, kRealm5, HttpAuth::AUTH_SCHEME_BASIC);
  EXPECT_TRUE(NULL == entry);

  // While Realm3 does exist, the origin scheme is wrong.
  entry = cache.Lookup(GURL("https://www.google.com"), kRealm3,
                       HttpAuth::AUTH_SCHEME_BASIC);
  EXPECT_TRUE(NULL == entry);

  // Realm, origin scheme ok, authentication scheme wrong
  entry = cache.Lookup
      (GURL("http://www.google.com"), kRealm1, HttpAuth::AUTH_SCHEME_DIGEST);
  EXPECT_TRUE(NULL == entry);

  // Valid lookup by origin, realm, scheme.
  entry = cache.Lookup(
      GURL("http://www.google.com:80"), kRealm3, HttpAuth::AUTH_SCHEME_BASIC);
  ASSERT_FALSE(NULL == entry);
  EXPECT_EQ(HttpAuth::AUTH_SCHEME_BASIC, entry->scheme());
  EXPECT_EQ(kRealm3, entry->realm());
  EXPECT_EQ("Basic realm=Realm3", entry->auth_challenge());
  EXPECT_EQ(ASCIIToUTF16("realm3-basic-user"), entry->credentials().username());
  EXPECT_EQ(ASCIIToUTF16("realm3-basic-password"),
            entry->credentials().password());

  // Same realm, scheme with different origins
  HttpAuthCache::Entry* entry2 = cache.Lookup(
      GURL("http://www.foobar.com:80"), kRealm3, HttpAuth::AUTH_SCHEME_BASIC);
  ASSERT_FALSE(NULL == entry2);
  EXPECT_NE(entry, entry2);

  // Valid lookup by origin, realm, scheme when there's a duplicate
  // origin, realm in the cache
  entry = cache.Lookup(
      GURL("http://www.google.com:80"), kRealm3, HttpAuth::AUTH_SCHEME_DIGEST);
  ASSERT_FALSE(NULL == entry);
  EXPECT_EQ(HttpAuth::AUTH_SCHEME_DIGEST, entry->scheme());
  EXPECT_EQ(kRealm3, entry->realm());
  EXPECT_EQ("Digest realm=Realm3", entry->auth_challenge());
  EXPECT_EQ(ASCIIToUTF16("realm3-digest-user"),
            entry->credentials().username());
  EXPECT_EQ(ASCIIToUTF16("realm3-digest-password"),
            entry->credentials().password());

  // Valid lookup by realm.
  entry = cache.Lookup(origin, kRealm2, HttpAuth::AUTH_SCHEME_BASIC);
  ASSERT_FALSE(NULL == entry);
  EXPECT_EQ(HttpAuth::AUTH_SCHEME_BASIC, entry->scheme());
  EXPECT_EQ(kRealm2, entry->realm());
  EXPECT_EQ("Basic realm=Realm2", entry->auth_challenge());
  EXPECT_EQ(ASCIIToUTF16("realm2-user"), entry->credentials().username());
  EXPECT_EQ(ASCIIToUTF16("realm2-password"), entry->credentials().password());

  // Check that subpaths are recognized.
  HttpAuthCache::Entry* p_realm2_entry =
      cache.Lookup(origin, kRealm2, HttpAuth::AUTH_SCHEME_BASIC);
  HttpAuthCache::Entry* p_realm4_entry =
      cache.Lookup(origin, kRealm4, HttpAuth::AUTH_SCHEME_BASIC);
  EXPECT_FALSE(NULL == p_realm2_entry);
  EXPECT_FALSE(NULL == p_realm4_entry);
  HttpAuthCache::Entry realm2_entry = *p_realm2_entry;
  HttpAuthCache::Entry realm4_entry = *p_realm4_entry;
  // Realm4 applies to '/' and Realm2 applies to '/foo2/'.
  // LookupByPath() should return the closest enclosing path.
  // Positive tests:
  entry = cache.LookupByPath(origin, "/foo2/index.html");
  EXPECT_TRUE(realm2_entry.IsEqualForTesting(*entry));
  entry = cache.LookupByPath(origin, "/foo2/foobar.html");
  EXPECT_TRUE(realm2_entry.IsEqualForTesting(*entry));
  entry = cache.LookupByPath(origin, "/foo2/bar/index.html");
  EXPECT_TRUE(realm2_entry.IsEqualForTesting(*entry));
  entry = cache.LookupByPath(origin, "/foo2/");
  EXPECT_TRUE(realm2_entry.IsEqualForTesting(*entry));
  entry = cache.LookupByPath(origin, "/foo2");
  EXPECT_TRUE(realm4_entry.IsEqualForTesting(*entry));
  entry = cache.LookupByPath(origin, "/");
  EXPECT_TRUE(realm4_entry.IsEqualForTesting(*entry));

  // Negative tests:
  entry = cache.LookupByPath(origin, "/foo3/index.html");
  EXPECT_FALSE(realm2_entry.IsEqualForTesting(*entry));
  entry = cache.LookupByPath(origin, std::string());
  EXPECT_FALSE(realm2_entry.IsEqualForTesting(*entry));

  // Confirm we find the same realm, different auth scheme by path lookup
  HttpAuthCache::Entry* p_realm3_digest_entry =
      cache.Lookup(origin, kRealm3, HttpAuth::AUTH_SCHEME_DIGEST);
  EXPECT_FALSE(NULL == p_realm3_digest_entry);
  HttpAuthCache::Entry realm3_digest_entry = *p_realm3_digest_entry;
  entry = cache.LookupByPath(origin, "/baz/index.html");
  EXPECT_TRUE(realm3_digest_entry.IsEqualForTesting(*entry));
  entry = cache.LookupByPath(origin, "/baz/");
  EXPECT_TRUE(realm3_digest_entry.IsEqualForTesting(*entry));
  entry = cache.LookupByPath(origin, "/baz");
  EXPECT_FALSE(realm3_digest_entry.IsEqualForTesting(*entry));

  // Confirm we find the same realm, different auth scheme by path lookup
  HttpAuthCache::Entry* p_realm3DigestEntry =
      cache.Lookup(origin, kRealm3, HttpAuth::AUTH_SCHEME_DIGEST);
  EXPECT_FALSE(NULL == p_realm3DigestEntry);
  HttpAuthCache::Entry realm3DigestEntry = *p_realm3DigestEntry;
  entry = cache.LookupByPath(origin, "/baz/index.html");
  EXPECT_TRUE(realm3DigestEntry.IsEqualForTesting(*entry));
  entry = cache.LookupByPath(origin, "/baz/");
  EXPECT_TRUE(realm3DigestEntry.IsEqualForTesting(*entry));
  entry = cache.LookupByPath(origin, "/baz");
  EXPECT_FALSE(realm3DigestEntry.IsEqualForTesting(*entry));

  // Lookup using empty path (may be used for proxy).
  entry = cache.LookupByPath(origin, std::string());
  EXPECT_FALSE(NULL == entry);
  EXPECT_EQ(HttpAuth::AUTH_SCHEME_BASIC, entry->scheme());
  EXPECT_EQ(kRealm3, entry->realm());
}

TEST(HttpAuthCacheTest, AddPath) {
  HttpAuthCache::Entry entry;

  // All of these paths have a common root /1/2/2/4/5/
  entry.AddPath("/1/2/3/4/5/x.txt");
  entry.AddPath("/1/2/3/4/5/y.txt");
  entry.AddPath("/1/2/3/4/5/z.txt");

  EXPECT_EQ(1U, entry.paths_.size());
  EXPECT_EQ("/1/2/3/4/5/", entry.paths_.front());

  // Add a new entry (not a subpath).
  entry.AddPath("/1/XXX/q");
  EXPECT_EQ(2U, entry.paths_.size());
  EXPECT_EQ("/1/XXX/", entry.paths_.front());
  EXPECT_EQ("/1/2/3/4/5/", entry.paths_.back());

  // Add containing paths of /1/2/3/4/5/ -- should swallow up the deeper paths.
  entry.AddPath("/1/2/3/4/x.txt");
  EXPECT_EQ(2U, entry.paths_.size());
  EXPECT_EQ("/1/2/3/4/", entry.paths_.front());
  EXPECT_EQ("/1/XXX/", entry.paths_.back());
  entry.AddPath("/1/2/3/x");
  EXPECT_EQ(2U, entry.paths_.size());
  EXPECT_EQ("/1/2/3/", entry.paths_.front());
  EXPECT_EQ("/1/XXX/", entry.paths_.back());

  entry.AddPath("/index.html");
  EXPECT_EQ(1U, entry.paths_.size());
  EXPECT_EQ("/", entry.paths_.front());
}

// Calling Add when the realm entry already exists, should append that
// path.
TEST(HttpAuthCacheTest, AddToExistingEntry) {
  HttpAuthCache cache;
  GURL origin("http://www.foobar.com:70");
  const std::string auth_challenge = "Basic realm=MyRealm";

  std::unique_ptr<HttpAuthHandler> handler(new MockAuthHandler(
      HttpAuth::AUTH_SCHEME_BASIC, "MyRealm", HttpAuth::AUTH_SERVER));
  HttpAuthCache::Entry* orig_entry = cache.Add(
      origin, handler->realm(), handler->auth_scheme(), auth_challenge,
      CreateASCIICredentials("user1", "password1"), "/x/y/z/");
  cache.Add(origin, handler->realm(), handler->auth_scheme(), auth_challenge,
            CreateASCIICredentials("user2", "password2"), "/z/y/x/");
  cache.Add(origin, handler->realm(), handler->auth_scheme(), auth_challenge,
            CreateASCIICredentials("user3", "password3"), "/z/y");

  HttpAuthCache::Entry* entry = cache.Lookup(
      origin, "MyRealm", HttpAuth::AUTH_SCHEME_BASIC);

  EXPECT_TRUE(entry == orig_entry);
  EXPECT_EQ(ASCIIToUTF16("user3"), entry->credentials().username());
  EXPECT_EQ(ASCIIToUTF16("password3"), entry->credentials().password());

  EXPECT_EQ(2U, entry->paths_.size());
  EXPECT_EQ("/z/", entry->paths_.front());
  EXPECT_EQ("/x/y/z/", entry->paths_.back());
}

TEST(HttpAuthCacheTest, Remove) {
  GURL origin("http://foobar2.com");

  std::unique_ptr<HttpAuthHandler> realm1_handler(new MockAuthHandler(
      HttpAuth::AUTH_SCHEME_BASIC, kRealm1, HttpAuth::AUTH_SERVER));

  std::unique_ptr<HttpAuthHandler> realm2_handler(new MockAuthHandler(
      HttpAuth::AUTH_SCHEME_BASIC, kRealm2, HttpAuth::AUTH_SERVER));

  std::unique_ptr<HttpAuthHandler> realm3_basic_handler(new MockAuthHandler(
      HttpAuth::AUTH_SCHEME_BASIC, kRealm3, HttpAuth::AUTH_SERVER));

  std::unique_ptr<HttpAuthHandler> realm3_digest_handler(new MockAuthHandler(
      HttpAuth::AUTH_SCHEME_DIGEST, kRealm3, HttpAuth::AUTH_SERVER));

  HttpAuthCache cache;
  cache.Add(origin, realm1_handler->realm(), realm1_handler->auth_scheme(),
            "basic realm=Realm1", AuthCredentials(kAlice, k123), "/");
  cache.Add(origin, realm2_handler->realm(), realm2_handler->auth_scheme(),
            "basic realm=Realm2", CreateASCIICredentials("bob", "princess"),
            "/");
  cache.Add(origin, realm3_basic_handler->realm(),
            realm3_basic_handler->auth_scheme(), "basic realm=Realm3",
            AuthCredentials(kAdmin, kPassword), "/");
  cache.Add(origin, realm3_digest_handler->realm(),
            realm3_digest_handler->auth_scheme(), "digest realm=Realm3",
            AuthCredentials(kRoot, kWileCoyote), "/");

  // Fails, because there is no realm "Realm5".
  EXPECT_FALSE(cache.Remove(
      origin, kRealm5, HttpAuth::AUTH_SCHEME_BASIC,
      AuthCredentials(kAlice, k123)));

  // Fails because the origin is wrong.
  EXPECT_FALSE(cache.Remove(GURL("http://foobar2.com:100"),
                            kRealm1,
                            HttpAuth::AUTH_SCHEME_BASIC,
                            AuthCredentials(kAlice, k123)));

  // Fails because the username is wrong.
  EXPECT_FALSE(cache.Remove(
      origin, kRealm1, HttpAuth::AUTH_SCHEME_BASIC,
      AuthCredentials(kAlice2, k123)));

  // Fails because the password is wrong.
  EXPECT_FALSE(cache.Remove(
      origin, kRealm1, HttpAuth::AUTH_SCHEME_BASIC,
      AuthCredentials(kAlice, k1234)));

  // Fails because the authentication type is wrong.
  EXPECT_FALSE(cache.Remove(
      origin, kRealm1, HttpAuth::AUTH_SCHEME_DIGEST,
      AuthCredentials(kAlice, k123)));

  // Succeeds.
  EXPECT_TRUE(cache.Remove(
      origin, kRealm1, HttpAuth::AUTH_SCHEME_BASIC,
      AuthCredentials(kAlice, k123)));

  // Fails because we just deleted the entry!
  EXPECT_FALSE(cache.Remove(
      origin, kRealm1, HttpAuth::AUTH_SCHEME_BASIC,
      AuthCredentials(kAlice, k123)));

  // Succeed when there are two authentication types for the same origin,realm.
  EXPECT_TRUE(cache.Remove(
      origin, kRealm3, HttpAuth::AUTH_SCHEME_DIGEST,
      AuthCredentials(kRoot, kWileCoyote)));

  // Succeed as above, but when entries were added in opposite order
  cache.Add(origin, realm3_digest_handler->realm(),
            realm3_digest_handler->auth_scheme(), "digest realm=Realm3",
            AuthCredentials(kRoot, kWileCoyote), "/");
  EXPECT_TRUE(cache.Remove(
      origin, kRealm3, HttpAuth::AUTH_SCHEME_BASIC,
      AuthCredentials(kAdmin, kPassword)));

  // Make sure that removing one entry still leaves the other available for
  // lookup.
  HttpAuthCache::Entry* entry = cache.Lookup(
      origin, kRealm3, HttpAuth::AUTH_SCHEME_DIGEST);
  EXPECT_FALSE(NULL == entry);
}

TEST(HttpAuthCacheTest, ClearEntriesAddedSince) {
  GURL origin("http://foobar.com");

  base::Time start_time;
  ASSERT_TRUE(base::Time::FromString("30 May 2018 12:00:00", &start_time));
  base::SimpleTestClock test_clock;
  test_clock.SetNow(start_time);

  HttpAuthCache cache;
  cache.set_clock_for_testing(&test_clock);

  cache.Add(origin, kRealm1, HttpAuth::AUTH_SCHEME_BASIC, "basic realm=Realm1",
            AuthCredentials(kAlice, k123), "/");
  cache.Add(origin, kRealm2, HttpAuth::AUTH_SCHEME_BASIC, "basic realm=Realm2",
            AuthCredentials(kRoot, kWileCoyote), "/");

  test_clock.Advance(base::TimeDelta::FromSeconds(10));  // Time now 12:00:10
  cache.Add(origin, kRealm3, HttpAuth::AUTH_SCHEME_BASIC, "basic realm=Realm3",
            AuthCredentials(kAlice2, k1234), "/");
  cache.Add(origin, kRealm4, HttpAuth::AUTH_SCHEME_BASIC, "basic realm=Realm4",
            AuthCredentials(kUsername, kPassword), "/");
  // Add path to existing entry.
  cache.Add(origin, kRealm2, HttpAuth::AUTH_SCHEME_BASIC, "basic realm=Realm2",
            AuthCredentials(kAdmin, kPassword), "/baz/");

  base::Time test_time;
  ASSERT_TRUE(base::Time::FromString("30 May 2018 12:00:05", &test_time));
  cache.ClearEntriesAddedSince(test_time);

  // Realms 1 and 2 are older than 12:00:05 and should not be cleared
  EXPECT_NE(nullptr,
            cache.Lookup(origin, kRealm1, HttpAuth::AUTH_SCHEME_BASIC));
  EXPECT_NE(nullptr,
            cache.Lookup(origin, kRealm2, HttpAuth::AUTH_SCHEME_BASIC));
  // Creation time is set for a whole entry rather than for a particular path.
  // Path added within the requested duration isn't be removed.
  EXPECT_NE(nullptr, cache.LookupByPath(origin, "/baz/"));

  // Realms 3 and 4 are newer than 12:00:05 and should be cleared.
  EXPECT_EQ(nullptr,
            cache.Lookup(origin, kRealm3, HttpAuth::AUTH_SCHEME_BASIC));
  EXPECT_EQ(nullptr,
            cache.Lookup(origin, kRealm4, HttpAuth::AUTH_SCHEME_BASIC));

  cache.ClearEntriesAddedSince(start_time - base::TimeDelta::FromSeconds(1));
  EXPECT_EQ(nullptr,
            cache.Lookup(origin, kRealm1, HttpAuth::AUTH_SCHEME_BASIC));
  EXPECT_EQ(nullptr,
            cache.Lookup(origin, kRealm2, HttpAuth::AUTH_SCHEME_BASIC));
  EXPECT_EQ(nullptr, cache.LookupByPath(origin, "/baz/"));
}

TEST(HttpAuthCacheTest, ClearEntriesAddedSinceWithNullTime) {
  GURL origin("http://foobar.com");

  base::SimpleTestClock test_clock;
  test_clock.SetNow(base::Time::Now());

  HttpAuthCache cache;
  cache.set_clock_for_testing(&test_clock);

  cache.Add(origin, kRealm1, HttpAuth::AUTH_SCHEME_BASIC, "basic realm=Realm1",
            AuthCredentials(kAlice, k123), "/");
  cache.Add(origin, kRealm2, HttpAuth::AUTH_SCHEME_BASIC, "basic realm=Realm2",
            AuthCredentials(kRoot, kWileCoyote), "/");

  test_clock.Advance(base::TimeDelta::FromSeconds(10));
  cache.Add(origin, kRealm3, HttpAuth::AUTH_SCHEME_BASIC, "basic realm=Realm3",
            AuthCredentials(kAlice2, k1234), "/");
  cache.Add(origin, kRealm4, HttpAuth::AUTH_SCHEME_BASIC, "basic realm=Realm4",
            AuthCredentials(kUsername, kPassword), "/");
  // Add path to existing entry.
  cache.Add(origin, kRealm2, HttpAuth::AUTH_SCHEME_BASIC, "basic realm=Realm2",
            AuthCredentials(kAdmin, kPassword), "/baz/");

  cache.ClearEntriesAddedSince(base::Time());

  // All entries should be cleared.
  EXPECT_EQ(nullptr,
            cache.Lookup(origin, kRealm1, HttpAuth::AUTH_SCHEME_BASIC));
  EXPECT_EQ(nullptr,
            cache.Lookup(origin, kRealm2, HttpAuth::AUTH_SCHEME_BASIC));
  EXPECT_EQ(nullptr, cache.LookupByPath(origin, "/baz/"));
  EXPECT_EQ(nullptr,
            cache.Lookup(origin, kRealm3, HttpAuth::AUTH_SCHEME_BASIC));
  EXPECT_EQ(nullptr,
            cache.Lookup(origin, kRealm4, HttpAuth::AUTH_SCHEME_BASIC));
}

TEST(HttpAuthCacheTest, ClearAllEntries) {
  GURL origin("http://foobar.com");

  base::SimpleTestClock test_clock;
  test_clock.SetNow(base::Time::Now());

  HttpAuthCache cache;
  cache.set_clock_for_testing(&test_clock);

  cache.Add(origin, kRealm1, HttpAuth::AUTH_SCHEME_BASIC, "basic realm=Realm1",
            AuthCredentials(kAlice, k123), "/");
  cache.Add(origin, kRealm2, HttpAuth::AUTH_SCHEME_BASIC, "basic realm=Realm2",
            AuthCredentials(kRoot, kWileCoyote), "/");

  test_clock.Advance(base::TimeDelta::FromSeconds(10));
  cache.Add(origin, kRealm3, HttpAuth::AUTH_SCHEME_BASIC, "basic realm=Realm3",
            AuthCredentials(kAlice2, k1234), "/");
  cache.Add(origin, kRealm4, HttpAuth::AUTH_SCHEME_BASIC, "basic realm=Realm4",
            AuthCredentials(kUsername, kPassword), "/");
  // Add path to existing entry.
  cache.Add(origin, kRealm2, HttpAuth::AUTH_SCHEME_BASIC, "basic realm=Realm2",
            AuthCredentials(kAdmin, kPassword), "/baz/");

  test_clock.Advance(base::TimeDelta::FromSeconds(55));
  cache.ClearAllEntries();

  // All entries should be cleared.
  EXPECT_EQ(nullptr,
            cache.Lookup(origin, kRealm1, HttpAuth::AUTH_SCHEME_BASIC));
  EXPECT_EQ(nullptr,
            cache.Lookup(origin, kRealm2, HttpAuth::AUTH_SCHEME_BASIC));
  EXPECT_EQ(nullptr, cache.LookupByPath(origin, "/baz/"));
  EXPECT_EQ(nullptr,
            cache.Lookup(origin, kRealm3, HttpAuth::AUTH_SCHEME_BASIC));
  EXPECT_EQ(nullptr,
            cache.Lookup(origin, kRealm4, HttpAuth::AUTH_SCHEME_BASIC));
}

TEST(HttpAuthCacheTest, UpdateStaleChallenge) {
  HttpAuthCache cache;
  GURL origin("http://foobar2.com");
  std::unique_ptr<HttpAuthHandler> digest_handler(new MockAuthHandler(
      HttpAuth::AUTH_SCHEME_DIGEST, kRealm1, HttpAuth::AUTH_PROXY));
  HttpAuthCache::Entry* entry_pre = cache.Add(
      origin,
      digest_handler->realm(),
      digest_handler->auth_scheme(),
      "Digest realm=Realm1,"
      "nonce=\"s3MzvFhaBAA=4c520af5acd9d8d7ae26947529d18c8eae1e98f4\"",
      CreateASCIICredentials("realm-digest-user", "realm-digest-password"),
      "/baz/index.html");
  ASSERT_TRUE(entry_pre != NULL);

  EXPECT_EQ(2, entry_pre->IncrementNonceCount());
  EXPECT_EQ(3, entry_pre->IncrementNonceCount());
  EXPECT_EQ(4, entry_pre->IncrementNonceCount());

  bool update_success = cache.UpdateStaleChallenge(
      origin,
      digest_handler->realm(),
      digest_handler->auth_scheme(),
      "Digest realm=Realm1,"
      "nonce=\"claGgoRXBAA=7583377687842fdb7b56ba0555d175baa0b800e3\","
      "stale=\"true\"");
  EXPECT_TRUE(update_success);

  // After the stale update, the entry should still exist in the cache and
  // the nonce count should be reset to 0.
  HttpAuthCache::Entry* entry_post = cache.Lookup(
      origin,
      digest_handler->realm(),
      digest_handler->auth_scheme());
  ASSERT_TRUE(entry_post != NULL);
  EXPECT_EQ(2, entry_post->IncrementNonceCount());

  // UpdateStaleChallenge will fail if an entry doesn't exist in the cache.
  bool update_failure = cache.UpdateStaleChallenge(
      origin,
      kRealm2,
      digest_handler->auth_scheme(),
      "Digest realm=Realm2,"
      "nonce=\"claGgoRXBAA=7583377687842fdb7b56ba0555d175baa0b800e3\","
      "stale=\"true\"");
  EXPECT_FALSE(update_failure);
}

TEST(HttpAuthCacheTest, UpdateAllFrom) {
  GURL origin("http://example.com");
  std::string path("/some/path");
  std::string another_path("/another/path");

  std::unique_ptr<HttpAuthHandler> realm1_handler(new MockAuthHandler(
      HttpAuth::AUTH_SCHEME_BASIC, kRealm1, HttpAuth::AUTH_SERVER));

  std::unique_ptr<HttpAuthHandler> realm2_handler(new MockAuthHandler(
      HttpAuth::AUTH_SCHEME_BASIC, kRealm2, HttpAuth::AUTH_PROXY));

  std::unique_ptr<HttpAuthHandler> realm3_digest_handler(new MockAuthHandler(
      HttpAuth::AUTH_SCHEME_DIGEST, kRealm3, HttpAuth::AUTH_SERVER));

  std::unique_ptr<HttpAuthHandler> realm4_handler(new MockAuthHandler(
      HttpAuth::AUTH_SCHEME_BASIC, kRealm4, HttpAuth::AUTH_SERVER));

  HttpAuthCache first_cache;
  HttpAuthCache::Entry* entry;

  first_cache.Add(origin, realm1_handler->realm(),
                  realm1_handler->auth_scheme(), "basic realm=Realm1",
                  AuthCredentials(kAlice, k123), path);
  first_cache.Add(origin, realm2_handler->realm(),
                  realm2_handler->auth_scheme(), "basic realm=Realm2",
                  AuthCredentials(kAlice2, k1234), path);
  first_cache.Add(origin, realm3_digest_handler->realm(),
                  realm3_digest_handler->auth_scheme(), "digest realm=Realm3",
                  AuthCredentials(kRoot, kWileCoyote), path);
  entry = first_cache.Add(
      origin, realm3_digest_handler->realm(),
      realm3_digest_handler->auth_scheme(), "digest realm=Realm3",
      AuthCredentials(kRoot, kWileCoyote), another_path);

  EXPECT_EQ(2, entry->IncrementNonceCount());

  HttpAuthCache second_cache;
  // Will be overwritten by kRoot:kWileCoyote.
  second_cache.Add(origin, realm3_digest_handler->realm(),
                   realm3_digest_handler->auth_scheme(), "digest realm=Realm3",
                   AuthCredentials(kAlice2, k1234), path);
  // Should be left intact.
  second_cache.Add(origin, realm4_handler->realm(),
                   realm4_handler->auth_scheme(), "basic realm=Realm4",
                   AuthCredentials(kAdmin, kRoot), path);

  second_cache.UpdateAllFrom(first_cache);

  // Copied from first_cache.
  entry = second_cache.Lookup(origin, kRealm1, HttpAuth::AUTH_SCHEME_BASIC);
  EXPECT_TRUE(NULL != entry);
  EXPECT_EQ(kAlice, entry->credentials().username());
  EXPECT_EQ(k123, entry->credentials().password());

  // Copied from first_cache.
  entry = second_cache.Lookup(origin, kRealm2, HttpAuth::AUTH_SCHEME_BASIC);
  EXPECT_TRUE(NULL != entry);
  EXPECT_EQ(kAlice2, entry->credentials().username());
  EXPECT_EQ(k1234, entry->credentials().password());

  // Overwritten from first_cache.
  entry = second_cache.Lookup(origin, kRealm3, HttpAuth::AUTH_SCHEME_DIGEST);
  EXPECT_TRUE(NULL != entry);
  EXPECT_EQ(kRoot, entry->credentials().username());
  EXPECT_EQ(kWileCoyote, entry->credentials().password());
  // Nonce count should get copied.
  EXPECT_EQ(3, entry->IncrementNonceCount());

  // All paths should get copied.
  entry = second_cache.LookupByPath(origin, another_path);
  EXPECT_TRUE(NULL != entry);
  EXPECT_EQ(kRoot, entry->credentials().username());
  EXPECT_EQ(kWileCoyote, entry->credentials().password());

  // Left intact in second_cache.
  entry = second_cache.Lookup(origin, kRealm4, HttpAuth::AUTH_SCHEME_BASIC);
  EXPECT_TRUE(NULL != entry);
  EXPECT_EQ(kAdmin, entry->credentials().username());
  EXPECT_EQ(kRoot, entry->credentials().password());
}

// Test fixture class for eviction tests (contains helpers for bulk
// insertion and existence testing).
class HttpAuthCacheEvictionTest : public testing::Test {
 protected:
  HttpAuthCacheEvictionTest() : origin_("http://www.google.com") { }

  std::string GenerateRealm(int realm_i) {
    return base::StringPrintf("Realm %d", realm_i);
  }

  std::string GeneratePath(int realm_i, int path_i) {
    return base::StringPrintf("/%d/%d/x/y", realm_i, path_i);
  }

  void AddRealm(int realm_i) {
    AddPathToRealm(realm_i, 0);
  }

  void AddPathToRealm(int realm_i, int path_i) {
    cache_.Add(origin_,
               GenerateRealm(realm_i),
               HttpAuth::AUTH_SCHEME_BASIC,
               std::string(),
               AuthCredentials(kUsername, kPassword),
               GeneratePath(realm_i, path_i));
  }

  void CheckRealmExistence(int realm_i, bool exists) {
    const HttpAuthCache::Entry* entry =
        cache_.Lookup(
            origin_, GenerateRealm(realm_i), HttpAuth::AUTH_SCHEME_BASIC);
    if (exists) {
      EXPECT_FALSE(entry == NULL);
      EXPECT_EQ(GenerateRealm(realm_i), entry->realm());
    } else {
      EXPECT_TRUE(entry == NULL);
    }
  }

  void CheckPathExistence(int realm_i, int path_i, bool exists) {
    const HttpAuthCache::Entry* entry =
        cache_.LookupByPath(origin_, GeneratePath(realm_i, path_i));
    if (exists) {
      EXPECT_FALSE(entry == NULL);
      EXPECT_EQ(GenerateRealm(realm_i), entry->realm());
    } else {
      EXPECT_TRUE(entry == NULL);
    }
  }

  GURL origin_;
  HttpAuthCache cache_;

  static const int kMaxPaths = HttpAuthCache::kMaxNumPathsPerRealmEntry;
  static const int kMaxRealms = HttpAuthCache::kMaxNumRealmEntries;
};

// Add the maxinim number of realm entries to the cache. Each of these entries
// must still be retrievable. Next add three more entries -- since the cache is
// full this causes FIFO eviction of the first three entries by time of last
// use.
TEST_F(HttpAuthCacheEvictionTest, RealmEntryEviction) {
  base::SimpleTestTickClock test_clock;
  test_clock.SetNowTicks(base::TimeTicks::Now());
  cache_.set_tick_clock_for_testing(&test_clock);

  for (int i = 0; i < kMaxRealms; ++i) {
    AddRealm(i);
    test_clock.Advance(base::TimeDelta::FromSeconds(1));
  }

  for (int i = 0; i < kMaxRealms; ++i) {
    CheckRealmExistence(i, true);
    test_clock.Advance(base::TimeDelta::FromSeconds(1));
  }

  for (int i = 0; i < 3; ++i) {
    AddRealm(i + kMaxRealms);
    test_clock.Advance(base::TimeDelta::FromSeconds(1));
  }

  for (int i = 0; i < 3; ++i) {
    CheckRealmExistence(i, false);
    test_clock.Advance(base::TimeDelta::FromSeconds(1));
  }

  for (int i = 0; i < kMaxRealms; ++i) {
    CheckRealmExistence(i + 3, true);
    test_clock.Advance(base::TimeDelta::FromSeconds(1));
  }
}

// Add the maximum number of paths to a single realm entry. Each of these
// paths should be retrievable. Next add 3 more paths -- since the cache is
// full this causes FIFO eviction of the first three paths.
TEST_F(HttpAuthCacheEvictionTest, RealmPathEviction) {
  for (int i = 0; i < kMaxPaths; ++i)
    AddPathToRealm(0, i);

  for (int i = 1; i < kMaxRealms; ++i)
    AddRealm(i);

  for (int i = 0; i < 3; ++i)
    AddPathToRealm(0, i + kMaxPaths);

  for (int i = 0; i < 3; ++i)
    CheckPathExistence(0, i, false);

  for (int i = 0; i < kMaxPaths; ++i)
    CheckPathExistence(0, i + 3, true);

  for (int i = 0; i < kMaxRealms; ++i)
    CheckRealmExistence(i, true);
}

}  // namespace net
