// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include "base/strings/string_number_conversions.h"
#include "google_apis/gcm/engine/gservices_settings.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gcm {

namespace {

const int64_t kAlternativeCheckinInterval = 16 * 60 * 60;
const char kAlternativeCheckinURL[] = "http://alternative.url/checkin";
const char kAlternativeMCSHostname[] = "alternative.gcm.host";
const int kAlternativeMCSSecurePort = 7777;
const char kAlternativeRegistrationURL[] =
    "http://alternative.url/registration";

const int64_t kDefaultCheckinInterval = 2 * 24 * 60 * 60;  // seconds = 2 days.
const char kDefaultCheckinURL[] = "https://android.clients.google.com/checkin";
const char kDefaultRegistrationURL[] =
    "https://android.clients.google.com/c2dm/register3";
const char kDefaultSettingsDigest[] =
    "1-da39a3ee5e6b4b0d3255bfef95601890afd80709";
const char kAlternativeSettingsDigest[] =
    "1-7da4aa4eb38a8bd3e330e3751cc0899924499134";

void AddSettingsToResponse(
    checkin_proto::AndroidCheckinResponse& checkin_response,
    const GServicesSettings::SettingsMap& settings,
    bool settings_diff) {
  for (GServicesSettings::SettingsMap::const_iterator iter = settings.begin();
       iter != settings.end();
       ++iter) {
    checkin_proto::GservicesSetting* setting = checkin_response.add_setting();
    setting->set_name(iter->first);
    setting->set_value(iter->second);
  }
  checkin_response.set_settings_diff(settings_diff);
}

}  // namespace

class GServicesSettingsTest : public testing::Test {
 public:
  GServicesSettingsTest();
  ~GServicesSettingsTest() override;

  void CheckAllSetToDefault();

  GServicesSettings& settings() {
    return gservices_settings_;
  }

 private:
  GServicesSettings gservices_settings_;
};

GServicesSettingsTest::GServicesSettingsTest()
    : gservices_settings_() {
}

GServicesSettingsTest::~GServicesSettingsTest() {}

void GServicesSettingsTest::CheckAllSetToDefault() {
  EXPECT_EQ(base::Seconds(kDefaultCheckinInterval),
            settings().GetCheckinInterval());
  EXPECT_EQ(GURL(kDefaultCheckinURL), settings().GetCheckinURL());
  EXPECT_EQ(GURL("https://mtalk.google.com:5228"),
            settings().GetMCSMainEndpoint());
  EXPECT_EQ(GURL("https://mtalk.google.com:443"),
            settings().GetMCSFallbackEndpoint());
  EXPECT_EQ(GURL(kDefaultRegistrationURL), settings().GetRegistrationURL());
}

// Verifies default values of the G-services settings and settings digest.
TEST_F(GServicesSettingsTest, DefaultSettingsAndDigest) {
  CheckAllSetToDefault();
  EXPECT_EQ(kDefaultSettingsDigest, settings().digest());
  EXPECT_EQ(kDefaultSettingsDigest,
            GServicesSettings::CalculateDigest(settings().settings_map()));
}

// Verifies digest calculation for the sample provided by protocol owners.
TEST_F(GServicesSettingsTest, CalculateDigest) {
  GServicesSettings::SettingsMap settings_map;
  settings_map["android_id"] = "55XXXXXXXXXXXXXXXX0";
  settings_map["checkin_interval"] = "86400";
  settings_map["checkin_url"] =
      "https://fake.address.google.com/canary/checkin";
  settings_map["chrome_device"] = "1";
  settings_map["device_country"] = "us";
  settings_map["gcm_hostname"] = "fake.address.google.com";
  settings_map["gcm_secure_port"] = "443";

  EXPECT_EQ("1-33381ccd1cf5791dc0e6dfa234266fa9f1259197",
            GServicesSettings::CalculateDigest(settings_map));
}

// Verifies that settings are not updated when load result is empty.
TEST_F(GServicesSettingsTest, UpdateFromEmptyLoadResult) {
  GCMStore::LoadResult result;
  result.gservices_digest = "";
  settings().UpdateFromLoadResult(result);

  CheckAllSetToDefault();
  EXPECT_EQ(kDefaultSettingsDigest, settings().digest());
}

// Verifies that settings are not when digest value does not match.
TEST_F(GServicesSettingsTest, UpdateFromLoadResultWithSettingMissing) {
  GCMStore::LoadResult result;
  result.gservices_settings["checkin_internval"] = "100000";
  result.gservices_digest = "digest_value";
  settings().UpdateFromLoadResult(result);

  CheckAllSetToDefault();
  EXPECT_EQ(kDefaultSettingsDigest, settings().digest());
}

