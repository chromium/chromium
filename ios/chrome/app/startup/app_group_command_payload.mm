// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/startup/app_group_command_payload.h"

#import "base/apple/foundation_util.h"
#import "base/metrics/histogram_macros.h"
#import "base/numerics/safe_conversions.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/common/app_group/app_group_constants.h"

namespace {

// Histogram recording the delay, in seconds, between writing and reading an App
// Group command.
const char kApplicationGroupCommandDelay[] =
    "Startup.ApplicationGroupCommandDelay";

// Returns the value for the `key` App Group command preference in
// `dictionary`, or nil if missing or not of type `T`.
template <typename T>
T* GetAppGroupCommandValue(NSDictionary* dictionary, const char* key) {
  return base::apple::ObjCCast<T>(
      [dictionary objectForKey:base::SysUTF8ToNSString(key)]);
}

}  // namespace

AppGroupCommandPayload::AppGroupCommandPayload() = default;
AppGroupCommandPayload::AppGroupCommandPayload(const AppGroupCommandPayload&) =
    default;
AppGroupCommandPayload& AppGroupCommandPayload::operator=(
    const AppGroupCommandPayload&) = default;
AppGroupCommandPayload::AppGroupCommandPayload(AppGroupCommandPayload&&) =
    default;
AppGroupCommandPayload& AppGroupCommandPayload::operator=(
    AppGroupCommandPayload&&) = default;
AppGroupCommandPayload::~AppGroupCommandPayload() = default;

AppGroupCommandPayload ConsumeAppGroupCommandPayload() {
  NSUserDefaults* shared_defaults = app_group::GetGroupUserDefaults();
  NSString* command_preference =
      base::SysUTF8ToNSString(app_group::kChromeAppGroupCommandPreference);
  NSDictionary* dictionary = base::apple::ObjCCast<NSDictionary>(
      [shared_defaults objectForKey:command_preference]);
  [shared_defaults removeObjectForKey:command_preference];
  // The shared defaults are used to communicate with the extensions.
  // Synchronize them to avoid removing a command written after this one.
  [shared_defaults synchronize];

  AppGroupCommandPayload payload;
  if (!dictionary) {
    return payload;
  }
  payload.command = GetAppGroupCommandValue<NSString>(
      dictionary, app_group::kChromeAppGroupCommandCommandPreference);
  payload.caller_app = GetAppGroupCommandValue<NSString>(
      dictionary, app_group::kChromeAppGroupCommandAppPreference);
  if (NSDate* command_time = GetAppGroupCommandValue<NSDate>(
          dictionary, app_group::kChromeAppGroupCommandTimePreference)) {
    payload.command_time = base::Time::FromNSDate(command_time);
  }
  payload.external_text = GetAppGroupCommandValue<NSString>(
      dictionary, app_group::kChromeAppGroupCommandTextPreference);
  payload.external_data = GetAppGroupCommandValue<NSData>(
      dictionary, app_group::kChromeAppGroupCommandDataPreference);
  payload.index = GetAppGroupCommandValue<NSNumber>(
      dictionary, app_group::kChromeAppGroupCommandIndexPreference);
  if (!payload.command || !payload.caller_app) {
    return payload;
  }

  base::TimeDelta delay = base::Time::Now() - payload.command_time.value_or(
                                                  base::Time::UnixEpoch());
  UMA_HISTOGRAM_COUNTS_100(kApplicationGroupCommandDelay,
                           base::saturated_cast<int>(delay.InSeconds()));
  payload.is_valid = payload.command_time.has_value() &&
                     delay <= base::Seconds(app_group::kAppGroupCommandTimeout);
  return payload;
}
