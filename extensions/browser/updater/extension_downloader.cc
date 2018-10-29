// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/updater/extension_downloader.h"

#include <stddef.h>

#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/task/post_task.h"
#include "base/time/time.h"
#include "base/version.h"
#include "components/update_client/update_query_params.h"
#include "content/public/browser/file_url_loader.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "extensions/browser/extension_file_task_runner.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/browser/notification_types.h"
#include "extensions/browser/updater/extension_cache.h"
#include "extensions/browser/updater/extension_downloader_test_delegate.h"
#include "extensions/browser/updater/request_queue_impl.h"
#include "extensions/common/extension_updater_uma.h"
#include "extensions/common/extension_urls.h"
#include "extensions/common/manifest_url_handlers.h"
#include "net/base/backoff_entry.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "net/url_request/url_request_status.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"

using base::Time;
using base::TimeDelta;
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
    -1,

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

  // Since HTTP errors are now presented as ERR_FAILED by default, this will
  // let both network and HTTP errors through.
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
  while (url::ExtractQueryKeyValue(old_query.c_str(), &query, &key, &value)) {
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
  std::string new_query_string = base::JoinString(new_query_parts, "&");
  url::Component new_query(0, new_query_string.size());
  url::Replacements<char> replacements;
  replacements.SetQuery(new_query_string.c_str(), new_query);
  *url = url->ReplaceComponents(replacements);
  return true;
}

}  // namespace

const char ExtensionDownloader::kUpdateInteractivityHeader[] =
    "X-Goog-Update-Interactivity";
const char ExtensionDownloader::kUpdateAppIdHeader[] = "X-Goog-Update-AppId";
const char ExtensionDownloader::kUpdateUpdaterHeader[] =
    "X-Goog-Update-Updater";

const char ExtensionDownloader::kUpdateInteractivityForeground[] = "fg";
const char ExtensionDownloader::kUpdateInteractivityBackground[] = "bg";

UpdateDetails::UpdateDetails(const std::string& id,
                             const base::Version& version)
    : id(id), version(version) {}

UpdateDetails::~UpdateDetails() {
}

ExtensionDownloader::ExtensionFetch::ExtensionFetch()
    : url(), credentials(CREDENTIALS_NONE) {
}

ExtensionDownloader::ExtensionFetch::ExtensionFetch(
    const std::string& id,
    const GURL& url,
    const std::string& package_hash,
    const std::string& version,
    const std::set<int>& request_ids)
    : id(id),
      url(url),
      package_hash(package_hash),
      version(version),
      request_ids(request_ids),
      credentials(CREDENTIALS_NONE),
      oauth2_attempt_count(0) {
}

ExtensionDownloader::ExtensionFetch::~ExtensionFetch() {
}

ExtensionDownloader::ExtraParams::ExtraParams() : is_corrupt_reinstall(false) {}

ExtensionDownloader::ExtensionDownloader(
    ExtensionDownloaderDelegate* delegate,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    service_manager::Connector* connector,
    const base::FilePath& profile_path)
    : OAuth2TokenService::Consumer(kTokenServiceConsumerId),
      delegate_(delegate),
      url_loader_factory_(std::move(url_loader_factory)),
      profile_path_for_url_loader_factory_(profile_path),
      connector_(connector),
      manifests_queue_(
          &kDefaultBackoffPolicy,
          base::BindRepeating(&ExtensionDownloader::CreateManifestLoader,
                              base::Unretained(this))),
      extensions_queue_(
          &kDefaultBackoffPolicy,
          base::BindRepeating(&ExtensionDownloader::CreateExtensionLoader,
                              base::Unretained(this))),
      extension_cache_(nullptr),
      token_service_(nullptr),
      weak_ptr_factory_(this) {
  DCHECK(delegate_);
  DCHECK(url_loader_factory_);
}

ExtensionDownloader::~ExtensionDownloader() {
}

bool ExtensionDownloader::AddExtension(
    const Extension& extension,
    int request_id,
    ManifestFetchData::FetchPriority fetch_priority) {
  // Skip extensions with empty update URLs converted from user
  // scripts.
  if (extension.converted_from_user_script() &&
      ManifestURL::GetUpdateURL(&extension).is_empty()) {
    return false;
  }

  ExtraParams extra;

  // If the extension updates itself from the gallery, ignore any update URL
  // data.  At the moment there is no extra data that an extension can
  // communicate to the gallery update servers.
  std::string update_url_data;
  if (!ManifestURL::UpdatesFromGallery(&extension))
    extra.update_url_data = delegate_->GetUpdateUrlData(extension.id());

  return AddExtensionData(extension.id(), extension.version(),
                          extension.GetType(), extension.location(),
                          ManifestURL::GetUpdateURL(&extension), extra,
                          request_id, fetch_priority);
}

