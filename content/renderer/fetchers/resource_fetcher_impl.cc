// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/fetchers/resource_fetcher_impl.h"

#include <stdint.h>

#include "base/logging.h"
#include "base/macros.h"
#include "base/single_thread_task_runner.h"
#include "content/public/common/referrer.h"
#include "content/public/renderer/render_frame.h"
#include "content/renderer/loader/resource_dispatcher.h"
#include "content/renderer/loader/web_url_request_util.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "net/base/net_errors.h"
#include "net/http/http_request_headers.h"
#include "net/url_request/url_request_context.h"
#include "services/network/public/cpp/resource_request_body.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "third_party/blink/public/platform/web_security_origin.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/platform/web_url_response.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_local_frame.h"

namespace {

constexpr int32_t kRoutingId = 0;
const char kAccessControlAllowOriginHeader[] = "Access-Control-Allow-Origin";

}  // namespace

namespace content {

// static
std::unique_ptr<ResourceFetcher> ResourceFetcher::Create(const GURL& url) {
  // Can not use std::make_unique<> because the constructor is private.
  return std::unique_ptr<ResourceFetcher>(new ResourceFetcherImpl(url));
}

// TODO(toyoshim): Internal implementation might be replaced with
// SimpleURLLoader, and content::ResourceFetcher could be a thin-wrapper
// class to use SimpleURLLoader with blink-friendly types.
class ResourceFetcherImpl::ClientImpl : public network::mojom::URLLoaderClient {
 public:
  ClientImpl(ResourceFetcherImpl* parent,
             Callback callback,
             size_t maximum_download_size,
             scoped_refptr<base::SingleThreadTaskRunner> task_runner)
      : parent_(parent),
        client_binding_(this),
        data_pipe_watcher_(FROM_HERE,
                           mojo::SimpleWatcher::ArmingPolicy::MANUAL,
                           std::move(task_runner)),
        status_(Status::kNotStarted),
        completed_(false),
        maximum_download_size_(maximum_download_size),
        callback_(std::move(callback)) {}

  ~ClientImpl() override {
    callback_ = Callback();
    Cancel();
  }

  void Start(const network::ResourceRequest& request,
             scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
             const net::NetworkTrafficAnnotationTag& annotation_tag,
             scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
    status_ = Status::kStarted;
    response_.SetURL(request.url);

    network::mojom::URLLoaderClientPtr client;
    client_binding_.Bind(mojo::MakeRequest(&client), std::move(task_runner));

    url_loader_factory->CreateLoaderAndStart(
        mojo::MakeRequest(&loader_), kRoutingId,
        ResourceDispatcher::MakeRequestID(), network::mojom::kURLLoadOptionNone,
        request, std::move(client),
        net::MutableNetworkTrafficAnnotationTag(annotation_tag));
  }

  void Cancel() {
    ClearReceivedDataToFail();
    completed_ = true;
    Close();
  }

  bool IsActive() const {
    return status_ == Status::kStarted || status_ == Status::kFetching ||
           status_ == Status::kClosed;
  }

 private:
  enum class Status {
    kNotStarted,  // Initial state.
    kStarted,     // Start() is called, but data pipe is not ready yet.
    kFetching,    // Fetching via data pipe.
    kClosed,      // Data pipe is already closed, but may not be completed yet.
    kCompleted,   // Final state.
  };

  void MayComplete() {
    DCHECK(IsActive()) << "status: " << static_cast<int>(status_);
    DCHECK_NE(Status::kCompleted, status_);

    if (status_ == Status::kFetching || !completed_)
      return;

    status_ = Status::kCompleted;
    loader_.reset();

    parent_->OnLoadComplete();

    if (callback_.is_null())
      return;

    std::move(callback_).Run(response_, data_);
  }

  void ClearReceivedDataToFail() {
    response_ = blink::WebURLResponse();
    data_.clear();
  }

  void ReadDataPipe() {
    DCHECK_EQ(Status::kFetching, status_);

    for (;;) {
      const void* data;
      uint32_t size;
      MojoResult result =
          data_pipe_->BeginReadData(&data, &size, MOJO_READ_DATA_FLAG_NONE);
      if (result == MOJO_RESULT_SHOULD_WAIT) {
        data_pipe_watcher_.ArmOrNotify();
        return;
      }

      if (result == MOJO_RESULT_FAILED_PRECONDITION) {
        // Complete to read the data pipe successfully.
        Close();
        return;
      }
      DCHECK_EQ(MOJO_RESULT_OK, result);  // Only program errors can fire.

      if (data_.size() + size > maximum_download_size_) {
        data_pipe_->EndReadData(size);
        Cancel();
        return;
      }

      data_.append(static_cast<const char*>(data), size);

      result = data_pipe_->EndReadData(size);
      DCHECK_EQ(MOJO_RESULT_OK, result);  // Only program errors can fire.
    }
  }

  void Close() {
    if (status_ == Status::kFetching) {
      data_pipe_watcher_.Cancel();
      data_pipe_.reset();
    }
    status_ = Status::kClosed;
    MayComplete();
  }

  void OnDataPipeSignaled(MojoResult result,
                          const mojo::HandleSignalsState& state) {
    ReadDataPipe();
  }

