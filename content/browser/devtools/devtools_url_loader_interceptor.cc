// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/devtools_url_loader_interceptor.h"
#include "base/barrier_closure.h"
#include "base/base64.h"
#include "base/no_destructor.h"
#include "base/strings/stringprintf.h"
#include "base/task/post_task.h"
#include "base/time/time.h"
#include "base/unguessable_token.h"
#include "content/browser/devtools/protocol/network.h"
#include "content/browser/devtools/protocol/network_handler.h"
#include "content/browser/frame_host/frame_tree_node.h"
#include "content/browser/loader/download_utils_impl.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "mojo/public/cpp/system/data_pipe_drainer.h"
#include "net/base/load_flags.h"
#include "net/base/mime_sniffer.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/http/http_util.h"
#include "net/url_request/redirect_util.h"
#include "net/url_request/url_request.h"
#include "services/network/public/cpp/resource_request_body.h"
#include "third_party/blink/public/platform/resource_request_blocked_reason.h"

namespace content {

namespace {

using RequestInterceptedCallback =
    DevToolsNetworkInterceptor::RequestInterceptedCallback;
using ContinueInterceptedRequestCallback =
    DevToolsNetworkInterceptor::ContinueInterceptedRequestCallback;
using GetResponseBodyForInterceptionCallback =
    DevToolsNetworkInterceptor::GetResponseBodyForInterceptionCallback;
using TakeResponseBodyPipeCallback =
    DevToolsNetworkInterceptor::TakeResponseBodyPipeCallback;
using Modifications = DevToolsNetworkInterceptor::Modifications;
using InterceptionStage = DevToolsNetworkInterceptor::InterceptionStage;
using protocol::Response;
using GlobalRequestId = std::tuple<int32_t, int32_t, int32_t>;

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

class BodyReader : public mojo::DataPipeDrainer::Client {
 public:
  explicit BodyReader(base::OnceClosure download_complete_callback)
      : download_complete_callback_(std::move(download_complete_callback)) {}

  void StartReading(mojo::ScopedDataPipeConsumerHandle body);

  void AddCallback(
      std::unique_ptr<GetResponseBodyForInterceptionCallback> callback) {
    callbacks_.push_back(std::move(callback));
    if (data_complete_) {
      DCHECK_EQ(1UL, callbacks_.size());
      base::PostTaskWithTraits(
          FROM_HERE, {BrowserThread::UI},
          base::BindOnce(&BodyReader::DispatchBodyOnUI, std::move(callbacks_),
                         encoded_body_));
    }
  }

  bool data_complete() const { return data_complete_; }
  const std::string& body() const { return body_; }

  void CancelWithError(std::string error) {
    base::PostTaskWithTraits(
        FROM_HERE, {BrowserThread::UI},
        base::BindOnce(&BodyReader::DispatchErrorOnUI, std::move(callbacks_),
                       std::move(error)));
  }

 private:
  using CallbackVector =
      std::vector<std::unique_ptr<GetResponseBodyForInterceptionCallback>>;
  static void DispatchBodyOnUI(const CallbackVector& callbacks,
                               const std::string& body);
  static void DispatchErrorOnUI(const CallbackVector& callbacks,
                                const std::string& error);

  void OnDataAvailable(const void* data, size_t num_bytes) override {
    DCHECK(!data_complete_);
    body_.append(std::string(static_cast<const char*>(data), num_bytes));
  }

  void OnDataComplete() override;

  std::unique_ptr<mojo::DataPipeDrainer> body_pipe_drainer_;
  CallbackVector callbacks_;
  base::OnceClosure download_complete_callback_;
  std::string body_;
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
  base::Base64Encode(body_, &encoded_body_);
  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::UI},
      base::BindOnce(&BodyReader::DispatchBodyOnUI, std::move(callbacks_),
                     encoded_body_));
  std::move(download_complete_callback_).Run();
}

// static
void BodyReader::DispatchBodyOnUI(const CallbackVector& callbacks,
                                  const std::string& encoded_body) {
  for (const auto& cb : callbacks)
    cb->sendSuccess(encoded_body, true);
}

// static
void BodyReader::DispatchErrorOnUI(const CallbackVector& callbacks,
                                   const std::string& error) {
  for (const auto& cb : callbacks)
    cb->sendFailure(Response::Error(error));
}

struct ResponseMetadata {
  ResponseMetadata() = default;
  explicit ResponseMetadata(const network::ResourceResponseHead& head)
      : head(head) {}

  network::ResourceResponseHead head;
  std::unique_ptr<net::RedirectInfo> redirect_info;
  std::vector<uint8_t> cached_metadata;
  size_t encoded_length = 0;
  size_t transfer_size = 0;
  network::URLLoaderCompletionStatus status;
};

