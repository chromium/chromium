// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/throttling_url_loader.h"

#include "base/debug/alias.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_task_runner_handle.h"
#include "net/http/http_status_code.h"
#include "net/http/http_util.h"

namespace content {

class ThrottlingURLLoader::ForwardingThrottleDelegate
    : public URLLoaderThrottle::Delegate {
 public:
  ForwardingThrottleDelegate(ThrottlingURLLoader* loader,
                             URLLoaderThrottle* throttle)
      : loader_(loader), throttle_(throttle) {}
  ~ForwardingThrottleDelegate() override = default;

  // URLLoaderThrottle::Delegate:
  void CancelWithError(int error_code,
                       base::StringPiece custom_reason) override {
    if (!loader_)
      return;

    ScopedDelegateCall scoped_delegate_call(this);
    loader_->CancelWithError(error_code, custom_reason);
  }

  void Resume() override {
    if (!loader_)
      return;

    ScopedDelegateCall scoped_delegate_call(this);
    loader_->StopDeferringForThrottle(throttle_);
  }

  void SetPriority(net::RequestPriority priority) override {
    if (!loader_)
      return;

    ScopedDelegateCall scoped_delegate_call(this);
    loader_->SetPriority(priority);
  }

  void UpdateDeferredResponseHead(
      const network::ResourceResponseHead& new_response_head) override {
    if (!loader_)
      return;
    ScopedDelegateCall scoped_delegate_call(this);
    loader_->UpdateDeferredResponseHead(new_response_head);
  }

  void PauseReadingBodyFromNet() override {
    if (!loader_)
      return;

    ScopedDelegateCall scoped_delegate_call(this);
    loader_->PauseReadingBodyFromNet(throttle_);
  }

  void ResumeReadingBodyFromNet() override {
    if (!loader_)
      return;

    ScopedDelegateCall scoped_delegate_call(this);
    loader_->ResumeReadingBodyFromNet(throttle_);
  }

  void InterceptResponse(
      network::mojom::URLLoaderPtr new_loader,
      network::mojom::URLLoaderClientRequest new_client_request,
      network::mojom::URLLoaderPtr* original_loader,
      network::mojom::URLLoaderClientRequest* original_client_request)
      override {
    if (!loader_)
      return;

    ScopedDelegateCall scoped_delegate_call(this);
    loader_->InterceptResponse(std::move(new_loader),
                               std::move(new_client_request), original_loader,
                               original_client_request);
  }
  void Detach() { loader_ = nullptr; }

 private:
  // This class helps ThrottlingURLLoader to keep track of whether it is being
  // called by its throttles.
  // If ThrottlingURLLoader is destoyed while any of the throttles is calling
  // into it, it delays destruction of the throttles. That way throttles don't
  // need to worry about any delegate calls may destory them synchronously.
  class ScopedDelegateCall {
   public:
    explicit ScopedDelegateCall(ForwardingThrottleDelegate* owner)
        : owner_(owner) {
      DCHECK(owner_->loader_);

      owner_->loader_->inside_delegate_calls_++;
    }

    ~ScopedDelegateCall() {
      // The loader may have been detached and destroyed.
      if (owner_->loader_)
        owner_->loader_->inside_delegate_calls_--;
    }

   private:
    ForwardingThrottleDelegate* const owner_;
    DISALLOW_COPY_AND_ASSIGN(ScopedDelegateCall);
  };

  ThrottlingURLLoader* loader_;
  URLLoaderThrottle* const throttle_;

  DISALLOW_COPY_AND_ASSIGN(ForwardingThrottleDelegate);
};

ThrottlingURLLoader::StartInfo::StartInfo(
    scoped_refptr<network::SharedURLLoaderFactory> in_url_loader_factory,
    int32_t in_routing_id,
    int32_t in_request_id,
    uint32_t in_options,
    network::ResourceRequest* in_url_request,
    scoped_refptr<base::SingleThreadTaskRunner> in_task_runner)
    : url_loader_factory(std::move(in_url_loader_factory)),
      routing_id(in_routing_id),
      request_id(in_request_id),
      options(in_options),
      url_request(*in_url_request),
      task_runner(std::move(in_task_runner)) {}

ThrottlingURLLoader::StartInfo::~StartInfo() = default;

ThrottlingURLLoader::ResponseInfo::ResponseInfo(
    const network::ResourceResponseHead& in_response_head)
    : response_head(in_response_head) {}

ThrottlingURLLoader::ResponseInfo::~ResponseInfo() = default;

ThrottlingURLLoader::RedirectInfo::RedirectInfo(
    const net::RedirectInfo& in_redirect_info,
    const network::ResourceResponseHead& in_response_head)
    : redirect_info(in_redirect_info), response_head(in_response_head) {}

ThrottlingURLLoader::RedirectInfo::~RedirectInfo() = default;

ThrottlingURLLoader::PriorityInfo::PriorityInfo(
    net::RequestPriority in_priority,
    int32_t in_intra_priority_value)
    : priority(in_priority), intra_priority_value(in_intra_priority_value) {}

ThrottlingURLLoader::PriorityInfo::~PriorityInfo() = default;

// static
std::unique_ptr<ThrottlingURLLoader> ThrottlingURLLoader::CreateLoaderAndStart(
    scoped_refptr<network::SharedURLLoaderFactory> factory,
    std::vector<std::unique_ptr<URLLoaderThrottle>> throttles,
    int32_t routing_id,
    int32_t request_id,
    uint32_t options,
    network::ResourceRequest* url_request,
    network::mojom::URLLoaderClient* client,
    const net::NetworkTrafficAnnotationTag& traffic_annotation,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  std::unique_ptr<ThrottlingURLLoader> loader(new ThrottlingURLLoader(
      std::move(throttles), client, traffic_annotation));
  loader->Start(std::move(factory), routing_id, request_id, options,
                url_request, std::move(task_runner));
  return loader;
}

ThrottlingURLLoader::~ThrottlingURLLoader() {
  if (inside_delegate_calls_ > 0) {
    // A throttle is calling into this object. In this case, delay destruction
    // of the throttles, so that throttles don't need to worry about any
    // delegate calls may destory them synchronously.
    for (auto& entry : throttles_)
      entry.delegate->Detach();

    auto throttles =
        std::make_unique<std::vector<ThrottleEntry>>(std::move(throttles_));
    base::ThreadTaskRunnerHandle::Get()->DeleteSoon(FROM_HERE,
                                                    std::move(throttles));
  }
}

void ThrottlingURLLoader::FollowRedirect(
    const base::Optional<net::HttpRequestHeaders>& modified_headers) {
  debug_log_.emplace_back("FollowRedirect");
  const base::Optional<net::HttpRequestHeaders>* modified_headers_to_send =
      &modified_headers;
  if (modified_request_headers_) {
    if (modified_headers)
      modified_request_headers_->MergeFrom(*modified_headers);
    modified_headers_to_send = &modified_request_headers_;
  }

  if (!throttle_redirect_url_.is_empty()) {
    throttle_redirect_url_ = GURL();
    // This is a synthesized redirect, so no need to tell the URLLoader.
    DCHECK(!modified_headers_to_send->has_value())
        << "ThrottlingURLLoader doesn't support modifying headers for "
           "synthesized requests.";
    StartNow();
    return;
  }

  if (url_loader_) {
    url_loader_->FollowRedirect(to_be_removed_request_headers_,
                                *modified_headers_to_send);
  }

  to_be_removed_request_headers_.reset();
  modified_request_headers_.reset();
}

void ThrottlingURLLoader::FollowRedirectForcingRestart() {
  debug_log_.emplace_back("FollowRedirectForcingRestart");
  url_loader_.reset();
  client_binding_.Close();

  if (to_be_removed_request_headers_) {
    for (const std::string& key : *to_be_removed_request_headers_)
      start_info_->url_request.headers.RemoveHeader(key);
    to_be_removed_request_headers_.reset();
  }

  if (modified_request_headers_) {
    start_info_->url_request.headers.MergeFrom(*modified_request_headers_);
    modified_request_headers_.reset();
  }

  StartNow();
}

void ThrottlingURLLoader::RestartWithFactory(
    scoped_refptr<network::SharedURLLoaderFactory> factory,
    uint32_t url_loader_options) {
  debug_log_.emplace_back("RestartWithFactory");
  // TODO(crbug.com/882661): Remove these aliases and turn CHECKs to DCHECKs
  // when the linked bug is fixed.
  DeferredStage deferred_stage = deferred_stage_;
  base::debug::Alias(&deferred_stage);
  bool loader_completed = loader_completed_;
  base::debug::Alias(&loader_completed);
  CHECK_EQ(DEFERRED_NONE, deferred_stage_);
  CHECK(!loader_completed_);
  url_loader_.reset();
  client_binding_.Close();
  start_info_->url_loader_factory = std::move(factory);
  start_info_->options = url_loader_options;
  StartNow();
}

void ThrottlingURLLoader::SetPriority(net::RequestPriority priority,
                                      int32_t intra_priority_value) {
  if (!url_loader_) {
    if (!loader_completed_) {
      DCHECK_EQ(DEFERRED_START, deferred_stage_);
      priority_info_ =
          std::make_unique<PriorityInfo>(priority, intra_priority_value);
    }
    return;
  }

  url_loader_->SetPriority(priority, intra_priority_value);
}

network::mojom::URLLoaderClientEndpointsPtr ThrottlingURLLoader::Unbind() {
  return network::mojom::URLLoaderClientEndpoints::New(
      url_loader_.PassInterface(), client_binding_.Unbind());
}

ThrottlingURLLoader::ThrottlingURLLoader(
    std::vector<std::unique_ptr<URLLoaderThrottle>> throttles,
    network::mojom::URLLoaderClient* client,
    const net::NetworkTrafficAnnotationTag& traffic_annotation)
    : forwarding_client_(client),
      client_binding_(this),
      traffic_annotation_(traffic_annotation),
      weak_factory_(this) {
  debug_log_.emplace_back("ctor");
  throttles_.reserve(throttles.size());
  for (auto& throttle : throttles)
    throttles_.emplace_back(this, std::move(throttle));
}

void ThrottlingURLLoader::Start(
    scoped_refptr<network::SharedURLLoaderFactory> factory,
    int32_t routing_id,
    int32_t request_id,
    uint32_t options,
    network::ResourceRequest* url_request,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  debug_log_.emplace_back("Start");
  DCHECK_EQ(DEFERRED_NONE, deferred_stage_);
  DCHECK(!loader_completed_);

  bool deferred = false;
  DCHECK(deferring_throttles_.empty());
  if (!throttles_.empty()) {
    for (auto& entry : throttles_) {
      auto* throttle = entry.throttle.get();
      bool throttle_deferred = false;
      GURL original_url = url_request->url;
      throttle->WillStartRequest(url_request, &throttle_deferred);
      if (original_url != url_request->url) {
        DCHECK(throttle_redirect_url_.is_empty())
            << "ThrottlingURLLoader doesn't support multiple throttles "
               "changing the URL.";
        CHECK_EQ(original_url.GetOrigin(), url_request->url.GetOrigin())
            << "ThrottlingURLLoader doesn't support a throttle making a "
            << "cross-origin redirect.";
        throttle_redirect_url_ = url_request->url;
        // Restore the original URL so that all throttles see the same original
        // URL.
        url_request->url = original_url;
      }
      if (!HandleThrottleResult(throttle, throttle_deferred, &deferred)) {
        debug_log_.emplace_back("Start::Return");
        return;
      }
    }

    // If a throttle had changed the URL, set it in the ResourceRequest struct
    // so that it is the URL that's requested.
    if (!throttle_redirect_url_.is_empty())
      url_request->url = throttle_redirect_url_;
  }

  start_info_ =
      std::make_unique<StartInfo>(factory, routing_id, request_id, options,
                                  url_request, std::move(task_runner));
  if (deferred) {
    debug_log_.emplace_back("Start::Deferred");
    deferred_stage_ = DEFERRED_START;
  } else {
    StartNow();
  }
}

void ThrottlingURLLoader::StartNow() {
  DCHECK(start_info_);
  debug_log_.emplace_back("StartNow");
  if (!throttle_redirect_url_.is_empty()) {
    net::RedirectInfo redirect_info;
    redirect_info.status_code = net::HTTP_TEMPORARY_REDIRECT;
    redirect_info.new_method = start_info_->url_request.method;
    redirect_info.new_url = throttle_redirect_url_;
    redirect_info.new_site_for_cookies = throttle_redirect_url_;

    network::ResourceResponseHead response_head;
    std::string header_string = base::StringPrintf(
        "HTTP/1.1 %i Internal Redirect\n"
        "Location: %s\n",
        net::HTTP_TEMPORARY_REDIRECT, throttle_redirect_url_.spec().c_str());
    response_head.headers =
        new net::HttpResponseHeaders(net::HttpUtil::AssembleRawHeaders(
            header_string.c_str(), header_string.length()));
    response_head.encoded_data_length = header_string.size();
    OnReceiveRedirect(redirect_info, response_head);
    debug_log_.emplace_back("StartNow::Redirect");
    return;
  }

  network::mojom::URLLoaderClientPtr client;
  client_binding_.Bind(mojo::MakeRequest(&client), start_info_->task_runner);
  client_binding_.set_connection_error_handler(base::BindOnce(
      &ThrottlingURLLoader::OnClientConnectionError, base::Unretained(this)));

  DCHECK(start_info_->url_loader_factory);
  start_info_->url_loader_factory->CreateLoaderAndStart(
      mojo::MakeRequest(&url_loader_), start_info_->routing_id,
      start_info_->request_id, start_info_->options, start_info_->url_request,
      std::move(client),
      net::MutableNetworkTrafficAnnotationTag(traffic_annotation_));

  if (!pausing_reading_body_from_net_throttles_.empty())
    url_loader_->PauseReadingBodyFromNet();

  if (priority_info_) {
    auto priority_info = std::move(priority_info_);
    url_loader_->SetPriority(priority_info->priority,
                             priority_info->intra_priority_value);
  }

  // Initialize with the request URL, may be updated when on redirects
  response_url_ = start_info_->url_request.url;
}

bool ThrottlingURLLoader::HandleThrottleResult(URLLoaderThrottle* throttle,
                                               bool throttle_deferred,
                                               bool* should_defer) {
  DCHECK(!deferring_throttles_.count(throttle));
  if (loader_completed_)
    return false;
  *should_defer |= throttle_deferred;
  if (throttle_deferred)
    deferring_throttles_.insert(throttle);
  return true;
}

void ThrottlingURLLoader::StopDeferringForThrottle(
    URLLoaderThrottle* throttle) {
  if (deferring_throttles_.find(throttle) == deferring_throttles_.end())
    return;

  deferring_throttles_.erase(throttle);
  if (deferring_throttles_.empty() && !loader_completed_)
    Resume();
}

void ThrottlingURLLoader::OnReceiveResponse(
    const network::ResourceResponseHead& response_head) {
  debug_log_.emplace_back("OnReceiveResponse");
  DCHECK_EQ(DEFERRED_NONE, deferred_stage_);
  DCHECK(!loader_completed_);
  DCHECK(deferring_throttles_.empty());

  network::ResourceResponseHead response_head_copy = response_head;
  if (!throttles_.empty()) {
    bool deferred = false;
    for (auto& entry : throttles_) {
      auto* throttle = entry.throttle.get();
      bool throttle_deferred = false;
      throttle->WillProcessResponse(response_url_, &response_head_copy,
                                    &throttle_deferred);
      if (!HandleThrottleResult(throttle, throttle_deferred, &deferred)) {
        debug_log_.emplace_back("OnReceiveResponse::Return");
        return;
      }
    }

    if (deferred) {
      deferred_stage_ = DEFERRED_RESPONSE;
      response_info_ = std::make_unique<ResponseInfo>(response_head_copy);
      client_binding_.PauseIncomingMethodCallProcessing();
      debug_log_.emplace_back("OnReceiveResponse::Deferred");
      return;
    }
  }

  sent_on_receive_response_ = true;
  debug_log_.emplace_back("OnReceiveResponse::Sent");
  forwarding_client_->OnReceiveResponse(response_head_copy);
}

void ThrottlingURLLoader::OnReceiveRedirect(
    const net::RedirectInfo& redirect_info,
    const network::ResourceResponseHead& response_head) {
  debug_log_.emplace_back("OnReceiveRedirect");
  DCHECK_EQ(DEFERRED_NONE, deferred_stage_);
  DCHECK(!loader_completed_);
  DCHECK(deferring_throttles_.empty());

  if (!throttles_.empty()) {
    bool deferred = false;
    for (auto& entry : throttles_) {
      auto* throttle = entry.throttle.get();
      bool throttle_deferred = false;
      auto weak_ptr = weak_factory_.GetWeakPtr();
      std::vector<std::string> to_be_removed_headers;
      net::HttpRequestHeaders modified_headers;
      throttle->WillRedirectRequest(redirect_info, response_head,
                                    &throttle_deferred, &to_be_removed_headers,
                                    &modified_headers);
      if (!weak_ptr)
        return;
      if (!HandleThrottleResult(throttle, throttle_deferred, &deferred)) {
        debug_log_.emplace_back("OnReceiveRedirect::Return");
        return;
      }

      if (!to_be_removed_headers.empty()) {
        if (to_be_removed_request_headers_) {
          for (auto& header : to_be_removed_headers) {
            if (!base::ContainsValue(*to_be_removed_request_headers_, header))
              to_be_removed_request_headers_->push_back(std::move(header));
          }
        } else {
          to_be_removed_request_headers_ = std::move(to_be_removed_headers);
        }
      }

      if (!modified_headers.IsEmpty()) {
        if (modified_request_headers_)
          modified_request_headers_->MergeFrom(modified_headers);
        else
          modified_request_headers_ = std::move(modified_headers);
      }
    }

    if (deferred) {
      deferred_stage_ = DEFERRED_REDIRECT;
      redirect_info_ =
          std::make_unique<RedirectInfo>(redirect_info, response_head);
      client_binding_.PauseIncomingMethodCallProcessing();
      debug_log_.emplace_back("OnReceiveRedirect::Deferred");
      return;
    }
  }

  // Update the request in case |FollowRedirectForcingRestart()| is called, and
  // needs to use the request updated for the redirect.
  network::ResourceRequest& request = start_info_->url_request;
  request.url = redirect_info.new_url;
  request.method = redirect_info.new_method;
  request.site_for_cookies = redirect_info.new_site_for_cookies;
  request.referrer = GURL(redirect_info.new_referrer);
  request.referrer_policy = redirect_info.new_referrer_policy;

  // TODO(dhausknecht) at this point we do not actually know if we commit to the
  // redirect or if it will be cancelled. FollowRedirect would be a more
  // suitable place to set this URL but there we do not have the data.
  response_url_ = redirect_info.new_url;
  debug_log_.emplace_back("OnReceiveRedirect::Sent");
  forwarding_client_->OnReceiveRedirect(redirect_info, response_head);
}

void ThrottlingURLLoader::OnUploadProgress(
    int64_t current_position,
    int64_t total_size,
    OnUploadProgressCallback ack_callback) {
  DCHECK_EQ(DEFERRED_NONE, deferred_stage_);
  DCHECK(!loader_completed_);

  forwarding_client_->OnUploadProgress(current_position, total_size,
                                       std::move(ack_callback));
}

void ThrottlingURLLoader::OnReceiveCachedMetadata(
    const std::vector<uint8_t>& data) {
  DCHECK_EQ(DEFERRED_NONE, deferred_stage_);
  DCHECK(!loader_completed_);

  forwarding_client_->OnReceiveCachedMetadata(data);
}

void ThrottlingURLLoader::OnTransferSizeUpdated(int32_t transfer_size_diff) {
  DCHECK_EQ(DEFERRED_NONE, deferred_stage_);
  DCHECK(!loader_completed_);

  forwarding_client_->OnTransferSizeUpdated(transfer_size_diff);
}

void ThrottlingURLLoader::OnStartLoadingResponseBody(
    mojo::ScopedDataPipeConsumerHandle body) {
  debug_log_.emplace_back("OnStartLoadingResponseBody");

  DCHECK_EQ(DEFERRED_NONE, deferred_stage_);
  DCHECK(!loader_completed_);
  DCHECK(sent_on_receive_response_);

  // TODO(crbug.com/882661): Remove when the linked bug is fixed.
  if (!sent_on_receive_response_)
    Crash();

  forwarding_client_->OnStartLoadingResponseBody(std::move(body));
}

void ThrottlingURLLoader::OnComplete(
    const network::URLLoaderCompletionStatus& status) {
  debug_log_.emplace_back("OnComplete::Sent");
  DCHECK_EQ(DEFERRED_NONE, deferred_stage_);
  DCHECK(!loader_completed_);

  // This is the last expected message. Pipe closure before this is an error
  // (see OnClientConnectionError). After this it is expected and should be
  // ignored. The owner of |this| is expected to destroy |this| when
  // OnComplete() and all data has been read. Destruction of |this| will
  // destroy |url_loader_| appropriately.
  loader_completed_ = true;
  forwarding_client_->OnComplete(status);
}

void ThrottlingURLLoader::OnClientConnectionError() {
  CancelWithError(net::ERR_ABORTED, nullptr);
}

void ThrottlingURLLoader::CancelWithError(int error_code,
                                          base::StringPiece custom_reason) {
  debug_log_.emplace_back("CancelWithError");
  if (loader_completed_)
    return;

  network::URLLoaderCompletionStatus status;
  status.error_code = error_code;
  status.completion_time = base::TimeTicks::Now();

  deferred_stage_ = DEFERRED_NONE;
  DisconnectClient(custom_reason);
  forwarding_client_->OnComplete(status);
}

void ThrottlingURLLoader::Resume() {
  debug_log_.emplace_back("Resume::" + base::NumberToString(deferred_stage_));
  if (loader_completed_ || deferred_stage_ == DEFERRED_NONE) {
    debug_log_.emplace_back("Resume::Return");
    return;
  }

  auto prev_deferred_stage = deferred_stage_;
  deferred_stage_ = DEFERRED_NONE;
  switch (prev_deferred_stage) {
    case DEFERRED_START: {
      StartNow();
      break;
    }
    case DEFERRED_REDIRECT: {
      client_binding_.ResumeIncomingMethodCallProcessing();
      // TODO(dhausknecht) at this point we do not actually know if we commit to
      // the redirect or if it will be cancelled. FollowRedirect would be a more
      // suitable place to set this URL but there we do not have the data.
      response_url_ = redirect_info_->redirect_info.new_url;
      forwarding_client_->OnReceiveRedirect(redirect_info_->redirect_info,
                                            redirect_info_->response_head);
      // Note: |this| may be deleted here.
      break;
    }
    case DEFERRED_RESPONSE: {
      client_binding_.ResumeIncomingMethodCallProcessing();
      sent_on_receive_response_ = true;
      forwarding_client_->OnReceiveResponse(response_info_->response_head);
      // Note: |this| may be deleted here.
      break;
    }
    default:
      NOTREACHED();
      break;
  }
}

void ThrottlingURLLoader::SetPriority(net::RequestPriority priority) {
  if (url_loader_)
    url_loader_->SetPriority(priority, -1);
}

void ThrottlingURLLoader::UpdateDeferredResponseHead(
    const network::ResourceResponseHead& new_response_head) {
  DCHECK(response_info_);
  DCHECK_EQ(DEFERRED_RESPONSE, deferred_stage_);
  response_info_->response_head = new_response_head;
}

void ThrottlingURLLoader::PauseReadingBodyFromNet(URLLoaderThrottle* throttle) {
  if (pausing_reading_body_from_net_throttles_.empty() && url_loader_)
    url_loader_->PauseReadingBodyFromNet();

  pausing_reading_body_from_net_throttles_.insert(throttle);
}

void ThrottlingURLLoader::ResumeReadingBodyFromNet(
    URLLoaderThrottle* throttle) {
  auto iter = pausing_reading_body_from_net_throttles_.find(throttle);
  if (iter == pausing_reading_body_from_net_throttles_.end())
    return;

  pausing_reading_body_from_net_throttles_.erase(iter);
  if (pausing_reading_body_from_net_throttles_.empty() && url_loader_)
    url_loader_->ResumeReadingBodyFromNet();
}

void ThrottlingURLLoader::InterceptResponse(
    network::mojom::URLLoaderPtr new_loader,
    network::mojom::URLLoaderClientRequest new_client_request,
    network::mojom::URLLoaderPtr* original_loader,
    network::mojom::URLLoaderClientRequest* original_client_request) {
  response_intercepted_ = true;

  if (original_loader)
    *original_loader = std::move(url_loader_);
  url_loader_ = std::move(new_loader);

  if (original_client_request)
    *original_client_request = client_binding_.Unbind();
  client_binding_.Bind(std::move(new_client_request));
  client_binding_.set_connection_error_handler(base::BindOnce(
      &ThrottlingURLLoader::OnClientConnectionError, base::Unretained(this)));
}

void ThrottlingURLLoader::DisconnectClient(base::StringPiece custom_reason) {
  client_binding_.Close();

  if (!custom_reason.empty()) {
    url_loader_.ResetWithReason(
        network::mojom::URLLoader::kClientDisconnectReason,
        custom_reason.as_string());
  } else {
    url_loader_ = nullptr;
  }

  loader_completed_ = true;
}

void ThrottlingURLLoader::Crash() {
  std::string log = base::JoinString(debug_log_, " ");
  DEBUG_ALIAS_FOR_CSTR(log_buf, log.c_str(), 2048);
  CHECK(false);
}

ThrottlingURLLoader::ThrottleEntry::ThrottleEntry(
    ThrottlingURLLoader* loader,
    std::unique_ptr<URLLoaderThrottle> the_throttle)
    : delegate(
          std::make_unique<ForwardingThrottleDelegate>(loader,
                                                       the_throttle.get())),
      throttle(std::move(the_throttle)) {
  throttle->set_delegate(delegate.get());
}

ThrottlingURLLoader::ThrottleEntry::ThrottleEntry(ThrottleEntry&& other) =
    default;

ThrottlingURLLoader::ThrottleEntry::~ThrottleEntry() = default;

ThrottlingURLLoader::ThrottleEntry& ThrottlingURLLoader::ThrottleEntry::
operator=(ThrottleEntry&& other) = default;

}  // namespace content
