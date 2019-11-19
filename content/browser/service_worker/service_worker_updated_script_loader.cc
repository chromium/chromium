// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_updated_script_loader.h"

#include <memory>
#include <vector>

#include "base/bind.h"
#include "base/numerics/safe_conversions.h"
#include "base/task/post_task.h"
#include "content/browser/appcache/appcache_response.h"
#include "content/browser/service_worker/service_worker_cache_writer.h"
#include "content/browser/service_worker/service_worker_consts.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/service_worker/service_worker_disk_cache.h"
#include "content/browser/service_worker/service_worker_loader_helpers.h"
#include "content/browser/service_worker/service_worker_storage.h"
#include "content/browser/service_worker/service_worker_version.h"
#include "content/browser/url_loader_factory_getter.h"
#include "content/common/service_worker/service_worker_utils.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_client.h"
#include "net/base/ip_endpoint.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/cert/cert_status_flags.h"
#include "services/network/public/cpp/resource_response.h"
#include "third_party/blink/public/common/loader/throttling_url_loader.h"
#include "third_party/blink/public/common/service_worker/service_worker_utils.h"

namespace content {

// We chose this size because the AppCache uses this.
const uint32_t ServiceWorkerUpdatedScriptLoader::kReadBufferSize = 32768;

ServiceWorkerUpdatedScriptLoader::ThrottlingURLLoaderCoreWrapper::LoaderOnUI::
    LoaderOnUI() = default;
ServiceWorkerUpdatedScriptLoader::ThrottlingURLLoaderCoreWrapper::LoaderOnUI::
    ~LoaderOnUI() = default;
ServiceWorkerUpdatedScriptLoader::ThrottlingURLLoaderCoreWrapper::
    ThrottlingURLLoaderCoreWrapper()
    : loader_on_ui_(new LoaderOnUI()) {}
ServiceWorkerUpdatedScriptLoader::ThrottlingURLLoaderCoreWrapper::
    ~ThrottlingURLLoaderCoreWrapper() = default;

// static
std::unique_ptr<
    ServiceWorkerUpdatedScriptLoader::ThrottlingURLLoaderCoreWrapper>
ServiceWorkerUpdatedScriptLoader::ThrottlingURLLoaderCoreWrapper::
    CreateLoaderAndStart(
        std::unique_ptr<network::SharedURLLoaderFactoryInfo>
            loader_factory_info,
        BrowserContextGetter browser_context_getter,
        int32_t routing_id,
        int32_t request_id,
        uint32_t options,
        const network::ResourceRequest& resource_request,
        mojo::PendingRemote<network::mojom::URLLoaderClient> client,
        const net::NetworkTrafficAnnotationTag& traffic_annotation) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  auto wrapper = base::WrapUnique(new ThrottlingURLLoaderCoreWrapper());

  RunOrPostTaskOnThread(
      FROM_HERE, BrowserThread::UI,
      base::BindOnce(&ThrottlingURLLoaderCoreWrapper::StartInternalOnUI,
                     std::move(loader_factory_info),
                     std::move(browser_context_getter), routing_id, request_id,
                     options, network::ResourceRequest(resource_request),
                     std::move(client),
                     net::NetworkTrafficAnnotationTag(traffic_annotation),
                     base::Unretained(wrapper->loader_on_ui_.get())));
  return wrapper;
}

