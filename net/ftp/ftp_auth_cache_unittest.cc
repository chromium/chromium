// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/ftp/ftp_auth_cache.h"

#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "net/base/auth.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using base::ASCIIToUTF16;

namespace net {

namespace {

const base::string16 kBogus(ASCIIToUTF16("bogus"));
const base::string16 kOthername(ASCIIToUTF16("othername"));
const base::string16 kOtherword(ASCIIToUTF16("otherword"));
const base::string16 kPassword(ASCIIToUTF16("password"));
const base::string16 kPassword1(ASCIIToUTF16("password1"));
const base::string16 kPassword2(ASCIIToUTF16("password2"));
const base::string16 kPassword3(ASCIIToUTF16("password3"));
const base::string16 kUsername(ASCIIToUTF16("username"));
const base::string16 kUsername1(ASCIIToUTF16("username1"));
const base::string16 kUsername2(ASCIIToUTF16("username2"));
const base::string16 kUsername3(ASCIIToUTF16("username3"));

}  // namespace

TEST(FtpAuthCacheTest, LookupAddRemove) {
  FtpAuthCache cache;

  GURL origin1("ftp://foo1");
  GURL origin2("ftp://foo2");

  // Lookup non-existent entry.
  EXPECT_TRUE(cache.Lookup(origin1) == nullptr);

  // Add entry for origin1.
  cache.Add(origin1, AuthCredentials(kUsername1, kPassword1));
  FtpAuthCache::Entry* entry1 = cache.Lookup(origin1);
  ASSERT_TRUE(entry1);
  EXPECT_EQ(origin1, entry1->origin);
  EXPECT_EQ(kUsername1, entry1->credentials.username());
  EXPECT_EQ(kPassword1, entry1->credentials.password());

  // Add an entry for origin2.
  cache.Add(origin2, AuthCredentials(kUsername2, kPassword2));
  FtpAuthCache::Entry* entry2 = cache.Lookup(origin2);
  ASSERT_TRUE(entry2);
  EXPECT_EQ(origin2, entry2->origin);
  EXPECT_EQ(kUsername2, entry2->credentials.username());
  EXPECT_EQ(kPassword2, entry2->credentials.password());

  // The original entry1 should still be there.
  EXPECT_EQ(entry1, cache.Lookup(origin1));

  // Overwrite the entry for origin1.
  cache.Add(origin1, AuthCredentials(kUsername3, kPassword3));
  FtpAuthCache::Entry* entry3 = cache.Lookup(origin1);
  ASSERT_TRUE(entry3);
  EXPECT_EQ(origin1, entry3->origin);
  EXPECT_EQ(kUsername3, entry3->credentials.username());
  EXPECT_EQ(kPassword3, entry3->credentials.password());

  // Remove entry of origin1.
  cache.Remove(origin1, AuthCredentials(kUsername3, kPassword3));
  EXPECT_TRUE(cache.Lookup(origin1) == nullptr);

  // Remove non-existent entry.
  cache.Remove(origin1, AuthCredentials(kUsername3, kPassword3));
  EXPECT_TRUE(cache.Lookup(origin1) == nullptr);
}

// Check that if the origin differs only by port number, it is considered
// a separate origin.
TEST(FtpAuthCacheTest, LookupWithPort) {
  FtpAuthCache cache;

  GURL origin1("ftp://foo:80");
  GURL origin2("ftp://foo:21");

  cache.Add(origin1, AuthCredentials(kUsername, kPassword));
  cache.Add(origin2, AuthCredentials(kUsername, kPassword));

  EXPECT_NE(cache.Lookup(origin1), cache.Lookup(origin2));
}

TEST(FtpAuthCacheTest, NormalizedKey) {
  // GURL is automatically canonicalized. Hence the following variations in
  // url format should all map to the same entry (case insensitive host,
  // default port of 21).

  FtpAuthCache cache;

  // Add.
  cache.Add(GURL("ftp://HoSt:21"), AuthCredentials(kUsername, kPassword));

  // Lookup.
  FtpAuthCache::Entry* entry1 = cache.Lookup(GURL("ftp://HoSt:21"));
  ASSERT_TRUE(entry1);
  EXPECT_EQ(entry1, cache.Lookup(GURL("ftp://host:21")));
  EXPECT_EQ(entry1, cache.Lookup(GURL("ftp://host")));

  // Overwrite.
  cache.Add(GURL("ftp://host"), AuthCredentials(kOthername, kOtherword));
  FtpAuthCache::Entry* entry2 = cache.Lookup(GURL("ftp://HoSt:21"));
  ASSERT_TRUE(entry2);
  EXPECT_EQ(GURL("ftp://host"), entry2->origin);
  EXPECT_EQ(kOthername, entry2->credentials.username());
  EXPECT_EQ(kOtherword, entry2->credentials.password());

  // Remove
  cache.Remove(GURL("ftp://HOsT"), AuthCredentials(kOthername, kOtherword));
  EXPECT_TRUE(cache.Lookup(GURL("ftp://host")) == nullptr);
}

TEST(FtpAuthCacheTest, OnlyRemoveMatching) {
  FtpAuthCache cache;

  cache.Add(GURL("ftp://host"), AuthCredentials(kUsername, kPassword));
  EXPECT_TRUE(cache.Lookup(GURL("ftp://host")));

  // Auth data doesn't match, shouldn't remove.
  cache.Remove(GURL("ftp://host"), AuthCredentials(kBogus, kBogus));
  EXPECT_TRUE(cache.Lookup(GURL("ftp://host")));

  // Auth data matches, should remove.
  cache.Remove(GURL("ftp://host"), AuthCredentials(kUsername, kPassword));
  EXPECT_TRUE(cache.Lookup(GURL("ftp://host")) == nullptr);
}

TEST(FtpAuthCacheTest, EvictOldEntries) {
  FtpAuthCache cache;

  for (size_t i = 0; i < FtpAuthCache::kMaxEntries; i++) {
    cache.Add(GURL("ftp://host" + base::NumberToString(i)),
              AuthCredentials(kUsername, kPassword));
  }

  // No entries should be evicted before reaching the limit.
  for (size_t i = 0; i < FtpAuthCache::kMaxEntries; i++) {
    EXPECT_TRUE(cache.Lookup(GURL("ftp://host" + base::NumberToString(i))));
  }

  // Adding one entry should cause eviction of the first entry.
  cache.Add(GURL("ftp://last_host"), AuthCredentials(kUsername, kPassword));
  EXPECT_TRUE(cache.Lookup(GURL("ftp://host0")) == nullptr);

  // Remaining entries should not get evicted.
  for (size_t i = 1; i < FtpAuthCache::kMaxEntries; i++) {
    EXPECT_TRUE(cache.Lookup(GURL("ftp://host" + base::NumberToString(i))));
  }
  EXPECT_TRUE(cache.Lookup(GURL("ftp://last_host")));
}

}  // namespace net
