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
#include "base/containers/enum_set.h"
#include "base/containers/fixed_flat_set.h"
#include "base/feature_list.h"
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
#include "base/sequence_checker.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/string_view_util.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/thread_annotations.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/typed_macros.h"
#include "base/unguessable_token.h"
#include "build/build_config.h"
#include "mojo/public/cpp/bindings/shared_remote.h"
#include "mojo/public/cpp/system/simple_watcher.h"
#include "net/base/completion_once_callback.h"
#include "net/base/features.h"
#include "net/base/isolation_info.h"
#include "net/base/load_flags.h"
#include "net/base/load_timing_info.h"
#include "net/base/load_timing_internal_info.h"
#include "net/base/mime_sniffer.h"
#include "net/base/net_error_details.h"
#include "net/base/net_errors.h"
#include "net/base/proxy_chain.h"
#include "net/base/schemeful_site.h"
#include "net/base/task/task_runner.h"
#include "net/base/transport_info.h"
#include "net/base/upload_data_stream.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_inclusion_status.h"
#include "net/cookies/cookie_setting_override.h"
#include "net/cookies/cookie_store.h"
#include "net/cookies/cookie_util.h"
#include "net/cookies/site_for_cookies.h"
#include "net/cookies/static_cookie_policy.h"
#include "net/device_bound_sessions/session.h"
#include "net/dns/public/secure_dns_policy.h"
#include "net/filter/filter_source_stream.h"
#include "net/http/http_connection_info.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_util.h"
#include "net/http/structured_headers.h"
#include "net/log/net_log_source_type.h"
#include "net/log/net_log_util.h"
#include "net/log/net_log_with_source.h"
#include "net/ssl/client_cert_store.h"
#include "net/ssl/ssl_connection_status_flags.h"
#include "net/ssl/ssl_info.h"
#include "net/ssl/ssl_private_key.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "net/url_request/redirect_info.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"
#include "services/network/accept_ch_frame_interceptor.h"
#include "services/network/ad_heuristic_cookie_overrides.h"
#include "services/network/cookie_settings.h"
#include "services/network/devtools_durable_msg.h"
#include "services/network/file_opener_for_upload.h"
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
#include "services/network/public/cpp/loading_params.h"
#include "services/network/public/cpp/net_adapters.h"
#include "services/network/public/cpp/network_switches.h"
#include "services/network/public/cpp/parsed_headers.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/sri_message_signatures.h"
#include "services/network/public/mojom/client_security_state.mojom-forward.h"
#include "services/network/public/mojom/client_security_state.mojom.h"
#include "services/network/public/mojom/cookie_access_observer.mojom-forward.h"
#include "services/network/public/mojom/cookie_access_observer.mojom.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "services/network/public/mojom/devtools_observer.mojom.h"
#include "services/network/public/mojom/early_hints.mojom.h"
#include "services/network/public/mojom/fetch_api.mojom.h"
#include "services/network/public/mojom/http_raw_headers.mojom.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/network_context_client.mojom.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/resource_scheduler/resource_scheduler_client.h"
#include "services/network/sec_header_helpers.h"
#include "services/network/shared_dictionary/shared_dictionary_access_checker.h"
#include "services/network/shared_dictionary/shared_dictionary_manager.h"
#include "services/network/shared_dictionary/shared_dictionary_storage.h"
#include "services/network/shared_resource_checker.h"
#include "services/network/shared_storage/shared_storage_request_helper.h"
#include "services/network/slop_bucket.h"
#include "services/network/ssl_private_key_proxy.h"
#include "services/network/throttling/scoped_throttling_token.h"
#include "services/network/throttling/throttling_controller.h"
#include "services/network/throttling/throttling_network_interceptor.h"
#include "services/network/trust_tokens/trust_token_request_helper.h"
#include "services/network/trust_tokens/trust_token_url_loader_interceptor.h"
#include "services/network/url_loader_factory.h"
#include "services/network/url_loader_util.h"
#include "third_party/abseil-cpp/absl/container/inlined_vector.h"
#include "url/origin.h"

