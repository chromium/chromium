// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/updater/extension_downloader.h"

#include <stddef.h>

#include <string_view>
#include <utility>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "base/version.h"
#include "components/crx_file/crx_verifier.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/primary_account_access_token_fetcher.h"
#include "components/signin/public/identity_manager/scope_set.h"
#include "components/update_client/update_query_params.h"
#include "content/public/browser/file_url_loader.h"
#include "content/public/browser/shared_cors_origin_access_list.h"
#include "extensions/browser/extension_file_task_runner.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/browser/updater/extension_cache.h"
#include "extensions/browser/updater/extension_downloader_delegate.h"
#include "extensions/browser/updater/extension_downloader_test_delegate.h"
#include "extensions/browser/updater/request_queue_impl.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/extension_updater_uma.h"
#include "extensions/common/extension_urls.h"
#include "extensions/common/verifier_formats.h"
#include "net/base/backoff_entry.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

using base::Time;
using update_client::UpdateQueryParams;

namespace extensions {

namespace {

const net::BackoffEntry::Policy kDefaultBackoffPolicy = {
    // Number of initial errors (in sequence) to ignore before applying
    // exponential back-off rules.
    0,

    // Initial delay for exponential back-off in ms.
    2000,

    // Factor by which the waiting time will be multiplied.
    2,

    // Fuzzing percentage. ex: 10% will spread requests randomly
    // between 90%-100% of the calculated time.
    0.1,

    // Maximum amount of time we are willing to delay our request in ms.
    600000,  // Ten minutes.

    // Time to keep an entry from being discarded even when it
    // has no significant state, -1 to never discard.
    -1,

    // Don't use initial delay unless the last request was an error.
    false,
};

const char kAuthUserQueryKey[] = "authuser";

const int kMaxAuthUserValue = 10;
const int kMaxOAuth2Attempts = 3;

const char kNotFromWebstoreInstallSource[] = "notfromwebstore";
const char kDefaultInstallSource[] = "";
const char kReinstallInstallSource[] = "reinstall";

const char kGoogleDotCom[] = "google.com";
const char kTokenServiceConsumerId[] = "extension_downloader";
const char kWebstoreOAuth2Scope[] =
    "https://www.googleapis.com/auth/chromewebstore.readonly";

ExtensionDownloader::TestObserver* g_test_observer = nullptr;
ExtensionDownloaderTestDelegate* g_test_delegate = nullptr;

#define RETRY_HISTOGRAM(name, retry_count, url)                           \
  if ((url).DomainIs(kGoogleDotCom)) {                                    \
    UMA_HISTOGRAM_CUSTOM_COUNTS("Extensions." name "RetryCountGoogleUrl", \
                                retry_count,                              \
                                1,                                        \
                                kMaxRetries,                              \
                                kMaxRetries + 1);                         \
  } else {                                                                \
    UMA_HISTOGRAM_CUSTOM_COUNTS("Extensions." name "RetryCountOtherUrl",  \
                                retry_count,                              \
                                1,                                        \
                                kMaxRetries,                              \
                                kMaxRetries + 1);                         \
  }

bool ShouldRetryRequest(const network::SimpleURLLoader* loader) {
  DCHECK(loader);

  // Since HTTP errors are now presented as ERR_HTTP_RESPONSE_CODE_FAILURE
  // by default, this will let both network and HTTP errors through.
  if (loader->NetError() == net::OK)
    return false;

  // If it failed without receiving response headers, retry.
  if (!loader->ResponseInfo() || !loader->ResponseInfo()->headers)
    return true;

  // If a response code was received, only retry on 5xx codes (server errors).
  int response_code = loader->ResponseInfo()->headers->response_code();
  return response_code >= 500 && response_code < 600;
}

// This parses and updates a URL query such that the value of the |authuser|
// query parameter is incremented by 1. If parameter was not present in the URL,
// it will be added with a value of 1. All other query keys and values are
// preserved as-is. Returns |false| if the user index exceeds a hard-coded
// maximum.
bool IncrementAuthUserIndex(GURL* url) {
  int user_index = 0;
  std::string old_query = url->query();
  std::vector<std::string> new_query_parts;
  url::Component query(0, old_query.length());
  url::Component key, value;
  while (url::ExtractQueryKeyValue(old_query, &query, &key, &value)) {
    std::string key_string = old_query.substr(key.begin, key.len);
    std::string value_string = old_query.substr(value.begin, value.len);
    if (key_string == kAuthUserQueryKey) {
      base::StringToInt(value_string, &user_index);
    } else {
      new_query_parts.push_back(base::StringPrintf(
          "%s=%s", key_string.c_str(), value_string.c_str()));
    }
  }
  if (user_index >= kMaxAuthUserValue)
    return false;
  new_query_parts.push_back(
      base::StringPrintf("%s=%d", kAuthUserQueryKey, user_index + 1));
  std::string new_query = base::JoinString(new_query_parts, "&");
  GURL::Replacements replacements;
  replacements.SetQueryStr(new_query);
  *url = url->ReplaceComponents(replacements);
  return true;
}

// This sanitizes update urls used to fetch update manifests for extensions.
std::optional<GURL> SanitizeUpdateURL(const ExtensionId& extension_id,
                                      const GURL& update_url) {
  if (update_url.is_empty()) {
    // Fill in default update URL.
    return extension_urls::GetWebstoreUpdateUrl();
  }

  // Skip extensions with non-empty invalid update URLs.
  if (!update_url.is_valid()) {
    DLOG(WARNING) << "Extension " << extension_id << " has invalid update url "
                  << update_url;
    return std::nullopt;
  }

  // Don't handle data URLs, since they don't support query parameters.
  if (update_url.SchemeIs(url::kDataScheme)) {
    DLOG(WARNING) << "Extension " << extension_id
                  << " has unsupported data: URI scheme in update url "
                  << update_url;
    return std::nullopt;
  }

  // Make sure we use SSL for store-hosted extensions.
  if (extension_urls::IsWebstoreUpdateUrl(update_url) &&
      !update_url.SchemeIsCryptographic()) {
    return extension_urls::GetWebstoreUpdateUrl();
  }

  return update_url;
}

}  // namespace

const char ExtensionDownloader::kUpdateInteractivityHeader[] =
    "X-Goog-Update-Interactivity";
const char ExtensionDownloader::kUpdateAppIdHeader[] = "X-Goog-Update-AppId";
const char ExtensionDownloader::kUpdateUpdaterHeader[] =
    "X-Goog-Update-Updater";

const char ExtensionDownloader::kUpdateInteractivityForeground[] = "fg";
const char ExtensionDownloader::kUpdateInteractivityBackground[] = "bg";

DownloadFailure::DownloadFailure(
    ExtensionDownloaderDelegate::Error error,
    ExtensionDownloaderDelegate::FailureData failure_data)
    : error(error), failure_data(failure_data) {}
DownloadFailure::DownloadFailure(DownloadFailure&&) = default;
DownloadFailure::~DownloadFailure() = default;

ExtensionDownloader::ExtensionFetch::ExtensionFetch(
    ExtensionDownloaderTask task,
    const GURL& url,
    const std::string& package_hash,
    const std::string& version,
    const DownloadFetchPriority fetch_priority)
    : id(task.id),
      url(url),
      package_hash(package_hash),
      version(version),
      fetch_priority(fetch_priority),
      credentials(CREDENTIALS_NONE),
      oauth2_attempt_count(0) {
  associated_tasks.emplace_back(std::move(task));
}

ExtensionDownloader::ExtensionFetch::~ExtensionFetch() = default;

std::set<int> ExtensionDownloader::ExtensionFetch::GetRequestIds() const {
  std::set<int> request_ids;
  for (const ExtensionDownloaderTask& task : associated_tasks)
    request_ids.insert(task.request_id);
  return request_ids;
}

ExtensionDownloader::FetchDataGroupKey::FetchDataGroupKey() = default;

ExtensionDownloader::FetchDataGroupKey::FetchDataGroupKey(
    const FetchDataGroupKey& other) = default;

ExtensionDownloader::FetchDataGroupKey::FetchDataGroupKey(
    const int request_id,
    const GURL& update_url,
    const bool is_force_installed)
    : request_id(request_id),
      update_url(update_url),
      is_force_installed(is_force_installed) {}

ExtensionDownloader::FetchDataGroupKey::~FetchDataGroupKey() = default;

bool ExtensionDownloader::FetchDataGroupKey::operator<(
    const FetchDataGroupKey& other) const {
  return std::tie(request_id, update_url, is_force_installed) <
         std::tie(other.request_id, other.update_url, other.is_force_installed);
}

ExtensionDownloader::ExtensionDownloader(
    ExtensionDownloaderDelegate* delegate,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const crx_file::VerifierFormat crx_format_requirement,
    const base::FilePath& profile_path)
    : delegate_(delegate),
      url_loader_factory_(std::move(url_loader_factory)),
      profile_path_for_url_loader_factory_(profile_path),
      manifests_queue_(
          kDefaultBackoffPolicy,
          base::BindRepeating(&ExtensionDownloader::CreateManifestLoader,
                              base::Unretained(this))),
      extensions_queue_(
          kDefaultBackoffPolicy,
          base::BindRepeating(&ExtensionDownloader::CreateExtensionLoader,
                              base::Unretained(this))),
      extension_cache_(nullptr),
      identity_manager_(nullptr),
      crx_format_requirement_(crx_format_requirement) {
  DCHECK(delegate_);
  DCHECK(url_loader_factory_);
}

ExtensionDownloader::~ExtensionDownloader() = default;

bool ExtensionDownloader::AddPendingExtension(ExtensionDownloaderTask task) {
  task.delegate = delegate_;
  task.OnStageChanged(ExtensionDownloaderDelegate::Stage::PENDING);
  return AddExtensionData(std::move(task));
}

void ExtensionDownloader::StartAllPending(ExtensionCache* cache) {
  if (cache) {
    extension_cache_ = cache;
    extension_cache_->Start(
        base::BindOnce(&ExtensionDownloader::DoStartAllPending,
                       weak_ptr_factory_.GetWeakPtr()));
  } else {
    DoStartAllPending();
  }
}

void ExtensionDownloader::AddToFetches(
    std::map<FetchDataGroupKey,
             std::vector<std::unique_ptr<ManifestFetchData>>>&
        fetches_preparing,
    ExtensionDownloaderTask task) {
  std::string install_source =
      extension_urls::IsWebstoreUpdateUrl(task.update_url)
          ? kDefaultInstallSource
          : kNotFromWebstoreInstallSource;
  if (task.is_corrupt_reinstall)
    install_source = kReinstallInstallSource;

  DownloadPingData ping_data;
  DownloadPingData* optional_ping_data = nullptr;
  if (delegate_->GetPingDataForExtension(task.id, &ping_data))
    optional_ping_data = &ping_data;

  // Find or create a ManifestFetchData to add this extension to.
  bool is_new_extension_force_installed =
      task.install_location == mojom::ManifestLocation::kExternalPolicyDownload;
  FetchDataGroupKey key(task.request_id, task.update_url,
                        is_new_extension_force_installed);
  auto existing_iter = fetches_preparing.find(key);
  if (existing_iter != fetches_preparing.end() &&
      !existing_iter->second.empty()) {
    // Try to add to the ManifestFetchData at the end of the list.
    ManifestFetchData* existing_fetch = existing_iter->second.back().get();
    if (existing_fetch->AddExtension(task.id, task.version.GetString(),
                                     optional_ping_data, task.update_url_data,
                                     install_source, task.install_location,
                                     task.fetch_priority)) {
      existing_fetch->AddAssociatedTask(std::move(task));
      return;
    }
  }
  // Otherwise add a new element to the list, if the list doesn't exist or
  // if its last element is already full.
  std::unique_ptr<ManifestFetchData> fetch(CreateManifestFetchData(
      task.update_url, task.request_id, task.fetch_priority));
  ManifestFetchData* fetch_ptr = fetch.get();
  if (is_new_extension_force_installed)
    fetch_ptr->set_is_all_external_policy_download();
  fetches_preparing[key].push_back(std::move(fetch));
  bool did_add = fetch_ptr->AddExtension(
      task.id, task.version.GetString(), optional_ping_data,
      task.update_url_data, install_source, task.install_location,
      task.fetch_priority);
  DCHECK(did_add);
  fetch_ptr->AddAssociatedTask(std::move(task));
}

void ExtensionDownloader::DoStartAllPending() {
  if (g_test_delegate) {
    g_test_delegate->StartUpdateCheck(this, delegate_,
                                      std::move(pending_tasks_));
    pending_tasks_.clear();
    return;
  }
  // We limit the number of extensions grouped together in one batch to avoid
  // running into the limits on the length of http GET requests, so there might
  // be multiple ManifestFetchData* objects with the same update_url.
  std::map<FetchDataGroupKey, std::vector<std::unique_ptr<ManifestFetchData>>>
      fetches_preparing;
  for (ExtensionDownloaderTask& task : pending_tasks_)
    AddToFetches(fetches_preparing, std::move(task));
  pending_tasks_.clear();

  for (auto& fetch_list : fetches_preparing) {
    for (auto& fetch : fetch_list.second)
      StartUpdateCheck(std::move(fetch));
  }
}

void ExtensionDownloader::SetIdentityManager(
    signin::IdentityManager* identity_manager) {
  identity_manager_ = identity_manager;
}

ExtensionDownloader::TestObserver::TestObserver() = default;
ExtensionDownloader::TestObserver::~TestObserver() = default;

// static
void ExtensionDownloader::set_test_observer(TestObserver* observer) {
  g_test_observer = observer;
}

// static
ExtensionDownloader::TestObserver* ExtensionDownloader::test_observer() {
  return g_test_observer;
}

// static
void ExtensionDownloader::set_test_delegate(
    ExtensionDownloaderTestDelegate* delegate) {
  g_test_delegate = delegate;
}

void ExtensionDownloader::SetBackoffPolicy(
    std::optional<net::BackoffEntry::Policy> backoff_policy) {
  manifests_queue_.set_backoff_policy(
      backoff_policy.value_or(kDefaultBackoffPolicy));
  extensions_queue_.set_backoff_policy(
      backoff_policy.value_or(kDefaultBackoffPolicy));
}

bool ExtensionDownloader::HasActiveManifestRequestForTesting() {
  return manifests_queue_.active_request();
}

ManifestFetchData* ExtensionDownloader::GetActiveManifestFetchForTesting() {
  return manifests_queue_.active_request();
}

bool ExtensionDownloader::AddExtensionData(ExtensionDownloaderTask task) {
  // Skip extensions with empty IDs.
  if (task.id.empty()) {
    DLOG(WARNING) << "Found extension with empty ID";
    task.OnStageChanged(ExtensionDownloaderDelegate::Stage::FINISHED);
    return false;
  }

  std::optional<GURL> sanitized_update_url =
      SanitizeUpdateURL(task.id, task.update_url);
  if (!sanitized_update_url) {
    task.OnStageChanged(ExtensionDownloaderDelegate::Stage::FINISHED);
    return false;
  }
  task.update_url = *sanitized_update_url;

  DCHECK(!task.update_url.is_empty());
  DCHECK(task.update_url.is_valid());

  pending_tasks_.push_back(std::move(task));

  return true;
}

void ExtensionDownloader::StartUpdateCheck(
    std::unique_ptr<ManifestFetchData> fetch_data) {
  const ExtensionIdSet extension_ids = fetch_data->GetExtensionIds();
  if (!ExtensionsBrowserClient::Get()->IsBackgroundUpdateAllowed()) {
    NotifyExtensionsDownloadStageChanged(
        extension_ids, ExtensionDownloaderDelegate::Stage::FINISHED);
    NotifyExtensionsDownloadFailed(
        extension_ids, fetch_data->request_ids(),
        ExtensionDownloaderDelegate::Error::DISABLED);
    return;
  }

  RequestQueue<ManifestFetchData>::iterator i;
  for (i = manifests_queue_.begin(); i != manifests_queue_.end(); ++i) {
    if (fetch_data->full_url() == i->full_url()) {
      // This url is already scheduled to be fetched.
      NotifyExtensionsDownloadStageChanged(
          extension_ids,
          ExtensionDownloaderDelegate::Stage::QUEUED_FOR_MANIFEST);
      i->Merge(std::move(fetch_data));
      return;
    }
  }

  // In case the request with the same URL is already in-flight, we'll merge
  // ours into it in `OnManifestLoadComplete`.

  NotifyExtensionsDownloadStageChanged(
      extension_ids, ExtensionDownloaderDelegate::Stage::QUEUED_FOR_MANIFEST);
  manifests_queue_.ScheduleRequest(std::move(fetch_data));
}

network::mojom::URLLoaderFactory* ExtensionDownloader::GetURLLoaderFactoryToUse(
    const GURL& url) {
  if (!url.SchemeIsFile()) {
    DCHECK(url_loader_factory_);
    return url_loader_factory_.get();
  }

  // For file:// URL support, since we only issue "no-cors" requests with this
  // factory, we can pass nullptr for the second argument.
  file_url_loader_factory_.Bind(content::CreateFileURLLoaderFactory(
      profile_path_for_url_loader_factory_,
      nullptr /* shared_cors_origin_access_list */));
  return file_url_loader_factory_.get();
}

void ExtensionDownloader::CreateManifestLoader() {
  const ManifestFetchData* active_request = manifests_queue_.active_request();
  const ExtensionIdSet extension_ids = active_request->GetExtensionIds();
  NotifyExtensionsDownloadStageChanged(
      extension_ids, ExtensionDownloaderDelegate::Stage::DOWNLOADING_MANIFEST);
  std::vector<std::string_view> id_vector(extension_ids.begin(),
                                          extension_ids.end());
  std::string id_list = base::JoinString(id_vector, ",");
  VLOG(2) << "Fetching " << active_request->full_url() << " for " << id_list;
  VLOG(2) << "Update interactivity: "
          << (active_request->foreground_check()
                  ? kUpdateInteractivityForeground
                  : kUpdateInteractivityBackground);

  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("extension_manifest_fetcher", R"(
        semantics {
          sender: "Extension Downloader"
          description:
            "Fetches information about an extension manifest (using its "
            "update_url, which is usually Chrome Web Store) in order to update "
            "the extension."
          trigger:
            "An update timer indicates that it's time to update extensions, or "
            "a user triggers an extension update flow."
          data:
            "The extension id, version and install source (the cause of the "
            "update flow). The client's OS, architecture, language, Chromium "
            "version, channel and a flag stating whether the request "
            "originated in the foreground or the background. Authentication is "
            "used only for non-Chrome-Web-Store update_urls."
          destination: WEBSITE
        }
        policy {
          cookies_allowed: YES
          cookies_store: "user"
          setting:
            "This feature cannot be disabled. It is only enabled when the user "
            "has installed extensions."
          chrome_policy {
            ExtensionInstallBlocklist {
              policy_options {mode: MANDATORY}
              ExtensionInstallBlocklist: {
                entries: '*'
              }
            }
          }
        })");
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = active_request->full_url(),
  resource_request->load_flags = net::LOAD_DISABLE_CACHE;

  if (active_request->fetch_priority() == DownloadFetchPriority::kForeground) {
    resource_request->priority = net::MEDIUM;
  }

  // Send traffic-management headers to the webstore, and omit credentials.
  // https://bugs.chromium.org/p/chromium/issues/detail?id=647516
  if (extension_urls::IsWebstoreUpdateUrl(active_request->full_url())) {
    resource_request->headers.SetHeader(kUpdateInteractivityHeader,
                                        active_request->foreground_check()
                                            ? kUpdateInteractivityForeground
                                            : kUpdateInteractivityBackground);
    resource_request->headers.SetHeader(kUpdateAppIdHeader, id_list);
    resource_request->headers.SetHeader(
        kUpdateUpdaterHeader,
        base::StringPrintf(
            "%s-%s", UpdateQueryParams::GetProdIdString(UpdateQueryParams::CRX),
            UpdateQueryParams::GetProdVersion().c_str()));
    resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  } else {
    // Non-webstore sources may require HTTP auth.
    resource_request->credentials_mode =
        network::mojom::CredentialsMode::kInclude;
    resource_request->site_for_cookies =
        net::SiteForCookies::FromUrl(active_request->full_url());
  }

  std::unique_ptr<network::SimpleURLLoader> loader =
      network::SimpleURLLoader::Create(std::move(resource_request),
                                       traffic_annotation);
  // Update checks can be interrupted if a network change is detected; this is
  // common for the retail mode AppPack on ChromeOS. Retrying once should be
  // enough to recover in those cases; let the fetcher retry up to 3 times
  // just in case. http://crosbug.com/130602
  loader->SetRetryOptions(
      3, network::SimpleURLLoader::RetryMode::RETRY_ON_NETWORK_CHANGE);

  network::mojom::URLLoaderFactory* url_loader_factory_to_use =
      GetURLLoaderFactoryToUse(active_request->full_url());
  // Loader will be owned by the callback, so we need a temporary reference to
  // avoid use after move.
  network::SimpleURLLoader* loader_reference = loader.get();
  loader_reference->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      url_loader_factory_to_use,
      base::BindOnce(&ExtensionDownloader::OnManifestLoadComplete,
                     weak_ptr_factory_.GetWeakPtr(), std::move(loader)));
}

