// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/devtools_url_loader_interceptor.h"
#include "base/barrier_closure.h"
#include "base/base64.h"
#include "base/bind.h"
#include "base/no_destructor.h"
#include "base/strings/pattern.h"
#include "base/strings/string_piece.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "base/unguessable_token.h"
#include "content/browser/devtools/protocol/network.h"
#include "content/browser/devtools/protocol/network_handler.h"
#include "content/browser/loader/download_utils_impl.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/public/browser/browser_task_traits.h"
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
#include "services/network/public/cpp/resource_request_body.h"
#include "services/network/public/mojom/network_context.mojom.h"
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
    const base::string16& username,
    const base::string16& password)
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
    std::unique_ptr<HeadersVector> modified_headers)
    : modified_url(std::move(modified_url)),
      modified_method(std::move(modified_method)),
      modified_post_data(std::move(modified_post_data)),
      modified_headers(std::move(modified_headers)) {}

DevToolsURLLoaderInterceptor::Modifications::Modifications(
    base::Optional<net::Error> error_reason,
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
      resource_types.find(resource_type) == resource_types.end()) {
    return false;
  }
  return base::MatchPattern(url, url_pattern);
}

struct CreateLoaderParameters {
  CreateLoaderParameters(
      int32_t routing_id,
      int32_t request_id,
      uint32_t options,
      network::ResourceRequest request,
      net::MutableNetworkTrafficAnnotationTag traffic_annotation)
      : routing_id(routing_id),
        request_id(request_id),
        options(options),
        request(request),
        traffic_annotation(traffic_annotation) {}

  const int32_t routing_id;
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
using GlobalRequestId = std::tuple<int32_t, int32_t, int32_t>;
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
  void OnDataAvailable(const void* data, size_t num_bytes) override {
    DCHECK(!data_complete_);
    body_->data().append(
        std::string(static_cast<const char*>(data), num_bytes));
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

  body_pipe_drainer_.reset(new mojo::DataPipeDrainer(this, std::move(body)));
}

void BodyReader::OnDataComplete() {
  DCHECK(!data_complete_);
  data_complete_ = true;
  body_pipe_drainer_.reset();
  // TODO(caseq): only encode if necessary.
  base::Base64Encode(body_->data(), &encoded_body_);
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
  mojo_base::BigBuffer cached_metadata;
  size_t encoded_length = 0;
  size_t transfer_size = 0;
  network::URLLoaderCompletionStatus status;
};

}  // namespace

