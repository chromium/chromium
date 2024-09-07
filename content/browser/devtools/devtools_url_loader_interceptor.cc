// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/342213636): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "content/browser/devtools/devtools_url_loader_interceptor.h"

#include <memory>
#include <optional>
#include <string_view>

#include "base/barrier_closure.h"
#include "base/base64.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/no_destructor.h"
#include "base/strings/pattern.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "base/unguessable_token.h"
#include "content/browser/devtools/protocol/network.h"
#include "content/browser/devtools/protocol/network_handler.h"
#include "content/browser/devtools/request_body_collector.h"
#include "content/browser/loader/download_utils_impl.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_client.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/system/data_pipe_drainer.h"
#include "net/base/load_flags.h"
#include "net/base/mime_sniffer.h"
#include "net/cookies/cookie_access_result.h"
#include "net/cookies/cookie_util.h"
#include "net/http/http_util.h"
#include "net/url_request/redirect_info.h"
#include "net/url_request/redirect_util.h"
#include "net/url_request/referrer_policy.h"
#include "services/network/public/cpp/cors/cors.h"
#include "services/network/public/cpp/header_util.h"
#include "services/network/public/cpp/record_ontransfersizeupdate_utils.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/resource_request_body.h"
#include "services/network/public/cpp/url_loader_completion_status.h"
#include "services/network/public/mojom/early_hints.mojom.h"
#include "services/network/public/mojom/encoded_body_length.mojom-forward.h"
#include "services/network/public/mojom/encoded_body_length.mojom.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "third_party/blink/public/platform/resource_request_blocked_reason.h"

namespace content {

InterceptedRequestInfo::InterceptedRequestInfo() = default;

InterceptedRequestInfo::~InterceptedRequestInfo() = default;

DevToolsURLLoaderInterceptor::AuthChallengeResponse::AuthChallengeResponse(
    ResponseType response_type)
    : response_type(response_type) {
  DCHECK_NE(kProvideCredentials, response_type);
}

DevToolsURLLoaderInterceptor::AuthChallengeResponse::AuthChallengeResponse(
    const std::u16string& username,
    const std::u16string& password)
    : response_type(kProvideCredentials), credentials(username, password) {}

DevToolsURLLoaderInterceptor::FilterEntry::FilterEntry(
    const base::UnguessableToken& target_id,
    std::vector<Pattern> patterns,
    RequestInterceptedCallback callback)
    : target_id(target_id),
      patterns(std::move(patterns)),
      callback(std::move(callback)) {}

DevToolsURLLoaderInterceptor::FilterEntry::FilterEntry(FilterEntry&&) = default;
DevToolsURLLoaderInterceptor::FilterEntry::~FilterEntry() = default;

DevToolsURLLoaderInterceptor::Modifications::Modifications() = default;

DevToolsURLLoaderInterceptor::Modifications::Modifications(
    net::Error error_reason)
    : error_reason(error_reason) {}

DevToolsURLLoaderInterceptor::Modifications::Modifications(
    scoped_refptr<net::HttpResponseHeaders> response_headers,
    scoped_refptr<base::RefCountedMemory> response_body)
    : response_headers(std::move(response_headers)),
      response_body(std::move(response_body)) {}

DevToolsURLLoaderInterceptor::Modifications::Modifications(
    std::unique_ptr<AuthChallengeResponse> auth_challenge_response)
    : auth_challenge_response(std::move(auth_challenge_response)) {}

DevToolsURLLoaderInterceptor::Modifications::Modifications(
    protocol::Maybe<std::string> modified_url,
    protocol::Maybe<std::string> modified_method,
    protocol::Maybe<protocol::Binary> modified_post_data,
    std::unique_ptr<HeadersVector> modified_headers,
    protocol::Maybe<bool> intercept_response)
    : modified_url(std::move(modified_url)),
      modified_method(std::move(modified_method)),
      modified_post_data(std::move(modified_post_data)),
      modified_headers(std::move(modified_headers)),
      intercept_response(std::move(intercept_response)) {}

DevToolsURLLoaderInterceptor::Modifications::Modifications(
    std::optional<net::Error> error_reason,
    scoped_refptr<net::HttpResponseHeaders> response_headers,
    scoped_refptr<base::RefCountedMemory> response_body,
    size_t body_offset,
    protocol::Maybe<std::string> modified_url,
    protocol::Maybe<std::string> modified_method,
    protocol::Maybe<protocol::Binary> modified_post_data,
    std::unique_ptr<HeadersVector> modified_headers,
    std::unique_ptr<AuthChallengeResponse> auth_challenge_response)
    : error_reason(std::move(error_reason)),
      response_headers(std::move(response_headers)),
      response_body(std::move(response_body)),
      body_offset(body_offset),
      modified_url(std::move(modified_url)),
      modified_method(std::move(modified_method)),
      modified_post_data(std::move(modified_post_data)),
      modified_headers(std::move(modified_headers)),
      auth_challenge_response(std::move(auth_challenge_response)) {}

DevToolsURLLoaderInterceptor::Modifications::~Modifications() = default;

DevToolsURLLoaderInterceptor::Pattern::~Pattern() = default;

DevToolsURLLoaderInterceptor::Pattern::Pattern(const Pattern& other) = default;

DevToolsURLLoaderInterceptor::Pattern::Pattern(
    const std::string& url_pattern,
    base::flat_set<blink::mojom::ResourceType> resource_types,
    InterceptionStage interception_stage)
    : url_pattern(url_pattern),
      resource_types(std::move(resource_types)),
      interception_stage(interception_stage) {}

bool DevToolsURLLoaderInterceptor::Pattern::Matches(
    const std::string& url,
    blink::mojom::ResourceType resource_type) const {
  if (!resource_types.empty() &&
      !base::Contains(resource_types, resource_type)) {
    return false;
  }
  return base::MatchPattern(url, url_pattern);
}

struct CreateLoaderParameters {
  CreateLoaderParameters(
      int32_t request_id,
      uint32_t options,
      network::ResourceRequest request,
      net::MutableNetworkTrafficAnnotationTag traffic_annotation)
      : request_id(request_id),
        options(options),
        request(request),
        traffic_annotation(traffic_annotation) {}

  const int32_t request_id;
  const uint32_t options;
  network::ResourceRequest request;
  const net::MutableNetworkTrafficAnnotationTag traffic_annotation;
};

namespace {

using RequestInterceptedCallback =
    DevToolsURLLoaderInterceptor::RequestInterceptedCallback;
using ContinueInterceptedRequestCallback =
    DevToolsURLLoaderInterceptor::ContinueInterceptedRequestCallback;
using GetResponseBodyCallback =
    DevToolsURLLoaderInterceptor::GetResponseBodyForInterceptionCallback;
using TakeResponseBodyPipeCallback =
    DevToolsURLLoaderInterceptor::TakeResponseBodyPipeCallback;
using Modifications = DevToolsURLLoaderInterceptor::Modifications;
using InterceptionStage = DevToolsURLLoaderInterceptor::InterceptionStage;
using protocol::Response;
using network::mojom::CredentialsMode;
using network::mojom::FetchResponseType;

class BodyReader : public mojo::DataPipeDrainer::Client {
 public:
  explicit BodyReader(base::OnceClosure download_complete_callback)
      : download_complete_callback_(std::move(download_complete_callback)),
        body_(base::MakeRefCounted<base::RefCountedString>()) {}

  void StartReading(mojo::ScopedDataPipeConsumerHandle body);

  void AddCallback(std::unique_ptr<GetResponseBodyCallback> callback) {
    if (data_complete_) {
      DCHECK(callbacks_.empty());
      callback->sendSuccess(encoded_body_, true);
      return;
    }
    callbacks_.push_back(std::move(callback));
  }

  bool data_complete() const { return data_complete_; }

  scoped_refptr<base::RefCountedMemory> body() const {
    DCHECK(data_complete_);
    return body_;
  }

  void CancelWithError(std::string error) {
    for (auto& cb : callbacks_)
      cb->sendFailure(Response::ServerError(error));
    callbacks_.clear();
  }

 private:
  void OnDataAvailable(base::span<const uint8_t> data) override {
    DCHECK(!data_complete_);
    body_->as_string().append(base::as_string_view(data));
  }

  void OnDataComplete() override;

