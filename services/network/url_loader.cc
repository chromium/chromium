// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/url_loader.h"

#include <algorithm>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/containers/fixed_flat_set.h"
#include "base/files/file.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/not_fatal_until.h"
#include "base/numerics/safe_conversions.h"
#include "base/ranges/algorithm.h"
#include "base/sequence_checker.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/thread_annotations.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/typed_macros.h"
#include "build/build_config.h"
#include "mojo/public/cpp/system/simple_watcher.h"
#include "net/base/elements_upload_data_stream.h"
#include "net/base/isolation_info.h"
#include "net/base/load_flags.h"
#include "net/base/load_timing_info.h"
#include "net/base/mime_sniffer.h"
#include "net/base/proxy_chain.h"
#include "net/base/schemeful_site.h"
#include "net/base/transport_info.h"
#include "net/base/upload_bytes_element_reader.h"
#include "net/base/upload_file_element_reader.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_inclusion_status.h"
#include "net/cookies/cookie_setting_override.h"
#include "net/cookies/cookie_store.h"
#include "net/cookies/cookie_util.h"
#include "net/cookies/site_for_cookies.h"
#include "net/cookies/static_cookie_policy.h"
#include "net/dns/public/secure_dns_policy.h"
#include "net/http/http_connection_info.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_util.h"
#include "net/http/structured_headers.h"
#include "net/log/net_log_source_type.h"
#include "net/log/net_log_with_source.h"
#include "net/ssl/client_cert_store.h"
#include "net/ssl/ssl_connection_status_flags.h"
#include "net/ssl/ssl_private_key.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "net/url_request/redirect_info.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"
#include "services/network/ad_heuristic_cookie_overrides.h"
#include "services/network/attribution/attribution_request_helper.h"
#include "services/network/chunked_data_pipe_upload_data_stream.h"
#include "services/network/data_pipe_element_reader.h"
#include "services/network/orb/orb_impl.h"
#include "services/network/public/cpp/client_hints.h"
#include "services/network/public/cpp/constants.h"
#include "services/network/public/cpp/cors/cors.h"
#include "services/network/public/cpp/cors/origin_access_list.h"
#include "services/network/public/cpp/cross_origin_resource_policy.h"
#include "services/network/public/cpp/empty_url_loader_client.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/header_util.h"
#include "services/network/public/cpp/ip_address_space_util.h"
#include "services/network/public/cpp/net_adapters.h"
#include "services/network/public/cpp/network_switches.h"
#include "services/network/public/cpp/parsed_headers.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/client_security_state.mojom-forward.h"
#include "services/network/public/mojom/cookie_access_observer.mojom-forward.h"
#include "services/network/public/mojom/cookie_access_observer.mojom.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "services/network/public/mojom/devtools_observer.mojom.h"
#include "services/network/public/mojom/early_hints.mojom.h"
#include "services/network/public/mojom/fetch_api.mojom.h"
#include "services/network/public/mojom/http_raw_headers.mojom.h"
#include "services/network/public/mojom/network_context_client.mojom.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/resource_scheduler/resource_scheduler_client.h"
#include "services/network/sec_header_helpers.h"
#include "services/network/shared_dictionary/shared_dictionary_access_checker.h"
#include "services/network/shared_dictionary/shared_dictionary_manager.h"
#include "services/network/shared_dictionary/shared_dictionary_storage.h"
#include "services/network/shared_storage/shared_storage_request_helper.h"
#include "services/network/slop_bucket.h"
#include "services/network/throttling/scoped_throttling_token.h"
#include "services/network/trust_tokens/trust_token_request_helper.h"
#include "services/network/url_loader_factory.h"
#include "third_party/abseil-cpp/absl/container/inlined_vector.h"
#include "url/origin.h"

namespace network {

namespace {

// Cannot use 0, because this means "default" in
// mojo::core::Core::CreateDataPipe
constexpr size_t kBlockedBodyAllocationSize = 1;

// Size to allocate for `discard_buffer_`.
constexpr size_t kDiscardBufferSize = 128 * 1024;

constexpr char kActivateStorageAccessHeader[] = "activate-storage-access";

// A subclass of net::UploadBytesElementReader which owns
// ResourceRequestBody.
class BytesElementReader : public net::UploadBytesElementReader {
 public:
  BytesElementReader(ResourceRequestBody* resource_request_body,
                     const DataElementBytes& element)
      : net::UploadBytesElementReader(element.bytes()),
        resource_request_body_(resource_request_body) {}

  BytesElementReader(const BytesElementReader&) = delete;
  BytesElementReader& operator=(const BytesElementReader&) = delete;

  ~BytesElementReader() override {}

 private:
  scoped_refptr<ResourceRequestBody> resource_request_body_;
};

// A subclass of net::UploadFileElementReader which owns
// ResourceRequestBody.
// This class is necessary to ensure the BlobData and any attached shareable
// files survive until upload completion.
class FileElementReader : public net::UploadFileElementReader {
 public:
  FileElementReader(ResourceRequestBody* resource_request_body,
                    base::TaskRunner* task_runner,
                    const DataElementFile& element,
                    base::File&& file)
      : net::UploadFileElementReader(task_runner,
                                     std::move(file),
                                     element.path(),
                                     element.offset(),
                                     element.length(),
                                     element.expected_modification_time()),
        resource_request_body_(resource_request_body) {}

  FileElementReader(const FileElementReader&) = delete;
  FileElementReader& operator=(const FileElementReader&) = delete;

  ~FileElementReader() override {}

 private:
  scoped_refptr<ResourceRequestBody> resource_request_body_;
};

std::unique_ptr<net::UploadDataStream> CreateUploadDataStream(
    ResourceRequestBody* body,
    std::vector<base::File>& opened_files,
    base::SequencedTaskRunner* file_task_runner) {
  // In the case of a chunked upload, there will just be one element.
  if (body->elements()->size() == 1) {
    if (body->elements()->begin()->type() ==
        network::mojom::DataElementDataView::Tag::kChunkedDataPipe) {
      auto& element =
          body->elements_mutable()->at(0).As<DataElementChunkedDataPipe>();
      const bool has_null_source = element.read_only_once().value();
      auto upload_data_stream =
          std::make_unique<ChunkedDataPipeUploadDataStream>(
              body, element.ReleaseChunkedDataPipeGetter(), has_null_source);
      if (element.read_only_once()) {
        upload_data_stream->EnableCache();
      }
      return upload_data_stream;
    }
  }

  auto opened_file = opened_files.begin();
  std::vector<std::unique_ptr<net::UploadElementReader>> element_readers;
  for (const auto& element : *body->elements()) {
    switch (element.type()) {
      case network::mojom::DataElementDataView::Tag::kBytes:
        element_readers.push_back(std::make_unique<BytesElementReader>(
            body, element.As<DataElementBytes>()));
        break;
      case network::mojom::DataElementDataView::Tag::kFile:
        CHECK(opened_file != opened_files.end(), base::NotFatalUntil::M130);
        element_readers.push_back(std::make_unique<FileElementReader>(
            body, file_task_runner, element.As<network::DataElementFile>(),
            std::move(*opened_file++)));
        break;
      case network::mojom::DataElementDataView::Tag::kDataPipe: {
        element_readers.push_back(std::make_unique<DataPipeElementReader>(
            body,
            element.As<network::DataElementDataPipe>().CloneDataPipeGetter()));
        break;
      }
      case network::mojom::DataElementDataView::Tag::kChunkedDataPipe: {
        // This shouldn't happen, as the traits logic should ensure that if
        // there's a chunked pipe, there's one and only one element.
        NOTREACHED_IN_MIGRATION();
        break;
      }
    }
  }
  DCHECK(opened_file == opened_files.end());

  return std::make_unique<net::ElementsUploadDataStream>(
      std::move(element_readers), body->identifier());
}

class SSLPrivateKeyInternal : public net::SSLPrivateKey {
 public:
  SSLPrivateKeyInternal(
      const std::string& provider_name,
      const std::vector<uint16_t>& algorithm_preferences,
      mojo::PendingRemote<mojom::SSLPrivateKey> ssl_private_key)
      : provider_name_(provider_name),
        algorithm_preferences_(algorithm_preferences),
        ssl_private_key_(std::move(ssl_private_key)) {
    ssl_private_key_.set_disconnect_handler(
        base::BindOnce(&SSLPrivateKeyInternal::HandleSSLPrivateKeyError,
                       base::Unretained(this)));
  }

  SSLPrivateKeyInternal(const SSLPrivateKeyInternal&) = delete;
  SSLPrivateKeyInternal& operator=(const SSLPrivateKeyInternal&) = delete;

  // net::SSLPrivateKey:
  std::string GetProviderName() override { return provider_name_; }

  std::vector<uint16_t> GetAlgorithmPreferences() override {
    return algorithm_preferences_;
  }

  void Sign(uint16_t algorithm,
            base::span<const uint8_t> input,
            net::SSLPrivateKey::SignCallback callback) override {
    std::vector<uint8_t> input_vector(input.begin(), input.end());
    if (!ssl_private_key_ || !ssl_private_key_.is_connected()) {
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE,
          base::BindOnce(std::move(callback),
                         net::ERR_SSL_CLIENT_AUTH_CERT_NO_PRIVATE_KEY,
                         input_vector));
      return;
    }

    ssl_private_key_->Sign(algorithm, input_vector,
                           base::BindOnce(&SSLPrivateKeyInternal::Callback,
                                          this, std::move(callback)));
  }

 private:
  ~SSLPrivateKeyInternal() override = default;

  void HandleSSLPrivateKeyError() { ssl_private_key_.reset(); }

  void Callback(net::SSLPrivateKey::SignCallback callback,
                int32_t net_error,
                const std::vector<uint8_t>& input) {
    DCHECK_LE(net_error, 0);
    DCHECK_NE(net_error, net::ERR_IO_PENDING);
    std::move(callback).Run(static_cast<net::Error>(net_error), input);
  }

  std::string provider_name_;
  std::vector<uint16_t> algorithm_preferences_;
  mojo::Remote<mojom::SSLPrivateKey> ssl_private_key_;
};

bool ShouldNotifyAboutCookie(net::CookieInclusionStatus status) {
  // Notify about cookies actually used, and those blocked by preferences ---
  // for purposes of cookie UI --- as well those carrying warnings pertaining to
  // SameSite features and cookies with non-ASCII domain attributes, in order to
  // issue a deprecation warning for them.

  // Filter out tentative secure source scheme warnings. They're used for netlog
  // debugging and not something we want to inform cookie observers about.
  status.RemoveWarningReason(
      net::CookieInclusionStatus::
          WARN_TENTATIVELY_ALLOWING_SECURE_SOURCE_SCHEME);

  return status.IsInclude() || status.ShouldWarn() ||
         status.HasExclusionReason(
             net::CookieInclusionStatus::EXCLUDE_USER_PREFERENCES) ||
         status.HasExclusionReason(
             net::CookieInclusionStatus::EXCLUDE_THIRD_PARTY_PHASEOUT) ||
         status.HasExclusionReason(
             net::CookieInclusionStatus::EXCLUDE_DOMAIN_NON_ASCII);
}

// Parses AcceptCHFrame and removes client hints already in the headers.
std::vector<mojom::WebClientHintsType> ComputeAcceptCHFrameHints(
    const std::string& accept_ch_frame,
    const net::HttpRequestHeaders& headers) {
  std::optional<std::vector<mojom::WebClientHintsType>> maybe_hints =
      ParseClientHintsHeader(accept_ch_frame);

  if (!maybe_hints)
    return {};

  // Only look at/add headers that aren't already present.
  std::vector<mojom::WebClientHintsType> hints;
  for (auto hint : maybe_hints.value()) {
    // ResourceWidth is only for images, which won't trigger a restart.
    if (hint == mojom::WebClientHintsType::kResourceWidth ||
        hint == mojom::WebClientHintsType::kResourceWidth_DEPRECATED) {
      continue;
    }

    const std::string header = GetClientHintToNameMap().at(hint);
    if (!headers.HasHeader(header))
      hints.push_back(hint);
  }

  return hints;
}

// Returns true if the |credentials_mode| of the request allows sending
// credentials.
bool ShouldAllowCredentials(mojom::CredentialsMode credentials_mode) {
  switch (credentials_mode) {
    case mojom::CredentialsMode::kInclude:
    // TODO(crbug.com/40619226): Make this work with
    // CredentialsMode::kSameOrigin.
    case mojom::CredentialsMode::kSameOrigin:
      return true;

    case mojom::CredentialsMode::kOmit:
    case mojom::CredentialsMode::kOmitBug_775438_Workaround:
      return false;
  }
}

// Returns true when the |credentials_mode| of the request allows sending client
// certificates.
bool ShouldSendClientCertificates(mojom::CredentialsMode credentials_mode) {
  switch (credentials_mode) {
    case mojom::CredentialsMode::kInclude:
    case mojom::CredentialsMode::kSameOrigin:
      return true;

    // TODO(crbug.com/40089326): Due to a bug, the default behavior does
    // not properly correspond to Fetch's "credentials mode", in that client
    // certificates will be sent if available, or the handshake will be aborted
    // to allow selecting a client cert.
    // With the feature kOmitCorsClientCert enabled, the correct
    // behavior is done; omit all client certs and continue the handshake
    // without sending one if requested.
    case mojom::CredentialsMode::kOmit:
      return !base::FeatureList::IsEnabled(features::kOmitCorsClientCert);

    case mojom::CredentialsMode::kOmitBug_775438_Workaround:
      return false;
  }
}

template <typename T>
T* PtrOrFallback(const mojo::Remote<T>& remote, T* fallback) {
  return remote.is_bound() ? remote.get() : fallback;
}

// Retrieves the Cookie header from either `cors_exempt_headers` or `headers`.
std::string GetCookiesFromHeaders(
    const net::HttpRequestHeaders& headers,
    const net::HttpRequestHeaders& cors_exempt_headers) {
  std::optional<std::string> cookies =
      cors_exempt_headers.GetHeader(net::HttpRequestHeaders::kCookie);
  if (!cookies) {
    cookies = headers.GetHeader(net::HttpRequestHeaders::kCookie);
  }
  return std::move(cookies).value_or(std::string());
}

net::HttpRequestHeaders AttachCookies(const net::HttpRequestHeaders& headers,
                                      const std::string& cookies_from_browser) {
  DCHECK(!cookies_from_browser.empty());

  // Parse the existing cookie line.
  std::string old_cookies = headers.GetHeader(net::HttpRequestHeaders::kCookie)
                                .value_or(std::string());
  net::cookie_util::ParsedRequestCookies parsed_cookies;

  net::cookie_util::ParseRequestCookieLine(old_cookies, &parsed_cookies);
  net::cookie_util::ParsedRequestCookies parsed_cookies_from_browser;
  net::cookie_util::ParseRequestCookieLine(cookies_from_browser,
                                           &parsed_cookies_from_browser);

  // Add the browser cookies to the request.
  for (auto cookie : parsed_cookies_from_browser) {
    DCHECK(!cookie.first.empty());

    // Ensure we're not adding duplicate cookies.
    auto it = std::find_if(
        parsed_cookies.begin(), parsed_cookies.end(),
        [&cookie](const net::cookie_util::ParsedRequestCookie& old_cookie) {
          return old_cookie.first == cookie.first;
        });
    if (it != parsed_cookies.end())
      continue;

    parsed_cookies.emplace_back(cookie.first, cookie.second);
  }

  net::HttpRequestHeaders updated_headers = headers;
  std::string updated_cookies =
      net::cookie_util::SerializeRequestCookieLine(parsed_cookies);
  updated_headers.SetHeader(net::HttpRequestHeaders::kCookie, updated_cookies);

  return updated_headers;
}

const char* GetDestinationTypePartString(
    network::mojom::RequestDestination destination) {
  if (destination == network::mojom::RequestDestination::kDocument) {
    return "MainFrame";
  } else if (destination == network::mojom::RequestDestination::kFrame ||
             destination == network::mojom::RequestDestination::kIframe) {
    return "SubFrame";
  }
  return "Subresource";
}

const char* GetCertStatePartString(const net::SSLInfo& ssl_info) {
  if (!ssl_info.cert.get()) {
    return "NoCert";
  }
  return ssl_info.is_issued_by_known_root ? "KnownRootCert" : "UnknownRootCert";
}

void MaybeRecordSharedDictionaryUsedResponseMetrics(
    int error_code,
    network::mojom::RequestDestination destination,
    const net::HttpResponseInfo& response_info,
    bool shared_dictionary_allowed_check_passed) {
  if (response_info.was_cached) {
    return;
  }
  if (response_info.did_use_shared_dictionary) {
    base::UmaHistogramSparse(
        base::StrCat({"Net.SharedDictionaryUsedResponseErrorCodes2.",
                      GetDestinationTypePartString(destination), ".",
                      GetCertStatePartString(response_info.ssl_info)}),
        -error_code);
  }

  if (shared_dictionary_allowed_check_passed &&
      destination == network::mojom::RequestDestination::kDocument) {
    base::UmaHistogramBoolean(
        base::StrCat(
            {"Net.SharedDictionaryUsedByResponseWhenAvailable2.MainFrame.",
             net::HttpConnectionInfoCoarseToString(
                 net::HttpConnectionInfoToCoarse(
                     response_info.connection_info)),
             ".", GetCertStatePartString(response_info.ssl_info)}),
        response_info.did_use_shared_dictionary);
  }
}

std::vector<network::mojom::HttpRawHeaderPairPtr>
ResponseHeaderToRawHeaderPairs(
    const net::HttpResponseHeaders& response_headers) {
  std::vector<network::mojom::HttpRawHeaderPairPtr> header_array;
  size_t iterator = 0;
  std::string name, value;
  while (response_headers.EnumerateHeaderLines(&iterator, &name, &value)) {
    header_array.emplace_back(std::in_place, std::move(name), std::move(value));
  }
  return header_array;
}

bool IsMultiplexedConnection(const net::HttpResponseInfo& response_info) {
  switch (net::HttpConnectionInfoToCoarse(response_info.connection_info)) {
    case net::HttpConnectionInfoCoarse::kHTTP1:
      return false;
    case net::HttpConnectionInfoCoarse::kHTTP2:
    case net::HttpConnectionInfoCoarse::kQUIC:
      return true;
    case net::HttpConnectionInfoCoarse::kOTHER:
      return false;
  }
}

bool IncludesValidLoadField(const net::HttpResponseHeaders* headers) {
  if (!headers) {
    return false;
  }

  std::string header_value;
  if (!headers->GetNormalizedHeader(kActivateStorageAccessHeader,
                                    &header_value)) {
    return false;
  }
  const std::optional<net::structured_headers::ParameterizedItem> item =
      net::structured_headers::ParseItem(header_value);
  if (!item.has_value()) {
    return false;
  }
  return item->item.is_token() && item->item.GetString() == "load";
}

}  // namespace

