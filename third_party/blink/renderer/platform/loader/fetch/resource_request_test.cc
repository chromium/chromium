// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/loader/fetch/resource_request.h"

#include <memory>
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/renderer/platform/network/encoded_form_data.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/weborigin/referrer.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

namespace blink {

TEST(ResourceRequestTest, SetHasUserGesture) {
  ResourceRequest original;
  EXPECT_FALSE(original.HasUserGesture());
  original.SetHasUserGesture(true);
  EXPECT_TRUE(original.HasUserGesture());
  original.SetHasUserGesture(false);
  EXPECT_TRUE(original.HasUserGesture());
}

TEST(ResourceRequestTest, SetIsAdResource) {
  ResourceRequest original;
  EXPECT_FALSE(original.IsAdResource());
  original.SetIsAdResource();
  EXPECT_TRUE(original.IsAdResource());

  // Should persist across redirects.
  std::unique_ptr<ResourceRequest> redirect_request =
      original.CreateRedirectRequest(
          KURL("https://example.test/redirect"), original.HttpMethod(),
          original.SiteForCookies(), original.HttpReferrer(),
          original.GetReferrerPolicy(), original.GetSkipServiceWorker());
  EXPECT_TRUE(redirect_request->IsAdResource());
}

TEST(ResourceRequestTest, UpgradeIfInsecureAcrossRedirects) {
  ResourceRequest original;
  EXPECT_FALSE(original.UpgradeIfInsecure());
  original.SetUpgradeIfInsecure(true);
  EXPECT_TRUE(original.UpgradeIfInsecure());

  // Should persist across redirects.
  std::unique_ptr<ResourceRequest> redirect_request =
      original.CreateRedirectRequest(
          KURL("https://example.test/redirect"), original.HttpMethod(),
          original.SiteForCookies(), original.HttpReferrer(),
          original.GetReferrerPolicy(), original.GetSkipServiceWorker());
  EXPECT_TRUE(redirect_request->UpgradeIfInsecure());
}

}  // namespace blink
