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
#include "components/crx_file/crx_verifier.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/primary_account_access_token_fetcher.h"
#include "components/signin/public/identity_manager/scope_set.h"
#include "components/update_client/update_query_params.h"
#include "content/public/browser/file_url_loader.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/shared_cors_origin_access_list.h"
#include "extensions/browser/extension_file_task_runner.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/browser/notification_types.h"
#include "extensions/browser/updater/extension_cache.h"
#include "extensions/browser/updater/extension_downloader_test_delegate.h"
#include "extensions/browser/updater/request_queue_impl.h"
#include "extensions/common/extension_updater_uma.h"
#include "extensions/common/extension_urls.h"
#include "extensions/common/manifest_url_handlers.h"
#include "extensions/common/verifier_formats.h"
#include "net/base/backoff_entry.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
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

bool ShouldRetryRequestForExtensionNotFoundInCache(const int net_error_code) {
  return net_error_code == net::ERR_INTERNET_DISCONNECTED;
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

UpdateDetails::~UpdateDetails() = default;

ExtensionDownloader::ExtensionFetch::ExtensionFetch()
    : credentials(CREDENTIALS_NONE) {}

ExtensionDownloader::ExtensionFetch::ExtensionFetch(
    const std::string& id,
    const GURL& url,
    const std::string& package_hash,
    const std::string& version,
    const std::set<int>& request_ids,
    const ManifestFetchData::FetchPriority fetch_priority)
    : id(id),
      url(url),
      package_hash(package_hash),
      version(version),
      request_ids(request_ids),
      fetch_priority(fetch_priority),
      credentials(CREDENTIALS_NONE),
      oauth2_attempt_count(0) {}

ExtensionDownloader::ExtensionFetch::~ExtensionFetch() = default;

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

ExtensionDownloader::ExtraParams::ExtraParams() : is_corrupt_reinstall(false) {}

ExtensionDownloader::ExtensionDownloader(
    ExtensionDownloaderDelegate* delegate,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const crx_file::VerifierFormat crx_format_requirement,
    const base::FilePath& profile_path)
    : delegate_(delegate),
      url_loader_factory_(std::move(url_loader_factory)),
      profile_path_for_url_loader_factory_(profile_path),
      manifests_queue_(
          &kDefaultBackoffPolicy,
          base::BindRepeating(&ExtensionDownloader::CreateManifestLoader,
                              base::Unretained(this))),
      extensions_queue_(
          &kDefaultBackoffPolicy,
          base::BindRepeating(&ExtensionDownloader::CreateExtensionLoader,
                              base::Unretained(this))),
      extension_cache_(nullptr),
      identity_manager_(nullptr),
      crx_format_requirement_(crx_format_requirement) {
  DCHECK(delegate_);
  DCHECK(url_loader_factory_);
}

ExtensionDownloader::~ExtensionDownloader() = default;

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
  return AddPendingExtensionWithVersion(
      id, update_url, install_location, is_corrupt_reinstall, request_id,
      fetch_priority, base::Version("0.0.0.0"));
}