URLLoader::MaybeSyncURLLoaderClient::MaybeSyncURLLoaderClient(
    mojo::PendingRemote<mojom::URLLoaderClient> mojo_client,
    base::WeakPtr<mojom::URLLoaderClient> sync_client)
    : mojo_client_(std::move(mojo_client)),
      sync_client_(std::move(sync_client)) {}

URLLoader::MaybeSyncURLLoaderClient::~MaybeSyncURLLoaderClient() = default;

void URLLoader::MaybeSyncURLLoaderClient::Reset() {
  mojo_client_.reset();
  sync_client_.reset();
}

mojo::PendingReceiver<mojom::URLLoaderClient>
URLLoader::MaybeSyncURLLoaderClient::BindNewPipeAndPassReceiver() {
  sync_client_.reset();
  return mojo_client_.BindNewPipeAndPassReceiver();
}

mojom::URLLoaderClient* URLLoader::MaybeSyncURLLoaderClient::Get() {
  if (sync_client_)
    return sync_client_.get();
  if (mojo_client_)
    return mojo_client_.get();
  return nullptr;
}

URLLoader::PartialLoadInfo::PartialLoadInfo(net::LoadStateWithParam load_state,
                                            net::UploadProgress upload_progress)
    : load_state(std::move(load_state)),
      upload_progress(std::move(upload_progress)) {}

URLLoader::URLLoader(
    URLLoaderContext& context,
    DeleteCallback delete_callback,
    mojo::PendingReceiver<mojom::URLLoader> url_loader_receiver,
    int32_t options,
    const ResourceRequest& request,
    mojo::PendingRemote<mojom::URLLoaderClient> url_loader_client,
    base::WeakPtr<mojom::URLLoaderClient> sync_url_loader_client,
    const net::NetworkTrafficAnnotationTag& traffic_annotation,
    base::StrictNumeric<int32_t> request_id,
    int keepalive_request_size,
    base::WeakPtr<KeepaliveStatisticsRecorder> keepalive_statistics_recorder,
    std::unique_ptr<TrustTokenRequestHelperFactory> trust_token_helper_factory,
    SharedDictionaryManager* shared_dictionary_manager,
    std::unique_ptr<SharedDictionaryAccessChecker> shared_dictionary_checker,
    mojo::PendingRemote<mojom::CookieAccessObserver> cookie_observer,
    mojo::PendingRemote<mojom::TrustTokenAccessObserver> trust_token_observer,
    mojo::PendingRemote<mojom::URLLoaderNetworkServiceObserver>
        url_loader_network_observer,
    mojo::PendingRemote<mojom::DevToolsObserver> devtools_observer,
    mojo::PendingRemote<mojom::AcceptCHFrameObserver> accept_ch_frame_observer,
    std::unique_ptr<AttributionRequestHelper> attribution_request_helper,
    bool shared_storage_writable_eligible)
    : url_request_context_(context.GetUrlRequestContext()),
      network_context_client_(context.GetNetworkContextClient()),
      delete_callback_(std::move(delete_callback)),
      options_(options),
      resource_type_(request.resource_type),
      is_load_timing_enabled_(request.enable_load_timing),
      factory_params_(context.GetFactoryParams()),
      coep_reporter_(context.GetCoepReporter()),
      request_id_(request_id),
      keepalive_request_size_(keepalive_request_size),
      keepalive_(request.keepalive),
      do_not_prompt_for_login_(request.do_not_prompt_for_login),
      receiver_(this, std::move(url_loader_receiver)),
      url_loader_client_(std::move(url_loader_client),
                         std::move(sync_url_loader_client)),
      writable_handle_watcher_(FROM_HERE,
                               mojo::SimpleWatcher::ArmingPolicy::MANUAL,
                               base::SequencedTaskRunner::GetCurrentDefault()),
      peer_closed_handle_watcher_(
          FROM_HERE,
          mojo::SimpleWatcher::ArmingPolicy::MANUAL,
          base::SequencedTaskRunner::GetCurrentDefault()),
      per_factory_orb_state_(context.GetMutableOrbState()),
      devtools_request_id_(request.devtools_request_id),
      request_mode_(request.mode),
      request_credentials_mode_(request.credentials_mode),
      has_user_activation_(request.trusted_params &&
                           request.trusted_params->has_user_activation),
      request_destination_(request.destination),
      resource_scheduler_client_(context.GetResourceSchedulerClient()),
      keepalive_statistics_recorder_(std::move(keepalive_statistics_recorder)),
      custom_proxy_pre_cache_headers_(request.custom_proxy_pre_cache_headers),
      custom_proxy_post_cache_headers_(request.custom_proxy_post_cache_headers),
      fetch_window_id_(request.fetch_window_id),
      private_network_access_checker_(
          request,
          factory_params_->client_security_state.get(),
          options_),
      trust_token_helper_factory_(std::move(trust_token_helper_factory)),
      storage_access_api_status_(request.storage_access_api_status),
      shared_dictionary_checker_(std::move(shared_dictionary_checker)),
      attribution_request_helper_(std::move(attribution_request_helper)),
      origin_access_list_(context.GetOriginAccessList()),
      cookie_observer_remote_(std::move(cookie_observer)),
      cookie_observer_(PtrOrFallback(cookie_observer_remote_,
                                     context.GetCookieAccessObserver())),
      trust_token_observer_remote_(std::move(trust_token_observer)),
      trust_token_observer_(
          PtrOrFallback(trust_token_observer_remote_,
                        context.GetTrustTokenAccessObserver())),
      url_loader_network_observer_remote_(
          std::move(url_loader_network_observer)),
      url_loader_network_observer_(
          PtrOrFallback(url_loader_network_observer_remote_,
                        context.GetURLLoaderNetworkServiceObserver())),
      devtools_observer_remote_(std::move(devtools_observer)),
      devtools_observer_(PtrOrFallback(devtools_observer_remote_,
                                       context.GetDevToolsObserver())),
      shared_storage_request_helper_(
          std::make_unique<SharedStorageRequestHelper>(
              shared_storage_writable_eligible,
              url_loader_network_observer_)),
      has_fetch_streaming_upload_body_(HasFetchStreamingUploadBody(&request)),
      accept_ch_frame_observer_(std::move(accept_ch_frame_observer)),
      allow_cookies_from_browser_(
          request.trusted_params &&
          request.trusted_params->allow_cookies_from_browser),
      cookies_from_browser_(
          allow_cookies_from_browser_
              ? GetCookiesFromHeaders(request.headers,
                                      request.cors_exempt_headers)
              : std::string()),
      include_request_cookies_with_response_(
          request.trusted_params &&
          request.trusted_params->include_request_cookies_with_response),
      provide_data_use_updates_(context.DataUseUpdatesEnabled()) {
  DCHECK(delete_callback_);

  if (options_ & mojom::kURLLoadOptionReadAndDiscardBody) {
    CHECK(!(options_ & mojom::kURLLoadOptionSniffMimeType))
        << "options ReadAndDiscardBody and SniffMimeType cannot be used "
           "together";
    if (factory_params_->is_orb_enabled) {
      // TODO(ricea): Make ReadAndDiscardBody and ORB work together.
      LOG(WARNING) << "Disabling ReadAndDiscardBody because ORB is enabled";
      options_ &= ~mojom::kURLLoadOptionReadAndDiscardBody;
    } else {
      discard_buffer_ =
          base::MakeRefCounted<net::IOBufferWithSize>(kDiscardBufferSize);
    }
  }

  mojom::TrustedURLLoaderHeaderClient* url_loader_header_client =
      context.GetUrlLoaderHeaderClient();
  if (url_loader_header_client &&
      (options_ & mojom::kURLLoadOptionUseHeaderClient)) {
    if (options_ & mojom::kURLLoadOptionAsCorsPreflight) {
      url_loader_header_client->OnLoaderForCorsPreflightCreated(
          request, header_client_.BindNewPipeAndPassReceiver());
    } else {
      url_loader_header_client->OnLoaderCreated(
          request_id_, header_client_.BindNewPipeAndPassReceiver());
    }
    // Make sure the loader dies if |header_client_| has an error, otherwise
    // requests can hang.
    header_client_.set_disconnect_handler(
        base::BindOnce(&URLLoader::OnMojoDisconnect, base::Unretained(this)));
  }
  if (devtools_request_id()) {
    options_ |= mojom::kURLLoadOptionSendSSLInfoWithResponse |
                mojom::kURLLoadOptionSendSSLInfoForCertificateError;
  }
  receiver_.set_disconnect_handler(
      base::BindOnce(&URLLoader::OnMojoDisconnect, base::Unretained(this)));
  url_request_ = url_request_context_->CreateRequest(
      request.url, request.priority, this, traffic_annotation,
      /*is_for_websockets=*/false, request.net_log_create_info);

  TRACE_EVENT(
      "loading", "URLLoader::URLLoader",
      perfetto::Flow::ProcessScoped(url_request_->net_log().source().id));

  // |cors_exempt_headers| must be merged here to avoid breaking CORS checks.
  // They are non-empty when the values are given by the UA code, therefore
  // they should be ignored by CORS checks.
  net::HttpRequestHeaders merged_headers = request.headers;
  merged_headers.MergeFrom(ComputeAttributionReportingHeaders(request));
  merged_headers.MergeFrom(request.cors_exempt_headers);

  // This should be ensured by the CorsURLLoaderFactory(), which is called
  // before URLLoaders are created.
  DCHECK(AreRequestHeadersSafe(merged_headers));

  // When a service worker forwards a navigation request it uses the
  // service worker's IsolationInfo.  This causes the cookie code to fail
  // to send SameSite=Lax cookies for main-frame navigations passed through
  // a service worker.  To fix this we check to see if the original destination
  // of the request was a main frame document and then set a flag indicating
  // SameSite cookies should treat it as a main frame navigation.
  const bool force_main_frame_for_same_site_cookies =
      request.mode == mojom::RequestMode::kNavigate &&
      request.destination == mojom::RequestDestination::kEmpty &&
      request.original_destination == mojom::RequestDestination::kDocument;

  const net::SecureDnsPolicy secure_dns_policy =
      factory_params_->disable_secure_dns ||
              (request.trusted_params &&
               request.trusted_params->disable_secure_dns)
          ? net::SecureDnsPolicy::kDisable
          : net::SecureDnsPolicy::kAllow;

  const net::RedirectInfo::FirstPartyURLPolicy first_party_url_policy =
      request.update_first_party_url_on_redirect
          ? net::RedirectInfo::FirstPartyURLPolicy::UPDATE_URL_ON_REDIRECT
          : net::RedirectInfo::FirstPartyURLPolicy::NEVER_CHANGE_URL;

  if (!request.navigation_redirect_chain.empty()) {
    DCHECK_EQ(request.mode, mojom::RequestMode::kNavigate);
  }

  ConfigureRequest(
      request.url, request.method, request.site_for_cookies,
      /*force_ignore_site_for_cookies=*/
      ShouldForceIgnoreSiteForCookies(request),
      request.navigation_redirect_chain, request.referrer,
      request.referrer_policy,
      /*upgrade_if_insecure=*/request.upgrade_if_insecure,
      /*is_ad_tagged=*/request.is_ad_tagged,
      /*isolation_info=*/
      GetIsolationInfo(factory_params_->isolation_info,
                       factory_params_->automatically_assign_isolation_info,
                       request),
      /*force_main_frame_for_same_site_cookies=*/
      force_main_frame_for_same_site_cookies, secure_dns_policy,
      std::move(merged_headers), request.devtools_accepted_stream_types,
      request.request_initiator, first_party_url_policy,
      /*request_load_flags=*/request.load_flags,
      /*priority_incremental=*/request.priority_incremental,
      /*cookie_setting_overrides=*/
      CalculateCookieSettingOverrides(factory_params_->cookie_setting_overrides,
                                      request),
      /*shared_dictionary_getter=*/
      shared_dictionary_manager
          ? std::make_optional(
                shared_dictionary_manager->MaybeCreateSharedDictionaryGetter(
                    request.load_flags, request_destination_))
          : std::nullopt);

  if (context.ShouldRequireIsolationInfo()) {
    DCHECK(!url_request_->isolation_info().IsEmpty());
  }

  throttling_token_ = network::ScopedThrottlingToken::MaybeCreate(
      url_request_->net_log().source().id, request.throttling_profile_id);

  if (keepalive_ && keepalive_statistics_recorder_) {
    keepalive_statistics_recorder_->OnLoadStarted(
        *factory_params_->top_frame_id, keepalive_request_size_);
  }

  if (request.net_log_reference_info) {
    // Log source object that created the request, if available.
    url_request_->net_log().AddEventReferencingSource(
        net::NetLogEventType::CREATED_BY,
        request.net_log_reference_info.value());
  }

  // Resolve elements from request_body and prepare upload data.
  if (request.request_body.get()) {
    OpenFilesForUpload(request);
    return;
  }

  ProcessOutboundTrustTokenInterceptor(request);
}

