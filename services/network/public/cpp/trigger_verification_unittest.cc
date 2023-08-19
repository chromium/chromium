// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/trigger_verification.h"

#include <string>

#include "base/uuid.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace network {
namespace {

TEST(TriggerVerificationTest, Create) {
  const struct {
    base::Uuid id;
    std::string token;
    bool expected_created;
  } kTestCases[] = {
      {
          base::Uuid::ParseLowercase("a2ab30b9-d664-4dfc-a9db-85f9729b9a30"),
          "token",
          true,
      },
      {
          // not a uuid
          base::Uuid::ParseLowercase("not-a-uuid"),
          "token",
          false,
      },
      {
          // not lowercased uuid
          base::Uuid::ParseLowercase("A2AB30B9-d664-4dFC-a9db-85f9729b9a30"),
          "token",
          false,
      },
      {
          // empty token
          base::Uuid::ParseLowercase("a2ab30b9-d664-4dfc-a9db-85f9729b9a30"),
          "",
          false,
      },
  };

  for (const auto& test_case : kTestCases) {
    absl::optional<TriggerVerification> actual =
        TriggerVerification::Create(test_case.token, test_case.id);

    EXPECT_EQ(test_case.expected_created, actual.has_value())
        << "id: " << test_case.id << " token: " << test_case.token;
    if (test_case.expected_created) {
      EXPECT_EQ(actual->token(), test_case.token);
      EXPECT_EQ(actual->aggregatable_report_id(), test_case.id);
    }
  }
}

}  // namespace
}  // namespace network
