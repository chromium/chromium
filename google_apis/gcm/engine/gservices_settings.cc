// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/gcm/engine/gservices_settings.h"

#include <stdint.h>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/hash/sha1.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "google_apis/gcm/engine/gservices_switches.h"

namespace {
// The expected time in seconds between periodic checkins.
const char kCheckinIntervalKey[] = "checkin_interval";
// The override URL to the checkin server.
const char kCheckinURLKey[] = "checkin_url";
// The MCS machine name to connect to.
const char kMCSHostnameKey[] = "gcm_hostname";
// The MCS port to connect to.
const char kMCSSecurePortKey[] = "gcm_secure_port";
// The URL to get MCS registration IDs.
const char kRegistrationURLKey[] = "gcm_registration_url";

const int64_t kDefaultCheckinInterval = 2 * 24 * 60 * 60;  // seconds = 2 days.
const int64_t kMinimumCheckinInterval = 12 * 60 * 60;  // seconds = 12 hours.
const char kDefaultCheckinURL[] = "https://android.clients.google.com/checkin";
const char kDefaultMCSHostname[] = "mtalk.google.com";
const int kDefaultMCSMainSecurePort = 5228;
const int kDefaultMCSFallbackSecurePort = 443;
const char kDefaultRegistrationURL[] =
    "https://android.clients.google.com/c2dm/register3";
// Settings that are to be deleted are marked with this prefix in checkin
// response.
const char kDeleteSettingPrefix[] = "delete_";
// Settings digest starts with verison number followed by '-'.
const char kDigestVersionPrefix[] = "1-";
const char kMCSEnpointTemplate[] = "https://%s:%d";
const int kMaxSecurePort = 65535;

std::string MakeMCSEndpoint(const std::string& mcs_hostname, int port) {
  return base::StringPrintf(kMCSEnpointTemplate, mcs_hostname.c_str(), port);
}

// Default settings can be omitted, as GServicesSettings class provides
// reasonable defaults.
bool CanBeOmitted(const std::string& settings_name) {
  return settings_name == kCheckinIntervalKey ||
         settings_name == kCheckinURLKey ||
         settings_name == kMCSHostnameKey ||
         settings_name == kMCSSecurePortKey ||
         settings_name == kRegistrationURLKey;
}

bool VerifyCheckinInterval(
    const gcm::GServicesSettings::SettingsMap& settings) {
  gcm::GServicesSettings::SettingsMap::const_iterator iter =
      settings.find(kCheckinIntervalKey);
  if (iter == settings.end())
    return CanBeOmitted(kCheckinIntervalKey);

  int64_t checkin_interval = kMinimumCheckinInterval;
  if (!base::StringToInt64(iter->second, &checkin_interval)) {
    DVLOG(1) << "Failed to parse checkin interval: " << iter->second;
    return false;
  }
  if (checkin_interval == std::numeric_limits<int64_t>::max()) {
    DVLOG(1) << "Checkin interval is too big: " << checkin_interval;
    return false;
  }
  if (checkin_interval < kMinimumCheckinInterval) {
    DVLOG(1) << "Checkin interval: " << checkin_interval
             << " is less than allowed minimum: " << kMinimumCheckinInterval;
  }

  return true;
}

bool VerifyMCSEndpoint(const gcm::GServicesSettings::SettingsMap& settings) {
  std::string mcs_hostname;
  gcm::GServicesSettings::SettingsMap::const_iterator iter =
      settings.find(kMCSHostnameKey);
  if (iter == settings.end()) {
    // Because endpoint has 2 parts (hostname and port) we are defaulting and
    // moving on with verification.
    if (CanBeOmitted(kMCSHostnameKey))
      mcs_hostname = kDefaultMCSHostname;
    else
      return false;
  } else if (iter->second.empty()) {
    DVLOG(1) << "Empty MCS hostname provided.";
    return false;
  } else {
    mcs_hostname = iter->second;
  }

  int mcs_secure_port = 0;
  iter = settings.find(kMCSSecurePortKey);
  if (iter == settings.end()) {
    // Simlarly we might have to default the port, when only hostname is
    // provided.
    if (CanBeOmitted(kMCSSecurePortKey))
      mcs_secure_port = kDefaultMCSMainSecurePort;
    else
      return false;
  } else if (!base::StringToInt(iter->second, &mcs_secure_port)) {
    DVLOG(1) << "Failed to parse MCS secure port: " << iter->second;
    return false;
  }

  if (mcs_secure_port < 0 || mcs_secure_port > kMaxSecurePort) {
    DVLOG(1) << "Incorrect port value: " << mcs_secure_port;
    return false;
  }

  GURL mcs_main_endpoint(MakeMCSEndpoint(mcs_hostname, mcs_secure_port));
  if (!mcs_main_endpoint.is_valid()) {
    DVLOG(1) << "Invalid main MCS endpoint: "
             << mcs_main_endpoint.possibly_invalid_spec();
    return false;
  }
  GURL mcs_fallback_endpoint(
      MakeMCSEndpoint(mcs_hostname, kDefaultMCSFallbackSecurePort));
  if (!mcs_fallback_endpoint.is_valid()) {
    DVLOG(1) << "Invalid fallback MCS endpoint: "
             << mcs_fallback_endpoint.possibly_invalid_spec();
    return false;
  }

  return true;
}

bool VerifyCheckinURL(const gcm::GServicesSettings::SettingsMap& settings) {
  gcm::GServicesSettings::SettingsMap::const_iterator iter =
      settings.find(kCheckinURLKey);
  if (iter == settings.end())
    return CanBeOmitted(kCheckinURLKey);

  GURL checkin_url(iter->second);
  if (!checkin_url.is_valid()) {
    DVLOG(1) << "Invalid checkin URL provided: " << iter->second;
    return false;
  }

  return true;
}

bool VerifyRegistrationURL(
    const gcm::GServicesSettings::SettingsMap& settings) {
  gcm::GServicesSettings::SettingsMap::const_iterator iter =
      settings.find(kRegistrationURLKey);
  if (iter == settings.end())
    return CanBeOmitted(kRegistrationURLKey);

  GURL registration_url(iter->second);
  if (!registration_url.is_valid()) {
    DVLOG(1) << "Invalid registration URL provided: " << iter->second;
    return false;
  }

  return true;
}

bool VerifySettings(const gcm::GServicesSettings::SettingsMap& settings) {
  return VerifyCheckinInterval(settings) && VerifyMCSEndpoint(settings) &&
         VerifyCheckinURL(settings) && VerifyRegistrationURL(settings);
}

}  // namespace