class InterceptionJob : public network::mojom::URLLoaderClient,
                        public network::mojom::URLLoader {
 public:
  static InterceptionJob* FindByRequestId(
      const GlobalRequestId& global_req_id) {
    const auto& map = GetInterceptionJobMap();
    auto it = map.find(global_req_id);
    return it == map.end() ? nullptr : it->second;
  }

  InterceptionJob(
      DevToolsURLLoaderInterceptor* interceptor,
      const std::string& id,
      const base::UnguessableToken& frame_token,
      int32_t process_id,
      const base::Optional<std::string>& renderer_request_id,
      std::unique_ptr<CreateLoaderParameters> create_loader_params,
      bool is_download,
      mojo::PendingReceiver<network::mojom::URLLoader> loader_receiver,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client,
      mojo::PendingRemote<network::mojom::URLLoaderFactory> target_factory,
      mojo::PendingRemote<network::mojom::CookieManager> cookie_manager);

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
  static std::map<GlobalRequestId, InterceptionJob*>& GetInterceptionJobMap() {
    static base::NoDestructor<std::map<GlobalRequestId, InterceptionJob*>> inst;
    return *inst;
  }

  ~InterceptionJob() override {
    if (registered_in_global_request_map_) {
      size_t erased = GetInterceptionJobMap().erase(global_req_id_);
      DCHECK_EQ(1lu, erased);
    }
  }

  Response InnerContinueRequest(std::unique_ptr<Modifications> modifications);
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
  void Shutdown();

  std::unique_ptr<InterceptedRequestInfo> BuildRequestInfo(
      const network::mojom::URLResponseHeadPtr& head);
  void NotifyClient(std::unique_ptr<InterceptedRequestInfo> request_info);
  void FetchCookies(
      network::mojom::CookieManager::GetCookieListCallback callback);
  void NotifyClientWithCookies(
      std::unique_ptr<InterceptedRequestInfo> request_info,
      const net::CookieAccessResultList& cookies_with_access_result,
      const net::CookieAccessResultList& excluded_cookies);

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
      const base::Optional<GURL>& new_url) override;
  void SetPriority(net::RequestPriority priority,
                   int32_t intra_priority_value) override;
  void PauseReadingBodyFromNet() override;
  void ResumeReadingBodyFromNet() override;

  // network::mojom::URLLoaderClient methods
  void OnReceiveResponse(network::mojom::URLResponseHeadPtr head) override;
  void OnReceiveRedirect(const net::RedirectInfo& redirect_info,
                         network::mojom::URLResponseHeadPtr head) override;
  void OnUploadProgress(int64_t current_position,
                        int64_t total_size,
                        OnUploadProgressCallback callback) override;
  void OnReceiveCachedMetadata(mojo_base::BigBuffer data) override;
  void OnTransferSizeUpdated(int32_t transfer_size_diff) override;
  void OnStartLoadingResponseBody(
      mojo::ScopedDataPipeConsumerHandle body) override;
  void OnComplete(const network::URLLoaderCompletionStatus& status) override;

  bool CanGetResponseBody(std::string* error_reason);
  bool StartJobAndMaybeNotify();

  void UpdateCORSFlag();
  network::mojom::FetchResponseType CalculateResponseTainting();
  network::ResourceRequest GetResourceRequestForCookies();

  const std::string id_prefix_;
  const GlobalRequestId global_req_id_;
  const base::UnguessableToken frame_token_;
  const bool report_upload_;

  DevToolsURLLoaderInterceptor* interceptor_;
  InterceptionStage stage_;

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

  std::unique_ptr<BodyReader> body_reader_;
  std::unique_ptr<ResponseMetadata> response_metadata_;
  bool registered_in_global_request_map_;

  base::Optional<std::pair<net::RequestPriority, int32_t>> priority_;
  DevToolsURLLoaderInterceptor::HandleAuthRequestCallback
      pending_auth_callback_;
  TakeResponseBodyPipeCallback pending_response_body_pipe_callback_;

  const base::Optional<std::string> renderer_request_id_;

  DISALLOW_COPY_AND_ASSIGN(InterceptionJob);
};

