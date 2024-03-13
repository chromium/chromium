// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/user_agent/user_agent_metadata.h"

#include <optional>

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
  to_encode.brand_full_version_list.emplace_back("a", "3.14");
  to_encode.brand_full_version_list.emplace_back("b", "5.03");
  to_encode.full_version = "3.14";
  to_encode.platform = "TR-DOS";
  to_encode.platform_version = "5.03";
  to_encode.architecture = "Z80";
  to_encode.model = "unofficial";
  to_encode.mobile = false;
  to_encode.bitness = "8";
  to_encode.wow64 = true;
  to_encode.form_factors = {"tubular"};
  return to_encode;
}

}  // namespace

TEST(UserAgentMetaDataTest, Boundary) {
  EXPECT_EQ(std::nullopt, UserAgentMetadata::Marshal(std::nullopt));
  EXPECT_EQ(std::nullopt, UserAgentMetadata::Demarshal(std::nullopt));
  EXPECT_EQ(std::nullopt,
            UserAgentMetadata::Demarshal(std::string("nonsense")));
}

TEST(UserAgentMetaDataTest, Basic) {
  blink::UserAgentMetadata to_encode = MakeToEncode();
  EXPECT_EQ(to_encode, UserAgentMetadata::Demarshal(
                           UserAgentMetadata::Marshal(to_encode)));
}

TEST(UserAgentMetaDataTest, Mobile) {
  blink::UserAgentMetadata to_encode = MakeToEncode();
  to_encode.mobile = true;
  EXPECT_EQ(to_encode, UserAgentMetadata::Demarshal(
                           UserAgentMetadata::Marshal(to_encode)));
}

TEST(UserAgentMetaDataTest, EmptyFormFactors) {
  blink::UserAgentMetadata to_encode = MakeToEncode();
  to_encode.form_factors = {};
  EXPECT_EQ(to_encode, UserAgentMetadata::Demarshal(
                           UserAgentMetadata::Marshal(to_encode)));
}

TEST(UserAgentMetaDataTest, MultiFormFactors) {
  blink::UserAgentMetadata to_encode = MakeToEncode();
  to_encode.form_factors = {"a", "b"};
  EXPECT_EQ(to_encode, UserAgentMetadata::Demarshal(
                           UserAgentMetadata::Marshal(to_encode)));
}

TEST(UserAgentMetaDataTest, SerializeFormFactors) {
  UserAgentMetadata uam;

  uam.form_factors = {};
  ASSERT_EQ(uam.SerializeFormFactors(), "") << "empty";

  uam.form_factors = {"Desktop"};
  ASSERT_EQ(uam.SerializeFormFactors(), "\"Desktop\"") << "empty";

  uam.form_factors = {"Desktop", "Tablet"};
  ASSERT_EQ(uam.SerializeFormFactors(), "\"Desktop\", \"Tablet\"") << "empty";
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
