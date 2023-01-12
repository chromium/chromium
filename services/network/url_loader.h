// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_URL_LOADER_H_
#define SERVICES_NETWORK_URL_LOADER_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
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
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "net/url_request/url_request.h"
#include "services/network/keepalive_statistics_recorder.h"
#include "services/network/network_service.h"
#include "services/network/network_service_memory_cache.h"
#include "services/network/private_network_access_checker.h"
#include "services/network/public/cpp/corb/corb_api.h"
#include "services/network/public/cpp/cors/cors_error_status.h"
#include "services/network/public/cpp/initiator_lock_compatibility.h"
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
#include "services/network/resource_scheduler/resource_scheduler.h"
#include "services/network/resource_scheduler/resource_scheduler_client.h"
#include "services/network/trust_tokens/pending_trust_token_store.h"
#include "services/network/trust_tokens/trust_token_request_helper.h"
#include "services/network/trust_tokens/trust_token_request_helper_factory.h"
#include "services/network/upload_progress_tracker.h"
#include "services/network/url_loader_context.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace net {
class HttpResponseHeaders;
class IPEndPoint;
struct RedirectInfo;
class URLRequestContext;
}  // namespace net

namespace network {

namespace cors {
class OriginAccessList;
}

constexpr size_t kMaxFileUploadRequestsPerBatch = 64;

class CacheTransparencySettings;
class KeepaliveStatisticsRecorder;
class NetToMojoPendingBuffer;
class ScopedThrottlingToken;
class URLLoaderFactory;

// When a request matches a pervasive payload url and checksum a value from this
// enum will be logged to the "Network.CacheTransparency.CacheNotUsed"
// histogram. These values are persisted to logs. Entries should not be
// renumbered and numeric values should never be reused. This is exposed in the
// header file for use in tests.
enum class CacheTransparencyCacheNotUsedReason {
  kTryingSingleKeyedCache = 0,
  kIncompatibleRequestType = 1,
  kIncompatibleRequestLoadFlags = 2,
  kIncompatibleRequestHeaders = 3,
  kMaxValue = kIncompatibleRequestHeaders,
};

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
      uint32_t request_id,
      int keepalive_request_size,
      base::WeakPtr<KeepaliveStatisticsRecorder> keepalive_statistics_recorder,
      std::unique_ptr<TrustTokenRequestHelperFactory>
          trust_token_helper_factory,
      mojo::PendingRemote<mojom::CookieAccessObserver> cookie_observer,
      mojo::PendingRemote<mojom::TrustTokenAccessObserver> trust_token_observer,
      mojo::PendingRemote<mojom::URLLoaderNetworkServiceObserver>
          url_loader_network_observer,
      mojo::PendingRemote<mojom::DevToolsObserver> devtools_observer,
      mojo::PendingRemote<mojom::AcceptCHFrameObserver>
          accept_ch_frame_observer,
      bool third_party_cookies_enabled,
      const CacheTransparencySettings* cache_transparency_settings);

  URLLoader(const URLLoader&) = delete;
  URLLoader& operator=(const URLLoader&) = delete;

  ~URLLoader() override;

  void SetMemoryCache(base::WeakPtr<NetworkServiceMemoryCache> memory_cache) {
    memory_cache_ = std::move(memory_cache);
  }

  // mojom::URLLoader implementation:
  void FollowRedirect(
      const std::vector<std::string>& removed_headers,
      const net::HttpRequestHeaders& modified_headers,
      const net::HttpRequestHeaders& modified_cors_exempt_headers,
      const absl::optional<GURL>& new_url) override;
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
      absl::optional<GURL>* preserve_fragment_on_redirect_url);

  mojom::URLLoaderNetworkServiceObserver* GetURLLoaderNetworkServiceObserver()
      const {
    return url_loader_network_observer_;
  }

  // mojom::AuthChallengeResponder:
  void OnAuthCredentials(
      const absl::optional<net::AuthCredentials>& credentials) override;

  // mojom::ClientCertificateResponder:
  void ContinueWithCertificate(
      const scoped_refptr<net::X509Certificate>& x509_certificate,
      const std::string& provider_name,
      const std::vector<uint16_t>& algorithm_preferences,
      mojo::PendingRemote<mojom::SSLPrivateKey> ssl_private_key) override;
  void ContinueWithoutCertificate() override;
  void CancelRequest() override;

  net::LoadState GetLoadState() const;
  net::UploadProgress GetUploadProgress() const;

  int32_t GetProcessId() const;
  uint32_t GetResourceType() const;

  // Whether this URLLoader should allow sending/setting cookies for requests
  // with |url| and |site_for_cookies|. This decision is based on the options
  // passed to URLLoaderFactory::CreateLoaderAndStart().
  bool AllowCookies(const GURL& url,
                    const net::SiteForCookies& site_for_cookies) const;

  const net::HttpRequestHeaders& custom_proxy_pre_cache_headers() const {
    return custom_proxy_pre_cache_headers_;
  }

  const net::HttpRequestHeaders& custom_proxy_post_cache_headers() const {
    return custom_proxy_post_cache_headers_;
  }

  const absl::optional<GURL>& new_redirect_url() const {
    return new_redirect_url_;
  }

  const absl::optional<std::string>& devtools_request_id() const {
    return devtools_request_id_;
  }

  void SetEnableReportingRawHeaders(bool enable);

  mojom::LoadInfoPtr CreateLoadInfo();

  // Gets the URLLoader associated with this request.
  static URLLoader* ForRequest(const net::URLRequest& request);

  static const void* const kUserDataKey;

  static bool HasFetchStreamingUploadBody(const ResourceRequest*);

  static absl::optional<net::IsolationInfo> GetIsolationInfo(
      const net::IsolationInfo& factory_isolation_info,
      bool automatically_assign_isolation_info,
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

  void OpenFilesForUpload(const ResourceRequest& request);
  void SetUpUpload(const ResourceRequest& request,
                   int error_code,
                   const std::vector<base::File> opened_files);

  // A request with Trust Tokens parameters will (assuming preconditions pass
  // and operations are successful) have one TrustTokenRequestHelper::Begin
  // executed against the request and one TrustTokenRequestHelper::Finalize
  // executed against its response.
  //
  // Outbound control flow:
  //
  // Start in BeginTrustTokenOperationIfNecessaryAndThenScheduleStart
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
  //   - receive the result in OnDoneFinalizingTrusttokenOperation;
  //   - if successful, ContinueOnResponseStarted; if there was an error, fail.
  void BeginTrustTokenOperationIfNecessaryAndThenScheduleStart(
      const ResourceRequest& request);
  void OnDoneConstructingTrustTokenHelper(
      mojom::TrustTokenOperationType type,
      TrustTokenStatusOrRequestHelper status_or_helper);
  void OnDoneBeginningTrustTokenOperation(
      absl::optional<net::HttpRequestHeaders> headers,
      mojom::TrustTokenOperationStatus status);
  void OnDoneFinalizingTrustTokenOperation(
      mojom::TrustTokenOperationStatus status);
  // Continuation of |OnResponseStarted| after possibly asynchronously
  // concluding the request's Trust Tokens operation.
  void ContinueOnResponseStarted();
  void MaybeSendTrustTokenOperationResultToDevTools();

  void ScheduleStart();
  void ReadMore();
  void DidRead(int num_bytes, bool completed_synchronously);
  void NotifyCompleted(int error_code);
  void OnMojoDisconnect();
  void OnResponseBodyStreamConsumerClosed(MojoResult result);
  void OnResponseBodyStreamReady(MojoResult result);
  void DeleteSelf();
  void SendResponseToClient();
  void CompletePendingWrite(bool success);
  void SetRawResponseHeaders(scoped_refptr<const net::HttpResponseHeaders>);
  void NotifyEarlyResponse(scoped_refptr<const net::HttpResponseHeaders>);
  void SetRawRequestHeadersAndNotify(net::HttpRawRequestHeaders);
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
      const absl::optional<net::HttpRequestHeaders>& headers);
  void OnHeadersReceivedComplete(
      net::CompletionOnceCallback callback,
      scoped_refptr<net::HttpResponseHeaders>* out_headers,
      absl::optional<GURL>* out_preserve_fragment_on_redirect_url,
      int result,
      const absl::optional<std::string>& headers,
      const absl::optional<GURL>& preserve_fragment_on_redirect_url);

  void CompleteBlockedResponse(
      int error_code,
      bool should_report_corb_blocking,
      absl::optional<mojom::BlockedByResponseReason> reason = absl::nullopt);

  enum BlockResponseForCorbResult {
    // Returned when caller of BlockResponseForCorb doesn't need to continue,
    // because the request will be cancelled soon.
    kWillCancelRequest,

    // Returned when the caller of BlockResponseForCorb should continue
    // processing the request (e.g. by calling ReadMore as necessary).
    kContinueRequest,
  };
  // Block the response because of CORB (or ORB).
  BlockResponseForCorbResult BlockResponseForCorb();
  // Decide whether to call block a response via BlockResponseForCorb.
  // Returns true if the request should be cancelled.
  bool MaybeBlockResponseForCorb(corb::ResponseAnalyzer::Decision);

  void ReportFlaggedResponseCookies();
  void StartReading();

  // Whether `force_ignore_site_for_cookies` should be set on net::URLRequest.
  bool ShouldForceIgnoreSiteForCookies(const ResourceRequest& request);

  // Whether `force_ignore_top_frame_party_for_cookies` should be set on
  // net::URLRequest.
  bool ShouldForceIgnoreTopFramePartyForCookies() const;

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

  // Returns whether sending/storing credentials is allowed by COEP.
  // |url| is the latest request URL, either the original URL or
  // `redirect_info.new_url`.
  // When Cross-Origin-Embedder-Policy: credentialless is set, do not
  // send or store credentials for no-cors cross-origin request.
  bool CoepAllowCredentials(const GURL& url);

  raw_ptr<net::URLRequestContext> url_request_context_;

  raw_ptr<mojom::NetworkContextClient> network_context_client_;
  DeleteCallback delete_callback_;

  int32_t options_;
  const bool corb_detachable_;
  const int resource_type_;
  const bool is_load_timing_enabled_;
  bool has_received_response_ = false;

  // URLLoaderFactory is guaranteed to outlive URLLoader, so it is safe to
  // store a raw pointer to mojom::URLLoaderFactoryParams.
  const raw_ref<const mojom::URLLoaderFactoryParams> factory_params_;
  // This also belongs to URLLoaderFactory and outlives this loader.
  const raw_ptr<mojom::CrossOriginEmbedderPolicyReporter> coep_reporter_;

  const uint32_t request_id_;
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

  // True if there's a URLRequest::Read() call in progress.
  bool read_in_progress_ = false;

  // Stores any CORS error encountered while processing |url_request_|.
  absl::optional<CorsErrorStatus> cors_error_status_;

  // True if a pervasive payload is found, for logging purposes.
  bool pervasive_payload_requested_ = false;

  // Used when deferring sending the data to the client until mime sniffing is
  // finished.
  mojom::URLResponseHeadPtr response_;
  mojo::ScopedDataPipeConsumerHandle consumer_handle_;

  // Sniffing state and CORB state.
  std::unique_ptr<corb::ResponseAnalyzer> corb_analyzer_;
  bool is_more_corb_sniffing_needed_ = false;
  bool is_more_mime_sniffing_needed_ = false;
  const raw_ref<corb::PerFactoryState> per_factory_corb_state_;

  std::unique_ptr<ResourceScheduler::ScheduledResourceRequest>
      resource_scheduler_request_handle_;

  bool enable_reporting_raw_headers_ = false;
  bool seen_raw_request_headers_ = false;
  scoped_refptr<const net::HttpResponseHeaders> raw_response_headers_;

  std::unique_ptr<UploadProgressTracker> upload_progress_tracker_;

  // Holds the URL of a redirect if it's currently deferred.
  std::unique_ptr<GURL> deferred_redirect_url_;

  // If |new_url| is given to FollowRedirect() it's saved here, so that it can
  // be later referred to from NetworkContext::OnBeforeURLRequestInternal, which
  // is called from NetworkDelegate::NotifyBeforeURLRequest.
  absl::optional<GURL> new_redirect_url_;

  // The ID that DevTools uses to track network requests. It is generated in the
  // renderer process and is only present when DevTools is enabled in the
  // renderer.
  const absl::optional<std::string> devtools_request_id_;

  bool should_pause_reading_body_ = false;
  // The response body stream is open, but transferring data is paused.
  bool paused_reading_body_ = false;

  // This is used to compute the delta since last time received
  // encoded body size was reported to the client.
  int64_t reported_total_encoded_bytes_ = 0;

  const mojom::RequestMode request_mode_;
  const mojom::CredentialsMode request_credentials_mode_;

  bool has_user_activation_ = false;

  mojom::RequestDestination request_destination_ =
      mojom::RequestDestination::kEmpty;

  scoped_refptr<ResourceSchedulerClient> resource_scheduler_client_;

  base::WeakPtr<KeepaliveStatisticsRecorder> keepalive_statistics_recorder_;

  base::WeakPtr<NetworkServiceMemoryCache> memory_cache_;
  std::unique_ptr<NetworkServiceMemoryCacheWriter> memory_cache_writer_;
  // Passed to `memory_cache_writer_`. Do not use other purposes.
  net::TransportInfo transport_info_;

  bool first_auth_attempt_ = true;

  std::unique_ptr<ScopedThrottlingToken> throttling_token_;

  net::HttpRequestHeaders custom_proxy_pre_cache_headers_;
  net::HttpRequestHeaders custom_proxy_post_cache_headers_;

  // Indicates the originating frame of the request, see
  // network::ResourceRequest::fetch_window_id for details.
  absl::optional<base::UnguessableToken> fetch_window_id_;

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

  // The cached result of the request's Trust Tokens protocol operation, if any.
  // This can describe the result of either an outbound (request-annotating)
  // protocol step or an inbound (response header reading) step; some error
  // codes, like kFailedPrecondition (outbound) and kBadResponse (inbound) are
  // specific to one direction.
  absl::optional<mojom::TrustTokenOperationStatus> trust_token_status_;

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

  const raw_ptr<const CacheTransparencySettings> cache_transparency_settings_;

  // Indicates |url_request_| is fetch upload request and that has streaming
  // body.
  const bool has_fetch_streaming_upload_body_;

  // Indicates whether fetch upload streaming is allowed/rejected over H/1.
  // Even if this is false but there is a QUIC/H2 stream, the upload is allowed.
  const bool allow_http1_for_streaming_upload_;

  bool emitted_devtools_raw_request_ = false;
  bool emitted_devtools_raw_response_ = false;

  mojo::Remote<mojom::AcceptCHFrameObserver> accept_ch_frame_observer_;

  // Stores cookies passed from the browser process to later add them to the
  // request. This prevents the network stack from overriding them.
  bool allow_cookies_from_browser_ = false;
  std::string cookies_from_browser_;

  base::WeakPtrFactory<URLLoader> weak_ptr_factory_{this};
};

}  // namespace network

#endif  // SERVICES_NETWORK_URL_LOADER_H_