bool ExtensionDownloader::AddPendingExtension(
    const std::string& id,
    const GURL& update_url,
    Manifest::Location install_location,
    bool is_corrupt_reinstall,
    int request_id,
    ManifestFetchData::FetchPriority fetch_priority) {
  // Use a zero version to ensure that a pending extension will always
  // be updated, and thus installed (assuming all extensions have
  // non-zero versions).
  base::Version version("0.0.0.0");
  DCHECK(version.IsValid());
  ExtraParams extra;
  if (is_corrupt_reinstall)
    extra.is_corrupt_reinstall = true;

  return AddExtensionData(id, version, Manifest::TYPE_UNKNOWN, install_location,
                          update_url, extra, request_id, fetch_priority);
}

void ExtensionDownloader::StartAllPending(ExtensionCache* cache) {
  if (cache) {
    extension_cache_ = cache;
    extension_cache_->Start(base::Bind(&ExtensionDownloader::DoStartAllPending,
                                       weak_ptr_factory_.GetWeakPtr()));
  } else {
    DoStartAllPending();
  }
}

void ExtensionDownloader::DoStartAllPending() {
  ReportStats();
  url_stats_ = URLStats();

  for (auto it = fetches_preparing_.begin(); it != fetches_preparing_.end();
       ++it) {
    std::vector<std::unique_ptr<ManifestFetchData>>& list = it->second;
    for (size_t i = 0; i < list.size(); ++i)
      StartUpdateCheck(std::move(list[i]));
  }
  fetches_preparing_.clear();
}

void ExtensionDownloader::SetWebstoreAuthenticationCapabilities(
    const GetWebstoreAccountCallback& webstore_account_callback,
    OAuth2TokenService* token_service) {
  webstore_account_callback_ = webstore_account_callback;
  token_service_ = token_service;
}

// static
void ExtensionDownloader::set_test_delegate(
    ExtensionDownloaderTestDelegate* delegate) {
  g_test_delegate = delegate;
}

bool ExtensionDownloader::AddExtensionData(
    const std::string& id,
    const base::Version& version,
    Manifest::Type extension_type,
    Manifest::Location extension_location,
    const GURL& extension_update_url,
    const ExtraParams& extra,
    int request_id,
    ManifestFetchData::FetchPriority fetch_priority) {
  GURL update_url(extension_update_url);
  // Skip extensions with non-empty invalid update URLs.
  if (!update_url.is_empty() && !update_url.is_valid()) {
    DLOG(WARNING) << "Extension " << id << " has invalid update url "
                  << update_url;
    return false;
  }

  // Make sure we use SSL for store-hosted extensions.
  if (extension_urls::IsWebstoreUpdateUrl(update_url) &&
      !update_url.SchemeIsCryptographic())
    update_url = extension_urls::GetWebstoreUpdateUrl();

  // Skip extensions with empty IDs.
  if (id.empty()) {
    DLOG(WARNING) << "Found extension with empty ID";
    return false;
  }

  if (update_url.DomainIs(kGoogleDotCom)) {
    url_stats_.google_url_count++;
  } else if (update_url.is_empty()) {
    url_stats_.no_url_count++;
    // Fill in default update URL.
    update_url = extension_urls::GetWebstoreUpdateUrl();
  } else {
    url_stats_.other_url_count++;
  }

  switch (extension_type) {
    case Manifest::TYPE_THEME:
      ++url_stats_.theme_count;
      break;
    case Manifest::TYPE_EXTENSION:
    case Manifest::TYPE_USER_SCRIPT:
      ++url_stats_.extension_count;
      break;
    case Manifest::TYPE_HOSTED_APP:
    case Manifest::TYPE_LEGACY_PACKAGED_APP:
      ++url_stats_.app_count;
      break;
    case Manifest::TYPE_PLATFORM_APP:
      ++url_stats_.platform_app_count;
      break;
    case Manifest::TYPE_UNKNOWN:
    default:
      ++url_stats_.pending_count;
      break;
  }

  DCHECK(!update_url.is_empty());
  DCHECK(update_url.is_valid());

  std::string install_source = extension_urls::IsWebstoreUpdateUrl(update_url)
                                   ? kDefaultInstallSource
                                   : kNotFromWebstoreInstallSource;
  if (extra.is_corrupt_reinstall)
    install_source = kReinstallInstallSource;

  const std::string install_location =
      ManifestFetchData::GetSimpleLocationString(extension_location);

  ManifestFetchData::PingData ping_data;
  ManifestFetchData::PingData* optional_ping_data = NULL;
  if (delegate_->GetPingDataForExtension(id, &ping_data))
    optional_ping_data = &ping_data;

  // Find or create a ManifestFetchData to add this extension to.
  bool added = false;
  auto existing_iter =
      fetches_preparing_.find(std::make_pair(request_id, update_url));
  if (existing_iter != fetches_preparing_.end() &&
      !existing_iter->second.empty()) {
    // Try to add to the ManifestFetchData at the end of the list.
    ManifestFetchData* existing_fetch = existing_iter->second.back().get();
    if (existing_fetch->AddExtension(
            id, version.GetString(), optional_ping_data, extra.update_url_data,
            install_source, install_location, fetch_priority)) {
      added = true;
    }
  }
  if (!added) {
    // Otherwise add a new element to the list, if the list doesn't exist or
    // if its last element is already full.
    std::unique_ptr<ManifestFetchData> fetch(
        CreateManifestFetchData(update_url, request_id, fetch_priority));
    ManifestFetchData* fetch_ptr = fetch.get();
    fetches_preparing_[std::make_pair(request_id, update_url)].push_back(
        std::move(fetch));
    added = fetch_ptr->AddExtension(id, version.GetString(), optional_ping_data,
                                    extra.update_url_data, install_source,
                                    install_location, fetch_priority);
    DCHECK(added);
  }

  return true;
}

