// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/client_info_helpers.h"

#include <string>

#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "printing/mojom/print.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace printing {

namespace {

TEST(ClientInfoHelpersTest,
     ClientInfoCollectionToCupsOptionValueValidWithAllFields) {
  mojom::IppClientInfo client_info(
      mojom::IppClientInfo::ClientType::kOperatingSystem, "a-", "B_", "1.",
      "a.1-B_");

  absl::optional<std::string> option_val =
      ClientInfoCollectionToCupsOptionValue(client_info);
  ASSERT_TRUE(option_val.has_value());
  ASSERT_GE(option_val.value().size(), 2u);
  ASSERT_EQ(option_val.value().front(), '{');
  ASSERT_EQ(option_val.value().back(), '}');

  base::StringPiece option_without_braces(option_val.value());
  option_without_braces.remove_prefix(1);
  option_without_braces.remove_suffix(1);

  std::vector<std::string> member_options =
      base::SplitString(option_without_braces, " ", base::TRIM_WHITESPACE,
                        base::SPLIT_WANT_NONEMPTY);

  EXPECT_THAT(member_options,
              testing::UnorderedElementsAre(
                  "client-name=a-", "client-type=4", "client-patches=B_",
                  "client-string-version=1.", "client-version=a.1-B_"));
}

TEST(ClientInfoHelpersTest,
     ClientInfoCollectionToCupsOptionValueValidWithMissingFields) {
  mojom::IppClientInfo::ClientType type =
      mojom::IppClientInfo::ClientType::kApplication;
  mojom::IppClientInfo client_info(type, "a-", absl::nullopt, "1.",
                                   absl::nullopt);
  absl::optional<std::string> option_val =
      ClientInfoCollectionToCupsOptionValue(client_info);
  ASSERT_TRUE(option_val.has_value());
  ASSERT_GE(option_val.value().size(), 2u);
  ASSERT_EQ(option_val.value().front(), '{');
  ASSERT_EQ(option_val.value().back(), '}');

  base::StringPiece option_without_braces(option_val.value());
  option_without_braces.remove_prefix(1);
  option_without_braces.remove_suffix(1);

  std::vector<std::string> member_options =
      base::SplitString(option_without_braces, " ", base::TRIM_WHITESPACE,
                        base::SPLIT_WANT_NONEMPTY);

  EXPECT_THAT(member_options,
              testing::UnorderedElementsAre("client-name=a-", "client-type=3",
                                            "client-string-version=1."));
}

TEST(ClientInfoHelpersTest, ClientInfoCollectionToCupsOptionValueInvalidChars) {
  mojom::IppClientInfo valid_client_info(
      mojom::IppClientInfo::ClientType::kOther, "name", "patch", "version",
      absl::nullopt);

  mojom::IppClientInfo client_info;

  client_info = valid_client_info;
  client_info.client_name = " ";
  EXPECT_FALSE(ClientInfoCollectionToCupsOptionValue(client_info).has_value());

  client_info = valid_client_info;
  client_info.client_patches = ";";
  EXPECT_FALSE(ClientInfoCollectionToCupsOptionValue(client_info).has_value());

  client_info = valid_client_info;
  client_info.client_version = "\\";
  EXPECT_FALSE(ClientInfoCollectionToCupsOptionValue(client_info).has_value());

  client_info = valid_client_info;
  client_info.client_string_version = "{";
  EXPECT_FALSE(ClientInfoCollectionToCupsOptionValue(client_info).has_value());
}

TEST(ClientInfoHelpersTest, ClientInfoCollectionToCupsOptionValueInvalidRange) {
  mojom::IppClientInfo valid_client_info(
      mojom::IppClientInfo::ClientType::kOther, "name", "patch", "version",
      absl::nullopt);

  mojom::IppClientInfo client_info;

  client_info = valid_client_info;
  client_info.client_name = std::string(kClientInfoMaxNameLength + 1, 'A');
  EXPECT_FALSE(ClientInfoCollectionToCupsOptionValue(client_info).has_value());

  client_info = valid_client_info;
  client_info.client_patches =
      std::string(kClientInfoMaxPatchesLength + 1, 'A');
  EXPECT_FALSE(ClientInfoCollectionToCupsOptionValue(client_info).has_value());

  client_info = valid_client_info;
  client_info.client_version =
      std::string(kClientInfoMaxVersionLength + 1, 'A');
  EXPECT_FALSE(ClientInfoCollectionToCupsOptionValue(client_info).has_value());

  client_info = valid_client_info;
  client_info.client_string_version =
      std::string(kClientInfoMaxStringVersionLength + 1, 'A');
  EXPECT_FALSE(ClientInfoCollectionToCupsOptionValue(client_info).has_value());

  client_info = valid_client_info;
  client_info.client_type = static_cast<mojom::IppClientInfo::ClientType>(
      static_cast<int>(mojom::IppClientInfo::ClientType::kMinValue) - 1);
  EXPECT_FALSE(ClientInfoCollectionToCupsOptionValue(client_info).has_value());

  client_info = valid_client_info;
  client_info.client_type = static_cast<mojom::IppClientInfo::ClientType>(
      static_cast<int>(mojom::IppClientInfo::ClientType::kMaxValue) + 1);
  EXPECT_FALSE(ClientInfoCollectionToCupsOptionValue(client_info).has_value());
}

}  // namespace

}  // namespace printing
