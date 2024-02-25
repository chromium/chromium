// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/isolation_info_mojom_traits.h"

#include <optional>
#include <vector>

#include "base/unguessable_token.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "net/base/isolation_info.h"
#include "net/cookies/site_for_cookies.h"
#include "services/network/public/mojom/isolation_info.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace mojo {

TEST(IsolationInfoMojomTraitsTest, SerializeAndDeserialize) {
  const url::Origin kOrigin1 = url::Origin::Create(GURL("https://a.test/"));
  const url::Origin kOrigin2 = url::Origin::Create(GURL("https://b.test/"));

  const base::UnguessableToken nonce = base::UnguessableToken::Create();

  std::vector<net::IsolationInfo> keys = {
      net::IsolationInfo(),
      net::IsolationInfo::CreateTransient(),
      net::IsolationInfo::Create(net::IsolationInfo::RequestType::kMainFrame,
                                 kOrigin1, kOrigin1,
                                 net::SiteForCookies::FromOrigin(kOrigin1)),
      net::IsolationInfo::Create(net::IsolationInfo::RequestType::kSubFrame,
                                 kOrigin1, kOrigin2,
                                 net::SiteForCookies::FromOrigin(kOrigin1)),
      net::IsolationInfo::Create(net::IsolationInfo::RequestType::kSubFrame,
                                 kOrigin1, kOrigin2, net::SiteForCookies()),
      net::IsolationInfo::Create(net::IsolationInfo::RequestType::kOther,
                                 kOrigin1, kOrigin1,
                                 net::SiteForCookies::FromOrigin(kOrigin1)),
      net::IsolationInfo::Create(net::IsolationInfo::RequestType::kOther,
                                 url::Origin(), url::Origin(),
                                 net::SiteForCookies()),
      net::IsolationInfo::Create(net::IsolationInfo::RequestType::kOther,
                                 url::Origin(), url::Origin(),
                                 net::SiteForCookies()),
      net::IsolationInfo::Create(net::IsolationInfo::RequestType::kMainFrame,
                                 kOrigin1, kOrigin1,
                                 net::SiteForCookies::FromOrigin(kOrigin1)),
      net::IsolationInfo::Create(net::IsolationInfo::RequestType::kSubFrame,
                                 kOrigin1, kOrigin2,
                                 net::SiteForCookies::FromOrigin(kOrigin1)),
      net::IsolationInfo::Create(net::IsolationInfo::RequestType::kSubFrame,
                                 kOrigin1, kOrigin2, net::SiteForCookies()),
      net::IsolationInfo::Create(net::IsolationInfo::RequestType::kOther,
                                 kOrigin1, kOrigin1,
                                 net::SiteForCookies::FromOrigin(kOrigin1)),
      net::IsolationInfo::Create(net::IsolationInfo::RequestType::kOther,
                                 url::Origin(), url::Origin(),
                                 net::SiteForCookies(), nonce),
      net::IsolationInfo::Create(net::IsolationInfo::RequestType::kOther,
                                 url::Origin(), url::Origin(),
                                 net::SiteForCookies(), nonce),
  };

  for (auto original : keys) {
    net::IsolationInfo copied;
    EXPECT_TRUE(
        mojo::test::SerializeAndDeserialize<network::mojom::IsolationInfo>(
            original, copied));
    EXPECT_TRUE(original.IsEqualForTesting(copied));
  }
}

}  // namespace mojo
