// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/shared_dictionary/shared_dictionary_storage_isolation_key.h"

#include "net/base/isolation_info.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace network {

namespace {
const GURL kUrl1("https://origin1.test/");
const net::SchemefulSite kSite1(GURL("https://origin1.test/"));
const net::SchemefulSite kSite2(GURL("https://origin2.test/"));
}  // namespace

TEST(SharedDictionaryStorageIsolationKeyTest, MaybeCreate) {
  url::Origin origin = url::Origin::Create(kUrl1);
  const absl::optional<SharedDictionaryStorageIsolationKey> isolation_key =
      SharedDictionaryStorageIsolationKey::MaybeCreate(
          net::IsolationInfo::Create(net::IsolationInfo::RequestType::kOther,
                                     origin, origin, net::SiteForCookies()));
  EXPECT_TRUE(isolation_key);
}

TEST(SharedDictionaryStorageIsolationKeyTest, MaybeCreateOpaqueTopFrameOrigin) {
  const absl::optional<SharedDictionaryStorageIsolationKey> isolation_key =
      SharedDictionaryStorageIsolationKey::MaybeCreate(
          net::IsolationInfo::Create(net::IsolationInfo::RequestType::kOther,
                                     url::Origin(), url::Origin::Create(kUrl1),
                                     net::SiteForCookies()));
  EXPECT_FALSE(isolation_key);
}

TEST(SharedDictionaryStorageIsolationKeyTest, MaybeCreateOpaqueFrameOrigin) {
  url::Origin origin = url::Origin::Create(kUrl1);
  const absl::optional<SharedDictionaryStorageIsolationKey> isolation_key =
      SharedDictionaryStorageIsolationKey::MaybeCreate(
          net::IsolationInfo::Create(net::IsolationInfo::RequestType::kOther,
                                     origin, url::Origin(),
                                     net::SiteForCookies()));
  EXPECT_FALSE(isolation_key);
}

TEST(SharedDictionaryStorageIsolationKeyTest, MaybeCreateWithNonce) {
  const absl::optional<SharedDictionaryStorageIsolationKey> isolation_key =
      SharedDictionaryStorageIsolationKey::MaybeCreate(
          net::IsolationInfo::Create(
              net::IsolationInfo::RequestType::kOther,
              url::Origin::Create(kUrl1), url::Origin(), net::SiteForCookies(),
              /*party_context=*/absl::nullopt,
              /*nonce=*/base::UnguessableToken::Create()));
  EXPECT_FALSE(isolation_key);
}

TEST(SharedDictionaryStorageIsolationKeyTest, SameFrameOriginSameTopFrameSite) {
  SharedDictionaryStorageIsolationKey isolation_key1(url::Origin::Create(kUrl1),
                                                     kSite1);
  SharedDictionaryStorageIsolationKey isolation_key2(url::Origin::Create(kUrl1),
                                                     kSite1);
  EXPECT_EQ(isolation_key1, isolation_key2);
}

TEST(SharedDictionaryStorageIsolationKeyTest,
     DifferentFrameOriginSameTopFrameSite) {
  SharedDictionaryStorageIsolationKey isolation_key1(
      url::Origin::Create(GURL("https://www1.origin1.test/")), kSite1);
  SharedDictionaryStorageIsolationKey isolation_key2(
      url::Origin::Create(GURL("https://www2.origin1.test/")), kSite1);
  EXPECT_NE(isolation_key1, isolation_key2);
}

TEST(SharedDictionaryStorageIsolationKeyTest,
     SameFrameOriginDifferentTopFrameSite) {
  SharedDictionaryStorageIsolationKey isolation_key1(url::Origin::Create(kUrl1),
                                                     kSite1);
  SharedDictionaryStorageIsolationKey isolation_key2(url::Origin::Create(kUrl1),
                                                     kSite2);
  EXPECT_NE(isolation_key1, isolation_key2);
}

}  // namespace network