void ExtensionDownloader::ReportStats() const {
  UMA_HISTOGRAM_COUNTS_100("Extensions.UpdateCheckExtension",
                           url_stats_.extension_count);
  UMA_HISTOGRAM_COUNTS_100("Extensions.UpdateCheckTheme",
                           url_stats_.theme_count);
  UMA_HISTOGRAM_COUNTS_100("Extensions.UpdateCheckApp", url_stats_.app_count);
  UMA_HISTOGRAM_COUNTS_100("Extensions.UpdateCheckPackagedApp",
                           url_stats_.platform_app_count);
  UMA_HISTOGRAM_COUNTS_100("Extensions.UpdateCheckPending",
                           url_stats_.pending_count);
  UMA_HISTOGRAM_COUNTS_100("Extensions.UpdateCheckGoogleUrl",
                           url_stats_.google_url_count);
  UMA_HISTOGRAM_COUNTS_100("Extensions.UpdateCheckOtherUrl",
                           url_stats_.other_url_count);
  UMA_HISTOGRAM_COUNTS_100("Extensions.UpdateCheckNoUrl",
                           url_stats_.no_url_count);
}

void ExtensionDownloader::StartUpdateCheck(
    std::unique_ptr<ManifestFetchData> fetch_data) {
  if (g_test_delegate) {
    g_test_delegate->StartUpdateCheck(this, delegate_, std::move(fetch_data));
    return;
  }

  if (!ExtensionsBrowserClient::Get()->IsBackgroundUpdateAllowed()) {
    NotifyExtensionsDownloadFailed(fetch_data->extension_ids(),
                                   fetch_data->request_ids(),
                                   ExtensionDownloaderDelegate::DISABLED);
    return;
  }

  RequestQueue<ManifestFetchData>::iterator i;
  for (i = manifests_queue_.begin(); i != manifests_queue_.end(); ++i) {
    if (fetch_data->full_url() == i->full_url()) {
      // This url is already scheduled to be fetched.
      i->Merge(*fetch_data);
      return;
    }
  }

  if (manifests_queue_.active_request() &&
      manifests_queue_.active_request()->full_url() == fetch_data->full_url()) {
    manifests_queue_.active_request()->Merge(*fetch_data);
  } else {
    UMA_HISTOGRAM_COUNTS_100("Extensions.ExtensionUpdaterUpdateCalls",
                             fetch_data->extension_ids().size());
    UMA_HISTOGRAM_COUNTS_1M(
        "Extensions.UpdateCheckUrlLength",
        fetch_data->full_url().possibly_invalid_spec().length());

    manifests_queue_.ScheduleRequest(std::move(fetch_data));
  }
}

