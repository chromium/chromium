// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_UPDATER_MANIFEST_FETCH_DATA_H_
#define EXTENSIONS_BROWSER_UPDATER_MANIFEST_FETCH_DATA_H_

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/version.h"
#include "extensions/browser/updater/extension_downloader_task.h"
#include "extensions/browser/updater/extension_downloader_types.h"
#include "extensions/common/extension_id.h"
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

  // Returns a string to use for reporting an extension's install location.
  // Some locations with a common purpose, such as the external locations, are
  // grouped together and will return the same string.
  static std::string GetSimpleLocationString(mojom::ManifestLocation loc);

  ManifestFetchData(const GURL& update_url,
                    int request_id,
                    const std::string& brand_code,
                    const std::string& base_query_params,
                    PingMode ping_mode,
                    DownloadFetchPriority fetch_priority);

  ManifestFetchData(const ManifestFetchData&) = delete;
  ManifestFetchData& operator=(const ManifestFetchData&) = delete;

  ~ManifestFetchData();

  // Returns true if this extension information was successfully added. If the
  // return value is false it means the full_url would have become too long or
  // the request type is not compatible the current request type, and
  // this ManifestFetchData object remains unchanged.
  bool AddExtension(const std::string& id,
                    const std::string& version,
                    const DownloadPingData* ping_data,
                    const std::string& update_url_data,
                    const std::string& install_source,
                    mojom::ManifestLocation install_location,
                    DownloadFetchPriority fetch_priority);

  // Stores a task in list of associated tasks, call this after successful
  // AddExtension.
  void AddAssociatedTask(ExtensionDownloaderTask task);

  const GURL& base_url() const { return base_url_; }
  const GURL& full_url() const { return full_url_; }
  ExtensionIdSet GetExtensionIds() const;
  const std::set<int>& request_ids() const { return request_ids_; }
  bool foreground_check() const {
    return fetch_priority_ == DownloadFetchPriority::kForeground;
  }
  DownloadFetchPriority fetch_priority() const { return fetch_priority_; }
  bool is_all_external_policy_download() const {
    return is_all_external_policy_download_;
  }

  // Returns true if the given id is included in this manifest fetch.
  bool Includes(const ExtensionId& extension_id) const;

  // Resets the full url to base url and removes |id_to_remove| from
  // the ManifestFetchData.
  void RemoveExtensions(const ExtensionIdSet& id_to_remove,
                        const std::string& base_query_params);

  // Returns true if a ping parameter for |type| was added to full_url for this
  // extension id.
  bool DidPing(const ExtensionId& extension_id, PingType type) const;

  // Assuming that both this ManifestFetchData and |other| have the same
  // full_url, this method merges the other information associated with the
  // fetch (in particular this adds all request ids associated with |other|
  // to this ManifestFetchData).
  void Merge(std::unique_ptr<ManifestFetchData> other);

  // Assigns true if all the extensions are force installed.
  void set_is_all_external_policy_download();

  // Returns list of associated tasks, added previously by `AddAssociatedTask`.
  const std::vector<ExtensionDownloaderTask>& GetAssociatedTasks() const {
    return associated_tasks_;
  }

  // Returns list of associated tasks. Note that the list will be moved from
  // ManifestFetchData.
  std::vector<ExtensionDownloaderTask> TakeAssociatedTasks();

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
  std::map<std::string, DownloadPingData> pings_;

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
  DownloadFetchPriority fetch_priority_;

  // The flag is set to true if all the extensions are force installed
  // extensions.
  bool is_all_external_policy_download_{false};

  // List of associated tasks for ExtensionDownloader.
  std::vector<ExtensionDownloaderTask> associated_tasks_;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_UPDATER_MANIFEST_FETCH_DATA_H_
