// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_UPDATER_EXTENSION_DOWNLOADER_H_
#define EXTENSIONS_BROWSER_UPDATER_EXTENSION_DOWNLOADER_H_

#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/version.h"
#include "extensions/browser/updater/extension_downloader_delegate.h"
#include "extensions/browser/updater/extension_downloader_task.h"
#include "extensions/browser/updater/extension_downloader_types.h"
#include "extensions/browser/updater/manifest_fetch_data.h"
#include "extensions/browser/updater/request_queue.h"
#include "extensions/browser/updater/safe_manifest_parser.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/mojom/manifest.mojom-shared.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/http/http_request_headers.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "url/gurl.h"

namespace crx_file {
enum class VerifierFormat;
}

namespace signin {
class PrimaryAccountAccessTokenFetcher;
class IdentityManager;
struct AccessTokenInfo;
}  // namespace signin

namespace network {
class SharedURLLoaderFactory;
class SimpleURLLoader;
struct ResourceRequest;
}  // namespace network

namespace extensions {

struct DownloadFailure {
  DownloadFailure(ExtensionDownloaderDelegate::Error error,
                  ExtensionDownloaderDelegate::FailureData failure_data);
  DownloadFailure(DownloadFailure&& other);
  ~DownloadFailure();

  ExtensionDownloaderDelegate::Error error;
  ExtensionDownloaderDelegate::FailureData failure_data;
};

class ExtensionCache;
class ExtensionDownloaderTestDelegate;
class ExtensionUpdaterTest;

// A class that checks for updates of a given list of extensions, and downloads
// the crx file when updates are found. It uses a |ExtensionDownloaderDelegate|
// that takes ownership of the downloaded crx files, and handles events during
// the update check.
class ExtensionDownloader {
 public:
  // A closure which constructs a new ExtensionDownloader to be owned by the
  // caller.
  using Factory = base::RepeatingCallback<std::unique_ptr<ExtensionDownloader>(
      ExtensionDownloaderDelegate* delegate)>;

  // |delegate| is stored as a raw pointer and must outlive the
  // ExtensionDownloader.
  ExtensionDownloader(
      ExtensionDownloaderDelegate* delegate,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      crx_file::VerifierFormat crx_format_requirement,
      const base::FilePath& profile_path = base::FilePath());

  ExtensionDownloader(const ExtensionDownloader&) = delete;
  ExtensionDownloader& operator=(const ExtensionDownloader&) = delete;

  ~ExtensionDownloader();

  // Adds extension to the list of extensions to check for updates.
  // Returns false if the extension can't be updated due to invalid details.
  // In that case, no callbacks will be performed on the |delegate_|. See
  // ExtensionDownloaderTask's description for more details and available
  // parameter.
  bool AddPendingExtension(ExtensionDownloaderTask task);

  // Schedules a fetch of the manifest of all the extensions added with
  // AddExtension() and AddPendingExtension().
  void StartAllPending(ExtensionCache* cache);

  // Sets the IdentityManager instance to be used for OAuth2 authentication on
  // protected Webstore downloads. The IdentityManager instance must be valid to
  // use for the lifetime of this object.
  void SetIdentityManager(signin::IdentityManager* identity_manager);

  void set_brand_code(const std::string& brand_code) {
    brand_code_ = brand_code;
  }

  void set_manifest_query_params(const std::string& params) {
    manifest_query_params_ = params;
  }

  void set_ping_enabled_domain(const std::string& domain) {
    ping_enabled_domain_ = domain;
  }

  // Set backoff policy for manifest and extension queue. Set `std::nullopt` to
  // restore to the default backoff policy. Used in tests and Kiosk launcher to
  // reduce retry backoff.
  void SetBackoffPolicy(
      std::optional<net::BackoffEntry::Policy> backoff_policy);

  bool HasActiveManifestRequestForTesting();

  ManifestFetchData* GetActiveManifestFetchForTesting();

  // An observer class that can be used by test code.
  class TestObserver {
   public:
    TestObserver();
    virtual ~TestObserver();

    // Invoked when an update is found for an extension, but before any attempt
    // to download it is made.
    // Will be invoked right before its namesake in ExtensionDownloaderDelegate.
    virtual void OnExtensionUpdateFound(const ExtensionId& id,
                                        const std::set<int>& request_ids,
                                        const base::Version& version) = 0;
  };
  // Sets a test observer to be used by any instances of this class.
  // The |observer| should outlive all ExtensionDownloader instances.
  static void set_test_observer(TestObserver* observer);
  // Returns the currently set TestObserver, if any.
  // Useful for sanity-checking test code.
  static TestObserver* test_observer();