network::mojom::URLLoaderFactory* ExtensionDownloader::GetURLLoaderFactoryToUse(
    const GURL& url) {
  if (!url.SchemeIsFile()) {
    DCHECK(url_loader_factory_);
    return url_loader_factory_.get();
  }

  // file:// URL support.
  auto file_url_loader_factory =
      content::CreateFileURLLoaderFactory(profile_path_for_url_loader_factory_);
  file_url_loader_factory_ = std::move(file_url_loader_factory);
  return file_url_loader_factory_.get();
}

void ExtensionDownloader::CreateManifestLoader() {
  const ManifestFetchData* active_request = manifests_queue_.active_request();
  std::vector<base::StringPiece> id_vector(
      active_request->extension_ids().begin(),
      active_request->extension_ids().end());
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
            "update_url, which is usually Chrome WebStore) in order to update "
            "the extension."
          trigger:
            "An update timer indicates that it's time to update extensions, or "
            "a user triggers an extension update flow."
          data:
            "The extension id, version and install source (the cause of the "
            "update flow). The client's OS, architecture, language, Chromium "
            "version, channel and a flag stating whether the request originated"
            "in the foreground or the background."
          destination: WEBSITE
        }
        policy {
          cookies_allowed: NO
          setting:
            "This feature cannot be disabled. It is only enabled when the user "
            "has installed extensions."
          chrome_policy {
            ExtensionInstallBlacklist {
              policy_options {mode: MANDATORY}
              ExtensionInstallBlacklist: {
                entries: '*'
              }
            }
          }
        })");
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = active_request->full_url(),
  resource_request->load_flags = net::LOAD_DO_NOT_SEND_COOKIES |
                                 net::LOAD_DO_NOT_SAVE_COOKIES |
                                 net::LOAD_DISABLE_CACHE;

  // Send traffic-management headers to the webstore.
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
  }

  manifest_loader_ = network::SimpleURLLoader::Create(
      std::move(resource_request), traffic_annotation);
  // Update checks can be interrupted if a network change is detected; this is
  // common for the retail mode AppPack on ChromeOS. Retrying once should be
  // enough to recover in those cases; let the fetcher retry up to 3 times
  // just in case. http://crosbug.com/130602
  const int kMaxRetries = 3;
  manifest_loader_->SetRetryOptions(
      kMaxRetries,
      network::SimpleURLLoader::RetryMode::RETRY_ON_NETWORK_CHANGE);

  network::mojom::URLLoaderFactory* url_loader_factory_to_use =
      GetURLLoaderFactoryToUse(active_request->full_url());
  manifest_loader_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      url_loader_factory_to_use,
      base::BindOnce(&ExtensionDownloader::OnManifestLoadComplete,
                     base::Unretained(this)));
}