  std::unique_ptr<mojo::DataPipeDrainer> body_pipe_drainer_;
  std::vector<std::unique_ptr<GetResponseBodyCallback>> callbacks_;
  base::OnceClosure download_complete_callback_;
  scoped_refptr<base::RefCountedString> body_;
  std::string encoded_body_;
  bool data_complete_ = false;
};

void BodyReader::StartReading(mojo::ScopedDataPipeConsumerHandle body) {
  DCHECK(!callbacks_.empty());
  DCHECK(!body_pipe_drainer_);
  DCHECK(!data_complete_);

  body_pipe_drainer_ =
      std::make_unique<mojo::DataPipeDrainer>(this, std::move(body));
}

void BodyReader::OnDataComplete() {
  DCHECK(!data_complete_);
  data_complete_ = true;
  body_pipe_drainer_.reset();
  // TODO(caseq): only encode if necessary.
  encoded_body_ = base::Base64Encode(*body_);
  for (auto& cb : callbacks_)
    cb->sendSuccess(encoded_body_, true);
  callbacks_.clear();
  std::move(download_complete_callback_).Run();
}

struct ResponseMetadata {
  ResponseMetadata() = default;
  explicit ResponseMetadata(network::mojom::URLResponseHeadPtr head)
      : head(std::move(head)) {}

  network::mojom::URLResponseHeadPtr head =
      network::mojom::URLResponseHead::New();
  std::unique_ptr<net::RedirectInfo> redirect_info;
  std::optional<mojo_base::BigBuffer> cached_metadata;
  size_t encoded_length = 0;
  size_t transfer_size = 0;
  network::URLLoaderCompletionStatus status;
};

void RemoveUnsafeRequestHeadersOnRedirect(net::HttpRequestHeaders& headers) {
  // Mimic the behavior of URLLoader::OnReceivedRedirect. It has already
  // been called for the request and we just need to reflect the changes
  // on our side. It is ok to remove more headers than the network stack
  // did as RequestReceivedExtraInfo event will contain all the actual
  // headers.
  headers.RemoveHeader(net::HttpRequestHeaders::kCookie);
  const net::HttpRequestHeaders::HeaderVector request_headers =
      headers.GetHeaderVector();
  for (const auto& header : request_headers) {
    if (StartsWith(header.key, "sec-ch-",
                   base::CompareCase::INSENSITIVE_ASCII) ||
        StartsWith(header.key, "sec-fetch-",
                   base::CompareCase::INSENSITIVE_ASCII)) {
      headers.RemoveHeader(header.key);
    }
  }
}

class HeadersOverride {
 public:
  static std::unique_ptr<HeadersOverride> SaveAndOverride(
      network::ResourceRequest& request,
      DevToolsURLLoaderInterceptor::Modifications::HeadersVector
          modified_headers) {
    std::unique_ptr<HeadersOverride> instance(new HeadersOverride(request));
    DCHECK(request.headers.IsEmpty());

    for (const auto& entry : modified_headers) {
      if (base::EqualsCaseInsensitiveASCII(entry.first,
                                           net::HttpRequestHeaders::kReferer)) {
        request.referrer = GURL(entry.second);
        request.referrer_policy = net::ReferrerPolicy::NEVER_CLEAR;
      } else {
        request.headers.SetHeader(entry.first, entry.second);
      }
    }
    return instance;
  }

  static void Revert(std::unique_ptr<HeadersOverride> instance) {
    instance->request_->headers = std::move(instance->original_headers_);
    instance->request_->referrer = instance->original_referrer_;
    instance->request_->referrer_policy = instance->original_referrer_policy_;
  }

  void RemoveUnsafeOriginalHeadersOnRedirect() {
    RemoveUnsafeRequestHeadersOnRedirect(original_headers_);
  }

  // Compute `remove_headers` and `modified_headers` that are needed
  // to turn `a` into `b`.
  static void ComputeModifications(const net::HttpRequestHeaders& a,
                                   const net::HttpRequestHeaders& b,
                                   std::vector<std::string>& removed_headers,
                                   net::HttpRequestHeaders& modified_headers) {
    DCHECK(removed_headers.empty());
    DCHECK(modified_headers.IsEmpty());

    std::map<std::string, std::string> old_headers;
    for (const auto& entry : a.GetHeaderVector())
      old_headers.insert({entry.key, entry.value});

    for (const auto& entry : b.GetHeaderVector()) {
      auto it = old_headers.find(entry.key);
      if (it == old_headers.end() || it->second != entry.value)
        modified_headers.SetHeader(entry.key, entry.value);
      if (it != old_headers.end())
        old_headers.erase(it);
    }
    for (const auto& entry : old_headers)
      removed_headers.push_back(entry.first);
  }

 private:
  explicit HeadersOverride(network::ResourceRequest& request)
      : request_(request),
        original_headers_(std::move(request.headers)),
        original_referrer_(request.referrer),
        original_referrer_policy_(request.referrer_policy) {}