void URLLoader::ConfigureRequest(
    const GURL& url,
    std::string_view method,
    const net::SiteForCookies& site_for_cookies,
    bool force_ignore_site_for_cookies,
    const std::vector<GURL>& url_chain,
    const GURL& referrer,
    net::ReferrerPolicy referrer_policy,
    bool upgrade_if_insecure,
    bool is_ad_tagged,
    std::optional<net::IsolationInfo> isolation_info,
    bool force_main_frame_for_same_site_cookies,
    net::SecureDnsPolicy secure_dns_policy,
    net::HttpRequestHeaders extra_request_headers,
    const std::optional<std::vector<net::SourceStream::SourceType>>&
        accepted_stream_types,
    const std::optional<url::Origin>& initiator,
    net::RedirectInfo::FirstPartyURLPolicy first_party_url_policy,
    int request_load_flags,
    bool priority_incremental,
    net::CookieSettingOverrides cookie_setting_overrides,
    std::optional<net::SharedDictionaryGetter> shared_dictionary_getter) {
  url_request_->set_method(method);
  url_request_->set_site_for_cookies(site_for_cookies);
  url_request_->set_force_ignore_site_for_cookies(
      force_ignore_site_for_cookies);
  if (!url_chain.empty()) {
    url_request_->SetURLChain(url_chain);
  }
  url_request_->SetReferrer(referrer.GetAsReferrer().spec());
  url_request_->set_referrer_policy(referrer_policy);
  url_request_->set_upgrade_if_insecure(upgrade_if_insecure);
  url_request_->set_ad_tagged(is_ad_tagged);

  if (isolation_info) {
    url_request_->set_isolation_info(std::move(isolation_info).value());
  }

  url_request_->set_force_main_frame_for_same_site_cookies(
      force_main_frame_for_same_site_cookies);

  url_request_->SetSecureDnsPolicy(secure_dns_policy);

  url_request_->SetExtraRequestHeaders(std::move(extra_request_headers));

  url_request_->SetUserData(kUserDataKey,
                            std::make_unique<UnownedPointer>(this));
  url_request_->set_accepted_stream_types(accepted_stream_types);

  url_request_->set_initiator(initiator);

  // Note: There are some ordering dependencies here. `SetRequestCredentials`
  // depends on `SetLoadFlags`; `CalculateStorageAccessStatus` depends on
  // `cookie_setting_overrides` and `SetRequestCredentials`.
  // `SetFetchMetadataHeaders` will depend on
  // `url_request_->storage_access_status()`, once https://crbug.com/366284840
  // is fixed.
  url_request_->cookie_setting_overrides() = cookie_setting_overrides;
  url_request_->SetLoadFlags(request_load_flags);
  SetRequestCredentials(url);
  url_request_->set_storage_access_status(
      url_request_->CalculateStorageAccessStatus());

  SetFetchMetadataHeaders(url_request_.get(), request_mode_,
                          has_user_activation_, request_destination_, nullptr,
                          *factory_params_, *origin_access_list_);

  url_request_->set_first_party_url_policy(first_party_url_policy);

  url_request_->SetPriorityIncremental(priority_incremental);

  url_request_->SetRequestHeadersCallback(base::BindRepeating(
      &URLLoader::SetRawRequestHeadersAndNotify, base::Unretained(this)));
  if (shared_dictionary_checker_) {
    url_request_->SetIsSharedDictionaryReadAllowedCallback(base::BindRepeating(
        &URLLoader::IsSharedDictionaryReadAllowed, base::Unretained(this)));
  }

  if (devtools_request_id()) {
    url_request_->SetResponseHeadersCallback(base::BindRepeating(
        &URLLoader::SetRawResponseHeaders, base::Unretained(this)));
  }

  url_request_->SetEarlyResponseHeadersCallback(base::BindRepeating(
      &URLLoader::NotifyEarlyResponse, base::Unretained(this)));

  if (shared_dictionary_getter) {
    url_request_->SetSharedDictionaryGetter(
        std::move(shared_dictionary_getter).value());
  }
}

// This class is used to manage the queue of pending file upload operations
// initiated by the URLLoader::OpenFilesForUpload().
class URLLoader::FileOpenerForUpload {
 public:
  typedef base::OnceCallback<void(int, std::vector<base::File>)>
      SetUpUploadCallback;

  FileOpenerForUpload(std::vector<base::FilePath> paths,
                      URLLoader* url_loader,
                      int32_t process_id,
                      mojom::NetworkContextClient* const network_context_client,
                      SetUpUploadCallback set_up_upload_callback)
      : paths_(std::move(paths)),
        url_loader_(url_loader),
        process_id_(process_id),
        network_context_client_(network_context_client),
        set_up_upload_callback_(std::move(set_up_upload_callback)) {
    StartOpeningNextBatch();
  }

  FileOpenerForUpload(const FileOpenerForUpload&) = delete;
  FileOpenerForUpload& operator=(const FileOpenerForUpload&) = delete;

  ~FileOpenerForUpload() {
    if (!opened_files_.empty())
      PostCloseFiles(std::move(opened_files_));
  }

 private:
  static void OnFilesForUploadOpened(
      base::WeakPtr<FileOpenerForUpload> file_opener,
      size_t num_files_requested,
      int error_code,
      std::vector<base::File> opened_files) {
    if (!file_opener) {
      PostCloseFiles(std::move(opened_files));
      return;
    }

    if (error_code == net::OK && num_files_requested != opened_files.size())
      error_code = net::ERR_FAILED;

    if (error_code != net::OK) {
      PostCloseFiles(std::move(opened_files));
      file_opener->FilesForUploadOpenedDone(error_code);
      return;
    }

    for (base::File& file : opened_files)
      file_opener->opened_files_.push_back(std::move(file));

    if (file_opener->opened_files_.size() < file_opener->paths_.size()) {
      file_opener->StartOpeningNextBatch();
      return;
    }

    file_opener->FilesForUploadOpenedDone(net::OK);
  }

  // |opened_files| need to be closed on a blocking task runner, so move the
  // |opened_files| vector onto a sequence that can block so it gets destroyed
  // there.
  static void PostCloseFiles(std::vector<base::File> opened_files) {
    base::ThreadPool::PostTask(
        FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_BLOCKING},
        base::DoNothingWithBoundArgs(std::move(opened_files)));
  }

  void StartOpeningNextBatch() {
    size_t num_files_to_request = std::min(paths_.size() - opened_files_.size(),
                                           kMaxFileUploadRequestsPerBatch);
    std::vector<base::FilePath> batch_paths(
        paths_.begin() + opened_files_.size(),
        paths_.begin() + opened_files_.size() + num_files_to_request);

    network_context_client_->OnFileUploadRequested(
        process_id_, /*async=*/true, batch_paths,
        url_loader_->url_request_->url(),
        base::BindOnce(&FileOpenerForUpload::OnFilesForUploadOpened,
                       weak_ptr_factory_.GetWeakPtr(), num_files_to_request));
  }

  void FilesForUploadOpenedDone(int error_code) {
    url_loader_->url_request_->LogUnblocked();

    if (error_code == net::OK)
      std::move(set_up_upload_callback_).Run(net::OK, std::move(opened_files_));
    else
      std::move(set_up_upload_callback_).Run(error_code, {});
  }

  // The paths of files for upload
  const std::vector<base::FilePath> paths_;
  const raw_ptr<URLLoader> url_loader_;
  const int32_t process_id_;
  const raw_ptr<mojom::NetworkContextClient> network_context_client_;
  SetUpUploadCallback set_up_upload_callback_;
  // The files opened so far.
  std::vector<base::File> opened_files_;

  base::WeakPtrFactory<FileOpenerForUpload> weak_ptr_factory_{this};
};

void URLLoader::OpenFilesForUpload(const ResourceRequest& request) {
  std::vector<base::FilePath> paths;
  for (const auto& element : *request.request_body.get()->elements()) {
    if (element.type() == mojom::DataElementDataView::Tag::kFile) {
      paths.push_back(element.As<network::DataElementFile>().path());
    }
  }
  if (paths.empty()) {
    SetUpUpload(request, net::OK, std::vector<base::File>());
    return;
  }
  if (!network_context_client_) {
    DLOG(ERROR) << "URLLoader couldn't upload a file because no "
                   "NetworkContextClient is set.";
    // Defer calling NotifyCompleted to make sure the URLLoader finishes
    // initializing before getting deleted.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&URLLoader::NotifyCompleted,
                       weak_ptr_factory_.GetWeakPtr(), net::ERR_ACCESS_DENIED));
    return;
  }
  url_request_->LogBlockedBy("Opening Files");
  file_opener_for_upload_ = std::make_unique<FileOpenerForUpload>(
      std::move(paths), this, factory_params_->process_id,
      network_context_client_,
      base::BindOnce(&URLLoader::SetUpUpload, base::Unretained(this), request));
}

void URLLoader::SetUpUpload(const ResourceRequest& request,
                            int error_code,
                            std::vector<base::File> opened_files) {
  if (error_code != net::OK) {
    DCHECK(opened_files.empty());
    // Defer calling NotifyCompleted to make sure the URLLoader finishes
    // initializing before getting deleted.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&URLLoader::NotifyCompleted,
                                  weak_ptr_factory_.GetWeakPtr(), error_code));
    return;
  }
  scoped_refptr<base::SequencedTaskRunner> task_runner =
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_VISIBLE});
  url_request_->set_upload(CreateUploadDataStream(
      request.request_body.get(), opened_files, task_runner.get()));

  if (request.enable_upload_progress) {
    upload_progress_tracker_ = std::make_unique<UploadProgressTracker>(
        FROM_HERE,
        base::BindRepeating(&URLLoader::SendUploadProgress,
                            base::Unretained(this)),
        url_request_.get());
  }
  ProcessOutboundTrustTokenInterceptor(request);
}

void URLLoader::ProcessOutboundSharedStorageInterceptor() {
  DCHECK(shared_storage_request_helper_);
  shared_storage_request_helper_->ProcessOutgoingRequest(*url_request_);
  ScheduleStart();
}

// TODO(crbug.com/40254265): Parallelize Private State Tokens and
// Attribution operations.
void URLLoader::ProcessOutboundAttributionInterceptor() {
  if (!attribution_request_helper_) {
    ProcessOutboundSharedStorageInterceptor();
    return;
  }

  attribution_request_helper_->Begin(
      *url_request_,
      base::BindOnce(&URLLoader::ProcessOutboundSharedStorageInterceptor,
                     weak_ptr_factory_.GetWeakPtr()));
}

void URLLoader::ProcessOutboundTrustTokenInterceptor(
    const ResourceRequest& request) {
  if (!request.trust_token_params) {
    ProcessOutboundAttributionInterceptor();
    return;
  }

  // Trust token operations other than signing cannot be served from cache
  // because it needs to send the server the Trust Tokens request header and
  // get the corresponding response header. It is okay to cache the results in
  // case subsequent requests are made to the same URL in non-trust-token
  // settings.
  if (request.trust_token_params->operation !=
      mojom::TrustTokenOperationType::kSigning) {
    url_request_->SetLoadFlags(url_request_->load_flags() |
                               net::LOAD_BYPASS_CACHE);
  }

  // Since the request has trust token parameters, |trust_token_helper_factory_|
  // is guaranteed to be non-null by URLLoader's constructor's contract.
  DCHECK(trust_token_helper_factory_);

  trust_token_helper_factory_->CreateTrustTokenHelperForRequest(
      url_request_->isolation_info().top_frame_origin().value_or(url::Origin()),
      url_request_->extra_request_headers(), request.trust_token_params.value(),
      url_request_->net_log(),
      base::BindOnce(&URLLoader::OnDoneConstructingTrustTokenHelper,
                     weak_ptr_factory_.GetWeakPtr(),
                     request.trust_token_params->operation));
}

void URLLoader::OnDoneConstructingTrustTokenHelper(
    mojom::TrustTokenOperationType operation,
    TrustTokenStatusOrRequestHelper status_or_helper) {
  trust_token_operation_ = operation;

  if (trust_token_observer_) {
    const net::IsolationInfo& isolation_info = url_request_->isolation_info();
    url::Origin top_frame_origin;
    if (isolation_info.top_frame_origin()) {
      top_frame_origin = *isolation_info.top_frame_origin();
    }

    bool token_operation_unauthorized =
        status_or_helper.status() ==
        mojom::TrustTokenOperationStatus::kUnauthorized;
    switch (operation) {
      case mojom::TrustTokenOperationType::kIssuance:
        trust_token_observer_->OnTrustTokensAccessed(
            mojom::TrustTokenAccessDetails::NewIssuance(
                mojom::TrustTokenIssuanceDetails::New(
                    top_frame_origin, url::Origin::Create(url_request_->url()),
                    token_operation_unauthorized)));
        break;
      case mojom::TrustTokenOperationType::kRedemption:
        trust_token_observer_->OnTrustTokensAccessed(
            mojom::TrustTokenAccessDetails::NewRedemption(
                mojom::TrustTokenRedemptionDetails::New(
                    top_frame_origin, url::Origin::Create(url_request_->url()),
                    token_operation_unauthorized)));
        break;
      case mojom::TrustTokenOperationType::kSigning:
        trust_token_observer_->OnTrustTokensAccessed(
            mojom::TrustTokenAccessDetails::NewSigning(
                mojom::TrustTokenSigningDetails::New(
                    top_frame_origin, token_operation_unauthorized)));
        break;
    }
  }

  if (!status_or_helper.ok()) {
    trust_token_status_ = status_or_helper.status();

    // Defer calling NotifyCompleted to make sure the URLLoader
    // finishes initializing before getting deleted.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&URLLoader::NotifyCompleted,
                                  weak_ptr_factory_.GetWeakPtr(),
                                  net::ERR_TRUST_TOKEN_OPERATION_FAILED));

    if (devtools_observer_ && devtools_request_id()) {
      mojom::TrustTokenOperationResultPtr operation_result =
          mojom::TrustTokenOperationResult::New();
      operation_result->status = *trust_token_status_;
      operation_result->operation = operation;
      devtools_observer_->OnTrustTokenOperationDone(
          devtools_request_id().value(), std::move(operation_result));
    }
    return;
  }

  trust_token_helper_ = status_or_helper.TakeOrCrash();
  trust_token_helper_->Begin(
      url_request_->url(),
      base::BindOnce(&URLLoader::OnDoneBeginningTrustTokenOperation,
                     weak_ptr_factory_.GetWeakPtr()));
}

void URLLoader::OnDoneBeginningTrustTokenOperation(
    std::optional<net::HttpRequestHeaders> headers,
    mojom::TrustTokenOperationStatus status) {
  trust_token_status_ = status;

  if (trust_token_operation_) {
    base::UmaHistogramEnumeration(
        base::StrCat({"Net.TrustTokens.OperationOutcome.",
                      internal::TrustTokenOperationTypeToString(
                          *trust_token_operation_)}),
        status);
  }

  // In case the operation failed or it succeeded in a manner where the request
  // does not need to be sent onwards, the DevTools event is emitted from here.
  // Otherwise the DevTools event is always emitted from
  // |OnDoneFinalizingTrustTokenOperation|.
  if (status != mojom::TrustTokenOperationStatus::kOk) {
    DCHECK(!headers);
    MaybeSendTrustTokenOperationResultToDevTools();
  }

  if (status == mojom::TrustTokenOperationStatus::kOk) {
    DCHECK(headers);
    for (const auto& header_pair : headers->GetHeaderVector()) {
      url_request_->SetExtraRequestHeaderByName(
          header_pair.key, header_pair.value, /*overwrite=*/true);
    }

    ProcessOutboundAttributionInterceptor();
  } else if (status == mojom::TrustTokenOperationStatus::kAlreadyExists ||
             status == mojom::TrustTokenOperationStatus::
                           kOperationSuccessfullyFulfilledLocally) {
    // The Trust Tokens operation succeeded without needing to send the request;
    // we return early with an "error" representing this success.
    //
    // Here and below, defer calling NotifyCompleted to make sure the URLLoader
    // finishes initializing before getting deleted.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(
            &URLLoader::NotifyCompleted, weak_ptr_factory_.GetWeakPtr(),
            net::ERR_TRUST_TOKEN_OPERATION_SUCCESS_WITHOUT_SENDING_REQUEST));
  } else {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&URLLoader::NotifyCompleted,
                                  weak_ptr_factory_.GetWeakPtr(),
                                  net::ERR_TRUST_TOKEN_OPERATION_FAILED));
  }
}