void DevToolsURLLoaderInterceptor::CreateJob(
    const base::UnguessableToken& frame_token,
    int32_t process_id,
    bool is_download,
    const base::Optional<std::string>& renderer_request_id,
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

InterceptionStage DevToolsURLLoaderInterceptor::GetInterceptionStage(
    const GURL& url,
    blink::mojom::ResourceType resource_type) const {
  InterceptionStage stage = InterceptionStage::DONT_INTERCEPT;
  std::string unused;
  std::string url_str = protocol::NetworkHandler::ExtractFragment(url, &unused);
  for (const auto& pattern : patterns_) {
    if (pattern.Matches(url_str, resource_type))
      stage |= pattern.interception_stage;
  }
  return stage;
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
      int32_t routing_id,
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
    int32_t routing_id,
    int32_t request_id,
    uint32_t options,
    const network::ResourceRequest& request,
    mojo::PendingRemote<network::mojom::URLLoaderClient> client,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  DevToolsURLLoaderInterceptor* interceptor = interceptor_.get();
  if (!interceptor_ || request.url.SchemeIs(url::kDataScheme)) {
    target_factory_->CreateLoaderAndStart(
        std::move(loader), routing_id, request_id, options, request,
        std::move(client), traffic_annotation);
    return;
  }
  auto creation_params = std::make_unique<CreateLoaderParameters>(
      routing_id, request_id, options, request, traffic_annotation);
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
    int32_t process_id,
    int32_t routing_id,
    int32_t request_id,
    const net::AuthChallengeInfo& auth_info,
    HandleAuthRequestCallback callback) {
  GlobalRequestId req_id = std::make_tuple(process_id, routing_id, request_id);
  if (auto* job = InterceptionJob::FindByRequestId(req_id))
    job->OnAuthRequest(auth_info, std::move(callback));
  else
    std::move(callback).Run(true, base::nullopt);
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
        mojo::ScopedDataPipeConsumerHandle(), base::EmptyString());
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
    RenderProcessHost* rph,
    const base::UnguessableToken& frame_token,
    bool is_navigation,
    bool is_download,
    network::mojom::URLLoaderFactoryOverride* intercepting_factory) {
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
  int process_id = is_navigation ? 0 : rph->GetID();
  rph->GetStoragePartition()->GetNetworkContext()->GetCookieManager(
      cookie_manager.InitWithNewPipeAndPassReceiver());
  new DevToolsURLLoaderFactoryProxy(
      frame_token, process_id, is_download,
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
    const base::Optional<std::string>& renderer_request_id,
    std::unique_ptr<CreateLoaderParameters> create_loader_params,
    bool is_download,
    mojo::PendingReceiver<network::mojom::URLLoader> loader_receiver,
    mojo::PendingRemote<network::mojom::URLLoaderClient> client,
    mojo::PendingRemote<network::mojom::URLLoaderFactory> target_factory,
    mojo::PendingRemote<network::mojom::CookieManager> cookie_manager)
    : id_prefix_(id),
      global_req_id_(
          std::make_tuple(process_id,
                          create_loader_params->request.render_frame_id,
                          create_loader_params->request_id)),
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
  stage_ = interceptor_->GetInterceptionStage(
      request.url,
      static_cast<blink::mojom::ResourceType>(request.resource_type));

  if (!(stage_ & InterceptionStage::REQUEST))
    return false;

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
  if (!(stage_ & InterceptionStage::RESPONSE)) {
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
  if (!body_reader_) {
    body_reader_ = std::make_unique<BodyReader>(base::BindOnce(
        &InterceptionJob::ResponseBodyComplete, base::Unretained(this)));
    client_receiver_.Resume();
    loader_->ResumeReadingBodyFromNet();
  }
  body_reader_->AddCallback(std::move(callback));
}

void InterceptionJob::TakeResponseBodyPipe(
    TakeResponseBodyPipeCallback callback) {
  std::string error_reason;
  if (!CanGetResponseBody(&error_reason)) {
    std::move(callback).Run(Response::ServerError(std::move(error_reason)),
                            mojo::ScopedDataPipeConsumerHandle(),
                            base::EmptyString());
    return;
  }
  DCHECK_EQ(state_, State::kResponseReceived);
  DCHECK(!!response_metadata_);
  state_ = State::kResponseTaken;
  pending_response_body_pipe_callback_ = std::move(callback);
  client_receiver_.Resume();
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
  stage_ = InterceptionStage::DONT_INTERCEPT;
  interceptor_ = nullptr;
  if (!waiting_for_resolution_)
    return;
  if (state_ == State::kAuthRequired) {
    state_ = State::kRequestSent;
    waiting_for_resolution_ = false;
    std::move(pending_auth_callback_).Run(true, base::nullopt);
    return;
  }
  InnerContinueRequest(std::make_unique<Modifications>());
}

Response InterceptionJob::InnerContinueRequest(
    std::unique_ptr<Modifications> modifications) {
  if (!waiting_for_resolution_)
    return Response::ServerError(
        "Invalid state for continueInterceptedRequest");
  waiting_for_resolution_ = false;

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
    client_->OnComplete(status);
    Shutdown();
    return Response::Success();
  }

  if (modifications->response_headers || modifications->response_body)
    return ProcessResponseOverride(std::move(modifications->response_headers),
                                   std::move(modifications->response_body),
                                   modifications->body_offset);

  if (state_ == State::kFollowRedirect) {
    if (modifications->modified_url.isJust()) {
      CancelRequest();
      // Fall through to the generic logic of re-starting the request
      // at the bottom of the method.
    } else {
      // TODO(caseq): report error if other modifications are present.
      state_ = State::kRequestSent;
      loader_->FollowRedirect({}, {}, {}, base::nullopt);
      return Response::Success();
    }
  }
  if (state_ == State::kRedirectReceived) {
    // TODO(caseq): report error if other modifications are present.
    if (modifications->modified_url.isJust()) {
      std::string location = modifications->modified_url.fromJust();
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
    DCHECK_EQ(State::kResponseReceived, state_);
    DCHECK(!body_reader_);
    client_->OnReceiveResponse(std::move(response_metadata_->head));
    response_metadata_.reset();
    loader_->ResumeReadingBodyFromNet();
    client_receiver_.Resume();
    return Response::Success();
  }

  DCHECK_EQ(State::kNotStarted, state_);
  ApplyModificationsToRequest(std::move(modifications));
  StartRequest();
  return Response::Success();
}

void InterceptionJob::ApplyModificationsToRequest(
    std::unique_ptr<Modifications> modifications) {
  network::ResourceRequest* request = &create_loader_params_->request;

  // Note this redirect is not visible to the page by design. If they want a
  // visible redirect they can mock a response with a 302.
  if (modifications->modified_url.isJust())
    request->url = GURL(modifications->modified_url.fromJust());

  if (modifications->modified_method.isJust())
    request->method = modifications->modified_method.fromJust();

  if (modifications->modified_post_data.isJust()) {
    const auto& post_data = modifications->modified_post_data.fromJust();
    request->request_body = network::ResourceRequestBody::CreateFromBytes(
        reinterpret_cast<const char*>(post_data.data()), post_data.size());
  }

  if (modifications->modified_headers) {
    request->headers.Clear();
    for (const auto& entry : *modifications->modified_headers) {
      if (base::EqualsCaseInsensitiveASCII(entry.first,
                                           net::HttpRequestHeaders::kReferer)) {
        request->referrer = GURL(entry.second);
        request->referrer_policy = net::ReferrerPolicy::NEVER_CLEAR;
      } else {
        request->headers.SetHeader(entry.first, entry.second);
      }
    }
  }
}

void InterceptionJob::ProcessAuthResponse(
    const DevToolsURLLoaderInterceptor::AuthChallengeResponse& response) {
  DCHECK_EQ(kAuthRequired, state_);
  switch (response.response_type) {
    case DevToolsURLLoaderInterceptor::AuthChallengeResponse::kDefault:
      std::move(pending_auth_callback_).Run(true, base::nullopt);
      break;
    case DevToolsURLLoaderInterceptor::AuthChallengeResponse::kCancelAuth:
      std::move(pending_auth_callback_).Run(false, base::nullopt);
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
  if (head->mime_type.empty() && body_size) {
    size_t bytes_to_sniff =
        std::min(body_size, static_cast<size_t>(net::kMaxBytesToSniff));
    net::SniffMimeType(
        base::StringPiece(body->front_as<const char>() + response_body_offset,
                          bytes_to_sniff),
        create_loader_params_->request.url, "",
        net::ForceSniffFileUrlsForHtml::kDisabled, &head->mime_type);
    head->did_mime_sniff = true;
  }
  // TODO(caseq): we're cheating here a bit, raw_headers() have \0's
  // where real headers would have \r\n, but the sizes here
  // probably don't have to be exact.
  size_t headers_size = head->headers->raw_headers().size();
  head->content_length = body_size;
  head->encoded_data_length = headers_size;
  head->encoded_body_length = 0;
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
    GURL redirect_url = create_loader_params_->request.url.Resolve(location);
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
  base::Time response_date;
  base::Optional<base::Time> server_time = base::nullopt;
  if (headers.GetDateValue(&response_date))
    server_time = base::make_optional(response_date);
  base::Time now = base::Time::Now();

  const base::StringPiece name("Set-Cookie");
  std::string cookie_line;
  size_t iter = 0;
  while (headers.EnumerateHeader(&iter, name, &cookie_line)) {
    std::unique_ptr<net::CanonicalCookie> cookie = net::CanonicalCookie::Create(
        create_loader_params_->request.url, cookie_line, now, server_time);
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
  options.set_same_site_cookie_context(
      net::cookie_util::ComputeSameSiteContextForResponse(
          create_loader_params_->request.url,
          create_loader_params_->request.site_for_cookies,
          create_loader_params_->request.request_initiator,
          (create_loader_params_->request.force_ignore_site_for_cookies ||
           should_treat_as_first_party)));

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
  client_->OnReceiveResponse(std::move(response_metadata_->head));
  if (response_metadata_->cached_metadata.size() != 0)
    client_->OnReceiveCachedMetadata(
        std::move(response_metadata_->cached_metadata));

  if (body) {
    DCHECK_LE(offset, body->size());
    size_t body_size = body->size() - offset;
    // We shouldn't be able to transfer a string that big over the protocol,
    // but just in case...
    DCHECK_LE(body_size, UINT32_MAX)
        << "Response bodies larger than " << UINT32_MAX << " are not supported";
    mojo::DataPipe pipe(body_size);
    uint32_t num_bytes = body_size;
    MojoResult res = pipe.producer_handle->WriteData(
        body->front() + offset, &num_bytes, MOJO_WRITE_DATA_FLAG_NONE);
    DCHECK_EQ(0u, res);
    DCHECK_EQ(num_bytes, body_size);
    client_->OnStartLoadingResponseBody(std::move(pipe.consumer_handle));
  }
  if (response_metadata_->transfer_size)
    client_->OnTransferSizeUpdated(response_metadata_->transfer_size);
  client_->OnComplete(response_metadata_->status);
  Shutdown();
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
      loader_.BindNewPipeAndPassReceiver(), create_loader_params_->routing_id,
      create_loader_params_->request_id, create_loader_params_->options,
      create_loader_params_->request,
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
  return result;
}

void InterceptionJob::FetchCookies(
    network::mojom::CookieManager::GetCookieListCallback callback) {
  if (!GetResourceRequestForCookies().SendsCookies()) {
    std::move(callback).Run({}, {});
    return;
  }
  net::CookieOptions options;
  options.set_include_httponly();
  options.set_do_not_update_access_time();

  const network::ResourceRequest& request = create_loader_params_->request;

  bool should_treat_as_first_party =
      GetContentClient()
          ->browser()
          ->ShouldIgnoreSameSiteCookieRestrictionsWhenTopLevel(
              request.site_for_cookies.scheme(),
              request.url.SchemeIsCryptographic());
  options.set_same_site_cookie_context(
      net::cookie_util::ComputeSameSiteContextForRequest(
          request.method, request.url, request.site_for_cookies,
          request.request_initiator,
          (request.force_ignore_site_for_cookies ||
           should_treat_as_first_party)));

  cookie_manager_->GetCookieList(request.url, options, std::move(callback));
}

void InterceptionJob::NotifyClient(
    std::unique_ptr<InterceptedRequestInfo> request_info) {
  FetchCookies(base::BindOnce(&InterceptionJob::NotifyClientWithCookies,
                              base::Unretained(this), std::move(request_info)));
}

void InterceptionJob::NotifyClientWithCookies(
    std::unique_ptr<InterceptedRequestInfo> request_info,
    const net::CookieAccessResultList& cookies_with_access_result,
    const net::CookieAccessResultList& excluded_cookies) {
  if (!interceptor_)
    return;
  std::string cookie_line;
  if (!cookies_with_access_result.empty()) {
    cookie_line =
        net::CanonicalCookie::BuildCookieLine(cookies_with_access_result);
  }
  request_info->network_request =
      protocol::NetworkHandler::CreateRequestFromResourceRequest(
          create_loader_params_->request, cookie_line);

  waiting_for_resolution_ = true;
  interceptor_->request_intercepted_callback_.Run(std::move(request_info));
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
    const base::Optional<GURL>& new_url) {
  DCHECK(!new_url.has_value()) << "Redirect with modified url was not "
                                  "supported yet. crbug.com/845683";
  DCHECK(!waiting_for_resolution_);

  network::ResourceRequest* request = &create_loader_params_->request;
  const net::RedirectInfo& info = *response_metadata_->redirect_info;
  const auto current_origin = url::Origin::Create(request->url);
  if (request->request_initiator &&
      (!url::Origin::Create(info.new_url).IsSameOriginWith(current_origin) &&
       !request->request_initiator->IsSameOriginWith(current_origin))) {
    tainted_origin_ = true;
  }

  bool clear_body = false;
  net::RedirectUtil::UpdateHttpRequest(request->url, request->method, info,
                                       removed_headers, modified_headers,
                                       &request->headers, &clear_body);
  request->cors_exempt_headers.MergeFrom(modified_cors_exempt_headers);
  for (const std::string& name : removed_headers)
    request->cors_exempt_headers.RemoveHeader(name);

  if (clear_body)
    request->request_body = nullptr;
  request->method = info.new_method;
  request->url = info.new_url;
  request->site_for_cookies = info.new_site_for_cookies;
  request->referrer_policy = info.new_referrer_policy;
  request->referrer = GURL(info.new_referrer);
  response_metadata_.reset();

  UpdateCORSFlag();

  if (interceptor_) {
    // Pretend that each redirect hop is a new request -- this is for
    // compatibilty with URLRequestJob-based interception implementation.
    interceptor_->RemoveJob(current_id_);
    redirect_count_++;
    if (StartJobAndMaybeNotify())
      return;
  }
  if (state_ == State::kRedirectReceived) {
    state_ = State::kRequestSent;
    loader_->FollowRedirect(removed_headers, modified_headers,
                            modified_cors_exempt_headers,
                            base::nullopt /* new_url */);
    return;
  }

  DCHECK_EQ(State::kNotStarted, state_);
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
void InterceptionJob::OnReceiveResponse(
    network::mojom::URLResponseHeadPtr head) {
  state_ = State::kResponseReceived;
  DCHECK(!response_metadata_);
  if (!(stage_ & InterceptionStage::RESPONSE)) {
    client_->OnReceiveResponse(std::move(head));
    return;
  }
  loader_->PauseReadingBodyFromNet();
  client_receiver_.Pause();

  auto request_info = BuildRequestInfo(head);
  const network::ResourceRequest& request = create_loader_params_->request;
  request_info->is_download =
      request_info->is_navigation &&
      (is_download_ || download_utils::IsDownload(
                           request.url, head->headers.get(), head->mime_type));

  response_metadata_ = std::make_unique<ResponseMetadata>(std::move(head));

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

  if (!(stage_ & InterceptionStage::RESPONSE)) {
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

void InterceptionJob::OnReceiveCachedMetadata(mojo_base::BigBuffer data) {
  if (ShouldBypassForResponse())
    client_->OnReceiveCachedMetadata(std::move(data));
  else
    response_metadata_->cached_metadata = std::move(data);
}

void InterceptionJob::OnTransferSizeUpdated(int32_t transfer_size_diff) {
  if (ShouldBypassForResponse())
    client_->OnTransferSizeUpdated(transfer_size_diff);
  else
    response_metadata_->transfer_size += transfer_size_diff;
}

void InterceptionJob::OnStartLoadingResponseBody(
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
  if (ShouldBypassForResponse())
    client_->OnStartLoadingResponseBody(std::move(body));
  else
    body_reader_->StartReading(std::move(body));
}

void InterceptionJob::OnComplete(
    const network::URLLoaderCompletionStatus& status) {
  // Essentially ShouldBypassForResponse(), but skip DCHECKs
  // since this may be called in any state during shutdown.
  if (!response_metadata_) {
    client_->OnComplete(status);
    Shutdown();
    return;
  }
  response_metadata_->status = status;
  // No need to listen to the channel any more, so just reset it, so if the pipe
  // is closed by the other end, |shutdown| isn't run.
  client_receiver_.reset();
  loader_.reset();
}

void InterceptionJob::OnAuthRequest(
    const net::AuthChallengeInfo& auth_info,
    DevToolsURLLoaderInterceptor::HandleAuthRequestCallback callback) {
  DCHECK_EQ(kRequestSent, state_);
  DCHECK(pending_auth_callback_.is_null());
  DCHECK(!waiting_for_resolution_);

  if (!(stage_ & InterceptionStage::REQUEST) || !interceptor_ ||
      !interceptor_->handle_auth_) {
    std::move(callback).Run(true, base::nullopt);
    return;
  }
  state_ = State::kAuthRequired;
  auto request_info = BuildRequestInfo(nullptr);
  request_info->auth_challenge =
      std::make_unique<net::AuthChallengeInfo>(auth_info);
  pending_auth_callback_ = std::move(callback);
  NotifyClient(std::move(request_info));
}

DevToolsURLLoaderFactoryAdapter::DevToolsURLLoaderFactoryAdapter(
    mojo::PendingRemote<network::mojom::URLLoaderFactory> factory)
    : factory_(std::move(factory)) {}

DevToolsURLLoaderFactoryAdapter::~DevToolsURLLoaderFactoryAdapter() = default;

void DevToolsURLLoaderFactoryAdapter::CreateLoaderAndStart(
    mojo::PendingReceiver<network::mojom::URLLoader> loader,
    int32_t routing_id,
    int32_t request_id,
    uint32_t options,
    const network::ResourceRequest& request,
    mojo::PendingRemote<network::mojom::URLLoaderClient> client,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation) {
  factory_->CreateLoaderAndStart(std::move(loader), routing_id, request_id,
                                 options, request, std::move(client),
                                 traffic_annotation);
}

void DevToolsURLLoaderFactoryAdapter::Clone(
    mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver) {
  factory_->Clone(std::move(receiver));
}

}  // namespace content
