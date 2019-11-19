// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/url_loader_interceptor.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/containers/unique_ptr_adapters.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/synchronization/lock.h"
#include "base/task/post_task.h"
#include "base/test/bind_test_util.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/browser/loader/navigation_url_loader_impl.h"
#include "content/browser/service_worker/embedded_worker_instance.h"
#include "content/browser/storage_partition_impl.h"
#include "content/browser/url_loader_factory_getter.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/mock_render_process_host.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "net/http/http_util.h"
#include "net/test/embedded_test_server/request_handler_util.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/mojom/url_loader.mojom.h"

namespace content {

namespace {

base::FilePath GetDataFilePath(const std::string& relative_path) {
  base::FilePath root_path;
  CHECK(base::PathService::Get(base::DIR_SOURCE_ROOT, &root_path));
  return root_path.AppendASCII(relative_path);
}

static std::string ReadFile(const base::FilePath& path) {
  std::string contents;
  CHECK(base::ReadFileToString(path, &contents));
  return contents;
}

}  // namespace

// Part of URLLoaderInterceptor which lives on the IO thread. Outlives
// URLLoaderInterceptor.
class URLLoaderInterceptor::IOState
    : public base::RefCountedThreadSafe<URLLoaderInterceptor::IOState,
                                        BrowserThread::DeleteOnIOThread> {
 public:
  explicit IOState(URLLoaderInterceptor* parent) : parent_(parent) {}
  void Initialize(
      const URLLoaderCompletionStatusCallback& completion_status_callback,
      base::OnceClosure closure);

  // Called when a SubresourceWrapper's binding has an error.
  void SubresourceWrapperBindingError(SubresourceWrapper* wrapper);

  // Unsets the parent pointer. Prevents URLLoaderInterceptor::Intercept from
  // being called.
  void UnsetParent() {
    base::AutoLock lock(intercept_lock_);
    parent_ = nullptr;
  }

  void Shutdown(base::OnceClosure closure) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
    url_loader_factory_getter_wrappers_.clear();
    subresource_wrappers_.clear();
    navigation_wrappers_.clear();

    URLLoaderFactoryGetter::SetGetNetworkFactoryCallbackForTesting(
        URLLoaderFactoryGetter::GetNetworkFactoryCallback());
    if (closure)
      std::move(closure).Run();
  }

  // Callback on IO thread whenever a
  // URLLoaderFactoryGetter::GetNetworkContext is called on an object that
  // doesn't have a test factory set up.
  void GetNetworkFactoryCallback(
      scoped_refptr<URLLoaderFactoryGetter> url_loader_factory_getter);

  void CreateURLLoaderFactoryForSubresources(
      mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver,
      int process_id,
      mojo::PendingRemote<network::mojom::URLLoaderFactory> original_factory);

  bool Intercept(RequestParams* params) {
    // The lock ensures that |URLLoaderInterceptor| can't be deleted while it
    // is processing an intercept. Before |URLLoaderInterceptor| is deleted,
    // parent_ is set to null so that requests can't be intercepted after
    // |URLLoaderInterceptor| is deleted.
    base::AutoLock lock(intercept_lock_);
    if (!parent_)
      return false;
    return parent_->Intercept(params);
  }

  bool BeginNavigationCallback(
      mojo::PendingReceiver<network::mojom::URLLoader>* receiver,
      int32_t routing_id,
      int32_t request_id,
      uint32_t options,
      const network::ResourceRequest& url_request,
      mojo::PendingRemote<network::mojom::URLLoaderClient>* client,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation) {
    RequestParams params;
    params.process_id = 0;
    params.receiver = std::move(*receiver);
    params.routing_id = routing_id;
    params.request_id = request_id;
    params.options = options;
    params.url_request = url_request;
    params.client.Bind(std::move(*client));
    params.traffic_annotation = traffic_annotation;

    if (Intercept(&params))
      return true;

    *receiver = std::move(params.receiver);
    *client = params.client.Unbind();
    return false;
  }

