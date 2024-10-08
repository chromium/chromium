// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_URL_LOADER_H_
#define SERVICES_NETWORK_URL_LOADER_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/numerics/safe_conversions.h"
#include "base/unguessable_token.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "mojo/public/cpp/system/simple_watcher.h"
#include "net/base/load_states.h"
#include "net/base/network_delegate.h"
#include "net/base/transport_info.h"
#include "net/base/upload_progress.h"
#include "net/cookies/cookie_setting_override.h"
#include "net/cookies/cookie_util.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "net/url_request/url_request.h"
#include "services/network/attribution/attribution_request_helper.h"
#include "services/network/keepalive_statistics_recorder.h"
#include "services/network/network_service.h"
#include "services/network/private_network_access_checker.h"
#include "services/network/public/cpp/cors/cors_error_status.h"
#include "services/network/public/cpp/initiator_lock_compatibility.h"
#include "services/network/public/cpp/orb/orb_api.h"
#include "services/network/public/cpp/private_network_access_check_result.h"
#include "services/network/public/mojom/accept_ch_frame_observer.mojom.h"
#include "services/network/public/mojom/cookie_access_observer.mojom.h"
#include "services/network/public/mojom/cross_origin_embedder_policy.mojom-forward.h"
#include "services/network/public/mojom/devtools_observer.mojom.h"
#include "services/network/public/mojom/fetch_api.mojom.h"
#include "services/network/public/mojom/ip_address_space.mojom-forward.h"
#include "services/network/public/mojom/ip_address_space.mojom-shared.h"
#include "services/network/public/mojom/network_service.mojom.h"
#include "services/network/public/mojom/trust_token_access_observer.mojom.h"
#include "services/network/public/mojom/trust_tokens.mojom-shared.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom-forward.h"
#include "services/network/resource_scheduler/resource_scheduler.h"
#include "services/network/resource_scheduler/resource_scheduler_client.h"
#include "services/network/shared_dictionary/shared_dictionary_access_checker.h"
#include "services/network/shared_storage/shared_storage_request_helper.h"
#include "services/network/trust_tokens/pending_trust_token_store.h"
#include "services/network/trust_tokens/trust_token_request_helper.h"
#include "services/network/trust_tokens/trust_token_request_helper_factory.h"
#include "services/network/upload_progress_tracker.h"
#include "services/network/url_loader_context.h"

namespace net {
class HttpResponseHeaders;
class IOBufferWithSize;
class IPEndPoint;
class URLRequestContext;
struct RedirectInfo;
}  // namespace net