  // Sets a test delegate to use by any instances of this class. The |delegate|
  // should outlive all instances.
  static void set_test_delegate(ExtensionDownloaderTestDelegate* delegate);

  // These are needed for unit testing, to help identify the correct mock
  // URLFetcher objects.
  static const int kManifestFetcherId = 1;
  static const int kExtensionFetcherId = 2;

  static const int kMaxRetries = 10;

  // Names of the header fields used for traffic management for extension
  // updater.
  static const char kUpdateInteractivityHeader[];
  static const char kUpdateAppIdHeader[];
  static const char kUpdateUpdaterHeader[];

  // Header values for foreground/background update requests.
  static const char kUpdateInteractivityForeground[];
  static const char kUpdateInteractivityBackground[];

 private:
  friend class ExtensionDownloaderTest;
  friend class ExtensionDownloaderTestHelper;
  friend class ExtensionUpdaterTest;

  // These counters are bumped as extensions are added to be fetched. They
  // are then recorded as UMA metrics when all the extensions have been added.
  struct URLStats {
    URLStats()
        : no_url_count(0),
          google_url_count(0),
          other_url_count(0),
          extension_count(0),
          theme_count(0),
          app_count(0),
          platform_app_count(0),
          pending_count(0) {}

    int no_url_count, google_url_count, other_url_count;
    int extension_count, theme_count, app_count, platform_app_count,
        pending_count;
  };

  // We need to keep track of some information associated with a url
  // when doing a fetch.
  struct ExtensionFetch {
    ExtensionFetch(ExtensionDownloaderTask task,
                   const GURL& url,
                   const std::string& package_hash,
                   const std::string& version,
                   DownloadFetchPriority fetch_priority);
    ~ExtensionFetch();

    // Collects request ids from associated tasks.
    std::set<int> GetRequestIds() const;

    ExtensionId id;
    GURL url;
    std::string package_hash;
    base::Version version;
    DownloadFetchPriority fetch_priority;
    std::vector<ExtensionDownloaderTask> associated_tasks;

    enum CredentialsMode {
      CREDENTIALS_NONE = 0,
      CREDENTIALS_OAUTH2_TOKEN,
      CREDENTIALS_COOKIES,
    };

    // Indicates the type of credentials to include with this fetch.
    CredentialsMode credentials;

    // Counts the number of times OAuth2 authentication has been attempted for
    // this fetch.
    int oauth2_attempt_count;
  };

  // We limit the number of extensions grouped together in one batch to avoid
  // running into the limits on the length of http GET requests, this represents
  // the key for grouping these extensions.
  struct FetchDataGroupKey {
    FetchDataGroupKey();
    FetchDataGroupKey(const FetchDataGroupKey& other);
    FetchDataGroupKey(const int request_id,
                      const GURL& update_url,
                      const bool is_force_installed);
    ~FetchDataGroupKey();

    bool operator<(const FetchDataGroupKey& other) const;

    int request_id{0};
    GURL update_url;
    // The extensions in current ManifestFetchData are all force installed
    // (mojom::ManifestLocation::kExternalPolicyDownload) or not. In a
    // ManifestFetchData we would have either all the extensions as force
    // installed or we would none extensions as force installed.
    bool is_force_installed{false};
  };

  enum class UpdateAvailability {
    kAvailable,
    kNoUpdate,
    kBadUpdateSpecification,
  };

  // Helper for AddExtension() and AddPendingExtension().
  bool AddExtensionData(ExtensionDownloaderTask task);

  // Begins an update check.
  void StartUpdateCheck(std::unique_ptr<ManifestFetchData> fetch_data);

  // Returns the URLLoaderFactory instance to be used, depending on whether
  // the URL being handled is file:// or not.
  network::mojom::URLLoaderFactory* GetURLLoaderFactoryToUse(const GURL& url);

  // Called by RequestQueue when a new manifest load request is started.
  void CreateManifestLoader();

  // Retries the active request with some backoff delay.
  void RetryManifestFetchRequest(
      RequestQueue<ManifestFetchData>::Request request,
      int network_error_code,
      int response_code);

  // Reports failures if we failed to fetch the manifest or the fetched manifest
  // was invalid.
  void ReportManifestFetchFailure(
      ManifestFetchData* fetch_data,
      ExtensionDownloaderDelegate::Error error,
      const ExtensionDownloaderDelegate::FailureData& data);