  // Callback on IO thread whenever NavigationURLLoaderImpl needs a
  // URLLoaderFactory with a network::mojom::TrustedURLLoaderHeaderClient or
  // for a non-network-service scheme.
  void InterceptNavigationRequestCallback(
      mojo::PendingReceiver<network::mojom::URLLoaderFactory>* receiver) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

    auto proxied_receiver = std::move(*receiver);
    mojo::PendingRemote<network::mojom::URLLoaderFactory> target_factory;
    *receiver = target_factory.InitWithNewPipeAndPassReceiver();

    navigation_wrappers_.emplace(
        std::make_unique<URLLoaderFactoryNavigationWrapper>(
            std::move(proxied_receiver), std::move(target_factory), this));
  }

  URLLoaderCompletionStatusCallback GetCompletionStatusCallback() {
    return completion_status_callback_;
  }

 private:
  friend struct BrowserThread::DeleteOnThread<BrowserThread::IO>;
  friend class base::DeleteHelper<IOState>;

  ~IOState() {}

  // This lock guarantees that when URLLoaderInterceptor is destroyed,
  // no intercept callbacks will be called.
  base::Lock intercept_lock_;
  URLLoaderInterceptor* parent_ GUARDED_BY(intercept_lock_);

  URLLoaderCompletionStatusCallback completion_status_callback_;

  // For intercepting frame requests with network service. There is one per
  // StoragePartition. Only accessed on IO thread.
  std::set<std::unique_ptr<URLLoaderFactoryGetterWrapper>>
      url_loader_factory_getter_wrappers_;
  // For intercepting subresources with network service. There is one per
  // active render frame commit. Only accessed on IO thread.
  std::set<std::unique_ptr<SubresourceWrapper>, base::UniquePtrComparator>
      subresource_wrappers_;
  std::set<std::unique_ptr<URLLoaderFactoryNavigationWrapper>>
      navigation_wrappers_;

  DISALLOW_COPY_AND_ASSIGN(IOState);
};

class URLLoaderClientInterceptor : public network::mojom::URLLoaderClient {
 public:
  explicit URLLoaderClientInterceptor(
      const base::Callback<network::mojom::URLLoaderFactory*()>& factory_getter,
      URLLoaderInterceptor::RequestParams params,
      const URLLoaderInterceptor::URLLoaderCompletionStatusCallback&
          completion_status_callback)
      : original_client_(std::move(params.client)),
        completion_status_callback_(std::move(completion_status_callback)),
        request_url_(params.url_request.url) {
    factory_getter.Run()->CreateLoaderAndStart(
        std::move(params.receiver), params.routing_id, params.request_id,
        params.options, std::move(params.url_request),
        delegating_client_receiver_.BindNewPipeAndPassRemote(),
        params.traffic_annotation);
  }

  void OnReceiveResponse(network::mojom::URLResponseHeadPtr head) override {
    original_client_->OnReceiveResponse(std::move(head));
  }

  void OnReceiveRedirect(const net::RedirectInfo& redirect_info,
                         network::mojom::URLResponseHeadPtr head) override {
    original_client_->OnReceiveRedirect(redirect_info, std::move(head));
  }

  void OnUploadProgress(int64_t current_position,
                        int64_t total_size,
                        base::OnceCallback<void()> callback) override {
    original_client_->OnUploadProgress(current_position, total_size,
                                       std::move(callback));
  }

  void OnReceiveCachedMetadata(mojo_base::BigBuffer data) override {
    original_client_->OnReceiveCachedMetadata(std::move(data));
  }

  void OnTransferSizeUpdated(int32_t transfer_size_diff) override {
    original_client_->OnTransferSizeUpdated(transfer_size_diff);
  }

  void OnStartLoadingResponseBody(
      mojo::ScopedDataPipeConsumerHandle body) override {
    original_client_->OnStartLoadingResponseBody(std::move(body));
  }

  void OnComplete(const network::URLLoaderCompletionStatus& status) override {
    if (!completion_status_callback_.is_null())
      completion_status_callback_.Run(request_url_, status);
    original_client_->OnComplete(status);
  }