namespace network {

namespace cors {
class OriginAccessList;
}

namespace internal {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(FetchKeepAliveRequestNetworkMetricType)
enum class FetchKeepAliveRequestNetworkMetricType {
  kOnCreate = 0,
  kOnResponse = 1,
  kMaxValue = kOnResponse
};
// LINT.ThenChange(//tools/metrics/histograms/enums.xml:FetchKeepAliveRequestNetworkMetricType)

}  // namespace internal

constexpr size_t kMaxFileUploadRequestsPerBatch = 64;

class KeepaliveStatisticsRecorder;
class NetToMojoPendingBuffer;
class ScopedThrottlingToken;
class SharedDictionaryManager;
class SlopBucket;

class COMPONENT_EXPORT(NETWORK_SERVICE) URLLoader
    : public mojom::URLLoader,
      public net::URLRequest::Delegate,
      public mojom::AuthChallengeResponder,
      public mojom::ClientCertificateResponder {
 public:
  using DeleteCallback = base::OnceCallback<void(URLLoader* loader)>;

  // Holds a sync and async implementation of URLLoaderClient. The sync
  // implementation can be used if present to avoid posting a task to call back
  // into CorsURLLoader.
  class MaybeSyncURLLoaderClient {
   public:
    MaybeSyncURLLoaderClient(
        mojo::PendingRemote<mojom::URLLoaderClient> mojo_client,
        base::WeakPtr<mojom::URLLoaderClient> sync_client);
    ~MaybeSyncURLLoaderClient();

    // Resets both URLLoaderClients.
    void Reset();

    // Rebinds the mojo URLLoaderClient and uses it for future calls.
    mojo::PendingReceiver<mojom::URLLoaderClient> BindNewPipeAndPassReceiver();

    // Gets the sync URLLoaderClient if available, otherwise the mojo remote.
    mojom::URLLoaderClient* Get();

   private:
    mojo::Remote<mojom::URLLoaderClient> mojo_client_;
    base::WeakPtr<mojom::URLLoaderClient> sync_client_;
  };

  // A subset of the fields in mojom::LoadInfo.
  struct PartialLoadInfo final {
    PartialLoadInfo() = default;
    PartialLoadInfo(net::LoadStateWithParam load_state,
                    net::UploadProgress upload_progress);

    // Avoid accidentally copying this object as `load_state` contains a string.
    PartialLoadInfo(const PartialLoadInfo&) = delete;
    PartialLoadInfo& operator=(const PartialLoadInfo&) = delete;

    // Moving it is good.
    PartialLoadInfo(PartialLoadInfo&&) = default;
    PartialLoadInfo& operator=(PartialLoadInfo&&) = default;

    net::LoadStateWithParam load_state;
    net::UploadProgress upload_progress;
  };

  // |delete_callback| tells the URLLoader's owner to destroy the URLLoader.
  //
  // |trust_token_helper_factory| must be non-null exactly when the request has
  // Trust Tokens parameters.
  //
  // The caller needs to guarantee that the pointers/references in the
  // |context| will live longer than the constructed URLLoader.  One
  // (incomplete) reason why this guarantee is true in production code is that
  // |context| is implemented by URLLoaderFactory which outlives the lifecycle
  // of the URLLoader (and some pointers in |context| point to objects owned by
  // URLLoaderFactory).
  //
  // Pointers from the |url_loader_context| will be used if
  // |dev_tools_observer|, |cookie_access_observer| or
  // |url_loader_network_observer| are not provided.
  //
  // |third_party_cookies_enabled| is also false if all cookies are disabled.
  // The mojom::kURLLoadOptionBlockThirdPartyCookies can be set or unset
  // independently of this option.
  URLLoader(
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
      std::unique_ptr<TrustTokenRequestHelperFactory>
          trust_token_helper_factory,
      SharedDictionaryManager* shared_dictionary_manager,
      std::unique_ptr<SharedDictionaryAccessChecker> shared_dictionary_checker,
      mojo::PendingRemote<mojom::CookieAccessObserver> cookie_observer,
      mojo::PendingRemote<mojom::TrustTokenAccessObserver> trust_token_observer,
      mojo::PendingRemote<mojom::URLLoaderNetworkServiceObserver>
          url_loader_network_observer,
      mojo::PendingRemote<mojom::DevToolsObserver> devtools_observer,
      mojo::PendingRemote<mojom::AcceptCHFrameObserver>
          accept_ch_frame_observer,
      std::unique_ptr<AttributionRequestHelper> attribution_request_helper,
      bool shared_storage_writable_eligible);

  URLLoader(const URLLoader&) = delete;
  URLLoader& operator=(const URLLoader&) = delete;

  ~URLLoader() override;

  // mojom::URLLoader implementation:
  void FollowRedirect(
      const std::vector<std::string>& removed_headers,
      const net::HttpRequestHeaders& modified_headers,
      const net::HttpRequestHeaders& modified_cors_exempt_headers,
      const std::optional<GURL>& new_url) override;
  void SetPriority(net::RequestPriority priority,
                   int32_t intra_priority_value) override;
  void PauseReadingBodyFromNet() override;
  void ResumeReadingBodyFromNet() override;

  // net::URLRequest::Delegate implementation:
  int OnConnected(net::URLRequest* url_request,
                  const net::TransportInfo& info,
                  net::CompletionOnceCallback callback) override;
  void OnReceivedRedirect(net::URLRequest* url_request,
                          const net::RedirectInfo& redirect_info,
                          bool* defer_redirect) override;
  void OnAuthRequired(net::URLRequest* request,
                      const net::AuthChallengeInfo& info) override;
  void OnCertificateRequested(net::URLRequest* request,
                              net::SSLCertRequestInfo* info) override;
  void OnSSLCertificateError(net::URLRequest* request,
                             int net_error,
                             const net::SSLInfo& info,
                             bool fatal) override;
  void OnResponseStarted(net::URLRequest* url_request, int net_error) override;
  void OnReadCompleted(net::URLRequest* url_request, int bytes_read) override;

  // These methods are called by the network delegate to forward these events to
  // the |header_client_|.
  int OnBeforeStartTransaction(
      const net::HttpRequestHeaders& headers,
      net::NetworkDelegate::OnBeforeStartTransactionCallback callback);
  int OnHeadersReceived(
      net::CompletionOnceCallback callback,
      const net::HttpResponseHeaders* original_response_headers,
      scoped_refptr<net::HttpResponseHeaders>* override_response_headers,
      const net::IPEndPoint& endpoint,
      std::optional<GURL>* preserve_fragment_on_redirect_url);

  mojom::URLLoaderNetworkServiceObserver* GetURLLoaderNetworkServiceObserver()
      const {
    return url_loader_network_observer_;
  }

  // mojom::AuthChallengeResponder:
  void OnAuthCredentials(
      const std::optional<net::AuthCredentials>& credentials) override;

  // mojom::ClientCertificateResponder:
  void ContinueWithCertificate(
      const scoped_refptr<net::X509Certificate>& x509_certificate,
      const std::string& provider_name,
      const std::vector<uint16_t>& algorithm_preferences,
      mojo::PendingRemote<mojom::SSLPrivateKey> ssl_private_key) override;
  void ContinueWithoutCertificate() override;
  void CancelRequest() override;

  // Cancel the request because network revocation was triggered.
  void CancelRequestIfNonceMatchesAndUrlNotExempted(
      const base::UnguessableToken& nonce,
      const std::set<GURL>& exemptions);

  net::LoadState GetLoadState() const;
  net::UploadProgress GetUploadProgress() const;

  int32_t GetProcessId() const;
  uint32_t GetResourceType() const;

  // Whether this URLLoader should allow sending/setting any cookies.
  // This decision is based on the options passed to
  // URLLoaderFactory::CreateLoaderAndStart().
  bool CookiesDisabled() const;

  // Whether this URLLoader should allow sending/setting cookies for requests
  // with |url| and |site_for_cookies|. This decision is based on the options
  // passed to URLLoaderFactory::CreateLoaderAndStart().
  // If this returns false, partitioned cookies could still be provided if
  // CookiesDisabled returns false.
  bool AllowFullCookies(const GURL& url,
                        const net::SiteForCookies& site_for_cookies) const;

  // Returns whether a particular cookie is allowed to be sent for requests
  // with |url| and |site_for_cookies|. This decision is based on the options
  // passed to URLLoaderFactory::CreateLoaderAndStart().
  bool AllowCookie(const net::CanonicalCookie& cookie,
                   const GURL& url,
                   const net::SiteForCookies& site_for_cookies) const;

  const net::HttpRequestHeaders& custom_proxy_pre_cache_headers() const {
    return custom_proxy_pre_cache_headers_;
  }

  const net::HttpRequestHeaders& custom_proxy_post_cache_headers() const {
    return custom_proxy_post_cache_headers_;
  }

  const std::optional<GURL>& new_redirect_url() const {
    return new_redirect_url_;
  }

  const std::optional<std::string>& devtools_request_id() const {
    return devtools_request_id_;
  }

  SharedStorageRequestHelper* shared_storage_request_helper() const {
    return shared_storage_request_helper_.get();
  }

  void SetEnableReportingRawHeaders(bool enable);

  // Returns a subset of the info in mojom::LoadInfo. This is sufficient to make
  // a decision on whether to call CreateLoadInfo() for this loader.
  PartialLoadInfo GetPartialLoadInfo() const;

  // Returns a mojom::LoadInfo, reusing the data returned by
  // GetPartialLoadInfo().
  mojom::LoadInfoPtr CreateLoadInfo(const PartialLoadInfo& partial_load_info);

  // Gets the URLLoader associated with this request.
  static URLLoader* ForRequest(const net::URLRequest& request);

  static const void* const kUserDataKey;

  static bool HasFetchStreamingUploadBody(const ResourceRequest*);

  static std::optional<net::IsolationInfo> GetIsolationInfo(
      const net::IsolationInfo& factory_isolation_info,
      bool automatically_assign_isolation_info,
      const ResourceRequest& request);

  static net::CookieSettingOverrides CalculateCookieSettingOverrides(
      net::CookieSettingOverrides factory_overrides,
      const ResourceRequest& request);

 private:
  // This class is used to set the URLLoader as user data on a URLRequest. This
  // is used instead of URLLoader directly because SetUserData requires a
  // std::unique_ptr. This is safe because URLLoader owns the URLRequest, so is
  // guaranteed to outlive it.
  class UnownedPointer : public base::SupportsUserData::Data {
   public:
    explicit UnownedPointer(URLLoader* pointer) : pointer_(pointer) {}

    UnownedPointer(const UnownedPointer&) = delete;
    UnownedPointer& operator=(const UnownedPointer&) = delete;

    URLLoader* get() const { return pointer_; }

   private:
    const raw_ptr<URLLoader> pointer_;
  };

  class FileOpenerForUpload;
  friend class FileOpenerForUpload;

  // An enum class representing the result of keepalive requests. This is used
  // for UMA so do NOT re-assign values.
  enum class KeepaliveRequestResult {
    kOk = 0,
    kMojoConnectionErrorBeforeResponseArrival = 1,
    kMojoConnectionErrorAfterResponseArrival = 2,
    kErrorBeforeResponseArrival = 3,
    kErrorAfterResponseArrival = 4,
    kMaxValue = kErrorAfterResponseArrival,
  };

  // Configures `url_request_`, including registering callbacks.
  void ConfigureRequest(
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
      std::optional<net::SharedDictionaryGetter> shared_dictionary_getter);

  void OpenFilesForUpload(const ResourceRequest& request);
  void SetUpUpload(const ResourceRequest& request,
                   int error_code,
                   const std::vector<base::File> opened_files);

  // A `ResourceRequest` where `shared_storage_writable_eligible` is true, is
  // eligible for shared storage operations via response headers.
  //
  // Outbound control flow:
  //
  // Start in `ProcessOutboundSharedStorageInterceptor()`
  // - Execute `SharedStorageRequestHelper::ProcessOutgoingRequest`, which will
  // add the `kSecSharedStorageWritableHeader` request header to the
  // `URLRequest` if `ResourceRequest::shared_storage_writable_eligible` is true
  // and there is a `mojom::URLLoaderNetworkServiceObserver*` available to
  // forward processed headers to.
  // - `ScheduleStart` immediately afterwards regardless of eligibility for
  // shared storage
  //
  // Outbound redirection control flow:
  //
  // Start in `FollowRedirect`
  // - Execute
  // `SharedStorageRequestHelper::UpdateSharedStorageWritableEligible`
  // to remove or restore the `kSecSharedStorageWritableHeader` request header
  // if eligibility has been lost or regained
  //
  // Inbound redirection control flow:
  //
  // Start in `ProcessInboundSharedStorageInterceptorOnReceivedRedirect`
  // - Execute `SharedStorageRequestHelper::ProcessIncomingResponse`
  // - If the request has received the `kSharedStorageWriteHeader` response
  // header and if it is currently eligible for shared storage (i.e., in
  // particular, the `kSecSharedStorageWritableHeader` has not been removed on a
  // redirect), the helper will parse the header value into a vector of Shared
  // Storage operations to call
  // - If the request has not received the `kSharedStorageWriteHeader` response
  // header, or if parsing fails to produce any valid operations, then
  // immediately call `ContinueOnReceivedRedirect`
  // - Otherwise, `ContinueOnReceivedRedirect` will be run asynchronously after
  // forwarding the operations to `URLLoaderNetworkServiceObserver` to queue via
  // Mojo
  //
  // Inbound control flow:
  //
  // Start in `ProcessInboundSharedStorageInterceptorOnResponseStarted`
  // - Execute `SharedStorageRequestHelper::ProcessIncomingResponse`
  // - If the request has received the `kSharedStorageWriteHeader` response
  // header and if it is currently eligible for shared storage (i.e., in
  // particular, the `kSecSharedStorageWritableHeader` has not been removed on a
  // redirect), the helper will parse the header value into a vector of Shared
  // Storage operations to call
  // - If the request has not received the `kSharedStorageWriteHeader` response
  // header, or if parsing fails to produce any valid operations, then
  // immediately call `ContinueOnResponseStarted`
  // - Otherwise, `ContinueOnResponseStarted` will be run asynchronously after
  // forwarding the operations to `URLLoaderNetworkServiceObserver` to queue via
  // Mojo
  void ProcessOutboundSharedStorageInterceptor();
  void ProcessInboundSharedStorageInterceptorOnReceivedRedirect(
      const ::net::RedirectInfo& redirect_info,
      mojom::URLResponseHeadPtr response);
  void ProcessInboundSharedStorageInterceptorOnResponseStarted();

  // A request where `attribution_request_helper_` is defined will (assuming
  // preconditions pass and operations are successful) have one
  // `AttributionRequestHelper::Begin` executed against the request, one
  // `AttributionRequestHelper::OnReceiveRedirect` per redirection received and
  // one `AttributionRequestHelper::Finalize` executed against its response.
  //
  // Outbound control flow:
  //
  // Start in `ProcessOutboundAttributionInterceptor`
  // - If `attribution_request_helper_` is not defined, immediately
  //   calls`ScheduleStart`.
  // - Otherwise:
  //   - Execute `AttributionRequestHelper::Begin`
  //   - On Begin's callback, calls `ScheduleStart`
  //
  // Redirection control flow:
  //
  // Start in `ProcessInboundAttributionInterceptorOnReceivedRedirect`
  //  - If `attribution_request_helper_` is not defined, immediately
  //    calls`ProcessInboundAttributionInterceptorOnReceivedRedirect`.
  // - Otherwise:
  //   - Execute `AttributionRequestHelper::OnReceiveRedirect`
  //   - On OnReceiveRedirect's callback, calls
  //   `ProcessInboundAttributionInterceptorOnReceivedRedirect`
  //
  // Inbound control flow:
  //
  // Start in `ProcessInboundAttributionInterceptorOnResponseStarted`
  //  - If `attribution_request_helper_` is not defined, immediately
  //    calls`ProcessInboundSharedStorageInterceptorOnResponseStarted`.
  // - Otherwise:
  //   - Execute `AttributionRequestHelper::Finalize`
  //   - On Finalize's callback, calls
  //   `ProcessInboundSharedStorageInterceptorOnResponseStarted`
  void ProcessOutboundAttributionInterceptor();
  void ProcessInboundAttributionInterceptorOnReceivedRedirect(
      const ::net::RedirectInfo& redirect_info,
      mojom::URLResponseHeadPtr response);
  void ProcessInboundAttributionInterceptorOnResponseStarted();

  // Continuation of `OnReceivedRedirect` after possibly asynchronously
  // concluding the request's Attribution and/or Shared Storage operations.
  void ContinueOnReceiveRedirect(const ::net::RedirectInfo& redirect_info,
                                 uint64_t response_index);

  // A request with Trust Tokens parameters will (assuming preconditions pass
  // and operations are successful) have one TrustTokenRequestHelper::Begin
  // executed against the request and one TrustTokenRequestHelper::Finalize
  // executed against its response.
  //
  // Outbound control flow:
  //
  // Start in ProcessOutboundTrustTokenInterceptor
  // - If there are no Trust Tokens parameters, immediately ScheduleStart.
  // - Otherwise:
  //   - asynchronously construct a TrustTokenRequestHelper;
  //   - receive the helper (or an error) in OnDoneConstructingTrustTokenHelper
  //   and, if an error, fail the request;
  //   - execute TrustTokenRequestHelper::Begin against the helper;
  //   - receive the result in OnDoneBeginningTrustTokenOperation;
  //   - if successful, ScheduleStart; if there was an error, fail.
  //
  // Inbound control flow:
  //
  // Start in OnResponseStarted
  // - If there are no Trust Tokens parameters, proceed to
  // ContinueOnResponseStarted.
  // - Otherwise:
  //   - execute TrustTokenRequestHelper::Finalize against the helper;
  //   - receive the result in OnDoneFinalizingTrustTokenOperation;
  //   - if successful, ProcessInboundAttributionInterceptorOnResponseStarted;
  //   if there was an error, fail.
  void ProcessOutboundTrustTokenInterceptor(const ResourceRequest& request);
  void OnDoneConstructingTrustTokenHelper(
      mojom::TrustTokenOperationType type,
      TrustTokenStatusOrRequestHelper status_or_helper);
  void OnDoneBeginningTrustTokenOperation(
      std::optional<net::HttpRequestHeaders> headers,
      mojom::TrustTokenOperationStatus status);
  void OnDoneFinalizingTrustTokenOperation(
      mojom::TrustTokenOperationStatus status);

  // Continuation of `OnResponseStarted` after possibly asynchronously
  // concluding the request's Trust Tokens, Attribution, and/or Shared Storage
  // operations.
  void ContinueOnResponseStarted();
  void MaybeSendTrustTokenOperationResultToDevTools();

  void ScheduleStart();
  void ReadMore();
  void DidRead(int num_bytes,
               bool completed_synchronously,
               bool into_slop_bucket);
  void NotifyCompleted(int error_code);
  void OnMojoDisconnect();
  void OnResponseBodyStreamConsumerClosed(MojoResult result);
  void OnResponseBodyStreamReady(MojoResult result);
  void DeleteSelf();
  void SendResponseToClient();
  void CompletePendingWrite(bool success);
  void SetRawResponseHeaders(scoped_refptr<const net::HttpResponseHeaders>);
  void NotifyEarlyResponse(scoped_refptr<const net::HttpResponseHeaders>);
  void MaybeNotifyEarlyResponseToDevtools(const net::HttpResponseHeaders&);
  void SetRawRequestHeadersAndNotify(net::HttpRawRequestHeaders);
  bool IsSharedDictionaryReadAllowed();
  void DispatchOnRawRequest(
      std::vector<network::mojom::HttpRawHeaderPairPtr> headers);
  bool DispatchOnRawResponse();
  void SendUploadProgress(const net::UploadProgress& progress);
  void OnUploadProgressACK();
  void OnSSLCertificateErrorResponse(const net::SSLInfo& ssl_info,
                                     int net_error);
  bool HasDataPipe() const;
  void RecordBodyReadFromNetBeforePausedIfNeeded();
  void ResumeStart();
  void OnBeforeSendHeadersComplete(
      net::NetworkDelegate::OnBeforeStartTransactionCallback callback,
      int result,
      const std::optional<net::HttpRequestHeaders>& headers);
  void OnHeadersReceivedComplete(
      net::CompletionOnceCallback callback,
      scoped_refptr<net::HttpResponseHeaders>* out_headers,
      std::optional<GURL>* out_preserve_fragment_on_redirect_url,
      int result,
      const std::optional<std::string>& headers,
      const std::optional<GURL>& preserve_fragment_on_redirect_url);

  void CompleteBlockedResponse(
      int error_code,
      bool should_report_orb_blocking,
      std::optional<mojom::BlockedByResponseReason> reason = std::nullopt);

  enum BlockResponseForOrbResult {
    // Returned when caller of BlockResponseForOrb doesn't need to continue,
    // because the request will be cancelled soon.
    kWillCancelRequest,

    // Returned when the caller of BlockResponseForOrb should continue
    // processing the request (e.g. by calling ReadMore as necessary).
    kContinueRequest,
  };
  // Block the response because of ORB.
  BlockResponseForOrbResult BlockResponseForOrb();
  // Decide whether to call block a response via BlockResponseForOrb.
  // Returns true if the request should be cancelled.
  bool MaybeBlockResponseForOrb(orb::ResponseAnalyzer::Decision);

  void ReportFlaggedResponseCookies(bool call_cookie_observer);
  void StartReading();

  // Whether `force_ignore_site_for_cookies` should be set on net::URLRequest.
  bool ShouldForceIgnoreSiteForCookies(const ResourceRequest& request);

  // Applies Private Network Access checks to the current request.
  //
  // Helper for `OnConnected()`.
  PrivateNetworkAccessCheckResult PrivateNetworkAccessCheck(
      const net::TransportInfo& transport_info);

  mojom::DevToolsObserver* GetDevToolsObserver() const;
  mojom::CookieAccessObserver* GetCookieAccessObserver() const;

  // Builds a response struct based on the data received so far.
  // Never returns nullptr.
  mojom::URLResponseHeadPtr BuildResponseHead() const;

  // Determine given the |url|, whether the |url_request_| should include
  // credentials and client certificates.
  void SetRequestCredentials(const GURL& url);

  // Returns whether sending/storing credentials is allowed by COEP and
  // Document-Isolation-Policy.
  // |url| is the latest request URL, either the original URL or
  // `redirect_info.new_url`.
  // When Cross-Origin-Embedder-Policy: credentialless or
  // Document-Isolation-Policy: isolate-and-credentialless are set, do not send
  // or store credentials for no-cors cross-origin request.
  bool WebPoliciesAllowCredentials(const GURL& url);

  // Returns whether TransferSizeUpdated IPC should be sent.
  bool ShouldSendTransferSizeUpdated() const;

  // Returns true if the corresponding `URLResponseHead`'s
  // `load_with_storage_access` field should be set.
  bool ShouldSetLoadWithStorageAccess() const;

  // Records metrics about GET requests.
  void RecordRequestMetrics();

  const raw_ptr<net::URLRequestContext> url_request_context_;

  const raw_ptr<mojom::NetworkContextClient> network_context_client_;
  DeleteCallback delete_callback_;

  int32_t options_;
  const int resource_type_;
  const bool is_load_timing_enabled_;
  bool has_received_response_ = false;

  // URLLoaderFactory is guaranteed to outlive URLLoader, so it is safe to
  // store a raw pointer to mojom::URLLoaderFactoryParams.
  const raw_ref<const mojom::URLLoaderFactoryParams> factory_params_;
  // This also belongs to URLLoaderFactory and outlives this loader.
  const raw_ptr<mojom::CrossOriginEmbedderPolicyReporter> coep_reporter_;

  const int32_t request_id_;
  const int keepalive_request_size_;
  const bool keepalive_;
  const bool do_not_prompt_for_login_;
  std::unique_ptr<net::URLRequest> url_request_;
  mojo::Receiver<mojom::URLLoader> receiver_;
  mojo::Receiver<mojom::AuthChallengeResponder>
      auth_challenge_responder_receiver_{this};
  mojo::Receiver<mojom::ClientCertificateResponder>
      client_cert_responder_receiver_{this};
  MaybeSyncURLLoaderClient url_loader_client_;
  int64_t total_written_bytes_ = 0;

  mojo::ScopedDataPipeProducerHandle response_body_stream_;
  scoped_refptr<NetToMojoPendingBuffer> pending_write_;
  uint32_t pending_write_buffer_size_ = 0;
  uint32_t pending_write_buffer_offset_ = 0;
  mojo::SimpleWatcher writable_handle_watcher_;
  mojo::SimpleWatcher peer_closed_handle_watcher_;

  scoped_refptr<net::IOBufferWithSize> discard_buffer_;

  // True if there's a URLRequest::Read() call in progress.
  bool read_in_progress_ = false;

  // Stores any CORS error encountered while processing |url_request_|.
  std::optional<CorsErrorStatus> cors_error_status_;

  // Used when deferring sending the data to the client until mime sniffing is
  // finished.
  mojom::URLResponseHeadPtr response_;
  mojo::ScopedDataPipeConsumerHandle consumer_handle_;

  // Sniffing state and ORB state.
  bool is_more_orb_sniffing_needed_ = false;
  bool is_more_mime_sniffing_needed_ = false;
  const raw_ref<orb::PerFactoryState> per_factory_orb_state_;
  // `orb_analyzer_` must be destructed before `per_factory_orb_state_`.
  std::unique_ptr<orb::ResponseAnalyzer> orb_analyzer_;

  std::unique_ptr<ResourceScheduler::ScheduledResourceRequest>
      resource_scheduler_request_handle_;

  bool enable_reporting_raw_headers_ = false;
  bool seen_raw_request_headers_ = false;
  // Used for metrics.
  size_t raw_request_line_size_ = 0;
  size_t raw_request_headers_size_ = 0;
  scoped_refptr<const net::HttpResponseHeaders> raw_response_headers_;

  std::unique_ptr<UploadProgressTracker> upload_progress_tracker_;

  // Holds the URL of a redirect if it's currently deferred.
  std::unique_ptr<GURL> deferred_redirect_url_;

  // If |new_url| is given to FollowRedirect() it's saved here, so that it can
  // be later referred to from NetworkContext::OnBeforeURLRequestInternal, which
  // is called from NetworkDelegate::NotifyBeforeURLRequest.
  std::optional<GURL> new_redirect_url_;

  // The ID that DevTools uses to track network requests. It is generated in the
  // renderer process and is only present when DevTools is enabled in the
  // renderer.
  const std::optional<std::string> devtools_request_id_;

  bool should_pause_reading_body_ = false;
  // The response body stream is open, but transferring data is paused.
  bool paused_reading_body_ = false;

  // This is used to compute the delta since last time received
  // encoded body size was reported to the client.
  int64_t reported_total_encoded_bytes_ = 0;

  const mojom::RequestMode request_mode_;
  const mojom::CredentialsMode request_credentials_mode_;

  const bool has_user_activation_ = false;

  const mojom::RequestDestination request_destination_ =
      mojom::RequestDestination::kEmpty;

  scoped_refptr<ResourceSchedulerClient> resource_scheduler_client_;

  base::WeakPtr<KeepaliveStatisticsRecorder> keepalive_statistics_recorder_;

  bool first_auth_attempt_ = true;

  std::unique_ptr<ScopedThrottlingToken> throttling_token_;

  const net::HttpRequestHeaders custom_proxy_pre_cache_headers_;
  const net::HttpRequestHeaders custom_proxy_post_cache_headers_;

  // Indicates the originating frame of the request, see
  // network::ResourceRequest::fetch_window_id for details.
  const std::optional<base::UnguessableToken> fetch_window_id_;

  PrivateNetworkAccessChecker private_network_access_checker_;

  mojo::Remote<mojom::TrustedHeaderClient> header_client_;

  std::unique_ptr<FileOpenerForUpload> file_opener_for_upload_;

  // If the request is configured for Trust Tokens
  // (https://github.com/WICG/trust-token-api) protocol operations, annotates
  // the request with the pertinent request headers and, on receiving the
  // corresponding response, processes and strips Trust Tokens response headers.
  //
  // For requests configured for Trust Tokens operations, |trust_token_helper_|
  // is constructed (using |trust_token_helper_factory_|) just before the
  // outbound (Begin) operation; for requests without associated Trust Tokens
  // operations, the field remains null, as does |trust_token_helper_factory_|.
  std::unique_ptr<TrustTokenRequestHelper> trust_token_helper_;
  std::unique_ptr<TrustTokenRequestHelperFactory> trust_token_helper_factory_;

  // The current Trust Token operation being processed by the request.
  std::optional<mojom::TrustTokenOperationType> trust_token_operation_;

  // The cached result of the request's Trust Tokens protocol operation, if any.
  // This can describe the result of either an outbound (request-annotating)
  // protocol step or an inbound (response header reading) step; some error
  // codes, like kFailedPrecondition (outbound) and kBadResponse (inbound) are
  // specific to one direction.
  std::optional<mojom::TrustTokenOperationStatus> trust_token_status_;

  // Whether the caller has opted into using the Storage Access API (via JS).
  const net::StorageAccessApiStatus storage_access_api_status_;

  // This is used to determine whether it is allowed to use a dictionary when
  // there is a matching shared dictionary for the request.
  std::unique_ptr<SharedDictionaryAccessChecker> shared_dictionary_checker_;

  // Request helper responsible for orchestrating Attribution operations
  // (https://github.com/WICG/attribution-reporting-api). Only set if the
  // request is related to attribution.
  std::unique_ptr<AttributionRequestHelper> attribution_request_helper_;

  // The `SharedStorageRequestHelper` takes a callback to trigger
  // `ContinueOnReceiveRedirect()`. To prevent re-entrancy, however, this
  // callback is conditionally run only if the helper successfully parses
  // operations and sends them via mojo to observer(s). We stash here the
  // non-copyable `mojom::URLResponseHeadPtr` to which
  // `ContinueOnReceiveRedirect()` needs access and pass the response's index in
  // the map as a parameter.
  std::map<uint64_t, mojom::URLResponseHeadPtr> on_receive_redirect_responses_;
  uint64_t next_on_receive_redirect_response_index_ = 0;

  // Outlives `this`.
  const raw_ref<const cors::OriginAccessList> origin_access_list_;

  // Observers bound to this specific URLLoader. There may be observers bound to
  // an URLLoaderFactory as well so these `mojo::Remote`s should not be used
  // directly, but the pointer fields should be used instead (e.g.
  // `cookie_observer_` should be used since, it can be set to *either*
  // `cookie_observer_.get()` *or* is can be pointing to some other
  // CookieAccessObserver implementation from the URLLoaderContext aka
  // URLLoaderFactory).
  const mojo::Remote<mojom::CookieAccessObserver> cookie_observer_remote_;
  const raw_ptr<mojom::CookieAccessObserver> cookie_observer_ = nullptr;
  const mojo::Remote<mojom::TrustTokenAccessObserver>
      trust_token_observer_remote_;
  const raw_ptr<mojom::TrustTokenAccessObserver> trust_token_observer_ =
      nullptr;
  const mojo::Remote<mojom::URLLoaderNetworkServiceObserver>
      url_loader_network_observer_remote_;
  const raw_ptr<mojom::URLLoaderNetworkServiceObserver>
      url_loader_network_observer_ = nullptr;
  const mojo::Remote<mojom::DevToolsObserver> devtools_observer_remote_;
  const raw_ptr<mojom::DevToolsObserver> devtools_observer_ = nullptr;

  // Request helper responsible for processing Shared Storage headers
  // (https://github.com/WICG/shared-storage#from-response-headers).
  std::unique_ptr<SharedStorageRequestHelper> shared_storage_request_helper_;

  // Indicates |url_request_| is fetch upload request and that has streaming
  // body.
  const bool has_fetch_streaming_upload_body_;

  bool emitted_devtools_raw_request_ = false;
  bool emitted_devtools_raw_response_ = false;

  mojo::Remote<mojom::AcceptCHFrameObserver> accept_ch_frame_observer_;

  // Stores cookies passed from the browser process to later add them to the
  // request. This prevents the network stack from overriding them.
  const bool allow_cookies_from_browser_ = false;
  std::string cookies_from_browser_;

  // Specifies that the response head should include request cookies.
  const bool include_request_cookies_with_response_ = false;
  net::cookie_util::ParsedRequestCookies request_cookies_;

  std::vector<network::mojom::CookieAccessDetailsPtr> cookie_access_details_;

  const bool provide_data_use_updates_;

  // A SlopBucket is used as temporary storage for response body data from
  // high-priority requests that cannot yet be written to the mojo data pipe
  // because it is full.
  std::unique_ptr<SlopBucket> slop_bucket_;

  // Keeps the result of IsSharedDictionaryReadAllowed(). Used only for metrics.
  bool shared_dictionary_allowed_check_passed_ = false;

  base::WeakPtrFactory<URLLoader> weak_ptr_factory_{this};
};

}  // namespace network

#endif  // SERVICES_NETWORK_URL_LOADER_H_