void ExtensionDownloader::RetryManifestFetchRequest(
    RequestQueue<ManifestFetchData>::Request request,
    int network_error_code,
    int response_code) {
  constexpr base::TimeDelta backoff_delay;
  const ExtensionIdSet& extension_ids = request.fetch->GetExtensionIds();
  NotifyExtensionsDownloadStageChanged(
      extension_ids,
      ExtensionDownloaderDelegate::Stage::DOWNLOADING_MANIFEST_RETRY);
  const ExtensionDownloaderDelegate::FailureData failure_data =
      ExtensionDownloaderDelegate::FailureData::CreateFromNetworkResponse(
          network_error_code, response_code, request.failure_count());
  for (const ExtensionId& id : extension_ids)
    delegate_->OnExtensionDownloadRetry(id, failure_data);
  manifests_queue_.ScheduleRetriedRequest(std::move(request), backoff_delay);
}

void ExtensionDownloader::ReportManifestFetchFailure(
    ManifestFetchData* fetch_data,
    ExtensionDownloaderDelegate::Error error,
    const ExtensionDownloaderDelegate::FailureData& data) {
  const ExtensionIdSet extension_ids = fetch_data->GetExtensionIds();
  NotifyExtensionsDownloadStageChanged(
      extension_ids, ExtensionDownloaderDelegate::Stage::FINISHED);
  NotifyExtensionsDownloadFailedWithFailureData(
      extension_ids, fetch_data->request_ids(), error, data);
}

