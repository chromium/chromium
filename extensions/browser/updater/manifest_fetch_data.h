// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_UPDATER_MANIFEST_FETCH_DATA_H_
#define EXTENSIONS_BROWSER_UPDATER_MANIFEST_FETCH_DATA_H_

#include <map>
#include <set>
#include <string>

#include "base/version.h"
#include "extensions/common/manifest.h"
#include "extensions/common/mojom/manifest.mojom-shared.h"
#include "url/gurl.h"

namespace extensions {

// To save on server resources we can request updates for multiple extensions
// in one manifest check. This class helps us keep track of the id's for a
// given fetch, building up the actual URL, and what if anything to include
// in the ping parameter.
class ManifestFetchData {
 public:
  static const int kNeverPinged = -1;

  // What ping mode this fetch should use.
  enum PingMode {
    // No ping, no extra metrics.
    NO_PING,

    // Ping without extra metrics.
    PING,

    // Ping with information about enabled/disabled state.
    PING_WITH_ENABLED_STATE,
  };

  // Each ping type is sent at most once per day.
  enum PingType {
    // Used for counting total installs of an extension/app/theme.
    ROLLCALL,

    // Used for counting number of active users of an app, where "active" means
    // the app was launched at least once since the last active ping.
    ACTIVE,
  };

  // What is the priority of the update request.
  enum FetchPriority {
    // Used for update requests not initiated by a user, for example regular
    // extension updates started by the scheduler.
    BACKGROUND,

    // Used for on-demate update requests i.e. requests initiated by a users.
    FOREGROUND,
  };

  struct PingData {
    // The number of days it's been since our last rollcall or active ping,
    // respectively. These are calculated based on the start of day from the
    // server's perspective.
    int rollcall_days;
    int active_days;

    // Whether the extension is enabled or not.
    bool is_enabled;

    // A bitmask of disable_reason::DisableReason's, which may contain one or
    // more reasons why an extension is disabled.
    int disable_reasons;

    PingData()
        : rollcall_days(0),
          active_days(0),
          is_enabled(true),
          disable_reasons(0) {}
    PingData(int rollcall, int active, bool enabled, int reasons)
        : rollcall_days(rollcall),
          active_days(active),
          is_enabled(enabled),
          disable_reasons(reasons) {}
  };

  // Returns a string to use for reporting an extension's install location.
  // Some locations with a common purpose, such as the external locations, are
  // grouped together and will return the same string.
  static std::string GetSimpleLocationString(mojom::ManifestLocation loc);

  ManifestFetchData(const GURL& update_url,
                    int request_id,
                    const std::string& brand_code,
                    const std::string& base_query_params,
                    PingMode ping_mode,
                    FetchPriority fetch_priority);

  ManifestFetchData(const ManifestFetchData&) = delete;
  ManifestFetchData& operator=(const ManifestFetchData&) = delete;

  ~ManifestFetchData();

  // Returns true if this extension information was successfully added. If the
  // return value is false it means the full_url would have become too long or
  // the request type is not compatible the current request type, and
  // this ManifestFetchData object remains unchanged.
  bool AddExtension(const std::string& id,
                    const std::string& version,
                    const PingData* ping_data,
                    const std::string& update_url_data,
                    const std::string& install_source,
                    mojom::ManifestLocation install_location,
                    FetchPriority fetch_priority);

  const GURL& base_url() const { return base_url_; }
  const GURL& full_url() const { return full_url_; }
  ExtensionIdSet GetExtensionIds() const;
  const std::set<int>& request_ids() const { return request_ids_; }
  bool foreground_check() const { return fetch_priority_ == FOREGROUND; }
  FetchPriority fetch_priority() const { return fetch_priority_; }
  bool is_all_external_policy_download() const {
    return is_all_external_policy_download_;
  }

  // Returns true if the given id is included in this manifest fetch.
  bool Includes(const std::string& extension_id) const;

  // Resets the full url to base url and removes |id_to_remove| from
  // the ManifestFetchData.
  void RemoveExtensions(const ExtensionIdSet& id_to_remove,
                        const std::string& base_query_params);

  // Returns true if a ping parameter for |type| was added to full_url for this
  // extension id.
  bool DidPing(const std::string& extension_id, PingType type) const;

  // Assuming that both this ManifestFetchData and |other| have the same
  // full_url, this method merges the other information associated with the
  // fetch (in particular this adds all request ids associated with |other|
  // to this ManifestFetchData).
  void Merge(const ManifestFetchData& other);

  // Assigns true if all the extensions are force installed.
  void set_is_all_external_policy_download();

 private:
  // Contains supplementary data needed to construct update manifest fetch
  // query.
  struct ExtensionData {
    ExtensionData();
    ExtensionData(const ExtensionData& other);
    ExtensionData(const base::Version& version,
                  const std::string& update_url_data,
                  const std::string& install_source,
                  mojom::ManifestLocation extension_location);

    ~ExtensionData();
    base::Version version;
    std::string update_url_data;
    std::string install_source;
    mojom::ManifestLocation extension_location{
        mojom::ManifestLocation::kInternal};
  };

  const std::map<ExtensionId, ExtensionData>& extensions_data() const {
    return extensions_data_;
  }

  // Appends query parameters to the full url if any.
  void UpdateFullUrl(const std::string& base_query_params);

  // The set of extension data for each extension.
  std::map<std::string, ExtensionData> extensions_data_;

  // The set of ping data we actually sent.
  std::map<std::string, PingData> pings_;

  // The base update url without any arguments added.
  GURL base_url_;

  // The base update url plus arguments indicating the id, version, etc.
  // information about each extension.
  GURL full_url_;

  // The set of request ids associated with this manifest fetch. If multiple
  // requests are trying to fetch the same manifest, they can be merged into
  // one fetch, so potentially multiple request ids can get associated with
  // one ManifestFetchData.
  std::set<int> request_ids_;

  // The brand code to include with manifest fetch queries, if non-empty and
  // |ping_mode_| >= PING.
  const std::string brand_code_;

  // The ping mode for this fetch. This determines whether or not ping data
  // (and possibly extra metrics) will be included in the fetch query.
  const PingMode ping_mode_;

  // The priority of the update.
  FetchPriority fetch_priority_;

  // The flag is set to true if all the extensions are force installed
  // extensions.
  bool is_all_external_policy_download_;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_UPDATER_MANIFEST_FETCH_DATA_H_