 private:
  mojo::Remote<network::mojom::URLLoaderClient> original_client_;
  mojo::Receiver<network::mojom::URLLoaderClient> delegating_client_receiver_{
      this};
  URLLoaderInterceptor::URLLoaderCompletionStatusCallback
      completion_status_callback_;
  GURL request_url_;
};

class URLLoaderInterceptor::Interceptor
    : public network::mojom::URLLoaderFactory {
 public:
  using ProcessIdGetter = base::Callback<int()>;
  using OriginalFactoryGetter =
      base::Callback<network::mojom::URLLoaderFactory*()>;

  Interceptor(URLLoaderInterceptor::IOState* parent,
              const ProcessIdGetter& process_id_getter,
              const OriginalFactoryGetter& original_factory_getter)
      : parent_(parent),
        process_id_getter_(process_id_getter),
        original_factory_getter_(original_factory_getter) {
    receivers_.set_disconnect_handler(base::BindRepeating(
        &Interceptor::OnConnectionError, base::Unretained(this)));
  }

  ~Interceptor() override {}

  void BindReceiver(
      mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver) {
    receivers_.Add(this, std::move(receiver));
  }

  void SetConnectionErrorHandler(base::OnceClosure handler) {
    error_handler_ = std::move(handler);
  }

 private:
  // network::mojom::URLLoaderFactory implementation:
  void CreateLoaderAndStart(
      mojo::PendingReceiver<network::mojom::URLLoader> receiver,
      int32_t routing_id,
      int32_t request_id,
      uint32_t options,
      const network::ResourceRequest& url_request,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation)
      override {
    RequestParams params;
    params.process_id = process_id_getter_.Run();
    params.receiver = std::move(receiver);
    params.routing_id = routing_id;
    params.request_id = request_id;
    params.options = options;
    params.url_request = std::move(url_request);
    params.client.Bind(std::move(client));
    params.traffic_annotation = traffic_annotation;

    if (parent_->Intercept(&params))
      return;

    url_loader_client_interceptors_.push_back(
        std::make_unique<URLLoaderClientInterceptor>(
            std::move(original_factory_getter_), std::move(params),
            parent_->GetCompletionStatusCallback()));
  }

  void Clone(mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver)
      override {
    BindReceiver(std::move(receiver));
  }

  void OnConnectionError() {
    if (receivers_.empty() && error_handler_)
      std::move(error_handler_).Run();
  }

  URLLoaderInterceptor::IOState* parent_;
  ProcessIdGetter process_id_getter_;
  OriginalFactoryGetter original_factory_getter_;
  mojo::ReceiverSet<network::mojom::URLLoaderFactory> receivers_;
  base::OnceClosure error_handler_;
  std::vector<std::unique_ptr<URLLoaderClientInterceptor>>
      url_loader_client_interceptors_;

  DISALLOW_COPY_AND_ASSIGN(Interceptor);
};

// This class intercepts calls to each StoragePartition's URLLoaderFactoryGetter
// so that it can intercept frame requests.
class URLLoaderInterceptor::URLLoaderFactoryGetterWrapper {
 public:
  URLLoaderFactoryGetterWrapper(
      scoped_refptr<URLLoaderFactoryGetter> url_loader_factory_getter,
      URLLoaderInterceptor::IOState* parent)
      : url_loader_factory_getter_(std::move(url_loader_factory_getter)) {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    frame_interceptor_ = std::make_unique<Interceptor>(
        parent, base::BindRepeating([]() { return 0; }),
        base::BindLambdaForTesting([=]() -> network::mojom::URLLoaderFactory* {
          return url_loader_factory_getter_
              ->original_network_factory_for_testing()
              ->get();
        }));
    url_loader_factory_getter_->SetNetworkFactoryForTesting(
        frame_interceptor_.get());
  }

  ~URLLoaderFactoryGetterWrapper() {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    url_loader_factory_getter_->SetNetworkFactoryForTesting(nullptr);
  }

