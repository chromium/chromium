// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/omaha/model/omaha_ping.h"

#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace {

using ::testing::Eq;

// Constants used by tests.
const int64_t kUnknownInstallDate = 2;
const int kUnknownLastServerDate = -2;

}  // anonymous namespace

using OmahaFormatPingTest = PlatformTest;

// Tests a ping message.
TEST_F(OmahaFormatPingTest, PingMessage) {
  EXPECT_THAT(
      FormatOmahaPingEvent(OmahaPingEvent::kUsagePing,
                           OmahaPingData{
                               .request_id = "requestId",
                               .session_id = "sessionId",
                               .channel_name = "channelName",
                               .locale_lang = "en-US",
                               .hardware_class = "hardwareClass",
                               .os_version = "osVersion",
                               .current_version = base::Version("5.0.17.0"),
                               .installation_time = base::Time::Now(),
                               .last_server_date = kUnknownLastServerDate,
                           }),
      Eq("<?xml version=\"1.0\"?>\n<request protocol=\"3.0\" updater=\"iOS\" "
         "updaterversion=\"5.0.17.0\" updaterchannel=\"channelName\" "
         "ismachine=\"1\" requestid=\"requestId\" sessionid=\"sessionId\" "
         "hardware_class=\"hardwareClass\"><os platform=\"ios\" "
         "version=\"osVersion\" arch=\"arm64\"/><app brand=\"RIMZ\" "
         "appid=\"{TestApplicationID}\" version=\"5.0.17.0\" nextversion=\"\" "
         "ap=\"channelName\" lang=\"en-US\" client=\"\" "
         "installage=\"0\"><updatecheck/><ping active=\"1\" ad=\"-2\" "
         "rd=\"-2\"/></app></request>\n"));
}

// Tests a ping message with unknown install date.
TEST_F(OmahaFormatPingTest, PingMessage_WithUnknownInstallDate) {
  const base::Time unknown_date = base::Time::FromTimeT(kUnknownInstallDate);
  EXPECT_THAT(
      FormatOmahaPingEvent(OmahaPingEvent::kUsagePing,
                           OmahaPingData{
                               .request_id = "requestId",
                               .session_id = "sessionId",
                               .channel_name = "channelName",
                               .locale_lang = "en-US",
                               .hardware_class = "hardwareClass",
                               .os_version = "osVersion",
                               .current_version = base::Version("5.0.17.0"),
                               .installation_time = unknown_date,
                               .last_server_date = kUnknownLastServerDate,
                           }),
      Eq("<?xml version=\"1.0\"?>\n<request protocol=\"3.0\" updater=\"iOS\" "
         "updaterversion=\"5.0.17.0\" updaterchannel=\"channelName\" "
         "ismachine=\"1\" requestid=\"requestId\" sessionid=\"sessionId\" "
         "hardware_class=\"hardwareClass\"><os platform=\"ios\" "
         "version=\"osVersion\" arch=\"arm64\"/><app brand=\"RIMZ\" "
         "appid=\"{TestApplicationID}\" version=\"5.0.17.0\" nextversion=\"\" "
         "ap=\"channelName\" lang=\"en-US\" client=\"\"><updatecheck/><ping "
         "active=\"1\" ad=\"-2\" rd=\"-2\"/></app></request>\n"));
}

// Tests a first install event message.
TEST_F(OmahaFormatPingTest, InstallMessage_FirstInstall) {
  EXPECT_THAT(
      FormatOmahaPingEvent(OmahaPingEvent::kInstallEvent,
                           OmahaPingData{
                               .request_id = "requestId",
                               .session_id = "sessionId",
                               .channel_name = "channelName",
                               .locale_lang = "en-US",
                               .hardware_class = "hardwareClass",
                               .os_version = "osVersion",
                               .current_version = base::Version("5.0.17.0"),
                               .installation_time = base::Time::Now(),
                               .last_server_date = kUnknownLastServerDate,
                           }),
      Eq("<?xml version=\"1.0\"?>\n<request protocol=\"3.0\" updater=\"iOS\" "
         "updaterversion=\"5.0.17.0\" updaterchannel=\"channelName\" "
         "ismachine=\"1\" requestid=\"requestId\" sessionid=\"sessionId\" "
         "hardware_class=\"hardwareClass\"><os platform=\"ios\" "
         "version=\"osVersion\" arch=\"arm64\"/><app brand=\"RIMZ\" "
         "appid=\"{TestApplicationID}\" version=\"\" nextversion=\"5.0.17.0\" "
         "ap=\"channelName\" lang=\"en-US\" client=\"\" "
         "installage=\"-1\"><event eventtype=\"2\" eventresult=\"1\"/><ping "
         "active=\"1\" ad=\"-2\" rd=\"-2\"/></app></request>\n"));
}

// Tests an update install event message.
TEST_F(OmahaFormatPingTest, InstallMessage_Update) {
  EXPECT_THAT(
      FormatOmahaPingEvent(OmahaPingEvent::kInstallEvent,
                           OmahaPingData{
                               .request_id = "requestId",
                               .session_id = "sessionId",
                               .channel_name = "channelName",
                               .locale_lang = "en-US",
                               .hardware_class = "hardwareClass",
                               .os_version = "osVersion",
                               .current_version = base::Version("5.0.17.0"),
                               .previous_version = base::Version("3.0.2.0"),
                               .installation_time = base::Time::Now(),
                               .last_server_date = kUnknownLastServerDate,
                           }),
      Eq("<?xml version=\"1.0\"?>\n<request protocol=\"3.0\" updater=\"iOS\" "
         "updaterversion=\"5.0.17.0\" updaterchannel=\"channelName\" "
         "ismachine=\"1\" requestid=\"requestId\" sessionid=\"sessionId\" "
         "hardware_class=\"hardwareClass\"><os platform=\"ios\" "
         "version=\"osVersion\" arch=\"arm64\"/><app brand=\"RIMZ\" "
         "appid=\"{TestApplicationID}\" version=\"3.0.2.0\" "
         "nextversion=\"5.0.17.0\" ap=\"channelName\" lang=\"en-US\" "
         "client=\"\" installage=\"0\"><event eventtype=\"3\" "
         "eventresult=\"1\"/><ping active=\"1\" ad=\"-2\" "
         "rd=\"-2\"/></app></request>\n"));
}