  const raw_ref<network::ResourceRequest> request_;
  net::HttpRequestHeaders original_headers_;
  GURL original_referrer_;
  net::ReferrerPolicy original_referrer_policy_;
};

}  // namespace

class InterceptionJob : public network::mojom::URLLoaderClient,
                        public network::mojom::URLLoader {
 public:
  static InterceptionJob* FindByRequestId(
      const GlobalRequestID& global_req_id) {
    const auto& map = GetInterceptionJobMap();
    auto it = map.find(global_req_id);
    return it == map.end() ? nullptr : it->second;
  }

  InterceptionJob(
      DevToolsURLLoaderInterceptor* interceptor,
      const std::string& id,
      const base::UnguessableToken& frame_token,
      int32_t process_id,
      const std::optional<std::string>& renderer_request_id,
      std::unique_ptr<CreateLoaderParameters> create_loader_params,
      bool is_download,
      mojo::PendingReceiver<network::mojom::URLLoader> loader_receiver,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client,
      mojo::PendingRemote<network::mojom::URLLoaderFactory> target_factory,
      mojo::PendingRemote<network::mojom::CookieManager> cookie_manager);

  InterceptionJob(const InterceptionJob&) = delete;
  InterceptionJob& operator=(const InterceptionJob&) = delete;

  void GetResponseBody(std::unique_ptr<GetResponseBodyCallback> callback);
  void TakeResponseBodyPipe(TakeResponseBodyPipeCallback callback);
  void ContinueInterceptedRequest(
      std::unique_ptr<Modifications> modifications,
      std::unique_ptr<ContinueInterceptedRequestCallback> callback);
  void Detach();

  void OnAuthRequest(
      const net::AuthChallengeInfo& auth_info,
      DevToolsURLLoaderInterceptor::HandleAuthRequestCallback callback);

 private:
  static std::map<GlobalRequestID, InterceptionJob*>& GetInterceptionJobMap() {
    static base::NoDestructor<std::map<GlobalRequestID, InterceptionJob*>> inst;
    return *inst;
  }

  ~InterceptionJob() override {
    if (registered_in_global_request_map_) {
      size_t erased = GetInterceptionJobMap().erase(global_req_id_);
      DCHECK_EQ(1lu, erased);
    }
  }

  Response InnerContinueRequest(std::unique_ptr<Modifications> modifications);
  void ProcessFollowRedirect(
      const net::HttpRequestHeaders& modified_cors_exempt_headers);
  void ProcessAuthResponse(
      const DevToolsURLLoaderInterceptor::AuthChallengeResponse&
          auth_challenge_response);
  Response ProcessResponseOverride(
      scoped_refptr<net::HttpResponseHeaders> headers,
      scoped_refptr<base::RefCountedMemory> body,
      size_t response_body_offset);
  void ProcessRedirectByClient(const GURL& redirect_url);
  void ProcessSetCookies(const net::HttpResponseHeaders& response_headers,
                         base::OnceClosure callback);
  void SendResponse(scoped_refptr<base::RefCountedMemory> body, size_t offset);
  void ApplyModificationsToRequest(
      std::unique_ptr<Modifications> modifications);

  void StartRequest();
  void CancelRequest();
  void CompleteRequest(const network::URLLoaderCompletionStatus& status);
  void Shutdown();

  std::unique_ptr<InterceptedRequestInfo> BuildRequestInfo(
      const network::mojom::URLResponseHeadPtr& head);
  void NotifyClient(std::unique_ptr<InterceptedRequestInfo> request_info);
  void FetchCookies(base::OnceClosure callback);
  void OnGotCookies(
      base::OnceClosure callback,
      const net::CookieAccessResultList& cookies_with_access_result,
      const net::CookieAccessResultList& excluded_cookies);
  void OnGotRequestBodies(base::OnceClosure callback,
                          std::vector<RequestBodyCollector::BodyEntry> bodies);
  void CompleteNotifyingClient(
      std::unique_ptr<InterceptedRequestInfo> request_info);

  void ResponseBodyComplete();

  bool ShouldBypassForResponse() const {
    if (state_ == State::kResponseTaken)
      return false;
    DCHECK_EQ(!!response_metadata_, !!body_reader_);
    DCHECK_EQ(state_, State::kResponseReceived);
    return !response_metadata_;
  }

  // network::mojom::URLLoader methods
  void FollowRedirect(
      const std::vector<std::string>& removed_headers,
      const net::HttpRequestHeaders& modified_headers,
      const net::HttpRequestHeaders& modified_cors_exempt_headers,
      const std::optional<GURL>& new_url) override;
  void SetPriority(net::RequestPriority priority,
                   int32_t intra_priority_value) override;
  void PauseReadingBodyFromNet() override;
  void ResumeReadingBodyFromNet() override;

  // network::mojom::URLLoaderClient methods
  void OnReceiveEarlyHints(network::mojom::EarlyHintsPtr early_hints) override;
  void OnReceiveResponse(
      network::mojom::URLResponseHeadPtr head,
      mojo::ScopedDataPipeConsumerHandle body,
      std::optional<mojo_base::BigBuffer> cached_metadata) override;
  void OnReceiveRedirect(const net::RedirectInfo& redirect_info,
                         network::mojom::URLResponseHeadPtr head) override;
  void OnUploadProgress(int64_t current_position,
                        int64_t total_size,
                        OnUploadProgressCallback callback) override;
  void OnTransferSizeUpdated(int32_t transfer_size_diff) override;
  void OnComplete(const network::URLLoaderCompletionStatus& status) override;

  void StartLoadingResponseBody(mojo::ScopedDataPipeConsumerHandle body);

  bool CanGetResponseBody(std::string* error_reason);
  bool StartJobAndMaybeNotify();

  void UpdateCORSFlag();
  network::mojom::FetchResponseType CalculateResponseTainting();
  network::ResourceRequest GetResourceRequestForCookies();

  const std::string id_prefix_;
  const GlobalRequestID global_req_id_;
  const base::UnguessableToken frame_token_;
  const bool report_upload_;

  raw_ptr<DevToolsURLLoaderInterceptor> interceptor_;
  DevToolsURLLoaderInterceptor::InterceptionStages stages_;

  std::unique_ptr<CreateLoaderParameters> create_loader_params_;
  const bool is_download_;

  mojo::Receiver<network::mojom::URLLoaderClient> client_receiver_{this};
  mojo::Receiver<network::mojom::URLLoader> loader_receiver_{this};

  mojo::Remote<network::mojom::URLLoaderClient> client_;
  mojo::Remote<network::mojom::URLLoader> loader_;
  mojo::Remote<network::mojom::URLLoaderFactory> target_factory_;
  mojo::Remote<network::mojom::CookieManager> cookie_manager_;

  enum State {
    kNotStarted,
    kRequestSent,
    kRedirectReceived,
    kFollowRedirect,
    kAuthRequired,
    kResponseReceived,
    kResponseTaken,
  };

  State state_;
  base::TimeTicks start_ticks_;
  base::Time start_time_;

  bool waiting_for_resolution_;
  int redirect_count_;
  bool tainted_origin_ = false;
  bool fetch_cors_flag_ = false;
  std::string current_id_;
  std::string redirected_request_id_;

  std::unique_ptr<BodyReader> body_reader_;
  std::unique_ptr<ResponseMetadata> response_metadata_;
  mojo::ScopedDataPipeConsumerHandle body_;
  bool registered_in_global_request_map_;

  std::optional<std::pair<net::RequestPriority, int32_t>> priority_;
  DevToolsURLLoaderInterceptor::HandleAuthRequestCallback
      pending_auth_callback_;
  TakeResponseBodyPipeCallback pending_response_body_pipe_callback_;

  const std::optional<std::string> renderer_request_id_;

  // List of URLs that have been redirected through. The last member is the
  // current request URL. Tracked for the purpose of computing the proper
  // SameSite cookies to return, which depends on the redirect chain.
  std::vector<GURL> url_chain_;
  // In case headers are overridden, keep the original and restore them
  // upon a redirect, so that overrides don't stick across redirects.
  std::unique_ptr<HeadersOverride> headers_override_;
  // Header overrides are reverted before the redirect, so that
  // request paused event contains original headers. Previous headers
  // are used on resume to compute the difference for the network stack.
  std::unique_ptr<net::HttpRequestHeaders> headers_before_redirect_;

  // These two are needed to build a Request and are prepared as needed when
  // sending Request for the first time. Both need to be cleared upon redirect.
  std::vector<RequestBodyCollector::BodyEntry> request_bodies_;
  std::optional<std::string> request_cookies_;

  // This is only for retaining the body collector for the duration of its
  // work (and properly cancelling it if the job gets prematurely destroyed).
  std::unique_ptr<RequestBodyCollector> request_body_collector_;

  SEQUENCE_CHECKER(sequence_checker_);
};

void DevToolsURLLoaderInterceptor::CreateJob(
    const base::UnguessableToken& frame_token,
    int32_t process_id,
    bool is_download,
    const std::optional<std::string>& renderer_request_id,
    std::unique_ptr<CreateLoaderParameters> create_params,
    mojo::PendingReceiver<network::mojom::URLLoader> loader_receiver,
    mojo::PendingRemote<network::mojom::URLLoaderClient> client,
    mojo::PendingRemote<network::mojom::URLLoaderFactory> target_factory,
    mojo::PendingRemote<network::mojom::CookieManager> cookie_manager) {
  DCHECK(!frame_token.is_empty());

  static int last_id = 0;

  std::string id = base::StringPrintf("interception-job-%d", ++last_id);
  // This class will manage its own life time to match the loader client.
  new InterceptionJob(
      this, std::move(id), frame_token, process_id, renderer_request_id,
      std::move(create_params), is_download, std::move(loader_receiver),
      std::move(client), std::move(target_factory), std::move(cookie_manager));
}

DevToolsURLLoaderInterceptor::InterceptionStages
DevToolsURLLoaderInterceptor::GetInterceptionStages(
    const GURL& url,
    blink::mojom::ResourceType resource_type) const {
  DevToolsURLLoaderInterceptor::InterceptionStages stages;
  std::string unused;
  std::string url_str = protocol::NetworkHandler::ExtractFragment(url, &unused);
  for (const auto& pattern : patterns_) {
    if (pattern.Matches(url_str, resource_type))
      stages.Put(pattern.interception_stage);
  }
  return stages;
}

class DevToolsURLLoaderFactoryProxy : public network::mojom::URLLoaderFactory {
 public:
  DevToolsURLLoaderFactoryProxy(
      const base::UnguessableToken& frame_token,
      int32_t process_id,
      bool is_download,
      mojo::PendingReceiver<network::mojom::URLLoaderFactory> loader_receiver,
      mojo::PendingRemote<network::mojom::URLLoaderFactory>
          target_factory_remote,
      mojo::PendingRemote<network::mojom::CookieManager> cookie_manager,
      base::WeakPtr<DevToolsURLLoaderInterceptor> interceptor);
  ~DevToolsURLLoaderFactoryProxy() override;

 private:
  // network::mojom::URLLoaderFactory implementation
  void CreateLoaderAndStart(
      mojo::PendingReceiver<network::mojom::URLLoader> loader,
      int32_t request_id,
      uint32_t options,
      const network::ResourceRequest& request,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation)
      override;
  void Clone(mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver)
      override;

  void OnProxyBindingError();
  void OnTargetFactoryError();

  const base::UnguessableToken frame_token_;
  const int32_t process_id_;
  const bool is_download_;

  mojo::Remote<network::mojom::URLLoaderFactory> target_factory_;
  mojo::Remote<network::mojom::CookieManager> cookie_manager_;
  base::WeakPtr<DevToolsURLLoaderInterceptor> interceptor_;
  mojo::ReceiverSet<network::mojom::URLLoaderFactory> receivers_;

