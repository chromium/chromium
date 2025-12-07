// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/webid/federated_auth_request_mojom_traits.h"

#include <optional>

#include "mojo/public/cpp/test_support/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/webid/login_status_account.h"
#include "third_party/blink/public/common/webid/login_status_options.h"
#include "third_party/blink/public/mojom/webid/federated_auth_request.mojom.h"

namespace blink {

using blink::common::webid::LoginStatusAccount;
using blink::common::webid::LoginStatusOptions;

namespace {

void TestAccountRoundTrip(const LoginStatusAccount& in) {
  LoginStatusAccount result;
  ASSERT_TRUE(mojo::test::SerializeAndDeserialize<mojom::LoginStatusAccount>(
      in, result));
  EXPECT_EQ(in, result);
}

void TestOptionsRoundTrip(const LoginStatusOptions& in) {
  LoginStatusOptions result;
  ASSERT_TRUE(mojo::test::SerializeAndDeserialize<mojom::LoginStatusOptions>(
      in, result));
  EXPECT_EQ(in, result);
}

TEST(LoginStatusOptionsMojomTraitsTest, AccountMinimalRoundTrip) {
  TestAccountRoundTrip(LoginStatusAccount("identifier", "email@example.com",
                                          "Alice Smith",
                                          /*given_name=*/std::nullopt,
                                          /*picture_url=*/std::nullopt));
}

TEST(LoginStatusOptionsMojomTraitsTest, AccountFullRoundTrip) {
  GURL picture_url("https://idp.example/user.png");
  std::string given_name = "Alice";
  TestAccountRoundTrip(LoginStatusAccount("identifier", "email@example.com",
                                          "Alice Smith", given_name,
                                          picture_url));
}

TEST(LoginStatusOptionsMojomTraitsTest, AccountInvalidUrlFails) {
  GURL invalid_url("https:://google.com");
  std::string given_name = "Alice";
  LoginStatusAccount input("identifier", "email@example.com", "Alice Smith",
                           given_name, invalid_url);

  LoginStatusAccount result;

  ASSERT_FALSE(mojo::test::SerializeAndDeserialize<mojom::LoginStatusAccount>(
      input, result));
}

TEST(LoginStatusOptionsMojomTraitsTest, AccountUntrustworthyUrlFails) {
  GURL untrustworthy_url("http://idp.example/user.png");
  std::string given_name = "Alice";
  LoginStatusAccount input("identifier", "email@example.com", "Alice Smith",
                           given_name, untrustworthy_url);
  LoginStatusAccount result;
  ASSERT_FALSE(mojo::test::SerializeAndDeserialize<mojom::LoginStatusAccount>(
      input, result));
}

TEST(LoginStatusOptionsMojomTraitsTest, OptionsRoundTrip) {
  GURL url("https://idp.example/user.png");
  std::string given_name = "Alice";
  auto account_list = std::vector<LoginStatusAccount>();
  account_list.emplace_back("identifier", "email@example.com", "Alice Smith",
                            &given_name, &url);
  TestOptionsRoundTrip(
      LoginStatusOptions(account_list, std::make_optional(base::Seconds(120))));
}

}  // namespace

}  // namespace blink
