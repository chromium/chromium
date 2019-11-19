// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_URL_LOADER_INTERCEPTOR_H_
#define CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_URL_LOADER_INTERCEPTOR_H_

#include "base/callback.h"
#include "base/containers/flat_set.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/unguessable_token.h"
#include "content/browser/devtools/protocol/network.h"
#include "content/public/common/resource_type.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "net/base/auth.h"
#include "net/base/net_errors.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"

namespace net {
class AuthChallengeInfo;
class HttpResponseHeaders;
}  // namespace net

namespace content {

class InterceptionJob;
class RenderProcessHost;
struct CreateLoaderParameters;

struct InterceptedRequestInfo {
  InterceptedRequestInfo();
  ~InterceptedRequestInfo();

  std::string interception_id;
  base::UnguessableToken frame_id;
  ResourceType resource_type;
  bool is_navigation = false;
  int response_error_code = net::OK;
  std::unique_ptr<protocol::Network::Request> network_request;
  std::unique_ptr<net::AuthChallengeInfo> auth_challenge;
  scoped_refptr<net::HttpResponseHeaders> response_headers;
  protocol::Maybe<bool> is_download;
  protocol::Maybe<protocol::String> redirect_url;
  protocol::Maybe<protocol::String> renderer_request_id;
};

class DevToolsURLLoaderInterceptor {
 public:
  using RequestInterceptedCallback =
      base::RepeatingCallback<void(std::unique_ptr<InterceptedRequestInfo>)>;
  using ContinueInterceptedRequestCallback =
      protocol::Network::Backend::ContinueInterceptedRequestCallback;
  using GetResponseBodyForInterceptionCallback =
      protocol::Network::Backend::GetResponseBodyForInterceptionCallback;
  using TakeResponseBodyPipeCallback =
      base::OnceCallback<void(protocol::Response,
                              mojo::ScopedDataPipeConsumerHandle,
                              const std::string& mime_type)>;

  struct AuthChallengeResponse {
    enum ResponseType {
      kDefault,
      kCancelAuth,
      kProvideCredentials,
    };

    explicit AuthChallengeResponse(ResponseType response_type);
    AuthChallengeResponse(const base::string16& username,
                          const base::string16& password);

    const ResponseType response_type;
    const net::AuthCredentials credentials;

    DISALLOW_COPY_AND_ASSIGN(AuthChallengeResponse);
  };

  struct Modifications {
    using HeadersVector = std::vector<std::pair<std::string, std::string>>;

    Modifications();
    explicit Modifications(net::Error error_reason);
    explicit Modifications(
        std::unique_ptr<AuthChallengeResponse> auth_challenge_response);
    Modifications(scoped_refptr<net::HttpResponseHeaders> response_headers,
                  scoped_refptr<base::RefCountedMemory> response_body);
    Modifications(protocol::Maybe<std::string> modified_url,
                  protocol::Maybe<std::string> modified_method,
                  protocol::Maybe<std::string> modified_post_data,
                  std::unique_ptr<HeadersVector> modified_headers);
    Modifications(
        base::Optional<net::Error> error_reason,
        scoped_refptr<net::HttpResponseHeaders> response_headers,
        scoped_refptr<base::RefCountedMemory> response_body,
        size_t body_offset,
        protocol::Maybe<std::string> modified_url,
        protocol::Maybe<std::string> modified_method,
        protocol::Maybe<std::string> modified_post_data,
        std::unique_ptr<HeadersVector> modified_headers,
        std::unique_ptr<AuthChallengeResponse> auth_challenge_response);
    ~Modifications();

    // If none of the following are set then the request will be allowed to
    // continue unchanged.
    base::Optional<net::Error> error_reason;  // Finish with error.
    // If either of the below fields is set, complete the request by
    // responding with the provided headers and body.
    scoped_refptr<net::HttpResponseHeaders> response_headers;
    scoped_refptr<base::RefCountedMemory> response_body;
    size_t body_offset = 0;

    // Optionally modify before sending to network.
    protocol::Maybe<std::string> modified_url;
    protocol::Maybe<std::string> modified_method;
    protocol::Maybe<std::string> modified_post_data;
    std::unique_ptr<HeadersVector> modified_headers;
    // AuthChallengeResponse is mutually exclusive with the above.
    std::unique_ptr<AuthChallengeResponse> auth_challenge_response;
  };

  enum InterceptionStage {
    DONT_INTERCEPT = 0,
    REQUEST = (1 << 0),
    RESPONSE = (1 << 1),
    // Note: Both is not sent from front-end. It is used if both Request
    // and HeadersReceived was found it upgrades it to Both.
    BOTH = (REQUEST | RESPONSE),
  };