  SEQUENCE_CHECKER(sequence_checker_);
};

// This class owns itself and will delete self when any mojo
// connection is broken.
DevToolsURLLoaderFactoryProxy::DevToolsURLLoaderFactoryProxy(
    const base::UnguessableToken& frame_token,
    int32_t process_id,
    bool is_download,
    mojo::PendingReceiver<network::mojom::URLLoaderFactory> loader_receiver,
    mojo::PendingRemote<network::mojom::URLLoaderFactory> target_factory_remote,
    mojo::PendingRemote<network::mojom::CookieManager> cookie_manager,
    base::WeakPtr<DevToolsURLLoaderInterceptor> interceptor)
    : frame_token_(frame_token),
      process_id_(process_id),
      is_download_(is_download),
      target_factory_(std::move(target_factory_remote)),
      interceptor_(std::move(interceptor)) {
  target_factory_.set_disconnect_handler(
      base::BindOnce(&DevToolsURLLoaderFactoryProxy::OnTargetFactoryError,
                     base::Unretained(this)));

  receivers_.Add(this, std::move(loader_receiver));
  receivers_.set_disconnect_handler(
      base::BindRepeating(&DevToolsURLLoaderFactoryProxy::OnProxyBindingError,
                          base::Unretained(this)));

  cookie_manager_.Bind(std::move(cookie_manager));
  cookie_manager_.set_disconnect_handler(
      base::BindOnce(&DevToolsURLLoaderFactoryProxy::OnTargetFactoryError,
                     base::Unretained(this)));
}

DevToolsURLLoaderFactoryProxy::~DevToolsURLLoaderFactoryProxy() = default;

void DevToolsURLLoaderFactoryProxy::CreateLoaderAndStart(
    mojo::PendingReceiver<network::mojom::URLLoader> loader,
    int32_t request_id,
    uint32_t options,
    const network::ResourceRequest& request,
    mojo::PendingRemote<network::mojom::URLLoaderClient> client,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  DevToolsURLLoaderInterceptor* interceptor = interceptor_.get();
  if (!interceptor_ || request.url.SchemeIs(url::kDataScheme)) {
    target_factory_->CreateLoaderAndStart(std::move(loader), request_id,
                                          options, request, std::move(client),
                                          traffic_annotation);
    return;
  }
  auto creation_params = std::make_unique<CreateLoaderParameters>(
      request_id, options, request, traffic_annotation);
  mojo::PendingRemote<network::mojom::URLLoaderFactory> factory_clone;
  target_factory_->Clone(factory_clone.InitWithNewPipeAndPassReceiver());
  mojo::PendingRemote<network::mojom::CookieManager> cookie_manager_clone;
  cookie_manager_->CloneInterface(
      cookie_manager_clone.InitWithNewPipeAndPassReceiver());
  interceptor->CreateJob(
      frame_token_, process_id_, is_download_, request.devtools_request_id,
      std::move(creation_params), std::move(loader), std::move(client),
      std::move(factory_clone), std::move(cookie_manager_clone));
}

void DevToolsURLLoaderFactoryProxy::Clone(
    mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  receivers_.Add(this, std::move(receiver));
}

void DevToolsURLLoaderFactoryProxy::OnTargetFactoryError() {
  delete this;
}

void DevToolsURLLoaderFactoryProxy::OnProxyBindingError() {
  if (receivers_.empty())
    delete this;
}

// static
void DevToolsURLLoaderInterceptor::HandleAuthRequest(
    GlobalRequestID req_id,
    const net::AuthChallengeInfo& auth_info,
    HandleAuthRequestCallback callback) {
  if (auto* job = InterceptionJob::FindByRequestId(req_id))
    job->OnAuthRequest(auth_info, std::move(callback));
  else
    std::move(callback).Run(true, std::nullopt);
}

DevToolsURLLoaderInterceptor::DevToolsURLLoaderInterceptor(
    RequestInterceptedCallback callback)
    : request_intercepted_callback_(std::move(callback)), weak_factory_(this) {}

DevToolsURLLoaderInterceptor::~DevToolsURLLoaderInterceptor() {
  for (auto const& entry : jobs_)
    entry.second->Detach();
}

void DevToolsURLLoaderInterceptor::SetPatterns(
    std::vector<DevToolsURLLoaderInterceptor::Pattern> patterns,
    bool handle_auth) {
  patterns_ = std::move(patterns);
  handle_auth_ = handle_auth;
  DCHECK(patterns_.size() || !handle_auth);
}

void DevToolsURLLoaderInterceptor::GetResponseBody(
    const std::string& interception_id,
    std::unique_ptr<GetResponseBodyCallback> callback) {
  if (InterceptionJob* job = FindJob(interception_id, &callback))
    job->GetResponseBody(std::move(callback));
}

void DevToolsURLLoaderInterceptor::TakeResponseBodyPipe(
    const std::string& interception_id,
    DevToolsURLLoaderInterceptor::TakeResponseBodyPipeCallback callback) {
  auto it = jobs_.find(interception_id);
  if (it == jobs_.end()) {
    std::move(callback).Run(
        protocol::Response::InvalidParams("Invalid InterceptionId."),
        mojo::ScopedDataPipeConsumerHandle(), std::string());
    return;
  }
  it->second->TakeResponseBodyPipe(std::move(callback));
}

void DevToolsURLLoaderInterceptor::ContinueInterceptedRequest(
    const std::string& interception_id,
    std::unique_ptr<Modifications> modifications,
    std::unique_ptr<ContinueInterceptedRequestCallback> callback) {
  if (InterceptionJob* job = FindJob(interception_id, &callback)) {
    job->ContinueInterceptedRequest(std::move(modifications),
                                    std::move(callback));
  }
}

bool DevToolsURLLoaderInterceptor::CreateProxyForInterception(
    int process_id,
    StoragePartition* storage_partition,
    const base::UnguessableToken& frame_token,
    bool is_navigation,
    bool is_download,
    network::mojom::URLLoaderFactoryOverride* intercepting_factory) {
  DCHECK(storage_partition);

  if (patterns_.empty())
    return false;

  // If we're the first interceptor to install an override, make a
  // remote/receiver pair, then handle this similarly to appending
  // a proxy to existing override.
  if (!intercepting_factory->overriding_factory) {
    DCHECK(!intercepting_factory->overridden_factory_receiver);
    intercepting_factory->overridden_factory_receiver =
        intercepting_factory->overriding_factory
            .InitWithNewPipeAndPassReceiver();
  }
  mojo::PendingRemote<network::mojom::URLLoaderFactory> target_remote;
  auto overridden_factory_receiver =
      target_remote.InitWithNewPipeAndPassReceiver();
  mojo::PendingRemote<network::mojom::CookieManager> cookie_manager;

  // TODO(crbug.com/40276949): Using 0 as the process id for navigations
  // can lead to collisions between multiple navigations/service workers main
  // script fetch. It should be replaced by the more robust
  // GlobalRequestID::MakeBrowserInitiated().
  int process_id_override = process_id;
  if (is_navigation)
    process_id_override = 0;

  storage_partition->GetNetworkContext()->GetCookieManager(
      cookie_manager.InitWithNewPipeAndPassReceiver());
  new DevToolsURLLoaderFactoryProxy(
      frame_token, process_id_override, is_download,
      std::move(intercepting_factory->overridden_factory_receiver),
      std::move(target_remote), std::move(cookie_manager),
      weak_factory_.GetWeakPtr());
  intercepting_factory->overridden_factory_receiver =
      std::move(overridden_factory_receiver);
  return true;
}

InterceptionJob::InterceptionJob(
    DevToolsURLLoaderInterceptor* interceptor,
    const std::string& id,
    const base::UnguessableToken& frame_token,
    int process_id,
    const std::optional<std::string>& renderer_request_id,
    std::unique_ptr<CreateLoaderParameters> create_loader_params,
    bool is_download,
    mojo::PendingReceiver<network::mojom::URLLoader> loader_receiver,
    mojo::PendingRemote<network::mojom::URLLoaderClient> client,
    mojo::PendingRemote<network::mojom::URLLoaderFactory> target_factory,
    mojo::PendingRemote<network::mojom::CookieManager> cookie_manager)
    : id_prefix_(id),
      global_req_id_(process_id, create_loader_params->request_id),
      frame_token_(frame_token),
      report_upload_(!!create_loader_params->request.request_body),
      interceptor_(interceptor),
      create_loader_params_(std::move(create_loader_params)),
      is_download_(is_download),
      client_(std::move(client)),
      target_factory_(std::move(target_factory)),
      cookie_manager_(std::move(cookie_manager)),
      state_(kNotStarted),
      waiting_for_resolution_(false),
      redirect_count_(0),
      renderer_request_id_(renderer_request_id) {
  loader_receiver_.Bind(std::move(loader_receiver));
  loader_receiver_.set_disconnect_handler(
      base::BindOnce(&InterceptionJob::Shutdown, base::Unretained(this)));

  auto& job_map = GetInterceptionJobMap();
  // TODO(caseq): for now, all auth requests will go to the top-level job.
  // Figure out if we need anything smarter here.
  registered_in_global_request_map_ =
      job_map.emplace(global_req_id_, this).second;

  url_chain_.push_back(create_loader_params_->request.url);

  if (StartJobAndMaybeNotify())
    return;

  StartRequest();
}

bool InterceptionJob::StartJobAndMaybeNotify() {
  UpdateCORSFlag();
  start_ticks_ = base::TimeTicks::Now();
  start_time_ = base::Time::Now();

  current_id_ = id_prefix_ + base::StringPrintf(".%d", redirect_count_);
  interceptor_->AddJob(current_id_, this);

  const network::ResourceRequest& request = create_loader_params_->request;
  stages_ = interceptor_->GetInterceptionStages(
      request.url,
      static_cast<blink::mojom::ResourceType>(request.resource_type));

  if (!stages_.Has(InterceptionStage::kRequest)) {
    return false;
  }

  if (state_ == State::kRedirectReceived)
    state_ = State::kFollowRedirect;
  else
    DCHECK_EQ(State::kNotStarted, state_);
  NotifyClient(BuildRequestInfo(nullptr));
  return true;
}

// FIXME(caseq): The logic in the three methods below is borrowed from
// CorsURLLoader as a matter of a quick and mergeable fix for crbug.com/1022173.
// This logic should be unified with CorsURLLoader.
network::mojom::FetchResponseType InterceptionJob::CalculateResponseTainting() {
  if (fetch_cors_flag_)
    return FetchResponseType::kCors;
  if (create_loader_params_->request.mode ==
          network::mojom::RequestMode::kNoCors &&
      tainted_origin_) {
    return FetchResponseType::kOpaque;
  }
  return FetchResponseType::kBasic;
}

network::ResourceRequest InterceptionJob::GetResourceRequestForCookies() {
  FetchResponseType response_tainting =
      fetch_cors_flag_ ? FetchResponseType::kCors : FetchResponseType::kBasic;

  network::ResourceRequest result = create_loader_params_->request;
  result.credentials_mode =
      network::cors::CalculateCredentialsFlag(
          create_loader_params_->request.credentials_mode, response_tainting)
          ? CredentialsMode::kInclude
          : CredentialsMode::kOmit;
  return result;
}

void InterceptionJob::UpdateCORSFlag() {
  if (fetch_cors_flag_)
    return;

  const network::ResourceRequest& request = create_loader_params_->request;
  fetch_cors_flag_ = network::cors::ShouldCheckCors(
      request.url, request.request_initiator, request.mode);
}

bool InterceptionJob::CanGetResponseBody(std::string* error_reason) {
  if (!stages_.Has(InterceptionStage::kResponse)) {
    *error_reason =
        "Can only get response body on HeadersReceived pattern matched "
        "requests.";
    return false;
  }
  if (state_ != State::kResponseReceived || !waiting_for_resolution_) {
    *error_reason =
        "Can only get response body on requests captured after headers "
        "received.";
    return false;
  }
  return true;
}

void InterceptionJob::GetResponseBody(
    std::unique_ptr<GetResponseBodyCallback> callback) {
  std::string error_reason;
  if (!CanGetResponseBody(&error_reason)) {
    callback->sendFailure(Response::ServerError(std::move(error_reason)));
    return;
  }
  bool body_reader_created = !body_reader_;
  if (!body_reader_) {
    body_reader_ = std::make_unique<BodyReader>(base::BindOnce(
        &InterceptionJob::ResponseBodyComplete, base::Unretained(this)));
    client_receiver_.Resume();
    loader_->ResumeReadingBodyFromNet();
  }
  body_reader_->AddCallback(std::move(callback));
  // Needs to happen after |AddCallback| to avoid a DCHECK.
  if (body_reader_created && body_)
    StartLoadingResponseBody(std::move(body_));
}

void InterceptionJob::TakeResponseBodyPipe(
    TakeResponseBodyPipeCallback callback) {
  std::string error_reason;
  if (!CanGetResponseBody(&error_reason)) {
    std::move(callback).Run(Response::ServerError(std::move(error_reason)),
                            mojo::ScopedDataPipeConsumerHandle(),
                            std::string());
    return;
  }
  DCHECK_EQ(state_, State::kResponseReceived);
  DCHECK(!!response_metadata_);
  state_ = State::kResponseTaken;
  pending_response_body_pipe_callback_ = std::move(callback);
  client_receiver_.Resume();
  if (body_)
    StartLoadingResponseBody(std::move(body_));
  loader_->ResumeReadingBodyFromNet();
}

void InterceptionJob::ContinueInterceptedRequest(
    std::unique_ptr<Modifications> modifications,
    std::unique_ptr<ContinueInterceptedRequestCallback> callback) {
  Response response = InnerContinueRequest(std::move(modifications));
  // |this| may be destroyed at this point.
  if (response.IsSuccess())
    callback->sendSuccess();
  else
    callback->sendFailure(std::move(response));
}

void InterceptionJob::Detach() {
  stages_.Clear();
  interceptor_ = nullptr;
  if (!waiting_for_resolution_)
    return;
  if (state_ == State::kAuthRequired) {
    state_ = State::kRequestSent;
    waiting_for_resolution_ = false;
    TRACE_EVENT_NESTABLE_ASYNC_END0("devtools", "Fetch.requestPaused", this);
    std::move(pending_auth_callback_).Run(true, std::nullopt);
    return;
  }
  InnerContinueRequest(std::make_unique<Modifications>());
}

Response InterceptionJob::InnerContinueRequest(
    std::unique_ptr<Modifications> modifications) {
  if (!waiting_for_resolution_) {
    return Response::ServerError(
        "Invalid state for continueInterceptedRequest");
  }
  waiting_for_resolution_ = false;
  TRACE_EVENT_NESTABLE_ASYNC_END0("devtools", "Fetch.requestPaused", this);
  if (modifications->intercept_response.has_value()) {
    stages_.PutOrRemove(InterceptionStage::kResponse,
                        modifications->intercept_response.value());
  }

  if (state_ == State::kAuthRequired) {
    if (!modifications->auth_challenge_response)
      return Response::InvalidParams("authChallengeResponse required.");
    ProcessAuthResponse(*modifications->auth_challenge_response);
    return Response::Success();
  }
  if (modifications->auth_challenge_response)
    return Response::InvalidParams("authChallengeResponse not expected.");

  if (modifications->error_reason) {
    network::URLLoaderCompletionStatus status(
        modifications->error_reason.value());
    status.completion_time = base::TimeTicks::Now();
    if (modifications->error_reason == net::ERR_BLOCKED_BY_CLIENT) {
      // So we know that these modifications originated from devtools
      // (also known as inspector), and can therefore annotate the
      // request. We only do this for one specific error code thus
      // far, to minimize risk of breaking other usages.
      status.extended_error_code =
          static_cast<int>(blink::ResourceRequestBlockedReason::kInspector);
    }
    CompleteRequest(status);
    return Response::Success();
  }

  if (modifications->response_headers || modifications->response_body) {
    // If only intercepted response headers are overridden continue with
    // normal load of the original response body.
    if (response_metadata_ && !modifications->response_body) {
      network::mojom::URLResponseHeadPtr& head = response_metadata_->head;
      head->headers = std::move(modifications->response_headers);
      // TODO(caseq): we're cheating here a bit, raw_headers() have \0's
      // where real headers would have \r\n, but the sizes here
      // probably don't have to be exact.
      size_t headers_size = head->headers->raw_headers().size();
      head->encoded_data_length = headers_size;
    } else {
      return ProcessResponseOverride(std::move(modifications->response_headers),
                                     std::move(modifications->response_body),
                                     modifications->body_offset);
    }
  }

  if (state_ == State::kFollowRedirect) {
    if (!modifications->modified_url.has_value()) {
      // TODO(caseq): report error if other modifications are present.

      // At this point we already reverted headers to the original state.
      if (modifications->modified_headers) {
        headers_override_ = HeadersOverride::SaveAndOverride(
            create_loader_params_->request,
            std::move(*modifications->modified_headers));
      }

      ProcessFollowRedirect({});
      return Response::Success();
    }
    CancelRequest();
    // Fall through to the generic logic of re-starting the request
    // at the bottom of the method.
  }
  if (state_ == State::kRedirectReceived) {
    // TODO(caseq): report error if other modifications are present.
    if (modifications->modified_url.has_value()) {
      std::string location = modifications->modified_url.value();
      CancelRequest();
      response_metadata_->head->headers->SetHeader("location", location);
      GURL redirect_url = create_loader_params_->request.url.Resolve(location);
      if (!redirect_url.is_valid())
        return Response::ServerError("Invalid modified URL");
      ProcessRedirectByClient(redirect_url);
      return Response::Success();
    }
    client_->OnReceiveRedirect(*response_metadata_->redirect_info,
                               std::move(response_metadata_->head));
    return Response::Success();
  }

  if (body_reader_) {
    if (body_reader_->data_complete())
      SendResponse(body_reader_->body(), 0);

    // There are read callbacks pending, so let the reader do its job and come
    // back when it's done.
    return Response::Success();
  }

  if (response_metadata_) {
    if (state_ == State::kResponseTaken) {
      return Response::InvalidParams(
          "Unable to continue request as is after body is taken");
    }
    // TODO(caseq): report error if other modifications are present.
    if (response_metadata_->status.error_code) {
      CompleteRequest(response_metadata_->status);
      return Response::Success();
    }
    DCHECK_EQ(State::kResponseReceived, state_);
    DCHECK(!body_reader_);
    client_->OnReceiveResponse(std::move(response_metadata_->head),
                               std::move(body_),
                               std::move(response_metadata_->cached_metadata));
    response_metadata_.reset();
    loader_->ResumeReadingBodyFromNet();
    client_receiver_.Resume();
    return Response::Success();
  }

  DCHECK_EQ(State::kNotStarted, state_);
  ApplyModificationsToRequest(std::move(modifications));
  headers_before_redirect_.reset();
  StartRequest();
  return Response::Success();
}

void InterceptionJob::ProcessFollowRedirect(
    const net::HttpRequestHeaders& modified_cors_exempt_headers) {
  std::vector<std::string> removed_headers;
  net::HttpRequestHeaders modified_headers;
  CHECK(headers_before_redirect_);
  HeadersOverride::ComputeModifications(*headers_before_redirect_,
                                        create_loader_params_->request.headers,
                                        removed_headers, modified_headers);
  headers_before_redirect_.reset();
  loader_->FollowRedirect(removed_headers, modified_headers,
                          modified_cors_exempt_headers, std::nullopt);
  state_ = State::kRequestSent;
}

void InterceptionJob::ApplyModificationsToRequest(
    std::unique_ptr<Modifications> modifications) {
  network::ResourceRequest* request = &create_loader_params_->request;

  // Note this redirect is not visible to the page by design. If they want a
  // visible redirect they can mock a response with a 302.
  if (modifications->modified_url.has_value()) {
    DCHECK_EQ(url_chain_.back(), request->url);
    const GURL new_url(modifications->modified_url.value());
    request->url = new_url;
    url_chain_.back() = new_url;
  }

  if (modifications->modified_method.has_value()) {
    request->method = modifications->modified_method.value();
  }

  if (modifications->modified_post_data.has_value()) {
    const auto& post_data = modifications->modified_post_data.value();
    request->request_body = network::ResourceRequestBody::CreateFromBytes(
        reinterpret_cast<const char*>(post_data.data()), post_data.size());
    request_bodies_.clear();
  }

  if (modifications->modified_headers) {
    DCHECK(!headers_override_);
    headers_override_ = HeadersOverride::SaveAndOverride(
        *request, std::move(*modifications->modified_headers));
  }
}

void InterceptionJob::ProcessAuthResponse(
    const DevToolsURLLoaderInterceptor::AuthChallengeResponse& response) {
  DCHECK_EQ(kAuthRequired, state_);
  switch (response.response_type) {
    case DevToolsURLLoaderInterceptor::AuthChallengeResponse::kDefault:
      std::move(pending_auth_callback_).Run(true, std::nullopt);
      break;
    case DevToolsURLLoaderInterceptor::AuthChallengeResponse::kCancelAuth:
      std::move(pending_auth_callback_).Run(false, std::nullopt);
      break;
    case DevToolsURLLoaderInterceptor::AuthChallengeResponse::
        kProvideCredentials:
      std::move(pending_auth_callback_).Run(false, response.credentials);
      break;
  }
  state_ = kRequestSent;
}

Response InterceptionJob::ProcessResponseOverride(
    scoped_refptr<net::HttpResponseHeaders> headers,
    scoped_refptr<base::RefCountedMemory> body,
    size_t response_body_offset) {
  CancelRequest();

  DCHECK_LE(response_body_offset, body ? body->size() : 0);
  size_t body_size = body ? body->size() - response_body_offset : 0;
  response_metadata_ = std::make_unique<ResponseMetadata>();
  network::mojom::URLResponseHeadPtr& head = response_metadata_->head;

  head->request_time = start_time_;
  head->response_time = base::Time::Now();

  // TODO(caseq): we're only doing this because some clients expect load timing
  // to be present with mocked responses. Consider removing this.
  const base::TimeTicks now_ticks = base::TimeTicks::Now();
  head->load_timing.request_start_time = start_time_;
  head->load_timing.request_start = start_ticks_;
  head->load_timing.receive_headers_end = now_ticks;

  static const char kDummyHeaders[] = "HTTP/1.1 200 OK\0\0";
  head->headers = std::move(headers);
  if (!head->headers) {
    head->headers =
        base::MakeRefCounted<net::HttpResponseHeaders>(kDummyHeaders);
  }
  head->headers->GetMimeTypeAndCharset(&head->mime_type, &head->charset);
  const GURL& url = create_loader_params_->request.url;
  if (create_loader_params_->options &
      network::mojom::kURLLoadOptionSniffMimeType) {
    if (body_size && network::ShouldSniffContent(url, *head)) {
      size_t bytes_to_sniff =
          std::min(body_size, static_cast<size_t>(net::kMaxBytesToSniff));
      const std::string hint = head->mime_type;
      net::SniffMimeType(base::as_string_view(*body).substr(
                             response_body_offset, bytes_to_sniff),
                         url, hint, net::ForceSniffFileUrlsForHtml::kDisabled,
                         &head->mime_type);
      head->did_mime_sniff = true;
    } else if (head->mime_type.empty()) {
      head->mime_type.assign("text/plain");
    }
  }
  // TODO(caseq): we're cheating here a bit, raw_headers() have \0's
  // where real headers would have \r\n, but the sizes here
  // probably don't have to be exact.
  size_t headers_size = head->headers->raw_headers().size();
  head->content_length = body_size;
  head->encoded_data_length = headers_size;
  head->encoded_body_length = network::mojom::EncodedBodyLength::New(0u);
  head->request_start = start_ticks_;
  head->response_start = now_ticks;

  response_metadata_->transfer_size = body_size;

  response_metadata_->status.completion_time = base::TimeTicks::Now();
  response_metadata_->status.encoded_data_length = headers_size + body_size;
  response_metadata_->status.encoded_body_length = body_size;
  response_metadata_->status.decoded_body_length = body_size;

  base::OnceClosure continue_after_cookies_set;
  std::string location;
  if (head->headers->IsRedirect(&location)) {
    GURL redirect_url = url.Resolve(location);
    if (redirect_url.is_valid()) {
      continue_after_cookies_set =
          base::BindOnce(&InterceptionJob::ProcessRedirectByClient,
                         base::Unretained(this), std::move(redirect_url));
    }
  }
  if (!continue_after_cookies_set) {
    continue_after_cookies_set =
        base::BindOnce(&InterceptionJob::SendResponse, base::Unretained(this),
                       std::move(body), response_body_offset);
  }
  ProcessSetCookies(*head->headers, std::move(continue_after_cookies_set));

  return Response::Success();
}

void InterceptionJob::ProcessSetCookies(const net::HttpResponseHeaders& headers,
                                        base::OnceClosure callback) {
  if (!GetResourceRequestForCookies().SavesCookies()) {
    std::move(callback).Run();
    return;
  }

  std::vector<std::unique_ptr<net::CanonicalCookie>> cookies;
  std::optional<base::Time> server_time = headers.GetDateValue();
  base::Time now = base::Time::Now();

  const std::string_view name("Set-Cookie");
  std::string cookie_line;
  size_t iter = 0;
  while (headers.EnumerateHeader(&iter, name, &cookie_line)) {
    std::unique_ptr<net::CanonicalCookie> cookie = net::CanonicalCookie::Create(
        create_loader_params_->request.url, cookie_line, now, server_time,
        std::nullopt, net::CookieSourceType::kOther,
        /*status=*/nullptr);
    if (cookie)
      cookies.emplace_back(std::move(cookie));
  }

  net::CookieOptions options;
  options.set_include_httponly();
  bool should_treat_as_first_party =
      GetContentClient()
          ->browser()
          ->ShouldIgnoreSameSiteCookieRestrictionsWhenTopLevel(
              create_loader_params_->request.site_for_cookies.scheme(),
              create_loader_params_->request.url.SchemeIsCryptographic());
  DCHECK_EQ(create_loader_params_->request.url, url_chain_.back());
  bool is_main_frame_navigation =
      create_loader_params_->request.trusted_params.has_value() &&
      create_loader_params_->request.trusted_params->isolation_info
              .request_type() == net::IsolationInfo::RequestType::kMainFrame;
  options.set_same_site_cookie_context(
      net::cookie_util::ComputeSameSiteContextForResponse(
          url_chain_, create_loader_params_->request.site_for_cookies,
          create_loader_params_->request.request_initiator,
          is_main_frame_navigation, should_treat_as_first_party));

  // |this| might be deleted here if |cookies| is empty!
  auto on_cookie_set = base::BindRepeating(
      [](base::RepeatingClosure closure, net::CookieAccessResult) {
        closure.Run();
      },
      base::BarrierClosure(cookies.size(), std::move(callback)));
  for (auto& cookie : cookies) {
    cookie_manager_->SetCanonicalCookie(
        *cookie, create_loader_params_->request.url, options, on_cookie_set);
  }
}

void InterceptionJob::ProcessRedirectByClient(const GURL& redirect_url) {
  DCHECK(redirect_url.is_valid());

  const net::HttpResponseHeaders& headers = *response_metadata_->head->headers;
  const network::ResourceRequest& request = create_loader_params_->request;

  auto first_party_url_policy =
      request.update_first_party_url_on_redirect
          ? net::RedirectInfo::FirstPartyURLPolicy::UPDATE_URL_ON_REDIRECT
          : net::RedirectInfo::FirstPartyURLPolicy::NEVER_CHANGE_URL;

  response_metadata_->redirect_info = std::make_unique<net::RedirectInfo>(
      net::RedirectInfo::ComputeRedirectInfo(
          request.method, request.url, request.site_for_cookies,
          first_party_url_policy, request.referrer_policy,
          request.referrer.spec(), headers.response_code(), redirect_url,
          net::RedirectUtil::GetReferrerPolicyHeader(&headers),
          false /* insecure_scheme_was_upgraded */, true /* copy_fragment */));

  client_->OnReceiveRedirect(*response_metadata_->redirect_info,
                             std::move(response_metadata_->head));
}

void InterceptionJob::SendResponse(scoped_refptr<base::RefCountedMemory> body,
                                   size_t offset) {
  base::span<const uint8_t> bytes_to_write;
  if (body) {
    bytes_to_write = base::as_byte_span(*body).subspan(offset);
  }
  // We shouldn't be able to transfer a string that big over the protocol,
  // but just in case...
  CHECK_LE(bytes_to_write.size(), UINT32_MAX)
      << "Response bodies larger than " << UINT32_MAX << " are not supported";
  mojo::ScopedDataPipeConsumerHandle consumer_handle;
  mojo::ScopedDataPipeProducerHandle producer_handle;
  CHECK_EQ(mojo::CreateDataPipe(bytes_to_write.size(), producer_handle,
                                consumer_handle),
           MOJO_RESULT_OK);

  if (body) {
    size_t actually_written_bytes = 0;
    MojoResult res = producer_handle->WriteData(
        bytes_to_write, MOJO_WRITE_DATA_FLAG_NONE, actually_written_bytes);
    DCHECK_EQ(0u, res);
    DCHECK_EQ(actually_written_bytes, bytes_to_write.size());
  }
  client_->OnReceiveResponse(std::move(response_metadata_->head),
                             std::move(consumer_handle),
                             std::move(response_metadata_->cached_metadata));

  if (response_metadata_->transfer_size)
    client_->OnTransferSizeUpdated(response_metadata_->transfer_size);
  CompleteRequest(response_metadata_->status);
}

void InterceptionJob::ResponseBodyComplete() {
  if (waiting_for_resolution_)
    return;
  // We're here only if client has already told us to proceed with unmodified
  // response.
  SendResponse(body_reader_->body(), 0);
}

void InterceptionJob::StartRequest() {
  DCHECK_EQ(State::kNotStarted, state_);
  DCHECK(!response_metadata_);

  state_ = State::kRequestSent;

  target_factory_->CreateLoaderAndStart(
      loader_.BindNewPipeAndPassReceiver(), create_loader_params_->request_id,
      create_loader_params_->options, create_loader_params_->request,
      client_receiver_.BindNewPipeAndPassRemote(),
      create_loader_params_->traffic_annotation);
  client_receiver_.set_disconnect_handler(
      base::BindOnce(&InterceptionJob::Shutdown, base::Unretained(this)));

  if (priority_)
    loader_->SetPriority(priority_->first, priority_->second);
}

void InterceptionJob::CancelRequest() {
  if (state_ == State::kNotStarted)
    return;
  client_receiver_.reset();
  body_.reset();
  loader_.reset();
  if (body_reader_) {
    body_reader_->CancelWithError(
        "Another command has cancelled the fetch request");
    body_reader_.reset();
  }
  state_ = State::kNotStarted;
}

std::unique_ptr<InterceptedRequestInfo> InterceptionJob::BuildRequestInfo(
    const network::mojom::URLResponseHeadPtr& head) {
  auto result = std::make_unique<InterceptedRequestInfo>();
  result->interception_id = current_id_;
  if (renderer_request_id_.has_value())
    result->renderer_request_id = renderer_request_id_.value();
  result->frame_id = frame_token_;
  blink::mojom::ResourceType resource_type =
      static_cast<blink::mojom::ResourceType>(
          create_loader_params_->request.resource_type);
  result->resource_type = resource_type;
  result->is_navigation =
      resource_type == blink::mojom::ResourceType::kMainFrame ||
      resource_type == blink::mojom::ResourceType::kSubFrame;

  if (head && head->headers)
    result->response_headers = head->headers;
  if (!redirected_request_id_.empty())
    result->redirected_request_id = redirected_request_id_;
  return result;
}

void InterceptionJob::FetchCookies(base::OnceClosure callback) {
  net::CookieOptions options;
  options.set_include_httponly();
  options.set_do_not_update_access_time();

  const network::ResourceRequest& request = create_loader_params_->request;
  DCHECK_EQ(request.url, url_chain_.back());

  bool should_treat_as_first_party =
      GetContentClient()
          ->browser()
          ->ShouldIgnoreSameSiteCookieRestrictionsWhenTopLevel(
              request.site_for_cookies.scheme(),
              request.url.SchemeIsCryptographic());
  bool is_main_frame_navigation =
      request.trusted_params.has_value() &&
      request.trusted_params->isolation_info.request_type() ==
          net::IsolationInfo::RequestType::kMainFrame;
  options.set_same_site_cookie_context(
      net::cookie_util::ComputeSameSiteContextForRequest(
          request.method, url_chain_, request.site_for_cookies,
          request.request_initiator, is_main_frame_navigation,
          should_treat_as_first_party));

  cookie_manager_->GetCookieList(
      request.url, options, net::CookiePartitionKeyCollection::Todo(),
      base::BindOnce(&InterceptionJob::OnGotCookies, base::Unretained(this),
                     std::move(callback)));
}

void InterceptionJob::NotifyClient(
    std::unique_ptr<InterceptedRequestInfo> request_info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!waiting_for_resolution_);

