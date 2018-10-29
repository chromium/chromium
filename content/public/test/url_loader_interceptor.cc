// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/url_loader_interceptor.h"

#include <string>
#include <utility>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/task/post_task.h"
#include "base/test/bind_test_util.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/browser/loader/navigation_url_loader_impl.h"
#include "content/browser/loader/resource_message_filter.h"
#include "content/browser/loader/url_loader_factory_impl.h"
#include "content/browser/service_worker/embedded_worker_instance.h"
#include "content/browser/storage_partition_impl.h"
#include "content/browser/url_loader_factory_getter.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "net/http/http_util.h"
#include "net/test/embedded_test_server/request_handler_util.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "third_party/blink/public/common/features.h"

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

class URLLoaderInterceptor::Interceptor
    : public network::mojom::URLLoaderFactory {
 public:
  using ProcessIdGetter = base::Callback<int()>;
  using OriginalFactoryGetter =
      base::Callback<network::mojom::URLLoaderFactory*()>;

  Interceptor(URLLoaderInterceptor* parent,
              const ProcessIdGetter& process_id_getter,
              const OriginalFactoryGetter& original_factory_getter)
      : parent_(parent),
        process_id_getter_(process_id_getter),
        original_factory_getter_(original_factory_getter) {
    bindings_.set_connection_error_handler(base::BindRepeating(
        &Interceptor::OnConnectionError, base::Unretained(this)));
  }

  ~Interceptor() override {}

  void BindRequest(network::mojom::URLLoaderFactoryRequest request) {
    bindings_.AddBinding(this, std::move(request));
  }

  void SetConnectionErrorHandler(base::OnceClosure handler) {
    error_handler_ = std::move(handler);
  }

 private:
  // network::mojom::URLLoaderFactory implementation:
  void CreateLoaderAndStart(network::mojom::URLLoaderRequest request,
                            int32_t routing_id,
                            int32_t request_id,
                            uint32_t options,
                            const network::ResourceRequest& url_request,
                            network::mojom::URLLoaderClientPtr client,
                            const net::MutableNetworkTrafficAnnotationTag&
                                traffic_annotation) override {
    RequestParams params;
    params.process_id = process_id_getter_.Run();
    params.request = std::move(request);
    params.routing_id = routing_id;
    params.request_id = request_id;
    params.options = options;
    params.url_request = std::move(url_request);
    params.client = std::move(client);
    params.traffic_annotation = traffic_annotation;

    if (parent_->Intercept(&params))
      return;

    original_factory_getter_.Run()->CreateLoaderAndStart(
        std::move(params.request), params.routing_id, params.request_id,
        params.options, std::move(params.url_request), std::move(params.client),
        params.traffic_annotation);
  }

  void Clone(network::mojom::URLLoaderFactoryRequest request) override {
    BindRequest(std::move(request));
  }

  void OnConnectionError() {
    if (bindings_.empty() && error_handler_)
      std::move(error_handler_).Run();
  }

  URLLoaderInterceptor* parent_;
  ProcessIdGetter process_id_getter_;
  OriginalFactoryGetter original_factory_getter_;
  mojo::BindingSet<network::mojom::URLLoaderFactory> bindings_;
  base::OnceClosure error_handler_;

  DISALLOW_COPY_AND_ASSIGN(Interceptor);
};

// This class intercepts calls to each StoragePartition's URLLoaderFactoryGetter
// so that it can intercept frame requests.
class URLLoaderInterceptor::URLLoaderFactoryGetterWrapper {
 public:
  URLLoaderFactoryGetterWrapper(
      URLLoaderFactoryGetter* url_loader_factory_getter,
      URLLoaderInterceptor* parent)
      : url_loader_factory_getter_(url_loader_factory_getter) {
    frame_interceptor_ = std::make_unique<Interceptor>(
        parent, base::BindRepeating([]() { return 0; }),
        base::BindLambdaForTesting([=]() -> network::mojom::URLLoaderFactory* {
          return url_loader_factory_getter
              ->original_network_factory_for_testing()
              ->get();
        }));
    url_loader_factory_getter_->SetNetworkFactoryForTesting(
        frame_interceptor_.get());
  }

  ~URLLoaderFactoryGetterWrapper() {
    url_loader_factory_getter_->SetNetworkFactoryForTesting(nullptr);
  }

 private:
  std::unique_ptr<Interceptor> frame_interceptor_;
  URLLoaderFactoryGetter* url_loader_factory_getter_;
};

// This class intercepts calls to
// StoragePartition::GetURLLoaderFactoryForBrowserProcess.
class URLLoaderInterceptor::BrowserProcessWrapper {
 public:
  BrowserProcessWrapper(network::mojom::URLLoaderFactoryRequest factory_request,
                        URLLoaderInterceptor* parent,
                        network::mojom::URLLoaderFactoryPtr original_factory)
      : interceptor_(
            parent,
            base::BindRepeating([]() { return 0; }),
            base::BindRepeating(&BrowserProcessWrapper::GetOriginalFactory,
                                base::Unretained(this))),
        original_factory_(std::move(original_factory)) {
    interceptor_.BindRequest(std::move(factory_request));
  }