void ExtensionDownloader::OnManifestLoadComplete(
    std::unique_ptr<std::string> response_body) {
  const GURL url = manifest_loader_->GetFinalURL();
  DCHECK(manifests_queue_.active_request());

  int response_code = -1;
  if (manifest_loader_->ResponseInfo() &&
      manifest_loader_->ResponseInfo()->headers)
    response_code = manifest_loader_->ResponseInfo()->headers->response_code();

  VLOG(2) << response_code << " " << url;

  const base::TimeDelta& backoff_delay = base::TimeDelta::FromMilliseconds(0);
  const int request_failure_count =
      manifests_queue_.active_request_failure_count();

  // We want to try parsing the manifest, and if it indicates updates are
  // available, we want to fire off requests to fetch those updates.
  if (response_body && !response_body->empty()) {
    RETRY_HISTOGRAM("ManifestFetchSuccess", request_failure_count, url);
    VLOG(2) << "beginning manifest parse for " << url;
    auto callback = base::BindOnce(&ExtensionDownloader::HandleManifestResults,
                                   weak_ptr_factory_.GetWeakPtr(),
                                   manifests_queue_.reset_active_request());
    ParseUpdateManifest(connector_, *response_body, std::move(callback));
  } else {
    VLOG(1) << "Failed to fetch manifest '" << url.possibly_invalid_spec()
            << "' response code:" << response_code;
    const auto* loader = manifest_loader_.get();
    if (request_failure_count == 0) {
      DCHECK(loader);
      // This is the first failure for this batch request, record the
      // http/network error for each extension in the batch request.
      const int error =
          response_code == -1 ? loader->NetError() : response_code;
      const std::string uma_histogram_name =
          url.DomainIs(kGoogleDotCom)
              ? std::string(
                    "Extensions."
                    "ExtensionUpdaterFirstUpdateCheckErrorsGoogleUrl")
              : std::string(
                    "Extensions."
                    "ExtensionUpdaterFirstUpdateCheckErrorsNonGoogleUrl");
      const auto& extension_ids =
          manifests_queue_.active_request()->extension_ids();
      for (auto it = extension_ids.begin(); it != extension_ids.end(); ++it) {
        base::UmaHistogramSparse(uma_histogram_name, error);
      }
    }
    if (ShouldRetryRequest(loader) && request_failure_count < kMaxRetries) {
      manifests_queue_.RetryRequest(backoff_delay);
    } else {
      RETRY_HISTOGRAM("ManifestFetchFailure",
                      manifests_queue_.active_request_failure_count(),
                      url);
      NotifyExtensionsDownloadFailed(
          manifests_queue_.active_request()->extension_ids(),
          manifests_queue_.active_request()->request_ids(),
          ExtensionDownloaderDelegate::MANIFEST_FETCH_FAILED);
    }
  }
  manifest_loader_.reset();
  file_url_loader_factory_.reset();
  manifests_queue_.reset_active_request();

  // If we have any pending manifest requests, fire off the next one.
  manifests_queue_.StartNextRequest();
}

void ExtensionDownloader::HandleManifestResults(
    std::unique_ptr<ManifestFetchData> fetch_data,
    std::unique_ptr<UpdateManifestResults> results,
    const base::Optional<std::string>& error) {
  // Keep a list of extensions that will not be updated, so that the |delegate_|
  // can be notified once we're done here.
  std::set<std::string> not_updated(fetch_data->extension_ids());

  if (!results) {
    VLOG(2) << "parsing manifest failed (" << fetch_data->full_url() << ")";
    NotifyExtensionsDownloadFailed(
        not_updated, fetch_data->request_ids(),
        ExtensionDownloaderDelegate::MANIFEST_INVALID);
    return;
  } else {
    VLOG(2) << "parsing manifest succeeded (" << fetch_data->full_url() << ")";
  }

  std::vector<UpdateManifestResult*> to_update;
  std::set<std::string> no_updates;
  std::set<std::string> errors;

  // Examine the parsed manifest and kick off fetches of any new crx files.
  DetermineUpdates(*fetch_data, *results, &to_update, &no_updates, &errors);
  for (const UpdateManifestResult* update : to_update) {
    const std::string& extension_id = update->extension_id;

    GURL crx_url = update->crx_url;
    NotifyUpdateFound(extension_id, update->version);
    FetchUpdatedExtension(std::make_unique<ExtensionFetch>(
        update->extension_id, crx_url, update->package_hash, update->version,
        fetch_data->request_ids()));
  }

  // If the manifest response included a <daystart> element, we want to save
  // that value for any extensions which had sent a ping in the request.
  if (fetch_data->base_url().DomainIs(kGoogleDotCom) &&
      results->daystart_elapsed_seconds >= 0) {
    Time day_start =
        Time::Now() - TimeDelta::FromSeconds(results->daystart_elapsed_seconds);

    for (const std::string& id : fetch_data->extension_ids()) {
      ExtensionDownloaderDelegate::PingResult& result = ping_results_[id];
      result.did_ping = fetch_data->DidPing(id, ManifestFetchData::ROLLCALL);
      result.day_start = day_start;
    }
  }

  NotifyExtensionsDownloadFailed(
      no_updates, fetch_data->request_ids(),
      ExtensionDownloaderDelegate::NO_UPDATE_AVAILABLE);
  NotifyExtensionsDownloadFailed(
      errors, fetch_data->request_ids(),
      ExtensionDownloaderDelegate::MANIFEST_FETCH_FAILED);
}