  const network::ResourceRequest request = GetResourceRequestForCookies();

  const bool have_cookies = !!request_cookies_;
  const bool want_cookies = request.SendsCookies();
  CHECK(!have_cookies || want_cookies);

  const bool have_request_bodies = !request_bodies_.empty();
  const bool want_request_bodies = !!request.request_body;
  CHECK(!have_request_bodies || want_request_bodies);

  const int pending_callback_count =
      (have_cookies == want_cookies ? 0 : 1) +
      (have_request_bodies == want_request_bodies ? 0 : 1);

  base::RepeatingClosure closure = BarrierClosure(
      pending_callback_count,
      base::BindOnce(&InterceptionJob::CompleteNotifyingClient,
                     base::Unretained(this), std::move(request_info)));
  if (have_cookies != want_cookies) {
    FetchCookies(closure);
  }
  if (have_request_bodies != want_request_bodies) {
    CHECK(!request_body_collector_);
    request_body_collector_ = RequestBodyCollector::Collect(
        *request.request_body,
        base::BindOnce(&InterceptionJob::OnGotRequestBodies,
                       base::Unretained(this), closure));
  }
}

void InterceptionJob::OnGotCookies(
    base::OnceClosure callback,
    const net::CookieAccessResultList& cookies_with_access_result,
    const net::CookieAccessResultList& excluded_cookies) {
  request_cookies_.emplace(
      cookies_with_access_result.empty()
          ? std::string()
          : net::CanonicalCookie::BuildCookieLine(cookies_with_access_result));
  std::move(callback).Run();
}

