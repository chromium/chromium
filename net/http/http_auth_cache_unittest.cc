// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/simple_test_clock.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/time/time.h"
#include "net/base/net_errors.h"
#include "net/base/network_anonymization_key.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/base/schemeful_site.h"
#include "net/http/http_auth_cache.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/scheme_host_port.h"

using base::ASCIIToUTF16;

namespace net {

namespace {

const char kRealm1[] = "Realm1";
const char kRealm2[] = "Realm2";
const char kRealm3[] = "Realm3";
const char kRealm4[] = "Realm4";
const char kRealm5[] = "Realm5";
const std::u16string k123(u"123");
const std::u16string k1234(u"1234");
const std::u16string k12345(u"12345");
const std::u16string kAdmin(u"admin");
const std::u16string kAlice(u"alice");
const std::u16string kAlice2(u"alice2");
const std::u16string kAlice3(u"alice3");
const std::u16string kPassword(u"password");
const std::u16string kRoot(u"root");
const std::u16string kUsername(u"username");
const std::u16string kWileCoyote(u"wilecoyote");

AuthCredentials CreateASCIICredentials(const char* username,
                                       const char* password) {
  return AuthCredentials(ASCIIToUTF16(username), ASCIIToUTF16(password));
}

bool DoesUrlMatchFilter(const std::set<std::string>& domains, const GURL& url) {
  std::string url_registerable_domain =
      registry_controlled_domains::GetDomainAndRegistry(
          url, registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
  bool found_domain = (domains.find(url_registerable_domain != ""
                                        ? url_registerable_domain
                                        : url.host()) != domains.end());

  return found_domain;
}

}  // namespace

// Test adding and looking-up cache entries (both by realm and by path).
TEST(HttpAuthCacheTest, Basic) {
  url::SchemeHostPort scheme_host_port(GURL("http://www.google.com"));
  url::SchemeHostPort scheme_host_port2(GURL("http://www.foobar.com"));
  HttpAuthCache cache(false /* key_entries_by_network_anonymization_key */);
  HttpAuthCache::Entry* entry;

  // Add cache entries for 4 realms: "Realm1", "Realm2", "Realm3" and
  // "Realm4"

  cache.Add(scheme_host_port, HttpAuth::AUTH_SERVER, kRealm1,
            HttpAuth::AUTH_SCHEME_BASIC, NetworkAnonymizationKey(),
            "Basic realm=Realm1",
            CreateASCIICredentials("realm1-user", "realm1-password"),
            "/foo/bar/index.html");

  cache.Add(scheme_host_port, HttpAuth::AUTH_SERVER, kRealm2,
            HttpAuth::AUTH_SCHEME_BASIC, NetworkAnonymizationKey(),
            "Basic realm=Realm2",
            CreateASCIICredentials("realm2-user", "realm2-password"),
            "/foo2/index.html");

  cache.Add(
      scheme_host_port, HttpAuth::AUTH_SERVER, kRealm3,
      HttpAuth::AUTH_SCHEME_BASIC, NetworkAnonymizationKey(),
      "Basic realm=Realm3",
      CreateASCIICredentials("realm3-basic-user", "realm3-basic-password"),
      std::string());

  cache.Add(
      scheme_host_port, HttpAuth::AUTH_SERVER, kRealm3,
      HttpAuth::AUTH_SCHEME_DIGEST, NetworkAnonymizationKey(),
      "Digest realm=Realm3",
      CreateASCIICredentials("realm3-digest-user", "realm3-digest-password"),
      "/baz/index.html");

  cache.Add(
      scheme_host_port, HttpAuth::AUTH_SERVER, kRealm4,
      HttpAuth::AUTH_SCHEME_BASIC, NetworkAnonymizationKey(),
      "Basic realm=Realm4",
      CreateASCIICredentials("realm4-basic-user", "realm4-basic-password"),
      "/");

  cache.Add(scheme_host_port2, HttpAuth::AUTH_SERVER, kRealm5,
            HttpAuth::AUTH_SCHEME_BASIC, NetworkAnonymizationKey(),
            "Basic realm=Realm5",
            CreateASCIICredentials("realm5-user", "realm5-password"), "/");
  cache.Add(
      scheme_host_port2, HttpAuth::AUTH_SERVER, kRealm3,
      HttpAuth::AUTH_SCHEME_BASIC, NetworkAnonymizationKey(),
      "Basic realm=Realm3",
      CreateASCIICredentials("realm3-basic-user", "realm3-basic-password"),
      std::string());

  // There is no Realm5 in `scheme_host_port`.
  entry = cache.Lookup(scheme_host_port, HttpAuth::AUTH_SERVER, kRealm5,
                       HttpAuth::AUTH_SCHEME_BASIC, NetworkAnonymizationKey());
  EXPECT_FALSE(entry);

  // While Realm3 does exist, the scheme is wrong.
  entry = cache.Lookup(url::SchemeHostPort(GURL("https://www.google.com")),
                       HttpAuth::AUTH_SERVER, kRealm3,
                       HttpAuth::AUTH_SCHEME_BASIC, NetworkAnonymizationKey());
  EXPECT_FALSE(entry);

  // Realm, scheme ok, authentication scheme wrong
  entry = cache.Lookup(url::SchemeHostPort(GURL("https://www.google.com")),
                       HttpAuth::AUTH_SERVER, kRealm1,
                       HttpAuth::AUTH_SCHEME_DIGEST, NetworkAnonymizationKey());
  EXPECT_FALSE(entry);

  // Valid lookup by SchemeHostPort, realm, scheme.
  entry = cache.Lookup(url::SchemeHostPort(GURL("http://www.google.com:80")),
                       HttpAuth::AUTH_SERVER, kRealm3,
                       HttpAuth::AUTH_SCHEME_BASIC, NetworkAnonymizationKey());
  ASSERT_TRUE(entry);
  EXPECT_EQ(HttpAuth::AUTH_SCHEME_BASIC, entry->scheme());
  EXPECT_EQ(kRealm3, entry->realm());
  EXPECT_EQ("Basic realm=Realm3", entry->auth_challenge());
  EXPECT_EQ(u"realm3-basic-user", entry->credentials().username());
  EXPECT_EQ(u"realm3-basic-password", entry->credentials().password());

  // Same realm, scheme with different SchemeHostPorts.
  HttpAuthCache::Entry* entry2 =
      cache.Lookup(url::SchemeHostPort(GURL("http://www.foobar.com:80")),
                   HttpAuth::AUTH_SERVER, kRealm3, HttpAuth::AUTH_SCHEME_BASIC,
                   NetworkAnonymizationKey());
  ASSERT_TRUE(entry2);
  EXPECT_NE(entry, entry2);

  // Valid lookup by SchemeHostPort, realm, scheme when there's a duplicate
  // SchemeHostPort, realm in the cache.
  entry = cache.Lookup(url::SchemeHostPort(GURL("http://www.google.com:80")),
                       HttpAuth::AUTH_SERVER, kRealm3,
                       HttpAuth::AUTH_SCHEME_DIGEST, NetworkAnonymizationKey());
  ASSERT_TRUE(entry);
  EXPECT_EQ(HttpAuth::AUTH_SCHEME_DIGEST, entry->scheme());
  EXPECT_EQ(kRealm3, entry->realm());
  EXPECT_EQ("Digest realm=Realm3", entry->auth_challenge());
  EXPECT_EQ(u"realm3-digest-user", entry->credentials().username());
  EXPECT_EQ(u"realm3-digest-password", entry->credentials().password());

  // Valid lookup by realm.
  entry = cache.Lookup(scheme_host_port, HttpAuth::AUTH_SERVER, kRealm2,
                       HttpAuth::AUTH_SCHEME_BASIC, NetworkAnonymizationKey());
  ASSERT_TRUE(entry);
  EXPECT_EQ(HttpAuth::AUTH_SCHEME_BASIC, entry->scheme());
  EXPECT_EQ(kRealm2, entry->realm());
  EXPECT_EQ("Basic realm=Realm2", entry->auth_challenge());
  EXPECT_EQ(u"realm2-user", entry->credentials().username());
  EXPECT_EQ(u"realm2-password", entry->credentials().password());

  // Check that subpaths are recognized.
  HttpAuthCache::Entry* p_realm2_entry =
      cache.Lookup(scheme_host_port, HttpAuth::AUTH_SERVER, kRealm2,
                   HttpAuth::AUTH_SCHEME_BASIC, NetworkAnonymizationKey());
  HttpAuthCache::Entry* p_realm4_entry =
      cache.Lookup(scheme_host_port, HttpAuth::AUTH_SERVER, kRealm4,
                   HttpAuth::AUTH_SCHEME_BASIC, NetworkAnonymizationKey());
  EXPECT_TRUE(p_realm2_entry);
  EXPECT_TRUE(p_realm4_entry);
  HttpAuthCache::Entry realm2_entry = *p_realm2_entry;
  HttpAuthCache::Entry realm4_entry = *p_realm4_entry;
  // Realm4 applies to '/' and Realm2 applies to '/foo2/'.
  // LookupByPath() should return the closest enclosing path.
  // Positive tests:
  entry = cache.LookupByPath(scheme_host_port, HttpAuth::AUTH_SERVER,
                             NetworkAnonymizationKey(), "/foo2/index.html");
  EXPECT_TRUE(realm2_entry.IsEqualForTesting(*entry));
  entry = cache.LookupByPath(scheme_host_port, HttpAuth::AUTH_SERVER,
                             NetworkAnonymizationKey(), "/foo2/foobar.html");
  EXPECT_TRUE(realm2_entry.IsEqualForTesting(*entry));
  entry = cache.LookupByPath(scheme_host_port, HttpAuth::AUTH_SERVER,
                             NetworkAnonymizationKey(), "/foo2/bar/index.html");
  EXPECT_TRUE(realm2_entry.IsEqualForTesting(*entry));
  entry = cache.LookupByPath(scheme_host_port, HttpAuth::AUTH_SERVER,
                             NetworkAnonymizationKey(), "/foo2/");
  EXPECT_TRUE(realm2_entry.IsEqualForTesting(*entry));
  entry = cache.LookupByPath(scheme_host_port, HttpAuth::AUTH_SERVER,
                             NetworkAnonymizationKey(), "/foo2");
  EXPECT_TRUE(realm4_entry.IsEqualForTesting(*entry));
  entry = cache.LookupByPath(scheme_host_port, HttpAuth::AUTH_SERVER,
                             NetworkAnonymizationKey(), "/");
  EXPECT_TRUE(realm4_entry.IsEqualForTesting(*entry));

  // Negative tests:
  entry = cache.LookupByPath(scheme_host_port, HttpAuth::AUTH_SERVER,
                             NetworkAnonymizationKey(), "/foo3/index.html");
  EXPECT_FALSE(realm2_entry.IsEqualForTesting(*entry));
  entry = cache.LookupByPath(scheme_host_port, HttpAuth::AUTH_SERVER,
                             NetworkAnonymizationKey(), std::string());
  EXPECT_FALSE(realm2_entry.IsEqualForTesting(*entry));

  // Confirm we find the same realm, different auth scheme by path lookup
  HttpAuthCache::Entry* p_realm3_digest_entry =
      cache.Lookup(scheme_host_port, HttpAuth::AUTH_SERVER, kRealm3,
                   HttpAuth::AUTH_SCHEME_DIGEST, NetworkAnonymizationKey());
  EXPECT_TRUE(p_realm3_digest_entry);
  HttpAuthCache::Entry realm3_digest_entry = *p_realm3_digest_entry;
  entry = cache.LookupByPath(scheme_host_port, HttpAuth::AUTH_SERVER,
                             NetworkAnonymizationKey(), "/baz/index.html");
  EXPECT_TRUE(realm3_digest_entry.IsEqualForTesting(*entry));
  entry = cache.LookupByPath(scheme_host_port, HttpAuth::AUTH_SERVER,
                             NetworkAnonymizationKey(), "/baz/");
  EXPECT_TRUE(realm3_digest_entry.IsEqualForTesting(*entry));
  entry = cache.LookupByPath(scheme_host_port, HttpAuth::AUTH_SERVER,
                             NetworkAnonymizationKey(), "/baz");
  EXPECT_FALSE(realm3_digest_entry.IsEqualForTesting(*entry));

  // Confirm we find the same realm, different auth scheme by path lookup
  HttpAuthCache::Entry* p_realm3DigestEntry =
      cache.Lookup(scheme_host_port, HttpAuth::AUTH_SERVER, kRealm3,
                   HttpAuth::AUTH_SCHEME_DIGEST, NetworkAnonymizationKey());
  EXPECT_TRUE(p_realm3DigestEntry);
  HttpAuthCache::Entry realm3DigestEntry = *p_realm3DigestEntry;
  entry = cache.LookupByPath(scheme_host_port, HttpAuth::AUTH_SERVER,
                             NetworkAnonymizationKey(), "/baz/index.html");
  EXPECT_TRUE(realm3DigestEntry.IsEqualForTesting(*entry));
  entry = cache.LookupByPath(scheme_host_port, HttpAuth::AUTH_SERVER,
                             NetworkAnonymizationKey(), "/baz/");
  EXPECT_TRUE(realm3DigestEntry.IsEqualForTesting(*entry));
  entry = cache.LookupByPath(scheme_host_port, HttpAuth::AUTH_SERVER,
                             NetworkAnonymizationKey(), "/baz");
  EXPECT_FALSE(realm3DigestEntry.IsEqualForTesting(*entry));

  // Lookup using empty path (may be used for proxy).
  entry = cache.LookupByPath(scheme_host_port, HttpAuth::AUTH_SERVER,
                             NetworkAnonymizationKey(), std::string());
  EXPECT_TRUE(entry);
  EXPECT_EQ(HttpAuth::AUTH_SCHEME_BASIC, entry->scheme());
  EXPECT_EQ(kRealm3, entry->realm());
}

// Make sure server and proxy credentials are treated separately.
TEST(HttpAuthCacheTest, SeparateByTarget) {
  const std::u16string kServerUser = u"server_user";
  const std::u16string kServerPass = u"server_pass";
  const std::u16string kProxyUser = u"proxy_user";
  const std::u16string kProxyPass = u"proxy_pass";

  const char kServerPath[] = "/foo/bar/index.html";

  url::SchemeHostPort scheme_host_port(GURL("http://www.google.com"));
  HttpAuthCache cache(false /* key_entries_by_network_anonymization_key */);
  HttpAuthCache::Entry* entry;

  // Add AUTH_SERVER entry.
  cache.Add(scheme_host_port, HttpAuth::AUTH_SERVER, kRealm1,
            HttpAuth::AUTH_SCHEME_BASIC, NetworkAnonymizationKey(),
            "Basic realm=Realm1", AuthCredentials(kServerUser, kServerPass),
            kServerPath);

  // Make sure credentials are only accessible with AUTH_SERVER target.
  entry = cache.Lookup(scheme_host_port, HttpAuth::AUTH_SERVER, kRealm1,
                       HttpAuth::AUTH_SCHEME_BASIC, NetworkAnonymizationKey());
  ASSERT_TRUE(entry);
  EXPECT_EQ(entry->credentials().username(), kServerUser);
  EXPECT_EQ(entry->credentials().password(), kServerPass);
  EXPECT_EQ(entry, cache.LookupByPath(scheme_host_port, HttpAuth::AUTH_SERVER,
                                      NetworkAnonymizationKey(), kServerPath));
  EXPECT_FALSE(cache.Lookup(scheme_host_port, HttpAuth::AUTH_PROXY, kRealm1,
                            HttpAuth::AUTH_SCHEME_BASIC,
                            NetworkAnonymizationKey()));
  EXPECT_FALSE(cache.LookupByPath(scheme_host_port, HttpAuth::AUTH_PROXY,
                                  NetworkAnonymizationKey(), kServerPath));

  // Add AUTH_PROXY entry with same SchemeHostPort and realm but different
  // credentials.
  cache.Add(scheme_host_port, HttpAuth::AUTH_PROXY, kRealm1,
            HttpAuth::AUTH_SCHEME_BASIC, NetworkAnonymizationKey(),
            "Basic realm=Realm1", AuthCredentials(kProxyUser, kProxyPass), "/");

  // Make sure credentials are only accessible with the corresponding target.
  entry = cache.Lookup(scheme_host_port, HttpAuth::AUTH_SERVER, kRealm1,
                       HttpAuth::AUTH_SCHEME_BASIC, NetworkAnonymizationKey());
  ASSERT_TRUE(entry);
  EXPECT_EQ(entry->credentials().username(), kServerUser);
  EXPECT_EQ(entry->credentials().password(), kServerPass);
  EXPECT_EQ(entry, cache.LookupByPath(scheme_host_port, HttpAuth::AUTH_SERVER,
                                      NetworkAnonymizationKey(), kServerPath));
  entry = cache.Lookup(scheme_host_port, HttpAuth::AUTH_PROXY, kRealm1,
                       HttpAuth::AUTH_SCHEME_BASIC, NetworkAnonymizationKey());
  ASSERT_TRUE(entry);
  EXPECT_EQ(entry->credentials().username(), kProxyUser);
  EXPECT_EQ(entry->credentials().password(), kProxyPass);
  EXPECT_EQ(entry, cache.LookupByPath(scheme_host_port, HttpAuth::AUTH_PROXY,
                                      NetworkAnonymizationKey(), "/"));

  // Remove the AUTH_SERVER entry.
  EXPECT_TRUE(cache.Remove(scheme_host_port, HttpAuth::AUTH_SERVER, kRealm1,
                           HttpAuth::AUTH_SCHEME_BASIC,
                           NetworkAnonymizationKey(),
                           AuthCredentials(kServerUser, kServerPass)));

  // Verify that only the AUTH_SERVER entry was removed.
  EXPECT_FALSE(cache.Lookup(scheme_host_port, HttpAuth::AUTH_SERVER, kRealm1,
                            HttpAuth::AUTH_SCHEME_BASIC,
                            NetworkAnonymizationKey()));
  EXPECT_FALSE(cache.LookupByPath(scheme_host_port, HttpAuth::AUTH_SERVER,
                                  NetworkAnonymizationKey(), kServerPath));
  entry = cache.Lookup(scheme_host_port, HttpAuth::AUTH_PROXY, kRealm1,
                       HttpAuth::AUTH_SCHEME_BASIC, NetworkAnonymizationKey());
  ASSERT_TRUE(entry);
  EXPECT_EQ(entry->credentials().username(), kProxyUser);
  EXPECT_EQ(entry->credentials().password(), kProxyPass);
  EXPECT_EQ(entry, cache.LookupByPath(scheme_host_port, HttpAuth::AUTH_PROXY,
                                      NetworkAnonymizationKey(), "/"));

  // Remove the AUTH_PROXY entry.
  EXPECT_TRUE(cache.Remove(scheme_host_port, HttpAuth::AUTH_PROXY, kRealm1,
                           HttpAuth::AUTH_SCHEME_BASIC,
                           NetworkAnonymizationKey(),
                           AuthCredentials(kProxyUser, kProxyPass)));

  // Verify that neither entry remains.
  EXPECT_FALSE(cache.Lookup(scheme_host_port, HttpAuth::AUTH_SERVER, kRealm1,
                            HttpAuth::AUTH_SCHEME_BASIC,
                            NetworkAnonymizationKey()));
  EXPECT_FALSE(cache.LookupByPath(scheme_host_port, HttpAuth::AUTH_SERVER,
                                  NetworkAnonymizationKey(), kServerPath));
  EXPECT_FALSE(cache.Lookup(scheme_host_port, HttpAuth::AUTH_PROXY, kRealm1,
                            HttpAuth::AUTH_SCHEME_BASIC,
                            NetworkAnonymizationKey()));
  EXPECT_FALSE(cache.LookupByPath(scheme_host_port, HttpAuth::AUTH_PROXY,
                                  NetworkAnonymizationKey(), "/"));
}

// Make sure server credentials with different NetworkAnonymizationKeys are
// treated separately if |key_entries_by_network_anonymization_key| is set to
// true.
TEST(HttpAuthCacheTest, SeparateServersByNetworkAnonymizationKey) {
  const SchemefulSite kSite1(GURL("https://foo.test/"));
  auto kNetworkAnonymizationKey1 =
      NetworkAnonymizationKey::CreateSameSite(kSite1);
  const SchemefulSite kSite2(GURL("https://bar.test/"));
  auto kNetworkAnonymizationKey2 =
      NetworkAnonymizationKey::CreateSameSite(kSite2);

  url::SchemeHostPort kSchemeHostPort(GURL("http://www.google.com"));
  const char kPath[] = "/";

  const std::u16string kUser1 = u"user1";
  const std::u16string kPass1 = u"pass1";
  const std::u16string kUser2 = u"user2";
  const std::u16string kPass2 = u"pass2";

  for (bool key_entries_by_network_anonymization_key : {false, true}) {
    HttpAuthCache cache(key_entries_by_network_anonymization_key);
    HttpAuthCache::Entry* entry;

    // Add entry for kNetworkAnonymizationKey1.
    cache.Add(kSchemeHostPort, HttpAuth::AUTH_SERVER, kRealm1,
              HttpAuth::AUTH_SCHEME_BASIC, kNetworkAnonymizationKey1,
              "Basic realm=Realm1", AuthCredentials(kUser1, kPass1), kPath);

    entry =
        cache.Lookup(kSchemeHostPort, HttpAuth::AUTH_SERVER, kRealm1,
                     HttpAuth::AUTH_SCHEME_BASIC, kNetworkAnonymizationKey1);
    ASSERT_TRUE(entry);
    EXPECT_EQ(entry->credentials().username(), kUser1);
    EXPECT_EQ(entry->credentials().password(), kPass1);
    EXPECT_EQ(entry, cache.LookupByPath(kSchemeHostPort, HttpAuth::AUTH_SERVER,
                                        kNetworkAnonymizationKey1, kPath));
    if (key_entries_by_network_anonymization_key) {
      EXPECT_FALSE(cache.Lookup(kSchemeHostPort, HttpAuth::AUTH_SERVER, kRealm1,
                                HttpAuth::AUTH_SCHEME_BASIC,
                                kNetworkAnonymizationKey2));
      EXPECT_FALSE(cache.LookupByPath(kSchemeHostPort, HttpAuth::AUTH_SERVER,
                                      kNetworkAnonymizationKey2, kPath));
    } else {
      EXPECT_EQ(entry, cache.Lookup(kSchemeHostPort, HttpAuth::AUTH_SERVER,
                                    kRealm1, HttpAuth::AUTH_SCHEME_BASIC,
                                    kNetworkAnonymizationKey2));
      EXPECT_EQ(entry,
                cache.LookupByPath(kSchemeHostPort, HttpAuth::AUTH_SERVER,
                                   kNetworkAnonymizationKey2, kPath));
    }

    // Add entry for kNetworkAnonymizationKey2.
    cache.Add(kSchemeHostPort, HttpAuth::AUTH_SERVER, kRealm1,
              HttpAuth::AUTH_SCHEME_BASIC, kNetworkAnonymizationKey2,
              "Basic realm=Realm1", AuthCredentials(kUser2, kPass2), kPath);

    entry =
        cache.Lookup(kSchemeHostPort, HttpAuth::AUTH_SERVER, kRealm1,
                     HttpAuth::AUTH_SCHEME_BASIC, kNetworkAnonymizationKey2);
    ASSERT_TRUE(entry);
    EXPECT_EQ(entry->credentials().username(), kUser2);
    EXPECT_EQ(entry->credentials().password(), kPass2);
    EXPECT_EQ(entry, cache.LookupByPath(kSchemeHostPort, HttpAuth::AUTH_SERVER,
                                        kNetworkAnonymizationKey2, kPath));
    entry =
        cache.Lookup(kSchemeHostPort, HttpAuth::AUTH_SERVER, kRealm1,
                     HttpAuth::AUTH_SCHEME_BASIC, kNetworkAnonymizationKey1);
    ASSERT_TRUE(entry);
    EXPECT_EQ(entry, cache.LookupByPath(kSchemeHostPort, HttpAuth::AUTH_SERVER,
                                        kNetworkAnonymizationKey1, kPath));
    if (key_entries_by_network_anonymization_key) {
      EXPECT_EQ(entry->credentials().username(), kUser1);
      EXPECT_EQ(entry->credentials().password(), kPass1);
    } else {
      EXPECT_EQ(entry->credentials().username(), kUser2);
      EXPECT_EQ(entry->credentials().password(), kPass2);
    }

    // Remove the entry that was just added.
    EXPECT_TRUE(cache.Remove(kSchemeHostPort, HttpAuth::AUTH_SERVER, kRealm1,
                             HttpAuth::AUTH_SCHEME_BASIC,
                             kNetworkAnonymizationKey2,
                             AuthCredentials(kUser2, kPass2)));

    EXPECT_FALSE(cache.Lookup(kSchemeHostPort, HttpAuth::AUTH_SERVER, kRealm1,
                              HttpAuth::AUTH_SCHEME_BASIC,
                              kNetworkAnonymizationKey2));
    EXPECT_FALSE(cache.LookupByPath(kSchemeHostPort, HttpAuth::AUTH_SERVER,
                                    kNetworkAnonymizationKey2, kPath));
    if (key_entries_by_network_anonymization_key) {
      entry =
          cache.Lookup(kSchemeHostPort, HttpAuth::AUTH_SERVER, kRealm1,
                       HttpAuth::AUTH_SCHEME_BASIC, kNetworkAnonymizationKey1);
      ASSERT_TRUE(entry);
      EXPECT_EQ(entry->credentials().username(), kUser1);
      EXPECT_EQ(entry->credentials().password(), kPass1);
      EXPECT_EQ(entry,
                cache.LookupByPath(kSchemeHostPort, HttpAuth::AUTH_SERVER,
                                   kNetworkAnonymizationKey1, kPath));
    } else {
      EXPECT_FALSE(cache.Lookup(kSchemeHostPort, HttpAuth::AUTH_SERVER, kRealm1,
                                HttpAuth::AUTH_SCHEME_BASIC,
                                kNetworkAnonymizationKey1));
      EXPECT_FALSE(cache.LookupByPath(kSchemeHostPort, HttpAuth::AUTH_SERVER,
                                      kNetworkAnonymizationKey1, kPath));
    }
  }
}

// Make sure added proxy credentials ignore NetworkAnonymizationKey, even if if
// |key_entries_by_network_anonymization_key| is set to true.
TEST(HttpAuthCacheTest, NeverSeparateProxiesByNetworkAnonymizationKey) {
  const SchemefulSite kSite1(GURL("https://foo.test/"));
  auto kNetworkAnonymizationKey1 =
      NetworkAnonymizationKey::CreateSameSite(kSite1);
  const SchemefulSite kSite2(GURL("https://bar.test/"));
  auto kNetworkAnonymizationKey2 =
      NetworkAnonymizationKey::CreateSameSite(kSite2);

  url::SchemeHostPort kSchemeHostPort(GURL("http://www.google.com"));
  const char kPath[] = "/";

  const std::u16string kUser1 = u"user1";
  const std::u16string kPass1 = u"pass1";
  const std::u16string kUser2 = u"user2";
  const std::u16string kPass2 = u"pass2";

  for (bool key_entries_by_network_anonymization_key : {false, true}) {
    HttpAuthCache cache(key_entries_by_network_anonymization_key);
    HttpAuthCache::Entry* entry;

    // Add entry for kNetworkAnonymizationKey1.
    cache.Add(kSchemeHostPort, HttpAuth::AUTH_PROXY, kRealm1,
              HttpAuth::AUTH_SCHEME_BASIC, kNetworkAnonymizationKey1,
              "Basic realm=Realm1", AuthCredentials(kUser1, kPass1), kPath);

    entry =
        cache.Lookup(kSchemeHostPort, HttpAuth::AUTH_PROXY, kRealm1,
                     HttpAuth::AUTH_SCHEME_BASIC, kNetworkAnonymizationKey1);
    ASSERT_TRUE(entry);
    EXPECT_EQ(entry->credentials().username(), kUser1);
    EXPECT_EQ(entry->credentials().password(), kPass1);
    EXPECT_EQ(entry, cache.LookupByPath(kSchemeHostPort, HttpAuth::AUTH_PROXY,
                                        kNetworkAnonymizationKey1, kPath));
    EXPECT_EQ(entry, cache.Lookup(kSchemeHostPort, HttpAuth::AUTH_PROXY,
                                  kRealm1, HttpAuth::AUTH_SCHEME_BASIC,
                                  kNetworkAnonymizationKey2));
    EXPECT_EQ(entry, cache.LookupByPath(kSchemeHostPort, HttpAuth::AUTH_PROXY,
                                        kNetworkAnonymizationKey2, kPath));

    // Add entry for kNetworkAnonymizationKey2. It should overwrite the entry
    // for kNetworkAnonymizationKey1.
    cache.Add(kSchemeHostPort, HttpAuth::AUTH_PROXY, kRealm1,
              HttpAuth::AUTH_SCHEME_BASIC, kNetworkAnonymizationKey2,
              "Basic realm=Realm1", AuthCredentials(kUser2, kPass2), kPath);

    entry =
        cache.Lookup(kSchemeHostPort, HttpAuth::AUTH_PROXY, kRealm1,
                     HttpAuth::AUTH_SCHEME_BASIC, kNetworkAnonymizationKey2);
    ASSERT_TRUE(entry);
    EXPECT_EQ(entry->credentials().username(), kUser2);
    EXPECT_EQ(entry->credentials().password(), kPass2);
    EXPECT_EQ(entry, cache.LookupByPath(kSchemeHostPort, HttpAuth::AUTH_PROXY,
                                        kNetworkAnonymizationKey2, kPath));
    EXPECT_EQ(entry, cache.Lookup(kSchemeHostPort, HttpAuth::AUTH_PROXY,
                                  kRealm1, HttpAuth::AUTH_SCHEME_BASIC,
                                  kNetworkAnonymizationKey1));
    EXPECT_EQ(entry, cache.LookupByPath(kSchemeHostPort, HttpAuth::AUTH_PROXY,
                                        kNetworkAnonymizationKey1, kPath));

    // Remove the entry that was just added using an empty
    // NetworkAnonymizationKey.
    EXPECT_TRUE(cache.Remove(kSchemeHostPort, HttpAuth::AUTH_PROXY, kRealm1,
                             HttpAuth::AUTH_SCHEME_BASIC,
                             NetworkAnonymizationKey(),
                             AuthCredentials(kUser2, kPass2)));

    EXPECT_FALSE(cache.Lookup(kSchemeHostPort, HttpAuth::AUTH_PROXY, kRealm1,
                              HttpAuth::AUTH_SCHEME_BASIC,
                              kNetworkAnonymizationKey2));
    EXPECT_FALSE(cache.LookupByPath(kSchemeHostPort, HttpAuth::AUTH_PROXY,
                                    kNetworkAnonymizationKey2, kPath));
    EXPECT_FALSE(cache.Lookup(kSchemeHostPort, HttpAuth::AUTH_PROXY, kRealm1,
                              HttpAuth::AUTH_SCHEME_BASIC,
                              kNetworkAnonymizationKey1));
    EXPECT_FALSE(cache.LookupByPath(kSchemeHostPort, HttpAuth::AUTH_PROXY,
                                    kNetworkAnonymizationKey1, kPath));
  }
}

// Test that SetKeyServerEntriesByNetworkAnonymizationKey() deletes server
// credentials when it toggles the setting. This test uses an empty
// NetworkAnonymizationKey() for all entries, as the interesting part of this
// method is what type entries are deleted, which doesn't depend on the
// NetworkAnonymizationKey the entries use.
TEST(HttpAuthCacheTest, SetKeyServerEntriesByNetworkAnonymizationKey) {
  const url::SchemeHostPort kSchemeHostPort(GURL("http://www.google.com"));
  const char kPath[] = "/";

  const std::u16string kUser1 = u"user1";
  const std::u16string kPass1 = u"pass1";
  const std::u16string kUser2 = u"user2";
  const std::u16string kPass2 = u"pass2";

  for (bool initially_key_entries_by_network_anonymization_key :
       {false, true}) {
    for (bool to_key_entries_by_network_anonymization_key : {false, true}) {
      HttpAuthCache cache(initially_key_entries_by_network_anonymization_key);
      EXPECT_EQ(initially_key_entries_by_network_anonymization_key,
                cache.key_server_entries_by_network_anonymization_key());

      cache.Add(kSchemeHostPort, HttpAuth::AUTH_PROXY, kRealm1,
                HttpAuth::AUTH_SCHEME_BASIC, NetworkAnonymizationKey(),
                "Basic realm=Realm1", AuthCredentials(kUser1, kPass1), kPath);
      cache.Add(kSchemeHostPort, HttpAuth::AUTH_SERVER, kRealm1,
                HttpAuth::AUTH_SCHEME_BASIC, NetworkAnonymizationKey(),
                "Basic realm=Realm1", AuthCredentials(kUser2, kPass2), kPath);

      EXPECT_TRUE(cache.Lookup(kSchemeHostPort, HttpAuth::AUTH_PROXY, kRealm1,
                               HttpAuth::AUTH_SCHEME_BASIC,
                               NetworkAnonymizationKey()));
      EXPECT_TRUE(cache.Lookup(kSchemeHostPort, HttpAuth::AUTH_SERVER, kRealm1,
                               HttpAuth::AUTH_SCHEME_BASIC,
                               NetworkAnonymizationKey()));

      cache.SetKeyServerEntriesByNetworkAnonymizationKey(
          to_key_entries_by_network_anonymization_key);
      EXPECT_EQ(to_key_entries_by_network_anonymization_key,
                cache.key_server_entries_by_network_anonymization_key());

      // AUTH_PROXY credentials should always remain in the cache.
      HttpAuthCache::Entry* entry =
          cache.LookupByPath(kSchemeHostPort, HttpAuth::AUTH_PROXY,
                             NetworkAnonymizationKey(), kPath);
      ASSERT_TRUE(entry);
      EXPECT_EQ(entry->credentials().username(), kUser1);
      EXPECT_EQ(entry->credentials().password(), kPass1);

      entry = cache.LookupByPath(kSchemeHostPort, HttpAuth::AUTH_SERVER,
                                 NetworkAnonymizationKey(), kPath);
      // AUTH_SERVER credentials should only remain in the cache if the proxy
      // configuration changes.
      EXPECT_EQ(initially_key_entries_by_network_anonymization_key ==
                    to_key_entries_by_network_anonymization_key,
                !!entry);
      if (entry) {
        EXPECT_EQ(entry->credentials().username(), kUser2);
        EXPECT_EQ(entry->credentials().password(), kPass2);
      }
    }
  }
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
  HttpAuthCache cache(false /* key_entries_by_network_anonymization_key */);
  url::SchemeHostPort scheme_host_port(GURL("http://www.foobar.com:70"));
  const std::string kAuthChallenge = "Basic realm=MyRealm";
  const std::string kRealm = "MyRealm";

  HttpAuthCache::Entry* orig_entry = cache.Add(
      scheme_host_port, HttpAuth::AUTH_SERVER, kRealm,
      HttpAuth::AUTH_SCHEME_BASIC, NetworkAnonymizationKey(), kAuthChallenge,
      CreateASCIICredentials("user1", "password1"), "/x/y/z/");
  cache.Add(scheme_host_port, HttpAuth::AUTH_SERVER, kRealm,
            HttpAuth::AUTH_SCHEME_BASIC, NetworkAnonymizationKey(),
            kAuthChallenge, CreateASCIICredentials("user2", "password2"),
            "/z/y/x/");
  cache.Add(scheme_host_port, HttpAuth::AUTH_SERVER, kRealm,
            HttpAuth::AUTH_SCHEME_BASIC, NetworkAnonymizationKey(),
            kAuthChallenge, CreateASCIICredentials("user3", "password3"),
            "/z/y");

  HttpAuthCache::Entry* entry =
      cache.Lookup(scheme_host_port, HttpAuth::AUTH_SERVER, kRealm,
                   HttpAuth::AUTH_SCHEME_BASIC, NetworkAnonymizationKey());

  EXPECT_TRUE(entry == orig_entry);
  EXPECT_EQ(u"user3", entry->credentials().username());
  EXPECT_EQ(u"password3", entry->credentials().password());

  EXPECT_EQ(2U, entry->paths_.size());
  EXPECT_EQ("/z/", entry->paths_.front());
  EXPECT_EQ("/x/y/z/", entry->paths_.back());
}

TEST(HttpAuthCacheTest, Remove) {
  url::SchemeHostPort scheme_host_port(GURL("http://foobar2.com"));

  HttpAuthCache cache(false /* key_entries_by_network_anonymization_key */);
  cache.Add(scheme_host_port, HttpAuth::AUTH_SERVER, kRealm1,
            HttpAuth::AUTH_SCHEME_BASIC, NetworkAnonymizationKey(),
            "basic realm=Realm1", AuthCredentials(kAlice, k123), "/");
  cache.Add(scheme_host_port, HttpAuth::AUTH_SERVER, kRealm2,
            HttpAuth::AUTH_SCHEME_BASIC, NetworkAnonymizationKey(),
            "basic realm=Realm2", CreateASCIICredentials("bob", "princess"),
            "/");
  cache.Add(scheme_host_port, HttpAuth::AUTH_SERVER, kRealm3,
            HttpAuth::AUTH_SCHEME_BASIC, NetworkAnonymizationKey(),
            "basic realm=Realm3", AuthCredentials(kAdmin, kPassword), "/");
  cache.Add(scheme_host_port, HttpAuth::AUTH_SERVER, kRealm3,
            HttpAuth::AUTH_SCHEME_DIGEST, NetworkAnonymizationKey(),
            "digest realm=Realm3", AuthCredentials(kRoot, kWileCoyote), "/");

  // Fails, because there is no realm "Realm5".
  EXPECT_FALSE(cache.Remove(scheme_host_port, HttpAuth::AUTH_SERVER, kRealm5,
                            HttpAuth::AUTH_SCHEME_BASIC,
                            NetworkAnonymizationKey(),
                            AuthCredentials(kAlice, k123)));

  // Fails because the origin is wrong.
  EXPECT_FALSE(
      cache.Remove(url::SchemeHostPort(GURL("http://foobar2.com:100")),
                   HttpAuth::AUTH_SERVER, kRealm1, HttpAuth::AUTH_SCHEME_BASIC,
                   NetworkAnonymizationKey(), AuthCredentials(kAlice, k123)));

  // Fails because the username is wrong.
  EXPECT_FALSE(cache.Remove(scheme_host_port, HttpAuth::AUTH_SERVER, kRealm1,
                            HttpAuth::AUTH_SCHEME_BASIC,
                            NetworkAnonymizationKey(),
                            AuthCredentials(kAlice2, k123)));

  // Fails because the password is wrong.
  EXPECT_FALSE(cache.Remove(scheme_host_port, HttpAuth::AUTH_SERVER, kRealm1,
                            HttpAuth::AUTH_SCHEME_BASIC,
                            NetworkAnonymizationKey(),
                            AuthCredentials(kAlice, k1234)));

  // Fails because the authentication type is wrong.
  EXPECT_FALSE(cache.Remove(scheme_host_port, HttpAuth::AUTH_SERVER, kRealm1,
                            HttpAuth::AUTH_SCHEME_DIGEST,
                            NetworkAnonymizationKey(),
                            AuthCredentials(kAlice, k123)));

  // Succeeds.
  EXPECT_TRUE(cache.Remove(scheme_host_port, HttpAuth::AUTH_SERVER, kRealm1,
                           HttpAuth::AUTH_SCHEME_BASIC,
                           NetworkAnonymizationKey(),
                           AuthCredentials(kAlice, k123)));

  // Fails because we just deleted the entry!
  EXPECT_FALSE(cache.Remove(scheme_host_port, HttpAuth::AUTH_SERVER, kRealm1,
                            HttpAuth::AUTH_SCHEME_BASIC,
                            NetworkAnonymizationKey(),
                            AuthCredentials(kAlice, k123)));

  // Succeed when there are two authentication types for the same origin,realm.
  EXPECT_TRUE(cache.Remove(scheme_host_port, HttpAuth::AUTH_SERVER, kRealm3,
                           HttpAuth::AUTH_SCHEME_DIGEST,
                           NetworkAnonymizationKey(),
                           AuthCredentials(kRoot, kWileCoyote)));

  // Succeed as above, but when entries were added in opposite order
  cache.Add(scheme_host_port, HttpAuth::AUTH_SERVER, kRealm3,
            HttpAuth::AUTH_SCHEME_DIGEST, NetworkAnonymizationKey(),
            "digest realm=Realm3", AuthCredentials(kRoot, kWileCoyote), "/");
  EXPECT_TRUE(cache.Remove(scheme_host_port, HttpAuth::AUTH_SERVER, kRealm3,
                           HttpAuth::AUTH_SCHEME_BASIC,
                           NetworkAnonymizationKey(),
                           AuthCredentials(kAdmin, kPassword)));

  // Make sure that removing one entry still leaves the other available for
  // lookup.
  HttpAuthCache::Entry* entry =
      cache.Lookup(scheme_host_port, HttpAuth::AUTH_SERVER, kRealm3,
                   HttpAuth::AUTH_SCHEME_DIGEST, NetworkAnonymizationKey());
  EXPECT_FALSE(nullptr == entry);
}

TEST(HttpAuthCacheTest, ClearEntriesAddedBetween) {
  url::SchemeHostPort scheme_host_port(GURL("http://foobar.com"));

  base::Time start_time;
  ASSERT_TRUE(base::Time::FromString("30 May 2018 12:00:00", &start_time));
  base::SimpleTestClock test_clock;
  test_clock.SetNow(start_time);

  HttpAuthCache cache(false /* key_entries_by_network_anonymization_key */);
  cache.set_clock_for_testing(&test_clock);

  cache.Add(scheme_host_port, HttpAuth::AUTH_SERVER, kRealm1,
            HttpAuth::AUTH_SCHEME_BASIC, NetworkAnonymizationKey(),
            "basic realm=Realm1", AuthCredentials(kAlice, k123), "/");
  cache.Add(scheme_host_port, HttpAuth::AUTH_SERVER, kRealm2,
            HttpAuth::AUTH_SCHEME_BASIC, NetworkAnonymizationKey(),
            "basic realm=Realm2", AuthCredentials(kRoot, kWileCoyote), "/");

  test_clock.Advance(base::Seconds(10));  // Time now 12:00:10
  cache.Add(scheme_host_port, HttpAuth::AUTH_SERVER, kRealm3,
            HttpAuth::AUTH_SCHEME_BASIC, NetworkAnonymizationKey(),
            "basic realm=Realm3", AuthCredentials(kAlice2, k1234), "/");
  cache.Add(scheme_host_port, HttpAuth::AUTH_SERVER, kRealm4,
            HttpAuth::AUTH_SCHEME_BASIC, NetworkAnonymizationKey(),
            "basic realm=Realm4", AuthCredentials(kUsername, kPassword), "/");
  // Add path to existing entry.
  cache.Add(scheme_host_port, HttpAuth::AUTH_SERVER, kRealm2,
            HttpAuth::AUTH_SCHEME_BASIC, NetworkAnonymizationKey(),
            "basic realm=Realm2", AuthCredentials(kAdmin, kPassword), "/baz/");

  test_clock.Advance(base::Seconds(10));  // Time now 12:00:20
  cache.Add(scheme_host_port, HttpAuth::AUTH_SERVER, kRealm5,
            HttpAuth::AUTH_SCHEME_BASIC, NetworkAnonymizationKey(),
            "basic realm=Realm5", AuthCredentials(kAlice3, k12345), "/");

  base::Time test_time1;
  ASSERT_TRUE(base::Time::FromString("30 May 2018 12:00:05", &test_time1));
  base::Time test_time2;
  ASSERT_TRUE(base::Time::FromString("30 May 2018 12:00:15", &test_time2));
  cache.ClearEntriesAddedBetween(test_time1, test_time2,
                                 base::RepeatingCallback<bool(const GURL&)>());

  // Realms 1 and 2 are older than 12:00:05 and should not be cleared
  EXPECT_NE(nullptr, cache.Lookup(scheme_host_port, HttpAuth::AUTH_SERVER,
                                  kRealm1, HttpAuth::AUTH_SCHEME_BASIC,
                                  NetworkAnonymizationKey()));
  EXPECT_NE(nullptr, cache.Lookup(scheme_host_port, HttpAuth::AUTH_SERVER,
                                  kRealm2, HttpAuth::AUTH_SCHEME_BASIC,
                                  NetworkAnonymizationKey()));

  // Realms 5 is newer than 12:00:15 and should not be cleared
  EXPECT_NE(nullptr, cache.Lookup(scheme_host_port, HttpAuth::AUTH_SERVER,
                                  kRealm5, HttpAuth::AUTH_SCHEME_BASIC,
                                  NetworkAnonymizationKey()));

  // Creation time is set for a whole entry rather than for a particular path.
  // Path added within the requested duration isn't be removed.
  EXPECT_NE(nullptr, cache.LookupByPath(scheme_host_port, HttpAuth::AUTH_SERVER,
                                        NetworkAnonymizationKey(), "/baz/"));

  // Realms 3 and 4 are between 12:00:05 and 12:00:10 and should be cleared.
  EXPECT_EQ(nullptr, cache.Lookup(scheme_host_port, HttpAuth::AUTH_SERVER,
                                  kRealm3, HttpAuth::AUTH_SCHEME_BASIC,
                                  NetworkAnonymizationKey()));
  EXPECT_EQ(nullptr, cache.Lookup(scheme_host_port, HttpAuth::AUTH_SERVER,
                                  kRealm4, HttpAuth::AUTH_SCHEME_BASIC,
                                  NetworkAnonymizationKey()));

  cache.ClearEntriesAddedBetween(start_time - base::Seconds(1),
                                 base::Time::Max(),
                                 base::RepeatingCallback<bool(const GURL&)>());
  EXPECT_EQ(nullptr, cache.Lookup(scheme_host_port, HttpAuth::AUTH_SERVER,
                                  kRealm1, HttpAuth::AUTH_SCHEME_BASIC,
                                  NetworkAnonymizationKey()));
  EXPECT_EQ(nullptr, cache.Lookup(scheme_host_port, HttpAuth::AUTH_SERVER,
                                  kRealm2, HttpAuth::AUTH_SCHEME_BASIC,
                                  NetworkAnonymizationKey()));
  EXPECT_EQ(nullptr, cache.LookupByPath(scheme_host_port, HttpAuth::AUTH_SERVER,
                                        NetworkAnonymizationKey(), "/baz/"));
}

TEST(HttpAuthCacheTest, ClearEntriesAddedBetweenByFilter) {
  url::SchemeHostPort scheme_host_port_1(GURL("http://foobar.com"));
  url::SchemeHostPort scheme_host_port_2(GURL("http://foobar2.com"));

  base::SimpleTestClock test_clock;
  test_clock.SetNow(base::Time::Now());

  HttpAuthCache cache(false /* key_entries_by_network_anonymization_key */);
  cache.set_clock_for_testing(&test_clock);

  cache.Add(scheme_host_port_1, HttpAuth::AUTH_SERVER, kRealm1,
            HttpAuth::AUTH_SCHEME_BASIC, NetworkAnonymizationKey(),
            "basic realm=Realm1", AuthCredentials(kAlice, k123), "/");
  cache.Add(scheme_host_port_2, HttpAuth::AUTH_SERVER, kRealm1,
            HttpAuth::AUTH_SCHEME_BASIC, NetworkAnonymizationKey(),
            "basic realm=Realm1", AuthCredentials(kRoot, kWileCoyote), "/");

  cache.ClearEntriesAddedBetween(
      base::Time::Min(), base::Time::Max(),
      base::BindRepeating(&DoesUrlMatchFilter,
                          std::set<std::string>({scheme_host_port_1.host()})));

  // Only foobar.com should be cleared while foobar2.com remains.
  EXPECT_EQ(nullptr, cache.Lookup(scheme_host_port_1, HttpAuth::AUTH_SERVER,
                                  kRealm1, HttpAuth::AUTH_SCHEME_BASIC,
                                  NetworkAnonymizationKey()));
  EXPECT_NE(nullptr, cache.Lookup(scheme_host_port_2, HttpAuth::AUTH_SERVER,
                                  kRealm1, HttpAuth::AUTH_SCHEME_BASIC,
                                  NetworkAnonymizationKey()));
}

TEST(HttpAuthCacheTest, ClearEntriesAddedBetweenWithAllTimeValues) {
  url::SchemeHostPort scheme_host_port(GURL("http://foobar.com"));

  base::SimpleTestClock test_clock;
  test_clock.SetNow(base::Time::Now());

  HttpAuthCache cache(false /* key_entries_by_network_anonymization_key */);
  cache.set_clock_for_testing(&test_clock);

  cache.Add(scheme_host_port, HttpAuth::AUTH_SERVER, kRealm1,
            HttpAuth::AUTH_SCHEME_BASIC, NetworkAnonymizationKey(),
            "basic realm=Realm1", AuthCredentials(kAlice, k123), "/");
  cache.Add(scheme_host_port, HttpAuth::AUTH_SERVER, kRealm2,
            HttpAuth::AUTH_SCHEME_BASIC, NetworkAnonymizationKey(),
            "basic realm=Realm2", AuthCredentials(kRoot, kWileCoyote), "/");

  test_clock.Advance(base::Seconds(10));
  cache.Add(scheme_host_port, HttpAuth::AUTH_SERVER, kRealm3,
            HttpAuth::AUTH_SCHEME_BASIC, NetworkAnonymizationKey(),
            "basic realm=Realm3", AuthCredentials(kAlice2, k1234), "/");
  cache.Add(scheme_host_port, HttpAuth::AUTH_SERVER, kRealm4,
            HttpAuth::AUTH_SCHEME_BASIC, NetworkAnonymizationKey(),
            "basic realm=Realm4", AuthCredentials(kUsername, kPassword), "/");
  // Add path to existing entry.
  cache.Add(scheme_host_port, HttpAuth::AUTH_SERVER, kRealm2,
            HttpAuth::AUTH_SCHEME_BASIC, NetworkAnonymizationKey(),
            "basic realm=Realm2", AuthCredentials(kAdmin, kPassword), "/baz/");

  cache.ClearEntriesAddedBetween(base::Time::Min(), base::Time::Max(),
                                 base::RepeatingCallback<bool(const GURL&)>());

  // All entries should be cleared.
  EXPECT_EQ(nullptr, cache.Lookup(scheme_host_port, HttpAuth::AUTH_SERVER,
                                  kRealm1, HttpAuth::AUTH_SCHEME_BASIC,
                                  NetworkAnonymizationKey()));
  EXPECT_EQ(nullptr, cache.Lookup(scheme_host_port, HttpAuth::AUTH_SERVER,
                                  kRealm2, HttpAuth::AUTH_SCHEME_BASIC,
                                  NetworkAnonymizationKey()));
  EXPECT_EQ(nullptr, cache.LookupByPath(scheme_host_port, HttpAuth::AUTH_SERVER,
                                        NetworkAnonymizationKey(), "/baz/"));
  EXPECT_EQ(nullptr, cache.Lookup(scheme_host_port, HttpAuth::AUTH_SERVER,
                                  kRealm3, HttpAuth::AUTH_SCHEME_BASIC,
                                  NetworkAnonymizationKey()));
  EXPECT_EQ(nullptr, cache.Lookup(scheme_host_port, HttpAuth::AUTH_SERVER,
                                  kRealm4, HttpAuth::AUTH_SCHEME_BASIC,
                                  NetworkAnonymizationKey()));
}

TEST(HttpAuthCacheTest, ClearAllEntries) {
  url::SchemeHostPort scheme_host_port(GURL("http://foobar.com"));

  base::SimpleTestClock test_clock;
  test_clock.SetNow(base::Time::Now());

  HttpAuthCache cache(false /* key_entries_by_network_anonymization_key */);
  cache.set_clock_for_testing(&test_clock);

  cache.Add(scheme_host_port, HttpAuth::AUTH_SERVER, kRealm1,
            HttpAuth::AUTH_SCHEME_BASIC, NetworkAnonymizationKey(),
            "basic realm=Realm1", AuthCredentials(kAlice, k123), "/");
  cache.Add(scheme_host_port, HttpAuth::AUTH_SERVER, kRealm2,
            HttpAuth::AUTH_SCHEME_BASIC, NetworkAnonymizationKey(),
            "basic realm=Realm2", AuthCredentials(kRoot, kWileCoyote), "/");

  test_clock.Advance(base::Seconds(10));
  cache.Add(scheme_host_port, HttpAuth::AUTH_SERVER, kRealm3,
            HttpAuth::AUTH_SCHEME_BASIC, NetworkAnonymizationKey(),
            "basic realm=Realm3", AuthCredentials(kAlice2, k1234), "/");
  cache.Add(scheme_host_port, HttpAuth::AUTH_SERVER, kRealm4,
            HttpAuth::AUTH_SCHEME_BASIC, NetworkAnonymizationKey(),
            "basic realm=Realm4", AuthCredentials(kUsername, kPassword), "/");
  // Add path to existing entry.
  cache.Add(scheme_host_port, HttpAuth::AUTH_SERVER, kRealm2,
            HttpAuth::AUTH_SCHEME_BASIC, NetworkAnonymizationKey(),
            "basic realm=Realm2", AuthCredentials(kAdmin, kPassword), "/baz/");

  test_clock.Advance(base::Seconds(55));
  cache.ClearAllEntries();

  // All entries should be cleared.
  EXPECT_EQ(nullptr, cache.Lookup(scheme_host_port, HttpAuth::AUTH_SERVER,
                                  kRealm1, HttpAuth::AUTH_SCHEME_BASIC,
                                  NetworkAnonymizationKey()));
  EXPECT_EQ(nullptr, cache.Lookup(scheme_host_port, HttpAuth::AUTH_SERVER,
                                  kRealm2, HttpAuth::AUTH_SCHEME_BASIC,
                                  NetworkAnonymizationKey()));
  EXPECT_EQ(nullptr, cache.LookupByPath(scheme_host_port, HttpAuth::AUTH_SERVER,
                                        NetworkAnonymizationKey(), "/baz/"));
  EXPECT_EQ(nullptr, cache.Lookup(scheme_host_port, HttpAuth::AUTH_SERVER,
                                  kRealm3, HttpAuth::AUTH_SCHEME_BASIC,
                                  NetworkAnonymizationKey()));
  EXPECT_EQ(nullptr, cache.Lookup(scheme_host_port, HttpAuth::AUTH_SERVER,
                                  kRealm4, HttpAuth::AUTH_SCHEME_BASIC,
                                  NetworkAnonymizationKey()));
}

TEST(HttpAuthCacheTest, UpdateStaleChallenge) {
  HttpAuthCache cache(false /* key_entries_by_network_anonymization_key */);
  url::SchemeHostPort scheme_host_port(GURL("http://foobar2.com"));
  HttpAuthCache::Entry* entry_pre = cache.Add(
      scheme_host_port, HttpAuth::AUTH_SERVER, kRealm1,
      HttpAuth::AUTH_SCHEME_DIGEST, NetworkAnonymizationKey(),
      "Digest realm=Realm1,"
      "nonce=\"s3MzvFhaBAA=4c520af5acd9d8d7ae26947529d18c8eae1e98f4\"",
      CreateASCIICredentials("realm-digest-user", "realm-digest-password"),
      "/baz/index.html");
  ASSERT_TRUE(entry_pre != nullptr);

  EXPECT_EQ(2, entry_pre->IncrementNonceCount());
  EXPECT_EQ(3, entry_pre->IncrementNonceCount());
  EXPECT_EQ(4, entry_pre->IncrementNonceCount());

  bool update_success = cache.UpdateStaleChallenge(
      scheme_host_port, HttpAuth::AUTH_SERVER, kRealm1,
      HttpAuth::AUTH_SCHEME_DIGEST, NetworkAnonymizationKey(),
      "Digest realm=Realm1,"
      "nonce=\"claGgoRXBAA=7583377687842fdb7b56ba0555d175baa0b800e3\","
      "stale=\"true\"");
  EXPECT_TRUE(update_success);

  // After the stale update, the entry should still exist in the cache and
  // the nonce count should be reset to 0.
  HttpAuthCache::Entry* entry_post =
      cache.Lookup(scheme_host_port, HttpAuth::AUTH_SERVER, kRealm1,
                   HttpAuth::AUTH_SCHEME_DIGEST, NetworkAnonymizationKey());
  ASSERT_TRUE(entry_post != nullptr);
  EXPECT_EQ(2, entry_post->IncrementNonceCount());

  // UpdateStaleChallenge will fail if an entry doesn't exist in the cache.
  bool update_failure = cache.UpdateStaleChallenge(
      scheme_host_port, HttpAuth::AUTH_SERVER, kRealm2,
      HttpAuth::AUTH_SCHEME_DIGEST, NetworkAnonymizationKey(),
      "Digest realm=Realm2,"
      "nonce=\"claGgoRXBAA=7583377687842fdb7b56ba0555d175baa0b800e3\","
      "stale=\"true\"");
  EXPECT_FALSE(update_failure);
}

TEST(HttpAuthCacheTest, CopyProxyEntriesFrom) {
  url::SchemeHostPort scheme_host_port(GURL("http://example.com"));
  std::string path("/some/path");
  std::string another_path("/another/path");

  HttpAuthCache first_cache(
      false /* key_entries_by_network_anonymization_key */);
  HttpAuthCache::Entry* entry;

  first_cache.Add(scheme_host_port, HttpAuth::AUTH_PROXY, kRealm1,
                  HttpAuth::AUTH_SCHEME_BASIC, NetworkAnonymizationKey(),
                  "basic realm=Realm1", AuthCredentials(kAlice, k123), path);
  first_cache.Add(scheme_host_port, HttpAuth::AUTH_PROXY, kRealm2,
                  HttpAuth::AUTH_SCHEME_BASIC, NetworkAnonymizationKey(),
                  "basic realm=Realm2", AuthCredentials(kAlice2, k1234), path);
  first_cache.Add(scheme_host_port, HttpAuth::AUTH_PROXY, kRealm3,
                  HttpAuth::AUTH_SCHEME_DIGEST, NetworkAnonymizationKey(),
                  "digest realm=Realm3", AuthCredentials(kRoot, kWileCoyote),
                  path);
  entry = first_cache.Add(scheme_host_port, HttpAuth::AUTH_PROXY, kRealm3,
                          HttpAuth::AUTH_SCHEME_DIGEST,
                          NetworkAnonymizationKey(), "digest realm=Realm3",
                          AuthCredentials(kRoot, kWileCoyote), another_path);

  EXPECT_EQ(2, entry->IncrementNonceCount());

  // Server entry, which should not be copied.
  first_cache.Add(scheme_host_port, HttpAuth::AUTH_SERVER, kRealm1,
                  HttpAuth::AUTH_SCHEME_BASIC, NetworkAnonymizationKey(),
                  "basic realm=Realm1", AuthCredentials(kAlice, k123), path);

  HttpAuthCache second_cache(
      false /* key_entries_by_network_anonymization_key */);
  // Will be overwritten by kRoot:kWileCoyote.
  second_cache.Add(scheme_host_port, HttpAuth::AUTH_PROXY, kRealm3,
                   HttpAuth::AUTH_SCHEME_DIGEST, NetworkAnonymizationKey(),
                   "digest realm=Realm3", AuthCredentials(kAlice2, k1234),
                   path);
  // Should be left intact.
  second_cache.Add(scheme_host_port, HttpAuth::AUTH_PROXY, kRealm4,
                   HttpAuth::AUTH_SCHEME_BASIC, NetworkAnonymizationKey(),
                   "basic realm=Realm4", AuthCredentials(kAdmin, kRoot), path);

  second_cache.CopyProxyEntriesFrom(first_cache);

  // Copied from first_cache.
  entry = second_cache.Lookup(scheme_host_port, HttpAuth::AUTH_PROXY, kRealm1,
                              HttpAuth::AUTH_SCHEME_BASIC,
                              NetworkAnonymizationKey());
  EXPECT_TRUE(nullptr != entry);
  EXPECT_EQ(kAlice, entry->credentials().username());
  EXPECT_EQ(k123, entry->credentials().password());

  // Copied from first_cache.
  entry = second_cache.Lookup(scheme_host_port, HttpAuth::AUTH_PROXY, kRealm2,
                              HttpAuth::AUTH_SCHEME_BASIC,
                              NetworkAnonymizationKey());
  EXPECT_TRUE(nullptr != entry);
  EXPECT_EQ(kAlice2, entry->credentials().username());
  EXPECT_EQ(k1234, entry->credentials().password());

  // Overwritten from first_cache.
  entry = second_cache.Lookup(scheme_host_port, HttpAuth::AUTH_PROXY, kRealm3,
                              HttpAuth::AUTH_SCHEME_DIGEST,
                              NetworkAnonymizationKey());
  EXPECT_TRUE(nullptr != entry);
  EXPECT_EQ(kRoot, entry->credentials().username());
  EXPECT_EQ(kWileCoyote, entry->credentials().password());
  // Nonce count should get copied.
  EXPECT_EQ(3, entry->IncrementNonceCount());

  // All paths should get copied.
  entry = second_cache.LookupByPath(scheme_host_port, HttpAuth::AUTH_PROXY,
                                    NetworkAnonymizationKey(), another_path);
  EXPECT_TRUE(nullptr != entry);
  EXPECT_EQ(kRoot, entry->credentials().username());
  EXPECT_EQ(kWileCoyote, entry->credentials().password());

  // Left intact in second_cache.
  entry = second_cache.Lookup(scheme_host_port, HttpAuth::AUTH_PROXY, kRealm4,
                              HttpAuth::AUTH_SCHEME_BASIC,
                              NetworkAnonymizationKey());
  EXPECT_TRUE(nullptr != entry);
  EXPECT_EQ(kAdmin, entry->credentials().username());
  EXPECT_EQ(kRoot, entry->credentials().password());

  // AUTH_SERVER entry should not have been copied from first_cache.
  EXPECT_TRUE(first_cache.Lookup(scheme_host_port, HttpAuth::AUTH_SERVER,
                                 kRealm1, HttpAuth::AUTH_SCHEME_BASIC,
                                 NetworkAnonymizationKey()));
  EXPECT_FALSE(second_cache.Lookup(scheme_host_port, HttpAuth::AUTH_SERVER,
                                   kRealm1, HttpAuth::AUTH_SCHEME_BASIC,
                                   NetworkAnonymizationKey()));
}

// Test fixture class for eviction tests (contains helpers for bulk
// insertion and existence testing).
class HttpAuthCacheEvictionTest : public testing::Test {
 protected:
  HttpAuthCacheEvictionTest()
      : scheme_host_port_(GURL("http://www.google.com")),
        cache_(false /* key_entries_by_network_anonymization_key */) {}

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
    cache_.Add(scheme_host_port_, HttpAuth::AUTH_SERVER, GenerateRealm(realm_i),
               HttpAuth::AUTH_SCHEME_BASIC, NetworkAnonymizationKey(),
               std::string(), AuthCredentials(kUsername, kPassword),
               GeneratePath(realm_i, path_i));
  }