bool ExtensionDownloader::TryFetchingExtensionsFromCache(
    ManifestFetchData* fetch_data) {
  ExtensionIdSet extensions_fetched_from_cache;
  std::vector<ExtensionDownloaderTask> tasks_left;
  std::map<ExtensionId, std::optional<base::FilePath>> cache_results;
  for (ExtensionDownloaderTask& task : fetch_data->TakeAssociatedTasks()) {
    // In case when there are multiple requests for the same extension ID (they
    // could appear in the same fetch due to merging same URLs) ask the cache
    // only once.
    if (cache_results.count(task.id) == 0u) {
      // Extension is fetched here only in cases when we fail to fetch the
      // update manifest or parsing of update manifest failed. In such cases, we
      // don't have expected version and expected hash. Thus, passing empty hash
      // and version would not be a problem as we only check for the expected
      // hash and version if we have them.
      cache_results.emplace(
          task.id, GetCachedExtension(task.id, /*hash not fetched*/ "",
                                      /*version not fetched*/ base::Version(),
                                      /*manifest_fetch_failed*/ true));
    }
    std::optional<base::FilePath>& cached_crx_path = cache_results[task.id];
    if (cached_crx_path) {
      const ExtensionId id = task.id;
      // TODO(https://crbug.com/981891#c30) The finished downloading stage will
      // be reported only once for all download requests for that extension.
      // Change this when the tracker will care about different requests, not
      // about extension ID in general.
      delegate_->OnExtensionDownloadStageChanged(
          id, ExtensionDownloaderDelegate::Stage::FINISHED);
      auto extension_fetch_data(std::make_unique<ExtensionFetch>(
          std::move(task), fetch_data->base_url(), /*hash not fetched*/ "",
          /*version not fetched*/ "", fetch_data->fetch_priority()));
      NotifyDelegateDownloadFinished(std::move(extension_fetch_data), true,
                                     cached_crx_path.value(), false);
      extensions_fetched_from_cache.insert(id);
    } else {
      tasks_left.emplace_back(std::move(task));
    }
  }
  bool all_found = tasks_left.empty();
  fetch_data->RemoveExtensions(extensions_fetched_from_cache,
                               manifest_query_params_);
  // Re-add tasks which weren't found in cache for continued processing.
  for (ExtensionDownloaderTask& task : tasks_left)
    fetch_data->AddAssociatedTask(std::move(task));
  return all_found;
}