// static
void ServiceWorkerUpdatedScriptLoader::ThrottlingURLLoaderCoreWrapper::
    StartInternalOnUI(
        std::unique_ptr<network::SharedURLLoaderFactoryInfo>
            loader_factory_info,
        BrowserContextGetter browser_context_getter,
        int32_t routing_id,
        int32_t request_id,
        uint32_t options,
        network::ResourceRequest resource_request,
        mojo::PendingRemote<network::mojom::URLLoaderClient> client_remote,
        net::NetworkTrafficAnnotationTag traffic_annotation,
        LoaderOnUI* loader_on_ui) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  BrowserContext* browser_context = browser_context_getter.Run();
  if (!browser_context)
    return;

  // Service worker update checking doesn't have a relevant frame and tab, so
  // that |wc_getter| returns nullptr and the frame id is set to
  // kNoFrameTreeNodeId.
  base::RepeatingCallback<WebContents*()> wc_getter =
      base::BindRepeating([]() -> WebContents* { return nullptr; });
  std::vector<std::unique_ptr<blink::URLLoaderThrottle>> throttles =
      GetContentClient()->browser()->CreateURLLoaderThrottles(
          resource_request, browser_context, std::move(wc_getter),
          /*navigation_ui_data=*/nullptr, RenderFrameHost::kNoFrameTreeNodeId);

  mojo::Remote<network::mojom::URLLoaderClient> client(
      std::move(client_remote));
  auto loader = blink::ThrottlingURLLoader::CreateLoaderAndStart(
      network::SharedURLLoaderFactory::Create(std::move(loader_factory_info)),
      std::move(throttles), routing_id, request_id, options, &resource_request,
      client.get(), traffic_annotation, base::ThreadTaskRunnerHandle::Get());
  loader_on_ui->loader = std::move(loader);
  loader_on_ui->client = std::move(client);
}

void ServiceWorkerUpdatedScriptLoader::ThrottlingURLLoaderCoreWrapper::
    SetPriority(net::RequestPriority priority, int32_t intra_priority_value) {
  RunOrPostTaskOnThread(
      FROM_HERE, BrowserThread::UI,
      base::BindOnce(
          [](LoaderOnUI* loader_on_ui, net::RequestPriority priority,
             int32_t intra_priority_value) {
            DCHECK(loader_on_ui->loader);
            loader_on_ui->loader->SetPriority(priority, intra_priority_value);
          },
          base::Unretained(loader_on_ui_.get()), priority,
          intra_priority_value));
}

void ServiceWorkerUpdatedScriptLoader::ThrottlingURLLoaderCoreWrapper::
    PauseReadingBodyFromNet() {
  RunOrPostTaskOnThread(FROM_HERE, BrowserThread::UI,
                        base::BindOnce(
                            [](LoaderOnUI* loader_on_ui) {
                              DCHECK(loader_on_ui->loader);
                              loader_on_ui->loader->PauseReadingBodyFromNet();
                            },
                            base::Unretained(loader_on_ui_.get())));
}

void ServiceWorkerUpdatedScriptLoader::ThrottlingURLLoaderCoreWrapper::
    ResumeReadingBodyFromNet() {
  RunOrPostTaskOnThread(FROM_HERE, BrowserThread::UI,
                        base::BindOnce(
                            [](LoaderOnUI* loader_on_ui) {
                              DCHECK(loader_on_ui->loader);
                              loader_on_ui->loader->ResumeReadingBodyFromNet();
                            },
                            base::Unretained(loader_on_ui_.get())));
}

// This is for debugging https://crbug.com/959627.
// The purpose is to see where the IOBuffer comes from by checking |__vfptr|.
class ServiceWorkerUpdatedScriptLoader::WrappedIOBuffer
    : public net::WrappedIOBuffer {
 public:
  WrappedIOBuffer(const char* data) : net::WrappedIOBuffer(data) {}

 private:
  ~WrappedIOBuffer() override = default;

  // This is to make sure that the vtable is not merged with other classes.
  virtual void dummy() { NOTREACHED(); }
};

std::unique_ptr<ServiceWorkerUpdatedScriptLoader>
ServiceWorkerUpdatedScriptLoader::CreateAndStart(
    uint32_t options,
    const network::ResourceRequest& original_request,
    mojo::PendingRemote<network::mojom::URLLoaderClient> client,
    scoped_refptr<ServiceWorkerVersion> version) {
  DCHECK(blink::ServiceWorkerUtils::IsImportedScriptUpdateCheckEnabled());
  return base::WrapUnique(new ServiceWorkerUpdatedScriptLoader(
      options, original_request, std::move(client), version));
}

