// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/url_loader_interceptor.h"

#include <string>
#include <string_view>
#include <utility>

#include "base/containers/unique_ptr_adapters.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/synchronization/lock.h"
#include "base/test/bind.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "content/browser/loader/url_loader_factory_utils.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/service_worker/embedded_worker_instance.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/mock_render_process_host.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "net/http/http_util.h"
#include "net/test/embedded_test_server/request_handler_util.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/parsed_headers.h"
#include "services/network/public/cpp/url_loader_factory_builder.h"
#include "services/network/public/mojom/early_hints.mojom.h"
#include "services/network/public/mojom/url_loader.mojom.h"

namespace content {

namespace {

base::FilePath GetDataFilePath(const std::string& relative_path) {
  base::FilePath root_path;
  CHECK(base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &root_path));
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

  IOState(const IOState&) = delete;
  IOState& operator=(const IOState&) = delete;

  void Initialize(
      const URLLoaderCompletionStatusCallback& completion_status_callback,
      base::OnceClosure closure);

  // Called when a Wrapper's binding has an error.
  void WrapperBindingError(Wrapper* wrapper);

  // Unsets the parent pointer. Prevents URLLoaderInterceptor::Intercept from
  // being called.
  void UnsetParent() {
    base::AutoLock lock(intercept_lock_);
    parent_ = nullptr;
  }

  void Shutdown(base::OnceClosure closure) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
    subresource_wrappers_.clear();
    url_loader_factory::SetHasInterceptorOnIOThreadForTesting(false);

    if (closure)
      std::move(closure).Run();
  }

  void CreateURLLoaderFactoryForRenderProcess(
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
  raw_ptr<URLLoaderInterceptor> parent_ GUARDED_BY(intercept_lock_);

  URLLoaderCompletionStatusCallback completion_status_callback_;

  // For intercepting requests via network service. There is one per factory
  // created via RenderProcessHost::CreateURLLoaderFactory. Only accessed on IO
  // thread.
  std::set<std::unique_ptr<Wrapper>, base::UniquePtrComparator>
      subresource_wrappers_;
};

class URLLoaderClientInterceptor : public network::mojom::URLLoaderClient {
 public:
  explicit URLLoaderClientInterceptor(
      base::OnceCallback<network::mojom::URLLoaderFactory*()> factory_getter,
      URLLoaderInterceptor::RequestParams params,
      const URLLoaderInterceptor::URLLoaderCompletionStatusCallback&
          completion_status_callback)
      : original_client_(std::move(params.client)),
        completion_status_callback_(std::move(completion_status_callback)),
        request_url_(params.url_request.url) {
    std::move(factory_getter)
        .Run()
        ->CreateLoaderAndStart(
            std::move(params.receiver), params.request_id, params.options,
            std::move(params.url_request),
            delegating_client_receiver_.BindNewPipeAndPassRemote(),
            params.traffic_annotation);
  }

  void OnReceiveEarlyHints(network::mojom::EarlyHintsPtr early_hints) override {
    original_client_->OnReceiveEarlyHints(std::move(early_hints));
  }

