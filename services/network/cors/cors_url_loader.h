// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_CORS_CORS_URL_LOADER_H_
#define SERVICES_NETWORK_CORS_CORS_URL_LOADER_H_

#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/types/strong_alias.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/cookies/cookie_setting_override.h"
#include "net/log/net_log_with_source.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "net/url_request/redirect_info.h"
#include "services/network/cors/preflight_controller.h"
#include "services/network/public/cpp/cors/cors_error_status.h"
#include "services/network/public/cpp/cross_origin_embedder_policy.h"
#include "services/network/public/mojom/client_security_state.mojom-forward.h"
#include "services/network/public/mojom/devtools_observer.mojom.h"
#include "services/network/public/mojom/fetch_api.mojom.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "services/network/public/mojom/url_loader_completion_status.mojom.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace net {
class SharedDictionary;
}  // namespace net

namespace network {
namespace mojom {
enum class SharedDictionaryError : int32_t;
}  // namespace mojom

class URLLoaderFactory;
class NetworkContext;
class SharedDictionaryStorage;
class SharedDictionaryDataPipeWriter;

namespace cors {

class OriginAccessList;

using HasFactoryOverride = base::StrongAlias<class HasFactoryOverrideTag, bool>;

// Wrapper class that adds cross-origin resource sharing capabilities
// (https://fetch.spec.whatwg.org/#http-cors-protocol), delegating requests as
// well as potential preflight requests to the supplied
// `network_loader_factory`. It is owned by the CorsURLLoaderFactory that
// created it.
class COMPONENT_EXPORT(NETWORK_SERVICE) CorsURLLoader
    : public mojom::URLLoader,
      public mojom::URLLoaderClient {
 public:
  using DeleteCallback = base::OnceCallback<void(CorsURLLoader* loader)>;

  // Raw pointer arguments must outlive the returned instance.
  // note: `url_loader_network_service_observer` must not be null.
  CorsURLLoader(
      mojo::PendingReceiver<mojom::URLLoader> loader_receiver,
      int32_t process_id,
      int32_t request_id,
      uint32_t options,
      DeleteCallback delete_callback,
      ResourceRequest resource_request,
      bool ignore_isolated_world_origin,
      bool skip_cors_enabled_scheme_check,
      mojo::PendingRemote<mojom::URLLoaderClient> client,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation,
      mojom::URLLoaderFactory* network_loader_factory,
      URLLoaderFactory* sync_network_loader_factory,
      const OriginAccessList* origin_access_list,
      bool allow_any_cors_exempt_header,
      HasFactoryOverride has_factory_override,
      const net::IsolationInfo& isolation_info,
      mojo::PendingRemote<mojom::DevToolsObserver> devtools_observer,
      const mojom::ClientSecurityState* factory_client_security_state,
      mojo::Remote<mojom::URLLoaderNetworkServiceObserver>*
          url_loader_network_service_observer,
      const CrossOriginEmbedderPolicy& cross_origin_embedder_policy,
      scoped_refptr<SharedDictionaryStorage> shared_dictionary_storage,
      raw_ptr<mojom::SharedDictionaryAccessObserver> shared_dictionary_observer,
      NetworkContext* context,
      net::CookieSettingOverrides factory_cookie_setting_overrides);

  CorsURLLoader(const CorsURLLoader&) = delete;
  CorsURLLoader& operator=(const CorsURLLoader&) = delete;

  ~CorsURLLoader() override;

  // Starts processing the request. This is expected to be called right after
  // the constructor.
  void Start();

  // mojom::URLLoader overrides:
  void FollowRedirect(
      const std::vector<std::string>& removed_headers,
      const net::HttpRequestHeaders& modified_headers,
      const net::HttpRequestHeaders& modified_cors_exempt_headers,
      const std::optional<GURL>& new_url) override;
  void SetPriority(net::RequestPriority priority,
                   int intra_priority_value) override;
  void PauseReadingBodyFromNet() override;
  void ResumeReadingBodyFromNet() override;

  // mojom::URLLoaderClient overrides:
  void OnReceiveEarlyHints(mojom::EarlyHintsPtr early_hints) override;
  void OnReceiveResponse(
      mojom::URLResponseHeadPtr head,
      mojo::ScopedDataPipeConsumerHandle body,
      std::optional<mojo_base::BigBuffer> cached_metadata) override;
  void OnReceiveRedirect(const net::RedirectInfo& redirect_info,
                         mojom::URLResponseHeadPtr head) override;
  void OnUploadProgress(int64_t current_position,
                        int64_t total_size,
                        base::OnceCallback<void()> callback) override;
  void OnTransferSizeUpdated(int32_t transfer_size_diff) override;
  void OnComplete(const URLLoaderCompletionStatus& status) override;

  // Cancel the request because network revocation was triggered.
  void CancelRequestIfNonceMatchesAndUrlNotExempted(
      const base::UnguessableToken& nonce,
      const std::set<GURL>& exemptions);

  static network::mojom::FetchResponseType CalculateResponseTaintingForTesting(
      const GURL& url,
      mojom::RequestMode request_mode,
      const std::optional<url::Origin>& origin,
      const std::optional<url::Origin>& isolated_world_origin,
      bool cors_flag,
      bool tainted_origin,
      const OriginAccessList& origin_access_list);

  static std::optional<CorsErrorStatus> CheckRedirectLocationForTesting(
      const GURL& url,
      mojom::RequestMode request_mode,
      const std::optional<url::Origin>& origin,
      bool cors_flag,
      bool tainted);

 private:
  void StartRequest();

  // Helper for `OnPreflightRequestComplete()`.
  std::optional<URLLoaderCompletionStatus> ConvertPreflightResult(
      int net_error,
      std::optional<CorsErrorStatus> status);

  void OnPreflightRequestComplete(int net_error,
                                  std::optional<CorsErrorStatus> status,
                                  bool has_authorization_covered_by_wildcard);
  void StartNetworkRequest();

  // Called when there is a connection error on the upstream pipe used for the
  // actual request.
  void OnUpstreamConnectionError();

  // Reports the error represented by `status` to DevTools, if possible.
  // If `is_warning` is true, then the error is only reported as a warning.
  void ReportCorsErrorToDevTools(const CorsErrorStatus& status,
                                 bool is_warning = false);

  // Reports an ORB error for `request_` to DevTools, if possible.
  void ReportOrbErrorToDevTools();

  // Reports an SharedDictionaryError for `request_` to DevTools, if possible.
  void MaybeReportSharedDictionaryErrorToDevTools(
      mojom::SharedDictionaryError error);

  // Handles OnComplete() callback.
  void HandleComplete(URLLoaderCompletionStatus status);

  void OnMojoDisconnect();

  void OnNetworkClientMojoDisconnect();

  void SetCorsFlagIfNeeded();

  // Returns true if request's origin has special access to the destination URL
  // (via `origin_access_list_`).
  bool HasSpecialAccessToDestination() const;

  bool PassesTimingAllowOriginCheck(
      const mojom::URLResponseHead& response) const;

  bool PassesTimingAllowOriginCheck(const mojom::URLResponseHead& response,
                                    const GURL&) const;

  void CheckTainted(const net::RedirectInfo&);

  // Returns the client security state that applies to this request.
  // May return nullptr.
  const mojom::ClientSecurityState* GetClientSecurityState() const;

  // Returns a clone of the value returned by `GetClientSecurityState()`.
  mojom::ClientSecurityStatePtr CloneClientSecurityState() const;

  // Returns whether preflight errors due exclusively to Private Network Access
  // checks should be ignored.
  //
  // This is used to soft-launch Private Network Access preflights: we send
  // preflights but do not require them to succeed.
  //
  // TODO(crbug.com/40204695): Remove this once preflight enforcement
  // is enabled.
  bool ShouldIgnorePrivateNetworkAccessErrors(
      mojom::IPAddressSpace target_address_space) const;

  // Returns the PNA-specific behavior to apply to the next preflight request.
  //
  // This is used to soft-launch Private Network Access preflights: we send
  // preflights but do not require them to succeed.
  //
  // TODO(crbug.com/40204695): Remove this once preflight enforcement
  // is enabled.
  PrivateNetworkAccessPreflightBehavior
  GetPrivateNetworkAccessPreflightBehavior(
      mojom::IPAddressSpace target_address_space) const;

  // Returns `pna_preflight_result_`'s value, then resets it.
  mojom::PrivateNetworkAccessPreflightResult
  TakePrivateNetworkAccessPreflightResult();

  static std::optional<std::string> GetHeaderString(
      const mojom::URLResponseHead& response,
      const std::string& header_name);

  // Precondition: The request is cross-origin with destination
  // `mojom::RequestDestination::kSharedStorageWorklet`.
  //
  // If the "Sec-Shared-Storage-Data-Origin" request header has not been set,
  // then returns true (as no shared storage response header is required).
  //
  // Otherwise, there is a value for the "Sec-Shared-Storage-Data-Origin"
  // request header. Parses this request header value into a URL. CHECKs that
  // the parsed data origin URL is valid and same-origin to the request's URL
  // (as regular JavaScript is unable modify this forbidden header, and any
  // modifications modae by extensions will not be propagated back to the
  // request' sheaders here).
  //
  // Parses the "Shared-Storage-Cross-Origin-Worklet-Allowed" response
  // header into a Structured Fields Boolean, and returns the result. Returns
  // false if the header does not exist or if the parsing fails.
  bool CheckSharedStorageCrossOriginWorkletAllowedResponseHeaderIfNeeded(
      const mojom::URLResponseHead& response);

  void OnSharedDictionaryWritten(bool success);

  mojo::Receiver<mojom::URLLoader> receiver_;

  // We need to save these for redirect, and DevTools.
  const int32_t process_id_;
  const int32_t request_id_;
  const uint32_t options_;

  DeleteCallback delete_callback_;

  // This raw URLLoaderFactory pointer is shared with the CorsURLLoaderFactory
  // that created and owns this object.
  raw_ptr<mojom::URLLoaderFactory> network_loader_factory_;
  // This has the same lifetime as `network_loader_factory_`, and should be used
  // when non-null to create optimized URLLoaders which can call URLLoaderClient
  // methods synchronously.
  raw_ptr<URLLoaderFactory> sync_network_loader_factory_;

  // For the actual request.
  mojo::Remote<mojom::URLLoader> network_loader_;
  base::TimeTicks network_loader_start_time_;
  // `sync_client_receiver_factory_` should be invalidated if this is ever
  // reset.
  mojo::Receiver<mojom::URLLoaderClient> network_client_receiver_{this};
  ResourceRequest request_;

  // To be a URLLoader for the client.
  mojo::Remote<mojom::URLLoaderClient> forwarding_client_;

  // The last response URL, that is usually the requested URL, but can be
  // different if redirects happen.
  GURL last_response_url_;

  // https://fetch.spec.whatwg.org/#concept-request-response-tainting
  // As "response tainting" is subset of "response type", we use
  // mojom::FetchResponseType for convenience.
  mojom::FetchResponseType response_tainting_ =
      mojom::FetchResponseType::kBasic;

  // Holds the URL of a redirect if it's currently deferred, waiting for
  // forwarding_client_ to call FollowRedirect.
  std::unique_ptr<GURL> deferred_redirect_url_;

  // Corresponds to the Fetch spec, https://fetch.spec.whatwg.org/.
  bool fetch_cors_flag_ = false;

  net::RedirectInfo redirect_info_;

  // https://fetch.spec.whatwg.org/#concept-request-tainted-origin
  bool tainted_ = false;

  // https://fetch.spec.whatwg.org/#concept-request-redirect-count
  int redirect_count_ = 0;

  // Whether we have called `OnReceiveResponse()` on `forwarding_client_` yet.
  bool has_forwarded_response_ = false;

  // https://fetch.spec.whatwg.org/#timing-allow-failed
  bool timing_allow_failed_flag_ = false;

  // We need to save this for redirect.
  net::MutableNetworkTrafficAnnotationTag traffic_annotation_;

  // Outlives `this`.
  const raw_ptr<const OriginAccessList> origin_access_list_;

  // Flag to specify if the CORS-enabled scheme check should be applied.
  const bool skip_cors_enabled_scheme_check_;

  const bool allow_any_cors_exempt_header_;

  const HasFactoryOverride has_factory_override_;

  net::IsolationInfo isolation_info_;

  // The client security state set on the factory that created this loader.
  //
  // If non-null, this is used over any request-specific client security state
  // passed in `request_.trusted_params`.
  //
  // NOTE: A default value is set here in order to avoid any risk of undefined
  // behavior, but it should never be used since the constructor always
  // initializes this member explicitly.
  raw_ptr<const mojom::ClientSecurityState> factory_client_security_state_ =
      nullptr;

  // Observer for this request and any preflight requests we send ahead of it.
  // Owned by the parent `CorsURLLoaderFactory`, never nullptr - though the
  // pointee remote itself may be unbound.
  raw_ptr<mojo::Remote<mojom::URLLoaderNetworkServiceObserver>>
      url_loader_network_service_observer_;

  const CrossOriginEmbedderPolicy cross_origin_embedder_policy_;

  bool has_authorization_covered_by_wildcard_ = false;

  PreflightController::PreflightMode preflight_mode_;

  // Whether the current preflight request is 1) solely sent for PNA, not for
  // CORS and PNA at the same time, and 2) in warning mode.
  //
  // If set to true, then any and all errors raised by subsequent preflight
  // requests are ignored.
  //
  // This is used to soft-launch Private Network Access preflights. In some
  // cases, the only reason we send a preflight is because of Private Network
  // Access. Errors that arise then would never have been noticed if we had not
  // sent the preflight, so we ignore them all.
  //
  // INVARIANT: if this is true, then
  // `ShouldIgnorePrivateNetworkAccessErrors()` is also true.
  //
  // TODO(crbug.com/40204695): Remove this along with
  // `ShouldIgnorePrivateNetworkAccessErrors()`.
  bool sending_pna_only_warning_preflight_ = false;

  // The result of sending a PNA preflight, if any.
  // Set when a PNA preflight completes. Reset and passed to
  // `forwarding_client_` via `URLResponseHead` in `OnReceiveRedirect()` and
  // `OnReceiveResponse()`.
  mojom::PrivateNetworkAccessPreflightResult pna_preflight_result_ =
      mojom::PrivateNetworkAccessPreflightResult::kNone;

  mojo::Remote<mojom::DevToolsObserver> devtools_observer_;
  base::WeakPtrFactory<mojo::Remote<mojom::DevToolsObserver>>
      weak_devtools_observer_factory_;

  net::NetLogWithSource net_log_;

  const raw_ptr<NetworkContext> context_;

  scoped_refptr<SharedDictionaryStorage> shared_dictionary_storage_;
  scoped_refptr<net::SharedDictionary> shared_dictionary_;
  raw_ptr<mojom::SharedDictionaryAccessObserver> shared_dictionary_observer_;
  std::unique_ptr<SharedDictionaryDataPipeWriter>
      shared_dictionary_data_pipe_writer_;
  std::optional<URLLoaderCompletionStatus> deferred_completion_status_;
  const net::CookieSettingOverrides factory_cookie_setting_overrides_;

  // Used to provide weak pointers of this class for synchronously calling
  // URLLoaderClient methods. This should be reset any time
  // `network_client_receiver_` is reset.
  base::WeakPtrFactory<CorsURLLoader> sync_client_receiver_factory_{this};

  // Used to run asynchronous class instance bound callbacks safely.
  base::WeakPtrFactory<CorsURLLoader> weak_factory_{this};
};

}  // namespace cors

}  // namespace network

#endif  // SERVICES_NETWORK_CORS_CORS_URL_LOADER_H_