  void CheckRealmExistence(int realm_i, bool exists) {
    const HttpAuthCache::Entry* entry = cache_.Lookup(
        scheme_host_port_, HttpAuth::AUTH_SERVER, GenerateRealm(realm_i),
        HttpAuth::AUTH_SCHEME_BASIC, NetworkAnonymizationKey());
    if (exists) {
      EXPECT_FALSE(entry == nullptr);
      EXPECT_EQ(GenerateRealm(realm_i), entry->realm());
    } else {
      EXPECT_TRUE(entry == nullptr);
    }
  }

  void CheckPathExistence(int realm_i, int path_i, bool exists) {
    const HttpAuthCache::Entry* entry = cache_.LookupByPath(
        scheme_host_port_, HttpAuth::AUTH_SERVER, NetworkAnonymizationKey(),
        GeneratePath(realm_i, path_i));
    if (exists) {
      EXPECT_FALSE(entry == nullptr);
      EXPECT_EQ(GenerateRealm(realm_i), entry->realm());
    } else {
      EXPECT_TRUE(entry == nullptr);
    }
  }

  url::SchemeHostPort scheme_host_port_;
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
    test_clock.Advance(base::Seconds(1));
  }

  for (int i = 0; i < kMaxRealms; ++i) {
    CheckRealmExistence(i, true);
    test_clock.Advance(base::Seconds(1));
  }

  for (int i = 0; i < 3; ++i) {
    AddRealm(i + kMaxRealms);
    test_clock.Advance(base::Seconds(1));
  }

  for (int i = 0; i < 3; ++i) {
    CheckRealmExistence(i, false);
    test_clock.Advance(base::Seconds(1));
  }

  for (int i = 0; i < kMaxRealms; ++i) {
    CheckRealmExistence(i + 3, true);
    test_clock.Advance(base::Seconds(1));
  }

  cache_.set_tick_clock_for_testing(nullptr);
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
