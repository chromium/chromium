// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_package/prefetched_signed_exchange_cache.h"

#include <optional>
#include <string_view>
#include <utility>

#include "base/base64.h"
#include "base/feature_list.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/observer_list.h"
#include "base/strings/string_util.h"
#include "components/link_header_util/link_header_util.h"
#include "content/browser/loader/cross_origin_read_blocking_checker.h"
#include "content/browser/loader/navigation_loader_interceptor.h"
#include "content/browser/loader/response_head_update_params.h"
#include "content/browser/navigation_subresource_loader_params.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/storage_partition_impl.h"
#include "content/browser/web_package/signed_exchange_inner_response_url_loader.h"
#include "content/browser/web_package/signed_exchange_reporter.h"
#include "content/browser/web_package/signed_exchange_utils.h"
#include "content/browser/web_package/subresource_signed_exchange_url_loader_factory.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_features.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "net/base/network_anonymization_key.h"
#include "net/cookies/cookie_setting_override.h"
#include "net/http/http_cache.h"
#include "net/http/http_util.h"
#include "net/url_request/redirect_util.h"
#include "services/network/public/cpp/constants.h"
#include "services/network/public/cpp/cors/cors.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/initiator_lock_compatibility.h"
#include "services/network/public/cpp/orb/orb_api.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/single_request_url_loader_factory.h"
#include "services/network/public/cpp/url_loader_completion_status.h"
#include "services/network/public/cpp/wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/fetch_api.mojom-shared.h"
#include "services/network/public/mojom/restricted_cookie_manager.mojom.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "storage/browser/blob/blob_data_handle.h"
#include "storage/browser/blob/blob_impl.h"
#include "storage/browser/blob/mojo_blob_reader.h"

namespace content {

namespace {

// The max number of cached entry per one frame. This limit is intended to
// prevent OOM crash of the browser process.
constexpr size_t kMaxEntrySize = 100u;

// A URLLoader which returns a synthesized redirect response for signed
// exchange's outer URL request.
class RedirectResponseURLLoader : public network::mojom::URLLoader {
 public:
  RedirectResponseURLLoader(
      const network::ResourceRequest& url_request,
      const GURL& inner_url,
      const network::mojom::URLResponseHead& outer_response,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client)
      : client_(std::move(client)) {
    auto response_head = signed_exchange_utils::CreateRedirectResponseHead(
        outer_response, false /* is_fallback_redirect */);
    response_head->was_fetched_via_cache = true;
    response_head->was_in_prefetch_cache = true;
    SignedExchangeInnerResponseURLLoader::UpdateRequestResponseStartTime(
        response_head.get());
    client_->OnReceiveRedirect(signed_exchange_utils::CreateRedirectInfo(
                                   inner_url, url_request, outer_response,
                                   false /* is_fallback_redirect */),
                               std::move(response_head));
  }

  RedirectResponseURLLoader(const RedirectResponseURLLoader&) = delete;
  RedirectResponseURLLoader& operator=(const RedirectResponseURLLoader&) =
      delete;

  ~RedirectResponseURLLoader() override {}

 private:
  // network::mojom::URLLoader overrides:
  void FollowRedirect(
      const std::vector<std::string>& removed_headers,
      const net::HttpRequestHeaders& modified_headers,
      const net::HttpRequestHeaders& modified_cors_exempt_headers,
      const std::optional<GURL>& new_url) override {
    NOTREACHED_IN_MIGRATION();
  }
  void SetPriority(net::RequestPriority priority,
                   int intra_priority_value) override {
    // There is nothing to do, because this class just calls OnReceiveRedirect.
  }
  void PauseReadingBodyFromNet() override {
    // There is nothing to do, because we don't fetch the resource from the
    // network.
  }
  void ResumeReadingBodyFromNet() override {
    // There is nothing to do, because we don't fetch the resource from the
    // network.
  }

