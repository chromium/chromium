// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_URL_LOADER_INTERCEPTOR_H_
#define CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_URL_LOADER_INTERCEPTOR_H_

#include <optional>

#include "base/containers/enum_set.h"
#include "base/containers/flat_set.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/unguessable_token.h"
#include "content/browser/devtools/protocol/fetch.h"
#include "content/browser/devtools/protocol/network.h"
#include "content/common/content_export.h"
#include "content/public/browser/global_request_id.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "net/base/auth.h"
#include "net/base/net_errors.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "third_party/blink/public/mojom/loader/resource_load_info.mojom-shared.h"

namespace net {
class AuthChallengeInfo;
class HttpResponseHeaders;
}  // namespace net

namespace network {
namespace mojom {
class URLLoaderFactoryOverride;
class TrustedURLLoaderHeaderClient;
}
}  // namespace network

namespace content {

class InterceptionJob;
class StoragePartition;
struct CreateLoaderParameters;

struct CONTENT_EXPORT InterceptedRequestInfo {
  InterceptedRequestInfo();
  ~InterceptedRequestInfo();

  std::string interception_id;
  base::UnguessableToken frame_id;
  blink::mojom::ResourceType resource_type;
  bool is_navigation = false;
  int response_error_code = net::OK;
  std::unique_ptr<protocol::Network::Request> network_request;
  std::unique_ptr<net::AuthChallengeInfo> auth_challenge;
  scoped_refptr<net::HttpResponseHeaders> response_headers;
  std::optional<bool> is_download;
  std::optional<protocol::String> redirect_url;
  std::optional<protocol::String> renderer_request_id;
  std::optional<protocol::String> redirected_request_id;
};

class CONTENT_EXPORT DevToolsURLLoaderInterceptor {
 public:
  using RequestInterceptedCallback =
      base::RepeatingCallback<void(std::unique_ptr<InterceptedRequestInfo>)>;
  using ContinueInterceptedRequestCallback =
      protocol::Fetch::Backend::ContinueRequestCallback;
  using GetResponseBodyForInterceptionCallback =
      protocol::Fetch::Backend::GetResponseBodyCallback;
  using TakeResponseBodyPipeCallback =
      base::OnceCallback<void(protocol::Response,
                              mojo::ScopedDataPipeConsumerHandle,
                              const std::string& mime_type)>;

  struct CONTENT_EXPORT AuthChallengeResponse {
    enum ResponseType {
      kDefault,
      kCancelAuth,
      kProvideCredentials,
    };

    explicit AuthChallengeResponse(ResponseType response_type);
    AuthChallengeResponse(const std::u16string& username,
                          const std::u16string& password);

    AuthChallengeResponse(const AuthChallengeResponse&) = delete;
    AuthChallengeResponse& operator=(const AuthChallengeResponse&) = delete;

    const ResponseType response_type;
    const net::AuthCredentials credentials;
  };

  struct CONTENT_EXPORT Modifications {
    using HeadersVector = std::vector<std::pair<std::string, std::string>>;

    Modifications();
    explicit Modifications(net::Error error_reason);
    explicit Modifications(
        std::unique_ptr<AuthChallengeResponse> auth_challenge_response);
    Modifications(scoped_refptr<net::HttpResponseHeaders> response_headers,
                  scoped_refptr<base::RefCountedMemory> response_body);
    Modifications(std::optional<std::string> modified_url,
                  std::optional<std::string> modified_method,
                  std::optional<protocol::Binary> modified_post_data,
                  std::unique_ptr<HeadersVector> modified_headers,
                  std::optional<bool> intercept_response);
    Modifications(
        std::optional<net::Error> error_reason,
        scoped_refptr<net::HttpResponseHeaders> response_headers,
        scoped_refptr<base::RefCountedMemory> response_body,
        size_t body_offset,
        std::optional<std::string> modified_url,
        std::optional<std::string> modified_method,
        std::optional<protocol::Binary> modified_post_data,
        std::unique_ptr<HeadersVector> modified_headers,
        std::unique_ptr<AuthChallengeResponse> auth_challenge_response);
    ~Modifications();

    // If none of the following are set then the request will be allowed to
    // continue unchanged.
    std::optional<net::Error> error_reason;  // Finish with error.
    // If either of the below fields is set, complete the request by
    // responding with the provided headers and body.
    scoped_refptr<net::HttpResponseHeaders> response_headers;
    scoped_refptr<base::RefCountedMemory> response_body;
    size_t body_offset = 0;

    // Optionally modify before sending to network.
    std::optional<std::string> modified_url;
    std::optional<std::string> modified_method;
    std::optional<protocol::Binary> modified_post_data;
    std::unique_ptr<HeadersVector> modified_headers;
    std::optional<bool> intercept_response;
    // AuthChallengeResponse is mutually exclusive with the above.
    std::unique_ptr<AuthChallengeResponse> auth_challenge_response;
  };

  enum InterceptionStage {
    kRequest,
    kResponse,
    kMinValue = kRequest,
    kMaxValue = kResponse,
  };

  struct CONTENT_EXPORT Pattern {
   public:
    ~Pattern();
    Pattern(const Pattern& other);
    Pattern(const std::string& url_pattern,
            base::flat_set<blink::mojom::ResourceType> resource_types,
            InterceptionStage interception_stage);

    bool Matches(const std::string& url,
                 blink::mojom::ResourceType resource_type) const;

    const std::string url_pattern;
    const base::flat_set<blink::mojom::ResourceType> resource_types;
    const InterceptionStage interception_stage;
  };

