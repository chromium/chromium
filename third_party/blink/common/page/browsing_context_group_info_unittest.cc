// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/page/browsing_context_group_info.h"

#include "base/unguessable_token.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/page/browsing_context_group_info_mojom_traits.h"
#include "third_party/blink/public/mojom/page/browsing_context_group_info.mojom.h"

namespace blink {

TEST(BrowsingContextGroupInfoTest, Create) {
  base::UnguessableToken browsing_context_group_token =
      base::UnguessableToken::Create();
  base::UnguessableToken coop_related_group_token =
      base::UnguessableToken::Create();
  BrowsingContextGroupInfo bcg_info(browsing_context_group_token,
                                    coop_related_group_token);

  EXPECT_FALSE(bcg_info.browsing_context_group_token.is_empty());
  EXPECT_FALSE(bcg_info.coop_related_group_token.is_empty());

  EXPECT_EQ(bcg_info.browsing_context_group_token,
            browsing_context_group_token);
  EXPECT_EQ(bcg_info.coop_related_group_token, coop_related_group_token);
}

TEST(BrowsingContextGroupInfoTest, CreateUnique) {
  BrowsingContextGroupInfo bcg_info = BrowsingContextGroupInfo::CreateUnique();

  EXPECT_FALSE(bcg_info.browsing_context_group_token.is_empty());
  EXPECT_FALSE(bcg_info.coop_related_group_token.is_empty());
  EXPECT_NE(bcg_info.coop_related_group_token,
            bcg_info.browsing_context_group_token);
}

TEST(BrowsingContextGroupInfoTest, ComparisonOperator) {
  // Check that two different BrowsingContextGroupInfo are not equal.
  BrowsingContextGroupInfo bcg_info = BrowsingContextGroupInfo::CreateUnique();
  BrowsingContextGroupInfo other_bcg_info =
      BrowsingContextGroupInfo::CreateUnique();
  EXPECT_NE(bcg_info, other_bcg_info);

  // Check that two BrowsingContextGroupInfo copied from one another are equal.
  BrowsingContextGroupInfo bcg_info_clone(bcg_info);
  EXPECT_EQ(bcg_info, bcg_info_clone);

  // Verify that having different browsing_context_group_token is enough to have
  // the comparison fail.
  bcg_info_clone.browsing_context_group_token =
      base::UnguessableToken::Create();
  EXPECT_NE(bcg_info, bcg_info_clone);

  // Verify that having different coop_related_group_tokens is enough to have
  // the comparison fail.
  bcg_info_clone.browsing_context_group_token =
      bcg_info.browsing_context_group_token;
  bcg_info_clone.coop_related_group_token = base::UnguessableToken::Create();
  EXPECT_NE(bcg_info, bcg_info_clone);
}

}  // namespace blink