  mojo::Remote<network::mojom::URLLoaderClient> client_;
};

// A NavigationLoaderInterceptor which handles a request which matches the
// prefetched signed exchange that has been stored to a
// PrefetchedSignedExchangeCache.
class PrefetchedNavigationLoaderInterceptor
    : public NavigationLoaderInterceptor {
 public:
  PrefetchedNavigationLoaderInterceptor(
      std::unique_ptr<const PrefetchedSignedExchangeCacheEntry> exchange,
      std::vector<blink::mojom::PrefetchedSignedExchangeInfoPtr> info_list,
      mojo::Remote<network::mojom::RestrictedCookieManager> cookie_manager)
      : exchange_(std::move(exchange)),
        info_list_(std::move(info_list)),
        cookie_manager_(std::move(cookie_manager)) {}

  PrefetchedNavigationLoaderInterceptor(
      const PrefetchedNavigationLoaderInterceptor&) = delete;
  PrefetchedNavigationLoaderInterceptor& operator=(
      const PrefetchedNavigationLoaderInterceptor&) = delete;

  ~PrefetchedNavigationLoaderInterceptor() override {}

  void MaybeCreateLoader(
      const network::ResourceRequest& tentative_resource_request,
      BrowserContext* browser_context,
      LoaderCallback callback,
      FallbackCallback fallback_callback) override {
    if (state_ == State::kInitial &&
        tentative_resource_request.url == exchange_->outer_url()) {
      state_ = State::kOuterRequestRequested;
      std::move(callback).Run(NavigationLoaderInterceptor::Result(
          base::MakeRefCounted<network::SingleRequestURLLoaderFactory>(
              base::BindOnce(
                  &PrefetchedNavigationLoaderInterceptor::StartRedirectResponse,
                  weak_factory_.GetWeakPtr())),
          /*subresource_loader_params=*/{}));
      return;
    }
    if (tentative_resource_request.url == exchange_->inner_url()) {
      DCHECK_EQ(State::kOuterRequestRequested, state_);
      if (signed_exchange_utils::IsCookielessOnlyExchange(
              *exchange_->inner_response()->headers)) {
        DCHECK(cookie_manager_);
        state_ = State::kCheckingCookies;
        CheckAbsenceOfCookies(tentative_resource_request, std::move(callback));
        return;
      } else {
        state_ = State::kInnerResponseRequested;
        SubresourceLoaderParams params;
        params.prefetched_signed_exchanges = std::move(info_list_);
        std::move(callback).Run(NavigationLoaderInterceptor::Result(
            base::MakeRefCounted<network::SingleRequestURLLoaderFactory>(
                base::BindOnce(
                    &PrefetchedNavigationLoaderInterceptor::StartInnerResponse,
                    weak_factory_.GetWeakPtr())),
            std::move(params)));
        return;
      }
    }
    DUMP_WILL_BE_NOTREACHED();
  }

 private:
  enum class State {
    kInitial,
    kOuterRequestRequested,
    kCheckingCookies,
    kInnerResponseRequested
  };

  void CheckAbsenceOfCookies(const network::ResourceRequest& request,
                             LoaderCallback callback) {
    auto match_options = network::mojom::CookieManagerGetOptions::New();
    match_options->name = "";
    match_options->match_type = network::mojom::CookieMatchType::STARTS_WITH;
    cookie_manager_->GetAllForUrl(
        request.url, request.trusted_params->isolation_info.site_for_cookies(),
        *request.trusted_params->isolation_info.top_frame_origin(),
        request.storage_access_api_status, std::move(match_options),
        request.is_ad_tagged,
        /*force_disable_third_party_cookies=*/false,
        base::BindOnce(&PrefetchedNavigationLoaderInterceptor::OnGetCookies,
                       weak_factory_.GetWeakPtr(), std::move(callback)));
  }

  void OnGetCookies(LoaderCallback callback,
                    const std::vector<net::CookieWithAccessResult>& results) {
    DCHECK_EQ(State::kCheckingCookies, state_);
    if (!results.empty()) {
      signed_exchange_utils::RecordLoadResultHistogram(
          SignedExchangeLoadResult::kHadCookieForCookielessOnlySXG);

      ResponseHeadUpdateParams head_update_params;
      head_update_params.load_timing_info =
          this->exchange_->outer_response()->load_timing;
      // TODO(crbug.com/40266535) test workerStart in SXG scenarios
      std::move(callback).Run(NavigationLoaderInterceptor::Result(
          /*factory=*/nullptr, /*subresource_loader_params=*/{},
          std::move(head_update_params)));
      return;
    }
    state_ = State::kInnerResponseRequested;
    SubresourceLoaderParams params;
    params.prefetched_signed_exchanges = std::move(info_list_);
    std::move(callback).Run(NavigationLoaderInterceptor::Result(
        base::MakeRefCounted<network::SingleRequestURLLoaderFactory>(
            base::BindOnce(
                &PrefetchedNavigationLoaderInterceptor::StartInnerResponse,
                weak_factory_.GetWeakPtr())),
        std::move(params)));
  }

  void StartRedirectResponse(
      const network::ResourceRequest& resource_request,
      mojo::PendingReceiver<network::mojom::URLLoader> receiver,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client) {
    mojo::MakeSelfOwnedReceiver(
        std::make_unique<RedirectResponseURLLoader>(
            resource_request, exchange_->inner_url(),
            *exchange_->outer_response(), std::move(client)),
        std::move(receiver));
  }

  void StartInnerResponse(
      const network::ResourceRequest& resource_request,
      mojo::PendingReceiver<network::mojom::URLLoader> receiver,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client) {
    // `resource_request.request_initiator()` is trustworthy, because:
    // 1) StartInnerResponse is only used from the navigation stack (via
    //    MaybeCreateLoader override of NavigationLoaderInterceptor)
    // 2) navigation initiator is validated in IPCs from the renderer (e.g. see
    //    VerifyBeginNavigationCommonParams).
    // Note that `request_initiator_origin_lock` below might be different from
    // `url::Origin::Create(exchange_->inner_url())` - for example in the
    // All/SignedExchangeRequestHandlerBrowserTest.Simple/3 testcase.
    CHECK_EQ(network::mojom::RequestMode::kNavigate, resource_request.mode);

    // PrefetchedNavigationLoaderInterceptor is only created for
    // renderer-initiated navigations - therefore `request_initiator` is
    // guaranteed to have a value here.
    CHECK(resource_request.request_initiator.has_value());

    // Okay to use separate/empty ORB state for each navigation request.
    // (Because ORB doesn't apply to navigation requests.)
    auto empty_orb_state = base::MakeRefCounted<
        base::RefCountedData<network::orb::PerFactoryState>>();

    mojo::MakeSelfOwnedReceiver(
        std::make_unique<SignedExchangeInnerResponseURLLoader>(
            resource_request, exchange_->inner_response().Clone(),
            std::make_unique<const storage::BlobDataHandle>(
                *exchange_->blob_data_handle()),
            *exchange_->completion_status(), std::move(client),
            true /* is_navigation_request */, std::move(empty_orb_state)),
        std::move(receiver));
  }

  State state_ = State::kInitial;
  const std::unique_ptr<const PrefetchedSignedExchangeCacheEntry> exchange_;
  std::vector<blink::mojom::PrefetchedSignedExchangeInfoPtr> info_list_;
  mojo::Remote<network::mojom::RestrictedCookieManager> cookie_manager_;

  base::WeakPtrFactory<PrefetchedNavigationLoaderInterceptor> weak_factory_{
      this};
};

bool CanStoreEntry(const PrefetchedSignedExchangeCacheEntry& entry) {
  const net::HttpResponseHeaders* outer_headers =
      entry.outer_response()->headers.get();
  // We don't store responses with a "cache-control: no-store" header.
  if (outer_headers->HasHeaderValue("cache-control", "no-store"))
    return false;

  // Generally we don't store responses with a "vary" header. We only allows
  // "accept-encoding" vary header. This is because content decoding is handled
  // by the network layer and PrefetchedSignedExchangeCache stores decoded
  // response bodies, so we can safely ignore varying on the "Accept-Encoding"
  // header.
  std::optional<std::string_view> value;
  size_t iter = 0;
  while ((value = outer_headers->EnumerateHeader(&iter, "vary"))) {
    if (!base::EqualsCaseInsensitiveASCII(*value, "accept-encoding")) {
      return false;
    }
  }
  return true;
}

bool CanUseEntry(const PrefetchedSignedExchangeCacheEntry& entry,
                 const base::Time& verification_time) {
  if (entry.signature_expire_time() < verification_time)
    return false;

  auto& outer_response = entry.outer_response();

  // Use the prefetched entry within kPrefetchReuseMins minutes without
  // validation.
  if (outer_response->headers->GetCurrentAge(outer_response->request_time,
                                             outer_response->response_time,
                                             verification_time) <
      base::Minutes(net::HttpCache::kPrefetchReuseMins)) {
    return true;
  }
  // We use the prefetched entry when we don't need the validation.
  if (outer_response->headers->RequiresValidation(
          outer_response->request_time, outer_response->response_time,
          verification_time) != net::VALIDATION_NONE) {
    return false;
  }
  return true;
}

// Deserializes a SHA256HashValue from a string. On error, returns false.
// This method support the form of "sha256-<base64-hash-value>".
bool ExtractSHA256HashValueFromString(std::string_view value,
                                      net::SHA256HashValue* out) {
  if (!base::StartsWith(value, "sha256-"))
    return false;
  const std::string_view base64_str = value.substr(7);
  std::string decoded;
  if (!base::Base64Decode(base64_str, &decoded) ||
      decoded.size() != sizeof(out->data)) {
    return false;
  }
  memcpy(out->data, decoded.data(), sizeof(out->data));
  return true;
}

// Returns a map of subresource URL to SHA256HashValue which are declared in the
// rel=allowd-alt-sxg link header of |main_exchange|'s inner response.
std::map<GURL, net::SHA256HashValue> GetAllowedAltSXG(
    const PrefetchedSignedExchangeCacheEntry& main_exchange) {
  std::map<GURL, net::SHA256HashValue> result;
  std::string link_header;
  main_exchange.inner_response()->headers->GetNormalizedHeader("link",
                                                               &link_header);
  if (link_header.empty())
    return result;

  for (const auto& value : link_header_util::SplitLinkHeader(link_header)) {
    std::string link_url;
    std::unordered_map<std::string, std::optional<std::string>> link_params;
    if (!link_header_util::ParseLinkHeaderValue(value.first, value.second,
                                                &link_url, &link_params)) {
      continue;
    }

    auto rel = link_params.find("rel");
    auto header_integrity = link_params.find(kHeaderIntegrity);
    net::SHA256HashValue header_integrity_value;
    if (rel == link_params.end() || header_integrity == link_params.end() ||
        rel->second.value_or("") != std::string(kAllowedAltSxg) ||
        !ExtractSHA256HashValueFromString(
            std::string_view(header_integrity->second.value_or("")),
            &header_integrity_value)) {
      continue;
    }
    result[main_exchange.inner_url().Resolve(link_url)] =
        header_integrity_value;
  }
  return result;
}

}  // namespace

PrefetchedSignedExchangeCache::PrefetchedSignedExchangeCache() = default;

PrefetchedSignedExchangeCache::~PrefetchedSignedExchangeCache() = default;

void PrefetchedSignedExchangeCache::Store(
    std::unique_ptr<const PrefetchedSignedExchangeCacheEntry> cached_exchange) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (exchanges_.size() > kMaxEntrySize)
    return;
  DCHECK(cached_exchange->outer_url().is_valid());
  DCHECK(cached_exchange->outer_response());
  DCHECK(cached_exchange->header_integrity());
  DCHECK(cached_exchange->inner_url().is_valid());
  DCHECK(cached_exchange->inner_response());
  DCHECK(cached_exchange->completion_status());
  DCHECK(cached_exchange->blob_data_handle());
  DCHECK(!cached_exchange->signature_expire_time().is_null());