class InterceptionJob : public network::mojom::URLLoaderClient,
                        public network::mojom::URLLoader {
 public:
  static InterceptionJob* FindByRequestId(
      const GlobalRequestId& global_req_id) {
    const auto& map = GetInterceptionJobMap();
    auto it = map.find(global_req_id);
    return it == map.end() ? nullptr : it->second;
  }

  InterceptionJob(DevToolsURLLoaderInterceptor::Impl* interceptor,
                  const std::string& id,
                  const base::UnguessableToken& frame_token,
                  int32_t process_id,
                  std::unique_ptr<CreateLoaderParameters> create_loader_params,
                  bool is_download,
                  network::mojom::URLLoaderRequest loader_request,
                  network::mojom::URLLoaderClientPtr client,
                  network::mojom::URLLoaderFactoryPtr target_factory,
                  network::mojom::CookieManagerPtr cookie_manager);

  void GetResponseBody(
      std::unique_ptr<GetResponseBodyForInterceptionCallback> callback);
  void TakeResponseBodyPipe(TakeResponseBodyPipeCallback callback);
  void ContinueInterceptedRequest(
      std::unique_ptr<Modifications> modifications,
      std::unique_ptr<ContinueInterceptedRequestCallback> callback);
  void Detach();

  void OnAuthRequest(
      const scoped_refptr<net::AuthChallengeInfo>& auth_info,
      DevToolsURLLoaderInterceptor::HandleAuthRequestCallback callback);

 private:
  static std::map<GlobalRequestId, InterceptionJob*>& GetInterceptionJobMap() {
    DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
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
      const DevToolsNetworkInterceptor::AuthChallengeResponse&
          auth_challenge_response);
  Response ProcessResponseOverride(
      scoped_refptr<net::HttpResponseHeaders> headers,
      std::unique_ptr<std::string> body);
  void ProcessRedirectByClient(const GURL& redirect_url);
  void ProcessSetCookies(const net::HttpResponseHeaders& response_headers,
                         base::OnceClosure callback);
  void SendResponse(const base::StringPiece& body);
  void ApplyModificationsToRequest(
      std::unique_ptr<Modifications> modifications);

  void StartRequest();
  void CancelRequest();
  void Shutdown();

  std::unique_ptr<InterceptedRequestInfo> BuildRequestInfo(
      const network::ResourceResponseHead* head);
  void NotifyClient(std::unique_ptr<InterceptedRequestInfo> request_info);
  void FetchCookies(
      base::OnceCallback<void(const std::vector<net::CanonicalCookie>&)>
          callback);
  void NotifyClientWithCookies(
      std::unique_ptr<InterceptedRequestInfo> request_info,
      const std::vector<net::CanonicalCookie>& cookie_list);

  void ResponseBodyComplete();

  bool ShouldBypassForResponse() const {
    if (state_ == State::kResponseTaken)
      return false;
    DCHECK_EQ(!!response_metadata_, !!body_reader_);
    DCHECK_EQ(state_, State::kResponseReceived);
    return !response_metadata_;
  }

  // network::mojom::URLLoader methods
  void FollowRedirect(const base::Optional<std::vector<std::string>>&
                          to_be_removed_request_headers,
                      const base::Optional<net::HttpRequestHeaders>&
                          modified_request_headers) override;
  void ProceedWithResponse() override;
  void SetPriority(net::RequestPriority priority,
                   int32_t intra_priority_value) override;
  void PauseReadingBodyFromNet() override;
  void ResumeReadingBodyFromNet() override;

  // network::mojom::URLLoaderClient methods
  void OnReceiveResponse(const network::ResourceResponseHead& head) override;
  void OnReceiveRedirect(const net::RedirectInfo& redirect_info,
                         const network::ResourceResponseHead& head) override;
  void OnUploadProgress(int64_t current_position,
                        int64_t total_size,
                        OnUploadProgressCallback callback) override;
  void OnReceiveCachedMetadata(const std::vector<uint8_t>& data) override;
  void OnTransferSizeUpdated(int32_t transfer_size_diff) override;
  void OnStartLoadingResponseBody(
      mojo::ScopedDataPipeConsumerHandle body) override;
  void OnComplete(const network::URLLoaderCompletionStatus& status) override;

  bool CanGetResponseBody(std::string* error_reason);
  void UpdateIdAndRegister();

  const std::string id_prefix_;
  const GlobalRequestId global_req_id_;
  const base::UnguessableToken frame_token_;
  const base::TimeTicks start_ticks_;
  const base::Time start_time_;
  const bool report_upload_;

  DevToolsURLLoaderInterceptor::Impl* interceptor_;
  InterceptionStage stage_;

  std::unique_ptr<CreateLoaderParameters> create_loader_params_;
  const bool is_download_;

  mojo::Binding<network::mojom::URLLoaderClient> client_binding_;
  mojo::Binding<network::mojom::URLLoader> loader_binding_;

  network::mojom::URLLoaderClientPtr client_;
  network::mojom::URLLoaderPtr loader_;
  network::mojom::URLLoaderFactoryPtr target_factory_;
  network::mojom::CookieManagerPtr cookie_manager_;

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
  bool waiting_for_resolution_;
  int redirect_count_;
  std::string current_id_;

  std::unique_ptr<BodyReader> body_reader_;
  std::unique_ptr<ResponseMetadata> response_metadata_;
  bool registered_in_global_request_map_;

  base::Optional<std::pair<net::RequestPriority, int32_t>> priority_;
  DevToolsURLLoaderInterceptor::HandleAuthRequestCallback
      pending_auth_callback_;
  TakeResponseBodyPipeCallback pending_response_body_pipe_callback_;

  DISALLOW_COPY_AND_ASSIGN(InterceptionJob);
};

}  // namespace

