// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/shared_dictionary/shared_dictionary_isolation_key.h"

#include "net/base/isolation_info.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace net {

namespace {
const GURL kUrl1("https://origin1.test/");
const SchemefulSite kSite1(GURL("https://origin1.test/"));
const SchemefulSite kSite2(GURL("https://origin2.test/"));
}  // namespace

TEST(SharedDictionaryIsolationKeyTest, MaybeCreate) {
  url::Origin origin = url::Origin::Create(kUrl1);
  const std::optional<SharedDictionaryIsolationKey> isolation_key =
      SharedDictionaryIsolationKey::MaybeCreate(
          IsolationInfo::Create(IsolationInfo::RequestType::kOther, origin,
                                origin, SiteForCookies()));
  EXPECT_TRUE(isolation_key);
}

TEST(SharedDictionaryIsolationKeyTest, MaybeCreateOpaqueTopFrameOrigin) {
  const std::optional<SharedDictionaryIsolationKey> isolation_key =
      SharedDictionaryIsolationKey::MaybeCreate(IsolationInfo::Create(
          IsolationInfo::RequestType::kOther, url::Origin(),
          url::Origin::Create(kUrl1), SiteForCookies()));
  EXPECT_FALSE(isolation_key);
}

TEST(SharedDictionaryIsolationKeyTest, MaybeCreateOpaqueFrameOrigin) {
  url::Origin origin = url::Origin::Create(kUrl1);
  const std::optional<SharedDictionaryIsolationKey> isolation_key =
      SharedDictionaryIsolationKey::MaybeCreate(
          IsolationInfo::Create(IsolationInfo::RequestType::kOther, origin,
                                url::Origin(), SiteForCookies()));
  EXPECT_FALSE(isolation_key);
}

TEST(SharedDictionaryIsolationKeyTest, MaybeCreateWithNonce) {
  const std::optional<SharedDictionaryIsolationKey> isolation_key =
      SharedDictionaryIsolationKey::MaybeCreate(IsolationInfo::Create(
          IsolationInfo::RequestType::kOther, url::Origin::Create(kUrl1),
          url::Origin(), SiteForCookies(),
          /*nonce=*/base::UnguessableToken::Create()));
  EXPECT_FALSE(isolation_key);
}

TEST(SharedDictionaryIsolationKeyTest, SameFrameOriginSameTopFrameSite) {
  SharedDictionaryIsolationKey isolation_key1(url::Origin::Create(kUrl1),
                                              kSite1);
  SharedDictionaryIsolationKey isolation_key2(url::Origin::Create(kUrl1),
                                              kSite1);
  EXPECT_EQ(isolation_key1, isolation_key2);
}

TEST(SharedDictionaryIsolationKeyTest, DifferentFrameOriginSameTopFrameSite) {
  SharedDictionaryIsolationKey isolation_key1(
      url::Origin::Create(GURL("https://www1.origin1.test/")), kSite1);
  SharedDictionaryIsolationKey isolation_key2(
      url::Origin::Create(GURL("https://www2.origin1.test/")), kSite1);
  EXPECT_NE(isolation_key1, isolation_key2);
}

TEST(SharedDictionaryIsolationKeyTest, SameFrameOriginDifferentTopFrameSite) {
  SharedDictionaryIsolationKey isolation_key1(url::Origin::Create(kUrl1),
                                              kSite1);
  SharedDictionaryIsolationKey isolation_key2(url::Origin::Create(kUrl1),
                                              kSite2);
  EXPECT_NE(isolation_key1, isolation_key2);
}

}  // namespace net