void URLLoader::ScheduleStart() {
  TRACE_EVENT(
      "loading", "URLLoader::ScheduleStart",
      perfetto::Flow::ProcessScoped(url_request_->net_log().source().id));
  bool defer = false;
  if (resource_scheduler_client_) {
    resource_scheduler_request_handle_ =
        resource_scheduler_client_->ScheduleRequest(
            !(options_ & mojom::kURLLoadOptionSynchronous), url_request_.get());
    resource_scheduler_request_handle_->set_resume_callback(
        base::BindOnce(&URLLoader::ResumeStart, base::Unretained(this)));
    resource_scheduler_request_handle_->WillStartRequest(&defer);
  }
  if (defer)
    url_request_->LogBlockedBy("ResourceScheduler");
  else
    url_request_->Start();
}

URLLoader::~URLLoader() {
  TRACE_EVENT(
      "loading", "URLLoader::~URLLoader",
      perfetto::Flow::ProcessScoped(url_request_->net_log().source().id));
  if (keepalive_ && keepalive_statistics_recorder_) {
    keepalive_statistics_recorder_->OnLoadFinished(
        *factory_params_->top_frame_id, keepalive_request_size_);
  }

  if (!cookie_access_details_.empty()) {
    // In case the response wasn't received successfully sent the call now.
    // Note `cookie_observer_` is guaranteed non-null since
    // `cookie_access_details_` is only appended to when it is valid.
    cookie_observer_->OnCookiesAccessed(std::move(cookie_access_details_));
  }
}

// static
const void* const URLLoader::kUserDataKey = &URLLoader::kUserDataKey;

void URLLoader::FollowRedirect(
    const std::vector<std::string>& removed_headers,
    const net::HttpRequestHeaders& modified_headers,
    const net::HttpRequestHeaders& modified_cors_exempt_headers,
    const std::optional<GURL>& new_url) {
  if (!deferred_redirect_url_) {
    NOTREACHED_IN_MIGRATION();
    return;
  }

  // Set seen_raw_request_headers_ to false in order to make sure this redirect
  // also calls the devtools observer.
  seen_raw_request_headers_ = false;

  // Removing headers can't make the set of pre-existing headers unsafe, but
  // adding headers can.
  if (!AreRequestHeadersSafe(modified_headers) ||
      !AreRequestHeadersSafe(modified_cors_exempt_headers)) {
    NotifyCompleted(net::ERR_INVALID_ARGUMENT);
    // |this| may have been deleted.
    return;
  }

  // Store any cookies passed from the browser process to later attach them to
  // the request.
  if (allow_cookies_from_browser_) {
    cookies_from_browser_ =
        GetCookiesFromHeaders(modified_headers, modified_cors_exempt_headers);
  }

  // Reset the state of the PNA checker - redirects should be treated like new
  // requests by the same client.
  if (new_url.has_value()) {
    private_network_access_checker_.ResetForRedirect(*new_url);
  } else {
    private_network_access_checker_.ResetForRedirect(*deferred_redirect_url_);
  }

  // Propagate removal or restoration of shared storage eligiblity to the helper
  // if the "Sec-Shared-Storage-Writable" request header has been removed or
  // restored.
  DCHECK(shared_storage_request_helper_);
  shared_storage_request_helper_->UpdateSharedStorageWritableEligible(
      removed_headers, modified_headers);

  deferred_redirect_url_.reset();
  new_redirect_url_ = new_url;

  net::HttpRequestHeaders merged_modified_headers = modified_headers;
  merged_modified_headers.MergeFrom(modified_cors_exempt_headers);
  url_request_->FollowDeferredRedirect(removed_headers,
                                       merged_modified_headers);
  new_redirect_url_.reset();
}

void URLLoader::SetPriority(net::RequestPriority priority,
                            int32_t intra_priority_value) {
  if (url_request_ && resource_scheduler_client_) {
    resource_scheduler_client_->ReprioritizeRequest(
        url_request_.get(), priority, intra_priority_value);
  }
}

void URLLoader::PauseReadingBodyFromNet() {
  DVLOG(1) << "URLLoader pauses fetching response body for "
           << (url_request_ ? url_request_->original_url().spec()
                            : "a URL that has completed loading or failed.");

  // Please note that we pause reading body in all cases. Even if the URL
  // request indicates that the response was cached, there could still be
  // network activity involved. For example, the response was only partially
  // cached.
  should_pause_reading_body_ = true;
}

void URLLoader::ResumeReadingBodyFromNet() {
  DVLOG(1) << "URLLoader resumes fetching response body for "
           << (url_request_ ? url_request_->original_url().spec()
                            : "a URL that has completed loading or failed.");
  should_pause_reading_body_ = false;

  if (paused_reading_body_) {
    paused_reading_body_ = false;
    ReadMore();
  }
}

PrivateNetworkAccessCheckResult URLLoader::PrivateNetworkAccessCheck(
    const net::TransportInfo& transport_info) {
  PrivateNetworkAccessCheckResult result =
      private_network_access_checker_.Check(transport_info);

  mojom::IPAddressSpace response_address_space =
      *private_network_access_checker_.ResponseAddressSpace();

  url_request_->net_log().AddEvent(
      net::NetLogEventType::PRIVATE_NETWORK_ACCESS_CHECK, [&] {
        return base::Value::Dict()
            .Set("client_address_space",
                 IPAddressSpaceToStringPiece(
                     private_network_access_checker_.ClientAddressSpace()))
            .Set("resource_address_space",
                 IPAddressSpaceToStringPiece(response_address_space))
            .Set("result",
                 PrivateNetworkAccessCheckResultToStringPiece(result));
      });

  bool is_warning = false;
  switch (result) {
    case PrivateNetworkAccessCheckResult::kAllowedByPolicyWarn:
      is_warning = true;
      break;
    case PrivateNetworkAccessCheckResult::kBlockedByPolicyBlock:
      is_warning = false;
      break;
    default:
      // Do not report anything to DevTools in these cases.
      return result;
  }

  // If `security_state` was nullptr, then `result` should not have mentioned
  // the policy set in `security_state->private_network_request_policy`.
  const mojom::ClientSecurityState* security_state =
      private_network_access_checker_.client_security_state();
  DCHECK(security_state);

  if (devtools_observer_) {
    devtools_observer_->OnPrivateNetworkRequest(
        devtools_request_id(), url_request_->url(), is_warning,
        response_address_space, security_state->Clone());
  }

  return result;
}

int URLLoader::OnConnected(net::URLRequest* url_request,
                           const net::TransportInfo& info,
                           net::CompletionOnceCallback callback) {
  DCHECK_EQ(url_request, url_request_.get());

  // Now that the request endpoint's address has been resolved, check if
  // this request should be blocked per Private Network Access.
  PrivateNetworkAccessCheckResult result = PrivateNetworkAccessCheck(info);
  std::optional<mojom::CorsError> cors_error =
      PrivateNetworkAccessCheckResultToCorsError(result);
  if (cors_error.has_value()) {
    if (result == PrivateNetworkAccessCheckResult::kBlockedByPolicyBlock &&
        (info.type == net::TransportType::kCached ||
         info.type == net::TransportType::kCachedFromProxy)) {
      // If the cached entry was blocked by the private network access check
      // without a preflight, we'll start over and attempt to request from the
      // network, so resetting the checker.
      private_network_access_checker_.ResetForRetry();
      return net::
          ERR_CACHED_IP_ADDRESS_SPACE_BLOCKED_BY_PRIVATE_NETWORK_ACCESS_POLICY;
    }
    // Remember the CORS error so we can annotate the URLLoaderCompletionStatus
    // with it later, then fail the request with the same net error code as
    // other CORS errors.
    cors_error_status_ = CorsErrorStatus(
        *cors_error, private_network_access_checker_.TargetAddressSpace(),
        *private_network_access_checker_.ResponseAddressSpace());
    if (result == PrivateNetworkAccessCheckResult::
                      kBlockedByInconsistentIpAddressSpace ||
        result ==
            PrivateNetworkAccessCheckResult::kBlockedByTargetIpAddressSpace) {
      return net::ERR_INCONSISTENT_IP_ADDRESS_SPACE;
    }
    return net::ERR_BLOCKED_BY_PRIVATE_NETWORK_ACCESS_CHECKS;
  }

  if (!accept_ch_frame_observer_ || info.accept_ch_frame.empty() ||
      !base::FeatureList::IsEnabled(features::kAcceptCHFrame)) {
    return net::OK;
  }

  // Find client hints that are in the ACCEPT_CH frame that were not already
  // included in the request
  std::vector<mojom::WebClientHintsType> hints = ComputeAcceptCHFrameHints(
      info.accept_ch_frame, url_request->extra_request_headers());

  // If there are hints in the ACCEPT_CH frame that weren't included in the
  // original request, notify the observer. If those hints can be included,
  // this URLLoader will be destroyed and another with the correct hints
  // started. Otherwise, the callback to continue the network transaction will
  // be called and the URLLoader will continue as normal.
  if (!hints.empty()) {
    accept_ch_frame_observer_->OnAcceptCHFrameReceived(
        url::Origin::Create(url_request->url()), hints, std::move(callback));
    return net::ERR_IO_PENDING;
  }

  return net::OK;
}

mojom::URLResponseHeadPtr URLLoader::BuildResponseHead() const {
  auto response = mojom::URLResponseHead::New();

  response->request_time = url_request_->request_time();
  response->response_time = url_request_->response_time();
  response->headers = url_request_->response_headers();
  response->parsed_headers =
      PopulateParsedHeaders(response->headers.get(), url_request_->url());

  url_request_->GetCharset(&response->charset);
  response->content_length = url_request_->GetExpectedContentSize();
  url_request_->GetMimeType(&response->mime_type);
  net::HttpResponseInfo response_info = url_request_->response_info();
  response->was_fetched_via_spdy = response_info.was_fetched_via_spdy;
  response->was_alpn_negotiated = response_info.was_alpn_negotiated;
  response->alpn_negotiated_protocol = response_info.alpn_negotiated_protocol;
  response->alternate_protocol_usage = response_info.alternate_protocol_usage;
  response->connection_info = response_info.connection_info;
  response->remote_endpoint = response_info.remote_endpoint;
  response->was_fetched_via_cache = url_request_->was_cached();
  response->is_validated = (response_info.cache_entry_status ==
                            net::HttpResponseInfo::ENTRY_VALIDATED);
  response->proxy_chain = url_request_->proxy_chain();
  response->network_accessed = response_info.network_accessed;
  response->async_revalidation_requested =
      response_info.async_revalidation_requested;
  response->was_in_prefetch_cache =
      !(url_request_->load_flags() & net::LOAD_PREFETCH) &&
      response_info.unused_since_prefetch;
  response->did_use_shared_dictionary = response_info.did_use_shared_dictionary;

  response->was_cookie_in_request = false;
  for (const auto& cookie_with_access_result :
       url_request_->maybe_sent_cookies()) {
    if (cookie_with_access_result.access_result.status.IsInclude()) {
      // IsInclude() true means the cookie was sent.
      response->was_cookie_in_request = true;
      break;
    }
  }

  if (is_load_timing_enabled_)
    url_request_->GetLoadTimingInfo(&response->load_timing);

  if (url_request_->ssl_info().cert.get()) {
    response->cert_status = url_request_->ssl_info().cert_status;
    if ((options_ & mojom::kURLLoadOptionSendSSLInfoWithResponse) ||
        (net::IsCertStatusError(url_request_->ssl_info().cert_status) &&
         (options_ & mojom::kURLLoadOptionSendSSLInfoForCertificateError))) {
      response->ssl_info = url_request_->ssl_info();
    }
  }

  if (request_cookies_.size()) {
    CHECK(include_request_cookies_with_response_);
    response->request_cookies = request_cookies_;
  }

  response->request_start = url_request_->creation_time();
  response->response_start = base::TimeTicks::Now();
  response->encoded_data_length = url_request_->GetTotalReceivedBytes();
  response->auth_challenge_info = url_request_->auth_challenge_info();
  response->has_range_requested =
      url_request_->extra_request_headers().HasHeader(
          net::HttpRequestHeaders::kRange);
  base::ranges::copy(url_request_->response_info().dns_aliases,
                     std::back_inserter(response->dns_aliases));
  // [spec]: https://fetch.spec.whatwg.org/#http-network-or-cache-fetch
  // 13. Set response’s request-includes-credentials to includeCredentials.
  response->request_include_credentials = url_request_->allow_credentials();

  response->response_address_space =
      private_network_access_checker_.ResponseAddressSpace().value_or(
          mojom::IPAddressSpace::kUnknown);
  response->client_address_space =
      private_network_access_checker_.ClientAddressSpace();

  response->load_with_storage_access = ShouldSetLoadWithStorageAccess();

  return response;
}

void URLLoader::OnReceivedRedirect(net::URLRequest* url_request,
                                   const net::RedirectInfo& redirect_info,
                                   bool* defer_redirect) {
  DCHECK(url_request == url_request_.get());

  DCHECK(!deferred_redirect_url_);
  deferred_redirect_url_ = std::make_unique<GURL>(redirect_info.new_url);

  // Send the redirect response to the client, allowing them to inspect it and
  // optionally follow the redirect.
  *defer_redirect = true;

  mojom::URLResponseHeadPtr response = BuildResponseHead();
  DispatchOnRawResponse();
  ReportFlaggedResponseCookies(false);

  // Enforce the Cross-Origin-Resource-Policy (CORP) header.
  const CrossOriginEmbedderPolicy kEmptyCoep;
  const CrossOriginEmbedderPolicy& cross_origin_embedder_policy =
      factory_params_->client_security_state
          ? factory_params_->client_security_state->cross_origin_embedder_policy
          : kEmptyCoep;
  const DocumentIsolationPolicy kEmptyDip;
  const DocumentIsolationPolicy& document_isolation_policy =
      factory_params_->client_security_state
          ? factory_params_->client_security_state->document_isolation_policy
          : kEmptyDip;
  if (std::optional<mojom::BlockedByResponseReason> blocked_reason =
          CrossOriginResourcePolicy::IsBlocked(
              url_request_->url(), url_request_->original_url(),
              url_request_->initiator(), *response, request_mode_,
              request_destination_, cross_origin_embedder_policy,
              coep_reporter_, document_isolation_policy)) {
    CompleteBlockedResponse(net::ERR_BLOCKED_BY_RESPONSE, false,
                            blocked_reason);
    // TODO(crbug.com/40054032):  Close the socket here.
    // For more details see https://crbug.com/1154250#c17.
    // Item 2 discusses redirect handling.
    //
    // "url_request_->AbortAndCloseConnection()" should ideally close the
    // socket, but unfortunately, URLRequestHttpJob caches redirects in a way
    // that ignores their response bodies, since they'll never be read. It does
    // this by calling HttpCache::Transaction::StopCaching(), which also has the
    // effect of detaching the HttpNetworkTransaction, which owns the socket,
    // from the HttpCache::Transaction. To fix this, we'd either need to call
    // StopCaching() later in the process, or make the HttpCache::Transaction
    // continue to hang onto the HttpNetworkTransaction after this call.
    DeleteSelf();
    return;
  }

  SetRequestCredentials(redirect_info.new_url);

  // Clear the Cookie header to ensure that cookies passed in through the
  // `ResourceRequest` do not persist across redirects.
  url_request_.get()->RemoveRequestHeaderByName(
      net::HttpRequestHeaders::kCookie);
  cookies_from_browser_.clear();
  request_cookies_.clear();

  net::cookie_util::AddOrRemoveStorageAccessApiOverride(
      redirect_info.new_url, storage_access_api_status_,
      url_request_->initiator(), url_request_->cookie_setting_overrides());

  // Note: There are some ordering dependencies here.
  // `CalculateStorageAccessStatus` depends on
  // `url_request->cookie_setting_overrides()`.  `SetFetchMetadataHeaders` will
  // depend on `url_request_->storage_access_status()`, once
  // https://crbug.com/366284840 is fixed.
  url_request_->set_storage_access_status(
      url_request_->CalculateStorageAccessStatus());

  // We may need to clear out old Sec- prefixed request headers. We'll attempt
  // to do this before we re-add any.
  MaybeRemoveSecHeaders(url_request_.get(), redirect_info.new_url);
  SetFetchMetadataHeaders(url_request_.get(), request_mode_,
                          has_user_activation_, request_destination_,
                          &redirect_info.new_url, *factory_params_,
                          *origin_access_list_);

  DCHECK_EQ(emitted_devtools_raw_request_, emitted_devtools_raw_response_);
  response->emitted_extra_info = emitted_devtools_raw_request_;

  ProcessInboundAttributionInterceptorOnReceivedRedirect(redirect_info,
                                                         std::move(response));
}