bool ExtensionDownloader::AddPendingExtensionWithVersion(
    const std::string& id,
    const GURL& update_url,
    Manifest::Location install_location,
    bool is_corrupt_reinstall,
    int request_id,
    ManifestFetchData::FetchPriority fetch_priority,
    base::Version version) {
  DCHECK(version.IsValid());
  ExtraParams extra;
  if (is_corrupt_reinstall)
    extra.is_corrupt_reinstall = true;

  delegate_->OnExtensionDownloadStageChanged(
      id, ExtensionDownloaderDelegate::Stage::PENDING);
  return AddExtensionData(id, version, Manifest::TYPE_UNKNOWN, install_location,
                          update_url, extra, request_id, fetch_priority);
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

void ExtensionDownloader::SetIdentityManager(
    signin::IdentityManager* identity_manager) {
  identity_manager_ = identity_manager;
}

// static
void ExtensionDownloader::set_test_delegate(
    ExtensionDownloaderTestDelegate* delegate) {
  g_test_delegate = delegate;
}

void ExtensionDownloader::SetBackoffPolicyForTesting(
    const net::BackoffEntry::Policy* backoff_policy) {
  manifests_queue_.set_backoff_policy(backoff_policy);
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
    delegate_->OnExtensionDownloadStageChanged(
        id, ExtensionDownloaderDelegate::Stage::FINISHED);
    return false;
  }

  // Make sure we use SSL for store-hosted extensions.
  if (extension_urls::IsWebstoreUpdateUrl(update_url) &&
      !update_url.SchemeIsCryptographic())
    update_url = extension_urls::GetWebstoreUpdateUrl();

  // Skip extensions with empty IDs.
  if (id.empty()) {
    DLOG(WARNING) << "Found extension with empty ID";
    delegate_->OnExtensionDownloadStageChanged(
        id, ExtensionDownloaderDelegate::Stage::FINISHED);
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

  ManifestFetchData::PingData ping_data;
  ManifestFetchData::PingData* optional_ping_data = NULL;
  if (delegate_->GetPingDataForExtension(id, &ping_data))
    optional_ping_data = &ping_data;

  // Find or create a ManifestFetchData to add this extension to.
  bool added = false;
  bool is_new_extension_force_installed =
      extension_location == Manifest::Location::EXTERNAL_POLICY_DOWNLOAD;
  FetchDataGroupKey key(request_id, update_url,
                        is_new_extension_force_installed);
  auto existing_iter = fetches_preparing_.find(key);
  if (existing_iter != fetches_preparing_.end() &&
      !existing_iter->second.empty()) {
    // Try to add to the ManifestFetchData at the end of the list.
    ManifestFetchData* existing_fetch = existing_iter->second.back().get();
    if (existing_fetch->AddExtension(
            id, version.GetString(), optional_ping_data, extra.update_url_data,
            install_source, extension_location, fetch_priority)) {
      added = true;
    }
  }
  if (!added) {
    // Otherwise add a new element to the list, if the list doesn't exist or
    // if its last element is already full.
    std::unique_ptr<ManifestFetchData> fetch(
        CreateManifestFetchData(update_url, request_id, fetch_priority));
    ManifestFetchData* fetch_ptr = fetch.get();
    if (is_new_extension_force_installed)
      fetch_ptr->set_is_all_external_policy_download();
    fetches_preparing_[key].push_back(std::move(fetch));
    added = fetch_ptr->AddExtension(id, version.GetString(), optional_ping_data,
                                    extra.update_url_data, install_source,
                                    extension_location, fetch_priority);
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
      i->Merge(*fetch_data);
      return;
    }
  }

  if (manifests_queue_.active_request() &&
      manifests_queue_.active_request()->full_url() == fetch_data->full_url()) {
    NotifyExtensionsDownloadStageChanged(
        extension_ids,
        ExtensionDownloaderDelegate::Stage::DOWNLOADING_MANIFEST);
    manifests_queue_.active_request()->Merge(*fetch_data);
  } else {
    UMA_HISTOGRAM_COUNTS_1M(
        "Extensions.UpdateCheckUrlLength",
        fetch_data->full_url().possibly_invalid_spec().length());

    NotifyExtensionsDownloadStageChanged(
        extension_ids, ExtensionDownloaderDelegate::Stage::QUEUED_FOR_MANIFEST);
    manifests_queue_.ScheduleRequest(std::move(fetch_data));
  }
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
  std::vector<base::StringPiece> id_vector(extension_ids.begin(),
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

  if (active_request->fetch_priority() ==
      ManifestFetchData::FetchPriority::FOREGROUND) {
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

void ExtensionDownloader::RetryManifestFetchRequest() {
  constexpr base::TimeDelta backoff_delay;
  NotifyExtensionsDownloadStageChanged(
      manifests_queue_.active_request()->GetExtensionIds(),
      ExtensionDownloaderDelegate::Stage::DOWNLOADING_MANIFEST_RETRY);
  manifests_queue_.RetryRequest(backoff_delay);
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

void ExtensionDownloader::TryFetchingExtensionsFromCache(
    ManifestFetchData* fetch_data,
    ExtensionDownloaderDelegate::Error error,
    const int net_error,
    const int response_code,
    const base::Optional<ManifestInvalidFailureDataList>&
        manifest_invalid_errors) {
  const ExtensionIdSet extension_ids = fetch_data->GetExtensionIds();
  ExtensionIdSet extensions_fetched_from_cache;
  for (const auto& extension_id : extension_ids) {
    // Extension is fetched here only in cases when we fail to fetch the update
    // manifest or parsing of update manifest failed. In such cases, we don't
    // have expected version and expected hash. Thus, passing empty hash and
    // version would not be a problem as we only check for the expected hash and
    // version if we have them.
    auto extension_fetch_data(std::make_unique<ExtensionFetch>(
        extension_id, fetch_data->base_url(), /*hash not fetched*/ "",
        /*version not fetched*/ "", fetch_data->request_ids(),
        fetch_data->fetch_priority()));
    base::Optional<base::FilePath> cached_crx_path = GetCachedExtension(
        *extension_fetch_data, /*manifest_fetch_failed*/ true);
    if (cached_crx_path) {
      delegate_->OnExtensionDownloadStageChanged(
          extension_id, ExtensionDownloaderDelegate::Stage::FINISHED);
      NotifyDelegateDownloadFinished(std::move(extension_fetch_data), true,
                                     cached_crx_path.value(), false);
      extensions_fetched_from_cache.insert(extension_id);
    }
  }
  // All the extensions were found in the cache, no need to retry any request or
  // report failure.
  if (extensions_fetched_from_cache.size() == extension_ids.size())
    return;
  fetch_data->RemoveExtensions(extensions_fetched_from_cache,
                               manifest_query_params_);

  if (ShouldRetryRequestForExtensionNotFoundInCache(net_error)) {
    RetryManifestFetchRequest();
    return;
  }
  if (error == ExtensionDownloaderDelegate::Error::MANIFEST_FETCH_FAILED) {
    ExtensionDownloaderDelegate::FailureData failure_data(
        -net_error,
        (net_error == net::Error::ERR_HTTP_RESPONSE_CODE_FAILURE)
            ? base::Optional<int>(response_code)
            : base::nullopt,
        manifests_queue_.active_request_failure_count());
    ReportManifestFetchFailure(fetch_data, error, failure_data);
    return;
  }
  DCHECK(manifest_invalid_errors);
  ManifestInvalidFailureDataList errors_for_remaining_extensions;
  for (const auto& manifest_invalid_error : manifest_invalid_errors.value()) {
    if (!extensions_fetched_from_cache.count(manifest_invalid_error.first))
      errors_for_remaining_extensions.push_back(manifest_invalid_error);
  }
  NotifyExtensionsDownloadStageChanged(
      fetch_data->GetExtensionIds(),
      ExtensionDownloaderDelegate::Stage::FINISHED);
  NotifyExtensionsManifestInvalidFailure(errors_for_remaining_extensions,
                                         fetch_data->request_ids());
}

void ExtensionDownloader::RetryRequestOrHandleFailureOnManifestFetchFailure(
    const network::SimpleURLLoader* loader,
    const int response_code) {
  bool all_force_installed_extensions =
      manifests_queue_.active_request()->is_all_external_policy_download();

  const int net_error = manifest_loader_->NetError();
  const int request_failure_count =
      manifests_queue_.active_request_failure_count();
  // If the device is offline, do not retry for force installed extensions,
  // try installing it from cache. Try fetching from cache only on first attempt
  // in this case, because we will retry the request only if there was no entry
  // in cache corresponding to this extension and there is no point in trying to
  // fetch extension from cache again.
  if (net_error == net::ERR_INTERNET_DISCONNECTED &&
      all_force_installed_extensions && request_failure_count == 0) {
    TryFetchingExtensionsFromCache(
        manifests_queue_.active_request(),
        ExtensionDownloaderDelegate::Error::MANIFEST_FETCH_FAILED, net_error,
        response_code, base::nullopt /*manifest_invalid_errors*/);
    return;
  }
  if (ShouldRetryRequest(loader) && request_failure_count < kMaxRetries) {
    RetryManifestFetchRequest();
    return;
  }
  const GURL url = loader->GetFinalURL();
  RETRY_HISTOGRAM("ManifestFetchFailure", request_failure_count, url);
  if (all_force_installed_extensions) {
    TryFetchingExtensionsFromCache(
        manifests_queue_.active_request(),
        ExtensionDownloaderDelegate::Error::MANIFEST_FETCH_FAILED, net_error,
        response_code, base::nullopt /*manifest_invalid_errors*/);
  } else {
    ExtensionDownloaderDelegate::FailureData failure_data(
        -net_error,
        (net_error == net::Error::ERR_HTTP_RESPONSE_CODE_FAILURE)
            ? base::Optional<int>(response_code)
            : base::nullopt,
        request_failure_count);
    ReportManifestFetchFailure(
        manifests_queue_.active_request(),
        ExtensionDownloaderDelegate::Error::MANIFEST_FETCH_FAILED,
        failure_data);
  }
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

  const int request_failure_count =
      manifests_queue_.active_request_failure_count();

  // We want to try parsing the manifest, and if it indicates updates are
  // available, we want to fire off requests to fetch those updates.
  if (response_body && !response_body->empty()) {
    RETRY_HISTOGRAM("ManifestFetchSuccess", request_failure_count, url);
    VLOG(2) << "beginning manifest parse for " << url;
    NotifyExtensionsDownloadStageChanged(
        manifests_queue_.active_request()->GetExtensionIds(),
        ExtensionDownloaderDelegate::Stage::PARSING_MANIFEST);
    auto callback = base::BindOnce(&ExtensionDownloader::HandleManifestResults,
                                   weak_ptr_factory_.GetWeakPtr(),
                                   manifests_queue_.reset_active_request());
    ParseUpdateManifest(*response_body, std::move(callback));
  } else {
    VLOG(1) << "Failed to fetch manifest '" << url.possibly_invalid_spec()
            << "' response code:" << response_code;
    RetryRequestOrHandleFailureOnManifestFetchFailure(manifest_loader_.get(),
                                                      response_code);
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
    const base::Optional<ManifestParseFailure>& error) {
  if (!results) {
    VLOG(2) << "parsing manifest failed (" << fetch_data->full_url() << ")";
    DCHECK(error.has_value());
    ManifestInvalidFailureDataList manifest_invalid_errors;
    const ExtensionIdSet extension_ids = fetch_data->GetExtensionIds();
    manifest_invalid_errors.reserve(extension_ids.size());
    // If the manifest parsing failed for all the extensions with a common
    // error, add all extensions in the list with that error.
    for (const auto& extension_id : extension_ids) {
      manifest_invalid_errors.push_back(std::make_pair(
          extension_id,
          ExtensionDownloaderDelegate::FailureData(error.value().error)));
    }
    TryFetchingExtensionsFromCache(
        fetch_data.get(), ExtensionDownloaderDelegate::Error::MANIFEST_INVALID,
        0 /*net_error_code*/, 0 /*response_code*/, manifest_invalid_errors);
    return;
  } else {
    VLOG(2) << "parsing manifest succeeded (" << fetch_data->full_url() << ")";
  }

  const ExtensionIdSet extension_ids = fetch_data->GetExtensionIds();
  NotifyExtensionsDownloadStageChanged(
      extension_ids, ExtensionDownloaderDelegate::Stage::MANIFEST_LOADED);

  std::vector<UpdateManifestResult*> to_update;
  std::set<std::string> no_updates;
  ManifestInvalidFailureDataList errors;

  // Examine the parsed manifest and kick off fetches of any new crx files.
  DetermineUpdates(*fetch_data, *results, &to_update, &no_updates, &errors);
  for (const UpdateManifestResult* update : to_update) {
    const std::string& extension_id = update->extension_id;

    GURL crx_url = update->crx_url;
    NotifyUpdateFound(extension_id, update->version);
    if (fetch_data->is_all_external_policy_download() && crx_url.is_empty()) {
      DCHECK_EQ(fetch_data->fetch_priority(),
                ManifestFetchData::FetchPriority::FOREGROUND);
    }
    FetchUpdatedExtension(
        std::make_unique<ExtensionFetch>(
            extension_id, crx_url, update->package_hash, update->version,
            fetch_data->request_ids(), fetch_data->fetch_priority()),
        update->info);
  }

  // If the manifest response included a <daystart> element, we want to save
  // that value for any extensions which had sent a ping in the request.
  if (fetch_data->base_url().DomainIs(kGoogleDotCom) &&
      results->daystart_elapsed_seconds >= 0) {
    Time day_start =
        Time::Now() - TimeDelta::FromSeconds(results->daystart_elapsed_seconds);

    for (const ExtensionId& id : extension_ids) {
      ExtensionDownloaderDelegate::PingResult& result = ping_results_[id];
      result.did_ping = fetch_data->DidPing(id, ManifestFetchData::ROLLCALL);
      result.day_start = day_start;
    }
  }

  NotifyExtensionsDownloadStageChanged(
      no_updates, ExtensionDownloaderDelegate::Stage::FINISHED);
  NotifyExtensionsDownloadFailed(
      no_updates, fetch_data->request_ids(),
      ExtensionDownloaderDelegate::Error::NO_UPDATE_AVAILABLE);
  ExtensionIdSet extension_ids_with_errors;
  for (const auto& error : errors)
    extension_ids_with_errors.insert(error.first);
  NotifyExtensionsDownloadStageChanged(
      extension_ids_with_errors, ExtensionDownloaderDelegate::Stage::FINISHED);
  NotifyExtensionsManifestInvalidFailure(errors, fetch_data->request_ids());
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
    ManifestInvalidFailureDataList* errors) {
  DCHECK_NE(nullptr, to_update);
  DCHECK_NE(nullptr, no_updates);
  DCHECK_NE(nullptr, errors);

  // Group successful possible updates by extension IDs.
  const std::map<std::string, std::vector<const UpdateManifestResult*>>
      update_groups = possible_updates.GroupSuccessfulByID();

  // Contains IDs of extensions which neither have successful update entry nor
  // are already inserted into |errors|.
  ExtensionIdSet extension_errors;

  const ExtensionIdSet extension_ids = fetch_data.GetExtensionIds();
  // For each extensions in the current batch, greedily find an update from
  // |possible_updates|.
  for (const auto& extension_id : extension_ids) {
    const auto it = update_groups.find(extension_id);
    if (it == update_groups.end()) {
      VLOG(2) << "Manifest doesn't have an update entry for " << extension_id;
      extension_errors.insert(extension_id);
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
        to_update->push_back(update_result);
        break;
      case UpdateAvailability::kNoUpdate:
        no_updates->insert(extension_id);
        break;
      case UpdateAvailability::kBadUpdateSpecification:
        errors->emplace_back(extension_id,
                             ManifestInvalidError::BAD_UPDATE_SPECIFICATION);
        break;
    }
  }
  for (const auto& possible_update : possible_updates.update_list) {
    const ExtensionId& id = possible_update.extension_id;
    if (!extension_errors.count(id))
      continue;
    DCHECK(possible_update.parse_error);
    ManifestInvalidError error_type = possible_update.parse_error.value().error;
    // Report any error corresponding to an extension.
    errors->emplace_back(
        id, error_type == ManifestInvalidError::BAD_APP_STATUS
                ? ExtensionDownloaderDelegate::FailureData(
                      error_type, possible_update.app_status)
                : ExtensionDownloaderDelegate::FailureData(error_type));
    extension_errors.erase(id);
  }
  // For the remaining extensions, we have missing ids.
  for (const auto& id : extension_errors) {
    errors->emplace_back(id, ExtensionDownloaderDelegate::FailureData(
                                 ManifestInvalidError::MISSING_APP_ID));
  }
}

base::Optional<base::FilePath> ExtensionDownloader::GetCachedExtension(
    const ExtensionFetch& fetch_data,
    bool manifest_fetch_failed) {
  if (!extension_cache_) {
    delegate_->OnExtensionDownloadCacheStatusRetrieved(
        fetch_data.id,
        ExtensionDownloaderDelegate::CacheStatus::CACHE_DISABLED);
    return base::nullopt;
  }

  std::string version;
  if (!extension_cache_->GetExtension(fetch_data.id, fetch_data.package_hash,
                                      nullptr, &version)) {
    delegate_->OnExtensionDownloadCacheStatusRetrieved(
        fetch_data.id, ExtensionDownloaderDelegate::CacheStatus::CACHE_MISS);
    return base::nullopt;
  }
  // If manifest fetch is failed, we need not verify the version of the cache as
  // we will try to install the version present in the cache.
  if (!manifest_fetch_failed && fetch_data.version != base::Version(version)) {
    delegate_->OnExtensionDownloadCacheStatusRetrieved(
        fetch_data.id,
        ExtensionDownloaderDelegate::CacheStatus::CACHE_OUTDATED);
    return base::nullopt;
  }

  delegate_->OnExtensionDownloadCacheStatusRetrieved(
      fetch_data.id, manifest_fetch_failed
                         ? ExtensionDownloaderDelegate::CacheStatus::
                               CACHE_HIT_ON_MANIFEST_FETCH_FAILURE
                         : ExtensionDownloaderDelegate::CacheStatus::CACHE_HIT);

  base::FilePath crx_path;
  // Now get .crx file path.
  // TODO(https://crbug.com/1018271#c2) This has a  side-effect in extension
  // cache implementation: extension in the cache will be marked as recently
  // used.
  extension_cache_->GetExtension(fetch_data.id, fetch_data.package_hash,
                                 &crx_path, &version);
  return std::move(crx_path);
}

// Begins (or queues up) download of an updated extension.
void ExtensionDownloader::FetchUpdatedExtension(
    std::unique_ptr<ExtensionFetch> fetch_data,
    base::Optional<std::string> info) {
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
          {fetch_data->id}, fetch_data->request_ids,
          ExtensionDownloaderDelegate::Error::CRX_FETCH_URL_EMPTY, data);
    } else {
      NotifyExtensionsDownloadFailed(
          {fetch_data->id}, fetch_data->request_ids,
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
      iter->request_ids.insert(fetch_data->request_ids.begin(),
                               fetch_data->request_ids.end());
      return;  // already scheduled
    }
  }

  if (extensions_queue_.active_request() &&
      extensions_queue_.active_request()->url == fetch_data->url) {
    delegate_->OnExtensionDownloadStageChanged(
        fetch_data->id, ExtensionDownloaderDelegate::Stage::DOWNLOADING_CRX);
    extensions_queue_.active_request()->request_ids.insert(
        fetch_data->request_ids.begin(), fetch_data->request_ids.end());
    return;
  }
  base::Optional<base::FilePath> cached_crx_path =
      GetCachedExtension(*fetch_data, /*manifest_fetch_failed*/ false);
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
  const std::set<int>& request_ids = fetch_data->request_ids;
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
              signin::PrimaryAccountAccessTokenFetcher::Mode::kImmediate);
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

  const ExtensionFetch* active_request = extensions_queue_.active_request();
  if (active_request->fetch_priority ==
      ManifestFetchData::FetchPriority::FOREGROUND) {
    extension_loader_resource_request_->priority = net::MEDIUM;
  }

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
  int net_error = extension_loader_->NetError();
  int response_code = -1;
  if (extension_loader_->ResponseInfo() &&
      extension_loader_->ResponseInfo()->headers) {
    response_code = extension_loader_->ResponseInfo()->headers->response_code();
  }
  const base::TimeDelta& backoff_delay = base::TimeDelta::FromMilliseconds(0);

  ExtensionFetch& active_request = *extensions_queue_.active_request();
  const ExtensionId& id = active_request.id;
  if (!crx_path.empty()) {
    RETRY_HISTOGRAM("CrxFetchSuccess",
                    extensions_queue_.active_request_failure_count(),
                    url);
    std::unique_ptr<ExtensionFetch> fetch_data =
        extensions_queue_.reset_active_request();
    delegate_->OnExtensionDownloadStageChanged(
        id, ExtensionDownloaderDelegate::Stage::FINISHED);
    NotifyDelegateDownloadFinished(std::move(fetch_data), false, crx_path,
                                   true);
  } else if (IterateFetchCredentialsAfterFailure(&active_request,
                                                 response_code)) {
    delegate_->OnExtensionDownloadStageChanged(
        id, ExtensionDownloaderDelegate::Stage::DOWNLOADING_CRX_RETRY);
    extensions_queue_.RetryRequest(backoff_delay);
    delegate_->OnExtensionDownloadRetryForTests();
  } else {
    const std::set<int>& request_ids = active_request.request_ids;
    const ExtensionDownloaderDelegate::PingResult& ping = ping_results_[id];
    VLOG(1) << "Failed to fetch extension '" << url.possibly_invalid_spec()
            << "' response code:" << response_code;
    if (ShouldRetryRequest(extension_loader_.get()) &&
        extensions_queue_.active_request_failure_count() < kMaxRetries) {
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
      ExtensionDownloaderDelegate::FailureData failure_data(
          -net_error,
          (net_error == net::Error::ERR_HTTP_RESPONSE_CODE_FAILURE)
              ? base::Optional<int>(response_code)
              : base::nullopt,
          extensions_queue_.active_request_failure_count());
      delegate_->OnExtensionDownloadFailed(
          id, ExtensionDownloaderDelegate::Error::CRX_FETCH_FAILED, ping,
          request_ids, failure_data);
    }
    ping_results_.erase(id);
    extensions_queue_.reset_active_request();
  }

  extension_loader_.reset();
  file_url_loader_factory_.reset();

  // If there are any pending downloads left, start the next one.
  extensions_queue_.StartNextRequest();
}

void ExtensionDownloader::NotifyExtensionsManifestInvalidFailure(
    const ManifestInvalidFailureDataList& errors,
    const std::set<int>& request_ids) {
  for (const auto& error_data : errors) {
    const ExtensionId& extension_id = error_data.first;
    ExtensionDownloaderDelegate::FailureData data = error_data.second;
    auto ping_iter = ping_results_.find(extension_id);
    delegate_->OnExtensionDownloadFailed(
        extension_id, ExtensionDownloaderDelegate::Error::MANIFEST_INVALID,
        ping_iter == ping_results_.end()
            ? ExtensionDownloaderDelegate::PingResult()
            : ping_iter->second,
        request_ids, data);
    ping_results_.erase(extension_id);
  }
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

void ExtensionDownloader::NotifyUpdateFound(const std::string& id,
                                            const std::string& version) {
  UpdateDetails updateInfo(id, base::Version(version));
  content::NotificationService::current()->Notify(
      extensions::NOTIFICATION_EXTENSION_UPDATE_FOUND,
      content::NotificationService::AllBrowserContextsAndSources(),
      content::Details<UpdateDetails>(&updateInfo));
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
            identity_manager_->GetPrimaryAccountId(), webstore_scopes,
            access_token_);
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
    ManifestFetchData::FetchPriority fetch_priority) {
  ManifestFetchData::PingMode ping_mode = ManifestFetchData::NO_PING;
  if (update_url.DomainIs(ping_enabled_domain_.c_str()))
    ping_mode = ManifestFetchData::PING_WITH_ENABLED_STATE;
  return new ManifestFetchData(update_url, request_id, brand_code_,
                               manifest_query_params_, ping_mode,
                               fetch_priority);
}

}  // namespace extensions