  if (!CanStoreEntry(*cached_exchange))
    return;
  const GURL outer_url = cached_exchange->outer_url();
  exchanges_[outer_url] = std::move(cached_exchange);
  for (TestObserver& observer : test_observers_)
    observer.OnStored(this, outer_url);
}

void PrefetchedSignedExchangeCache::Clear() {
  exchanges_.clear();
}

std::unique_ptr<NavigationLoaderInterceptor>
PrefetchedSignedExchangeCache::MaybeCreateInterceptor(
    const GURL& outer_url,
    FrameTreeNodeId frame_tree_node_id,
    const net::IsolationInfo& isolation_info) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  const auto it = exchanges_.find(outer_url);
  if (it == exchanges_.end())
    return nullptr;
  const base::Time verification_time =
      signed_exchange_utils::GetVerificationTime();
  const std::unique_ptr<const PrefetchedSignedExchangeCacheEntry>& exchange =
      it->second;
  if (!CanUseEntry(*exchange.get(), verification_time)) {
    exchanges_.erase(it);
    return nullptr;
  }
  auto info_list =
      GetInfoListForNavigation(*exchange, verification_time, frame_tree_node_id,
                               isolation_info.network_anonymization_key());

  mojo::Remote<network::mojom::RestrictedCookieManager> cookie_manager;
  auto* frame = FrameTreeNode::GloballyFindByID(frame_tree_node_id);
  if (frame) {
    StoragePartition* storage_partition =
        frame->current_frame_host()->GetProcess()->GetStoragePartition();
    url::Origin inner_url_origin = url::Origin::Create(exchange->inner_url());
    net::IsolationInfo inner_url_isolation_info =
        isolation_info.CreateForRedirect(inner_url_origin);

    RenderFrameHostImpl* render_frame_host = frame->current_frame_host();
    static_cast<StoragePartitionImpl*>(storage_partition)
        ->CreateRestrictedCookieManager(
            network::mojom::RestrictedCookieManagerRole::NETWORK,
            inner_url_origin, inner_url_isolation_info,
            /* is_service_worker = */ false,
            render_frame_host ? render_frame_host->GetProcess()->GetID() : -1,
            render_frame_host ? render_frame_host->GetRoutingID()
                              : MSG_ROUTING_NONE,
            render_frame_host ? render_frame_host->GetCookieSettingOverrides()
                              : net::CookieSettingOverrides(),
            cookie_manager.BindNewPipeAndPassReceiver(),
            render_frame_host ? render_frame_host->CreateCookieAccessObserver()
                              : mojo::NullRemote());
  }

  return std::make_unique<PrefetchedNavigationLoaderInterceptor>(
      exchange->Clone(), std::move(info_list), std::move(cookie_manager));
}