  ~BrowserProcessWrapper() {}

 private:
  network::mojom::URLLoaderFactory* GetOriginalFactory() {
    return original_factory_.get();
  }

  Interceptor interceptor_;
  network::mojom::URLLoaderFactoryPtr original_factory_;

  DISALLOW_COPY_AND_ASSIGN(BrowserProcessWrapper);
};

// This class is sent along a RenderFrame commit message as a subresource
// loader so that it can intercept subresource requests.
class URLLoaderInterceptor::SubresourceWrapper {
 public:
  SubresourceWrapper(network::mojom::URLLoaderFactoryRequest factory_request,
                     int process_id,
                     URLLoaderInterceptor* parent,
                     network::mojom::URLLoaderFactoryPtrInfo original_factory)
      : interceptor_(
            parent,
            base::BindRepeating([](int process_id) { return process_id; },
                                process_id),
            base::BindRepeating(&SubresourceWrapper::GetOriginalFactory,
                                base::Unretained(this))),
        original_factory_(std::move(original_factory)) {
    interceptor_.BindRequest(std::move(factory_request));
    interceptor_.SetConnectionErrorHandler(
        base::BindOnce(&URLLoaderInterceptor::SubresourceWrapperBindingError,
                       base::Unretained(parent), this));
  }

  ~SubresourceWrapper() {}

 private:
  network::mojom::URLLoaderFactory* GetOriginalFactory() {
    return original_factory_.get();
  }

  Interceptor interceptor_;
  network::mojom::URLLoaderFactoryPtr original_factory_;

  DISALLOW_COPY_AND_ASSIGN(SubresourceWrapper);
};

URLLoaderInterceptor::RequestParams::RequestParams() = default;
URLLoaderInterceptor::RequestParams::~RequestParams() = default;
URLLoaderInterceptor::RequestParams::RequestParams(RequestParams&& other) =
    default;
URLLoaderInterceptor::RequestParams& URLLoaderInterceptor::RequestParams::
operator=(RequestParams&& other) = default;

URLLoaderInterceptor::URLLoaderInterceptor(const InterceptCallback& callback)
    : callback_(callback) {
  DCHECK(!BrowserThread::IsThreadInitialized(BrowserThread::UI) ||
         BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (base::FeatureList::IsEnabled(
          blink::features::kServiceWorkerServicification) ||
      base::FeatureList::IsEnabled(network::features::kNetworkService)) {
    RenderFrameHostImpl::SetNetworkFactoryForTesting(base::BindRepeating(
        &URLLoaderInterceptor::CreateURLLoaderFactoryForSubresources,
        base::Unretained(this)));
    // Note: This URLLoaderFactory creation callback will be used not only for
    // subresource loading from service workers (i.e., fetch()), but also for
    // loading non-installed service worker scripts.
    EmbeddedWorkerInstance::SetNetworkFactoryForTesting(base::BindRepeating(
        &URLLoaderInterceptor::CreateURLLoaderFactoryForSubresources,
        base::Unretained(this)));
  }

  StoragePartitionImpl::
      SetGetURLLoaderFactoryForBrowserProcessCallbackForTesting(
          base::BindRepeating(
              &URLLoaderInterceptor::GetURLLoaderFactoryForBrowserProcess,
              base::Unretained(this)));

  if (BrowserThread::IsThreadInitialized(BrowserThread::IO)) {
    base::RunLoop run_loop;
    base::PostTaskWithTraits(
        FROM_HERE, {BrowserThread::IO},
        base::BindOnce(&URLLoaderInterceptor::InitializeOnIOThread,
                       base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
  } else {
    InitializeOnIOThread(base::OnceClosure());
  }
}

URLLoaderInterceptor::~URLLoaderInterceptor() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (base::FeatureList::IsEnabled(
          blink::features::kServiceWorkerServicification) ||
      base::FeatureList::IsEnabled(network::features::kNetworkService)) {
    RenderFrameHostImpl::SetNetworkFactoryForTesting(
        RenderFrameHostImpl::CreateNetworkFactoryCallback());
    EmbeddedWorkerInstance::SetNetworkFactoryForTesting(
        RenderFrameHostImpl::CreateNetworkFactoryCallback());
  }

  StoragePartitionImpl::
      SetGetURLLoaderFactoryForBrowserProcessCallbackForTesting(
          StoragePartitionImpl::CreateNetworkFactoryCallback());

  base::RunLoop run_loop;
  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(&URLLoaderInterceptor::ShutdownOnIOThread,
                     base::Unretained(this), run_loop.QuitClosure()));
  run_loop.Run();
}

void URLLoaderInterceptor::WriteResponse(
    const std::string& headers,
    const std::string& body,
    network::mojom::URLLoaderClient* client,
    base::Optional<net::SSLInfo> ssl_info) {
  net::HttpResponseInfo info;
  info.headers = new net::HttpResponseHeaders(
      net::HttpUtil::AssembleRawHeaders(headers.c_str(), headers.length()));
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
    network::mojom::URLLoaderFactoryRequest request,
    int process_id,
    network::mojom::URLLoaderFactoryPtrInfo original_factory) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::IO)) {
    base::PostTaskWithTraits(
        FROM_HERE, {BrowserThread::IO},
        base::BindOnce(
            &URLLoaderInterceptor::CreateURLLoaderFactoryForSubresources,
            base::Unretained(this), std::move(request), process_id,
            std::move(original_factory)));
    return;
  }

  subresource_wrappers_.emplace(std::make_unique<SubresourceWrapper>(
      std::move(request), process_id, this, std::move(original_factory)));
}