 private:
  std::unique_ptr<Interceptor> frame_interceptor_;
  scoped_refptr<URLLoaderFactoryGetter> url_loader_factory_getter_;
};

class URLLoaderInterceptor::URLLoaderFactoryNavigationWrapper {
 public:
  URLLoaderFactoryNavigationWrapper(
      mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver,
      mojo::PendingRemote<network::mojom::URLLoaderFactory> target_factory,
      URLLoaderInterceptor::IOState* parent)
      : target_factory_(std::move(target_factory)) {
    interceptor_ = std::make_unique<Interceptor>(
        parent, base::BindRepeating([]() { return 0; }),
        base::BindLambdaForTesting([=]() -> network::mojom::URLLoaderFactory* {
          return this->target_factory_.get();
        }));
    interceptor_->BindReceiver(std::move(receiver));
  }

 private:
  std::unique_ptr<Interceptor> interceptor_;
  mojo::Remote<network::mojom::URLLoaderFactory> target_factory_;
};

// This class intercepts calls to
// StoragePartition::GetURLLoaderFactoryForBrowserProcess.
class URLLoaderInterceptor::BrowserProcessWrapper {
 public:
  BrowserProcessWrapper(
      mojo::PendingReceiver<network::mojom::URLLoaderFactory> factory_receiver,
      URLLoaderInterceptor::IOState* parent,
      mojo::PendingRemote<network::mojom::URLLoaderFactory> original_factory)
      : interceptor_(
            parent,
            base::BindRepeating([]() { return 0; }),
            base::BindRepeating(&BrowserProcessWrapper::GetOriginalFactory,
                                base::Unretained(this))),
        original_factory_(std::move(original_factory)) {
    interceptor_.BindReceiver(std::move(factory_receiver));
  }

  ~BrowserProcessWrapper() {}

 private:
  network::mojom::URLLoaderFactory* GetOriginalFactory() {
    return original_factory_.get();
  }

  Interceptor interceptor_;
  mojo::Remote<network::mojom::URLLoaderFactory> original_factory_;

  DISALLOW_COPY_AND_ASSIGN(BrowserProcessWrapper);
};

// This class is sent along a RenderFrame commit message as a subresource
// loader so that it can intercept subresource requests.
class URLLoaderInterceptor::SubresourceWrapper {
 public:
  SubresourceWrapper(
      mojo::PendingReceiver<network::mojom::URLLoaderFactory> factory_receiver,
      int process_id,
      URLLoaderInterceptor::IOState* parent,
      mojo::PendingRemote<network::mojom::URLLoaderFactory> original_factory)
      : interceptor_(
            parent,
            base::BindRepeating([](int process_id) { return process_id; },
                                process_id),
            base::BindRepeating(&SubresourceWrapper::GetOriginalFactory,
                                base::Unretained(this))),
        original_factory_(std::move(original_factory)) {
    interceptor_.BindReceiver(std::move(factory_receiver));
    interceptor_.SetConnectionErrorHandler(base::BindOnce(
        &URLLoaderInterceptor::IOState::SubresourceWrapperBindingError,
        base::Unretained(parent), this));
  }

  ~SubresourceWrapper() {}

 private:
  network::mojom::URLLoaderFactory* GetOriginalFactory() {
    return original_factory_.get();
  }

  Interceptor interceptor_;
  mojo::Remote<network::mojom::URLLoaderFactory> original_factory_;

  DISALLOW_COPY_AND_ASSIGN(SubresourceWrapper);
};

URLLoaderInterceptor::RequestParams::RequestParams() = default;
URLLoaderInterceptor::RequestParams::~RequestParams() = default;
URLLoaderInterceptor::RequestParams::RequestParams(RequestParams&& other) =
    default;
URLLoaderInterceptor::RequestParams& URLLoaderInterceptor::RequestParams::
operator=(RequestParams&& other) = default;