  struct Pattern {
   public:
    ~Pattern();
    Pattern(const Pattern& other);
    Pattern(const std::string& url_pattern,
            base::flat_set<ResourceType> resource_types,
            InterceptionStage interception_stage);

    bool Matches(const std::string& url, ResourceType resource_type) const;

    const std::string url_pattern;
    const base::flat_set<ResourceType> resource_types;
    const InterceptionStage interception_stage;
  };

  struct FilterEntry {
    FilterEntry(const base::UnguessableToken& target_id,
                std::vector<Pattern> patterns,
                RequestInterceptedCallback callback);
    FilterEntry(FilterEntry&&);
    ~FilterEntry();

    const base::UnguessableToken target_id;
    std::vector<Pattern> patterns;
    const RequestInterceptedCallback callback;

    DISALLOW_COPY_AND_ASSIGN(FilterEntry);
  };

  using HandleAuthRequestCallback =
      base::OnceCallback<void(bool use_fallback,
                              const base::Optional<net::AuthCredentials>&)>;
  // Can only be called on the IO thread.
  static void HandleAuthRequest(int32_t process_id,
                                int32_t routing_id,
                                int32_t request_id,
                                const net::AuthChallengeInfo& auth_info,
                                HandleAuthRequestCallback callback);

  explicit DevToolsURLLoaderInterceptor(RequestInterceptedCallback callback);
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
      RenderProcessHost* rph,
      const base::UnguessableToken& frame_token,
      bool is_navigation,
      bool is_download,
      mojo::PendingReceiver<network::mojom::URLLoaderFactory>*
          target_factory_receiver);

 private:
  friend class InterceptionJob;
  friend class DevToolsURLLoaderFactoryProxy;

  void CreateJob(
      const base::UnguessableToken& frame_token,
      int32_t process_id,
      bool is_download,
      const base::Optional<std::string>& renderer_request_id,
      std::unique_ptr<CreateLoaderParameters> create_params,
      mojo::PendingReceiver<network::mojom::URLLoader> loader_receiver,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client,
      mojo::PendingRemote<network::mojom::URLLoaderFactory> target_factory,
      mojo::PendingRemote<network::mojom::CookieManager> cookie_manager);

  InterceptionStage GetInterceptionStage(const GURL& url,
                                         ResourceType resource_type) const;

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

  void RemoveJob(const std::string& id) { jobs_.erase(id); }
  void AddJob(const std::string& id, InterceptionJob* job) {
    jobs_.emplace(id, job);
  }

  const RequestInterceptedCallback request_intercepted_callback_;

  std::vector<Pattern> patterns_;
  bool handle_auth_ = false;
  std::map<std::string, InterceptionJob*> jobs_;

  base::WeakPtrFactory<DevToolsURLLoaderInterceptor> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(DevToolsURLLoaderInterceptor);
};

// The purpose of this class is to have a thin wrapper around
// InterfacePtr<URLLoaderFactory> that is held by the client as
// unique_ptr<network::mojom::URLLoaderFactory>, since this is the
// way some clients pass the factory. We prefer wrapping a mojo proxy
// rather than exposing original DevToolsURLLoaderFactoryProxy because
// this takes care of thread hopping when necessary.
class DevToolsURLLoaderFactoryAdapter
    : public network::mojom::URLLoaderFactory {
 public:
  DevToolsURLLoaderFactoryAdapter() = delete;
  explicit DevToolsURLLoaderFactoryAdapter(
      mojo::PendingRemote<network::mojom::URLLoaderFactory> factory);
  ~DevToolsURLLoaderFactoryAdapter() override;

 private:
  // network::mojom::URLLoaderFactory implementation
  void CreateLoaderAndStart(
      mojo::PendingReceiver<network::mojom::URLLoader> loader,
      int32_t routing_id,
      int32_t request_id,
      uint32_t options,
      const network::ResourceRequest& request,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation)
      override;
  void Clone(mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver)
      override;

  mojo::Remote<network::mojom::URLLoaderFactory> factory_;
};

inline DevToolsURLLoaderInterceptor::InterceptionStage& operator|=(
    DevToolsURLLoaderInterceptor::InterceptionStage& a,
    const DevToolsURLLoaderInterceptor::InterceptionStage& b) {
  a = static_cast<DevToolsURLLoaderInterceptor::InterceptionStage>(a | b);
  return a;
}

}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_URL_LOADER_INTERCEPTOR_H_