network::mojom::URLLoaderFactoryPtr
URLLoaderInterceptor::GetURLLoaderFactoryForBrowserProcess(
    network::mojom::URLLoaderFactoryPtr original_factory) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  network::mojom::URLLoaderFactoryPtr loader_factory;
  browser_process_interceptors_.emplace(std::make_unique<BrowserProcessWrapper>(
      mojo::MakeRequest(&loader_factory), this, std::move(original_factory)));
  return loader_factory;
}

void URLLoaderInterceptor::GetNetworkFactoryCallback(
    URLLoaderFactoryGetter* url_loader_factory_getter) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  url_loader_factory_getter_wrappers_.emplace(
      std::make_unique<URLLoaderFactoryGetterWrapper>(url_loader_factory_getter,
                                                      this));
}

bool URLLoaderInterceptor::BeginNavigationCallback(
    network::mojom::URLLoaderRequest* request,
    int32_t routing_id,
    int32_t request_id,
    uint32_t options,
    const network::ResourceRequest& url_request,
    network::mojom::URLLoaderClientPtr* client,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation) {
  RequestParams params;
  params.process_id = 0;
  params.request = std::move(*request);
  params.routing_id = routing_id;
  params.request_id = request_id;
  params.options = options;
  params.url_request = url_request;
  params.client = std::move(*client);
  params.traffic_annotation = traffic_annotation;

  if (Intercept(&params))
    return true;

  *request = std::move(params.request);
  *client = std::move(params.client);
  return false;
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

void URLLoaderInterceptor::SubresourceWrapperBindingError(
    SubresourceWrapper* wrapper) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  for (auto& it : subresource_wrappers_) {
    if (it.get() == wrapper) {
      subresource_wrappers_.erase(it);
      return;
    }
  }

  NOTREACHED();
}

void URLLoaderInterceptor::InitializeOnIOThread(base::OnceClosure closure) {
  if (base::FeatureList::IsEnabled(network::features::kNetworkService)) {
    URLLoaderFactoryGetter::SetGetNetworkFactoryCallbackForTesting(
        base::BindRepeating(&URLLoaderInterceptor::GetNetworkFactoryCallback,
                            base::Unretained(this)));
  } else {
    NavigationURLLoaderImpl::SetBeginNavigationInterceptorForTesting(
        base::BindRepeating(&URLLoaderInterceptor::BeginNavigationCallback,
                            base::Unretained(this)));
  }

  if (!base::FeatureList::IsEnabled(network::features::kNetworkService)) {
    rmf_interceptor_ = std::make_unique<Interceptor>(
        this, base::BindRepeating([]() {
          return ResourceMessageFilter::GetCurrentForTesting()->child_id();
        }),
        base::BindRepeating([]() {
          network::mojom::URLLoaderFactory* factory =
              ResourceMessageFilter::GetCurrentForTesting();
          return factory;
        }));
    ResourceMessageFilter::SetNetworkFactoryForTesting(rmf_interceptor_.get());
  }

  if (closure)
    std::move(closure).Run();
}

void URLLoaderInterceptor::ShutdownOnIOThread(base::OnceClosure closure) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  url_loader_factory_getter_wrappers_.clear();
  subresource_wrappers_.clear();

  if (base::FeatureList::IsEnabled(network::features::kNetworkService)) {
    URLLoaderFactoryGetter::SetGetNetworkFactoryCallbackForTesting(
        URLLoaderFactoryGetter::GetNetworkFactoryCallback());
  } else {
    NavigationURLLoaderImpl::SetBeginNavigationInterceptorForTesting(
        NavigationURLLoaderImpl::BeginNavigationInterceptor());
  }

  if (!base::FeatureList::IsEnabled(network::features::kNetworkService)) {
    ResourceMessageFilter::SetNetworkFactoryForTesting(nullptr);
  }

  std::move(closure).Run();
}

// static
std::unique_ptr<content::URLLoaderInterceptor>
URLLoaderInterceptor::SetupRequestFailForURL(const GURL& url,
                                             net::Error error) {
  return std::make_unique<content::URLLoaderInterceptor>(base::BindRepeating(
      [](const GURL& url, net::Error error,
         content::URLLoaderInterceptor::RequestParams* params) {
        if (params->url_request.url != url)
          return false;
        params->client->OnComplete(network::URLLoaderCompletionStatus(error));
        return true;
      },
      url, error));
}

}  // namespace content
