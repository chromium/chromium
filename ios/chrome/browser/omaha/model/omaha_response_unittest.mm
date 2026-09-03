// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/omaha/model/omaha_response.h"

#import <optional>
#import <string_view>

#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace {

using ::testing::Eq;

// Constants used by tests.
constexpr std::string_view kApplicationId = "test-app-id";

// Strip leading spaces in `string`.
std::string_view Strip(std::string_view string) {
  const std::size_t pos = string.find_first_not_of("\r\n\t\v ");
  if (pos != std::string_view::npos) {
    return string.substr(pos);
  }
  return string;
}

}  // namespace

using OmahaResponseTest = PlatformTest;

// Tests that parsing an empty response results in failure.
TEST_F(OmahaResponseTest, EmptyResponse) {
  EXPECT_THAT(ParseOmahaResponse(kApplicationId, std::string_view{}),
              Eq(base::unexpected(OmahaParsingError::kServerError)));
}

// Tests that parsing an empty response results in failure.
TEST_F(OmahaResponseTest, MalformedResponse) {
  EXPECT_THAT(ParseOmahaResponse(kApplicationId, Strip(R"(NotXML)")),
              Eq(base::unexpected(OmahaParsingError::kInvalidXML)));
}

// Tests that parsing a response with unknown status results in failure.
TEST_F(OmahaResponseTest, InvalidResponse) {
  EXPECT_THAT(ParseOmahaResponse(kApplicationId, Strip(R"(
              <?xml version="1.0"?>
              <response protocol="3.0" server="prod">
                <daystart elapsed_days="4088"/>
                <app appid="test-app-id" status="ok">
                  <updatecheck status="error"/>
                  <ping status="ok"/>
                </app>
              </response>)")),
              Eq(base::unexpected(OmahaParsingError::kInvalidResponse)));
}

// Tests that parsing a response with unknown app id status results in failure.
TEST_F(OmahaResponseTest, InvalidApplicationId) {
  EXPECT_THAT(ParseOmahaResponse(kApplicationId, Strip(R"(
              <?xml version="1.0"?>
              <response protocol="3.0" server="prod">
                <daystart elapsed_days="4088"/>
                <app appid="unknown-app-id" status="ok">
                  <updatecheck status="ok"/>
                  <ping status="ok"/>
                </app>
              </response>)")),
              Eq(base::unexpected(OmahaParsingError::kInvalidResponse)));
}

// Tests parsing a response with a version update.
TEST_F(OmahaResponseTest, ValidResponse) {
  EXPECT_THAT(ParseOmahaResponse(kApplicationId, Strip(R"(
              <?xml version="1.0"?>
              <response protocol="3.0" server="prod">
                <daystart elapsed_days="56754"/>
                <app appid="test-app-id" status="ok">
                  <updatecheck status="ok">
                    <urls>
                      <url codebase="http://www.goo.fr/foo/"/>
                    </urls>
                    <manifest version="0.0.1075.1441">
                      <packages>
                        <package hash="0" name="Chrome"
                            required="true" size="0"/>
                      </packages>
                      <actions>
                        <action event="update" run="Chrome"/>
                        <action event="postinstall" osminversion="6.0"/>
                      </actions>
                    </manifest>
                  </updatecheck>
                  <ping status="ok"/>
                </app>
              </response>)")),
              Eq(OmahaResponse{
                  .server_date = 56754,
                  .details =
                      UpgradeRecommendedDetails{
                          .upgrade_url = GURL("http://www.goo.fr/foo"),
                          .next_version = "0.0.1075.1441",
                          .is_up_to_date = false,
                      },
              }));
}

// Tests parsing a response indicating that the application is up to date.
TEST_F(OmahaResponseTest, ValidResponseUpToDate) {
  EXPECT_THAT(ParseOmahaResponse(kApplicationId, Strip(R"(
              <?xml version="1.0"?>
              <response protocol="3.0" server="prod">
                <daystart elapsed_days="4088"/>
                <app appid="test-app-id" status="ok">
                  <updatecheck status="noupdate"/>
                  <ping status="ok"/>
                </app>
              </response>)")),
              Eq(OmahaResponse{
                  .server_date = 4088,
                  .details =
                      UpgradeRecommendedDetails{
                          .is_up_to_date = true,
                      },
              }));
}

// Tests parsing a valid response without details is still a success.
TEST_F(OmahaResponseTest, ValidResponseNoDetails) {
  EXPECT_THAT(ParseOmahaResponse(kApplicationId, Strip(R"(
              <?xml version="1.0"?>
              <response protocol="3.0" server="prod">
                <daystart elapsed_days="4088"/>
                <app appid="test-app-id" status="ok">
                  <event status="ok"/>
                  <ping status="ok"/>
                </app>
              </response>)")),
              Eq(OmahaResponse{
                  .server_date = 4088,
                  .details = std::nullopt,
              }));
}