void ExtensionDownloader::RetryRequestOrHandleFailureOnManifestFetchFailure(
    RequestQueue<ManifestFetchData>::Request request,
    const network::SimpleURLLoader& loader,
    const int response_code) {
  bool all_force_installed_extensions =
      request.fetch->is_all_external_policy_download();

  const int net_error = loader.NetError();
  const int request_failure_count = request.failure_count();
  // If the device is offline, do not retry for force installed extensions,
  // try installing it from cache. Try fetching from cache only on first attempt
  // in this case, because we will retry the request only if there was no entry
  // in cache corresponding to this extension and there is no point in trying to
  // fetch extension from cache again.
  if (net_error == net::ERR_INTERNET_DISCONNECTED &&
      all_force_installed_extensions && request_failure_count == 0) {
    if (!TryFetchingExtensionsFromCache(request.fetch.get()))
      RetryManifestFetchRequest(std::move(request), net_error, response_code);
    return;
  }
  if (ShouldRetryRequest(&loader) && request_failure_count < kMaxRetries) {
    RetryManifestFetchRequest(std::move(request), loader.NetError(),
                              response_code);
    return;
  }
  const GURL url = loader.GetFinalURL();
  RETRY_HISTOGRAM("ManifestFetchFailure", request_failure_count, url);
  if (all_force_installed_extensions) {
    if (TryFetchingExtensionsFromCache(request.fetch.get()))
      return;
    const ExtensionDownloaderDelegate::FailureData failure_data =
        ExtensionDownloaderDelegate::FailureData::CreateFromNetworkResponse(
            net_error, response_code, request.failure_count());
    ReportManifestFetchFailure(
        request.fetch.get(),
        ExtensionDownloaderDelegate::Error::MANIFEST_FETCH_FAILED,
        failure_data);
  } else {
    const ExtensionDownloaderDelegate::FailureData failure_data =
        ExtensionDownloaderDelegate::FailureData::CreateFromNetworkResponse(
            net_error, response_code, request_failure_count);
    ReportManifestFetchFailure(
        request.fetch.get(),
        ExtensionDownloaderDelegate::Error::MANIFEST_FETCH_FAILED,
        failure_data);
  }
}

void ExtensionDownloader::OnManifestLoadComplete(
    std::unique_ptr<network::SimpleURLLoader> loader,
    std::unique_ptr<std::string> response_body) {
  const GURL url = loader->GetFinalURL();
  DCHECK(loader);

  RequestQueue<ManifestFetchData>::Request request =
      manifests_queue_.reset_active_request();

  // There is a chance that some other request has exactly some URL, so we won't
  // fetch it twice.
  std::vector<std::unique_ptr<ManifestFetchData>> duplicates =
      manifests_queue_.erase_if(base::BindRepeating(
          [](const GURL& full_url, const ManifestFetchData& fetch) {
            return fetch.full_url() == full_url;
          },
          request.fetch->full_url()));
  for (std::unique_ptr<ManifestFetchData>& fetch : duplicates) {
    NotifyExtensionsDownloadStageChanged(
        fetch->GetExtensionIds(),
        ExtensionDownloaderDelegate::Stage::DOWNLOADING_MANIFEST);
    request.fetch->Merge(std::move(fetch));
  }

  int response_code = -1;
  if (loader->ResponseInfo() && loader->ResponseInfo()->headers)
    response_code = loader->ResponseInfo()->headers->response_code();

  VLOG(2) << response_code << " " << url;

  const int request_failure_count = request.failure_count();

  // We want to try parsing the manifest, and if it indicates updates are
  // available, we want to fire off requests to fetch those updates.
  if (response_body && !response_body->empty()) {
    RETRY_HISTOGRAM("ManifestFetchSuccess", request_failure_count, url);
    VLOG(2) << "beginning manifest parse for " << url;
    NotifyExtensionsDownloadStageChanged(
        request.fetch->GetExtensionIds(),
        ExtensionDownloaderDelegate::Stage::PARSING_MANIFEST);
    auto callback = base::BindOnce(&ExtensionDownloader::HandleManifestResults,
                                   weak_ptr_factory_.GetWeakPtr(),
                                   std::move(request.fetch));
    ParseUpdateManifest(*response_body, std::move(callback));
  } else {
    VLOG(1) << "Failed to fetch manifest '" << url.possibly_invalid_spec()
            << "' response code:" << response_code;
    RetryRequestOrHandleFailureOnManifestFetchFailure(std::move(request),
                                                      *loader, response_code);
  }
  file_url_loader_factory_.reset();

  // If we have any pending manifest requests, fire off the next one.
  manifests_queue_.StartNextRequest();
}