// Verifies that the settings are set correctly based on the load result.
TEST_F(GServicesSettingsTest, UpdateFromLoadResult) {
  GCMStore::LoadResult result;
  result.gservices_settings["checkin_interval"] =
      base::NumberToString(kAlternativeCheckinInterval);
  result.gservices_settings["checkin_url"] = kAlternativeCheckinURL;
  result.gservices_settings["gcm_hostname"] = kAlternativeMCSHostname;
  result.gservices_settings["gcm_secure_port"] =
      base::NumberToString(kAlternativeMCSSecurePort);
  result.gservices_settings["gcm_registration_url"] =
      kAlternativeRegistrationURL;
  result.gservices_digest = kAlternativeSettingsDigest;
  settings().UpdateFromLoadResult(result);

  EXPECT_EQ(base::Seconds(kAlternativeCheckinInterval),
            settings().GetCheckinInterval());
  EXPECT_EQ(GURL(kAlternativeCheckinURL), settings().GetCheckinURL());
  EXPECT_EQ(GURL("https://alternative.gcm.host:7777"),
            settings().GetMCSMainEndpoint());
  EXPECT_EQ(GURL("https://alternative.gcm.host:443"),
            settings().GetMCSFallbackEndpoint());
  EXPECT_EQ(GURL(kAlternativeRegistrationURL), settings().GetRegistrationURL());
  EXPECT_EQ(GServicesSettings::CalculateDigest(result.gservices_settings),
            settings().digest());
}

// Verifies that the checkin interval is updated to minimum if the original
// value is less than minimum.
TEST_F(GServicesSettingsTest, CheckinResponseMinimumCheckinInterval) {
  // Setting the checkin interval to less than minimum.
  checkin_proto::AndroidCheckinResponse checkin_response;
  GServicesSettings::SettingsMap new_settings;
  new_settings["checkin_interval"] = "3600";
  AddSettingsToResponse(checkin_response, new_settings, false);

  EXPECT_TRUE(settings().UpdateFromCheckinResponse(checkin_response));

  EXPECT_EQ(GServicesSettings::MinimumCheckinInterval(),
            settings().GetCheckinInterval());
  EXPECT_EQ(GServicesSettings::CalculateDigest(new_settings),
            settings().digest());
}

// Verifies that default checkin interval can be selectively overwritten.
TEST_F(GServicesSettingsTest, CheckinResponseUpdateCheckinInterval) {
  checkin_proto::AndroidCheckinResponse checkin_response;
  GServicesSettings::SettingsMap new_settings;
  new_settings["checkin_interval"] = "86400";
  AddSettingsToResponse(checkin_response, new_settings, false);

  EXPECT_TRUE(settings().UpdateFromCheckinResponse(checkin_response));

  // Only the checkin interval was updated:
  EXPECT_EQ(base::Seconds(86400), settings().GetCheckinInterval());

  // Other settings still set to default.
  EXPECT_EQ(GURL("https://mtalk.google.com:5228"),
            settings().GetMCSMainEndpoint());
  EXPECT_EQ(GURL("https://mtalk.google.com:443"),
            settings().GetMCSFallbackEndpoint());
  EXPECT_EQ(GURL(kDefaultCheckinURL), settings().GetCheckinURL());
  EXPECT_EQ(GURL(kDefaultRegistrationURL), settings().GetRegistrationURL());

  EXPECT_EQ(GServicesSettings::CalculateDigest(new_settings),
            settings().digest());
}

// Verifies that default registration URL can be selectively overwritten.
TEST_F(GServicesSettingsTest, CheckinResponseUpdateRegistrationURL) {
  checkin_proto::AndroidCheckinResponse checkin_response;
  GServicesSettings::SettingsMap new_settings;
  new_settings["gcm_registration_url"] = "https://new.registration.url";
  AddSettingsToResponse(checkin_response, new_settings, false);

  EXPECT_TRUE(settings().UpdateFromCheckinResponse(checkin_response));

  // Only the registration URL was updated:
  EXPECT_EQ(GURL("https://new.registration.url"),
            settings().GetRegistrationURL());

  // Other settings still set to default.
  EXPECT_EQ(base::Seconds(kDefaultCheckinInterval),
            settings().GetCheckinInterval());
  EXPECT_EQ(GURL("https://mtalk.google.com:5228"),
            settings().GetMCSMainEndpoint());
  EXPECT_EQ(GURL("https://mtalk.google.com:443"),
            settings().GetMCSFallbackEndpoint());
  EXPECT_EQ(GURL(kDefaultCheckinURL), settings().GetCheckinURL());

  EXPECT_EQ(GServicesSettings::CalculateDigest(new_settings),
            settings().digest());
}

// Verifies that default checkin URL can be selectively overwritten.
TEST_F(GServicesSettingsTest, CheckinResponseUpdateCheckinURL) {
  checkin_proto::AndroidCheckinResponse checkin_response;
  GServicesSettings::SettingsMap new_settings;
  new_settings["checkin_url"] = "https://new.checkin.url";
  AddSettingsToResponse(checkin_response, new_settings, false);

  EXPECT_TRUE(settings().UpdateFromCheckinResponse(checkin_response));

  // Only the checkin URL was updated:
  EXPECT_EQ(GURL("https://new.checkin.url"), settings().GetCheckinURL());

  // Other settings still set to default.
  EXPECT_EQ(base::Seconds(kDefaultCheckinInterval),
            settings().GetCheckinInterval());
  EXPECT_EQ(GURL("https://mtalk.google.com:5228"),
            settings().GetMCSMainEndpoint());
  EXPECT_EQ(GURL("https://mtalk.google.com:443"),
            settings().GetMCSFallbackEndpoint());
  EXPECT_EQ(GURL(kDefaultRegistrationURL), settings().GetRegistrationURL());

  EXPECT_EQ(GServicesSettings::CalculateDigest(new_settings),
            settings().digest());
}