ExtensionDownloader::UpdateAvailability
ExtensionDownloader::GetUpdateAvailability(
    const std::string& extension_id,
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
        VLOG(2) << "Manifest indicates " << extension_id << " has no update";
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
        has_noupdate = true;
        continue;
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
    const ManifestFetchData& fetch_data,
    const UpdateManifestResults& possible_updates,
    std::vector<UpdateManifestResult*>* to_update,
    std::set<std::string>* no_updates,
    std::set<std::string>* errors) {
  DCHECK_NE(nullptr, to_update);
  DCHECK_NE(nullptr, no_updates);
  DCHECK_NE(nullptr, errors);

  // Group possible updates by extension IDs.
  const std::map<std::string, std::vector<const UpdateManifestResult*>>
      update_groups = possible_updates.GroupByID();

  // For each extensions in the current batch, greedily find an update from
  // |possible_updates|.
  for (const std::string& extension_id : fetch_data.extension_ids()) {
    const auto it = update_groups.find(extension_id);
    if (it == update_groups.end()) {
      UMA_HISTOGRAM_COUNTS_100("Extensions.UpdateManifestDuplicateEntryCount",
                               0);
      VLOG(2) << "Manifest doesn't have an update entry for " << extension_id;
      errors->insert(extension_id);
      continue;
    }

    const std::vector<const UpdateManifestResult*>& possible_candidates =
        it->second;
    DCHECK(!possible_candidates.empty());

    UMA_HISTOGRAM_COUNTS_100("Extensions.UpdateManifestDuplicateEntryCount",
                             possible_candidates.size());

    VLOG(2) << "Manifest has " << possible_candidates.size()
            << " update entries for " << extension_id;

    UpdateManifestResult* update_result = nullptr;
    UpdateAvailability update_availability = GetUpdateAvailability(
        extension_id, possible_candidates, &update_result);

    switch (update_availability) {
      case UpdateAvailability::kAvailable:
        DCHECK_NE(nullptr, update_result);
        to_update->push_back(update_result);
        break;
      case UpdateAvailability::kNoUpdate:
        no_updates->insert(extension_id);
        break;
      case UpdateAvailability::kBadUpdateSpecification:
        errors->insert(extension_id);
        break;
    }
  }
}

// Begins (or queues up) download of an updated extension.
void ExtensionDownloader::FetchUpdatedExtension(
    std::unique_ptr<ExtensionFetch> fetch_data) {
  if (!fetch_data->url.is_valid()) {
    // TODO(asargent): This can sometimes be invalid. See crbug.com/130881.
    DLOG(WARNING) << "Invalid URL: '" << fetch_data->url.possibly_invalid_spec()
                  << "' for extension " << fetch_data->id;
    return;
  }

  for (RequestQueue<ExtensionFetch>::iterator iter = extensions_queue_.begin();
       iter != extensions_queue_.end();
       ++iter) {
    if (iter->id == fetch_data->id || iter->url == fetch_data->url) {
      iter->request_ids.insert(fetch_data->request_ids.begin(),
                               fetch_data->request_ids.end());
      return;  // already scheduled
    }
  }

  if (extensions_queue_.active_request() &&
      extensions_queue_.active_request()->url == fetch_data->url) {
    extensions_queue_.active_request()->request_ids.insert(
        fetch_data->request_ids.begin(), fetch_data->request_ids.end());
  } else {
    std::string version;
    if (extension_cache_ &&
        extension_cache_->GetExtension(fetch_data->id, fetch_data->package_hash,
                                       NULL, &version) &&
        version == fetch_data->version) {
      base::FilePath crx_path;
      // Now get .crx file path and mark extension as used.
      extension_cache_->GetExtension(fetch_data->id, fetch_data->package_hash,
                                     &crx_path, &version);
      NotifyDelegateDownloadFinished(std::move(fetch_data), true, crx_path,
                                     false);
    } else {
      extensions_queue_.ScheduleRequest(std::move(fetch_data));
    }
  }
}