void ExtensionDownloader::HandleManifestResults(
    std::unique_ptr<ManifestFetchData> fetch_data,
    std::unique_ptr<UpdateManifestResults> results,
    const std::optional<ManifestParseFailure>& error) {
  if (!results) {
    VLOG(2) << "parsing manifest failed (" << fetch_data->full_url() << ")";
    DCHECK(error.has_value());
    if (TryFetchingExtensionsFromCache(fetch_data.get()))
      return;
    // If not all extension were found in the cache, collect them and report
    // failure.
    const ExtensionIdSet extension_ids = fetch_data->GetExtensionIds();
    std::vector<std::pair<ExtensionDownloaderTask, DownloadFailure>>
        manifest_invalid_errors;
    manifest_invalid_errors.reserve(extension_ids.size());
    for (ExtensionDownloaderTask& task : fetch_data->TakeAssociatedTasks()) {
      manifest_invalid_errors.emplace_back(
          std::move(task),
          DownloadFailure(
              ExtensionDownloaderDelegate::Error::MANIFEST_INVALID,
              ExtensionDownloaderDelegate::FailureData(error.value().error)));
    }
    NotifyExtensionsDownloadStageChanged(
        extension_ids, ExtensionDownloaderDelegate::Stage::FINISHED);
    NotifyExtensionsDownloadFailedWithList(std::move(manifest_invalid_errors),
                                           fetch_data->request_ids());
    return;
  } else {
    VLOG(2) << "parsing manifest succeeded (" << fetch_data->full_url() << ")";
  }

  const ExtensionIdSet extension_ids = fetch_data->GetExtensionIds();
  NotifyExtensionsDownloadStageChanged(
      extension_ids, ExtensionDownloaderDelegate::Stage::MANIFEST_LOADED);

  std::vector<std::pair<ExtensionDownloaderTask, UpdateManifestResult*>>
      to_update;
  std::vector<std::pair<ExtensionDownloaderTask, DownloadFailure>> failures;

  // Examine the parsed manifest and kick off fetches of any new crx files.
  // NOTE: This transfers ownership on tasks to the DetermineUpdates method,
  // which returns tasks back via its output arguments.
  DetermineUpdates(fetch_data->TakeAssociatedTasks(), *results, &to_update,
                   &failures);
  for (auto& update : to_update) {
    const ExtensionId& extension_id = update.first.id;

    GURL crx_url = update.second->crx_url;

    if (g_test_observer) {
      g_test_observer->OnExtensionUpdateFound(
          extension_id, fetch_data->request_ids(),
          base::Version(update.second->version));
    }
    delegate_->OnExtensionUpdateFound(extension_id, fetch_data->request_ids(),
                                      base::Version(update.second->version));

    if (fetch_data->is_all_external_policy_download() && crx_url.is_empty()) {
      DCHECK_EQ(fetch_data->fetch_priority(),
                DownloadFetchPriority::kForeground);
    }
    FetchUpdatedExtension(
        std::make_unique<ExtensionFetch>(
            std::move(update.first), crx_url, update.second->package_hash,
            update.second->version, fetch_data->fetch_priority()),
        update.second->info);
  }

  // If the manifest response included a <daystart> element, we want to save
  // that value for any extensions which had sent a ping in the request.
  if (fetch_data->base_url().DomainIs(kGoogleDotCom) &&
      results->daystart_elapsed_seconds >= 0) {
    Time day_start =
        Time::Now() - base::Seconds(results->daystart_elapsed_seconds);

    for (const ExtensionId& id : extension_ids) {
      ExtensionDownloaderDelegate::PingResult& result = ping_results_[id];
      result.did_ping = fetch_data->DidPing(id, ManifestFetchData::ROLLCALL);
      result.day_start = day_start;
    }
  }

  ExtensionIdSet extension_ids_with_errors;
  for (const auto& failure : failures)
    extension_ids_with_errors.insert(failure.first.id);
  NotifyExtensionsDownloadStageChanged(
      extension_ids_with_errors, ExtensionDownloaderDelegate::Stage::FINISHED);
  NotifyExtensionsDownloadFailedWithList(std::move(failures),
                                         fetch_data->request_ids());
}

ExtensionDownloader::UpdateAvailability
ExtensionDownloader::GetUpdateAvailability(
    const ExtensionId& extension_id,
    const std::vector<const UpdateManifestResult*>& possible_candidates,
    UpdateManifestResult** update_result_out) const {
  const bool is_extension_pending = delegate_->IsExtensionPending(extension_id);
  std::string extension_version;
  if (!is_extension_pending) {
    // If we're not installing pending extension, we can only update
    // extensions that have already existed in the system.
    if (!delegate_->GetExtensionExistingVersion(extension_id,
                                                &extension_version)) {
      VLOG(2) << extension_id << " is not installed";
      return UpdateAvailability::kBadUpdateSpecification;
    }
    VLOG(2) << extension_id << " is at '" << extension_version << "'";
  }

  bool has_noupdate = false;
  for (const UpdateManifestResult* update : possible_candidates) {
    const std::string& update_version_str = update->version;
    if (VLOG_IS_ON(2)) {
      if (update_version_str.empty())
        VLOG(2) << "Manifest indicates " << extension_id
                << " has no update (info: " << update->info.value_or("no info")
                << ")";
      else
        VLOG(2) << "Manifest indicates " << extension_id
                << " latest version is '" << update_version_str << "'";
    }

    if (!is_extension_pending) {
      // If we're not installing pending extension, and the update
      // version is the same or older than what's already installed,
      // we don't want it.
      if (update_version_str.empty()) {
        // If update manifest doesn't have version number => no update.
        VLOG(2) << extension_id << " has empty version";
        has_noupdate = true;
        continue;
      }

      const base::Version update_version(update_version_str);
      if (!update_version.IsValid()) {
        VLOG(2) << extension_id << " has invalid version '"
                << update_version_str << "'";
        continue;
      }

      const base::Version existing_version(extension_version);
      if (update_version.CompareTo(existing_version) <= 0) {
        VLOG(2) << extension_id << " version is not older than '"
                << update_version_str << "'";
        bool can_rollback =
            update_version.CompareTo(existing_version) < 0 &&
            (delegate_->RequestRollback(extension_id) ==
             ExtensionDownloaderDelegate::RequestRollbackResult::kAllowed);
        if (!can_rollback) {
          has_noupdate = true;
          continue;
        }
      }
    }

    // If the update specifies a browser minimum version, do we qualify?
    if (update->browser_min_version.length() > 0 &&
        !ExtensionsBrowserClient::Get()->IsMinBrowserVersionSupported(
            update->browser_min_version)) {
      // TODO(asargent) - We may want this to show up in the extensions UI
      // eventually. (http://crbug.com/12547).
      DLOG(WARNING) << "Updated version of extension " << extension_id
                    << " available, but requires chrome version "
                    << update->browser_min_version;
      has_noupdate = true;
      continue;
    }

    // Stop checking as soon as an update for |extension_id| is found.
    VLOG(2) << "Will try to update " << extension_id;
    *update_result_out = const_cast<UpdateManifestResult*>(update);
    return UpdateAvailability::kAvailable;
  }
  return has_noupdate ? UpdateAvailability::kNoUpdate
                      : UpdateAvailability::kBadUpdateSpecification;
}