void InterceptionJob::OnGotRequestBodies(
    base::OnceClosure callback,
    std::vector<RequestBodyCollector::BodyEntry> bodies) {
  request_bodies_ = std::move(bodies);
  request_body_collector_.reset();
  std::move(callback).Run();
}

void InterceptionJob::CompleteNotifyingClient(
    std::unique_ptr<InterceptedRequestInfo> request_info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!interceptor_)
    return;
  request_info->network_request =
      protocol::NetworkHandler::CreateRequestFromResourceRequest(
          create_loader_params_->request,
          request_cookies_.value_or(std::string()), request_bodies_);

  TRACE_EVENT_NESTABLE_ASYNC_BEGIN0("devtools", "Fetch.requestPaused", this);
  waiting_for_resolution_ = true;
  interceptor_->request_intercepted_callback_.Run(std::move(request_info));
}

void InterceptionJob::CompleteRequest(
    const network::URLLoaderCompletionStatus& status) {
  client_->OnComplete(status);
  Shutdown();
}

void InterceptionJob::Shutdown() {
  if (interceptor_)
    interceptor_->RemoveJob(current_id_);
  delete this;
}

// URLLoader methods
void InterceptionJob::FollowRedirect(
    const std::vector<std::string>& removed_headers,
    const net::HttpRequestHeaders& modified_headers,
    const net::HttpRequestHeaders& modified_cors_exempt_headers,
    const std::optional<GURL>& new_url) {
  DCHECK(!new_url.has_value()) << "Redirect with modified url was not "
                                  "supported yet. crbug.com/845683";
  DCHECK(!waiting_for_resolution_);

  network::ResourceRequest* request = &create_loader_params_->request;
  const net::RedirectInfo& info = *response_metadata_->redirect_info;
  if (request->request_initiator &&
      (!url::IsSameOriginWith(info.new_url, request->url) &&
       !request->request_initiator->IsSameOriginWith(request->url))) {
    tainted_origin_ = true;
  }

  // Save previous headers and revert to the original ones before applying
  // any client changes.
  headers_before_redirect_ = std::make_unique<net::HttpRequestHeaders>(
      create_loader_params_->request.headers);
  if (headers_override_) {
    // Always revert to the first request in the chain.
    HeadersOverride::Revert(std::move(headers_override_));
  }

  bool clear_body = false;
  // Reflect changes to the request that the network service will make on
  // FollowRedirect.
  net::RedirectUtil::UpdateHttpRequest(request->url, request->method, info,
                                       removed_headers, modified_headers,
                                       &request->headers, &clear_body);
  request->cors_exempt_headers.MergeFrom(modified_cors_exempt_headers);
  for (const std::string& name : removed_headers)
    request->cors_exempt_headers.RemoveHeader(name);

  if (clear_body) {
    request->request_body = nullptr;
    request_bodies_.clear();
  }
  request_cookies_.reset();

  request->method = info.new_method;
  request->url = info.new_url;
  request->site_for_cookies = info.new_site_for_cookies;
  request->referrer_policy = info.new_referrer_policy;
  request->referrer = GURL(info.new_referrer);
  if (request->trusted_params) {
    const auto new_origin = url::Origin::Create(info.new_url);
    request->trusted_params->isolation_info =
        request->trusted_params->isolation_info.CreateForRedirect(new_origin);
  }
  response_metadata_.reset();

  UpdateCORSFlag();

  url_chain_.push_back(create_loader_params_->request.url);

  if (interceptor_) {
    redirected_request_id_ = current_id_;
    // Pretend that each redirect hop is a new request -- this is for
    // compatibilty with URLRequestJob-based interception implementation.
    interceptor_->RemoveJob(current_id_);
    redirect_count_++;
    if (StartJobAndMaybeNotify())
      return;
  }
  if (state_ == State::kRedirectReceived) {
    ProcessFollowRedirect(modified_cors_exempt_headers);
    return;
  }

  DCHECK_EQ(State::kNotStarted, state_);
  headers_before_redirect_.reset();
  StartRequest();
}