void ExtensionDownloader::NotifyDelegateDownloadFinished(
    std::unique_ptr<ExtensionFetch> fetch_data,
    bool from_cache,
    const base::FilePath& crx_path,
    bool file_ownership_passed) {
  // Dereference required params before passing a scoped_ptr.
  const std::string& id = fetch_data->id;
  const std::string& package_hash = fetch_data->package_hash;
  const GURL& url = fetch_data->url;
  const std::string& version = fetch_data->version;
  const std::set<int>& request_ids = fetch_data->request_ids;
  delegate_->OnExtensionDownloadFinished(
      CRXFileInfo(id, crx_path, package_hash), file_ownership_passed, url,
      version, ping_results_[id], request_ids,
      from_cache ? base::BindRepeating(&ExtensionDownloader::CacheInstallDone,
                                       weak_ptr_factory_.GetWeakPtr(),
                                       base::Passed(&fetch_data))
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
  extension_loader_resource_request_ =
      std::make_unique<network::ResourceRequest>();
  extension_loader_resource_request_->url = fetch->url;

  int load_flags = net::LOAD_DISABLE_CACHE;
  bool is_secure = fetch->url.SchemeIsCryptographic();
  if (fetch->credentials != ExtensionFetch::CREDENTIALS_COOKIES || !is_secure) {
    load_flags |= net::LOAD_DO_NOT_SEND_COOKIES | net::LOAD_DO_NOT_SAVE_COOKIES;
  }
  extension_loader_resource_request_->load_flags = load_flags;

  if (fetch->credentials == ExtensionFetch::CREDENTIALS_OAUTH2_TOKEN &&
      is_secure) {
    if (access_token_.empty()) {
      // We should try OAuth2, but we have no token cached. This
      // ExtensionLoader will be started once the token fetch is complete,
      // in either OnTokenFetchSuccess or OnTokenFetchFailure.
      DCHECK(token_service_);
      DCHECK(!webstore_account_callback_.is_null());
      OAuth2TokenService::ScopeSet webstore_scopes;
      webstore_scopes.insert(kWebstoreOAuth2Scope);
      access_token_request_ = token_service_->StartRequest(
          webstore_account_callback_.Run(), webstore_scopes, this);
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
            ExtensionInstallBlacklist {
              policy_options {mode: MANDATORY}
              ExtensionInstallBlacklist: {
                entries: '*'
              }
            }
          }
        })");

  last_extension_loader_resource_request_headers_for_testing_ =
      extension_loader_resource_request_->headers;
  last_extension_loader_load_flags_for_testing_ =
      extension_loader_resource_request_->load_flags;

  network::mojom::URLLoaderFactory* url_loader_factory_to_use =
      GetURLLoaderFactoryToUse(extension_loader_resource_request_->url);
  extension_loader_ = network::SimpleURLLoader::Create(
      std::move(extension_loader_resource_request_), traffic_annotation);
  const int kMaxRetries = 3;
  extension_loader_->SetRetryOptions(
      kMaxRetries,
      network::SimpleURLLoader::RetryMode::RETRY_ON_NETWORK_CHANGE);

  extension_loader_->DownloadToTempFile(
      url_loader_factory_to_use,
      base::BindOnce(&ExtensionDownloader::OnExtensionLoadComplete,
                     base::Unretained(this)));
}

void ExtensionDownloader::OnExtensionLoadComplete(base::FilePath crx_path) {
  GURL url = extension_loader_->GetFinalURL();
  net::URLRequestStatus status =
      net::URLRequestStatus::FromError(extension_loader_->NetError());
  int response_code = -1;
  if (extension_loader_->ResponseInfo() &&
      extension_loader_->ResponseInfo()->headers) {
    response_code = extension_loader_->ResponseInfo()->headers->response_code();
  }
  const base::TimeDelta& backoff_delay = base::TimeDelta::FromMilliseconds(0);

  ExtensionFetch& active_request = *extensions_queue_.active_request();
  const std::string& id = active_request.id;
  if (!crx_path.empty()) {
    RETRY_HISTOGRAM("CrxFetchSuccess",
                    extensions_queue_.active_request_failure_count(),
                    url);
    std::unique_ptr<ExtensionFetch> fetch_data =
        extensions_queue_.reset_active_request();
    if (extension_cache_) {
      const std::string& version = fetch_data->version;
      const std::string& expected_hash = fetch_data->package_hash;
      extension_cache_->PutExtension(
          id, expected_hash, crx_path, version,
          base::BindRepeating(
              &ExtensionDownloader::NotifyDelegateDownloadFinished,
              weak_ptr_factory_.GetWeakPtr(), base::Passed(&fetch_data),
              false));
    } else {
      NotifyDelegateDownloadFinished(std::move(fetch_data), false, crx_path,
                                     true);
    }
  } else if (IterateFetchCredentialsAfterFailure(
                 &active_request, status, response_code)) {
    extensions_queue_.RetryRequest(backoff_delay);
    delegate_->OnExtensionDownloadRetryForTests();
  } else {
    const std::set<int>& request_ids = active_request.request_ids;
    const ExtensionDownloaderDelegate::PingResult& ping = ping_results_[id];
    VLOG(1) << "Failed to fetch extension '" << url.possibly_invalid_spec()
            << "' response code:" << response_code;
    if (ShouldRetryRequest(extension_loader_.get()) &&
        extensions_queue_.active_request_failure_count() < kMaxRetries) {
      extensions_queue_.RetryRequest(backoff_delay);
      delegate_->OnExtensionDownloadRetryForTests();
    } else {
      RETRY_HISTOGRAM("CrxFetchFailure",
                      extensions_queue_.active_request_failure_count(),
                      url);
      // status.error() is 0 (net::OK) or negative. (See net/base/net_errors.h)
      base::UmaHistogramSparse("Extensions.CrxFetchError", -status.error());
      delegate_->OnExtensionDownloadFailed(
          id, ExtensionDownloaderDelegate::CRX_FETCH_FAILED, ping, request_ids);
    }
    ping_results_.erase(id);
    extensions_queue_.reset_active_request();
  }

  extension_loader_.reset();
  file_url_loader_factory_.reset();

  // If there are any pending downloads left, start the next one.
  extensions_queue_.StartNextRequest();
}