class DevToolsURLLoaderInterceptor::Impl
    : public base::SupportsWeakPtr<DevToolsURLLoaderInterceptor::Impl> {
 public:
  explicit Impl(RequestInterceptedCallback callback)
      : request_intercepted_callback_(callback) {}
  ~Impl() {
    for (auto const& entry : jobs_)
      entry.second->Detach();
  }

  void CreateJob(const base::UnguessableToken& frame_token,
                 int32_t process_id,
                 bool is_download,
                 std::unique_ptr<CreateLoaderParameters> create_params,
                 network::mojom::URLLoaderRequest loader_request,
                 network::mojom::URLLoaderClientPtr client,
                 network::mojom::URLLoaderFactoryPtr target_factory,
                 network::mojom::CookieManagerPtr cookie_manager) {
    DCHECK(!frame_token.is_empty());

    static int last_id = 0;

    std::string id = base::StringPrintf("interception-job-%d", ++last_id);
    // This class will manage its own life time to match the loader client.
    new InterceptionJob(this, std::move(id), frame_token, process_id,
                        std::move(create_params), is_download,
                        std::move(loader_request), std::move(client),
                        std::move(target_factory), std::move(cookie_manager));
  }

  void SetPatterns(std::vector<DevToolsNetworkInterceptor::Pattern> patterns) {
    patterns_ = std::move(patterns);
  }

  InterceptionStage GetInterceptionStage(const GURL& url,
                                         ResourceType resource_type) const {
    InterceptionStage stage = InterceptionStage::DONT_INTERCEPT;
    std::string unused;
    std::string url_str =
        protocol::NetworkHandler::ExtractFragment(url, &unused);
    for (const auto& pattern : patterns_) {
      if (pattern.Matches(url_str, resource_type))
        stage |= pattern.interception_stage;
    }
    return stage;
  }

  void GetResponseBody(
      const std::string& interception_id,
      std::unique_ptr<GetResponseBodyForInterceptionCallback> callback) {
    if (InterceptionJob* job = FindJob(interception_id, &callback))
      job->GetResponseBody(std::move(callback));
  }

  void TakeResponseBodyPipe(
      const std::string& interception_id,
      DevToolsNetworkInterceptor::TakeResponseBodyPipeCallback callback) {
    auto it = jobs_.find(interception_id);
    if (it == jobs_.end()) {
      std::move(callback).Run(
          protocol::Response::InvalidParams("Invalid InterceptionId."),
          mojo::ScopedDataPipeConsumerHandle(), std::string());
      return;
    }
    it->second->TakeResponseBodyPipe(std::move(callback));
  }

  void ContinueInterceptedRequest(
      const std::string& interception_id,
      std::unique_ptr<Modifications> modifications,
      std::unique_ptr<ContinueInterceptedRequestCallback> callback) {
    if (InterceptionJob* job = FindJob(interception_id, &callback)) {
      job->ContinueInterceptedRequest(std::move(modifications),
                                      std::move(callback));
    }
  }

 private:
  friend class content::InterceptionJob;

  template <typename Callback>
  InterceptionJob* FindJob(const std::string& id,
                           std::unique_ptr<Callback>* callback) {
    auto it = jobs_.find(id);
    if (it != jobs_.end())
      return it->second;
    base::PostTaskWithTraits(
        FROM_HERE, {BrowserThread::UI},
        base::BindOnce(
            &Callback::sendFailure, std::move(*callback),
            protocol::Response::InvalidParams("Invalid InterceptionId.")));
    return nullptr;
  }

  void RemoveJob(const std::string& id) { jobs_.erase(id); }
  void AddJob(const std::string& id, InterceptionJob* job) {
    jobs_.emplace(id, job);
  }

  std::map<std::string, InterceptionJob*> jobs_;
  RequestInterceptedCallback request_intercepted_callback_;
  std::vector<DevToolsNetworkInterceptor::Pattern> patterns_;

  DISALLOW_COPY_AND_ASSIGN(Impl);
};

class DevToolsURLLoaderFactoryProxy : public network::mojom::URLLoaderFactory {
 public:
  DevToolsURLLoaderFactoryProxy(
      const base::UnguessableToken& frame_token,
      int32_t process_id,
      bool is_download,
      network::mojom::URLLoaderFactoryRequest loader_request,
      network::mojom::URLLoaderFactoryPtrInfo target_factory_info,
      network::mojom::CookieManagerPtrInfo cookie_manager,
      base::WeakPtr<DevToolsURLLoaderInterceptor::Impl> interceptor);
  ~DevToolsURLLoaderFactoryProxy() override;

 private:
  // network::mojom::URLLoaderFactory implementation
  void CreateLoaderAndStart(network::mojom::URLLoaderRequest loader,
                            int32_t routing_id,
                            int32_t request_id,
                            uint32_t options,
                            const network::ResourceRequest& request,
                            network::mojom::URLLoaderClientPtr client,
                            const net::MutableNetworkTrafficAnnotationTag&
                                traffic_annotation) override;
  void Clone(network::mojom::URLLoaderFactoryRequest request) override;

  void StartOnIO(network::mojom::URLLoaderFactoryRequest loader_request,
                 network::mojom::URLLoaderFactoryPtrInfo target_factory_info,
                 network::mojom::CookieManagerPtrInfo cookie_manager);
  void OnProxyBindingError();
  void OnTargetFactoryError();

  const base::UnguessableToken frame_token_;
  const int32_t process_id_;
  const bool is_download_;

  network::mojom::URLLoaderFactoryPtr target_factory_;
  network::mojom::CookieManagerPtr cookie_manager_;
  base::WeakPtr<DevToolsURLLoaderInterceptor::Impl> interceptor_;
  mojo::BindingSet<network::mojom::URLLoaderFactory> bindings_;

  SEQUENCE_CHECKER(sequence_checker_);
};

DevToolsURLLoaderFactoryProxy::DevToolsURLLoaderFactoryProxy(
    const base::UnguessableToken& frame_token,
    int32_t process_id,
    bool is_download,
    network::mojom::URLLoaderFactoryRequest loader_request,
    network::mojom::URLLoaderFactoryPtrInfo target_factory_info,
    network::mojom::CookieManagerPtrInfo cookie_manager,
    base::WeakPtr<DevToolsURLLoaderInterceptor::Impl> interceptor)
    : frame_token_(frame_token),
      process_id_(process_id),
      is_download_(is_download),
      interceptor_(std::move(interceptor)) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(&DevToolsURLLoaderFactoryProxy::StartOnIO,
                     base::Unretained(this), std::move(loader_request),
                     std::move(target_factory_info),
                     std::move(cookie_manager)));
}

DevToolsURLLoaderFactoryProxy::~DevToolsURLLoaderFactoryProxy() {}