const PrefetchedSignedExchangeCache::EntryMap&
PrefetchedSignedExchangeCache::GetExchanges() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return exchanges_;
}

void PrefetchedSignedExchangeCache::RecordHistograms() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (exchanges_.empty())
    return;
  UMA_HISTOGRAM_COUNTS_100("PrefetchedSignedExchangeCache.Count",
                           exchanges_.size());
}

std::vector<blink::mojom::PrefetchedSignedExchangeInfoPtr>
PrefetchedSignedExchangeCache::GetInfoListForNavigation(
    const PrefetchedSignedExchangeCacheEntry& main_exchange,
    const base::Time& verification_time,
    FrameTreeNodeId frame_tree_node_id,
    const net::NetworkAnonymizationKey& network_anonymization_key) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  const url::Origin outer_url_origin =
      url::Origin::Create(main_exchange.outer_url());
  const url::Origin request_initiator_origin_lock =
      url::Origin::Create(main_exchange.inner_url());
  const auto inner_url_header_integrity_map = GetAllowedAltSXG(main_exchange);

  std::vector<blink::mojom::PrefetchedSignedExchangeInfoPtr> info_list;
  EntryMap::iterator exchanges_it = exchanges_.begin();
  while (exchanges_it != exchanges_.end()) {
    const std::unique_ptr<const PrefetchedSignedExchangeCacheEntry>& exchange =
        exchanges_it->second;
    if (!CanUseEntry(*exchange.get(), verification_time)) {
      exchanges_.erase(exchanges_it++);
      continue;
    }
    auto it = inner_url_header_integrity_map.find(exchange->inner_url());
    if (it == inner_url_header_integrity_map.end()) {
      ++exchanges_it;
      continue;
    }

    // Restrict the main SXG and the subresources SXGs to be served from the
    // same origin.
    if (!outer_url_origin.IsSameOriginWith(
            url::Origin::Create(exchange->outer_url()))) {
      ++exchanges_it;
      continue;
    }

    if (it->second != *exchange->header_integrity()) {
      ++exchanges_it;
      auto reporter = SignedExchangeReporter::MaybeCreate(
          exchange->outer_url(), main_exchange.outer_url().spec(),
          *exchange->outer_response(), network_anonymization_key,
          frame_tree_node_id);
      if (reporter) {
        reporter->set_cert_server_ip_address(
            exchange->cert_server_ip_address());
        reporter->set_inner_url(exchange->inner_url());
        reporter->set_cert_url(exchange->cert_url());
        reporter->ReportHeaderIntegrityMismatch();
      }
      continue;
    }

    mojo::PendingRemote<network::mojom::URLLoaderFactory>
        pending_loader_factory;
    new SubresourceSignedExchangeURLLoaderFactory(
        pending_loader_factory.InitWithNewPipeAndPassReceiver(),
        exchange->Clone(), request_initiator_origin_lock);
    info_list.emplace_back(blink::mojom::PrefetchedSignedExchangeInfo::New(
        exchange->outer_url(), *exchange->header_integrity(),
        exchange->inner_url(), exchange->inner_response().Clone(),
        std::move(pending_loader_factory)));
    ++exchanges_it;
  }
  return info_list;
}

void PrefetchedSignedExchangeCache::AddObserverForTesting(
    TestObserver* observer) {
  test_observers_.AddObserver(observer);
}

void PrefetchedSignedExchangeCache::RemoveObserverForTesting(
    const TestObserver* observer) {
  test_observers_.RemoveObserver(observer);
}

}  // namespace content