namespace gcm {

// static
const base::TimeDelta GServicesSettings::MinimumCheckinInterval() {
  return base::Seconds(kMinimumCheckinInterval);
}

// static
std::string GServicesSettings::CalculateDigest(const SettingsMap& settings) {
  std::string data;
  for (SettingsMap::const_iterator iter = settings.begin();
       iter != settings.end();
       ++iter) {
    data += iter->first;
    data += '\0';
    data += iter->second;
    data += '\0';
  }
  std::string digest = kDigestVersionPrefix;
  digest += base::ToLowerASCII(
      base::HexEncode(base::SHA1Hash(base::as_byte_span(data))));
  return digest;
}

GServicesSettings::GServicesSettings() {
  digest_ = CalculateDigest(settings_);
}

GServicesSettings::~GServicesSettings() {
}

bool GServicesSettings::UpdateFromCheckinResponse(
    const checkin_proto::AndroidCheckinResponse& checkin_response) {
  if (!checkin_response.has_settings_diff()) {
    DVLOG(1) << "Field settings_diff not set in response.";
    return false;
  }

  bool settings_diff = checkin_response.settings_diff();
  SettingsMap new_settings;
  // Only reuse the existing settings, if we are given a settings difference.
  if (settings_diff)
    new_settings = settings_map();

  for (int i = 0; i < checkin_response.setting_size(); ++i) {
    std::string name = checkin_response.setting(i).name();
    if (name.empty()) {
      DVLOG(1) << "Setting name is empty";
      return false;
    }

    if (settings_diff && base::StartsWith(name, kDeleteSettingPrefix,
                                          base::CompareCase::SENSITIVE)) {
      std::string setting_to_delete =
          name.substr(std::size(kDeleteSettingPrefix) - 1);
      new_settings.erase(setting_to_delete);
      DVLOG(1) << "Setting deleted: " << setting_to_delete;
    } else {
      std::string value = checkin_response.setting(i).value();
      new_settings[name] = value;
      DVLOG(1) << "New setting: '" << name << "' : '" << value << "'";
    }
  }

  if (!VerifySettings(new_settings))
    return false;

  settings_.swap(new_settings);
  digest_ = CalculateDigest(settings_);
  return true;
}

void GServicesSettings::UpdateFromLoadResult(
    const GCMStore::LoadResult& load_result) {
  // No need to try to update settings when load_result is empty.
  if (load_result.gservices_settings.empty())
    return;
  if (!VerifySettings(load_result.gservices_settings))
    return;
  std::string digest = CalculateDigest(load_result.gservices_settings);
  if (digest != load_result.gservices_digest) {
    DVLOG(1) << "G-services settings digest mismatch. "
             << "Expected digest: " << load_result.gservices_digest
             << ". Calculated digest is: " << digest;
    return;
  }

  settings_ = load_result.gservices_settings;
  digest_ = load_result.gservices_digest;
}

base::TimeDelta GServicesSettings::GetCheckinInterval() const {
  int64_t checkin_interval = kMinimumCheckinInterval;
  SettingsMap::const_iterator iter = settings_.find(kCheckinIntervalKey);
  if (iter == settings_.end() ||
      !base::StringToInt64(iter->second, &checkin_interval)) {
    checkin_interval = kDefaultCheckinInterval;
  }

  if (checkin_interval < kMinimumCheckinInterval)
    checkin_interval = kMinimumCheckinInterval;

  return base::Seconds(checkin_interval);
}

GURL GServicesSettings::GetCheckinURL() const {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kGCMCheckinURL))
    return GURL(command_line->GetSwitchValueASCII(switches::kGCMCheckinURL));

  SettingsMap::const_iterator iter = settings_.find(kCheckinURLKey);
  if (iter == settings_.end() || iter->second.empty())
    return GURL(kDefaultCheckinURL);
  return GURL(iter->second);
}