void URLLoader::ProcessInboundSharedStorageInterceptorOnReceivedRedirect(
    const ::net::RedirectInfo& redirect_info,
    mojom::URLResponseHeadPtr response) {
  DCHECK(shared_storage_request_helper_);
  uint64_t response_index = next_on_receive_redirect_response_index_++;
  on_receive_redirect_responses_[response_index] = std::move(response);
  if (!shared_storage_request_helper_->ProcessIncomingResponse(
          *url_request_, base::BindOnce(&URLLoader::ContinueOnReceiveRedirect,
                                        weak_ptr_factory_.GetWeakPtr(),
                                        redirect_info, response_index))) {
    ContinueOnReceiveRedirect(redirect_info, response_index);
  }
}

void URLLoader::ProcessInboundAttributionInterceptorOnReceivedRedirect(
    const net::RedirectInfo& redirect_info,
    mojom::URLResponseHeadPtr response) {
  if (!attribution_request_helper_) {
    ProcessInboundSharedStorageInterceptorOnReceivedRedirect(
        redirect_info, std::move(response));
    return;
  }

  attribution_request_helper_->OnReceiveRedirect(
      *url_request_, std::move(response), redirect_info,
      base::BindOnce(
          &URLLoader::ProcessInboundSharedStorageInterceptorOnReceivedRedirect,
          weak_ptr_factory_.GetWeakPtr(), redirect_info));
}

void URLLoader::ContinueOnReceiveRedirect(
    const net::RedirectInfo& redirect_info,
    uint64_t response_index) {
  auto iter = on_receive_redirect_responses_.find(response_index);
  CHECK(iter != on_receive_redirect_responses_.end(),
        base::NotFatalUntil::M130);
  mojom::URLResponseHeadPtr response = std::move(iter->second);
  DCHECK(response);
  on_receive_redirect_responses_.erase(iter);
  url_loader_client_.Get()->OnReceiveRedirect(redirect_info,
                                              std::move(response));
}

// static
bool URLLoader::HasFetchStreamingUploadBody(const ResourceRequest* request) {
  const ResourceRequestBody* request_body = request->request_body.get();
  if (!request_body)
    return false;
  const std::vector<DataElement>* elements = request_body->elements();
  if (elements->size() != 1u)
    return false;
  const auto& element = elements->front();
  return element.type() == mojom::DataElementDataView::Tag::kChunkedDataPipe &&
         element.As<network::DataElementChunkedDataPipe>().read_only_once();
}

// static
std::optional<net::IsolationInfo> URLLoader::GetIsolationInfo(
    const net::IsolationInfo& factory_isolation_info,
    bool automatically_assign_isolation_info,
    const ResourceRequest& request) {
  if (!factory_isolation_info.IsEmpty())
    return factory_isolation_info;

  if (request.trusted_params &&
      !request.trusted_params->isolation_info.IsEmpty()) {
    if (request.credentials_mode != network::mojom::CredentialsMode::kOmit) {
      DCHECK(request.trusted_params->isolation_info.site_for_cookies()
                 .IsEquivalent(request.site_for_cookies));
    }
    return request.trusted_params->isolation_info;
  }

  if (automatically_assign_isolation_info) {
    url::Origin origin = url::Origin::Create(request.url);
    return net::IsolationInfo::Create(net::IsolationInfo::RequestType::kOther,
                                      origin, origin, net::SiteForCookies());
  }

  return std::nullopt;
}

// static
net::CookieSettingOverrides URLLoader::CalculateCookieSettingOverrides(
    net::CookieSettingOverrides factory_overrides,
    const ResourceRequest& request) {
  net::CookieSettingOverrides overrides(factory_overrides);
  if (request.is_outermost_main_frame &&
      network::cors::IsCorsEnabledRequestMode(request.mode)) {
    overrides.Put(
        net::CookieSettingOverride::kTopLevelStorageAccessGrantEligible);
  }

  AddAdsHeuristicCookieSettingOverrides(request.is_ad_tagged, overrides);

  // The `kStorageAccessGrantEligible` override should not be present in
  // factory_overrides.
  CHECK(
      !overrides.Has(net::CookieSettingOverride::kStorageAccessGrantEligible));
  // Add/remove the Storage Access override enum based on whether the request's
  // url and initiator are same-site, to prevent cross-site sibling iframes
  // benefit from each other's storage access API grants. This must be updated
  // on redirects.
  net::cookie_util::AddOrRemoveStorageAccessApiOverride(
      request.url, request.storage_access_api_status, request.request_initiator,
      overrides);

  // The `kStorageAccessGrantEligibleViaHeader` override will be applied
  // (in-place) by individual request jobs as appropriate, but should not be
  // present initially.
  CHECK(!overrides.Has(
      net::CookieSettingOverride::kStorageAccessGrantEligibleViaHeader));

  return overrides;
}

void URLLoader::OnAuthRequired(net::URLRequest* url_request,
                               const net::AuthChallengeInfo& auth_info) {
  if (has_fetch_streaming_upload_body_) {
    NotifyCompleted(net::ERR_FAILED);
    // |this| may have been deleted.
    return;
  }
  if (!url_loader_network_observer_) {
    OnAuthCredentials(std::nullopt);
    return;
  }

  if (do_not_prompt_for_login_) {
    OnAuthCredentials(std::nullopt);
    return;
  }

  DCHECK(!auth_challenge_responder_receiver_.is_bound());

  url_loader_network_observer_->OnAuthRequired(
      fetch_window_id_, request_id_, url_request_->url(), first_auth_attempt_,
      auth_info, url_request->response_headers(),
      auth_challenge_responder_receiver_.BindNewPipeAndPassRemote());

  auth_challenge_responder_receiver_.set_disconnect_handler(
      base::BindOnce(&URLLoader::DeleteSelf, base::Unretained(this)));

  first_auth_attempt_ = false;
}

void URLLoader::OnCertificateRequested(net::URLRequest* unused,
                                       net::SSLCertRequestInfo* cert_info) {
  DCHECK(!client_cert_responder_receiver_.is_bound());

  if (!url_loader_network_observer_) {
    CancelRequest();
    return;
  }

  // Set up mojo endpoints for ClientCertificateResponder and bind to the
  // Receiver. This enables us to receive messages regarding the client
  // certificate selection.
  url_loader_network_observer_->OnCertificateRequested(
      fetch_window_id_, cert_info,
      client_cert_responder_receiver_.BindNewPipeAndPassRemote());
  client_cert_responder_receiver_.set_disconnect_handler(
      base::BindOnce(&URLLoader::CancelRequest, base::Unretained(this)));
}

void URLLoader::OnSSLCertificateError(net::URLRequest* request,
                                      int net_error,
                                      const net::SSLInfo& ssl_info,
                                      bool fatal) {
  if (!url_loader_network_observer_) {
    OnSSLCertificateErrorResponse(ssl_info, net_error);
    return;
  }
  url_loader_network_observer_->OnSSLCertificateError(
      url_request_->url(), net_error, ssl_info, fatal,
      base::BindOnce(&URLLoader::OnSSLCertificateErrorResponse,
                     weak_ptr_factory_.GetWeakPtr(), ssl_info));
}

void URLLoader::ProcessInboundSharedStorageInterceptorOnResponseStarted() {
  DCHECK(shared_storage_request_helper_);
  if (!shared_storage_request_helper_->ProcessIncomingResponse(
          *url_request_, base::BindOnce(&URLLoader::ContinueOnResponseStarted,
                                        weak_ptr_factory_.GetWeakPtr()))) {
    ContinueOnResponseStarted();
  }
}

void URLLoader::ProcessInboundAttributionInterceptorOnResponseStarted() {
  if (!attribution_request_helper_) {
    ProcessInboundSharedStorageInterceptorOnResponseStarted();
    return;
  }

  attribution_request_helper_->Finalize(
      *response_,
      base::BindOnce(
          &URLLoader::ProcessInboundSharedStorageInterceptorOnResponseStarted,
          weak_ptr_factory_.GetWeakPtr()));
}

void URLLoader::OnResponseStarted(net::URLRequest* url_request, int net_error) {
  DCHECK(url_request == url_request_.get());
  has_received_response_ = true;

  if (keepalive_) {
    base::UmaHistogramEnumeration(
        "FetchKeepAlive.Requests2.Network",
        internal::FetchKeepAliveRequestNetworkMetricType::kOnResponse);
  }

  // Use `true` to force sending the cookie accessed update now. This is because
  // for navigations the CookieObserver might get torn down by the time the
  // request completes.
  ReportFlaggedResponseCookies(true);

  if (net_error != net::OK) {
    NotifyCompleted(net_error);
    // |this| may have been deleted.
    return;
  }

  response_ = BuildResponseHead();
  DispatchOnRawResponse();

  // Parse and remove the Trust Tokens response headers, if any are expected,
  // potentially failing the request if an error occurs.
  if (response_ && response_->headers && trust_token_helper_) {
    DCHECK(response_);
    trust_token_helper_->Finalize(
        *response_->headers.get(),
        base::BindOnce(&URLLoader::OnDoneFinalizingTrustTokenOperation,
                       weak_ptr_factory_.GetWeakPtr()));
    // |this| may have been deleted.
    return;
  }

  ProcessInboundAttributionInterceptorOnResponseStarted();
}

void URLLoader::OnDoneFinalizingTrustTokenOperation(
    mojom::TrustTokenOperationStatus status) {
  trust_token_status_ = status;

  MaybeSendTrustTokenOperationResultToDevTools();

  if (status != mojom::TrustTokenOperationStatus::kOk) {
    NotifyCompleted(net::ERR_TRUST_TOKEN_OPERATION_FAILED);
    // |this| may have been deleted.
    return;
  }

  ProcessInboundAttributionInterceptorOnResponseStarted();
}

void URLLoader::MaybeSendTrustTokenOperationResultToDevTools() {
  CHECK(trust_token_helper_ && trust_token_status_);

  if (!devtools_observer_ || !devtools_request_id())
    return;

  mojom::TrustTokenOperationResultPtr operation_result =
      trust_token_helper_->CollectOperationResultWithStatus(
          *trust_token_status_);
  devtools_observer_->OnTrustTokenOperationDone(devtools_request_id().value(),
                                                std::move(operation_result));
}

void URLLoader::ContinueOnResponseStarted() {
  // Do not account header bytes when reporting received body bytes to client.
  reported_total_encoded_bytes_ = url_request_->GetTotalReceivedBytes();

  if (upload_progress_tracker_) {
    upload_progress_tracker_->OnUploadCompleted();
    upload_progress_tracker_ = nullptr;
  }

  if (!(options_ & mojom::kURLLoadOptionReadAndDiscardBody)) {
    MojoCreateDataPipeOptions options;
    options.struct_size = sizeof(MojoCreateDataPipeOptions);
    options.flags = MOJO_CREATE_DATA_PIPE_FLAG_NONE;
    options.element_num_bytes = 1;
    options.capacity_num_bytes =
        network::features::GetDataPipeDefaultAllocationSize(
            features::DataPipeAllocationSize::kLargerSizeIfPossible);
    MojoResult result =
        mojo::CreateDataPipe(&options, response_body_stream_, consumer_handle_);
    if (result != MOJO_RESULT_OK) {
      NotifyCompleted(net::ERR_INSUFFICIENT_RESOURCES);
      return;
    }
    CHECK(response_body_stream_.is_valid());
    CHECK(consumer_handle_.is_valid());
    peer_closed_handle_watcher_.Watch(
        response_body_stream_.get(), MOJO_HANDLE_SIGNAL_PEER_CLOSED,
        base::BindRepeating(&URLLoader::OnResponseBodyStreamConsumerClosed,
                            base::Unretained(this)));
    peer_closed_handle_watcher_.ArmOrNotify();

    writable_handle_watcher_.Watch(
        response_body_stream_.get(), MOJO_HANDLE_SIGNAL_WRITABLE,
        base::BindRepeating(&URLLoader::OnResponseBodyStreamReady,
                            base::Unretained(this)));
  }

  // Enforce the Cross-Origin-Resource-Policy (CORP) header.
  const CrossOriginEmbedderPolicy kEmptyCoep;
  const CrossOriginEmbedderPolicy& cross_origin_embedder_policy =
      factory_params_->client_security_state
          ? factory_params_->client_security_state->cross_origin_embedder_policy
          : kEmptyCoep;
  const DocumentIsolationPolicy kEmptyDip;
  const DocumentIsolationPolicy& document_isolation_policy =
      factory_params_->client_security_state
          ? factory_params_->client_security_state->document_isolation_policy
          : kEmptyDip;
  if (std::optional<mojom::BlockedByResponseReason> blocked_reason =
          CrossOriginResourcePolicy::IsBlocked(
              url_request_->url(), url_request_->original_url(),
              url_request_->initiator(), *response_, request_mode_,
              request_destination_, cross_origin_embedder_policy,
              coep_reporter_, document_isolation_policy)) {
    CompleteBlockedResponse(net::ERR_BLOCKED_BY_RESPONSE, false,
                            blocked_reason);
    // Close the socket associated with the request, to prevent leaking
    // information.
    url_request_->AbortAndCloseConnection();
    DeleteSelf();
    return;
  }

  // Enforce ad-auction-only signals -- the renderer process isn't allowed
  // to read auction-only signals for ad auctions; only the browser process
  // is allowed to read those, and only the browser process can issue trusted
  // requests.
  std::string auction_only;
  // TODO(crbug.com/40269364): Remove old names once API users have migrated to
  // new names.
  if (!factory_params_->is_trusted && response_->headers &&
      (response_->headers->GetNormalizedHeader("Ad-Auction-Only",
                                               &auction_only) ||
       response_->headers->GetNormalizedHeader("X-FLEDGE-Auction-Only",
                                               &auction_only)) &&
      base::EqualsCaseInsensitiveASCII(auction_only, "true")) {
    CompleteBlockedResponse(net::ERR_BLOCKED_BY_RESPONSE, false);
    url_request_->AbortAndCloseConnection();
    DeleteSelf();
    return;
  }

  // Figure out if we need to sniff (for MIME type detection or for Opaque
  // Response Blocking / ORB).
  if (factory_params_->is_orb_enabled) {
    // TODO(ricea): Make ORB and ReadAndDiscardBody work together if necessary.
    CHECK(!(options_ & mojom::kURLLoadOptionReadAndDiscardBody))
        << "ORB is incompatible with the ReadAndDiscardBody option";
    orb_analyzer_ = orb::ResponseAnalyzer::Create(&*per_factory_orb_state_);
    is_more_orb_sniffing_needed_ = true;
    auto decision =
        orb_analyzer_->Init(url_request_->url(), url_request_->initiator(),
                            request_mode_, request_destination_, *response_);
    if (MaybeBlockResponseForOrb(decision)) {
      return;
    }
  }

  if ((options_ & mojom::kURLLoadOptionSniffMimeType)) {
    if (ShouldSniffContent(url_request_->url(), *response_)) {
      // We're going to look at the data before deciding what the content type
      // is.  That means we need to delay sending the response started IPC.
      VLOG(1) << "Will sniff content for mime type: " << url_request_->url();
      is_more_mime_sniffing_needed_ = true;
    } else if (response_->mime_type.empty()) {
      // Ugg.  The server told us not to sniff the content but didn't give us
      // a mime type.  What's a browser to do?  Turns out, we're supposed to
      // treat the response as "text/plain".  This is the most secure option.
      response_->mime_type.assign("text/plain");
    }
  }

  StartReading();
}