void ExtensionDownloader::DetermineUpdates(
    std::vector<ExtensionDownloaderTask> tasks,
    const UpdateManifestResults& possible_updates,
    std::vector<std::pair<ExtensionDownloaderTask, UpdateManifestResult*>>*
        to_update,
    std::vector<std::pair<ExtensionDownloaderTask, DownloadFailure>>*
        failures) {
  DCHECK_NE(nullptr, to_update);
  DCHECK_NE(nullptr, failures);

  // Group successful possible updates by extension IDs.
  const std::map<std::string, std::vector<const UpdateManifestResult*>>
      update_groups = possible_updates.GroupSuccessfulByID();

  // Contains IDs of extensions which neither have successful update entry nor
  // are already inserted into |errors|.
  std::map<ExtensionId, ExtensionDownloaderTask> extension_errors;

  // For each extensions in the current batch, greedily find an update from
  // |possible_updates|.
  for (ExtensionDownloaderTask& task : tasks) {
    const ExtensionId& extension_id = task.id;
    const auto it = update_groups.find(extension_id);
    if (it == update_groups.end()) {
      VLOG(2) << "Manifest doesn't have an update entry for " << extension_id;
      extension_errors.emplace(extension_id, std::move(task));
      continue;
    }

    const std::vector<const UpdateManifestResult*>& possible_candidates =
        it->second;
    DCHECK(!possible_candidates.empty());

    VLOG(2) << "Manifest has " << possible_candidates.size()
            << " update entries for " << extension_id;

    UpdateManifestResult* update_result = nullptr;
    UpdateAvailability update_availability = GetUpdateAvailability(
        extension_id, possible_candidates, &update_result);

    switch (update_availability) {
      case UpdateAvailability::kAvailable:
        DCHECK_NE(nullptr, update_result);
        to_update->emplace_back(std::move(task), update_result);
        break;
      case UpdateAvailability::kNoUpdate:
        failures->emplace_back(
            std::move(task),
            DownloadFailure(
                ExtensionDownloaderDelegate::Error::NO_UPDATE_AVAILABLE,
                ExtensionDownloaderDelegate::FailureData()));
        break;
      case UpdateAvailability::kBadUpdateSpecification:
        failures->emplace_back(
            std::move(task),
            DownloadFailure(
                ExtensionDownloaderDelegate::Error::MANIFEST_INVALID,
                ExtensionDownloaderDelegate::FailureData(
                    ManifestInvalidError::BAD_UPDATE_SPECIFICATION)));
        break;
    }
  }
  for (const auto& possible_update : possible_updates.update_list) {
    const ExtensionId& id = possible_update.extension_id;
    auto error = extension_errors.find(id);
    if (error == extension_errors.end())
      continue;
    DCHECK(possible_update.parse_error);
    ManifestInvalidError error_type = possible_update.parse_error.value().error;
    // Report any error corresponding to an extension.
    failures->emplace_back(
        std::move(error->second),
        DownloadFailure(
            ExtensionDownloaderDelegate::Error::MANIFEST_INVALID,
            error_type == ManifestInvalidError::BAD_APP_STATUS
                ? ExtensionDownloaderDelegate::FailureData(
                      error_type, possible_update.app_status)
                : ExtensionDownloaderDelegate::FailureData(error_type)));
    extension_errors.erase(id);
  }
  // For the remaining extensions, we have missing ids.
  for (auto& error : extension_errors) {
    failures->emplace_back(
        std::move(error.second),
        DownloadFailure(ExtensionDownloaderDelegate::Error::MANIFEST_INVALID,
                        ExtensionDownloaderDelegate::FailureData(
                            ManifestInvalidError::MISSING_APP_ID)));
  }
}

std::optional<base::FilePath> ExtensionDownloader::GetCachedExtension(
    const ExtensionId& id,
    const std::string& package_hash,
    const base::Version& expected_version,
    bool manifest_fetch_failed) {
  if (!extension_cache_) {
    delegate_->OnExtensionDownloadCacheStatusRetrieved(
        id, ExtensionDownloaderDelegate::CacheStatus::CACHE_DISABLED);
    return std::nullopt;
  }

  std::string version;
  if (!extension_cache_->GetExtension(id, package_hash, nullptr, &version)) {
    delegate_->OnExtensionDownloadCacheStatusRetrieved(
        id, ExtensionDownloaderDelegate::CacheStatus::CACHE_MISS);
    return std::nullopt;
  }
  // If manifest fetch is failed, we need not verify the version of the cache as
  // we will try to install the version present in the cache.
  if (!manifest_fetch_failed && expected_version != base::Version(version)) {
    delegate_->OnExtensionDownloadCacheStatusRetrieved(
        id, ExtensionDownloaderDelegate::CacheStatus::CACHE_OUTDATED);
    return std::nullopt;
  }

  delegate_->OnExtensionDownloadCacheStatusRetrieved(
      id, manifest_fetch_failed
              ? ExtensionDownloaderDelegate::CacheStatus::
                    CACHE_HIT_ON_MANIFEST_FETCH_FAILURE
              : ExtensionDownloaderDelegate::CacheStatus::CACHE_HIT);

  base::FilePath crx_path;
  // Now get .crx file path.
  // TODO(https://crbug.com/1018271#c2) This has a  side-effect in extension
  // cache implementation: extension in the cache will be marked as recently
  // used.
  extension_cache_->GetExtension(id, package_hash, &crx_path, &version);
  return std::move(crx_path);
}

// Begins (or queues up) download of an updated extension.
void ExtensionDownloader::FetchUpdatedExtension(
    std::unique_ptr<ExtensionFetch> fetch_data,
    std::optional<std::string> info) {
  if (!fetch_data->url.is_valid()) {
    // TODO(asargent): This can sometimes be invalid. See crbug.com/130881.
    DLOG(WARNING) << "Invalid URL: '" << fetch_data->url.possibly_invalid_spec()
                  << "' for extension " << fetch_data->id;
    delegate_->OnExtensionDownloadStageChanged(
        fetch_data->id, ExtensionDownloaderDelegate::Stage::FINISHED);
    if (fetch_data->url.is_empty()) {
      // We expect to receive initialised |info| from the manifest parser in
      // case of no updates status in the update manifest.
      ExtensionDownloaderDelegate::FailureData data(info.value_or(""));
      NotifyExtensionsDownloadFailedWithFailureData(
          {fetch_data->id}, fetch_data->GetRequestIds(),
          ExtensionDownloaderDelegate::Error::CRX_FETCH_URL_EMPTY, data);
    } else {
      NotifyExtensionsDownloadFailed(
          {fetch_data->id}, fetch_data->GetRequestIds(),
          ExtensionDownloaderDelegate::Error::CRX_FETCH_URL_INVALID);
    }
    return;
  }

  for (RequestQueue<ExtensionFetch>::iterator iter = extensions_queue_.begin();
       iter != extensions_queue_.end();
       ++iter) {
    if (iter->id == fetch_data->id || iter->url == fetch_data->url) {
      delegate_->OnExtensionDownloadStageChanged(
          fetch_data->id, ExtensionDownloaderDelegate::Stage::QUEUED_FOR_CRX);
      iter->associated_tasks.insert(
          iter->associated_tasks.end(),
          std::make_move_iterator(fetch_data->associated_tasks.begin()),
          std::make_move_iterator(fetch_data->associated_tasks.end()));
      return;  // already scheduled
    }
  }

  // In case the request with the same URL is already in-flight, we'll merge
  // ours into it in `OnExtensionLoadComplete`.

  std::optional<base::FilePath> cached_crx_path =
      GetCachedExtension(fetch_data->id, fetch_data->package_hash,
                         fetch_data->version, /*manifest_fetch_failed*/ false);
  if (cached_crx_path) {
    delegate_->OnExtensionDownloadStageChanged(
        fetch_data->id, ExtensionDownloaderDelegate::Stage::FINISHED);
    NotifyDelegateDownloadFinished(std::move(fetch_data), true,
                                   cached_crx_path.value(), false);
  } else {
    delegate_->OnExtensionDownloadStageChanged(
        fetch_data->id, ExtensionDownloaderDelegate::Stage::QUEUED_FOR_CRX);
    extensions_queue_.ScheduleRequest(std::move(fetch_data));
  }
}

void ExtensionDownloader::NotifyDelegateDownloadFinished(
    std::unique_ptr<ExtensionFetch> fetch_data,
    bool from_cache,
    const base::FilePath& crx_path,
    bool file_ownership_passed) {
  // Dereference required params before passing a scoped_ptr.
  const ExtensionId& id = fetch_data->id;
  const std::string& package_hash = fetch_data->package_hash;
  const GURL& url = fetch_data->url;
  const base::Version& version = fetch_data->version;
  const std::set<int> request_ids = fetch_data->GetRequestIds();
  const crx_file::VerifierFormat required_format =
      extension_urls::IsWebstoreUpdateUrl(fetch_data->url)
          ? GetWebstoreVerifierFormat(false)
          : crx_format_requirement_;
  CRXFileInfo crx_info(crx_path, required_format);
  crx_info.expected_hash = package_hash;
  crx_info.extension_id = id;
  crx_info.expected_version = version;
  delegate_->OnExtensionDownloadFinished(
      crx_info, file_ownership_passed, url, ping_results_[id], request_ids,
      from_cache ? base::BindOnce(&ExtensionDownloader::CacheInstallDone,
                                  weak_ptr_factory_.GetWeakPtr(),
                                  std::move(fetch_data))
                 : ExtensionDownloaderDelegate::InstallCallback());
  if (!from_cache)
    ping_results_.erase(id);
}