GURL GServicesSettings::GetMCSMainEndpoint() const {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kGCMMCSEndpoint))
    return GURL(command_line->GetSwitchValueASCII(switches::kGCMMCSEndpoint));

  // Get alternative hostname or use default.
  std::string mcs_hostname;
  SettingsMap::const_iterator iter = settings_.find(kMCSHostnameKey);
  if (iter != settings_.end() && !iter->second.empty())
    mcs_hostname = iter->second;
  else
    mcs_hostname = kDefaultMCSHostname;

  // Get alternative secure port or use default.
  int mcs_secure_port = 0;
  iter = settings_.find(kMCSSecurePortKey);
  if (iter == settings_.end() || iter->second.empty() ||
      !base::StringToInt(iter->second, &mcs_secure_port)) {
    mcs_secure_port = kDefaultMCSMainSecurePort;
  }

  // If constructed address makes sense use it.
  GURL mcs_endpoint(MakeMCSEndpoint(mcs_hostname, mcs_secure_port));
  if (mcs_endpoint.is_valid())
    return mcs_endpoint;

  // Otherwise use default settings.
  return GURL(MakeMCSEndpoint(kDefaultMCSHostname, kDefaultMCSMainSecurePort));
}

GURL GServicesSettings::GetMCSFallbackEndpoint() const {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kGCMMCSEndpoint))
    return GURL();  // No fallback endpoint when using command line override.

  // Get alternative hostname or use default.
  std::string mcs_hostname;
  SettingsMap::const_iterator iter = settings_.find(kMCSHostnameKey);
  if (iter != settings_.end() && !iter->second.empty())
    mcs_hostname = iter->second;
  else
    mcs_hostname = kDefaultMCSHostname;

  // If constructed address makes sense use it.
  GURL mcs_endpoint(
      MakeMCSEndpoint(mcs_hostname, kDefaultMCSFallbackSecurePort));
  if (mcs_endpoint.is_valid())
    return mcs_endpoint;

  return GURL(
      MakeMCSEndpoint(kDefaultMCSHostname, kDefaultMCSFallbackSecurePort));
}

GURL GServicesSettings::GetRegistrationURL() const {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kGCMRegistrationURL)) {
    return GURL(
        command_line->GetSwitchValueASCII(switches::kGCMRegistrationURL));
  }

  SettingsMap::const_iterator iter = settings_.find(kRegistrationURLKey);
  if (iter == settings_.end() || iter->second.empty())
    return GURL(kDefaultRegistrationURL);
  return GURL(iter->second);
}

}  // namespace gcm