URLLoaderInterceptor::URLLoaderInterceptor(const InterceptCallback& callback)
    : URLLoaderInterceptor(callback, {}, {}) {}

URLLoaderInterceptor::URLLoaderInterceptor(
    const InterceptCallback& callback,
    const URLLoaderCompletionStatusCallback& completion_status_callback,
    base::OnceClosure ready_callback)
    : callback_(callback), io_thread_(base::MakeRefCounted<IOState>(this)) {
  DCHECK(!BrowserThread::IsThreadInitialized(BrowserThread::UI) ||
         BrowserThread::CurrentlyOn(BrowserThread::UI));
  use_runloop_ = !ready_callback;
  RenderFrameHostImpl::SetNetworkFactoryForTesting(base::BindRepeating(
      &URLLoaderInterceptor::CreateURLLoaderFactoryForSubresources,
      base::Unretained(this)));
  SharedWorkerHost::SetNetworkFactoryForSubresourcesForTesting(
      base::BindRepeating(
          &URLLoaderInterceptor::CreateURLLoaderFactoryForSubresources,
          base::Unretained(this)));
  // Note: This URLLoaderFactory creation callback will be used not only for
  // subresource loading from service workers (i.e., fetch()), but also for
  // loading non-installed service worker scripts.
  EmbeddedWorkerInstance::SetNetworkFactoryForTesting(base::BindRepeating(
      &URLLoaderInterceptor::CreateURLLoaderFactoryForSubresources,
      base::Unretained(this)));

  StoragePartitionImpl::
      SetGetURLLoaderFactoryForBrowserProcessCallbackForTesting(
          base::BindRepeating(
              &URLLoaderInterceptor::GetURLLoaderFactoryForBrowserProcess,
              base::Unretained(this)));

  NavigationURLLoaderImpl::SetURLLoaderFactoryInterceptorForTesting(
      base::BindRepeating(
          &URLLoaderInterceptor::InterceptNavigationRequestCallback,
          base::Unretained(this)));

  MockRenderProcessHost::SetNetworkFactory(base::BindRepeating(
      &URLLoaderInterceptor::CreateURLLoaderFactoryForSubresources,
      base::Unretained(this)));

  if (BrowserThread::IsThreadInitialized(BrowserThread::IO)) {
    if (use_runloop_) {
      base::RunLoop run_loop;
      base::PostTask(
          FROM_HERE, {BrowserThread::IO},
          base::BindOnce(&URLLoaderInterceptor::IOState::Initialize, io_thread_,
                         std::move(completion_status_callback),
                         run_loop.QuitClosure()));
      run_loop.Run();
    } else {
      base::OnceClosure wrapped_callback = base::BindOnce(
          [](base::OnceClosure callback) {
            base::PostTask(FROM_HERE, {BrowserThread::UI}, std::move(callback));
          },
          std::move(ready_callback));

      base::PostTask(
          FROM_HERE, {BrowserThread::IO},
          base::BindOnce(&URLLoaderInterceptor::IOState::Initialize, io_thread_,
                         std::move(completion_status_callback),
                         std::move(wrapped_callback)));
    }
  } else {
    io_thread_->Initialize(std::move(completion_status_callback),
                           std::move(ready_callback));
  }
}

URLLoaderInterceptor::~URLLoaderInterceptor() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  io_thread_->UnsetParent();

  RenderFrameHostImpl::SetNetworkFactoryForTesting(
      RenderFrameHostImpl::CreateNetworkFactoryCallback());
  SharedWorkerHost::SetNetworkFactoryForSubresourcesForTesting(
      RenderFrameHostImpl::CreateNetworkFactoryCallback());
  EmbeddedWorkerInstance::SetNetworkFactoryForTesting(
      RenderFrameHostImpl::CreateNetworkFactoryCallback());

  StoragePartitionImpl::
      SetGetURLLoaderFactoryForBrowserProcessCallbackForTesting(
          StoragePartitionImpl::CreateNetworkFactoryCallback());

  NavigationURLLoaderImpl::SetURLLoaderFactoryInterceptorForTesting(
      NavigationURLLoaderImpl::URLLoaderFactoryInterceptor());

  MockRenderProcessHost::SetNetworkFactory(
      MockRenderProcessHost::CreateNetworkFactoryCallback());

  if (use_runloop_) {
    base::RunLoop run_loop;
    base::PostTask(FROM_HERE, {BrowserThread::IO},
                   base::BindOnce(&URLLoaderInterceptor::IOState::Shutdown,
                                  io_thread_, run_loop.QuitClosure()));
    run_loop.Run();
  } else {
    base::PostTask(FROM_HERE, {BrowserThread::IO},
                   base::BindOnce(&URLLoaderInterceptor::IOState::Shutdown,
                                  io_thread_, base::OnceClosure()));
  }
}

