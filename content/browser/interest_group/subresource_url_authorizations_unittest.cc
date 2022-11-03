// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/interest_group/subresource_url_authorizations.h"

#include <tuple>

#include "content/browser/interest_group/auction_worklet_manager.h"
#include "content/browser/interest_group/subresource_url_builder.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/interest_group/auction_config.h"
#include "url/gurl.h"

namespace content {

namespace {

using WorkletHandle = AuctionWorkletManager::WorkletHandle;
using BundleSubresourceInfo = SubresourceUrlBuilder::BundleSubresourceInfo;

// We don't need to actually make real WorkletHandles --
// SubresourceUrlAuthorizations only cares about the addresses of
// WorkletHandles, so we can use hard-coded pointers.
const WorkletHandle* const kWorkletHandle1 =
    reinterpret_cast<const WorkletHandle*>(0xABC123);

const WorkletHandle* const kWorkletHandle2 =
    reinterpret_cast<const WorkletHandle*>(0xDEF456);

constexpr char kBundleUrl[] = "https://seller.test/bundle";

constexpr char kSubresourceUrl1[] = "https://seller.test/signals1";
constexpr char kSubresourceUrl2[] = "https://seller.test/signals2";

// NOTE: MakeBlinkSubresource() does *not* yield the same results each
// time it is called, since it generates a random token in the
// DirectFromSellerSignalsSubresource constructor.

blink::DirectFromSellerSignalsSubresource MakeBlinkSubresource() {
  blink::DirectFromSellerSignalsSubresource subresource;
  subresource.bundle_url = GURL(kBundleUrl);
  return subresource;
}

}  // namespace

class SubresourceUrlAuthorizationsTest : public ::testing::Test {
 protected:
  void AuthorizeSubresourceUrls(
      const AuctionWorkletManager::WorkletHandle* worklet_handle,
      const std::vector<SubresourceUrlBuilder::BundleSubresourceInfo>&
          authorized_subresource_urls,
      SubresourceUrlAuthorizations& authorizations) {
    authorizations.AuthorizeSubresourceUrls(worklet_handle,
                                            authorized_subresource_urls);
  }