void DevToolsURLLoaderFactoryProxy::CreateLoaderAndStart(
    network::mojom::URLLoaderRequest loader,
    int32_t routing_id,
    int32_t request_id,
    uint32_t options,
    const network::ResourceRequest& request,
    network::mojom::URLLoaderClientPtr client,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  DevToolsURLLoaderInterceptor::Impl* interceptor = interceptor_.get();
  if (!interceptor_) {
    target_factory_->CreateLoaderAndStart(
        std::move(loader), routing_id, request_id, options, request,
        std::move(client), traffic_annotation);
    return;
  }
  auto creation_params = std::make_unique<CreateLoaderParameters>(
      routing_id, request_id, options, request, traffic_annotation);
  network::mojom::URLLoaderFactoryPtr factory_clone;
  target_factory_->Clone(MakeRequest(&factory_clone));
  network::mojom::CookieManagerPtr cookie_manager_clone;
  cookie_manager_->CloneInterface(mojo::MakeRequest(&cookie_manager_clone));
  interceptor->CreateJob(frame_token_, process_id_, is_download_,
                         std::move(creation_params), std::move(loader),
                         std::move(client), std::move(factory_clone),
                         std::move(cookie_manager_clone));
}

void DevToolsURLLoaderFactoryProxy::StartOnIO(
    network::mojom::URLLoaderFactoryRequest loader_request,
    network::mojom::URLLoaderFactoryPtrInfo target_factory_info,
    network::mojom::CookieManagerPtrInfo cookie_manager) {
  target_factory_.Bind(std::move(target_factory_info));
  target_factory_.set_connection_error_handler(
      base::BindOnce(&DevToolsURLLoaderFactoryProxy::OnTargetFactoryError,
                     base::Unretained(this)));

  bindings_.AddBinding(this, std::move(loader_request));
  bindings_.set_connection_error_handler(
      base::BindRepeating(&DevToolsURLLoaderFactoryProxy::OnProxyBindingError,
                          base::Unretained(this)));

  cookie_manager_.Bind(std::move(cookie_manager));
  cookie_manager_.set_connection_error_handler(
      base::BindOnce(&DevToolsURLLoaderFactoryProxy::OnTargetFactoryError,
                     base::Unretained(this)));
}

void DevToolsURLLoaderFactoryProxy::Clone(
    network::mojom::URLLoaderFactoryRequest request) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  bindings_.AddBinding(this, std::move(request));
}

void DevToolsURLLoaderFactoryProxy::OnTargetFactoryError() {
  delete this;
}

void DevToolsURLLoaderFactoryProxy::OnProxyBindingError() {
  if (bindings_.empty())
    delete this;
}

// static
void DevToolsURLLoaderInterceptor::HandleAuthRequest(
    int32_t process_id,
    int32_t routing_id,
    int32_t request_id,
    const scoped_refptr<net::AuthChallengeInfo>& auth_info,
    HandleAuthRequestCallback callback) {
  GlobalRequestId req_id = std::make_tuple(process_id, routing_id, request_id);
  if (auto* job = InterceptionJob::FindByRequestId(req_id))
    job->OnAuthRequest(auth_info, std::move(callback));
  else
    std::move(callback).Run(true, base::nullopt);
}

DevToolsURLLoaderInterceptor::DevToolsURLLoaderInterceptor(
    RequestInterceptedCallback callback)
    : enabled_(false),
      impl_(new DevToolsURLLoaderInterceptor::Impl(std::move(callback)),
            base::OnTaskRunnerDeleter(
                base::CreateSingleThreadTaskRunnerWithTraits(
                    {BrowserThread::IO}))),
      weak_impl_(impl_->AsWeakPtr()) {}

DevToolsURLLoaderInterceptor::~DevToolsURLLoaderInterceptor() = default;

void DevToolsURLLoaderInterceptor::SetPatterns(
    std::vector<DevToolsNetworkInterceptor::Pattern> patterns) {
  enabled_ = !!patterns.size();
  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(&Impl::SetPatterns, base::Unretained(impl_.get()),
                     std::move(patterns)));
}

void DevToolsURLLoaderInterceptor::GetResponseBody(
    const std::string& interception_id,
    std::unique_ptr<GetResponseBodyForInterceptionCallback> callback) {
  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(&Impl::GetResponseBody, base::Unretained(impl_.get()),
                     interception_id, std::move(callback)));
}

void DevToolsURLLoaderInterceptor::TakeResponseBodyPipe(
    const std::string& interception_id,
    DevToolsNetworkInterceptor::TakeResponseBodyPipeCallback callback) {
  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(&Impl::TakeResponseBodyPipe, base::Unretained(impl_.get()),
                     interception_id, std::move(callback)));
}

void DevToolsURLLoaderInterceptor::ContinueInterceptedRequest(
    const std::string& interception_id,
    std::unique_ptr<Modifications> modifications,
    std::unique_ptr<ContinueInterceptedRequestCallback> callback) {
  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(&Impl::ContinueInterceptedRequest,
                     base::Unretained(impl_.get()), interception_id,
                     std::move(modifications), std::move(callback)));
}

bool DevToolsURLLoaderInterceptor::CreateProxyForInterception(
    RenderFrameHostImpl* rfh,
    bool is_navigation,
    bool is_download,
    network::mojom::URLLoaderFactoryRequest* request) const {
  if (!enabled_)
    return false;

  network::mojom::URLLoaderFactoryRequest original_request =
      std::move(*request);
  network::mojom::URLLoaderFactoryPtrInfo target_ptr_info;
  *request = MakeRequest(&target_ptr_info);
  network::mojom::CookieManagerPtrInfo cookie_manager;
  int process_id = is_navigation ? 0 : rfh->GetProcess()->GetID();
  rfh->GetProcess()
      ->GetStoragePartition()
      ->GetNetworkContext()
      ->GetCookieManager(mojo::MakeRequest(&cookie_manager));
  new DevToolsURLLoaderFactoryProxy(rfh->GetDevToolsFrameToken(), process_id,
                                    is_download, std::move(original_request),
                                    std::move(target_ptr_info),
                                    std::move(cookie_manager), weak_impl_);
  return true;
}