ServiceWorkerUpdatedScriptLoader::ServiceWorkerUpdatedScriptLoader(
    uint32_t options,
    const network::ResourceRequest& original_request,
    mojo::PendingRemote<network::mojom::URLLoaderClient> client,
    scoped_refptr<ServiceWorkerVersion> version)
    : request_url_(original_request.url),
      resource_type_(static_cast<ResourceType>(original_request.resource_type)),
      options_(options),
      version_(std::move(version)),
      network_watcher_(FROM_HERE,
                       mojo::SimpleWatcher::ArmingPolicy::MANUAL,
                       base::SequencedTaskRunnerHandle::Get()),
      client_(std::move(client)),
      client_producer_watcher_(FROM_HERE,
                               mojo::SimpleWatcher::ArmingPolicy::MANUAL,
                               base::SequencedTaskRunnerHandle::Get()),
      request_start_(base::TimeTicks::Now()) {
#if DCHECK_IS_ON()
  CheckVersionStatusBeforeLoad();
#endif  // DCHECK_IS_ON()

  DCHECK(client_);
  ServiceWorkerUpdateChecker::ComparedScriptInfo info =
      version_->TakeComparedScriptInfo(request_url_);
  if (info.result == ServiceWorkerSingleScriptUpdateChecker::Result::kFailed) {
    DCHECK(!info.paused_state);
    // A network error received during update checking. This replays it.
    CommitCompleted(info.failure_info->network_status,
                    info.failure_info->error_message);
    return;
  }

  cache_writer_ = std::move(info.paused_state->cache_writer);
  DCHECK(cache_writer_);

  network_loader_ = std::move(info.paused_state->network_loader);
  pending_network_client_receiver_ =
      std::move(info.paused_state->network_client_receiver);
  network_consumer_ = std::move(info.paused_state->network_consumer);

  network_loader_state_ = info.paused_state->network_loader_state;
  DCHECK(network_loader_state_ == LoaderState::kLoadingBody ||
         network_loader_state_ == LoaderState::kCompleted);

  body_writer_state_ = info.paused_state->body_writer_state;
  DCHECK(body_writer_state_ == WriterState::kWriting ||
         body_writer_state_ == WriterState::kCompleted);

  version_->script_cache_map()->NotifyStartedCaching(
      request_url_, cache_writer_->WriterResourceId());

  // Resume the cache writer and observe its writes, so all data written
  // is sent to |client_|.
  cache_writer_->set_write_observer(this);
  net::Error error = cache_writer_->Resume(
      base::BindOnce(&ServiceWorkerUpdatedScriptLoader::OnCacheWriterResumed,
                     weak_factory_.GetWeakPtr()));

  if (error != net::ERR_IO_PENDING) {
    OnCacheWriterResumed(error);
  }
}

ServiceWorkerUpdatedScriptLoader::~ServiceWorkerUpdatedScriptLoader() = default;

void ServiceWorkerUpdatedScriptLoader::FollowRedirect(
    const std::vector<std::string>& removed_headers,
    const net::HttpRequestHeaders& modified_headers,
    const base::Optional<GURL>& new_url) {
  // Resource requests for service worker scripts should not follow redirects.
  // See comments in OnReceiveRedirect().
  NOTREACHED();
}

void ServiceWorkerUpdatedScriptLoader::SetPriority(
    net::RequestPriority priority,
    int32_t intra_priority_value) {
  if (network_loader_)
    network_loader_->SetPriority(priority, intra_priority_value);
}

void ServiceWorkerUpdatedScriptLoader::PauseReadingBodyFromNet() {
  if (network_loader_)
    network_loader_->PauseReadingBodyFromNet();
}

void ServiceWorkerUpdatedScriptLoader::ResumeReadingBodyFromNet() {
  if (network_loader_)
    network_loader_->ResumeReadingBodyFromNet();
}