void InterceptionJob::SetPriority(net::RequestPriority priority,
                                  int32_t intra_priority_value) {
  priority_ = std::make_pair(priority, intra_priority_value);

  if (loader_)
    loader_->SetPriority(priority, intra_priority_value);
}

void InterceptionJob::PauseReadingBodyFromNet() {
  if (!body_reader_ && loader_ && state_ != State::kResponseTaken)
    loader_->PauseReadingBodyFromNet();
}

void InterceptionJob::ResumeReadingBodyFromNet() {
  if (!body_reader_ && loader_ && state_ != State::kResponseTaken)
    loader_->ResumeReadingBodyFromNet();
}

// URLLoaderClient methods
void InterceptionJob::OnReceiveEarlyHints(
    network::mojom::EarlyHintsPtr early_hints) {
  client_->OnReceiveEarlyHints(std::move(early_hints));
}

void InterceptionJob::OnReceiveResponse(
    network::mojom::URLResponseHeadPtr head,
    mojo::ScopedDataPipeConsumerHandle body,
    std::optional<mojo_base::BigBuffer> cached_metadata) {
  state_ = State::kResponseReceived;
  DCHECK(!response_metadata_);
  if (!stages_.Has(InterceptionStage::kResponse)) {
    client_->OnReceiveResponse(std::move(head), std::move(body),
                               std::move(cached_metadata));
    return;
  }
  loader_->PauseReadingBodyFromNet();
  client_receiver_.Pause();
  body_ = std::move(body);

  auto request_info = BuildRequestInfo(head);
  const network::ResourceRequest& request = create_loader_params_->request;
  request_info->is_download =
      request_info->is_navigation &&
      (is_download_ ||
       download_utils::IsDownload(/*browser_context=*/nullptr, request.url,
                                  head->headers.get(), head->mime_type));

  response_metadata_ = std::make_unique<ResponseMetadata>(std::move(head));
  response_metadata_->cached_metadata = std::move(cached_metadata);

  NotifyClient(std::move(request_info));
}