  struct FilterEntry {
    FilterEntry(const base::UnguessableToken& target_id,
                std::vector<Pattern> patterns,
                RequestInterceptedCallback callback);
    FilterEntry(FilterEntry&&);

    FilterEntry(const FilterEntry&) = delete;
    FilterEntry& operator=(const FilterEntry&) = delete;

    ~FilterEntry();

    const base::UnguessableToken target_id;
    std::vector<Pattern> patterns;
    const RequestInterceptedCallback callback;
  };

  // Called with (use_fallback, credentials). If `use_fallback` is true,
  // DevTools declines to handle the challenge and the network stack should
  // proceed with default auth handling. If false, `credentials` contains
  // supplied credentials or std::nullopt if authentication was canceled.
  using HandleAuthRequestCallback =
      base::OnceCallback<void(bool use_fallback,
                              const std::optional<net::AuthCredentials>&)>;
  using CheckCookieAccessCallback =
      base::RepeatingCallback<bool(const net::CanonicalCookie&)>;
  // Routes an authentication challenge to the innermost interceptor with auth
  // handling enabled for the matching request. Falls back to default handling
  // if no interceptors handle auth or no matching job exists.
  // Can only be called on the UI thread.
  static void HandleAuthRequest(GlobalRequestID req_id,
                                const net::AuthChallengeInfo& auth_info,
                                HandleAuthRequestCallback callback);

  explicit DevToolsURLLoaderInterceptor(
      RequestInterceptedCallback callback,
      CheckCookieAccessCallback cookie_access_callback = {});

  DevToolsURLLoaderInterceptor(const DevToolsURLLoaderInterceptor&) = delete;
  DevToolsURLLoaderInterceptor& operator=(const DevToolsURLLoaderInterceptor&) =
      delete;

  ~DevToolsURLLoaderInterceptor();

  void SetPatterns(std::vector<Pattern> patterns, bool handle_auth);

  void GetResponseBody(
      const std::string& interception_id,
      std::unique_ptr<GetResponseBodyForInterceptionCallback> callback);
  void TakeResponseBodyPipe(const std::string& interception_id,
                            TakeResponseBodyPipeCallback callback);
  void ContinueInterceptedRequest(
      const std::string& interception_id,
      std::unique_ptr<Modifications> modifications,
      std::unique_ptr<ContinueInterceptedRequestCallback> callback);

  bool CreateProxyForInterception(
      int process_id,
      StoragePartition* storage_partition,
      const base::UnguessableToken& frame_token,
      bool is_navigation,
      bool is_download,
      network::mojom::URLLoaderFactoryOverride* intercepting_factory,
      mojo::PendingRemote<network::mojom::TrustedURLLoaderHeaderClient>*
          header_client);

 private:
  friend class InterceptionJob;
  friend class DevToolsURLLoaderFactoryProxy;

  using InterceptionStages = base::EnumSet<InterceptionStage>;

  void CreateJob(
      const base::UnguessableToken& frame_token,
      int32_t process_id,
      bool is_download,
      const std::optional<std::string>& renderer_request_id,
      std::unique_ptr<CreateLoaderParameters> create_params,
      mojo::PendingReceiver<network::mojom::URLLoader> loader_receiver,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client,
      mojo::PendingRemote<network::mojom::URLLoaderFactory> target_factory,
      mojo::PendingRemote<network::mojom::CookieManager> cookie_manager);

  InterceptionStages GetInterceptionStages(
      const GURL& url,
      blink::mojom::ResourceType resource_type) const;

  template <typename Callback>
  InterceptionJob* FindJob(const std::string& id,
                           std::unique_ptr<Callback>* callback) {
    auto it = jobs_.find(id);
    if (it != jobs_.end())
      return it->second;
    (*callback)->sendFailure(
        protocol::Response::InvalidParams("Invalid InterceptionId."));
    return nullptr;
  }

  // Looks up an active InterceptionJob by its originating process and network
  // request ID. Used for duplicate collision checks and NetworkService header
  // client callbacks where only request_id is known.
  InterceptionJob* FindJobByGlobalId(const GlobalRequestID& global_req_id) {
    auto it = jobs_by_global_req_id_.find(global_req_id);
    return it == jobs_by_global_req_id_.end() ? nullptr : it->second;
  }

  void RemoveJob(const GlobalRequestID& global_req_id, const std::string& id) {
    jobs_by_global_req_id_.erase(global_req_id);
    jobs_.erase(id);
  }
  void AddJob(const GlobalRequestID& global_req_id,
              const std::string& id,
              InterceptionJob* job) {
    jobs_.emplace(id, job);
    jobs_by_global_req_id_.emplace(global_req_id, job);
  }

  // Registers/unregisters an InterceptionJob with the process-global in-flight
  // job stack map used for routing network-originating auth challenges.
  static void RegisterJob(InterceptionJob* job);
  static void UnregisterJob(InterceptionJob* job);

  const RequestInterceptedCallback request_intercepted_callback_;
  const CheckCookieAccessCallback cookie_access_callback_;

  std::vector<Pattern> patterns_;
  bool handle_auth_ = false;
  // Maps DevTools protocol interception IDs to active jobs.
  std::map<std::string, raw_ptr<InterceptionJob, CtnExperimental>> jobs_;
  // Maps originating (process_id, request_id) to active jobs for collision
  // checks and routing network service callbacks where only request_id is
  // known.
  std::map<GlobalRequestID, raw_ptr<InterceptionJob, CtnExperimental>>
      jobs_by_global_req_id_;

  base::WeakPtrFactory<DevToolsURLLoaderInterceptor> weak_factory_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_URL_LOADER_INTERCEPTOR_H_
