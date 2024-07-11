// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/client_info_helpers.h"

#include <optional>
#include <string>

#include "base/strings/string_split.h"
#include "printing/mojom/print.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace printing {

namespace {

TEST(ClientInfoHelpersTest, ValidateClientInfoItemValidWithAllFields) {
  mojom::IppClientInfo client_info(
      mojom::IppClientInfo::ClientType::kOperatingSystem, "a-", "B_", "1.",
      "a.1-B_");

  EXPECT_TRUE(ValidateClientInfoItem(client_info));
}

TEST(ClientInfoHelpersTest, ValidateClientInfoItemValidWithMissingFields) {
  mojom::IppClientInfo::ClientType type =
      mojom::IppClientInfo::ClientType::kApplication;
  mojom::IppClientInfo client_info(type, "a-", std::nullopt, "1.",
                                   std::nullopt);

  EXPECT_TRUE(ValidateClientInfoItem(client_info));
}

TEST(ClientInfoHelpersTest, ValidateClientInfoItemInvalidChars) {
  mojom::IppClientInfo valid_client_info(
      mojom::IppClientInfo::ClientType::kOther, "name", "patch", "version",
      std::nullopt);

  mojom::IppClientInfo client_info;

  client_info = valid_client_info;
  client_info.client_name = " ";
  EXPECT_FALSE(ValidateClientInfoItem(client_info));

  client_info = valid_client_info;
  client_info.client_patches = ";";
  EXPECT_FALSE(ValidateClientInfoItem(client_info));

  client_info = valid_client_info;
  client_info.client_version = "\\";
  EXPECT_FALSE(ValidateClientInfoItem(client_info));

  client_info = valid_client_info;
  client_info.client_string_version = "{";
  EXPECT_FALSE(ValidateClientInfoItem(client_info));
}

TEST(ClientInfoHelpersTest, ValidateClientInfoItemInvalidRange) {
  mojom::IppClientInfo valid_client_info(
      mojom::IppClientInfo::ClientType::kOther, "name", "patch", "version",
      std::nullopt);

  mojom::IppClientInfo client_info;

  client_info = valid_client_info;
  client_info.client_name = std::string(kClientInfoMaxNameLength + 1, 'A');
  EXPECT_FALSE(ValidateClientInfoItem(client_info));

  client_info = valid_client_info;
  client_info.client_patches =
      std::string(kClientInfoMaxPatchesLength + 1, 'A');
  EXPECT_FALSE(ValidateClientInfoItem(client_info));

  client_info = valid_client_info;
  client_info.client_version =
      std::string(kClientInfoMaxVersionLength + 1, 'A');
  EXPECT_FALSE(ValidateClientInfoItem(client_info));

  client_info = valid_client_info;
  client_info.client_string_version =
      std::string(kClientInfoMaxStringVersionLength + 1, 'A');
  EXPECT_FALSE(ValidateClientInfoItem(client_info));

  client_info = valid_client_info;
  client_info.client_type = static_cast<mojom::IppClientInfo::ClientType>(
      static_cast<int>(mojom::IppClientInfo::ClientType::kMinValue) - 1);
  EXPECT_FALSE(ValidateClientInfoItem(client_info));

  client_info = valid_client_info;
  client_info.client_type = static_cast<mojom::IppClientInfo::ClientType>(
      static_cast<int>(mojom::IppClientInfo::ClientType::kMaxValue) + 1);
  EXPECT_FALSE(ValidateClientInfoItem(client_info));
}

}  // namespace

}  // namespace printing