void ExtensionDownloader::CacheInstallDone(
    std::unique_ptr<ExtensionFetch> fetch_data,
    bool should_download) {
  ping_results_.erase(fetch_data->id);
  if (should_download) {
    // Resume download from cached manifest data.
    extensions_queue_.ScheduleRequest(std::move(fetch_data));
  }
}

void ExtensionDownloader::CreateExtensionLoader() {
  const ExtensionFetch* fetch = extensions_queue_.active_request();
  delegate_->OnExtensionDownloadStageChanged(
      fetch->id, ExtensionDownloaderDelegate::Stage::DOWNLOADING_CRX);
  extension_loader_resource_request_ =
      std::make_unique<network::ResourceRequest>();
  extension_loader_resource_request_->url = fetch->url;

  int load_flags = net::LOAD_DISABLE_CACHE;
  bool is_secure = fetch->url.SchemeIsCryptographic();
  extension_loader_resource_request_->load_flags = load_flags;
  if (fetch->credentials != ExtensionFetch::CREDENTIALS_COOKIES || !is_secure) {
    extension_loader_resource_request_->credentials_mode =
        network::mojom::CredentialsMode::kOmit;
  } else {
    extension_loader_resource_request_->site_for_cookies =
        net::SiteForCookies::FromUrl(fetch->url);
  }

  if (fetch->credentials == ExtensionFetch::CREDENTIALS_OAUTH2_TOKEN &&
      is_secure) {
    if (access_token_.empty()) {
      // We should try OAuth2, but we have no token cached. This
      // ExtensionLoader will be started once the token fetch is complete,
      // in either OnTokenFetchSuccess or OnTokenFetchFailure.
      DCHECK(identity_manager_);
      signin::ScopeSet webstore_scopes;
      webstore_scopes.insert(kWebstoreOAuth2Scope);
      // It is safe to use Unretained(this) here given that the callback
      // will not be invoked if this object is deleted.
      access_token_fetcher_ =
          std::make_unique<signin::PrimaryAccountAccessTokenFetcher>(
              kTokenServiceConsumerId, identity_manager_, webstore_scopes,
              base::BindOnce(&ExtensionDownloader::OnAccessTokenFetchComplete,
                             base::Unretained(this)),
              signin::PrimaryAccountAccessTokenFetcher::Mode::kImmediate,
              signin::ConsentLevel::kSync);
      return;
    }
    extension_loader_resource_request_->headers.SetHeader(
        net::HttpRequestHeaders::kAuthorization,
        base::StringPrintf("Bearer %s", access_token_.c_str()));
  }

  VLOG(2) << "Starting load of " << fetch->url << " for " << fetch->id;

  StartExtensionLoader();
}

void ExtensionDownloader::StartExtensionLoader() {
  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("extension_crx_fetcher", R"(
        semantics {
          sender: "Extension Downloader"
          description:
            "Downloads an extension's crx file in order to update the "
            "extension, using update_url from the extension's manifest which "
            "is usually Chrome WebStore."
          trigger:
            "An update check indicates an extension update is available."
          data:
            "URL and required data to specify the extension to download. "
            "OAuth2 token is also sent if connection is secure and to Google."
          destination: WEBSITE
        }
        policy {
          cookies_allowed: YES
          cookies_store: "user"
          setting:
            "This feature cannot be disabled. It is only enabled when the user "
            "has installed extensions and it needs updating."
          chrome_policy {
            ExtensionInstallBlocklist {
              policy_options {mode: MANDATORY}
              ExtensionInstallBlocklist: {
                entries: '*'
              }
            }
          }
        })");

  last_extension_loader_resource_request_headers_for_testing_ =
      extension_loader_resource_request_->headers;
  last_extension_loader_load_flags_for_testing_ =
      extension_loader_resource_request_->load_flags;

  const ExtensionFetch* active_request = extensions_queue_.active_request();
  if (active_request->fetch_priority == DownloadFetchPriority::kForeground) {
    extension_loader_resource_request_->priority = net::MEDIUM;
  }

  network::mojom::URLLoaderFactory* url_loader_factory_to_use =
      GetURLLoaderFactoryToUse(extension_loader_resource_request_->url);
  extension_loader_ = network::SimpleURLLoader::Create(
      std::move(extension_loader_resource_request_), traffic_annotation);
  // Retry up to 3 times.
  extension_loader_->SetRetryOptions(
      3, network::SimpleURLLoader::RetryMode::RETRY_ON_NETWORK_CHANGE);

  extension_loader_->DownloadToTempFile(
      url_loader_factory_to_use,
      base::BindOnce(&ExtensionDownloader::OnExtensionLoadComplete,
                     base::Unretained(this)));
}

void ExtensionDownloader::OnExtensionLoadComplete(base::FilePath crx_path) {
  // There is a chance that some other request has exactly some URL, so we won't
  // fetch it twice.
  std::vector<std::unique_ptr<ExtensionFetch>> duplicates =
      extensions_queue_.erase_if(base::BindRepeating(
          [](const ExtensionFetch* active_fetch, const ExtensionFetch& fetch) {
            return fetch.url == active_fetch->url;
          },
          extensions_queue_.active_request()));
  for (std::unique_ptr<ExtensionFetch>& fetch : duplicates) {
    delegate_->OnExtensionDownloadStageChanged(
        fetch->id, ExtensionDownloaderDelegate::Stage::DOWNLOADING_CRX);
    extensions_queue_.active_request()->associated_tasks.insert(
        extensions_queue_.active_request()->associated_tasks.end(),
        std::make_move_iterator(fetch->associated_tasks.begin()),
        std::make_move_iterator(fetch->associated_tasks.end()));
  }
  GURL url = extension_loader_->GetFinalURL();
  int net_error = extension_loader_->NetError();
  int response_code = -1;
  if (extension_loader_->ResponseInfo() &&
      extension_loader_->ResponseInfo()->headers) {
    response_code = extension_loader_->ResponseInfo()->headers->response_code();
  }
  const base::TimeDelta& backoff_delay = base::Milliseconds(0);

  ExtensionFetch& active_request = *extensions_queue_.active_request();
  const ExtensionId& id = active_request.id;
  if (!crx_path.empty()) {
    RETRY_HISTOGRAM("CrxFetchSuccess",
                    extensions_queue_.active_request_failure_count(),
                    url);
    std::unique_ptr<ExtensionFetch> fetch_data =
        std::move(extensions_queue_.reset_active_request().fetch);
    delegate_->OnExtensionDownloadStageChanged(
        id, ExtensionDownloaderDelegate::Stage::FINISHED);
    NotifyDelegateDownloadFinished(std::move(fetch_data), false, crx_path,
                                   true);
  } else if (IterateFetchCredentialsAfterFailure(&active_request,
                                                 response_code)) {
    const ExtensionDownloaderDelegate::FailureData failure_data =
        ExtensionDownloaderDelegate::FailureData::CreateFromNetworkResponse(
            net_error, response_code,
            extensions_queue_.active_request_failure_count());
    delegate_->OnExtensionDownloadRetry(id, failure_data);
    delegate_->OnExtensionDownloadStageChanged(
        id, ExtensionDownloaderDelegate::Stage::DOWNLOADING_CRX_RETRY);
    extensions_queue_.RetryRequest(backoff_delay);
    delegate_->OnExtensionDownloadRetryForTests();
  } else {
    const std::set<int> request_ids = active_request.GetRequestIds();
    const ExtensionDownloaderDelegate::PingResult& ping = ping_results_[id];
    VLOG(1) << "Failed to fetch extension '" << url.possibly_invalid_spec()
            << "' response code:" << response_code;
    if (ShouldRetryRequest(extension_loader_.get()) &&
        extensions_queue_.active_request_failure_count() < kMaxRetries) {
      const ExtensionDownloaderDelegate::FailureData failure_data =
          ExtensionDownloaderDelegate::FailureData::CreateFromNetworkResponse(
              net_error, response_code,
              extensions_queue_.active_request_failure_count());
      delegate_->OnExtensionDownloadRetry(id, failure_data);
      delegate_->OnExtensionDownloadStageChanged(
          id, ExtensionDownloaderDelegate::Stage::DOWNLOADING_CRX_RETRY);
      extensions_queue_.RetryRequest(backoff_delay);
      delegate_->OnExtensionDownloadRetryForTests();
    } else {
      RETRY_HISTOGRAM("CrxFetchFailure",
                      extensions_queue_.active_request_failure_count(),
                      url);
      delegate_->OnExtensionDownloadStageChanged(
          id, ExtensionDownloaderDelegate::Stage::FINISHED);
      const ExtensionDownloaderDelegate::FailureData failure_data =
          ExtensionDownloaderDelegate::FailureData::CreateFromNetworkResponse(
              net_error, response_code,
              extensions_queue_.active_request_failure_count());
      delegate_->OnExtensionDownloadFailed(
          id, ExtensionDownloaderDelegate::Error::CRX_FETCH_FAILED, ping,
          request_ids, failure_data);
    }
    ping_results_.erase(id);
    if (extensions_queue_.active_request())
      extensions_queue_.reset_active_request();
  }

  extension_loader_.reset();
  file_url_loader_factory_.reset();

  // If there are any pending downloads left, start the next one.
  extensions_queue_.StartNextRequest();
}

