// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/user_agent/user_agent_metadata.h"

#include "base/optional.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/user_agent/user_agent_mojom_traits.h"
#include "third_party/blink/public/mojom/user_agent/user_agent_metadata.mojom.h"

namespace blink {

namespace {

blink::UserAgentMetadata MakeToEncode() {
  blink::UserAgentMetadata to_encode;
  to_encode.brand_version_list.emplace_back("a", "3");
  to_encode.brand_version_list.emplace_back("b", "5");
  to_encode.full_version = "3.14";
  to_encode.platform = "TR-DOS";
  to_encode.platform_version = "5.03";
  to_encode.architecture = "Z80";
  to_encode.model = "unofficial";
  to_encode.mobile = false;
  return to_encode;
}

}  // namespace

TEST(UserAgentMetaDataTest, Boundary) {
  EXPECT_EQ(base::nullopt, UserAgentMetadata::Marshal(base::nullopt));
  EXPECT_EQ(base::nullopt, UserAgentMetadata::Demarshal(base::nullopt));
  EXPECT_EQ(base::nullopt,
            UserAgentMetadata::Demarshal(std::string("nonsense")));
}

TEST(UserAgentMetaDataTest, Basic) {
  blink::UserAgentMetadata to_encode = MakeToEncode();
  EXPECT_EQ(to_encode, UserAgentMetadata::Demarshal(
                           UserAgentMetadata::Marshal(to_encode)));

  to_encode.mobile = true;
  EXPECT_EQ(to_encode, UserAgentMetadata::Demarshal(
                           UserAgentMetadata::Marshal(to_encode)));
}

TEST(UserAgentMetaDataTest, MojoTraits) {
  blink::UserAgentMetadata to_encode = MakeToEncode();
  blink::UserAgentMetadata copied;
  mojo::test::SerializeAndDeserialize<mojom::UserAgentMetadata>(to_encode,
                                                                copied);
  EXPECT_EQ(to_encode, copied);

  to_encode.mobile = true;
  mojo::test::SerializeAndDeserialize<mojom::UserAgentMetadata>(to_encode,
                                                                copied);
  EXPECT_EQ(to_encode, copied);
}

}  // namespace blink
