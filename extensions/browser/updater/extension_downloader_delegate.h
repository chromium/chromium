// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_UPDATER_EXTENSION_DOWNLOADER_DELEGATE_H_
#define EXTENSIONS_BROWSER_UPDATER_EXTENSION_DOWNLOADER_DELEGATE_H_

#include <set>
#include <string>

#include "base/callback.h"
#include "base/time/time.h"
#include "extensions/browser/crx_file_info.h"
#include "extensions/browser/updater/manifest_fetch_data.h"
#include "extensions/common/extension_id.h"

class GURL;

namespace extensions {

class ExtensionDownloaderDelegate {
 public:
  virtual ~ExtensionDownloaderDelegate();

  // Passed as an argument to ExtensionDownloader::OnExtensionDownloadFailed()
  // to detail the reason for the failure.
  enum class Error {
    // Background networking is disabled.
    DISABLED,

    // Failed to fetch the manifest for this extension.
    MANIFEST_FETCH_FAILED,

    // The manifest couldn't be parsed.
    MANIFEST_INVALID,

    // The manifest was fetched and parsed, and there are no updates for
    // this extension.
    NO_UPDATE_AVAILABLE,

    // There was an update for this extension but the download of the crx
    // failed.
    CRX_FETCH_FAILED,
  };

  // Passed as an argument to OnExtensionDownloadStageChanged() to detail how
  // downloading is going on. Typical sequence is: PENDING ->
  // QUEUED_FOR_MANIFEST -> DOWNLOADING_MANIFEST -> PARSING_MANIFEST ->
  // MANIFEST_LOADED -> QUEUED_FOR_CRX -> DOWNLOADING_CRX -> FINISHED. Stages
  // QUEUED_FOR_MANIFEST and QUEUED_FOR_CRX are optional and may be skipped.
  // Failure on any stage will result in skipping all remained stages, moving to
  // FINISHED instantly. Special stages DOWNLOADING_MANIFEST_RETRY and
  // DOWNLOADING_CRX_RETRY are similar to QUEUED_* ones, but signify that
  // download failed at least once already. So, this may result in sequence like
  // ... -> MANIFEST_LOADED -> QUEUED_FOR_CRX -> DOWNLOADING_CRX ->
  // DOWNLOADING_CRX_RETRY -> DOWNLOADING_CRX -> FINISHED.
  // Note: enum used for UMA. Do NOT reorder or remove entries. Don't forget to
  // update enums.xml (name: ExtensionInstallationDownloadingStage) when adding
  // new entries.
  enum class Stage {
    // Downloader just received extension download request.
    PENDING = 0,

    // Extension is in manifest loading queue.
    QUEUED_FOR_MANIFEST = 1,

    // There is an active request to download extension's manifest.
    DOWNLOADING_MANIFEST = 2,

    // There were one or more unsuccessful tries to download manifest, but we'll
    // try more.
    DOWNLOADING_MANIFEST_RETRY = 3,

    // Manifest downloaded and is about to parse.
    PARSING_MANIFEST = 4,

    // Manifest downloaded and successfully parsed.
    MANIFEST_LOADED = 5,

    // Extension in CRX loading queue.
    QUEUED_FOR_CRX = 6,

    // There is an active request to download extension archive.
    DOWNLOADING_CRX = 7,

    // There were one or more unsuccessful tries to download archive, but we'll
    // try more.
    DOWNLOADING_CRX_RETRY = 8,

    // Downloading finished, either successfully or not.
    FINISHED = 9,

    // Magic constant used by the histogram macros.
    // Always update it to the max value.
    kMaxValue = FINISHED,
  };

  // Passes as an argument to OnExtensionDownloadCacheStatusRetrieved to inform
  // delegate about cache status.
  // Note: enum used for UMA. Do NOT reorder or remove entries. Don't forget to
  // update enums.xml (name: ExtensionInstallationCacheStatus) when adding new
  // entries.
  enum class CacheStatus {
    // No information about cache status. This is never reported by
    // ExtensionDownloader, but may be used later in statistics.
    CACHE_UNKNOWN = 0,

    // There is no cache at all.
    CACHE_DISABLED = 1,

    // Extension was not found in cache.
    CACHE_MISS = 2,

    // There is an extension in cache, but its version is not as expected.
    CACHE_OUTDATED = 3,

    // Cache entry is good and will be used.
    CACHE_HIT = 4,