void URLLoader::ReadMore() {
  DCHECK(!read_in_progress_);
  // Once the MIME type is sniffed, all data is sent as soon as it is read from
  // the network.
  DCHECK(consumer_handle_.is_valid() || !pending_write_);

  if (should_pause_reading_body_) {
    paused_reading_body_ = true;
    return;
  }

  // TODO(ricea): Refactor this method and DidRead() to reduce duplication.
  if (options_ & mojom::kURLLoadOptionReadAndDiscardBody) {
    read_in_progress_ = true;
    int bytes_read =
        url_request_->Read(discard_buffer_.get(), discard_buffer_->size());
    if (bytes_read != net::ERR_IO_PENDING) {
      DidRead(bytes_read, /*completed_synchronously=*/true,
              /*into_slop_bucket=*/false);
      // `this` may have been deleted.
    }
    return;
  }

  if (!pending_write_.get()) {
    // TODO: we should use the abstractions in MojoAsyncResourceHandler.
    DCHECK_EQ(0u, pending_write_buffer_offset_);
    MojoResult result = NetToMojoPendingBuffer::BeginWrite(
        &response_body_stream_, &pending_write_);
    switch (result) {
      case MOJO_RESULT_OK:
        break;
      case MOJO_RESULT_SHOULD_WAIT:
        CHECK(!pending_write_);
        if (base::FeatureList::IsEnabled(kSlopBucket) && !slop_bucket_) {
          slop_bucket_ = SlopBucket::RequestSlopBucket(url_request_.get());
        }
        if (slop_bucket_ && !slop_bucket_->read_in_progress() &&
            !slop_bucket_->IsComplete()) {
          // Read into the slop bucket while we're waiting for the mojo data
          // pipe to empty out.
          std::optional<int> bytes_read_maybe = slop_bucket_->AttemptRead();
          if (bytes_read_maybe.has_value()) {
            int bytes_read = bytes_read_maybe.value();
            if (bytes_read != net::ERR_IO_PENDING) {
              // DidRead() will not delete `this` when `into_slop_bucket` is
              // true, so it is safe to access member variables after this call.
              DidRead(bytes_read, /*completed_synchronously=*/true,
                      /*into_slop_bucket=*/true);
            }
          }
        }  // The pipe is full. We need to wait for it to have more space.
        writable_handle_watcher_.ArmOrNotify();
        return;
      default:
        // The response body stream is in a bad state. Bail.
        NotifyCompleted(net::ERR_FAILED);
        return;
    }
    pending_write_buffer_size_ = pending_write_->size();
    DCHECK_GT(static_cast<uint32_t>(std::numeric_limits<int>::max()),
              pending_write_buffer_size_);
    if (consumer_handle_.is_valid()) {
      DCHECK_GE(pending_write_buffer_size_,
                static_cast<uint32_t>(net::kMaxBytesToSniff));
    }

    // We may be able to fill up the buffer from the slop bucket.
    if (slop_bucket_) {
      const size_t consumed = slop_bucket_->Consume(pending_write_->buffer(),
                                                    pending_write_buffer_size_);
      if (consumed) {
        // TODO(ricea): Refactor the way pending writes work so we don't need to
        // poke a value into `pending_write_buffer_offset_` here.
        pending_write_buffer_offset_ = consumed;
        CompletePendingWrite(true);
        base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE, base::BindOnce(&URLLoader::ReadMore,
                                      weak_ptr_factory_.GetWeakPtr()));
        return;
      } else if (slop_bucket_->read_in_progress()) {
        // There were no bytes available, but a read is in progress. Need to
        // prevent starting another read until it completes.
        CompletePendingWrite(true);
        return;
      } else if (slop_bucket_->IsComplete()) {
        // The SlopBucket didn't have any bytes available. It has finished
        // reading from the URLRequest and now the render process has caught up,
        // so we can notify completion.
        CompletePendingWrite(true);
        NotifyCompleted(slop_bucket_->completion_code());
        // `this` is deleted.
        return;
      }
      // Nothing was available. Read from `url_request_` as usual.
    }
  }

  CHECK(!slop_bucket_ || !slop_bucket_->IsComplete());
  auto buf = base::MakeRefCounted<NetToMojoIOBuffer>(
      pending_write_, pending_write_buffer_offset_);
  read_in_progress_ = true;
  int bytes_read = url_request_->Read(
      buf.get(), static_cast<int>(pending_write_buffer_size_ -
                                  pending_write_buffer_offset_));
  if (bytes_read != net::ERR_IO_PENDING) {
    DidRead(bytes_read, /*completed_synchronously=*/true,
            /*into_slop_bucket=*/false);
    // |this| may have been deleted.
  }
}

// Handles the completion of a read. `num_bytes` is the number of bytes read, 0
// if we reached the end of the response body, or a net::Error otherwise.
// `completed_synchronously` is true if the call to URLRequest::Read did not
// return net::ERR_IO_PENDING. `into_slop_bucket` is true if this was actually a
// read into `slop_bucket_` and not into our own buffer.
void URLLoader::DidRead(int num_bytes,
                        bool completed_synchronously,
                        bool into_slop_bucket) {
  DCHECK(read_in_progress_ || into_slop_bucket);
  read_in_progress_ = false;

  size_t new_data_offset = pending_write_buffer_offset_;
  if (num_bytes > 0) {
    if (!into_slop_bucket) {
      pending_write_buffer_offset_ += num_bytes;
    }

    // Only notify client of download progress if we're done sniffing and
    // started sending response.
    if (!consumer_handle_.is_valid()) {
      int64_t total_encoded_bytes = url_request_->GetTotalReceivedBytes();
      if (ShouldSendTransferSizeUpdated()) {
        int64_t delta = total_encoded_bytes - reported_total_encoded_bytes_;
        DCHECK_LE(0, delta);
        if (delta) {
          url_loader_client_.Get()->OnTransferSizeUpdated(delta);
        }
      }
      reported_total_encoded_bytes_ = total_encoded_bytes;
    }
  }

  bool complete_read = true;
  if (consumer_handle_.is_valid()) {
    // `consumer_handle_` is only valid when we are sniffing. Sniffing is only
    // applied to the first 1024 bytes. The mojo data pipe is always larger than
    // 1024 bytes, therefore it will never fill up while we are sniffing.
    // Therefore a SlopBucket will not have been created yet and
    // `into_slop_bucket` can't be true.
    CHECK(!into_slop_bucket);

    // |pending_write_| may be null if the job self-aborts due to a suspend;
    // this will have |consumer_handle_| valid when the loader is paused.
    if (pending_write_) {
      // Limit sniffing to the first net::kMaxBytesToSniff.
      size_t data_length = pending_write_buffer_offset_;
      if (data_length > net::kMaxBytesToSniff)
        data_length = net::kMaxBytesToSniff;

      std::string_view data(pending_write_->buffer(), data_length);
      bool stop_sniffing_after_processing_current_data =
          (num_bytes <= 0 ||
           pending_write_buffer_offset_ >= net::kMaxBytesToSniff);

      if (is_more_mime_sniffing_needed_) {
        const std::string& type_hint = response_->mime_type;
        std::string new_type;
        is_more_mime_sniffing_needed_ = !net::SniffMimeType(
            data, url_request_->url(), type_hint,
            net::ForceSniffFileUrlsForHtml::kDisabled, &new_type);
        // SniffMimeType() returns false if there is not enough data to
        // determine the mime type. However, even if it returns false, it
        // returns a new type that is probably better than the current one.
        response_->mime_type.assign(new_type);
        response_->did_mime_sniff = true;

        if (stop_sniffing_after_processing_current_data)
          is_more_mime_sniffing_needed_ = false;
      }

      if (is_more_orb_sniffing_needed_) {
        orb::ResponseAnalyzer::Decision orb_decision =
            orb::ResponseAnalyzer::Decision::kSniffMore;

        // `has_new_data_to_sniff` can be false at the end-of-stream.
        bool has_new_data_to_sniff = new_data_offset < data.length();
        if (has_new_data_to_sniff)
          orb_decision = orb_analyzer_->Sniff(data);

        if (orb_decision == orb::ResponseAnalyzer::Decision::kSniffMore &&
            stop_sniffing_after_processing_current_data) {
          orb_decision = orb_analyzer_->HandleEndOfSniffableResponseBody();
          DCHECK_NE(orb::ResponseAnalyzer::Decision::kSniffMore, orb_decision);
        }

        if (MaybeBlockResponseForOrb(orb_decision)) {
          return;
        }
      }
    }

    if (!is_more_mime_sniffing_needed_ && !is_more_orb_sniffing_needed_) {
      SendResponseToClient();
    } else {
      complete_read = false;
    }
  }

  if (num_bytes <= 0) {
    // There may be no |pending_write_| if a URLRequestJob cancelled itself in
    // URLRequestJob::OnSuspend() after receiving headers, while there was no
    // pending read.
    // TODO(mmenke): That case is rather unfortunate - something should be done
    // at the socket layer instead, both to make for a better API (Only complete
    // reads when there's a pending read), and to cover all TCP socket uses,
    // since the concern is the effect that entering suspend mode has on
    // sockets. See https://crbug.com/651120.
    if (pending_write_) {
      CHECK(!into_slop_bucket);
      CompletePendingWrite(num_bytes == 0);
    }
    // If we are reading into the SlopBucket then notification of completion
    // will be postponed until the data has been forwarded to the mojo data
    // pipe.
    if (!into_slop_bucket) {
      NotifyCompleted(num_bytes);
      // |this| will have been deleted.
    }
    return;
  }

  if (complete_read && !into_slop_bucket) {
    CompletePendingWrite(true /* success */);
  }
  if (completed_synchronously) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&URLLoader::ReadMore, weak_ptr_factory_.GetWeakPtr()));
  } else {
    ReadMore();
  }
}

void URLLoader::OnReadCompleted(net::URLRequest* url_request, int bytes_read) {
  DCHECK(url_request == url_request_.get());

  bool into_slop_bucket = false;
  if (slop_bucket_ && slop_bucket_->read_in_progress()) {
    slop_bucket_->OnReadCompleted(bytes_read);
    into_slop_bucket = true;
  }

  DidRead(bytes_read, /*completed_synchronously=*/false, into_slop_bucket);
  // |this| may have been deleted.
}

int URLLoader::OnBeforeStartTransaction(
    const net::HttpRequestHeaders& headers,
    net::NetworkDelegate::OnBeforeStartTransactionCallback callback) {
  const net::HttpRequestHeaders* used_headers = &headers;
  net::HttpRequestHeaders headers_with_bonus_cookies;
  if (!cookies_from_browser_.empty()) {
    headers_with_bonus_cookies = AttachCookies(headers, cookies_from_browser_);
    used_headers = &headers_with_bonus_cookies;
  }

  if (include_request_cookies_with_response_) {
    request_cookies_.clear();
    std::string cookie_header =
        used_headers->GetHeader(net::HttpRequestHeaders::kCookie)
            .value_or(std::string());
    net::cookie_util::ParseRequestCookieLine(cookie_header, &request_cookies_);
  }

  if (header_client_) {
    header_client_->OnBeforeSendHeaders(
        *used_headers,
        base::BindOnce(&URLLoader::OnBeforeSendHeadersComplete,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
    return net::ERR_IO_PENDING;
  }

  // Additional cookies were added to the existing headers, so `callback` must
  // be invoked to ensure that the cookies are included in the request.
  if (!cookies_from_browser_.empty()) {
    CHECK_EQ(used_headers, &headers_with_bonus_cookies);
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), net::OK,
                                  std::move(headers_with_bonus_cookies)));
    return net::ERR_IO_PENDING;
  }

  return net::OK;
}

int URLLoader::OnHeadersReceived(
    net::CompletionOnceCallback callback,
    const net::HttpResponseHeaders* original_response_headers,
    scoped_refptr<net::HttpResponseHeaders>* override_response_headers,
    const net::IPEndPoint& endpoint,
    std::optional<GURL>* preserve_fragment_on_redirect_url) {
  if (header_client_) {
    header_client_->OnHeadersReceived(
        original_response_headers->raw_headers(), endpoint,
        base::BindOnce(&URLLoader::OnHeadersReceivedComplete,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                       override_response_headers,
                       preserve_fragment_on_redirect_url));
    return net::ERR_IO_PENDING;
  }
  return net::OK;
}

URLLoader::PartialLoadInfo URLLoader::GetPartialLoadInfo() const {
  return PartialLoadInfo(url_request_->GetLoadState(),
                         url_request_->GetUploadProgress());
}

mojom::LoadInfoPtr URLLoader::CreateLoadInfo(
    const PartialLoadInfo& partial_load_info) {
  return mojom::LoadInfo::New(
      base::TimeTicks::Now(), url_request_->url().host(),
      partial_load_info.load_state.state, partial_load_info.load_state.param,
      partial_load_info.upload_progress.position(),
      partial_load_info.upload_progress.size());
}

net::LoadState URLLoader::GetLoadState() const {
  return url_request_->GetLoadState().state;
}

net::UploadProgress URLLoader::GetUploadProgress() const {
  return url_request_->GetUploadProgress();
}

int32_t URLLoader::GetProcessId() const {
  return factory_params_->process_id;
}

void URLLoader::SetEnableReportingRawHeaders(bool allow) {
  enable_reporting_raw_headers_ = allow;
}

uint32_t URLLoader::GetResourceType() const {
  return resource_type_;
}

bool URLLoader::CookiesDisabled() const {
  return options_ & mojom::kURLLoadOptionBlockAllCookies;
}

bool URLLoader::AllowCookie(const net::CanonicalCookie& cookie,
                            const GURL& url,
                            const net::SiteForCookies& site_for_cookies) const {
  if (cookie.IsPartitioned() && !CookiesDisabled()) {
    return true;
  }
  return AllowFullCookies(url, site_for_cookies);
}

bool URLLoader::AllowFullCookies(
    const GURL& url,
    const net::SiteForCookies& site_for_cookies) const {
  net::StaticCookiePolicy::Type policy =
      net::StaticCookiePolicy::ALLOW_ALL_COOKIES;
  if (CookiesDisabled()) {
    policy = net::StaticCookiePolicy::BLOCK_ALL_COOKIES;
  } else if (options_ & mojom::kURLLoadOptionBlockThirdPartyCookies) {
    policy = net::StaticCookiePolicy::BLOCK_ALL_THIRD_PARTY_COOKIES;
  } else {
    return true;
  }
  return net::StaticCookiePolicy(policy).CanAccessCookies(
             url, site_for_cookies) == net::OK;
}

// static
URLLoader* URLLoader::ForRequest(const net::URLRequest& request) {
  auto* pointer =
      static_cast<UnownedPointer*>(request.GetUserData(kUserDataKey));
  if (!pointer)
    return nullptr;
  return pointer->get();
}

void URLLoader::OnAuthCredentials(
    const std::optional<net::AuthCredentials>& credentials) {
  auth_challenge_responder_receiver_.reset();

  if (!credentials.has_value()) {
    url_request_->CancelAuth();
  } else {
    // CancelAuth will proceed to the body, so cookies only need to be reported
    // here.
    ReportFlaggedResponseCookies(false);
    url_request_->SetAuth(credentials.value());
  }
}

void URLLoader::ContinueWithCertificate(
    const scoped_refptr<net::X509Certificate>& x509_certificate,
    const std::string& provider_name,
    const std::vector<uint16_t>& algorithm_preferences,
    mojo::PendingRemote<mojom::SSLPrivateKey> ssl_private_key) {
  client_cert_responder_receiver_.reset();
  auto key = base::MakeRefCounted<SSLPrivateKeyInternal>(
      provider_name, algorithm_preferences, std::move(ssl_private_key));
  url_request_->ContinueWithCertificate(std::move(x509_certificate),
                                        std::move(key));
}

void URLLoader::ContinueWithoutCertificate() {
  client_cert_responder_receiver_.reset();
  url_request_->ContinueWithCertificate(nullptr, nullptr);
}

void URLLoader::CancelRequest() {
  client_cert_responder_receiver_.reset();
  url_request_->CancelWithError(net::ERR_SSL_CLIENT_AUTH_CERT_NEEDED);
}

void URLLoader::CancelRequestIfNonceMatchesAndUrlNotExempted(
    const base::UnguessableToken& nonce,
    const std::set<GURL>& exemptions) {
  if (url_request_->isolation_info().nonce() == nonce) {
    if (!exemptions.contains(
            url_request_->original_url().GetWithoutFilename())) {
      url_request_->CancelWithError(net::ERR_NETWORK_ACCESS_REVOKED);
    }
  }
}