InterceptionJob::InterceptionJob(
    DevToolsURLLoaderInterceptor::Impl* interceptor,
    const std::string& id,
    const base::UnguessableToken& frame_token,
    int process_id,
    std::unique_ptr<CreateLoaderParameters> create_loader_params,
    bool is_download,
    network::mojom::URLLoaderRequest loader_request,
    network::mojom::URLLoaderClientPtr client,
    network::mojom::URLLoaderFactoryPtr target_factory,
    network::mojom::CookieManagerPtr cookie_manager)
    : id_prefix_(id),
      global_req_id_(
          std::make_tuple(process_id,
                          create_loader_params->request.render_frame_id,
                          create_loader_params->request_id)),
      frame_token_(frame_token),
      start_ticks_(base::TimeTicks::Now()),
      start_time_(base::Time::Now()),
      report_upload_(!!create_loader_params->request.request_body),
      interceptor_(interceptor),
      create_loader_params_(std::move(create_loader_params)),
      is_download_(is_download),
      client_binding_(this),
      loader_binding_(this),
      client_(std::move(client)),
      target_factory_(std::move(target_factory)),
      cookie_manager_(std::move(cookie_manager)),
      state_(kNotStarted),
      waiting_for_resolution_(false),
      redirect_count_(0) {
  UpdateIdAndRegister();
  const network::ResourceRequest& request = create_loader_params_->request;
  stage_ = interceptor_->GetInterceptionStage(
      request.url, static_cast<ResourceType>(request.resource_type));

  loader_binding_.Bind(std::move(loader_request));
  loader_binding_.set_connection_error_handler(
      base::BindOnce(&InterceptionJob::Shutdown, base::Unretained(this)));

  auto& job_map = GetInterceptionJobMap();
  // TODO(caseq): for now, all auth requests will go to the top-level job.
  // Figure out if we need anything smarter here.
  registered_in_global_request_map_ =
      job_map.emplace(global_req_id_, this).second;

  if (stage_ & InterceptionStage::REQUEST) {
    NotifyClient(BuildRequestInfo(nullptr));
    return;
  }

  StartRequest();
}

void InterceptionJob::UpdateIdAndRegister() {
  current_id_ = id_prefix_ + base::StringPrintf(".%d", redirect_count_);
  interceptor_->AddJob(current_id_, this);
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
    std::unique_ptr<GetResponseBodyForInterceptionCallback> callback) {
  std::string error_reason;
  if (!CanGetResponseBody(&error_reason)) {
    base::PostTaskWithTraits(
        FROM_HERE, {BrowserThread::UI},
        base::BindOnce(&GetResponseBodyForInterceptionCallback::sendFailure,
                       std::move(callback),
                       Response::Error(std::move(error_reason))));
    return;
  }
  if (!body_reader_) {
    body_reader_ = std::make_unique<BodyReader>(base::BindOnce(
        &InterceptionJob::ResponseBodyComplete, base::Unretained(this)));
    client_binding_.ResumeIncomingMethodCallProcessing();
    loader_->ResumeReadingBodyFromNet();
  }
  body_reader_->AddCallback(std::move(callback));
}

void InterceptionJob::TakeResponseBodyPipe(
    TakeResponseBodyPipeCallback callback) {
  std::string error_reason;
  if (!CanGetResponseBody(&error_reason)) {
    base::PostTaskWithTraits(
        FROM_HERE, {BrowserThread::UI},
        base::BindOnce(std::move(callback),
                       Response::Error(std::move(error_reason)),
                       mojo::ScopedDataPipeConsumerHandle(), std::string()));
    return;
  }
  DCHECK_EQ(state_, State::kResponseReceived);
  DCHECK(!!response_metadata_);
  state_ = State::kResponseTaken;
  pending_response_body_pipe_callback_ = std::move(callback);
  client_binding_.ResumeIncomingMethodCallProcessing();
  loader_->ResumeReadingBodyFromNet();
}