  void OnReceiveResponse(
      network::mojom::URLResponseHeadPtr head,
      mojo::ScopedDataPipeConsumerHandle body,
      std::optional<mojo_base::BigBuffer> cached_metadata) override {
    original_client_->OnReceiveResponse(std::move(head), std::move(body),
                                        std::move(cached_metadata));
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

  void OnTransferSizeUpdated(int32_t transfer_size_diff) override {
    original_client_->OnTransferSizeUpdated(transfer_size_diff);
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
  using ProcessIdGetter = base::RepeatingCallback<int()>;
  using OriginalFactoryGetter =
      base::RepeatingCallback<network::mojom::URLLoaderFactory*()>;

  Interceptor(URLLoaderInterceptor::IOState* parent,
              ProcessIdGetter process_id_getter,
              OriginalFactoryGetter original_factory_getter)
      : parent_(parent),
        process_id_getter_(std::move(process_id_getter)),
        original_factory_getter_(std::move(original_factory_getter)) {
    receivers_.set_disconnect_handler(base::BindRepeating(
        &Interceptor::OnConnectionError, base::Unretained(this)));
  }

  Interceptor(const Interceptor&) = delete;
  Interceptor& operator=(const Interceptor&) = delete;

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
      int32_t request_id,
      uint32_t options,
      const network::ResourceRequest& url_request,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation)
      override {
    RequestParams params;
    params.process_id = process_id_getter_.Run();
    params.receiver = std::move(receiver);
    params.request_id = request_id;
    params.options = options;
    params.url_request = std::move(url_request);
    params.client.Bind(std::move(client));
    params.traffic_annotation = traffic_annotation;

    if (parent_->Intercept(&params))
      return;

    url_loader_client_interceptors_.push_back(
        std::make_unique<URLLoaderClientInterceptor>(
            original_factory_getter_, std::move(params),
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

  raw_ptr<URLLoaderInterceptor::IOState> parent_;
  ProcessIdGetter process_id_getter_;
  OriginalFactoryGetter original_factory_getter_;
  mojo::ReceiverSet<network::mojom::URLLoaderFactory> receivers_;
  base::OnceClosure error_handler_;
  std::vector<std::unique_ptr<URLLoaderClientInterceptor>>
      url_loader_client_interceptors_;
};

class URLLoaderInterceptor::Wrapper final {
 public:
  Wrapper(
      URLLoaderInterceptor::IOState* parent,
      mojo::PendingReceiver<network::mojom::URLLoaderFactory> factory_receiver,
      int process_id,
      mojo::PendingRemote<network::mojom::URLLoaderFactory> original_factory)
      : interceptor_(
            parent,
            base::BindRepeating([](int process_id) { return process_id; },
                                process_id),
            base::BindRepeating(&Wrapper::GetOriginalFactory,
                                base::Unretained(this))),
        original_factory_(std::move(original_factory)) {
    interceptor_.BindReceiver(std::move(factory_receiver));
  }

  Wrapper(const Wrapper&) = delete;
  Wrapper& operator=(const Wrapper&) = delete;

  ~Wrapper() = default;

  Interceptor& GetTestingInterceptor() { return interceptor_; }

 private:
  network::mojom::URLLoaderFactory* GetOriginalFactory() {
    return original_factory_.get();
  }

  Interceptor interceptor_;
  mojo::Remote<network::mojom::URLLoaderFactory> original_factory_;
};

URLLoaderInterceptor::RequestParams::RequestParams() = default;
URLLoaderInterceptor::RequestParams::~RequestParams() = default;
URLLoaderInterceptor::RequestParams::RequestParams(RequestParams&& other) =
    default;
URLLoaderInterceptor::RequestParams& URLLoaderInterceptor::RequestParams::
operator=(RequestParams&& other) = default;

URLLoaderInterceptor::URLLoaderInterceptor(
    InterceptCallback intercept_callback,
    const URLLoaderCompletionStatusCallback& completion_status_callback,
    base::OnceClosure ready_callback)
    : callback_(std::move(intercept_callback)),
      io_thread_(base::MakeRefCounted<IOState>(this)) {
  DCHECK(!BrowserThread::IsThreadInitialized(BrowserThread::UI) ||
         BrowserThread::CurrentlyOn(BrowserThread::UI));
  use_runloop_ = !ready_callback;
  url_loader_factory::SetInterceptorForTesting(base::BindRepeating(
      &URLLoaderInterceptor::InterceptorCallback, base::Unretained(this)));

  if (BrowserThread::IsThreadInitialized(BrowserThread::IO)) {
    if (use_runloop_) {
      base::RunLoop run_loop;
      GetIOThreadTaskRunner({})->PostTask(
          FROM_HERE,
          base::BindOnce(&URLLoaderInterceptor::IOState::Initialize, io_thread_,
                         std::move(completion_status_callback),
                         run_loop.QuitClosure()));
      run_loop.Run();
    } else {
      base::OnceClosure wrapped_callback = base::BindOnce(
          [](base::OnceClosure callback) {
            GetUIThreadTaskRunner({})->PostTask(FROM_HERE, std::move(callback));
          },
          std::move(ready_callback));

      GetIOThreadTaskRunner({})->PostTask(
          FROM_HERE,
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

  url_loader_factory::SetInterceptorForTesting({});

  if (use_runloop_) {
    base::RunLoop run_loop;
    GetIOThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(&URLLoaderInterceptor::IOState::Shutdown,
                                  io_thread_, run_loop.QuitClosure()));
    run_loop.Run();
  } else {
    GetIOThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(&URLLoaderInterceptor::IOState::Shutdown,
                                  io_thread_, base::OnceClosure()));
  }
}

const GURL& URLLoaderInterceptor::GetLastRequestURL() {
  base::AutoLock lock(last_request_lock_);
  return last_request_url_;
}

const net::HttpRequestHeaders& URLLoaderInterceptor::GetLastRequestHeaders() {
  base::AutoLock lock(last_request_lock_);
  return last_request_headers_;
}

void URLLoaderInterceptor::SetLastRequestURL(const GURL& url) {
  base::AutoLock lock(last_request_lock_);
  last_request_url_ = url;
}

void URLLoaderInterceptor::SetLastRequestHeaders(
    const net::HttpRequestHeaders& headers) {
  base::AutoLock lock(last_request_lock_);
  last_request_headers_ = headers;
}

// static
std::unique_ptr<URLLoaderInterceptor>
URLLoaderInterceptor::ServeFilesFromDirectoryAtOrigin(
    const std::string& relative_base_path,
    const GURL& origin,
    base::RepeatingCallback<void(const GURL&)> callback) {
  return std::make_unique<URLLoaderInterceptor>(base::BindLambdaForTesting(
      [=](content::URLLoaderInterceptor::RequestParams* params) -> bool {
        // Ignore requests for other origins.
        if (params->url_request.url.DeprecatedGetOriginAsURL() !=
            origin.DeprecatedGetOriginAsURL())
          return false;

        // Remove the leading slash from the url path, so that it can be
        // treated as a relative path by base::FilePath::AppendASCII.
        auto path = base::TrimString(params->url_request.url.path_piece(), "/",
                                     base::TRIM_LEADING);

        // URLLoaderInterceptor insists that all files exist unless
        // explicitly said to be failing.  Many browsertests fetch
        // nonessential urls like favicons, so just ignore missing files
        // entirely, to behave more like net::test::EmbeddedTestServer.
        base::ScopedAllowBlockingForTesting allow_blocking;
        auto full_path = GetDataFilePath(relative_base_path).AppendASCII(path);
        if (!base::PathExists(full_path))
          return false;

        callback.Run(params->url_request.url);
        content::URLLoaderInterceptor::WriteResponse(
            full_path, params->client.get(), /*headers=*/nullptr,
            /*ssl_info=*/std::nullopt, /*url=*/params->url_request.url);
        return true;
      }));
}

void URLLoaderInterceptor::WriteResponse(
    std::string_view headers,
    std::string_view body,
    network::mojom::URLLoaderClient* client,
    std::optional<net::SSLInfo> ssl_info,
    std::optional<GURL> url) {
  net::HttpResponseInfo info;
  info.headers = base::MakeRefCounted<net::HttpResponseHeaders>(
      net::HttpUtil::AssembleRawHeaders(headers));
  auto response = network::mojom::URLResponseHead::New();
  response->headers = info.headers;
  response->headers->GetMimeType(&response->mime_type);
  if (url.has_value()) {
    response->parsed_headers =
        network::PopulateParsedHeaders(response->headers.get(), *url);
  }
  response->ssl_info = std::move(ssl_info);

  mojo::ScopedDataPipeProducerHandle producer_handle;
  mojo::ScopedDataPipeConsumerHandle consumer_handle;

  MojoCreateDataPipeOptions options;
  options.struct_size = sizeof(MojoCreateDataPipeOptions);
  options.flags = MOJO_CREATE_DATA_PIPE_FLAG_NONE;
  options.element_num_bytes = 1;
  options.capacity_num_bytes = body.size();

  MojoResult result =
      CreateDataPipe(&options, producer_handle, consumer_handle);
  CHECK_EQ(result, MOJO_RESULT_OK);

  result = producer_handle->WriteAllData(base::as_byte_span(body));
  CHECK_EQ(result, MOJO_RESULT_OK);
  // Ok to ignore `actually_written_bytes` because of `...ALL_OR_NONE` flag.

  client->OnReceiveResponse(std::move(response), std::move(consumer_handle),
                            std::nullopt);

  network::URLLoaderCompletionStatus status;
  status.decoded_body_length = body.size();
  status.error_code = net::OK;
  client->OnComplete(status);
}

void URLLoaderInterceptor::WriteResponse(
    const std::string& relative_path,
    network::mojom::URLLoaderClient* client,
    const std::string* headers,
    std::optional<net::SSLInfo> ssl_info,
    std::optional<GURL> url) {
  return WriteResponse(GetDataFilePath(relative_path), client, headers,
                       std::move(ssl_info), std::move(url));
}

void URLLoaderInterceptor::WriteResponse(
    const base::FilePath& file_path,
    network::mojom::URLLoaderClient* client,
    const std::string* headers,
    std::optional<net::SSLInfo> ssl_info,
    std::optional<GURL> url) {
  base::ScopedAllowBlockingForTesting allow_io;
  std::string headers_str;
  if (headers) {
    headers_str = *headers;
  } else {
    base::FilePath headers_path(
        file_path.AddExtension(net::test_server::kMockHttpHeadersExtension));
    if (base::PathExists(headers_path)) {
      headers_str = ReadFile(headers_path);
    } else {
      headers_str = "HTTP/1.0 200 OK\nContent-type: " +
                    net::test_server::GetContentType(file_path) + "\n\n";
    }
  }
  WriteResponse(headers_str, ReadFile(file_path), client, std::move(ssl_info),
                std::move(url));
}

void URLLoaderInterceptor::InterceptorCallback(
    int process_id,
    network::URLLoaderFactoryBuilder& factory_builder) {
  auto [receiver, original_factory] = factory_builder.Append();

  if (process_id != network::mojom::kBrowserProcessId) {
    // Intercept requests from renderer processes on the IO thread.
    if (!BrowserThread::CurrentlyOn(BrowserThread::IO)) {
      GetIOThreadTaskRunner({})->PostTask(
          FROM_HERE, base::BindOnce(&URLLoaderInterceptor::IOState::
                                        CreateURLLoaderFactoryForRenderProcess,
                                    io_thread_, std::move(receiver), process_id,
                                    std::move(original_factory)));
      return;
    }
    io_thread_->CreateURLLoaderFactoryForRenderProcess(
        std::move(receiver), process_id, std::move(original_factory));
  } else {
    // Intercept requests from the browser process on the UI thread.
    CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    wrappers_on_ui_thread_.emplace(std::make_unique<Wrapper>(
        io_thread_.get(), std::move(receiver),
        network::mojom::kBrowserProcessId, std::move(original_factory)));
  }
}

bool URLLoaderInterceptor::Intercept(RequestParams* params) {
  if (callback_.Run(params)) {
    // Only set the last request url and headers if the request was actually
    // processed by the interceptor.
    SetLastRequestURL(params->url_request.url);
    SetLastRequestHeaders(params->url_request.headers);
    return true;
  }

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

void URLLoaderInterceptor::IOState::WrapperBindingError(Wrapper* wrapper) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  auto it = subresource_wrappers_.find(wrapper);
  CHECK(it != subresource_wrappers_.end());
  subresource_wrappers_.erase(it);
}

void URLLoaderInterceptor::IOState::Initialize(
    const URLLoaderCompletionStatusCallback& completion_status_callback,
    base::OnceClosure closure) {
  completion_status_callback_ = std::move(completion_status_callback);
  url_loader_factory::SetHasInterceptorOnIOThreadForTesting(true);
  if (closure)
    std::move(closure).Run();
}

void URLLoaderInterceptor::IOState::CreateURLLoaderFactoryForRenderProcess(
    mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver,
    int process_id,
    mojo::PendingRemote<network::mojom::URLLoaderFactory> original_factory) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  auto wrapper = std::make_unique<Wrapper>(
      this, std::move(receiver), process_id, std::move(original_factory));
  wrapper->GetTestingInterceptor().SetConnectionErrorHandler(
      base::BindOnce(&URLLoaderInterceptor::IOState::WrapperBindingError,
                     base::Unretained(this), base::Unretained(wrapper.get())));
  subresource_wrappers_.emplace(std::move(wrapper));
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