    // Magic constant used by the histogram macros.
    // Always update it to the max value.
    kMaxValue = CACHE_HIT,
  };

  // Passed as an argument to the completion callbacks to signal whether
  // the extension update sent a ping.
  struct PingResult {
    PingResult();
    ~PingResult();

    // Whether a ping was sent.
    bool did_ping;

    // The start of day, from the server's perspective. This is only valid
    // when |did_ping| is true.
    base::Time day_start;
  };

  // A callback that is called to indicate if ExtensionDownloader should ignore
  // the cached entry and download a new .crx file.
  typedef base::Callback<void(bool should_download)> InstallCallback;

  // One of the following 3 methods is always invoked for a given extension
  // id, if AddExtension() or AddPendingExtension() returned true when that
  // extension was added to the ExtensionDownloader.
  // To avoid duplicate work, ExtensionDownloader might merge multiple identical
  // requests, so there is not necessarily a separate invocation of one of these
  // methods for each call to AddExtension/AddPendingExtension. If it is
  // important to be able to match up AddExtension calls with
  // OnExtensionDownload callbacks, you need to make sure that for every call to
  // AddExtension/AddPendingExtension the combination of extension id and
  // request id is unique. The OnExtensionDownload related callbacks will then
  // be called with all request ids that resulted in that extension being
  // checked.

  // Invoked several times during downloading, |stage| contains current stage
  // of downloading.
  virtual void OnExtensionDownloadStageChanged(const ExtensionId& id,
                                               Stage stage);

  // Invoked once during downloading, after fetching and parsing update
  // manifest, |cache_status| contains information about what have we found in
  // local cache about the extension.
  virtual void OnExtensionDownloadCacheStatusRetrieved(
      const ExtensionId& id,
      CacheStatus cache_status);

  // Invoked if the extension couldn't be downloaded. |error| contains the
  // failure reason.
  virtual void OnExtensionDownloadFailed(const ExtensionId& id,
                                         Error error,
                                         const PingResult& ping_result,
                                         const std::set<int>& request_ids);

  // Invoked if the extension had an update available and its crx was
  // successfully downloaded to |path|. |ownership_passed| is true if delegate
  // should get ownership of the file. The downloader may be able to get the
  // .crx file both from a locally cached version or by issuing a network
  // request. If the install attempt by the delegate fails and the source was
  // the cache, the cached version may be corrupt (or simply not the desired
  // one), and we'd like to try downloading the .crx from the network and have
  // the delegate attempt install again. So if the |callback| parameter is
  // non-null (if the file was taken from the cache), on install failure the
  // downloader should be notified to try download from network by calling the
  // callback with true; on successful install it should be called with false so
  // that downloader could release all downloaded metadata. After downloading
  // the delegate will be once again called with OnExtensionDownloadFinished (or
  // OnExtensionDownloadFailed) called again with the same |request_ids|.
  virtual void OnExtensionDownloadFinished(const CRXFileInfo& file,
                                           bool file_ownership_passed,
                                           const GURL& download_url,
                                           const std::string& version,
                                           const PingResult& ping_result,
                                           const std::set<int>& request_ids,
                                           const InstallCallback& callback) = 0;

  // Invoked when an extension fails to load, but a retry is triggered.
  // It allows unittests to easily set up and verify resourse request and
  // load results between a failure / retry sequence.
  virtual void OnExtensionDownloadRetryForTests();

  // The remaining methods are used by the ExtensionDownloader to retrieve
  // information about extensions from the delegate.

  // Invoked to fill the PingData for the given extension id. Returns false
  // if PingData should not be included for this extension's update check
  // (this is the default).
  virtual bool GetPingDataForExtension(const ExtensionId& id,
                                       ManifestFetchData::PingData* ping);

  // Invoked to get the update url data for this extension's update url, if
  // there is any. The default implementation returns an empty string.
  virtual std::string GetUpdateUrlData(const ExtensionId& id);

  // Invoked to determine whether extension |id| is currently
  // pending installation.
  virtual bool IsExtensionPending(const ExtensionId& id) = 0;

  // Invoked to get the current version of extension |id|. Returns false if
  // that extension is not installed.
  virtual bool GetExtensionExistingVersion(const ExtensionId& id,
                                           std::string* version) = 0;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_UPDATER_EXTENSION_DOWNLOADER_DELEGATE_H_