void URLLoaderInterceptor::WriteResponse(
    const std::string& headers,
    const std::string& body,
    network::mojom::URLLoaderClient* client,
    base::Optional<net::SSLInfo> ssl_info) {
  net::HttpResponseInfo info;
  info.headers = base::MakeRefCounted<net::HttpResponseHeaders>(
      net::HttpUtil::AssembleRawHeaders(headers));
  network::ResourceResponseHead response;
  response.headers = info.headers;
  response.headers->GetMimeType(&response.mime_type);
  response.ssl_info = std::move(ssl_info);
  client->OnReceiveResponse(response);

  uint32_t bytes_written = body.size();
  mojo::DataPipe data_pipe(body.size());
  CHECK_EQ(MOJO_RESULT_OK,
           data_pipe.producer_handle->WriteData(
               body.data(), &bytes_written, MOJO_WRITE_DATA_FLAG_ALL_OR_NONE));
  client->OnStartLoadingResponseBody(std::move(data_pipe.consumer_handle));

  network::URLLoaderCompletionStatus status;
  status.decoded_body_length = body.size();
  status.error_code = net::OK;
  client->OnComplete(status);
}

void URLLoaderInterceptor::WriteResponse(
    const std::string& relative_path,
    network::mojom::URLLoaderClient* client,
    const std::string* headers,
    base::Optional<net::SSLInfo> ssl_info) {
  return WriteResponse(GetDataFilePath(relative_path), client, headers,
                       std::move(ssl_info));
}

void URLLoaderInterceptor::WriteResponse(
    const base::FilePath& file_path,
    network::mojom::URLLoaderClient* client,
    const std::string* headers,
    base::Optional<net::SSLInfo> ssl_info) {
  base::ScopedAllowBlockingForTesting allow_io;
  std::string headers_str;
  if (headers) {
    headers_str = *headers;
  } else {
    base::FilePath::StringPieceType mock_headers_extension;
#if defined(OS_WIN)
    base::string16 temp =
        base::ASCIIToUTF16(net::test_server::kMockHttpHeadersExtension);
    mock_headers_extension = temp;
#else
    mock_headers_extension = net::test_server::kMockHttpHeadersExtension;
#endif

    base::FilePath headers_path(file_path.AddExtension(mock_headers_extension));
    if (base::PathExists(headers_path)) {
      headers_str = ReadFile(headers_path);
    } else {
      headers_str = "HTTP/1.0 200 OK\nContent-type: " +
                    net::test_server::GetContentType(file_path) + "\n\n";
    }
  }
  WriteResponse(headers_str, ReadFile(file_path), client, std::move(ssl_info));
}

void URLLoaderInterceptor::CreateURLLoaderFactoryForSubresources(
    mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver,
    int process_id,
    mojo::PendingRemote<network::mojom::URLLoaderFactory> original_factory) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::IO)) {
    base::PostTask(
        FROM_HERE, {BrowserThread::IO},
        base::BindOnce(
            &URLLoaderInterceptor::CreateURLLoaderFactoryForSubresources,
            base::Unretained(this), std::move(receiver), process_id,
            std::move(original_factory)));
    return;
  }
  io_thread_->CreateURLLoaderFactoryForSubresources(
      std::move(receiver), process_id, std::move(original_factory));
}