void ExtensionDownloader::NotifyExtensionsDownloadFailed(
    const std::set<std::string>& extension_ids,
    const std::set<int>& request_ids,
    ExtensionDownloaderDelegate::Error error) {
  for (auto it = extension_ids.cbegin(); it != extension_ids.cend(); ++it) {
    const ExtensionDownloaderDelegate::PingResult& ping = ping_results_[*it];
    delegate_->OnExtensionDownloadFailed(*it, error, ping, request_ids);
    ping_results_.erase(*it);
  }
}

void ExtensionDownloader::NotifyUpdateFound(const std::string& id,
                                            const std::string& version) {
  UMA_HISTOGRAM_COUNTS_100("Extensions.ExtensionUpdaterUpdateFoundCount", 1);

  UpdateDetails updateInfo(id, base::Version(version));
  content::NotificationService::current()->Notify(
      extensions::NOTIFICATION_EXTENSION_UPDATE_FOUND,
      content::NotificationService::AllBrowserContextsAndSources(),
      content::Details<UpdateDetails>(&updateInfo));
}

bool ExtensionDownloader::IterateFetchCredentialsAfterFailure(
    ExtensionFetch* fetch,
    const net::URLRequestStatus& status,
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
      if (fetch->url.DomainIs(kGoogleDotCom) && token_service_) {
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
        DCHECK(token_service_);
        DCHECK(!webstore_account_callback_.is_null());
        OAuth2TokenService::ScopeSet webstore_scopes;
        webstore_scopes.insert(kWebstoreOAuth2Scope);
        token_service_->InvalidateAccessToken(webstore_account_callback_.Run(),
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
      NOTREACHED();
  }
  NOTREACHED();
  return false;
}

void ExtensionDownloader::OnGetTokenSuccess(
    const OAuth2TokenService::Request* request,
    const OAuth2AccessTokenConsumer::TokenResponse& token_response) {
  access_token_ = token_response.access_token;
  extension_loader_resource_request_->headers.SetHeader(
      net::HttpRequestHeaders::kAuthorization,
      base::StringPrintf("Bearer %s", access_token_.c_str()));
  StartExtensionLoader();
}

void ExtensionDownloader::OnGetTokenFailure(
    const OAuth2TokenService::Request* request,
    const GoogleServiceAuthError& error) {
  // If we fail to get an access token, kick the pending fetch and let it fall
  // back on cookies.
  StartExtensionLoader();
}

ManifestFetchData* ExtensionDownloader::CreateManifestFetchData(
    const GURL& update_url,
    int request_id,
    ManifestFetchData::FetchPriority fetch_priority) {
  ManifestFetchData::PingMode ping_mode = ManifestFetchData::NO_PING;
  if (update_url.DomainIs(ping_enabled_domain_.c_str()))
    ping_mode = ManifestFetchData::PING_WITH_ENABLED_STATE;
  return new ManifestFetchData(update_url, request_id, brand_code_,
                               manifest_query_params_, ping_mode,
                               fetch_priority);
}

}  // namespace extensions