void InterceptionJob::ContinueInterceptedRequest(
    std::unique_ptr<Modifications> modifications,
    std::unique_ptr<ContinueInterceptedRequestCallback> callback) {
  Response response = InnerContinueRequest(std::move(modifications));
  // |this| may be destroyed at this point.
  bool success = response.isSuccess();
  base::OnceClosure task =
      success ? base::BindOnce(&ContinueInterceptedRequestCallback::sendSuccess,
                               std::move(callback))
              : base::BindOnce(&ContinueInterceptedRequestCallback::sendFailure,
                               std::move(callback), std::move(response));
  base::PostTaskWithTraits(FROM_HERE, {BrowserThread::UI}, std::move(task));
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
    return Response::Error("Invalid state for continueInterceptedRequest");
  waiting_for_resolution_ = false;

  if (state_ == State::kAuthRequired) {
    if (!modifications->auth_challenge_response)
      return Response::InvalidParams("authChallengeResponse required.");
    ProcessAuthResponse(*modifications->auth_challenge_response);
    return Response::OK();
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
    return Response::OK();
  }

  if (modifications->response_headers || modifications->response_body)
    return ProcessResponseOverride(std::move(modifications->response_headers),
                                   std::move(modifications->response_body));

  if (state_ == State::kFollowRedirect) {
    if (modifications->modified_url.isJust()) {
      CancelRequest();
      // Fall through to the generic logic of re-starting the request
      // at the bottom of the method.
    } else {
      // TODO(caseq): report error if other modifications are present.
      state_ = State::kRequestSent;
      loader_->FollowRedirect(base::nullopt, base::nullopt);
      return Response::OK();
    }
  }
  if (state_ == State::kRedirectReceived) {
    // TODO(caseq): report error if other modifications are present.
    if (modifications->modified_url.isJust()) {
      std::string location = modifications->modified_url.fromJust();
      CancelRequest();
      auto* headers = response_metadata_->head.headers.get();
      headers->RemoveHeader("location");
      headers->AddHeader("location: " + location);
      GURL redirect_url = create_loader_params_->request.url.Resolve(location);
      if (!redirect_url.is_valid())
        return Response::Error("Invalid modified URL");
      ProcessRedirectByClient(redirect_url);
      return Response::OK();
    }
    client_->OnReceiveRedirect(*response_metadata_->redirect_info,
                               response_metadata_->head);
    return Response::OK();
  }

  if (body_reader_) {
    if (body_reader_->data_complete())
      SendResponse(body_reader_->body());

    // There are read callbacks pending, so let the reader do its job and come
    // back when it's done.
    return Response::OK();
  }

  if (response_metadata_) {
    if (state_ == State::kResponseTaken) {
      return Response::InvalidParams(
          "Unable to continue request as is after body is taken");
    }
    // TODO(caseq): report error if other modifications are present.
    DCHECK_EQ(State::kResponseReceived, state_);
    DCHECK(!body_reader_);
    client_->OnReceiveResponse(response_metadata_->head);
    response_metadata_.reset();
    loader_->ResumeReadingBodyFromNet();
    client_binding_.ResumeIncomingMethodCallProcessing();
    return Response::OK();
  }

  DCHECK_EQ(State::kNotStarted, state_);
  ApplyModificationsToRequest(std::move(modifications));
  StartRequest();
  return Response::OK();
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
    const std::string& post_data = modifications->modified_post_data.fromJust();
    request->request_body = network::ResourceRequestBody::CreateFromBytes(
        post_data.data(), post_data.size());
  }

  if (modifications->modified_headers) {
    request->headers.Clear();
    for (const auto& entry : *modifications->modified_headers) {
      if (base::EqualsCaseInsensitiveASCII(entry.first,
                                           net::HttpRequestHeaders::kReferer)) {
        request->referrer = GURL(entry.second);
        request->referrer_policy = net::URLRequest::NEVER_CLEAR_REFERRER;
      } else {
        request->headers.SetHeader(entry.first, entry.second);
      }
    }
  }
}

void InterceptionJob::ProcessAuthResponse(
    const DevToolsNetworkInterceptor::AuthChallengeResponse& response) {
  switch (response.response_type) {
    case DevToolsNetworkInterceptor::AuthChallengeResponse::kDefault:
      std::move(pending_auth_callback_).Run(true, base::nullopt);
      break;
    case DevToolsNetworkInterceptor::AuthChallengeResponse::kCancelAuth:
      std::move(pending_auth_callback_).Run(false, base::nullopt);
      break;
    case DevToolsNetworkInterceptor::AuthChallengeResponse::kProvideCredentials:
      std::move(pending_auth_callback_).Run(false, response.credentials);
      break;
  }
}

Response InterceptionJob::ProcessResponseOverride(
    scoped_refptr<net::HttpResponseHeaders> headers,
    std::unique_ptr<std::string> maybe_body) {
  CancelRequest();

  std::string body = maybe_body ? std::move(*maybe_body) : "";
  size_t body_size = body.size();

  response_metadata_ = std::make_unique<ResponseMetadata>();
  network::ResourceResponseHead* head = &response_metadata_->head;

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
  if (head->mime_type.empty()) {
    size_t bytes_to_sniff =
        std::min(body_size, static_cast<size_t>(net::kMaxBytesToSniff));
    net::SniffMimeType(
        body.data(), bytes_to_sniff, create_loader_params_->request.url, "",
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
                       std::move(body));
  }
  ProcessSetCookies(*head->headers, std::move(continue_after_cookies_set));

  return Response::OK();
}

void InterceptionJob::ProcessSetCookies(const net::HttpResponseHeaders& headers,
                                        base::OnceClosure callback) {
  if (create_loader_params_->request.load_flags &
      net::LOAD_DO_NOT_SAVE_COOKIES) {
    std::move(callback).Run();
    return;
  }

  const base::StringPiece name("Set-Cookie");
  std::string cookie_line;
  size_t iter = 0;
  net::CookieOptions options;
  options.set_include_httponly();
  std::vector<std::unique_ptr<net::CanonicalCookie>> cookies;
  base::Time response_date;
  if (headers.GetDateValue(&response_date))
    options.set_server_time(response_date);
  base::Time now = base::Time::Now();
  while (headers.EnumerateHeader(&iter, name, &cookie_line)) {
    std::unique_ptr<net::CanonicalCookie> cookie = net::CanonicalCookie::Create(
        create_loader_params_->request.url, cookie_line, now, options);
    if (cookie)
      cookies.emplace_back(std::move(cookie));
  }
  auto on_cookie_set = base::BindRepeating(
      [](base::RepeatingClosure closure, bool) { closure.Run(); },
      base::BarrierClosure(cookies.size(), std::move(callback)));
  for (auto& cookie : cookies) {
    cookie_manager_->SetCanonicalCookie(*cookie, true, true, on_cookie_set);
  }
}

void InterceptionJob::ProcessRedirectByClient(const GURL& redirect_url) {
  DCHECK(redirect_url.is_valid());

  const net::HttpResponseHeaders& headers = *response_metadata_->head.headers;
  const network::ResourceRequest& request = create_loader_params_->request;

  auto first_party_url_policy =
      request.update_first_party_url_on_redirect
          ? net::URLRequest::FirstPartyURLPolicy::
                UPDATE_FIRST_PARTY_URL_ON_REDIRECT
          : net::URLRequest::FirstPartyURLPolicy::NEVER_CHANGE_FIRST_PARTY_URL;

  response_metadata_->redirect_info = std::make_unique<net::RedirectInfo>(
      net::RedirectInfo::ComputeRedirectInfo(
          request.method, request.url, request.site_for_cookies,
          first_party_url_policy, request.referrer_policy,
          request.referrer.spec(), &headers, headers.response_code(),
          redirect_url, false /* insecure_scheme_was_upgraded */,
          true /* copy_fragment */));

  client_->OnReceiveRedirect(*response_metadata_->redirect_info,
                             response_metadata_->head);
}