void URLLoader::NotifyCompleted(int error_code) {
  // Ensure sending the final upload progress message here, since
  // OnResponseCompleted can be called without OnResponseStarted on cancellation
  // or error cases.
  if (upload_progress_tracker_) {
    upload_progress_tracker_->OnUploadCompleted();
    upload_progress_tracker_ = nullptr;
  }

  auto total_received = url_request_->GetTotalReceivedBytes();
  auto total_sent = url_request_->GetTotalSentBytes();
  if (total_received > 0) {
    base::UmaHistogramCustomCounts("DataUse.BytesReceived3.Delegate",
                                   total_received, 50, 10 * 1000 * 1000, 50);
  }

  if (total_sent > 0) {
    UMA_HISTOGRAM_COUNTS_1M("DataUse.BytesSent3.Delegate", total_sent);
  }

  MaybeRecordSharedDictionaryUsedResponseMetrics(
      error_code, request_destination_, url_request_->response_info(),
      shared_dictionary_allowed_check_passed_);

  if ((total_received > 0 || total_sent > 0)) {
    if (url_loader_network_observer_ && provide_data_use_updates_) {
      url_loader_network_observer_->OnDataUseUpdate(
          url_request_->traffic_annotation().unique_id_hash_code,
          total_received, total_sent);
    }
  }

  if (url_loader_client_.Get()) {
    if (consumer_handle_.is_valid())
      SendResponseToClient();

    URLLoaderCompletionStatus status;
    status.error_code = error_code;
    if (error_code == net::ERR_QUIC_PROTOCOL_ERROR) {
      net::NetErrorDetails details;
      url_request_->PopulateNetErrorDetails(&details);
      status.extended_error_code = details.quic_connection_error;
    } else if (error_code == net::ERR_INCONSISTENT_IP_ADDRESS_SPACE) {
      // The error code is only used internally, translate it into a CORS error.
      DCHECK(cors_error_status_.has_value());
      status.error_code = net::ERR_FAILED;
    }
    status.exists_in_cache = url_request_->response_info().was_cached;
    status.completion_time = base::TimeTicks::Now();
    status.encoded_data_length = url_request_->GetTotalReceivedBytes();
    status.encoded_body_length = url_request_->GetRawBodyBytes();
    status.decoded_body_length = total_written_bytes_;
    status.resolve_error_info =
        url_request_->response_info().resolve_error_info;
    if (trust_token_status_)
      status.trust_token_operation_status = *trust_token_status_;
    status.cors_error_status = cors_error_status_;

    if ((options_ & mojom::kURLLoadOptionSendSSLInfoForCertificateError) &&
        net::IsCertStatusError(url_request_->ssl_info().cert_status)) {
      status.ssl_info = url_request_->ssl_info();
    }

    url_loader_client_.Get()->OnComplete(status);
  }

  DeleteSelf();
}

void URLLoader::OnMojoDisconnect() {
  NotifyCompleted(net::ERR_FAILED);
}

void URLLoader::OnResponseBodyStreamConsumerClosed(MojoResult result) {
  NotifyCompleted(net::ERR_FAILED);
}

void URLLoader::OnResponseBodyStreamReady(MojoResult result) {
  if (result != MOJO_RESULT_OK) {
    NotifyCompleted(net::ERR_FAILED);
    return;
  }

  ReadMore();
}

void URLLoader::DeleteSelf() {
  std::move(delete_callback_).Run(this);
}

void URLLoader::SendResponseToClient() {
  TRACE_EVENT(
      "loading", "network::URLLoader::SendResponseToClient",
      perfetto::Flow::ProcessScoped(url_request_->net_log().source().id), "url",
      url_request_->url());
  DCHECK_EQ(emitted_devtools_raw_request_, emitted_devtools_raw_response_);
  response_->emitted_extra_info = emitted_devtools_raw_request_;

  url_loader_client_.Get()->OnReceiveResponse(
      response_->Clone(), std::move(consumer_handle_), std::nullopt);
}

void URLLoader::CompletePendingWrite(bool success) {
  if (success && pending_write_) {
    // The write can only be completed immediately in case of a success, since
    // doing so invalidates memory of any attached NetToMojoIOBuffer's; but in
    // case of an abort, particularly one caused by a suspend, the failure may
    // be delivered to URLLoader while the disk_cache layer is still hanging on
    // to the now-invalid IOBuffer in some worker thread trying to commit it to
    // disk.  In case of an error, this will have to wait till everything is
    // destroyed.
    response_body_stream_ =
        pending_write_->Complete(pending_write_buffer_offset_);
  }
  total_written_bytes_ += pending_write_buffer_offset_;
  pending_write_ = nullptr;
  pending_write_buffer_offset_ = 0;
}

void URLLoader::SetRawResponseHeaders(
    scoped_refptr<const net::HttpResponseHeaders> headers) {
  raw_response_headers_ = headers;
}

void URLLoader::NotifyEarlyResponse(
    scoped_refptr<const net::HttpResponseHeaders> headers) {
  DCHECK(!has_received_response_);
  DCHECK(url_loader_client_.Get());
  DCHECK(headers);
  DCHECK_EQ(headers->response_code(), 103);

  // Calculate IP address space.
  mojom::ParsedHeadersPtr parsed_headers =
      PopulateParsedHeaders(headers.get(), url_request_->url());
  net::IPEndPoint transaction_endpoint;
  bool has_endpoint =
      url_request_->GetTransactionRemoteEndpoint(&transaction_endpoint);
  DCHECK(has_endpoint);
  CalculateClientAddressSpaceParams params{
      .client_address_space_inherited_from_service_worker = std::nullopt,
      .parsed_headers = &parsed_headers,
      .remote_endpoint = &transaction_endpoint,
  };
  mojom::IPAddressSpace ip_address_space =
      CalculateClientAddressSpace(url_request_->url(), params);

  mojom::ReferrerPolicy referrer_policy = ParseReferrerPolicy(*headers);

  url_loader_client_.Get()->OnReceiveEarlyHints(mojom::EarlyHints::New(
      std::move(parsed_headers), referrer_policy, ip_address_space));

  MaybeNotifyEarlyResponseToDevtools(*headers);
}

void URLLoader::MaybeNotifyEarlyResponseToDevtools(
    const net::HttpResponseHeaders& headers) {
  if (!devtools_observer_ || !devtools_request_id()) {
    return;
  }
  devtools_observer_->OnEarlyHintsResponse(
      devtools_request_id().value(), ResponseHeaderToRawHeaderPairs(headers));
}

void URLLoader::SetRawRequestHeadersAndNotify(
    net::HttpRawRequestHeaders headers) {
  // If we have seen_raw_request_headers_, then don't notify DevTools to prevent
  // duplicate ExtraInfo events.
  if (!seen_raw_request_headers_ && devtools_observer_ &&
      devtools_request_id()) {
    std::vector<network::mojom::HttpRawHeaderPairPtr> header_array;
    header_array.reserve(headers.headers().size());

    for (const auto& header : headers.headers()) {
      network::mojom::HttpRawHeaderPairPtr pair =
          network::mojom::HttpRawHeaderPair::New();
      pair->key = header.first;
      pair->value = header.second;
      header_array.push_back(std::move(pair));
    }
    DispatchOnRawRequest(std::move(header_array));
  }

  raw_request_line_size_ = headers.request_line().size();
  // Each header's format is "{key}: {value}\r\n" so add 4 bytes for each
  // header.
  raw_request_headers_size_ = headers.headers().size() * 4;
  for (const auto& [key, value] : headers.headers()) {
    raw_request_headers_size_ += key.size() + value.size();
  }

  if (cookie_observer_) {
    std::vector<mojom::CookieOrLineWithAccessResultPtr> reported_cookies;
    for (const auto& cookie_with_access_result :
         url_request_->maybe_sent_cookies()) {
      if (ShouldNotifyAboutCookie(
              cookie_with_access_result.access_result.status)) {
        reported_cookies.push_back(mojom::CookieOrLineWithAccessResult::New(
            mojom::CookieOrLine::NewCookie(cookie_with_access_result.cookie),
            cookie_with_access_result.access_result));
      }
    }

    if (!reported_cookies.empty()) {
      cookie_access_details_.emplace_back(mojom::CookieAccessDetails::New(
          mojom::CookieAccessDetails::Type::kRead, url_request_->url(),
          url_request_->isolation_info().top_frame_origin().value_or(
              url::Origin()),
          url_request_->site_for_cookies(), std::move(reported_cookies),
          devtools_request_id(), url_request_->ad_tagged(),
          url_request_->cookie_setting_overrides()));
    }
  }
}

bool URLLoader::IsSharedDictionaryReadAllowed() {
  shared_dictionary_allowed_check_passed_ =
      shared_dictionary_checker_->CheckAllowedToReadAndReport(
          url_request_->url(), url_request_->site_for_cookies(),
          url_request_->isolation_info());
  return shared_dictionary_allowed_check_passed_;
}

void URLLoader::DispatchOnRawRequest(
    std::vector<network::mojom::HttpRawHeaderPairPtr> headers) {
  DCHECK(devtools_observer_ && devtools_request_id());

  seen_raw_request_headers_ = true;

  net::LoadTimingInfo load_timing_info;
  url_request_->GetLoadTimingInfo(&load_timing_info);

  emitted_devtools_raw_request_ = true;

  bool is_main_frame_navigation =
      url_request_->isolation_info().IsMainFrameRequest() ||
      url_request_->force_main_frame_for_same_site_cookies();
  // TODO when crbug.com/40093296 "Don't trust |site_for_cookies| provided by
  // the renderer" is fixed. Update the FromNetworkIsolationKey method to use
  // url_request_->isolation_info().site_for_cookies() instead of
  // url_request_->site_for_cookies().
  std::optional<bool> site_has_cookie_in_other_partition =
      url_request_->context()->cookie_store()->SiteHasCookieInOtherPartition(
          net::SchemefulSite(url_request_->url()),
          net::CookiePartitionKey::FromNetworkIsolationKey(
              url_request_->isolation_info().network_isolation_key(),
              url_request_->site_for_cookies(),
              net::SchemefulSite(url_request_->url()),
              is_main_frame_navigation));
  network::mojom::OtherPartitionInfoPtr other_partition_info = nullptr;
  if (site_has_cookie_in_other_partition.has_value()) {
    other_partition_info = network::mojom::OtherPartitionInfo::New();
    other_partition_info->site_has_cookie_in_other_partition =
        *site_has_cookie_in_other_partition;
  }

  devtools_observer_->OnRawRequest(
      devtools_request_id().value(), url_request_->maybe_sent_cookies(),
      std::move(headers), load_timing_info.request_start,
      private_network_access_checker_.CloneClientSecurityState(),
      std::move(other_partition_info));
}

bool URLLoader::DispatchOnRawResponse() {
  if (url_request_->response_headers() && !seen_raw_request_headers_) {
    // Record request metrics here instead of in NotifyCompleted to account for
    // redirects.
    RecordRequestMetrics();
  }

  if (!devtools_observer_ || !devtools_request_id() ||
      !url_request_->response_headers()) {
    return false;
  }

  if (url_request_->was_cached() && !seen_raw_request_headers_) {
    // If a response in a redirect chain has been cached,
    // we need to clear the emitted_devtools_raw_request_ and
    // emitted_devtools_raw_response_ flags to prevent misreporting
    // that extra info was available on the response. We also suppress
    // reporting the extra info events here.
    emitted_devtools_raw_request_ = false;
    emitted_devtools_raw_response_ = false;
    return false;
  }

  // This is gated by enable_reporting_raw_headers_ to be backwards compatible
  // with the old report_raw_headers behavior, where we wouldn't even send
  // raw_response_headers_ to the trusted browser process based devtools
  // instrumentation. This is observed in the case of HSTS redirects, where
  // url_request_->response_headers has the HSTS redirect headers, like
  // Non-Authoritative-Reason, but raw_response_headers_ has something else
  // which doesn't include HSTS information. This is tested by
  // DevToolsTest.TestRawHeadersWithRedirectAndHSTS.
  // TODO(crbug.com/40781698): Remove enable_reporting_raw_headers_
  const net::HttpResponseHeaders* response_headers =
      raw_response_headers_ && enable_reporting_raw_headers_
          ? raw_response_headers_.get()
          : url_request_->response_headers();
  std::vector<network::mojom::HttpRawHeaderPairPtr> header_array =
      ResponseHeaderToRawHeaderPairs(*response_headers);

  // Only send the "raw" header text when the headers were actually send in
  // text form (i.e. not QUIC or SPDY)
  std::optional<std::string> raw_response_headers;

  const net::HttpResponseInfo& response_info = url_request_->response_info();

  if (!response_info.DidUseQuic() && !response_info.was_fetched_via_spdy) {
    raw_response_headers =
        std::make_optional(net::HttpUtil::ConvertHeadersBackToHTTPResponse(
            response_headers->raw_headers()));
  }

  if (!seen_raw_request_headers_) {
    // If we send OnRawResponse(), make sure we send OnRawRequest() event if
    // we haven't had the callback from net, to make the client life easier.
    DispatchOnRawRequest({});
  }

  emitted_devtools_raw_response_ = true;
  devtools_observer_->OnRawResponse(
      devtools_request_id().value(), url_request_->maybe_stored_cookies(),
      std::move(header_array), raw_response_headers,
      private_network_access_checker_.ResponseAddressSpace().value_or(
          mojom::IPAddressSpace::kUnknown),
      response_headers->response_code(), url_request_->cookie_partition_key());

  return true;
}

void URLLoader::SendUploadProgress(const net::UploadProgress& progress) {
  url_loader_client_.Get()->OnUploadProgress(
      progress.position(), progress.size(),
      base::BindOnce(&URLLoader::OnUploadProgressACK,
                     weak_ptr_factory_.GetWeakPtr()));
}

void URLLoader::OnUploadProgressACK() {
  if (upload_progress_tracker_)
    upload_progress_tracker_->OnAckReceived();
}

void URLLoader::OnSSLCertificateErrorResponse(const net::SSLInfo& ssl_info,
                                              int net_error) {
  if (net_error == net::OK) {
    url_request_->ContinueDespiteLastError();
    return;
  }

  url_request_->CancelWithSSLError(net_error, ssl_info);
}

bool URLLoader::HasDataPipe() const {
  return pending_write_ || response_body_stream_.is_valid();
}

void URLLoader::ResumeStart() {
  url_request_->LogUnblocked();
  url_request_->Start();
}

void URLLoader::OnBeforeSendHeadersComplete(
    net::NetworkDelegate::OnBeforeStartTransactionCallback callback,
    int result,
    const std::optional<net::HttpRequestHeaders>& headers) {
  if (include_request_cookies_with_response_ && headers) {
    request_cookies_.clear();
    std::string cookie_header =
        headers->GetHeader(net::HttpRequestHeaders::kCookie)
            .value_or(std::string());
    net::cookie_util::ParseRequestCookieLine(cookie_header, &request_cookies_);
  }
  std::move(callback).Run(result, headers);
}

void URLLoader::OnHeadersReceivedComplete(
    net::CompletionOnceCallback callback,
    scoped_refptr<net::HttpResponseHeaders>* out_headers,
    std::optional<GURL>* out_preserve_fragment_on_redirect_url,
    int result,
    const std::optional<std::string>& headers,
    const std::optional<GURL>& preserve_fragment_on_redirect_url) {
  if (headers) {
    *out_headers =
        base::MakeRefCounted<net::HttpResponseHeaders>(headers.value());
  }
  *out_preserve_fragment_on_redirect_url = preserve_fragment_on_redirect_url;
  std::move(callback).Run(result);
}

void URLLoader::CompleteBlockedResponse(
    int error_code,
    bool should_report_orb_blocking,
    std::optional<mojom::BlockedByResponseReason> reason) {
  if (has_received_response_) {
    // The response headers and body shouldn't yet be sent to the
    // URLLoaderClient.
    CHECK(response_);
    CHECK(consumer_handle_.is_valid() ||
          (options_ & mojom::kURLLoadOptionReadAndDiscardBody));
  }

  // Tell the URLLoaderClient that the response has been completed.
  URLLoaderCompletionStatus status;
  status.error_code = error_code;
  status.completion_time = base::TimeTicks::Now();
  status.encoded_data_length = 0;
  status.encoded_body_length = 0;
  status.decoded_body_length = 0;
  status.should_report_orb_blocking = should_report_orb_blocking;
  status.blocked_by_response_reason = reason;

  url_loader_client_.Get()->OnComplete(status);

  // Reset the connection to the URLLoaderClient.  This helps ensure that we
  // won't accidentally leak any data to the renderer from this point on.
  url_loader_client_.Reset();
}