// URLLoaderClient for network loader ------------------------------------------

void ServiceWorkerUpdatedScriptLoader::OnReceiveResponse(
    network::mojom::URLResponseHeadPtr response_head) {
  NOTREACHED();
}

void ServiceWorkerUpdatedScriptLoader::OnReceiveRedirect(
    const net::RedirectInfo& redirect_info,
    network::mojom::URLResponseHeadPtr response_head) {
  NOTREACHED();
}

void ServiceWorkerUpdatedScriptLoader::OnUploadProgress(
    int64_t current_position,
    int64_t total_size,
    OnUploadProgressCallback ack_callback) {
  NOTREACHED();
}

void ServiceWorkerUpdatedScriptLoader::OnReceiveCachedMetadata(
    mojo_base::BigBuffer data) {
  client_->OnReceiveCachedMetadata(std::move(data));
}

void ServiceWorkerUpdatedScriptLoader::OnTransferSizeUpdated(
    int32_t transfer_size_diff) {
  client_->OnTransferSizeUpdated(transfer_size_diff);
}

void ServiceWorkerUpdatedScriptLoader::OnStartLoadingResponseBody(
    mojo::ScopedDataPipeConsumerHandle consumer) {
  NOTREACHED();
}

void ServiceWorkerUpdatedScriptLoader::OnComplete(
    const network::URLLoaderCompletionStatus& status) {
  LoaderState previous_state = network_loader_state_;
  network_loader_state_ = LoaderState::kCompleted;
  if (status.error_code != net::OK) {
    CommitCompleted(status,
                    ServiceWorkerConsts::kServiceWorkerFetchScriptError);
    return;
  }

  DCHECK_EQ(LoaderState::kLoadingBody, previous_state);
  switch (body_writer_state_) {
    case WriterState::kNotStarted:
      NOTREACHED();
      return;
    case WriterState::kWriting:
      // Wait until it's written. OnNetworkDataAvailable() will call
      // CommitCompleted() after all data from |network_consumer_| is
      // consumed.
      return;
    case WriterState::kCompleted:
      CommitCompleted(network::URLLoaderCompletionStatus(net::OK),
                      std::string() /* status_message */);
      return;
  }
  NOTREACHED();
}

// End of URLLoaderClient ------------------------------------------------------

int ServiceWorkerUpdatedScriptLoader::WillWriteInfo(
    scoped_refptr<HttpResponseInfoIOBuffer> response_info) {
  DCHECK(response_info);
  const net::HttpResponseInfo* info = response_info->http_info.get();
  DCHECK(info);

  if (resource_type_ == ResourceType::kServiceWorker) {
    version_->SetMainScriptHttpResponseInfo(*info);
  }

  auto response = ServiceWorkerUtils::CreateResourceResponseHeadAndMetadata(
      info, options_, request_start_, base::TimeTicks::Now(),
      response_info->response_data_size);
  // Don't pass SSLInfo to the client when the original request doesn't ask
  // to send it.
  if (response.head.ssl_info.has_value() &&
      !(options_ & network::mojom::kURLLoadOptionSendSSLInfoWithResponse)) {
    response.head.ssl_info.reset();
  }

  client_->OnReceiveResponse(std::move(response.head));
  if (!response.metadata.empty())
    client_->OnReceiveCachedMetadata(std::move(response.metadata));

  mojo::ScopedDataPipeConsumerHandle client_consumer;
  if (mojo::CreateDataPipe(nullptr, &client_producer_, &client_consumer) !=
      MOJO_RESULT_OK) {
    // Reports error to cache writer and finally the loader would process this
    // failure in OnCacheWriterResumed()
    return net::ERR_INSUFFICIENT_RESOURCES;
  }

  // Pass the consumer handle to the client.
  client_->OnStartLoadingResponseBody(std::move(client_consumer));
  client_producer_watcher_.Watch(
      client_producer_.get(), MOJO_HANDLE_SIGNAL_WRITABLE,
      base::BindRepeating(&ServiceWorkerUpdatedScriptLoader::OnClientWritable,
                          weak_factory_.GetWeakPtr()));
  return net::OK;
}