  // network::mojom::URLLoaderClient overrides:
  void OnReceiveResponse(
      const network::ResourceResponseHead& response_head) override {
    DCHECK_EQ(Status::kStarted, status_);
    // Existing callers need URL and HTTP status code. URL is already set in
    // Start().
    if (response_head.headers)
      response_.SetHTTPStatusCode(response_head.headers->response_code());
  }
  void OnReceiveRedirect(
      const net::RedirectInfo& redirect_info,
      const network::ResourceResponseHead& response_head) override {
    DCHECK_EQ(Status::kStarted, status_);
    loader_->FollowRedirect(base::nullopt, base::nullopt);
    response_.SetURL(redirect_info.new_url);
  }
  void OnUploadProgress(int64_t current_position,
                        int64_t total_size,
                        OnUploadProgressCallback ack_callback) override {}
  void OnReceiveCachedMetadata(const std::vector<uint8_t>& data) override {}
  void OnTransferSizeUpdated(int32_t transfer_size_diff) override {}
  void OnStartLoadingResponseBody(
      mojo::ScopedDataPipeConsumerHandle body) override {
    DCHECK_EQ(Status::kStarted, status_);
    status_ = Status::kFetching;

    data_pipe_ = std::move(body);
    data_pipe_watcher_.Watch(
        data_pipe_.get(),
        MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_PEER_CLOSED,
        MOJO_WATCH_CONDITION_SATISFIED,
        base::BindRepeating(
            &ResourceFetcherImpl::ClientImpl::OnDataPipeSignaled,
            base::Unretained(this)));
    ReadDataPipe();
  }
  void OnComplete(const network::URLLoaderCompletionStatus& status) override {
    // When Cancel() sets |complete_|, OnComplete() may be called.
    if (completed_)
      return;

    DCHECK(IsActive()) << "status: " << static_cast<int>(status_);
    if (status.error_code != net::OK) {
      ClearReceivedDataToFail();
      Close();
    }
    completed_ = true;
    MayComplete();
  }

 private:
  ResourceFetcherImpl* parent_;
  network::mojom::URLLoaderPtr loader_;
  mojo::Binding<network::mojom::URLLoaderClient> client_binding_;
  mojo::ScopedDataPipeConsumerHandle data_pipe_;
  mojo::SimpleWatcher data_pipe_watcher_;

  Status status_;

  // A flag to represent if OnComplete() is already called. |data_pipe_| can be
  // ready even after OnComplete() is called.
  bool completed_;

  // Maximum download size to be stored in |data_|.
  const size_t maximum_download_size_;

  // Received data to be passed to the |callback_|.
  std::string data_;

  // Response to be passed to the |callback_|.
  blink::WebURLResponse response_;

  // Callback when we're done.
  Callback callback_;

  DISALLOW_COPY_AND_ASSIGN(ClientImpl);
};

ResourceFetcherImpl::ResourceFetcherImpl(const GURL& url) {
  DCHECK(url.is_valid());
  request_.url = url;
}

ResourceFetcherImpl::~ResourceFetcherImpl() {
  client_.reset();
}

void ResourceFetcherImpl::SetMethod(const std::string& method) {
  DCHECK(!client_);
  request_.method = method;
}

void ResourceFetcherImpl::SetBody(const std::string& body) {
  DCHECK(!client_);
  request_.request_body =
      network::ResourceRequestBody::CreateFromBytes(body.data(), body.size());
}

void ResourceFetcherImpl::SetHeader(const std::string& header,
                                    const std::string& value) {
  DCHECK(!client_);
  if (base::LowerCaseEqualsASCII(header, net::HttpRequestHeaders::kReferer)) {
    request_.referrer = GURL(value);
    DCHECK(request_.referrer.is_valid());
    request_.referrer_policy = Referrer::GetDefaultReferrerPolicy();
  } else {
    request_.headers.SetHeader(header, value);
  }
}

void ResourceFetcherImpl::Start(
    blink::WebLocalFrame* frame,
    blink::mojom::RequestContextType request_context,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const net::NetworkTrafficAnnotationTag& annotation_tag,
    Callback callback,
    size_t maximum_download_size) {
  DCHECK(!client_);
  DCHECK(frame);
  DCHECK(url_loader_factory);
  DCHECK(!frame->GetDocument().IsNull());
  if (request_.method.empty())
    request_.method = net::HttpRequestHeaders::kGetMethod;
  if (request_.request_body) {
    DCHECK(!base::LowerCaseEqualsASCII(request_.method,
                                       net::HttpRequestHeaders::kGetMethod))
        << "GETs can't have bodies.";
  }

  request_.fetch_request_context_type = static_cast<int>(request_context);
  request_.site_for_cookies = frame->GetDocument().SiteForCookies();
  if (!frame->GetDocument().GetSecurityOrigin().IsNull()) {
    request_.request_initiator =
        static_cast<url::Origin>(frame->GetDocument().GetSecurityOrigin());
    SetHeader(kAccessControlAllowOriginHeader,
              blink::WebSecurityOrigin::CreateUnique().ToString().Ascii());
  }
  request_.resource_type = RequestContextToResourceType(request_context);

  client_ = std::make_unique<ClientImpl>(
      this, std::move(callback), maximum_download_size,
      frame->GetTaskRunner(blink::TaskType::kNetworking));
  // TODO(kinuko, toyoshim): This task runner should be given by the consumer
  // of this class.
  client_->Start(request_, std::move(url_loader_factory), annotation_tag,
                 frame->GetTaskRunner(blink::TaskType::kNetworking));

  // No need to hold on to the request; reset it now.
  request_ = network::ResourceRequest();
}

void ResourceFetcherImpl::SetTimeout(const base::TimeDelta& timeout) {
  DCHECK(client_);
  DCHECK(client_->IsActive());
  DCHECK(!timeout_timer_.IsRunning());

  timeout_timer_.Start(FROM_HERE, timeout, this,
                       &ResourceFetcherImpl::OnTimeout);
}

void ResourceFetcherImpl::OnLoadComplete() {
  timeout_timer_.Stop();
}

void ResourceFetcherImpl::OnTimeout() {
  DCHECK(client_);
  DCHECK(client_->IsActive());
  client_->Cancel();
}

}  // namespace content