  // Tries fetching the extension from cache. Removes found extensions from
  // |fetch_data|. Return true if all extensions were found.
  bool TryFetchingExtensionsFromCache(ManifestFetchData* fetch_data);

  // Makes a retry attempt, reports failure by calling
  // AddFailureDataOnManifestFetchFailed when fetching of update manifest
  // failed.
  void RetryRequestOrHandleFailureOnManifestFetchFailure(
      RequestQueue<ManifestFetchData>::Request request,
      const network::SimpleURLLoader& loader,
      const int response_code);

  // Handles the result of a manifest fetch.
  void OnManifestLoadComplete(std::unique_ptr<network::SimpleURLLoader> loader,
                              std::unique_ptr<std::string> response_body);

  // Once a manifest is parsed, this starts fetches of any relevant crx files.
  // If |results| is null, it means something went wrong when parsing it.
  void HandleManifestResults(std::unique_ptr<ManifestFetchData> fetch_data,
                             std::unique_ptr<UpdateManifestResults> results,
                             const std::optional<ManifestParseFailure>& error);

  // This function partition extensions from given |tasks| into two sets:
  // update/error using the update information from |possible_updates| and
  // the extension system. When the function returns:
  // - |to_update| stores entries from |possible_updates| that will be updated.
  // - |errors| stores the entries of extension IDs along with the error that
  // occurred in the process (no update available is considered an error from
  // ExtensionDownloader's perspective).
  //   For example, a common error is |possible_updates| doesn't have any update
  //   information for some extensions.
  void DetermineUpdates(
      std::vector<ExtensionDownloaderTask> tasks,
      const UpdateManifestResults& possible_updates,
      std::vector<std::pair<ExtensionDownloaderTask, UpdateManifestResult*>>*
          to_update,
      std::vector<std::pair<ExtensionDownloaderTask, DownloadFailure>>* errors);

  // Checks whether extension is presented in cache. If yes, return path to its
  // cached CRX, std::nullopt otherwise. |manifest_fetch_failed| flag indicates
  // whether the lookup in cache is performed after the manifest is fetched or
  // due to failure while fetching or parsing manifest.
  std::optional<base::FilePath> GetCachedExtension(
      const ExtensionId& id,
      const std::string& package_hash,
      const base::Version& expected_version,
      bool manifest_fetch_failed);

  // Begins (or queues up) download of an updated extension. |info| represents
  // additional information about the extension update from the info field in
  // the update manifest.
  void FetchUpdatedExtension(std::unique_ptr<ExtensionFetch> fetch_data,
                             std::optional<std::string> info);

  // Called by RequestQueue when a new extension load request is started.
  void CreateExtensionLoader();
  void StartExtensionLoader();

  // Handles the result of a crx fetch.
  void OnExtensionLoadComplete(base::FilePath crx_path);

  void NotifyExtensionManifestUpdateCheckStatus(
      std::vector<UpdateManifestResult> results);

  // Invokes OnExtensionDownloadStageChanged() on the |delegate_| for each
  // extension in the set, with |stage| as the current stage. Make a copy of
  // arguments because there is no guarantee that callback won't indirectly
  // change source of IDs.
  void NotifyExtensionsDownloadStageChanged(
      ExtensionIdSet extension_ids,
      ExtensionDownloaderDelegate::Stage stage);

  // Calls NotifyExtensionsDownloadFailedWithFailureData with empty failure
  // data.
  void NotifyExtensionsDownloadFailed(ExtensionIdSet id_set,
                                      std::set<int> request_ids,
                                      ExtensionDownloaderDelegate::Error error);

  // Invokes OnExtensionDownloadFailed() on the |delegate_| for each extension
  // in the set, with |error| as the reason for failure, and failure data. Make
  // a copy of arguments because there is no guarantee that callback won't
  // indirectly change source of IDs.
  void NotifyExtensionsDownloadFailedWithFailureData(
      ExtensionIdSet extension_ids,
      std::set<int> request_ids,
      ExtensionDownloaderDelegate::Error error,
      const ExtensionDownloaderDelegate::FailureData& data);

  // Invokes OnExtensionDownloadFailed() on the |delegate_| for each extension
  // in the list, which also provides the reason for the failure. Make a copy
  // of arguments because there is no guarantee that callback won't indirectly
  // change source of them.
  void NotifyExtensionsDownloadFailedWithList(
      std::vector<std::pair<ExtensionDownloaderTask, DownloadFailure>> errors,
      std::set<int> request_ids);