void InterceptionJob::SendResponse(const base::StringPiece& body) {
  client_->OnReceiveResponse(response_metadata_->head);

  // We shouldn't be able to transfer a string that big over the protocol,
  // but just in case...
  DCHECK_LE(body.size(), UINT32_MAX)
      << "Response bodies larger than " << UINT32_MAX << " are not supported";
  mojo::DataPipe pipe(body.size());
  uint32_t num_bytes = body.size();
  MojoResult res = pipe.producer_handle->WriteData(body.data(), &num_bytes,
                                                   MOJO_WRITE_DATA_FLAG_NONE);
  DCHECK_EQ(0u, res);
  DCHECK_EQ(num_bytes, body.size());

  if (!response_metadata_->cached_metadata.empty())
    client_->OnReceiveCachedMetadata(response_metadata_->cached_metadata);
  client_->OnStartLoadingResponseBody(std::move(pipe.consumer_handle));
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
  SendResponse(body_reader_->body());
}

void InterceptionJob::StartRequest() {
  DCHECK_EQ(State::kNotStarted, state_);
  DCHECK(!response_metadata_);

  state_ = State::kRequestSent;

  network::mojom::URLLoaderClientPtr loader_client;
  client_binding_.Bind(MakeRequest(&loader_client));
  client_binding_.set_connection_error_handler(
      base::BindOnce(&InterceptionJob::Shutdown, base::Unretained(this)));

  target_factory_->CreateLoaderAndStart(
      MakeRequest(&loader_), create_loader_params_->routing_id,
      create_loader_params_->request_id, create_loader_params_->options,
      create_loader_params_->request, std::move(loader_client),
      create_loader_params_->traffic_annotation);

  if (priority_)
    loader_->SetPriority(priority_->first, priority_->second);
}

void InterceptionJob::CancelRequest() {
  if (state_ == State::kNotStarted)
    return;
  client_binding_.Close();
  loader_.reset();
  if (body_reader_) {
    body_reader_->CancelWithError(
        "Another command has cancelled the fetch request");
    body_reader_.reset();
  }
  state_ = State::kNotStarted;
}

std::unique_ptr<InterceptedRequestInfo> InterceptionJob::BuildRequestInfo(
    const network::ResourceResponseHead* head) {
  auto result = std::make_unique<InterceptedRequestInfo>();
  result->interception_id = current_id_;
  result->frame_id = frame_token_;
  ResourceType resource_type =
      static_cast<ResourceType>(create_loader_params_->request.resource_type);
  result->resource_type = resource_type;
  result->is_navigation = resource_type == RESOURCE_TYPE_MAIN_FRAME ||
                          resource_type == RESOURCE_TYPE_SUB_FRAME;

  if (head && head->headers)
    result->response_headers = head->headers;
  return result;
}

void InterceptionJob::FetchCookies(
    base::OnceCallback<void(const std::vector<net::CanonicalCookie>&)>
        callback) {
  if (create_loader_params_->request.load_flags &
      net::LOAD_DO_NOT_SEND_COOKIES) {
    std::move(callback).Run({});
    return;
  }
  net::CookieOptions options;
  options.set_include_httponly();
  options.set_do_not_update_access_time();

  const network::ResourceRequest& request = create_loader_params_->request;

  // The below is a copy of the logic in URLRequestHttpJob

  // Set SameSiteCookieMode according to the rules laid out in
  // https://tools.ietf.org/html/draft-ietf-httpbis-cookie-same-site:
  //
  // * Include both "strict" and "lax" same-site cookies if the request's
  //   |url|, |initiator|, and |site_for_cookies| all have the same
  //   registrable domain. Note: this also covers the case of a request
  //   without an initiator (only happens for browser-initiated main frame
  //   navigations).
  //
  // * Include only "lax" same-site cookies if the request's |URL| and
  //   |site_for_cookies| have the same registrable domain, _and_ the
  //   request's |method| is "safe" ("GET" or "HEAD").
  //
  //   Note that this will generally be the case only for cross-site requests
  //   which target a top-level browsing context.
  //
  // * Include both "strict" and "lax" same-site cookies if the request is
  //   tagged with a flag allowing it.
  //   Note that this can be the case for requests initiated by extensions,
  //   which need to behave as though they are made by the document itself,
  //   but appear like cross-site ones.
  //
  // * Otherwise, do not include same-site cookies.
  using namespace net::registry_controlled_domains;
  if (SameDomainOrHost(request.url, request.site_for_cookies,
                       INCLUDE_PRIVATE_REGISTRIES)) {
    if (!request.request_initiator ||
        SameDomainOrHost(request.url,
                         request.request_initiator.value().GetURL(),
                         INCLUDE_PRIVATE_REGISTRIES) ||
        request.attach_same_site_cookies) {
      options.set_same_site_cookie_mode(
          net::CookieOptions::SameSiteCookieMode::INCLUDE_STRICT_AND_LAX);
    } else if (net::HttpUtil::IsMethodSafe(request.method)) {
      options.set_same_site_cookie_mode(
          net::CookieOptions::SameSiteCookieMode::INCLUDE_LAX);
    }
  }
  cookie_manager_->GetCookieList(request.url, options, std::move(callback));
}

void InterceptionJob::NotifyClient(
    std::unique_ptr<InterceptedRequestInfo> request_info) {
  FetchCookies(base::BindOnce(&InterceptionJob::NotifyClientWithCookies,
                              base::Unretained(this), std::move(request_info)));
}