void InterceptionJob::OnReceiveRedirect(
    const net::RedirectInfo& redirect_info,
    network::mojom::URLResponseHeadPtr head) {
  DCHECK_EQ(State::kRequestSent, state_);
  state_ = State::kRedirectReceived;
  response_metadata_ = std::make_unique<ResponseMetadata>(head.Clone());
  response_metadata_->redirect_info =
      std::make_unique<net::RedirectInfo>(redirect_info);

  // Delete some headers to sync the request with what the network
  // service already did.
  RemoveUnsafeRequestHeadersOnRedirect(create_loader_params_->request.headers);
  if (headers_override_) {
    headers_override_->RemoveUnsafeOriginalHeadersOnRedirect();
  }

  if (!stages_.Has(InterceptionStage::kResponse)) {
    client_->OnReceiveRedirect(redirect_info, std::move(head));
    return;
  }

  std::unique_ptr<InterceptedRequestInfo> request_info = BuildRequestInfo(head);
  request_info->redirect_url = redirect_info.new_url.spec();
  NotifyClient(std::move(request_info));
}

void InterceptionJob::OnUploadProgress(int64_t current_position,
                                       int64_t total_size,
                                       OnUploadProgressCallback callback) {
  if (!report_upload_)
    return;
  client_->OnUploadProgress(current_position, total_size, std::move(callback));
}

void InterceptionJob::OnTransferSizeUpdated(int32_t transfer_size_diff) {
  network::RecordOnTransferSizeUpdatedUMA(
      network::OnTransferSizeUpdatedFrom::kInterceptionJob);
  if (ShouldBypassForResponse())
    client_->OnTransferSizeUpdated(transfer_size_diff);
  else
    response_metadata_->transfer_size += transfer_size_diff;
}

void InterceptionJob::StartLoadingResponseBody(
    mojo::ScopedDataPipeConsumerHandle body) {
  if (pending_response_body_pipe_callback_) {
    DCHECK_EQ(State::kResponseTaken, state_);
    DCHECK(!body_reader_);
    std::move(pending_response_body_pipe_callback_)
        .Run(Response::Success(), std::move(body),
             response_metadata_->head->mime_type);
    return;
  }
  DCHECK_EQ(State::kResponseReceived, state_);
  DCHECK(!ShouldBypassForResponse());
  body_reader_->StartReading(std::move(body));
}

void InterceptionJob::OnComplete(
    const network::URLLoaderCompletionStatus& status) {
  // No need to listen to the channel any more, so just reset it, so if the pipe
  // is closed by the other end, |shutdown| isn't run.
  client_receiver_.reset();
  loader_.reset();

  if (!response_metadata_) {
    // If we haven't seen response and get an error completion,
    // treat it as a response and intercept (provided response are
    // being intercepted).
    if (!stages_.Has(InterceptionStage::kResponse) || !status.error_code) {
      CompleteRequest(status);
      return;
    }
    response_metadata_ = std::make_unique<ResponseMetadata>();
    response_metadata_->status = status;
    auto request_info = BuildRequestInfo(nullptr);
    request_info->response_error_code = status.error_code;
    NotifyClient(std::move(request_info));
    return;
  }
  // Since we're not forwarding OnComplete right now, make sure
  // we're in the proper state. The completion is due upon client response.
  DCHECK(state_ == State::kResponseReceived || state_ == State::kResponseTaken)
      << "Unexpected state " << static_cast<int>(state_);
  DCHECK(waiting_for_resolution_);

  response_metadata_->status = status;
}

void InterceptionJob::OnAuthRequest(
    const net::AuthChallengeInfo& auth_info,
    DevToolsURLLoaderInterceptor::HandleAuthRequestCallback callback) {
  DCHECK_EQ(kRequestSent, state_);
  DCHECK(pending_auth_callback_.is_null());
  DCHECK(!waiting_for_resolution_);

  if (!stages_.Has(InterceptionStage::kRequest) || !interceptor_ ||
      !interceptor_->handle_auth_) {
    std::move(callback).Run(true, std::nullopt);
    return;
  }
  state_ = State::kAuthRequired;
  auto request_info = BuildRequestInfo(nullptr);
  request_info->auth_challenge =
      std::make_unique<net::AuthChallengeInfo>(auth_info);
  pending_auth_callback_ = std::move(callback);
  NotifyClient(std::move(request_info));
}

}  // namespace content