  // Helper method to populate lists of manifest fetch requests.
  void AddToFetches(std::map<FetchDataGroupKey,
                             std::vector<std::unique_ptr<ManifestFetchData>>>&
                        fetches_preparing,
                    ExtensionDownloaderTask task);

  // Do real work of StartAllPending. If .crx cache is used, this function
  // is called when cache is ready.
  void DoStartAllPending();

  // Notify delegate and remove ping results.
  void NotifyDelegateDownloadFinished(
      std::unique_ptr<ExtensionFetch> fetch_data,
      bool from_cache,
      const base::FilePath& crx_path,
      bool file_ownership_passed);

  // Cached extension installation completed. If it was not successful, we will
  // try to download it from the web store using already fetched manifest.
  void CacheInstallDone(std::unique_ptr<ExtensionFetch> fetch_data,
                        bool installed);

  // Potentially updates an ExtensionFetch's authentication state and returns
  // |true| if the fetch should be retried. Returns |false| if the failure was
  // not related to authentication, leaving the ExtensionFetch data unmodified.
  bool IterateFetchCredentialsAfterFailure(ExtensionFetch* fetch,
                                           int response_code);

  void OnAccessTokenFetchComplete(GoogleServiceAuthError error,
                                  signin::AccessTokenInfo token_info);

  ManifestFetchData* CreateManifestFetchData(
      const GURL& update_url,
      int request_id,
      DownloadFetchPriority fetch_priority);

  // This function helps obtain an update (if any) from |possible_updates|.
  // |possible_indices| is an array of indices of |possible_updates| which
  // the function would check to find an update.
  // If the return value is |kAvailable|, |update_index_out| will store the
  // index of the update in |possible_updates|.
  UpdateAvailability GetUpdateAvailability(
      const ExtensionId& extension_id,
      const std::vector<const UpdateManifestResult*>& possible_candidates,
      UpdateManifestResult** update_result_out) const;

  // The delegate that receives the crx files downloaded by the
  // ExtensionDownloader, and that fills in optional ping and update url data.
  raw_ptr<ExtensionDownloaderDelegate> delegate_;

  // The URL loader factory to use for the SimpleURLLoaders.
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  // The URL loader factory exclusively used to load file:// URLs.
  mojo::Remote<network::mojom::URLLoaderFactory> file_url_loader_factory_;

  // The profile path used to load file:// URLs. It can be invalid.
  base::FilePath profile_path_for_url_loader_factory_;

  // List of update requests added to the downloader but not started yet.
  std::vector<ExtensionDownloaderTask> pending_tasks_;

  // Outstanding url loader requests for manifests and updates.
  std::unique_ptr<network::SimpleURLLoader> extension_loader_;
  std::unique_ptr<network::ResourceRequest> extension_loader_resource_request_;

  // Pending manifests and extensions to be fetched when the appropriate fetcher
  // is available.
  RequestQueue<ManifestFetchData> manifests_queue_;
  RequestQueue<ExtensionFetch> extensions_queue_;

  // Maps an extension-id to its PingResult data.
  std::map<ExtensionId, ExtensionDownloaderDelegate::PingResult> ping_results_;

  // Cache for .crx files.
  raw_ptr<ExtensionCache, DanglingUntriaged> extension_cache_;

  // May be used to fetch access tokens for protected download requests. May be
  // null. If non-null, guaranteed to outlive this object.
  raw_ptr<signin::IdentityManager> identity_manager_;

  // A Webstore download-scoped access token for the |identity_provider_|'s
  // active account, if any.
  std::string access_token_;

  // A pending access token fetcher.
  std::unique_ptr<signin::PrimaryAccountAccessTokenFetcher>
      access_token_fetcher_;

  // Brand code to include with manifest fetch queries if sending ping data.
  std::string brand_code_;

  // Baseline parameters to include with manifest fetch queries.
  std::string manifest_query_params_;

  // Domain to enable ping data. Ping data will be sent with manifest fetches
  // to update URLs which match this domain. Defaults to empty (no domain).
  std::string ping_enabled_domain_;

  net::HttpRequestHeaders
      last_extension_loader_resource_request_headers_for_testing_;
  int last_extension_loader_load_flags_for_testing_ = 0;

  crx_file::VerifierFormat crx_format_requirement_;

  // Used to create WeakPtrs to |this|.
  base::WeakPtrFactory<ExtensionDownloader> weak_ptr_factory_{this};
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_UPDATER_EXTENSION_DOWNLOADER_H_