void ServiceWorkerUpdatedScriptLoader::OnClientWritable(MojoResult) {
  DCHECK(data_to_send_);
  DCHECK_GE(data_length_, bytes_sent_to_client_);
  DCHECK(client_producer_);

  // Cap the buffer size up to |kReadBufferSize|. The remaining will be written
  // next time.
  uint32_t bytes_newly_sent =
      std::min<uint32_t>(kReadBufferSize, data_length_ - bytes_sent_to_client_);

  MojoResult result =
      client_producer_->WriteData(data_to_send_->data() + bytes_sent_to_client_,
                                  &bytes_newly_sent, MOJO_WRITE_DATA_FLAG_NONE);

  if (result == MOJO_RESULT_SHOULD_WAIT) {
    // No data was written to |client_producer_| because the pipe was full.
    // Retry when the pipe becomes ready again.
    client_producer_watcher_.ArmOrNotify();
    return;
  }

  if (result != MOJO_RESULT_OK) {
    ServiceWorkerMetrics::CountWriteResponseResult(
        ServiceWorkerMetrics::WRITE_DATA_ERROR);
    CommitCompleted(network::URLLoaderCompletionStatus(net::ERR_FAILED),
                    ServiceWorkerConsts::kServiceWorkerFetchScriptError);
    return;
  }

  bytes_sent_to_client_ += bytes_newly_sent;
  if (bytes_sent_to_client_ != data_length_) {
    // Not all data is sent. Send the rest in another task.
    client_producer_watcher_.ArmOrNotify();
    return;
  }
  std::move(write_observer_complete_callback_).Run(net::OK);
}

int ServiceWorkerUpdatedScriptLoader::WillWriteData(
    scoped_refptr<net::IOBuffer> data,
    int length,
    base::OnceCallback<void(net::Error)> callback) {
  DCHECK(!write_observer_complete_callback_);
  DCHECK(client_producer_);

  data_to_send_ = std::move(data);
  data_length_ = length;
  bytes_sent_to_client_ = 0;
  write_observer_complete_callback_ = std::move(callback);
  client_producer_watcher_.ArmOrNotify();
  return net::ERR_IO_PENDING;
}

void ServiceWorkerUpdatedScriptLoader::OnCacheWriterResumed(net::Error error) {
  DCHECK_NE(error, net::ERR_IO_PENDING);
  // Stop observing write operations in cache writer as further data are
  // from network which would be processed by OnNetworkDataAvailable().
  cache_writer_->set_write_observer(nullptr);

  if (error != net::OK) {
    CommitCompleted(network::URLLoaderCompletionStatus(error),
                    ServiceWorkerConsts::kDatabaseErrorMessage);
    return;
  }
  // If the script has no body or all the body has already been read when it
  // was paused, we don't have to wait for more data from network.
  if (body_writer_state_ == WriterState::kCompleted) {
    CommitCompleted(network::URLLoaderCompletionStatus(net::OK), std::string());
    return;
  }

  // Continue to load the rest of the body from the network.
  DCHECK_EQ(body_writer_state_, WriterState::kWriting);
  DCHECK(network_consumer_);
  network_client_receiver_.Bind(std::move(pending_network_client_receiver_));
  network_watcher_.Watch(
      network_consumer_.get(),
      MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_PEER_CLOSED,
      base::BindRepeating(
          &ServiceWorkerUpdatedScriptLoader::OnNetworkDataAvailable,
          weak_factory_.GetWeakPtr()));
  network_watcher_.ArmOrNotify();
}