void ExtensionDownloader::NotifyExtensionsDownloadStageChanged(
    ExtensionIdSet extension_ids,
    ExtensionDownloaderDelegate::Stage stage) {
  for (const auto& it : extension_ids) {
    delegate_->OnExtensionDownloadStageChanged(it, stage);
  }
}

void ExtensionDownloader::NotifyExtensionsDownloadFailed(
    ExtensionIdSet extension_ids,
    std::set<int> request_ids,
    ExtensionDownloaderDelegate::Error error) {
  NotifyExtensionsDownloadFailedWithFailureData(
      std::move(extension_ids), std::move(request_ids), error,
      ExtensionDownloaderDelegate::FailureData());
}

void ExtensionDownloader::NotifyExtensionsDownloadFailedWithFailureData(
    ExtensionIdSet extension_ids,
    std::set<int> request_ids,
    ExtensionDownloaderDelegate::Error error,
    const ExtensionDownloaderDelegate::FailureData& data) {
  for (const auto& it : extension_ids) {
    auto ping_iter = ping_results_.find(it);
    delegate_->OnExtensionDownloadFailed(
        it, error,
        ping_iter == ping_results_.end()
            ? ExtensionDownloaderDelegate::PingResult()
            : ping_iter->second,
        request_ids, data);
    ping_results_.erase(it);
  }
}

void ExtensionDownloader::NotifyExtensionsDownloadFailedWithList(
    std::vector<std::pair<ExtensionDownloaderTask, DownloadFailure>> failures,
    std::set<int> request_ids) {
  for (const auto& failure : failures) {
    const ExtensionId& extension_id = failure.first.id;
    ExtensionDownloaderDelegate::Error error = failure.second.error;
    ExtensionDownloaderDelegate::FailureData data = failure.second.failure_data;
    auto ping_iter = ping_results_.find(extension_id);
    delegate_->OnExtensionDownloadFailed(
        extension_id, error,
        ping_iter == ping_results_.end()
            ? ExtensionDownloaderDelegate::PingResult()
            : ping_iter->second,
        request_ids, data);
    ping_results_.erase(extension_id);
  }
}

bool ExtensionDownloader::IterateFetchCredentialsAfterFailure(
    ExtensionFetch* fetch,
    int response_code) {
  bool auth_failure = response_code == net::HTTP_UNAUTHORIZED ||
                      response_code == net::HTTP_FORBIDDEN;
  if (!auth_failure) {
    return false;
  }
  // Here we decide what to do next if the server refused to authorize this
  // fetch.
  switch (fetch->credentials) {
    case ExtensionFetch::CREDENTIALS_NONE:
      if (fetch->url.DomainIs(kGoogleDotCom) && identity_manager_) {
        fetch->credentials = ExtensionFetch::CREDENTIALS_OAUTH2_TOKEN;
      } else {
        fetch->credentials = ExtensionFetch::CREDENTIALS_COOKIES;
      }
      return true;
    case ExtensionFetch::CREDENTIALS_OAUTH2_TOKEN:
      fetch->oauth2_attempt_count++;
      // OAuth2 may fail due to an expired access token, in which case we
      // should invalidate the token and try again.
      if (response_code == net::HTTP_UNAUTHORIZED &&
          fetch->oauth2_attempt_count <= kMaxOAuth2Attempts) {
        DCHECK(identity_manager_);
        signin::ScopeSet webstore_scopes;
        webstore_scopes.insert(kWebstoreOAuth2Scope);
        identity_manager_->RemoveAccessTokenFromCache(
            identity_manager_->GetPrimaryAccountId(signin::ConsentLevel::kSync),
            webstore_scopes, access_token_);
        access_token_.clear();
        return true;
      }
      // Either there is no Gaia identity available, the active identity
      // doesn't have access to this resource, or the server keeps returning
      // 401s and we've retried too many times. Fall back on cookies.
      if (access_token_.empty() || response_code == net::HTTP_FORBIDDEN ||
          fetch->oauth2_attempt_count > kMaxOAuth2Attempts) {
        fetch->credentials = ExtensionFetch::CREDENTIALS_COOKIES;
        return true;
      }
      // Something else is wrong. Time to give up.
      return false;
    case ExtensionFetch::CREDENTIALS_COOKIES:
      if (response_code == net::HTTP_FORBIDDEN) {
        // Try the next session identity, up to some maximum.
        return IncrementAuthUserIndex(&fetch->url);
      }
      return false;
    default:
      NOTREACHED_IN_MIGRATION();
  }
  NOTREACHED_IN_MIGRATION();
  return false;
}

void ExtensionDownloader::OnAccessTokenFetchComplete(
    GoogleServiceAuthError error,
    signin::AccessTokenInfo token_info) {
  access_token_fetcher_.reset();

  if (error.state() != GoogleServiceAuthError::NONE) {
    // If we fail to get an access token, kick the pending fetch and let it fall
    // back on cookies.
    StartExtensionLoader();
    return;
  }

  access_token_ = token_info.token;
  extension_loader_resource_request_->headers.SetHeader(
      net::HttpRequestHeaders::kAuthorization,
      base::StringPrintf("Bearer %s", access_token_.c_str()));
  StartExtensionLoader();
}

ManifestFetchData* ExtensionDownloader::CreateManifestFetchData(
    const GURL& update_url,
    int request_id,
    DownloadFetchPriority fetch_priority) {
  ManifestFetchData::PingMode ping_mode = ManifestFetchData::NO_PING;
  if (update_url.DomainIs(ping_enabled_domain_.c_str()))
    ping_mode = ManifestFetchData::PING_WITH_ENABLED_STATE;
  return new ManifestFetchData(update_url, request_id, brand_code_,
                               manifest_query_params_, ping_mode,
                               fetch_priority);
}

}  // namespace extensions