namespace network {

namespace {

BASE_FEATURE(kDelayedCookieNotification, base::FEATURE_DISABLED_BY_DEFAULT);

// Cannot use 0, because this means "default" in
// mojo::core::Core::CreateDataPipe
constexpr size_t kBlockedBodyAllocationSize = 1;

// Size to allocate for `discard_buffer_`.
constexpr size_t kDiscardBufferSize = 128 * 1024;

constexpr char kActivateStorageAccessHeader[] = "activate-storage-access";

bool ShouldNotifyAboutCookie(net::CookieInclusionStatus status) {
  // Notify about cookies actually used, and those blocked by preferences ---
  // for purposes of cookie UI --- as well those carrying warnings pertaining to
  // SameSite features and cookies with non-ASCII domain attributes, in order to
  // issue a deprecation warning for them.

  // Filter out tentative secure source scheme warnings. They're used for netlog
  // debugging and not something we want to inform cookie observers about.
  status.RemoveWarningReason(
      net::CookieInclusionStatus::WarningReason::
          WARN_TENTATIVELY_ALLOWING_SECURE_SOURCE_SCHEME);

  return status.IsInclude() || status.ShouldWarn() ||
         status.HasExclusionReason(net::CookieInclusionStatus::ExclusionReason::
                                       EXCLUDE_USER_PREFERENCES) ||
         status.HasExclusionReason(net::CookieInclusionStatus::ExclusionReason::
                                       EXCLUDE_THIRD_PARTY_PHASEOUT) ||
         status.HasExclusionReason(net::CookieInclusionStatus::ExclusionReason::
                                       EXCLUDE_DOMAIN_NON_ASCII) ||
         status.HasExclusionReason(net::CookieInclusionStatus::ExclusionReason::
                                       EXCLUDE_PORT_MISMATCH) ||
         status.HasExclusionReason(net::CookieInclusionStatus::ExclusionReason::
                                       EXCLUDE_SCHEME_MISMATCH);
}

scoped_refptr<RefCountedDeviceBoundSessionAccessObserverRemote>
MaybeInitializeDeviceBoundSessionAccessObserverSharedRemote(
    ObserverWrapper<mojom::DeviceBoundSessionAccessObserver>& observer,
    URLLoaderContext& context) {
  if (!base::FeatureList::IsEnabled(
          features::kDeviceBoundSessionAccessObserverSharedRemote)) {
    return nullptr;
  }
  auto remote = observer.TakeRemote();
  if (remote) {
    return base::MakeRefCounted<
        RefCountedDeviceBoundSessionAccessObserverRemote>(std::move(remote));
  }
  return context.GetDeviceBoundSessionAccessObserverSharedRemote();
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
  for (const auto& cookie : parsed_cookies_from_browser) {
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

bool IncludesValidLoadField(const net::HttpResponseHeaders* headers) {
  if (!headers) {
    return false;
  }

  std::optional<std::string> header_value =
      headers->GetNormalizedHeader(kActivateStorageAccessHeader);
  if (!header_value) {
    return false;
  }
  const std::optional<net::structured_headers::ParameterizedItem> item =
      net::structured_headers::ParseItem(*header_value);
  if (!item.has_value()) {
    return false;
  }
  return item->item.is_token() && item->item.GetString() == "load";
}

mojo::SharedRemote<mojom::DeviceBoundSessionAccessObserver> Clone(
    mojom::DeviceBoundSessionAccessObserver& observer) {
  TRACE_EVENT("loading", "CloneDeviceBoundSessionAccessObserver");
  base::ScopedUmaHistogramTimer timer(
      "NetworkService.URLLoader.CloneDeviceBoundSessionAccessObserver",
      base::ScopedUmaHistogramTimer::ScopedHistogramTiming::kMicrosecondTimes);
  mojo::SharedRemote<mojom::DeviceBoundSessionAccessObserver> new_observer;
  observer.Clone(new_observer.BindNewPipeAndPassReceiver());
  return new_observer;
}

int32_t PopulateOptions(int32_t initial_options,
                        bool is_orb_enabled,
                        bool has_devtools_request_id) {
  int32_t options = initial_options;

  if (options & mojom::kURLLoadOptionReadAndDiscardBody) {
    CHECK(!(options & mojom::kURLLoadOptionSniffMimeType))
        << "options ReadAndDiscardBody and SniffMimeType cannot be used "
           "together";
    if (is_orb_enabled) {
      // TODO(ricea): Make ReadAndDiscardBody and ORB work together.
      LOG(WARNING) << "Disabling ReadAndDiscardBody because ORB is enabled";
      options &= ~mojom::kURLLoadOptionReadAndDiscardBody;
    }
  }

  if (has_devtools_request_id) {
    options |= mojom::kURLLoadOptionSendSSLInfoWithResponse |
               mojom::kURLLoadOptionSendSSLInfoForCertificateError;
  }

  return options;
}

const scoped_refptr<base::SingleThreadTaskRunner>& TaskRunner(
    net::RequestPriority priority) {
  if (features::kNetworkServiceTaskSchedulerURLLoader.Get()) {
    return net::GetTaskRunner(priority);
  }
  return base::SingleThreadTaskRunner::GetCurrentDefault();
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
    ObserverWrapper<mojom::CookieAccessObserver> cookie_observer,
    ObserverWrapper<mojom::TrustTokenAccessObserver> trust_token_observer,
    ObserverWrapper<mojom::URLLoaderNetworkServiceObserver>
        url_loader_network_observer,
    ObserverWrapper<mojom::DevToolsObserver> devtools_observer,
    ObserverWrapper<mojom::DeviceBoundSessionAccessObserver>
        device_bound_session_observer,
    mojo::PendingRemote<mojom::AcceptCHFrameObserver> accept_ch_frame_observer,
    bool shared_storage_writable_eligible,
    SharedResourceChecker& shared_resource_checker,
    base::WeakPtr<DevtoolsDurableMessage> devtools_durable_message)
    : url_request_context_(context.GetUrlRequestContext()),
      network_context_client_(context.GetNetworkContextClient()),
      delete_callback_(std::move(delete_callback)),
      resource_type_(request.resource_type),
      is_load_timing_enabled_(request.enable_load_timing),
      factory_params_(context.GetFactoryParams()),
      coep_reporter_(context.GetCoepReporter()),
      dip_reporter_(context.GetDipReporter()),
      request_id_(request_id),
      keepalive_request_size_(keepalive_request_size),
      keepalive_(request.keepalive),
      client_security_state_(
          request.trusted_params
              ? request.trusted_params->client_security_state.Clone()
              : nullptr),
      do_not_prompt_for_login_(request.do_not_prompt_for_login),
      receiver_(this, std::move(url_loader_receiver)),
      url_loader_client_(std::move(url_loader_client),
                         std::move(sync_url_loader_client)),
      writable_handle_watcher_(FROM_HERE,
                               mojo::SimpleWatcher::ArmingPolicy::MANUAL,
                               TaskRunner(request.priority)),
      peer_closed_handle_watcher_(FROM_HERE,
                                  mojo::SimpleWatcher::ArmingPolicy::MANUAL,
                                  TaskRunner(request.priority)),
      per_factory_orb_state_(context.GetMutableOrbState()),
      devtools_request_id_(request.devtools_request_id),
      options_(PopulateOptions(options,
                               factory_params_->is_orb_enabled,
                               !!devtools_request_id())),
      request_mode_(request.mode),
      request_credentials_mode_(request.credentials_mode),
      has_user_activation_(request.trusted_params &&
                           request.trusted_params->has_user_activation),
      request_destination_(request.destination),
      expected_public_keys_(request.expected_public_keys),
      resource_scheduler_client_(context.GetResourceSchedulerClient()),
      keepalive_statistics_recorder_(std::move(keepalive_statistics_recorder)),
      fetch_window_id_(request.fetch_window_id),
      private_network_access_interceptor_(request,
                                          GetClientSecurityState(),
                                          options_),
      trust_token_interceptor_(TrustTokenUrlLoaderInterceptor::MaybeCreate(
          std::move(trust_token_helper_factory))),
      shared_dictionary_checker_(std::move(shared_dictionary_checker)),
      origin_access_list_(context.GetOriginAccessList()),
      cookie_observer_(std::move(cookie_observer)),
      trust_token_observer_(std::move(trust_token_observer)),
      url_loader_network_observer_(std::move(url_loader_network_observer)),
      devtools_observer_(std::move(devtools_observer)),
      device_bound_session_observer_(std::move(device_bound_session_observer)),
      device_bound_session_observer_shared_remote_(
          MaybeInitializeDeviceBoundSessionAccessObserverSharedRemote(
              device_bound_session_observer_,
              context)),
      shared_storage_request_helper_(
          std::make_unique<SharedStorageRequestHelper>(
              shared_storage_writable_eligible,
              url_loader_network_observer_.get())),
      ad_auction_event_record_request_helper_(
          request.attribution_reporting_eligibility,
          url_loader_network_observer_.get()),
      has_fetch_streaming_upload_body_(
          url_loader_util::HasFetchStreamingUploadBody(request)),
      accept_ch_frame_interceptor_(AcceptCHFrameInterceptor::MaybeCreate(
          std::move(accept_ch_frame_observer),
          request.trusted_params ? request.trusted_params->enabled_client_hints
                                 : std::nullopt)),
      allow_cookies_from_browser_(
          request.trusted_params &&
          request.trusted_params->allow_cookies_from_browser),
      cookies_from_browser_(allow_cookies_from_browser_
                                ? url_loader_util::GetCookiesFromHeaders(
                                      request.headers,
                                      request.cors_exempt_headers)
                                : std::string()),
      include_request_cookies_with_response_(
          request.trusted_params &&
          request.trusted_params->include_request_cookies_with_response),
      include_load_timing_internal_info_with_response_(
          request.trusted_params.has_value()),
      provide_data_use_updates_(context.DataUseUpdatesEnabled()),
      partial_decoder_decoding_buffer_size_(net::kMaxBytesToSniff),
      permissions_policy_(request.permissions_policy),
      devtools_durable_message_(devtools_durable_message) {
  DCHECK(delete_callback_);

  if (options_ & mojom::kURLLoadOptionReadAndDiscardBody) {
    if (!factory_params_->is_orb_enabled) {
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
  receiver_.set_disconnect_handler(
      base::BindOnce(&URLLoader::OnMojoDisconnect, base::Unretained(this)));

  // If the request is to a URL that we can determine is an LNA request from
  // just the URL, then trigger the LNA prompt. We only trigger this for request
  // where GetAddressSpaceFromUrl() returns a value as those would also trigger
  // if and when we move the LNA check to after hostname resolution but before
  // connection.
  const mojom::ClientSecurityState* client_security_state =
      GetClientSecurityState();
  if (client_security_state &&
      client_security_state->private_network_request_policy ==
          mojom::PrivateNetworkRequestPolicy::kPermissionBlock &&
      url_loader_network_observer_) {
    std::optional<mojom::IPAddressSpace> url_address_space =
        GetAddressSpaceFromUrl(request.url);
    if (url_address_space) {
      PrivateNetworkAccessChecker lna_checker(request, client_security_state,
                                              options_);
      if (lna_checker.CheckAddressSpace(*url_address_space) ==
          PrivateNetworkAccessCheckResult::kLNAPermissionRequired) {
        // Ignoring the result of the permission here because the point of this
        // call is to get the permission prompt shown if the permission is
        // "prompt". Later LNA checks will check the permission and use the
        // the result.
        url_loader_network_observer_->OnLocalNetworkAccessPermissionRequired(
            base::BindOnce([](bool permission_granted) {}));
      }
    }
  }

  url_request_ = url_request_context_->CreateRequest(
      request.url, request.priority, this, traffic_annotation,
      /*is_for_websockets=*/false, request.net_log_create_info);

  TRACE_EVENT("loading", "URLLoader::URLLoader",
              net::NetLogWithSourceToFlow(url_request_->net_log()));
  // Set up UserData (pointing to `this`) first.
  url_request_->SetUserData(kUserDataKey,
                            std::make_unique<UnownedPointer>(this));
  // Configure the main request parameters. This must happen after setting
  // UserData, as `ConfigureUrlRequest` might internally retrieve data (e.g.,
  // PermissionsPolicy) via the `url_request_`'s UserData pointer.
  url_loader_util::ConfigureUrlRequest(request, *factory_params_,
                                       *origin_access_list_, *url_request_,
                                       shared_resource_checker);
  if (context.ShouldRequireIsolationInfo()) {
    DCHECK(!url_request_->isolation_info().IsEmpty());
  }
  SetUpUrlRequestCallbacks(shared_dictionary_manager);

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

void URLLoader::SetUpUrlRequestCallbacks(
    SharedDictionaryManager* shared_dictionary_manager) {
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

  if (shared_dictionary_manager) {
    url_request_->SetSharedDictionaryGetter(
        shared_dictionary_manager->MaybeCreateSharedDictionaryGetter(
            url_request_->load_flags(), request_destination_));
  }

  // Device bound session access can happen asynchronously as a result
  // of this URLRequest. So pass a refcounted shared Remote that will outlive
  // `this`.
  if (device_bound_session_observer_shared_remote_) {
    url_request_->SetDeviceBoundSessionAccessCallback(base::BindRepeating(
        [](scoped_refptr<RefCountedDeviceBoundSessionAccessObserverRemote>
               shared_remote,
           const net::device_bound_sessions::SessionAccess& access) {
          shared_remote.get()->data->OnDeviceBoundSessionAccessed(access);
        },
        device_bound_session_observer_shared_remote_));
  } else if (device_bound_session_observer_) {
    // This is just for the experiment to measure the impact on the
    // DeviceBoundSessionAccessObserverSharedRemote feature.
    // TODO(crbug.com/407680127): Remove this and when the feature is enabled
    // and the feature flag is removed.
    url_request_->SetDeviceBoundSessionAccessCallback(base::BindRepeating(
        &mojom::DeviceBoundSessionAccessObserver::OnDeviceBoundSessionAccessed,
        Clone(*device_bound_session_observer_)));
  }
}

void URLLoader::OpenFilesForUpload(const ResourceRequest& request) {
  std::vector<base::FilePath> paths;
  for (const auto& element : *request.request_body.get()->elements()) {
    if (element.type() == mojom::DataElementDataView::Tag::kFile) {
      paths.push_back(element.As<network::DataElementFile>().path());
    }
  }
  if (paths.empty()) {
    SetUpUpload(request, std::vector<base::File>());
    return;
  }
  if (!network_context_client_) {
    DLOG(ERROR) << "URLLoader couldn't upload a file because no "
                   "NetworkContextClient is set.";
    // Defer calling NotifyCompleted to make sure the URLLoader finishes
    // initializing before getting deleted.
    TaskRunner(url_request_->priority())
        ->PostTask(FROM_HERE, base::BindOnce(&URLLoader::NotifyCompleted,
                                             weak_ptr_factory_.GetWeakPtr(),
                                             net::ERR_ACCESS_DENIED));
    return;
  }
  url_request_->LogBlockedBy("Opening Files");
  file_opener_for_upload_ = std::make_unique<FileOpenerForUpload>(
      std::move(paths), url_request_->url(), factory_params_->process_id,
      network_context_client_,
      base::BindOnce(&URLLoader::SetUpUpload, base::Unretained(this), request));
  file_opener_for_upload_->Start();
}

void URLLoader::SetUpUpload(
    const ResourceRequest& request,
    base::expected<std::vector<base::File>, net::Error> file_open_result) {
  if (file_opener_for_upload_) {
    file_opener_for_upload_.reset();
    // This corresponds to the LogBlockedBy call made just before creating
    // FileOpenerForUpload.
    url_request_->LogUnblocked();
  }
  if (!file_open_result.has_value()) {
    // Defer calling NotifyCompleted to make sure the URLLoader finishes
    // initializing before getting deleted.
    TaskRunner(url_request_->priority())
        ->PostTask(FROM_HERE, base::BindOnce(&URLLoader::NotifyCompleted,
                                             weak_ptr_factory_.GetWeakPtr(),
                                             file_open_result.error()));
    return;
  }
  scoped_refptr<base::SequencedTaskRunner> task_runner =
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_VISIBLE});
  url_request_->set_upload(url_loader_util::CreateUploadDataStream(
      request.request_body.get(), file_open_result.value(), task_runner.get()));

  if (request.enable_upload_progress) {
    upload_progress_tracker_ = std::make_unique<UploadProgressTracker>(
        FROM_HERE,
        base::BindRepeating(&URLLoader::SendUploadProgress,
                            base::Unretained(this)),
        url_request_.get());
  }
  ProcessOutboundTrustTokenInterceptor(request);
}

void URLLoader::ProcessOutboundTrustTokenInterceptor(
    const ResourceRequest& request) {
  // If no Trust Token parameters are specified, proceed to the next
  // interceptor.
  if (!request.trust_token_params) {
    ProcessOutboundSharedStorageInterceptor();
    return;
  }
  // If trust_token_params exist, the interceptor MUST have been created in the
  // URLLoader constructor.
  CHECK(trust_token_interceptor_);

  // Ask the interceptor if any special load flags are needed.
  url_request_->SetLoadFlags(url_request_->load_flags() |
                             trust_token_interceptor_->GetAdditionalLoadFlags(
                                 request.trust_token_params.value()));

  // Delegate the Begin phase of the Trust Token operation to the interceptor.
  // The interceptor will asynchronously handle helper creation, calling Begin,
  // and determining the outcome.
  trust_token_interceptor_->BeginOperation(
      request.trust_token_params->operation, url_request_->url(),
      url_request_->isolation_info().top_frame_origin().value_or(url::Origin()),
      url_request_->extra_request_headers(), request.trust_token_params.value(),
      url_request_->net_log(),
      // Provide a getter for the TrustTokenAccessObserver.
      base::BindOnce(
          [](base::WeakPtr<URLLoader> weak_ptr)
              -> mojom::TrustTokenAccessObserver* {
            return weak_ptr ? weak_ptr->trust_token_observer_.get() : nullptr;
          },
          weak_ptr_factory_.GetWeakPtr()),
      // Provide a getter for the DevTools reporting callback.
      base::BindOnce(
          [](base::WeakPtr<URLLoader> weak_ptr)
              -> base::OnceCallback<void(mojom::TrustTokenOperationResultPtr)> {
            if (weak_ptr && weak_ptr->devtools_observer_.get() &&
                weak_ptr->devtools_request_id_) {
              return base::BindOnce(
                  &mojom::DevToolsObserver::OnTrustTokenOperationDone,
                  base::Unretained(weak_ptr->devtools_observer_.get()),
                  *weak_ptr->devtools_request_id_);
            }
            return base::OnceCallback<void(
                mojom::TrustTokenOperationResultPtr)>();
          },
          weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&URLLoader::OnDoneBeginningTrustTokenOperation,
                     weak_ptr_factory_.GetWeakPtr()));
}

void URLLoader::OnDoneBeginningTrustTokenOperation(
    base::expected<net::HttpRequestHeaders, net::Error> result) {
  // If `result` does not have a value, the operation failed or completed
  // locally.
  if (!result.has_value()) {
    // Defer calling NotifyCompleted to make sure the URLLoader
    // finishes initializing before getting deleted.
    TaskRunner(url_request_->priority())
        ->PostTask(FROM_HERE, base::BindOnce(&URLLoader::NotifyCompleted,
                                             weak_ptr_factory_.GetWeakPtr(),
                                             result.error()));
    return;
  }
  // Operation succeeded and returned headers to add/overwrite.
  // Apply the headers provided by the interceptor.
  for (const auto& header_pair : result->GetHeaderVector()) {
    url_request_->SetExtraRequestHeaderByName(
        header_pair.key, header_pair.value, /*overwrite=*/true);
  }
  // Trust Token outbound processing is done, proceed to the next interceptor.
  ProcessOutboundSharedStorageInterceptor();
}

void URLLoader::ProcessOutboundSharedStorageInterceptor() {
  DCHECK(shared_storage_request_helper_);
  shared_storage_request_helper_->ProcessOutgoingRequest(*url_request_);
  ScheduleStart();
}

void URLLoader::ScheduleStart() {
  TRACE_EVENT("loading", "URLLoader::ScheduleStart",
              net::NetLogWithSourceToFlow(url_request_->net_log()));
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
  TRACE_EVENT("loading", "URLLoader::~URLLoader",
              net::NetLogWithSourceToFlow(url_request_->net_log()));
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
    NOTREACHED();
  }

  // Note: There are some ordering dependencies here.
  // `CalculateStorageAccessStatus` depends on
  // `url_request->cookie_setting_overrides()`. `SetFetchMetadataHeaders`
  // depends on `url_request_->storage_access_status()`.
  if (request_credentials_mode_ == mojom::CredentialsMode::kInclude) {
    url_request_->set_storage_access_status(
        url_request_->CalculateStorageAccessStatus());
  } else {
    url_request_->reset_storage_access_status();
  }

  // We may need to clear out old Sec- prefixed request headers. We'll attempt
  // to do this before we re-add any.
  MaybeRemoveSecHeaders(*url_request_, *deferred_redirect_url_);
  SetFetchMetadataHeaders(*url_request_, request_mode_, has_user_activation_,
                          request_destination_, *deferred_redirect_url_,
                          *factory_params_, *origin_access_list_,
                          request_credentials_mode_);

  ResetRawHeadersForRedirect();

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
    cookies_from_browser_ = url_loader_util::GetCookiesFromHeaders(
        modified_headers, modified_cors_exempt_headers);
  }

  // Reset the state of the PNA checker - redirects should be treated like new
  // requests by the same client.
  private_network_access_interceptor_.ResetForRedirect(
      new_url ? *new_url : *deferred_redirect_url_);

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

int URLLoader::OnConnected(net::URLRequest* url_request,
                           const net::TransportInfo& info,
                           net::CompletionOnceCallback callback) {
  DCHECK_EQ(url_request, url_request_.get());

  // Delegate the PNA check to the interceptor.
  net::Error net_error = private_network_access_interceptor_.OnConnected(
      url_request_->url(), info,
      // Callback getter for async continuation:
      base::BindOnce(
          [](URLLoader* self, const net::TransportInfo* info_ptr,
             net::CompletionOnceCallback* callback)
              -> base::OnceCallback<void(net::Error)> {
            return base::BindOnce(
                &URLLoader::
                    ProcessLocalNetworkAccessPermissionResultOnConnected,
                // Safe to use Unretained, as this callback will be owned by the
                // interceptor which is owned by the URLLoader.
                base::Unretained(self), *info_ptr, std::move(*callback));
          },
          // Safe to use Unretained, as this callback must be called
          // synchronously.
          base::Unretained(this), base::Unretained(&info),
          base::Unretained(&callback)),
      // Callback to set CORS error status:
      base::BindOnce(
          [](URLLoader* self, CorsErrorStatus cors_error_status) {
            self->cors_error_status_ = std::move(cors_error_status);
          },
          // Safe to use Unretained, as this callback must be called
          // synchronously.
          base::Unretained(this)),
      url_request_->net_log(), devtools_observer_.get(), devtools_request_id(),
      url_loader_network_observer_.get());

  // If PNA failed synchronously or requires async LNA (ERR_IO_PENDING), return
  // now.
  if (net_error != net::OK) {
    return net_error;
  }

  // PNA passed synchronously. Proceed to Accept-CH handling.
  return ProcessAcceptCHFrameOnConnected(info, std::move(callback));
}

void URLLoader::ProcessLocalNetworkAccessPermissionResultOnConnected(
    const net::TransportInfo& info,
    net::CompletionOnceCallback callback,
    net::Error pna_result) {
  // If LNA permission was denied, complete the request with the error.
  if (pna_result != net::OK) {
    std::move(callback).Run(pna_result);
    return;
  }
  // LNA granted. Proceed to the next step: Accept-CH handling.

  std::pair<net::CompletionOnceCallback, net::CompletionOnceCallback> split =
      base::SplitOnceCallback(std::move(callback));
  int result = ProcessAcceptCHFrameOnConnected(info, std::move(split.first));
  if (result != net::ERR_IO_PENDING) {
    std::move(split.second).Run(result);
  }
}

int URLLoader::ProcessAcceptCHFrameOnConnected(
    const net::TransportInfo& info,
    net::CompletionOnceCallback callback) {
  TRACE_EVENT("loading", "URLLoader::ProcessAcceptCHFrameOnConnected",
              net::NetLogWithSourceToFlow(url_request_->net_log()), "url",
              url_request_->url());
  if (info.negotiated_protocol == net::NextProto::kProtoHTTP2) {
    base::UmaHistogramBoolean("Net.URLLoader.AcceptCHFrameReceivedOnHttp2",
                              !info.accept_ch_frame.empty());
  } else if (info.negotiated_protocol == net::NextProto::kProtoQUIC) {
    base::UmaHistogramBoolean("Net.URLLoader.AcceptCHFrameReceivedOnHttp3",
                              !info.accept_ch_frame.empty());
  }
  if (!accept_ch_frame_interceptor_) {
    return net::OK;
  }
  return accept_ch_frame_interceptor_->OnConnected(
      url_request_->url(), info.accept_ch_frame,
      url_request_->extra_request_headers(), std::move(callback));
}

mojom::URLResponseHeadPtr URLLoader::BuildResponseHead() const {
  CHECK(request_cookies_.empty() || include_request_cookies_with_response_);
  return url_loader_util::BuildResponseHead(
      *url_request_, request_cookies_,
      private_network_access_interceptor_.ClientAddressSpace(),
      private_network_access_interceptor_.ResponseAddressSpace().value_or(
          mojom::IPAddressSpace::kUnknown),
      options_, ShouldSetLoadWithStorageAccess(), is_load_timing_enabled_,
      include_load_timing_internal_info_with_response_,
      /*response_start=*/base::TimeTicks::Now(), devtools_observer_.get(),
      devtools_request_id().value_or(""));
}

void URLLoader::OnReceivedRedirect(net::URLRequest* url_request,
                                   const net::RedirectInfo& redirect_info,
                                   bool* defer_redirect) {
  DCHECK(url_request == url_request_.get());

  DCHECK(!deferred_redirect_url_);
  deferred_redirect_url_ = std::make_unique<GURL>(redirect_info.new_url);

  TRACE_EVENT("loading", "URLLoader::OnReceivedRedirect",
              net::NetLogWithSourceToFlow(url_request_->net_log()), "new_url",
              deferred_redirect_url_);

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
              coep_reporter_, document_isolation_policy, dip_reporter_)) {
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

  url_loader_util::SetRequestCredentials(
      redirect_info.new_url, factory_params_->client_security_state,
      request_mode_, request_credentials_mode_, url_request_->initiator(),
      *url_request_);

  // Clear the Cookie header to ensure that cookies passed in through the
  // `ResourceRequest` do not persist across redirects.
  url_request_.get()->RemoveRequestHeaderByName(
      net::HttpRequestHeaders::kCookie);
  cookies_from_browser_.clear();
  request_cookies_.clear();

  const url::Origin origin = url::Origin::Create(url_request_->url());
  const url::Origin pending_origin = url::Origin::Create(redirect_info.new_url);
  if (!origin.IsSameOriginWith(pending_origin)) {
    url_request_->cookie_setting_overrides().Remove(
        net::CookieSettingOverride::kStorageAccessGrantEligibleViaHeader);
    url_request_->cookie_setting_overrides().Remove(
        net::CookieSettingOverride::kStorageAccessGrantEligible);
  }

  DCHECK_EQ(emitted_devtools_raw_request_, emitted_devtools_raw_response_);
  response->emitted_extra_info = emitted_devtools_raw_request_;

  ad_auction_event_record_request_helper_.HandleResponse(
      *url_request_, GetPermissionsPolicy());

  ProcessInboundSharedStorageInterceptorOnReceivedRedirect(redirect_info,
                                                           std::move(response));
}

void URLLoader::ProcessInboundSharedStorageInterceptorOnReceivedRedirect(
    const net::RedirectInfo& redirect_info,
    mojom::URLResponseHeadPtr response) {
  DCHECK(shared_storage_request_helper_);

  auto split = base::SplitOnceCallback(base::BindOnce(
      &URLLoader::ContinueOnReceiveRedirect, weak_ptr_factory_.GetWeakPtr(),
      redirect_info, std::move(response)));
  if (!shared_storage_request_helper_->ProcessIncomingResponse(
          *url_request_, std::move(split.first))) {
    std::move(split.second).Run();
  }
}

void URLLoader::ContinueOnReceiveRedirect(
    const net::RedirectInfo& redirect_info,
    mojom::URLResponseHeadPtr response) {
  DCHECK(response);
  url_loader_client_.Get()->OnReceiveRedirect(redirect_info,
                                              std::move(response));
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

void URLLoader::OnResponseStarted(net::URLRequest* url_request, int net_error) {
  DCHECK(url_request == url_request_.get());
  has_received_response_ = true;

  if (keepalive_) {
    base::UmaHistogramEnumeration(
        "FetchKeepAlive.Requests2.Network",
        internal::FetchKeepAliveRequestNetworkMetricType::kOnResponse);
  }

  // Wait to report for main frame navigations. This is because handling the
  // cookie notification contends with ReadyToCommitNavigation, which is in the
  // critical path for loading. Additionally, cookie observers for navigations
  // now outlive the NavigationRequest.
  bool delay_cookie_call =
      url_request_->isolation_info().IsMainFrameRequest() &&
      base::FeatureList::IsEnabled(kDelayedCookieNotification);
  ReportFlaggedResponseCookies(!delay_cookie_call);

  if (net_error != net::OK) {
    NotifyCompleted(net_error);
    // |this| may have been deleted.
    return;
  }

  response_ = BuildResponseHead();
  DispatchOnRawResponse();

  ad_auction_event_record_request_helper_.HandleResponse(
      *url_request_, GetPermissionsPolicy());

  // Parse and remove the Trust Tokens response headers, if any are expected,
  // potentially failing the request if an error occurs.
  if (response_ && response_->headers && trust_token_interceptor_) {
    DCHECK(response_);
    trust_token_interceptor_->FinalizeOperation(
        *response_->headers.get(),
        base::BindOnce(&URLLoader::OnDoneFinalizingTrustTokenOperation,
                       weak_ptr_factory_.GetWeakPtr()));
    // |this| may have been deleted.
    return;
  }

  ProcessInboundSharedStorageInterceptorOnResponseStarted();
}

void URLLoader::OnDoneFinalizingTrustTokenOperation(net::Error error) {
  if (error != net::OK) {
    NotifyCompleted(error);
    // |this| may have been deleted.
    return;
  }
  ProcessInboundSharedStorageInterceptorOnResponseStarted();
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
    options.capacity_num_bytes = GetDataPipeDefaultAllocationSize(
        DataPipeAllocationSize::kLargerSizeIfPossible);
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
              coep_reporter_, document_isolation_policy, dip_reporter_)) {
    CompleteBlockedResponse(net::ERR_BLOCKED_BY_RESPONSE, false,
                            blocked_reason);
    // Close the socket associated with the request, to prevent leaking
    // information.
    url_request_->AbortAndCloseConnection();
    DeleteSelf();
    return;
  }

  // Enforce SRI-compliant HTTP Message Signature headers.
  //
  // https://wicg.github.io/signature-based-sri/
  if (std::optional<mojom::BlockedByResponseReason> blocked_reason =
          MaybeBlockResponseForSRIMessageSignature(
              *url_request_, *response_, expected_public_keys_,
              devtools_observer_.get(), devtools_request_id().value_or(""))) {
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
  // TODO(crbug.com/40269364): Remove old names once API users have migrated to
  // new names.
  if (!factory_params_->is_trusted && response_->headers) {
    std::optional<std::string> auction_only =
        response_->headers->GetNormalizedHeader("Ad-Auction-Only");
    if (!auction_only) {
      auction_only =
          response_->headers->GetNormalizedHeader("X-FLEDGE-Auction-Only");
    }
    if (auction_only &&
        base::EqualsCaseInsensitiveASCII(*auction_only, "true")) {
      CompleteBlockedResponse(net::ERR_BLOCKED_BY_RESPONSE, false);
      url_request_->AbortAndCloseConnection();
      DeleteSelf();
      return;
    }
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
      mime_type_before_sniffing_ = response_->mime_type;
    } else if (response_->mime_type.empty()) {
      // Ugg.  The server told us not to sniff the content but didn't give us
      // a mime type.  What's a browser to do?  Turns out, we're supposed to
      // treat the response as "text/plain".  This is the most secure option.
      response_->mime_type.assign("text/plain");
    }
  }

  // If client-side content decoding is requested, store the types of decoding
  // to be used with the Durable Message so it can decode on retrieval.
  if (devtools_durable_message_) {
    devtools_durable_message_->set_client_decoding_types(
        response_->client_side_content_decoding_types);
  }

  // If client-side content decoding is requested and either ORB or MIME
  // sniffing is needed, use PartialDecoder to get decoded data for sniffing.
  if (!response_->client_side_content_decoding_types.empty() &&
      (is_more_orb_sniffing_needed_ || is_more_mime_sniffing_needed_)) {
    // Create a PartialDecoder to decode the response body up to a certain limit
    // for sniffing purposes.
    partial_decoder_ = std::make_unique<PartialDecoder>(
        base::BindRepeating(
            [](base::WeakPtr<net::URLRequest> url_request, net::IOBuffer* dest,
               int dest_size) {
              CHECK(url_request);
              return url_request->Read(dest, dest_size);
            },
            url_request_->GetWeakPtr()),
        response_->client_side_content_decoding_types,
        partial_decoder_decoding_buffer_size_);
    // Start reading decoded data from the partial decoder.
    ReadDecodedDataFromPartialDecoder();
    return;
  }

  StartReading();
}

void URLLoader::ReadDecodedDataFromPartialDecoder() {
  // Keep reading from the partial decoder until it signals pending or
  // completes.
  while (partial_decoder_) {
    // Attempt to read more decoded data.
    int result = partial_decoder_->ReadDecodedDataMore(
        base::BindOnce(&URLLoader::OnReadDecodedDataFromPartialDecoder,
                       base::Unretained(this)));
    if (result == net::ERR_IO_PENDING) {
      // If the read is pending, return and wait for the callback.
      return;
    }
    // Process the result immediately.
    CheckPartialDecoderResult(result);
    if (partial_decoder_result_) {
      // Sniffing is complete, and we have the raw data. Stop using the
      // partial decoder and proceed to read the rest of the response.
      StartReading();
      return;
    }
  }
}

void URLLoader::OnReadDecodedDataFromPartialDecoder(int result) {
  // This is the callback from `PartialDecoder::ReadDecodedDataMore`.
  // Process the result of the partial decode.
  CheckPartialDecoderResult(result);
  if (partial_decoder_result_) {
    // Sniffing is complete. Proceed to read the full response.
    StartReading();
  } else if (partial_decoder_) {
    // More sniffing might be needed, continue reading from the partial decoder.
    ReadDecodedDataFromPartialDecoder();
  }
}

void URLLoader::CheckPartialDecoderResult(int result) {
  if (result < 0) {
    // The partial decoder failed. Stop decoding and report the error.
    partial_decoder_.reset();
    // Defer calling NotifyCompleted to make sure the caller can still access
    // |this|.
    TaskRunner(url_request_->priority())
        ->PostTask(FROM_HERE,
                   base::BindOnce(&URLLoader::NotifyCompleted,
                                  weak_ptr_factory_.GetWeakPtr(), result));
    return;
  }
  // Check if we should stop sniffing after processing the current chunk of
  // decoded data. This happens if the decoder returns 0 (end of stream) or
  // if the decoded buffer is full.
  const bool stop_sniffing_after_processing_current_data =
      (result == 0 || !partial_decoder_->HasRemainingBuffer());

  // Get a view of the decoded data, limited to the maximum sniffing size.
  // Capping the size to net::kMaxBytesToSniff for tests which have called
  // set_partial_decoder_decoding_buffer_size_for_testing().
  const std::string_view decoded_data_to_sniff =
      base::as_string_view(partial_decoder_->decoded_data())
          .substr(0, net::kMaxBytesToSniff);

  if (is_more_mime_sniffing_needed_) {
    // Perform MIME sniffing using the decoded data.
    CHECK(mime_type_before_sniffing_.has_value());
    std::string new_type;
    is_more_mime_sniffing_needed_ = !net::SniffMimeType(
        decoded_data_to_sniff, url_request_->url(),
        mime_type_before_sniffing_.value(),
        net::ForceSniffFileUrlsForHtml::kDisabled, &new_type);
    response_->mime_type = std::move(new_type);
    response_->did_mime_sniff = true;

    if (stop_sniffing_after_processing_current_data) {
      is_more_mime_sniffing_needed_ = false;
    }
  }

  if (is_more_orb_sniffing_needed_) {
    // Perform ORB sniffing using the decoded data.
    orb::ResponseAnalyzer::Decision orb_decision =
        result == 0 ? orb::ResponseAnalyzer::Decision::kSniffMore
                    : orb_analyzer_->Sniff(decoded_data_to_sniff);
    if (orb_decision == orb::ResponseAnalyzer::Decision::kSniffMore &&
        stop_sniffing_after_processing_current_data) {
      // If more sniffing was requested but we've reached the limit, get the
      // final decision for the sniffed data.
      orb_decision = orb_analyzer_->HandleEndOfSniffableResponseBody();
      DCHECK_NE(orb::ResponseAnalyzer::Decision::kSniffMore, orb_decision);
    }
    if (MaybeBlockResponseForOrb(orb_decision)) {
      partial_decoder_.reset();
      return;
    }
  }
  // If no more sniffing is needed, retrieve the accumulated raw data from the
  // partial decoder.
  if (!is_more_orb_sniffing_needed_ && !is_more_mime_sniffing_needed_) {
    partial_decoder_result_ = std::move(*partial_decoder_).TakeResult();
  }
}

void URLLoader::ReadMore() {
  CHECK_NE(url_read_state_, URLReadState::kURLReadInProgress);
  url_read_state_ = URLReadState::kWaitMojoPipeWritable;

  if (partial_decoder_result_) {
    // If we have buffered raw data from the partial decoder, send that first.
    while (partial_decoder_result_->HasRawData()) {
      MojoResult result = NetToMojoPendingBuffer::BeginWrite(
          &response_body_stream_, &pending_write_);
      switch (result) {
        case MOJO_RESULT_OK:
          break;
        case MOJO_RESULT_SHOULD_WAIT:
          // The Mojo data pipe is full. Wait for it to become writable.
          // While a SlopBucket would be ideal when enabled, it's omitted here
          // for simplicity.
          //
          // This scenario occurs when the PartialDecoder reads raw data
          // exceeding the Mojo data pipe's capacity. However, since the
          // PartialDecoder only reads decompressed data up to
          // net::kMaxBytesToSniff, this situation is extremely rare in
          // real-world scenarios.
          writable_handle_watcher_.ArmOrNotify();
          return;
        default:
          // The response body stream is in a bad state. This happens when the
          // consumer handle of the body was synchronously released in
          // URLLoaderClient::OnReceiveResponse().
          NotifyCompleted(net::ERR_FAILED);
          return;
      }
      // Copy data from the raw buffer into the Mojo pipe buffer.
      pending_write_buffer_offset_ = partial_decoder_result_->ConsumeRawData(
          base::as_writable_byte_span(*pending_write_));
      CHECK(pending_write_buffer_offset_);
      MaybeCollectDurableMessage(0, pending_write_buffer_offset_);
      CompletePendingWrite(true);
    }
    // Check if the partial decoder finished with a specific status.
    if (partial_decoder_result_->completion_status()) {
      NotifyCompleted(*partial_decoder_result_->completion_status());
      return;
    }
    partial_decoder_result_.reset();
  }

  // Once the MIME type is sniffed, all data is sent as soon as it is read from
  // the network.
  DCHECK(consumer_handle_.is_valid() || !pending_write_);

  // TODO(ricea): Refactor this method and DidRead() to reduce duplication.
  if (options_ & mojom::kURLLoadOptionReadAndDiscardBody) {
    url_read_state_ = URLReadState::kURLReadInProgress;
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
    mojo_begin_write_count_for_uma_++;
    switch (result) {
      case MOJO_RESULT_OK:
        break;
      case MOJO_RESULT_SHOULD_WAIT: {
        CHECK(!pending_write_);
        bool should_wait = true;
        if (base::FeatureList::IsEnabled(kSlopBucket) && !slop_bucket_) {
          slop_bucket_ = SlopBucket::RequestSlopBucket(url_request_.get());
          was_slop_bucket_enabled_ = true;
        }
        if (slop_bucket_ && !slop_bucket_->read_in_progress() &&
            !slop_bucket_->IsComplete()) {
          // Read into the slop bucket while we're waiting for the mojo data
          // pipe to empty out.
          std::optional<int> bytes_read_maybe = slop_bucket_->AttemptRead();
          if (bytes_read_maybe.has_value()) {
            url_read_state_ = URLReadState::kURLReadInProgress;
            should_wait = false;
            int bytes_read = bytes_read_maybe.value();
            if (bytes_read != net::ERR_IO_PENDING) {
              // DidRead() will not delete `this` when `into_slop_bucket` is
              // true, so it is safe to access member variables after this call.
              DidRead(bytes_read, /*completed_synchronously=*/true,
                      /*into_slop_bucket=*/true);
            }
          }
        }  // The pipe is full. We need to wait for it to have more space.
        if (should_wait) {
          mojo_blocked_write_count_for_uma_++;
        }
        writable_handle_watcher_.ArmOrNotify();
        return;
      }
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
      const size_t consumed =
          slop_bucket_->Consume(base::as_writable_byte_span(*pending_write_));
      if (consumed) {
        // TODO(ricea): Refactor the way pending writes work so we don't need to
        // poke a value into `pending_write_buffer_offset_` here.
        pending_write_buffer_offset_ = consumed;
        CompletePendingWrite(true);
        ReadMoreAsync();
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
  url_read_state_ = URLReadState::kURLReadInProgress;
  int bytes_read = url_request_->Read(
      buf.get(), static_cast<int>(pending_write_buffer_size_ -
                                  pending_write_buffer_offset_));
  if (bytes_read != net::ERR_IO_PENDING) {
    DidRead(bytes_read, /*completed_synchronously=*/true,
            /*into_slop_bucket=*/false);
    // |this| may have been deleted.
  }
}

void URLLoader::ReadMoreAsync() {
  CHECK_EQ(url_read_state_, URLReadState::kWaitMojoPipeWritable);
  url_read_state_ = URLReadState::kReadMoreTaskPosted;
  TaskRunner(url_request_->priority())
      ->PostTask(FROM_HERE, base::BindOnce(&URLLoader::ReadMore,
                                           weak_ptr_factory_.GetWeakPtr()));
}

// Handles the completion of a read. `num_bytes` is the number of bytes read, 0
// if we reached the end of the response body, or a net::Error otherwise.
// `completed_synchronously` is true if the call to URLRequest::Read did not
// return net::ERR_IO_PENDING. `into_slop_bucket` is true if this was actually a
// read into `slop_bucket_` and not into our own buffer.
void URLLoader::DidRead(int num_bytes,
                        bool completed_synchronously,
                        bool into_slop_bucket) {
  CHECK_EQ(url_read_state_, URLReadState::kURLReadInProgress);
  url_read_state_ = URLReadState::kWaitMojoPipeWritable;

  size_t new_data_offset = pending_write_buffer_offset_;
  MaybeCollectDurableMessage(new_data_offset, num_bytes);

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
        CHECK(mime_type_before_sniffing_.has_value());
        std::string new_type;
        is_more_mime_sniffing_needed_ = !net::SniffMimeType(
            data, url_request_->url(), mime_type_before_sniffing_.value(),
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
    ReadMoreAsync();
  } else {
    ReadMore();
  }
}

void URLLoader::OnReadCompleted(net::URLRequest* url_request, int bytes_read) {
  DCHECK(url_request == url_request_.get());
  if (partial_decoder_ && partial_decoder_->read_in_progress()) {
    // When `partial_decoder_` is reading the body from `url_request`, pass the
    // result to `partial_decoder_`.
    partial_decoder_->OnReadRawDataCompleted(bytes_read);
    return;
  }

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
    TaskRunner(url_request_->priority())
        ->PostTask(FROM_HERE,
                   base::BindOnce(std::move(callback), net::OK,
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
    std::optional<GURL>* preserve_fragment_on_redirect_url,
    const std::optional<net::SSLInfo>& ssl_info) {
  if (header_client_) {
    header_client_->OnHeadersReceived(
        original_response_headers->raw_headers(), endpoint, ssl_info,
        base::BindOnce(&URLLoader::OnHeadersReceivedComplete,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                       override_response_headers,
                       preserve_fragment_on_redirect_url));
    return net::ERR_IO_PENDING;
  }
  return net::OK;
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
  auto key = base::MakeRefCounted<SSLPrivateKeyProxy>(
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
    mojo_begin_write_count_for_uma_ =
        std::max(mojo_begin_write_count_for_uma_, 1);
    const int proportion = mojo_blocked_write_count_for_uma_ * 100 /
                           mojo_begin_write_count_for_uma_;
    base::UmaHistogramPercentage(
        "Net.URLLoader.ProportionOfWritesBlockedByMojo", proportion);
    if (was_slop_bucket_enabled_) {
      base::UmaHistogramPercentage(
          "Net.URLLoader.SlopBucket.ProportionOfWritesBlockedByMojo.All",
          proportion);

      using CacheEntryStatus = net::HttpResponseInfo::CacheEntryStatus;
      const CacheEntryStatus& cache_entry_status =
          url_request_->response_info().cache_entry_status;
      if (cache_entry_status != CacheEntryStatus::ENTRY_USED &&
          cache_entry_status != CacheEntryStatus::ENTRY_VALIDATED) {
        base::UmaHistogramPercentage(
            "Net.URLLoader.SlopBucket.ProportionOfWritesBlockedByMojo.NoCache",
            proportion);
      }
    }
  }

  if (total_sent > 0) {
    UMA_HISTOGRAM_COUNTS_1M("DataUse.BytesSent3.Delegate", total_sent);
  }

  url_loader_util::MaybeRecordSharedDictionaryUsedResponseMetrics(
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
    if (trust_token_interceptor_ && trust_token_interceptor_->status()) {
      status.trust_token_operation_status = *trust_token_interceptor_->status();
    }
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

  if (url_read_state_ == URLReadState::kWaitMojoPipeWritable) {
    ReadMore();
  }
}

void URLLoader::DeleteSelf() {
  std::move(delete_callback_).Run(this);
}

void URLLoader::SendResponseToClient() {
  TRACE_EVENT("loading", "network::URLLoader::SendResponseToClient",
              net::NetLogWithSourceToFlow(url_request_->net_log()), "url",
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
  // If we have emitted_devtools_raw_request_, don't notify DevTools
  // to prevent duplicate ExtraInfo events.
  if (!emitted_devtools_raw_request_ && devtools_observer_ &&
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
          url_request_->isolation_info().frame_origin(),
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
          url_request_->isolation_info(), url_request_->cookie_partition_key());
  return shared_dictionary_allowed_check_passed_;
}

void URLLoader::DispatchOnRawRequest(
    std::vector<network::mojom::HttpRawHeaderPairPtr> headers) {
  DCHECK(devtools_observer_ && devtools_request_id());

  net::LoadTimingInfo load_timing_info;
  url_request_->GetLoadTimingInfo(&load_timing_info);

  emitted_devtools_raw_request_ = true;

  bool is_main_frame_navigation =
      url_request_->isolation_info().IsMainFrameRequest() ||
      url_request_->force_main_frame_for_same_site_cookies();
  net::SchemefulSite request_site(url_request_->url());
  // TODO when crbug.com/40093296 "Don't trust |site_for_cookies| provided by
  // the renderer" is fixed. Update the FromNetworkIsolationKey method to use
  // url_request_->isolation_info().site_for_cookies() instead of
  // url_request_->site_for_cookies().
  std::optional<net::CookiePartitionKey> partition_key =
      net::CookiePartitionKey::FromNetworkIsolationKey(
          url_request_->isolation_info().network_isolation_key(),
          url_request_->site_for_cookies(), request_site,
          is_main_frame_navigation);
  std::optional<bool> site_has_cookie_in_other_partition;
  if (partition_key.has_value()) {
    site_has_cookie_in_other_partition =
        url_request_->context()->cookie_store()->SiteHasCookieInOtherPartition(
            request_site, partition_key.value());
  }
  network::mojom::OtherPartitionInfoPtr other_partition_info = nullptr;
  if (site_has_cookie_in_other_partition.has_value()) {
    other_partition_info = network::mojom::OtherPartitionInfo::New();
    other_partition_info->site_has_cookie_in_other_partition =
        *site_has_cookie_in_other_partition;
  }

  std::optional<base::UnguessableToken> applied_network_conditions_id;
  if (throttling_token_) {
    ThrottlingNetworkInterceptor* interceptor =
        ThrottlingController::GetInterceptor(throttling_token_->source_id(),
                                             url_request_->url());
    if (interceptor) {
      applied_network_conditions_id = interceptor->conditions().rule_id();
    }
  }

  devtools_observer_->OnRawRequest(
      devtools_request_id().value(), url_request_->maybe_sent_cookies(),
      std::move(headers), load_timing_info.request_start,
      private_network_access_interceptor_.CloneClientSecurityState(),
      std::move(other_partition_info),
      std::move(applied_network_conditions_id));
}

void URLLoader::DispatchOnRawResponse() {
  if (!emitted_devtools_raw_request_) {
    // TODO(ortuno): not sure why emitting of metrics is gated upon request not
    // having been dispatched to DevTools, but this has been so since it raw
    // header size metrics have been introduced by https://crrev.com/c/5824030.
    if (url_request_->response_headers()) {
      // Record request metrics here instead of in NotifyCompleted to account
      // for redirects.
      url_loader_util::RecordURLLoaderRequestMetrics(
          *url_request_, raw_request_line_size_, raw_request_headers_size_);
    }
    // If there were no raw request headers, we assume no raw response headers
    // either, to make client logic simpler.
    // TODO(caseq): ensure this is actually an invariant?
    return;
  }

  // Per `if (emitted_devtools_raw_request_)` above.
  CHECK(devtools_observer_);
  CHECK(devtools_request_id());

  if (!url_request_->response_headers()) {
    return;
  }

  const net::HttpResponseHeaders* response_headers =
      raw_response_headers_ ? raw_response_headers_.get()
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

  emitted_devtools_raw_response_ = true;
  devtools_observer_->OnRawResponse(
      devtools_request_id().value(), url_request_->maybe_stored_cookies(),
      std::move(header_array), raw_response_headers,
      private_network_access_interceptor_.ResponseAddressSpace().value_or(
          mojom::IPAddressSpace::kUnknown),
      response_headers->response_code(), url_request_->cookie_partition_key());
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
      TaskRunner(url_request_->priority())
          ->PostTask(FROM_HERE,
                     base::BindOnce(&URLLoader::NotifyCompleted,
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
  TaskRunner(url_request_->priority())
      ->PostTask(FROM_HERE, base::BindOnce(&URLLoader::DeleteSelf,
                                           weak_ptr_factory_.GetWeakPtr()));
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
        url_request_->isolation_info().frame_origin(),
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
    if (!url_request_->storage_access_status().IsSet()) {
      url_request_->set_storage_access_status(
          url_request_->CalculateStorageAccessStatus());
    }
    if (!url_request_->storage_access_status()
             .GetStatusForThirdPartyContext()) {
      return net::cookie_util::ActivateStorageAccessLoadOutcome::
          kFailureInvalidStatus;
    }
    switch (url_request_->storage_access_status()
                .GetStatusForThirdPartyContext()
                .value()) {
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

const mojom::ClientSecurityState* URLLoader::GetClientSecurityState() {
  return url_loader_util::SelectClientSecurityState(
      factory_params_->client_security_state.get(),
      client_security_state_.get());
}

void URLLoader::ResetRawHeadersForRedirect() {
  emitted_devtools_raw_request_ = false;
  emitted_devtools_raw_response_ = false;
  raw_request_line_size_ = 0;
  raw_request_headers_size_ = 0;
  raw_response_headers_ = nullptr;
}

void URLLoader::MaybeCollectDurableMessage(size_t new_data_offset,
                                           int num_bytes) {
  if (!pending_write_ || !devtools_durable_message_) {
    return;
  }

  if (num_bytes <= 0) {
    devtools_durable_message_->MarkComplete();
    return;
  }

  int64_t raw_bytes_cur_size = url_request_->GetRawBodyBytes();
  int64_t raw_bytes_delta =
      raw_bytes_cur_size - devtools_durable_message_raw_size_;
  devtools_durable_message_->AddBytes(
      base::as_byte_span(
          base::span(*pending_write_)
              .subspan(new_data_offset, static_cast<size_t>(num_bytes))),
      raw_bytes_delta);
  devtools_durable_message_raw_size_ = raw_bytes_cur_size;
}

}  // namespace network