#if DCHECK_IS_ON()
void ServiceWorkerUpdatedScriptLoader::CheckVersionStatusBeforeLoad() {
  DCHECK(version_);

  // ServiceWorkerUpdatedScriptLoader is used for fetching the service worker
  // main script (RESOURCE_TYPE_SERVICE_WORKER) during worker startup or
  // importScripts() (RESOURCE_TYPE_SCRIPT).
  // TODO(nhiroki): In the current implementation, importScripts() can be called
  // in any ServiceWorkerVersion::Status except for REDUNDANT, but the spec
  // defines importScripts() works only on the initial script evaluation and the
  // install event. Update this check once importScripts() is fixed.
  // (https://crbug.com/719052)
  DCHECK((resource_type_ == ResourceType::kServiceWorker &&
          version_->status() == ServiceWorkerVersion::NEW) ||
         (resource_type_ == ResourceType::kScript &&
          version_->status() != ServiceWorkerVersion::REDUNDANT));
}
#endif  // DCHECK_IS_ON()

void ServiceWorkerUpdatedScriptLoader::OnNetworkDataAvailable(MojoResult) {
  DCHECK_EQ(WriterState::kWriting, body_writer_state_);
  DCHECK(network_consumer_.is_valid());
  scoped_refptr<network::MojoToNetPendingBuffer> pending_buffer;
  uint32_t bytes_available = 0;
  MojoResult result = network::MojoToNetPendingBuffer::BeginRead(
      &network_consumer_, &pending_buffer, &bytes_available);
  switch (result) {
    case MOJO_RESULT_OK:
      WriteData(std::move(pending_buffer), bytes_available);
      return;
    case MOJO_RESULT_FAILED_PRECONDITION:
      // Call WriteData() with null buffer to let the cache writer know that
      // body from the network reaches to the end.
      WriteData(/*pending_buffer=*/nullptr, /*bytes_available=*/0);
      return;
    case MOJO_RESULT_SHOULD_WAIT:
      network_watcher_.ArmOrNotify();
      return;
  }
  NOTREACHED() << static_cast<int>(result);
}

void ServiceWorkerUpdatedScriptLoader::WriteData(
    scoped_refptr<network::MojoToNetPendingBuffer> pending_buffer,
    uint32_t bytes_available) {
  // Cap the buffer size up to |kReadBufferSize|. The remaining will be written
  // next time.
  uint32_t bytes_written = std::min<uint32_t>(kReadBufferSize, bytes_available);

  auto buffer = base::MakeRefCounted<WrappedIOBuffer>(
      pending_buffer ? pending_buffer->buffer() : nullptr);
  MojoResult result = client_producer_->WriteData(
      buffer->data(), &bytes_written, MOJO_WRITE_DATA_FLAG_NONE);
  switch (result) {
    case MOJO_RESULT_OK:
      break;
    case MOJO_RESULT_FAILED_PRECONDITION:
      ServiceWorkerMetrics::CountWriteResponseResult(
          ServiceWorkerMetrics::WRITE_DATA_ERROR);
      CommitCompleted(network::URLLoaderCompletionStatus(net::ERR_FAILED),
                      ServiceWorkerConsts::kServiceWorkerFetchScriptError);
      return;
    case MOJO_RESULT_SHOULD_WAIT:
      // No data was written to |client_producer_| because the pipe was full.
      // Retry when the pipe becomes ready again.
      pending_buffer->CompleteRead(0);
      network_consumer_ = pending_buffer->ReleaseHandle();
      network_watcher_.ArmOrNotify();
      return;
    default:
      NOTREACHED() << static_cast<int>(result);
      return;
  }

  // Write the buffer in the service worker script storage up to the size we
  // successfully wrote to the data pipe (i.e., |bytes_written|).
  // A null buffer and zero |bytes_written| are passed when this is the end of
  // the body.
  net::Error error = cache_writer_->MaybeWriteData(
      buffer.get(), base::strict_cast<size_t>(bytes_written),
      base::BindOnce(&ServiceWorkerUpdatedScriptLoader::OnWriteDataComplete,
                     weak_factory_.GetWeakPtr(), pending_buffer,
                     bytes_written));
  if (error == net::ERR_IO_PENDING) {
    // OnWriteDataComplete() will be called asynchronously.
    return;
  }
  // MaybeWriteData() doesn't run the callback if it finishes synchronously, so
  // explicitly call it here.
  OnWriteDataComplete(std::move(pending_buffer), bytes_written, error);
}