URLLoader::BlockResponseForOrbResult URLLoader::BlockResponseForOrb() {
  // ORB should only do work after the response headers have been received.
  DCHECK(has_received_response_);

  // Caller should have set up a OrbAnalyzer for BlockResponseForOrb to be
  // able to do its job.
  DCHECK(orb_analyzer_);

  // The response headers and body shouldn't yet be sent to the URLLoaderClient.
  DCHECK(response_);
  DCHECK(consumer_handle_.is_valid());

  // Send stripped headers to the real URLLoaderClient.
  orb::SanitizeBlockedResponseHeaders(*response_);

  // Determine error code. This essentially handles the "ORB v0.1" and "ORB
  // v0.2" difference.
  int blocked_error_code =
      (orb_analyzer_->ShouldHandleBlockedResponseAs() ==
       orb::ResponseAnalyzer::BlockedResponseHandling::kEmptyResponse)
          ? net::OK
          : net::ERR_BLOCKED_BY_ORB;

  // Send empty body to the real URLLoaderClient. This preserves "ORB v0.1"
  // behaviour and will also go away once
  // OpaqueResponseBlockingErrorsForAllFetches is perma-enabled.
  if (blocked_error_code == net::OK) {
    mojo::ScopedDataPipeProducerHandle producer_handle;
    mojo::ScopedDataPipeConsumerHandle consumer_handle;
    MojoResult result = mojo::CreateDataPipe(kBlockedBodyAllocationSize,
                                             producer_handle, consumer_handle);
    if (result != MOJO_RESULT_OK) {
      // Defer calling NotifyCompleted to make sure the caller can still access
      // |this|.
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(&URLLoader::NotifyCompleted,
                                    weak_ptr_factory_.GetWeakPtr(),
                                    net::ERR_INSUFFICIENT_RESOURCES));

      return kWillCancelRequest;
    }
    producer_handle.reset();

    // Tell the real URLLoaderClient that the response has been completed.
    url_loader_client_.Get()->OnReceiveResponse(
        response_->Clone(), std::move(consumer_handle), std::nullopt);
  }

  // At this point, orb_analyzer_ has done its duty. We'll reset it now
  // to force UMA reporting to happen earlier, to support easier testing.
  bool should_report_blocked_response =
      orb_analyzer_->ShouldReportBlockedResponse();
  orb_analyzer_.reset();
  CompleteBlockedResponse(blocked_error_code, should_report_blocked_response);

  // Close the socket associated with the request, to prevent leaking
  // information.
  url_request_->AbortAndCloseConnection();

  // Delete self and cancel the request - the caller doesn't need to continue.
  //
  // DeleteSelf is posted asynchronously, to make sure that the callers (e.g.
  // URLLoader::OnResponseStarted and/or URLLoader::DidRead instance methods)
  // can still safely dereference |this|.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&URLLoader::DeleteSelf, weak_ptr_factory_.GetWeakPtr()));
  return kWillCancelRequest;
}

bool URLLoader::MaybeBlockResponseForOrb(
    orb::ResponseAnalyzer::Decision orb_decision) {
  DCHECK(orb_analyzer_);
  DCHECK(is_more_orb_sniffing_needed_);
  bool will_cancel = false;
  switch (orb_decision) {
    case network::orb::ResponseAnalyzer::Decision::kBlock: {
      will_cancel = BlockResponseForOrb() == kWillCancelRequest;
      orb_analyzer_.reset();
      is_more_orb_sniffing_needed_ = false;
      break;
    }
    case network::orb::ResponseAnalyzer::Decision::kAllow:
      orb_analyzer_.reset();
      is_more_orb_sniffing_needed_ = false;
      break;
    case network::orb::ResponseAnalyzer::Decision::kSniffMore:
      break;
  }
  DCHECK_EQ(is_more_orb_sniffing_needed_, !!orb_analyzer_);
  return will_cancel;
}

void URLLoader::ReportFlaggedResponseCookies(bool call_cookie_observer) {
  if (!cookie_observer_) {
    return;
  }

  std::vector<mojom::CookieOrLineWithAccessResultPtr> reported_cookies;
  for (const auto& cookie_line_and_access_result :
       url_request_->maybe_stored_cookies()) {
    if (ShouldNotifyAboutCookie(
            cookie_line_and_access_result.access_result.status)) {
      mojom::CookieOrLinePtr cookie_or_line;
      if (cookie_line_and_access_result.cookie.has_value()) {
        cookie_or_line = mojom::CookieOrLine::NewCookie(
            cookie_line_and_access_result.cookie.value());
      } else {
        cookie_or_line = mojom::CookieOrLine::NewCookieString(
            cookie_line_and_access_result.cookie_string);
      }

      reported_cookies.push_back(mojom::CookieOrLineWithAccessResult::New(
          std::move(cookie_or_line),
          cookie_line_and_access_result.access_result));
    }
  }

  if (!reported_cookies.empty()) {
    cookie_access_details_.emplace_back(mojom::CookieAccessDetails::New(
        mojom::CookieAccessDetails::Type::kChange, url_request_->url(),
        url_request_->isolation_info().top_frame_origin().value_or(
            url::Origin()),
        url_request_->site_for_cookies(), std::move(reported_cookies),
        devtools_request_id(), url_request_->ad_tagged(),
        url_request_->cookie_setting_overrides()));
    if (call_cookie_observer) {
      cookie_observer_->OnCookiesAccessed(std::move(cookie_access_details_));
    }
  }
}

void URLLoader::StartReading() {
  if (!is_more_mime_sniffing_needed_ && !is_more_orb_sniffing_needed_) {
    // Treat feed types as text/plain.
    if (response_->mime_type == "application/rss+xml" ||
        response_->mime_type == "application/atom+xml") {
      response_->mime_type.assign("text/plain");
    }
    SendResponseToClient();
  }

  // Start reading...
  ReadMore();
}

bool URLLoader::ShouldForceIgnoreSiteForCookies(
    const ResourceRequest& request) {
  // Ignore site for cookies in requests from an initiator covered by the
  // same-origin-policy exclusions in `origin_access_list_` (typically requests
  // initiated by Chrome Extensions).
  if (request.request_initiator.has_value() &&
      cors::OriginAccessList::AccessState::kAllowed ==
          origin_access_list_->CheckAccessState(
              request.request_initiator.value(), request.url)) {
    return true;
  }

  // Convert `site_for_cookies` into an origin (an opaque origin if
  // `net::SiteForCookies::IsNull()` returns true).
  //
  // Note that `site_for_cookies` is a _site_ rather than an _origin_, but for
  // Chrome Extensions the _site_ and _origin_ of a host are the same extension
  // id.  Thanks to this, for Chrome Extensions, we can pass a _site_ into
  // OriginAccessChecks (which normally expect an _origin_).
  url::Origin site_origin =
      url::Origin::Create(request.site_for_cookies.RepresentativeUrl());

  // If `site_for_cookies` represents an origin that is granted access to the
  // initiator and the target by `origin_access_list_` (typically such
  // `site_for_cookies` represents a Chrome Extension), then we also should
  // force ignoring of site for cookies if the initiator and the target are
  // same-site.
  //
  // Ideally we would walk up the frame tree and check that each ancestor is
  // first-party to the main frame (treating the `origin_access_list_`
  // exceptions as "first-party").  But walking up the tree is not possible in
  // //services/network and so we make do with just checking the direct
  // initiator of the request.
  //
  // We also check same-siteness between the initiator and the requested URL,
  // because setting `force_ignore_site_for_cookies` to true causes Strict
  // cookies to be attached, and having the initiator be same-site to the
  // request URL is a requirement for Strict cookies (see
  // net::cookie_util::ComputeSameSiteContext).
  if (!site_origin.opaque() && request.request_initiator.has_value()) {
    bool site_can_access_target =
        cors::OriginAccessList::AccessState::kAllowed ==
        origin_access_list_->CheckAccessState(site_origin, request.url);
    bool site_can_access_initiator =
        cors::OriginAccessList::AccessState::kAllowed ==
        origin_access_list_->CheckAccessState(
            site_origin, request.request_initiator->GetURL());
    net::SiteForCookies site_of_initiator =
        net::SiteForCookies::FromOrigin(request.request_initiator.value());
    bool are_initiator_and_target_same_site =
        site_of_initiator.IsFirstParty(request.url);
    if (site_can_access_initiator && site_can_access_target &&
        are_initiator_and_target_same_site) {
      return true;
    }
  }

  return false;
}

void URLLoader::SetRequestCredentials(const GURL& url) {
  bool policies_allow_credentials = WebPoliciesAllowCredentials(url);

  bool allow_credentials = ShouldAllowCredentials(request_credentials_mode_) &&
                           policies_allow_credentials;

  bool allow_client_certificates =
      ShouldSendClientCertificates(request_credentials_mode_) &&
      policies_allow_credentials;

  // The decision not to include credentials is sticky. This is equivalent to
  // checking the tainted origin flag in the fetch specification.
  if (!allow_credentials)
    url_request_->set_allow_credentials(false);
  if (!allow_client_certificates)
    url_request_->set_send_client_certs(false);

  // Contrary to Firefox or blink's cache, the HTTP cache doesn't distinguish
  // requests including user's credentials from the anonymous ones yet. See
  // https://docs.google.com/document/d/1lvbiy4n-GM5I56Ncw304sgvY5Td32R6KHitjRXvkZ6U
  // As a workaround until a solution is implemented, the cached responses
  // aren't used for those requests.
  if (!policies_allow_credentials) {
    url_request_->SetLoadFlags(url_request_->load_flags() |
                               net::LOAD_BYPASS_CACHE);
  }
}

// [spec]:
// https://fetch.spec.whatwg.org/#cross-origin-embedder-policy-allows-credentials
bool URLLoader::WebPoliciesAllowCredentials(const GURL& url) {
  // [spec]: To check if Cross-Origin-Embedder-Policy allows credentials, given
  //         a request request, run these steps:

  // [spec]  1. If request’s mode is not "no-cors", then return true.
  switch (request_mode_) {
    case mojom::RequestMode::kCors:
    case mojom::RequestMode::kCorsWithForcedPreflight:
    case mojom::RequestMode::kNavigate:
    case mojom::RequestMode::kSameOrigin:
      return true;

    case mojom::RequestMode::kNoCors:
      break;
  }

  // [spec]: 2. If request’s client is null, then return true.
  if (!factory_params_->client_security_state)
    return true;

  // [spec]: 3. If request’s client’s policy container’s embedder policy’s value
  //            is not "credentialless", then return true.
  // Document-Isolation-Policy: also check that Document-Isolation-Policy allows
  // credentials.
  if (factory_params_->client_security_state->cross_origin_embedder_policy
              .value !=
          mojom::CrossOriginEmbedderPolicyValue::kCredentialless &&
      factory_params_->client_security_state->document_isolation_policy.value !=
          mojom::DocumentIsolationPolicyValue::kIsolateAndCredentialless) {
    return true;
  }

  // [spec]: 4. If request’s origin is same origin with request’s current URL’s
  //            origin and request does not have a redirect-tainted origin, then
  //            return true.
  url::Origin request_initiator =
      url_request_->initiator().value_or(url::Origin());
  if (request_initiator.IsSameOriginWith(url))
    return true;

  // [spec]: 5. Return false.
  return false;
}

bool URLLoader::ShouldSendTransferSizeUpdated() const {
  return devtools_request_id() || url_request_->ad_tagged() ||
         !base::FeatureList::IsEnabled(features::kReduceTransferSizeUpdatedIPC);
}

bool URLLoader::ShouldSetLoadWithStorageAccess() const {
  CHECK(url_request_);
  if (!IncludesValidLoadField(url_request_->response_headers())) {
    return false;
  }

  auto determine_storage_access_load_outcome =
      [&]() -> net::cookie_util::ActivateStorageAccessLoadOutcome {
    if (!url_request_context_->network_delegate()->IsStorageAccessHeaderEnabled(
            base::OptionalToPtr(
                url_request_->isolation_info().top_frame_origin()),
            url_request_->url())) {
      return net::cookie_util::ActivateStorageAccessLoadOutcome::
          kFailureHeaderDisabled;
    }
    if (!url_request_->storage_access_status()) {
      return net::cookie_util::ActivateStorageAccessLoadOutcome::
          kFailureInvalidStatus;
    }
    switch (url_request_->storage_access_status().value()) {
      case net::cookie_util::StorageAccessStatus::kNone:
        return net::cookie_util::ActivateStorageAccessLoadOutcome::
            kFailureInvalidStatus;
      case net::cookie_util::StorageAccessStatus::kInactive:
      case net::cookie_util::StorageAccessStatus::kActive:
        return net::cookie_util::ActivateStorageAccessLoadOutcome::kSuccess;
    }
    NOTREACHED();
  };

  auto outcome = determine_storage_access_load_outcome();
  base::UmaHistogramEnumeration(
      "API.StorageAccessHeader.ActivateStorageAccessLoadOutcome", outcome);
  return outcome ==
         net::cookie_util::ActivateStorageAccessLoadOutcome::kSuccess;
}

void URLLoader::RecordRequestMetrics() {
  // All histograms recorded here are of the form:
  // "NetworkService.Requests.{Multiplexed}.{RequestType}.{Method}.{Result}.{Metric}".
  // For example:
  // "NetworkService.Requests.Simple.MainFrame.Get.Success.TotalRequestSize".
  absl::InlinedVector<std::string_view, 10> histogram_prefix_pieces = {
      "NetworkService", "Requests"};

  const net::HttpResponseInfo& response_info = url_request_->response_info();
  if (IsMultiplexedConnection(response_info)) {
    histogram_prefix_pieces.push_back("Multiplexed");
  } else {
    histogram_prefix_pieces.push_back("Simple");
  }

  switch (url_request_->isolation_info().request_type()) {
    case net::IsolationInfo::RequestType::kMainFrame:
      histogram_prefix_pieces.push_back("MainFrame");
      break;
    case net::IsolationInfo::RequestType::kSubFrame:
    case net::IsolationInfo::RequestType::kOther:
      // TODO(crbug.com/362787712): Add metrics for other types of requests.
      return;
  }

  if (url_request_->method() == "GET") {
    histogram_prefix_pieces.push_back("Get");
  } else {
    // Other types of requests need to be handled differently e.g. the total
    // request size of a POST request needs to include the body.
    // TODO(crbug.com/362787712): Add metrics for other types of requests.
    return;
  }

  const int response_code = response_info.headers->response_code();
  if (response_code < 199) {
    // Ignore information responses because they are not complete requests.
    return;
  } else if (response_code < 299 || response_code < 399) {
    // We consider redirects a success.
    histogram_prefix_pieces.push_back("Success");
  } else if (response_code < 499) {
    histogram_prefix_pieces.push_back("ClientError");
  } else if (response_code < 599) {
    histogram_prefix_pieces.push_back("ServerError");
  } else {
    // Ignore unexpected server response codes.
    return;
  }

  auto make_histogram_name =
      [&histogram_prefix_pieces](std::string_view metric) {
        histogram_prefix_pieces.push_back(metric);
        std::string name = base::JoinString(histogram_prefix_pieces, ".");
        histogram_prefix_pieces.pop_back();
        return name;
      };

  base::UmaHistogramCounts100000(make_histogram_name("TotalUrlSize"),
                                 url_request_->url().spec().size());

  // HTTP/2 and HTTP/3 requests don't separate request line from headers so no
  // need to record header metrics separately.
  if (!IsMultiplexedConnection(response_info)) {
    base::UmaHistogramCounts100000(make_histogram_name("TotalHeadersSize"),
                                   raw_request_headers_size_);
  }

  // For HTTP/2 and HTTP/3 the request line is included in the headers, but
  // `raw_request_line_size_` is 0 for these requests, so we can add it
  // unconditionally for all requests.
  size_t total_request_size =
      raw_request_headers_size_ + raw_request_line_size_;
  base::UmaHistogramCounts100000(make_histogram_name("TotalRequestSize"),
                                 total_request_size);
}

}  // namespace network