  void OnWorkletHandleDestruction(
      const AuctionWorkletManager::WorkletHandle* worklet_handle,
      SubresourceUrlAuthorizations& authorizations) {
    authorizations.OnWorkletHandleDestruction(worklet_handle);
  }
};

TEST_F(SubresourceUrlAuthorizationsTest, Construct) {
  SubresourceUrlAuthorizations authorizations;
}

TEST_F(SubresourceUrlAuthorizationsTest, InitiallyNotAuthorized) {
  SubresourceUrlAuthorizations authorizations;

  GURL subresource_url1(kSubresourceUrl1);
  GURL subresource_url2(kSubresourceUrl2);

  EXPECT_EQ(nullptr, authorizations.GetAuthorizationInfo(subresource_url1));
  EXPECT_EQ(nullptr, authorizations.GetAuthorizationInfo(subresource_url2));
}

TEST_F(SubresourceUrlAuthorizationsTest, Simple) {
  SubresourceUrlAuthorizations authorizations;

  GURL subresource_url1(kSubresourceUrl1);
  GURL subresource_url2(kSubresourceUrl2);

  const BundleSubresourceInfo full_info1(subresource_url1,
                                         MakeBlinkSubresource());
  const BundleSubresourceInfo full_info2(subresource_url2,
                                         MakeBlinkSubresource());

  AuthorizeSubresourceUrls(kWorkletHandle1, {full_info1, full_info2},
                           authorizations);

  EXPECT_EQ(full_info1, *authorizations.GetAuthorizationInfo(subresource_url1));
  EXPECT_EQ(full_info2, *authorizations.GetAuthorizationInfo(subresource_url2));

  OnWorkletHandleDestruction(kWorkletHandle1, authorizations);

  EXPECT_EQ(nullptr, authorizations.GetAuthorizationInfo(subresource_url1));
  EXPECT_EQ(nullptr, authorizations.GetAuthorizationInfo(subresource_url2));
}

TEST_F(SubresourceUrlAuthorizationsTest, NotAuthorized) {
  SubresourceUrlAuthorizations authorizations;

  GURL subresource_url1(kSubresourceUrl1);
  GURL subresource_url2(kSubresourceUrl2);

  const BundleSubresourceInfo full_info1(subresource_url1,
                                         MakeBlinkSubresource());

  AuthorizeSubresourceUrls(kWorkletHandle1, {full_info1}, authorizations);

  EXPECT_EQ(nullptr, authorizations.GetAuthorizationInfo(subresource_url2));

  OnWorkletHandleDestruction(kWorkletHandle1, authorizations);

  EXPECT_EQ(nullptr, authorizations.GetAuthorizationInfo(subresource_url1));
  EXPECT_EQ(nullptr, authorizations.GetAuthorizationInfo(subresource_url2));
}

TEST_F(SubresourceUrlAuthorizationsTest, MultipleHandles) {
  SubresourceUrlAuthorizations authorizations;

  GURL subresource_url1(kSubresourceUrl1);
  GURL subresource_url2(kSubresourceUrl2);

  const BundleSubresourceInfo full_info1(subresource_url1,
                                         MakeBlinkSubresource());
  const BundleSubresourceInfo full_info2(subresource_url2,
                                         MakeBlinkSubresource());

  // `full_info1` is authorized by both `kWorkletHandle1` and `kWorkletHandle2`.
  // `full_info1` should only de-authorize when both `kWorkletHandle1` and
  // `kWorkletHandle2` are both destroyed.
  AuthorizeSubresourceUrls(kWorkletHandle1, {full_info1, full_info2},
                           authorizations);
  AuthorizeSubresourceUrls(kWorkletHandle2, {full_info1}, authorizations);

  EXPECT_EQ(full_info1, *authorizations.GetAuthorizationInfo(subresource_url1));
  EXPECT_EQ(full_info2, *authorizations.GetAuthorizationInfo(subresource_url2));

  OnWorkletHandleDestruction(kWorkletHandle1, authorizations);

  EXPECT_EQ(full_info1, *authorizations.GetAuthorizationInfo(subresource_url1));
  EXPECT_EQ(nullptr, authorizations.GetAuthorizationInfo(subresource_url2));

  OnWorkletHandleDestruction(kWorkletHandle2, authorizations);

  EXPECT_EQ(nullptr, authorizations.GetAuthorizationInfo(subresource_url1));
  EXPECT_EQ(nullptr, authorizations.GetAuthorizationInfo(subresource_url2));
}

TEST_F(SubresourceUrlAuthorizationsTest, BadScriptAltersSubresource) {
  SubresourceUrlAuthorizations authorizations;

  GURL subresource_url1(kSubresourceUrl1);

  const BundleSubresourceInfo full_info1(subresource_url1,
                                         MakeBlinkSubresource());
  // Try to authorize with a different token, but the same `subresource_url1`
  // subresource URL. This mimics the behavior of a misbehaving script that
  // alters the <script type="webbundle"> definition of a subresource while an
  // auction is still running.
  const BundleSubresourceInfo full_info2(subresource_url1,
                                         MakeBlinkSubresource());

  // When kWorkletHandle2 is authorized, `subresource_url1` will still map to
  // the full info from `full_info1`, and no crash should occur.
  AuthorizeSubresourceUrls(kWorkletHandle1, {full_info1}, authorizations);
  AuthorizeSubresourceUrls(kWorkletHandle2, {full_info2}, authorizations);

  EXPECT_EQ(full_info1, *authorizations.GetAuthorizationInfo(subresource_url1));

  OnWorkletHandleDestruction(kWorkletHandle1, authorizations);

  EXPECT_EQ(full_info1, *authorizations.GetAuthorizationInfo(subresource_url1));

  OnWorkletHandleDestruction(kWorkletHandle2, authorizations);

  EXPECT_EQ(nullptr, authorizations.GetAuthorizationInfo(subresource_url1));
}

}  // namespace content