void ServiceWorkerUpdatedScriptLoader::OnWriteDataComplete(
    scoped_refptr<network::MojoToNetPendingBuffer> pending_buffer,
    uint32_t bytes_written,
    net::Error error) {
  DCHECK_NE(net::ERR_IO_PENDING, error);
  if (error != net::OK) {
    ServiceWorkerMetrics::CountWriteResponseResult(
        ServiceWorkerMetrics::WRITE_DATA_ERROR);
    CommitCompleted(network::URLLoaderCompletionStatus(error),
                    ServiceWorkerConsts::kDatabaseErrorMessage);
    return;
  }
  ServiceWorkerMetrics::CountWriteResponseResult(
      ServiceWorkerMetrics::WRITE_OK);

  if (bytes_written == 0) {
    // Zero |bytes_written| with net::OK means that all data has been read from
    // the network and the Mojo data pipe has been closed. Thus we can complete
    // the request if OnComplete() has already been received.
    DCHECK(!pending_buffer);
    body_writer_state_ = WriterState::kCompleted;
    if (network_loader_state_ == LoaderState::kCompleted) {
      CommitCompleted(network::URLLoaderCompletionStatus(net::OK),
                      std::string() /* status_message */);
    }
    return;
  }

  DCHECK(pending_buffer);
  pending_buffer->CompleteRead(bytes_written);
  // Get the consumer handle from a previous read operation if we have one.
  network_consumer_ = pending_buffer->ReleaseHandle();
  network_watcher_.ArmOrNotify();
}

void ServiceWorkerUpdatedScriptLoader::CommitCompleted(
    const network::URLLoaderCompletionStatus& status,
    const std::string& status_message) {
  net::Error error_code = static_cast<net::Error>(status.error_code);
  int bytes_written = -1;
  if (error_code == net::OK) {
    DCHECK(cache_writer_);
    DCHECK_EQ(LoaderState::kCompleted, network_loader_state_);
    DCHECK_EQ(WriterState::kCompleted, body_writer_state_);
    // If all the calls to WriteHeaders/WriteData succeeded, but the incumbent
    // entry wasn't actually replaced because the new entry was equivalent, the
    // new version didn't actually install because it already exists.
    if (!cache_writer_->did_replace()) {
      version_->SetStartWorkerStatusCode(
          blink::ServiceWorkerStatusCode::kErrorExists);
      error_code = net::ERR_FILE_EXISTS;
    }
    bytes_written = cache_writer_->bytes_written();
  } else {
    // AddMessageConsole must be called before notifying that an error occurred
    // because the worker stops soon after receiving the error response.
    // TODO(nhiroki): Consider replacing this hacky way with the new error code
    // handling mechanism in URLLoader.
    version_->AddMessageToConsole(blink::mojom::ConsoleMessageLevel::kError,
                                  status_message);
  }

  // Cache writer could be nullptr when update checking observed a network error
  // and this loader hasn't started the caching yet.
  if (cache_writer_) {
    version_->script_cache_map()->NotifyFinishedCaching(
        request_url_, bytes_written, error_code, status_message);
  }

  client_->OnComplete(status);
  client_producer_.reset();
  client_producer_watcher_.Cancel();

  network_loader_.reset();
  network_client_receiver_.reset();
  network_consumer_.reset();
  network_watcher_.Cancel();
  cache_writer_.reset();
  network_loader_state_ = LoaderState::kCompleted;
  body_writer_state_ = WriterState::kCompleted;
}

}  // namespace content