void InterceptionJob::NotifyClientWithCookies(
    std::unique_ptr<InterceptedRequestInfo> request_info,
    const std::vector<net::CanonicalCookie>& cookie_list) {
  if (!interceptor_)
    return;
  std::string cookie_line;
  if (!cookie_list.empty())
    cookie_line = net::CanonicalCookie::BuildCookieLine(cookie_list);
  request_info->network_request =
      protocol::NetworkHandler::CreateRequestFromResourceRequest(
          create_loader_params_->request, cookie_line);

  waiting_for_resolution_ = true;
  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::UI},
      base::BindOnce(interceptor_->request_intercepted_callback_,
                     std::move(request_info)));
}

void InterceptionJob::Shutdown() {
  if (interceptor_)
    interceptor_->RemoveJob(current_id_);
  delete this;
}

// URLLoader methods
void InterceptionJob::FollowRedirect(
    const base::Optional<std::vector<std::string>>&
        to_be_removed_request_headers,
    const base::Optional<net::HttpRequestHeaders>& modified_request_headers) {
  DCHECK(!modified_request_headers.has_value()) << "Redirect with modified "
                                                   "headers was not supported "
                                                   "yet. crbug.com/845683";
  DCHECK(!waiting_for_resolution_);

  network::ResourceRequest* request = &create_loader_params_->request;
  const net::RedirectInfo& info = *response_metadata_->redirect_info;

  bool clear_body = false;
  net::RedirectUtil::UpdateHttpRequest(
      request->url, request->method, info,
      base::nullopt /* modified_request_headers */, &request->headers,
      &clear_body);
  if (clear_body)
    request->request_body = nullptr;
  request->method = info.new_method;
  request->url = info.new_url;
  request->site_for_cookies = info.new_site_for_cookies;
  request->referrer_policy = info.new_referrer_policy;
  request->referrer = GURL(info.new_referrer);
  response_metadata_.reset();

  if (interceptor_) {
    // Pretend that each redirect hop is a new request -- this is for
    // compatibilty with URLRequestJob-based interception implementation.
    interceptor_->RemoveJob(current_id_);
    redirect_count_++;
    UpdateIdAndRegister();

    stage_ = interceptor_->GetInterceptionStage(
        request->url, static_cast<ResourceType>(request->resource_type));
    if (stage_ & InterceptionStage::REQUEST) {
      if (state_ == State::kRedirectReceived)
        state_ = State::kFollowRedirect;
      else
        DCHECK_EQ(State::kNotStarted, state_);
      NotifyClient(BuildRequestInfo(nullptr));
      return;
    }
  }
  if (state_ == State::kRedirectReceived) {
    state_ = State::kRequestSent;
    loader_->FollowRedirect(base::nullopt, base::nullopt);
    return;
  }

  DCHECK_EQ(State::kNotStarted, state_);
  StartRequest();
}

void InterceptionJob::ProceedWithResponse() {
  NOTREACHED();
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
    const network::ResourceResponseHead& head) {
  state_ = State::kResponseReceived;
  DCHECK(!response_metadata_);
  if (!(stage_ & InterceptionStage::RESPONSE)) {
    client_->OnReceiveResponse(head);
    return;
  }
  loader_->PauseReadingBodyFromNet();
  client_binding_.PauseIncomingMethodCallProcessing();

  response_metadata_ = std::make_unique<ResponseMetadata>(head);

  auto request_info = BuildRequestInfo(&head);
  const network::ResourceRequest& request = create_loader_params_->request;
  request_info->is_download =
      request_info->is_navigation && request.allow_download &&
      (is_download_ || download_utils::IsDownload(
                           request.url, head.headers.get(), head.mime_type));
  NotifyClient(std::move(request_info));
}

void InterceptionJob::OnReceiveRedirect(
    const net::RedirectInfo& redirect_info,
    const network::ResourceResponseHead& head) {
  DCHECK_EQ(State::kRequestSent, state_);
  state_ = State::kRedirectReceived;
  response_metadata_ = std::make_unique<ResponseMetadata>(head);
  response_metadata_->redirect_info =
      std::make_unique<net::RedirectInfo>(redirect_info);

  if (!(stage_ & InterceptionStage::RESPONSE)) {
    client_->OnReceiveRedirect(redirect_info, head);
    return;
  }

  std::unique_ptr<InterceptedRequestInfo> request_info =
      BuildRequestInfo(&head);
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

void InterceptionJob::OnReceiveCachedMetadata(
    const std::vector<uint8_t>& data) {
  if (ShouldBypassForResponse())
    client_->OnReceiveCachedMetadata(data);
  else
    response_metadata_->cached_metadata = data;
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
    base::PostTaskWithTraits(
        FROM_HERE, {BrowserThread::UI},
        base::BindOnce(std::move(pending_response_body_pipe_callback_),
                       Response::OK(), std::move(body),
                       response_metadata_->head.mime_type));
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
  // No need to listen to the channel any more, so just close it, so if the pipe
  // is closed by the other end, |shutdown| isn't run.
  client_binding_.Close();
  loader_.reset();
}

void InterceptionJob::OnAuthRequest(
    const scoped_refptr<net::AuthChallengeInfo>& auth_info,
    DevToolsURLLoaderInterceptor::HandleAuthRequestCallback callback) {
  DCHECK_EQ(kRequestSent, state_);
  DCHECK(pending_auth_callback_.is_null());
  DCHECK(!waiting_for_resolution_);

  if (!(stage_ & InterceptionStage::REQUEST)) {
    std::move(callback).Run(true, base::nullopt);
    return;
  }
  state_ = State::kAuthRequired;
  auto request_info = BuildRequestInfo(nullptr);
  request_info->auth_challenge = auth_info;
  pending_auth_callback_ = std::move(callback);
  NotifyClient(std::move(request_info));
}

}  // namespace content