// Verifies that default MCS hostname can be selectively overwritten.
TEST_F(GServicesSettingsTest, CheckinResponseUpdateMCSHostname) {
  checkin_proto::AndroidCheckinResponse checkin_response;
  GServicesSettings::SettingsMap new_settings;
  new_settings["gcm_hostname"] = "new.gcm.hostname";
  AddSettingsToResponse(checkin_response, new_settings, false);

  EXPECT_TRUE(settings().UpdateFromCheckinResponse(checkin_response));

  // Only the MCS endpoints were updated:
  EXPECT_EQ(GURL("https://new.gcm.hostname:5228"),
            settings().GetMCSMainEndpoint());
  EXPECT_EQ(GURL("https://new.gcm.hostname:443"),
            settings().GetMCSFallbackEndpoint());

  // Other settings still set to default.
  EXPECT_EQ(base::Seconds(kDefaultCheckinInterval),
            settings().GetCheckinInterval());
  EXPECT_EQ(GURL(kDefaultCheckinURL), settings().GetCheckinURL());
  EXPECT_EQ(GURL(kDefaultRegistrationURL), settings().GetRegistrationURL());

  EXPECT_EQ(GServicesSettings::CalculateDigest(new_settings),
            settings().digest());
}

// Verifies that default MCS secure port can be selectively overwritten.
TEST_F(GServicesSettingsTest, CheckinResponseUpdateMCSSecurePort) {
  checkin_proto::AndroidCheckinResponse checkin_response;
  GServicesSettings::SettingsMap new_settings;
  new_settings["gcm_secure_port"] = "5229";
  AddSettingsToResponse(checkin_response, new_settings, false);

  EXPECT_TRUE(settings().UpdateFromCheckinResponse(checkin_response));

  // Only the main MCS endpoint was updated:
  EXPECT_EQ(GURL("https://mtalk.google.com:5229"),
            settings().GetMCSMainEndpoint());

  // Other settings still set to default.
  EXPECT_EQ(base::Seconds(kDefaultCheckinInterval),
            settings().GetCheckinInterval());
  EXPECT_EQ(GURL(kDefaultCheckinURL), settings().GetCheckinURL());
  EXPECT_EQ(GURL("https://mtalk.google.com:443"),
            settings().GetMCSFallbackEndpoint());
  EXPECT_EQ(GURL(kDefaultRegistrationURL), settings().GetRegistrationURL());

  EXPECT_EQ(GServicesSettings::CalculateDigest(new_settings),
            settings().digest());
}

// Update from checkin response should also do incremental update for both cases
// where some settings are removed or added.
TEST_F(GServicesSettingsTest, UpdateFromCheckinResponseSettingsDiff) {
  checkin_proto::AndroidCheckinResponse checkin_response;

  // Only the new settings will be included in the response with settings diff.
  GServicesSettings::SettingsMap settings_diff;
  settings_diff["new_setting_1"] = "new_setting_1_value";
  settings_diff["new_setting_2"] = "new_setting_2_value";
  settings_diff["gcm_secure_port"] = "5229";

  // Full settings are necessary to calculate digest.
  GServicesSettings::SettingsMap full_settings(settings_diff);
  std::string digest = GServicesSettings::CalculateDigest(full_settings);

  checkin_response.Clear();
  AddSettingsToResponse(checkin_response, settings_diff, true);
  EXPECT_TRUE(settings().UpdateFromCheckinResponse(checkin_response));
  EXPECT_EQ(full_settings, settings().settings_map());
  // Default setting overwritten by settings diff.
  EXPECT_EQ(GURL("https://mtalk.google.com:5229"),
            settings().GetMCSMainEndpoint());

  // Setting up diff removing some of the values (including default setting).
  settings_diff.clear();
  settings_diff["delete_new_setting_1"] = "";
  settings_diff["delete_gcm_secure_port"] = "";
  settings_diff["new_setting_3"] = "new_setting_3_value";

  // Updating full settings to calculate digest.
  full_settings.erase(full_settings.find("new_setting_1"));
  full_settings.erase(full_settings.find("gcm_secure_port"));
  full_settings["new_setting_3"] = "new_setting_3_value";
  digest = GServicesSettings::CalculateDigest(full_settings);

  checkin_response.Clear();
  AddSettingsToResponse(checkin_response, settings_diff, true);
  EXPECT_TRUE(settings().UpdateFromCheckinResponse(checkin_response));
  EXPECT_EQ(full_settings, settings().settings_map());
  // Default setting back to norm.
  EXPECT_EQ(GURL("https://mtalk.google.com:5228"),
            settings().GetMCSMainEndpoint());
}

}  // namespace gcm
