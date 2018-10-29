// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/updater/manifest_fetch_data.h"

#include <vector>

#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "extensions/browser/disable_reason.h"
#include "net/base/escape.h"

namespace extensions {

namespace {

// Maximum length of an extension manifest update check url, since it is a GET
// request. We want to stay under 2K because of proxies, etc.
const int kExtensionsManifestMaxURLSize = 2000;

// Strings to report the manifest location in Omaha update pings. Please use
// strings with no capitalization, spaces or underscorse.
const char kInternalLocation[] = "internal";
const char kExternalLocation[] = "external";
const char kPolicyLocation[] = "policy";
const char kOtherLocation[] = "other";
const char kInvalidLocation[] = "invalid";

void AddEnabledStateToPing(std::string* ping_value,
                      const ManifestFetchData::PingData* ping_data) {
  *ping_value += "&e=" + std::string(ping_data->is_enabled ? "1" : "0");
  if (!ping_data->is_enabled) {
    // Add a dr=<number> param for each bit set in disable reasons.
    for (int enum_value = 1; enum_value < disable_reason::DISABLE_REASON_LAST;
         enum_value <<= 1) {
      if (ping_data->disable_reasons & enum_value)
        *ping_value += "&dr=" + base::IntToString(enum_value);
    }
  }
}

}  // namespace

// static
std::string ManifestFetchData::GetSimpleLocationString(Manifest::Location loc) {
  std::string result = kInvalidLocation;
  switch (loc) {
    case Manifest::INTERNAL:
      result = kInternalLocation;
      break;
    case Manifest::EXTERNAL_PREF:
    case Manifest::EXTERNAL_PREF_DOWNLOAD:
    case Manifest::EXTERNAL_REGISTRY:
      result = kExternalLocation;
      break;
    case Manifest::COMPONENT:
    case Manifest::EXTERNAL_COMPONENT:
    case Manifest::UNPACKED:
    case Manifest::COMMAND_LINE:
      result = kOtherLocation;
      break;
    case Manifest::EXTERNAL_POLICY_DOWNLOAD:
    case Manifest::EXTERNAL_POLICY:
      result = kPolicyLocation;
      break;
    case Manifest::INVALID_LOCATION:
    case Manifest::NUM_LOCATIONS:
      NOTREACHED();
      break;
  }

  return result;
}

ManifestFetchData::ManifestFetchData(const GURL& update_url,
                                     int request_id,
                                     const std::string& brand_code,
                                     const std::string& base_query_params,
                                     PingMode ping_mode,
                                     FetchPriority fetch_priority)
    : base_url_(update_url),
      full_url_(update_url),
      brand_code_(brand_code),
      ping_mode_(ping_mode),
      fetch_priority_(fetch_priority) {
  std::string query =
      full_url_.has_query() ? full_url_.query() + "&" : std::string();
  query += base_query_params;
  GURL::Replacements replacements;
  replacements.SetQueryStr(query);
  full_url_ = full_url_.ReplaceComponents(replacements);

  request_ids_.insert(request_id);
}

ManifestFetchData::~ManifestFetchData() {
}

// The format for request parameters in update checks is:
//
//   ?x=EXT1_INFO&x=EXT2_INFO
//
// where EXT1_INFO and EXT2_INFO are url-encoded strings of the form:
//
//   id=EXTENSION_ID&v=VERSION&uc
//
// Provide ping data with the parameter ping=PING_DATA where PING_DATA
// looks like r=DAYS or a=DAYS for extensions in the Chrome extensions gallery.
// ('r' refers to 'roll call' ie installation, and 'a' refers to 'active').
// These values will each be present at most once every 24 hours, and indicate
// the number of days since the last time it was present in an update check.
//
// So for two extensions like:
//   Extension 1- id:aaaa version:1.1
//   Extension 2- id:bbbb version:2.0
//
// the full update url would be:
//   http://somehost/path?x=id%3Daaaa%26v%3D1.1%26uc&x=id%3Dbbbb%26v%3D2.0%26uc
//
// (Note that '=' is %3D and '&' is %26 when urlencoded.)
bool ManifestFetchData::AddExtension(const std::string& id,
                                     const std::string& version,
                                     const PingData* ping_data,
                                     const std::string& update_url_data,
                                     const std::string& install_source,
                                     const std::string& install_location,
                                     FetchPriority fetch_priority) {
  if (extension_ids_.find(id) != extension_ids_.end()) {
    NOTREACHED() << "Duplicate extension id " << id;
    return false;
  }

  if (fetch_priority_ != FOREGROUND) {
    fetch_priority_ = fetch_priority;
  }

  // Compute the string we'd append onto the full_url_, and see if it fits.
  std::vector<std::string> parts;
  parts.push_back("id=" + id);
  parts.push_back("v=" + version);
  if (!install_source.empty())
    parts.push_back("installsource=" + install_source);
  if (!install_location.empty())
    parts.push_back("installedby=" + install_location);
  parts.push_back("uc");

  if (!update_url_data.empty()) {
    // Make sure the update_url_data string is escaped before using it so that
    // there is no chance of overriding the id or v other parameter value
    // we place into the x= value.
    parts.push_back("ap=" + net::EscapeQueryParamValue(update_url_data, true));
  }

  // Append brand code, rollcall and active ping parameters.
  if (ping_mode_ != NO_PING) {
    if (!brand_code_.empty())
      parts.push_back(base::StringPrintf("brand=%s", brand_code_.c_str()));

    std::string ping_value;
    pings_[id] = PingData(0, 0, false, 0);
    if (ping_data) {
      if (ping_data->rollcall_days == kNeverPinged ||
          ping_data->rollcall_days > 0) {
        ping_value += "r=" + base::IntToString(ping_data->rollcall_days);
        if (ping_mode_ == PING_WITH_ENABLED_STATE)
          AddEnabledStateToPing(&ping_value, ping_data);
        pings_[id].rollcall_days = ping_data->rollcall_days;
        pings_[id].is_enabled = ping_data->is_enabled;
      }
      if (ping_data->active_days == kNeverPinged ||
          ping_data->active_days > 0) {
        if (!ping_value.empty())
          ping_value += "&";
        ping_value += "a=" + base::IntToString(ping_data->active_days);
        pings_[id].active_days = ping_data->active_days;
      }
    }
    if (!ping_value.empty())
      parts.push_back("ping=" + net::EscapeQueryParamValue(ping_value, true));
  }

  std::string extra = full_url_.has_query() ? "&" : "?";
  extra +=
      "x=" + net::EscapeQueryParamValue(base::JoinString(parts, "&"), true);

  // Check against our max url size, exempting the first extension added.
  int new_size = full_url_.possibly_invalid_spec().size() + extra.size();
  if (!extension_ids_.empty() && new_size > kExtensionsManifestMaxURLSize) {
    UMA_HISTOGRAM_PERCENTAGE("Extensions.UpdateCheckHitUrlSizeLimit", 1);
    return false;
  }
  UMA_HISTOGRAM_PERCENTAGE("Extensions.UpdateCheckHitUrlSizeLimit", 0);

  // We have room so go ahead and add the extension.
  extension_ids_.insert(id);
  full_url_ = GURL(full_url_.possibly_invalid_spec() + extra);
  return true;
}

bool ManifestFetchData::Includes(const std::string& extension_id) const {
  return extension_ids_.find(extension_id) != extension_ids_.end();
}

bool ManifestFetchData::DidPing(const std::string& extension_id,
                                PingType type) const {
  auto i = pings_.find(extension_id);
  if (i == pings_.end())
    return false;
  int value = 0;
  if (type == ROLLCALL)
    value = i->second.rollcall_days;
  else if (type == ACTIVE)
    value = i->second.active_days;
  else
    NOTREACHED();
  return value == kNeverPinged || value > 0;
}

void ManifestFetchData::Merge(const ManifestFetchData& other) {
  DCHECK(full_url() == other.full_url());
  if (fetch_priority_ != FOREGROUND) {
    fetch_priority_ = other.fetch_priority_;
  }
  request_ids_.insert(other.request_ids_.begin(), other.request_ids_.end());
}

}  // namespace extensions
