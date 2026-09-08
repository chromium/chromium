// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OMAHA_MODEL_OMAHA_PING_H_
#define IOS_CHROME_BROWSER_OMAHA_MODEL_OMAHA_PING_H_

#import <string>

#import "base/time/time.h"
#import "base/version.h"

// Represents the category of Omaha ping event.
enum class OmahaPingEvent {
  kInstallEvent,
  kUsagePing,
};

// Stores information about an Omaha ping event.
struct OmahaPingData {
  std::string request_id;
  std::string session_id;
  std::string channel_name;
  std::string locale_lang;
  std::string hardware_class;
  std::string os_version;
  base::Version current_version;
  base::Version previous_version;
  base::Time installation_time;
  const int last_server_date;
};

// Returns the formatted content for a ping event.
std::string FormatOmahaPingEvent(OmahaPingEvent event, OmahaPingData data);

#endif  // IOS_CHROME_BROWSER_OMAHA_MODEL_OMAHA_PING_H_