mojo::PendingRemote<network::mojom::URLLoaderFactory>
URLLoaderInterceptor::GetURLLoaderFactoryForBrowserProcess(
    mojo::PendingRemote<network::mojom::URLLoaderFactory> original_factory) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  mojo::PendingRemote<network::mojom::URLLoaderFactory> loader_factory;
  browser_process_interceptors_.emplace(std::make_unique<BrowserProcessWrapper>(
      loader_factory.InitWithNewPipeAndPassReceiver(), io_thread_.get(),
      std::move(original_factory)));
  return loader_factory;
}

void URLLoaderInterceptor::InterceptNavigationRequestCallback(
    mojo::PendingReceiver<network::mojom::URLLoaderFactory>* receiver) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  auto proxied_receiver = std::move(*receiver);
  mojo::PendingRemote<network::mojom::URLLoaderFactory> target_factory;
  *receiver = target_factory.InitWithNewPipeAndPassReceiver();

  navigation_wrappers_.emplace(
      std::make_unique<URLLoaderFactoryNavigationWrapper>(
          std::move(proxied_receiver), std::move(target_factory),
          io_thread_.get()));
}

bool URLLoaderInterceptor::Intercept(RequestParams* params) {
  if (callback_.Run(params))
    return true;

  // mock.failed.request is a special request whereby the query indicates what
  // error code to respond with.
  if (params->url_request.url.DomainIs("mock.failed.request")) {
    std::string query = params->url_request.url.query();
    std::string error_code = query.substr(query.find("=") + 1);

    int error = 0;
    base::StringToInt(error_code, &error);
    network::URLLoaderCompletionStatus status;
    status.error_code = error;
    params->client->OnComplete(status);
    return true;
  }

  return false;
}

void URLLoaderInterceptor::IOState::SubresourceWrapperBindingError(
    SubresourceWrapper* wrapper) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  auto it = subresource_wrappers_.find(wrapper);
  DCHECK(it != subresource_wrappers_.end());
  subresource_wrappers_.erase(it);
}

void URLLoaderInterceptor::IOState::Initialize(
    const URLLoaderCompletionStatusCallback& completion_status_callback,
    base::OnceClosure closure) {
  completion_status_callback_ = std::move(completion_status_callback);
  URLLoaderFactoryGetter::SetGetNetworkFactoryCallbackForTesting(
      base::BindRepeating(
          &URLLoaderInterceptor::IOState::GetNetworkFactoryCallback,
          base::Unretained(this)));

  if (closure)
    std::move(closure).Run();
}

void URLLoaderInterceptor::IOState::GetNetworkFactoryCallback(
    scoped_refptr<URLLoaderFactoryGetter> url_loader_factory_getter) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  url_loader_factory_getter_wrappers_.emplace(
      std::make_unique<URLLoaderFactoryGetterWrapper>(url_loader_factory_getter,
                                                      this));
}

void URLLoaderInterceptor::IOState::CreateURLLoaderFactoryForSubresources(
    mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver,
    int process_id,
    mojo::PendingRemote<network::mojom::URLLoaderFactory> original_factory) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  subresource_wrappers_.emplace(std::make_unique<SubresourceWrapper>(
      std::move(receiver), process_id, this, std::move(original_factory)));
}

// static
std::unique_ptr<content::URLLoaderInterceptor>
URLLoaderInterceptor::SetupRequestFailForURL(const GURL& url,
                                             net::Error error,
                                             base::OnceClosure ready_callback) {
  return std::make_unique<content::URLLoaderInterceptor>(
      base::BindRepeating(
          [](const GURL& url, net::Error error,
             content::URLLoaderInterceptor::RequestParams* params) {
            if (params->url_request.url != url)
              return false;
            params->client->OnComplete(
                network::URLLoaderCompletionStatus(error));
            return true;
          },
          url, error),
      URLLoaderCompletionStatusCallback(), std::move(ready_callback));
}

}  // namespace content
