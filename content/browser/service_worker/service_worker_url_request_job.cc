// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_url_request_job.h"

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/files/file_util.h"
#include "base/guid.h"
#include "base/location.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/task/post_task.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "content/browser/resource_context_impl.h"
#include "content/browser/service_worker/embedded_worker_instance.h"
#include "content/browser/service_worker/service_worker_blob_reader.h"
#include "content/browser/service_worker/service_worker_data_pipe_reader.h"
#include "content/browser/service_worker/service_worker_provider_host.h"
#include "content/browser/service_worker/service_worker_response_info.h"
#include "content/common/service_worker/service_worker_types.h"
#include "content/common/service_worker/service_worker_utils.h"
#include "content/public/browser/blob_handle.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/resource_request_info.h"
#include "content/public/browser/service_worker_context.h"
#include "content/public/common/content_features.h"
#include "content/public/common/referrer.h"
#include "net/base/net_errors.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_response_info.h"
#include "net/http/http_util.h"
#include "net/log/net_log.h"
#include "net/log/net_log_event_type.h"
#include "net/log/net_log_with_source.h"
#include "services/network/public/cpp/resource_request_body.h"
#include "storage/browser/blob/blob_data_builder.h"
#include "storage/browser/blob/blob_data_handle.h"
#include "storage/browser/blob/blob_impl.h"
#include "storage/browser/blob/blob_storage_context.h"
#include "ui/base/page_transition_types.h"

namespace content {

namespace {

net::URLRequestStatus ServiceWorkerResponseErrorToNetStatus(
    blink::mojom::ServiceWorkerResponseError error) {
  if (error ==
      blink::mojom::ServiceWorkerResponseError::kDataPipeCreationFailed) {
    return net::URLRequestStatus::FromError(net::ERR_INSUFFICIENT_RESOURCES);
  }

  // TODO(falken): Add more mapping to net errors.
  return net::URLRequestStatus::FromError(net::ERR_FAILED);
}

net::NetLogEventType RequestJobResultToNetEventType(
    ServiceWorkerMetrics::URLRequestJobResult result) {
  using n = net::NetLogEventType;
  using m = ServiceWorkerMetrics;
  switch (result) {
    case m::REQUEST_JOB_FALLBACK_RESPONSE:
      return n::SERVICE_WORKER_FALLBACK_RESPONSE;
    case m::REQUEST_JOB_FALLBACK_FOR_CORS:
      return n::SERVICE_WORKER_FALLBACK_FOR_CORS;
    case m::REQUEST_JOB_HEADERS_ONLY_RESPONSE:
      return n::SERVICE_WORKER_HEADERS_ONLY_RESPONSE;
    case m::REQUEST_JOB_STREAM_RESPONSE:
      return n::SERVICE_WORKER_STREAM_RESPONSE;
    case m::REQUEST_JOB_BLOB_RESPONSE:
      return n::SERVICE_WORKER_BLOB_RESPONSE;
    case m::REQUEST_JOB_ERROR_RESPONSE_STATUS_ZERO:
      return n::SERVICE_WORKER_ERROR_RESPONSE_STATUS_ZERO;
    case m::REQUEST_JOB_ERROR_BAD_BLOB:
      return n::SERVICE_WORKER_ERROR_BAD_BLOB;
    case m::REQUEST_JOB_ERROR_NO_PROVIDER_HOST:
      return n::SERVICE_WORKER_ERROR_NO_PROVIDER_HOST;
    case m::REQUEST_JOB_ERROR_NO_ACTIVE_VERSION:
      return n::SERVICE_WORKER_ERROR_NO_ACTIVE_VERSION;
    case m::REQUEST_JOB_ERROR_FETCH_EVENT_DISPATCH:
      return n::SERVICE_WORKER_ERROR_FETCH_EVENT_DISPATCH;
    case m::REQUEST_JOB_ERROR_BLOB_READ:
      return n::SERVICE_WORKER_ERROR_BLOB_READ;
    case m::REQUEST_JOB_ERROR_STREAM_ABORTED:
      return n::SERVICE_WORKER_ERROR_STREAM_ABORTED;
    case m::REQUEST_JOB_ERROR_KILLED:
      return n::SERVICE_WORKER_ERROR_KILLED;
    case m::REQUEST_JOB_ERROR_KILLED_WITH_BLOB:
      return n::SERVICE_WORKER_ERROR_KILLED_WITH_BLOB;
    case m::REQUEST_JOB_ERROR_KILLED_WITH_STREAM:
      return n::SERVICE_WORKER_ERROR_KILLED_WITH_STREAM;
    case m::REQUEST_JOB_ERROR_BAD_DELEGATE:
      return n::SERVICE_WORKER_ERROR_BAD_DELEGATE;
    case m::REQUEST_JOB_ERROR_REQUEST_BODY_BLOB_FAILED:
      return n::SERVICE_WORKER_ERROR_REQUEST_BODY_BLOB_FAILED;
    // Invalid type.
    case m::NUM_REQUEST_JOB_RESULT_TYPES:
      NOTREACHED() << result;
  }
  NOTREACHED() << result;
  return n::FAILED;
}

// Does file IO. Use with base::MayBlock().
std::vector<int64_t> GetFileSizes(std::vector<base::FilePath> file_paths) {
  std::vector<int64_t> sizes;
  sizes.reserve(file_paths.size());
  for (const base::FilePath& path : file_paths) {
    base::File::Info file_info;
    if (!base::GetFileInfo(path, &file_info) || file_info.is_directory)
      return std::vector<int64_t>();
    sizes.push_back(file_info.size);
  }
  return sizes;
}

}  // namespace

// Sets the size on each DataElement in the request body that is a file with
// unknown size. This ensures ServiceWorkerURLRequestJob::CreateRequestBodyBlob
// can successfuly create a blob from the data elements, as files with unknown
// sizes are not supported by the blob storage system.
class ServiceWorkerURLRequestJob::FileSizeResolver {
 public:
  explicit FileSizeResolver(ServiceWorkerURLRequestJob* owner)
      : owner_(owner), weak_factory_(this) {
    TRACE_EVENT_ASYNC_BEGIN1("ServiceWorker", "FileSizeResolver", this, "URL",
                             owner_->request()->url().spec());
    owner_->request()->net_log().BeginEvent(
        net::NetLogEventType::SERVICE_WORKER_WAITING_FOR_REQUEST_BODY_FILES);
  }

  ~FileSizeResolver() {
    owner_->request()->net_log().EndEvent(
        net::NetLogEventType::SERVICE_WORKER_WAITING_FOR_REQUEST_BODY_FILES,
        net::NetLog::BoolCallback("success", phase_ == Phase::SUCCESS));
    TRACE_EVENT_ASYNC_END1("ServiceWorker", "FileSizeResolver", this, "Success",
                           phase_ == Phase::SUCCESS);
  }

  void Resolve(base::OnceCallback<void(bool /* success */)> callback) {
    DCHECK_EQ(static_cast<int>(Phase::INITIAL), static_cast<int>(phase_));
    DCHECK(file_elements_.empty());
    phase_ = Phase::WAITING;
    body_ = owner_->body_;
    callback_ = std::move(callback);

    std::vector<base::FilePath> file_paths;
    for (network::DataElement& element : *body_->elements_mutable()) {
      if (element.type() == network::DataElement::TYPE_FILE &&
          element.length() == network::DataElement::kUnknownSize) {
        file_elements_.push_back(&element);
        file_paths.push_back(element.path());
      }
    }
    if (file_elements_.empty()) {
      Complete(true);
      return;
    }

    base::PostTaskWithTraitsAndReplyWithResult(
        FROM_HERE,
        {base::MayBlock(), base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
        base::BindOnce(&GetFileSizes, std::move(file_paths)),
        base::BindOnce(
            &ServiceWorkerURLRequestJob::FileSizeResolver::OnFileSizesResolved,
            weak_factory_.GetWeakPtr()));
  }

 private:
  enum class Phase { INITIAL, WAITING, SUCCESS, FAIL };

  void OnFileSizesResolved(std::vector<int64_t> sizes) {
    bool success = !sizes.empty();
    if (success) {
      DCHECK_EQ(sizes.size(), file_elements_.size());
      size_t num_elements = file_elements_.size();
      for (size_t i = 0; i < num_elements; i++) {
        network::DataElement* element = file_elements_[i];
        element->SetToFilePathRange(element->path(), element->offset(),
                                    base::checked_cast<uint64_t>(sizes[i]),
                                    element->expected_modification_time());
      }
      file_elements_.clear();
    }
    Complete(success);
  }

  void Complete(bool success) {
    DCHECK_EQ(static_cast<int>(Phase::WAITING), static_cast<int>(phase_));
    phase_ = success ? Phase::SUCCESS : Phase::FAIL;
    // Destroys |this|.
    std::move(callback_).Run(success);
  }

  // Owns and must outlive |this|.
  ServiceWorkerURLRequestJob* owner_;

  scoped_refptr<network::ResourceRequestBody> body_;
  std::vector<network::DataElement*> file_elements_;
  base::OnceCallback<void(bool /* success */)> callback_;
  Phase phase_ = Phase::INITIAL;
  base::WeakPtrFactory<FileSizeResolver> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(FileSizeResolver);
};

// A helper for recording navigation preload UMA. The UMA is recorded
// after both service worker preparation finished and the
// navigation preload response arrived.
class ServiceWorkerURLRequestJob::NavigationPreloadMetrics {
 public:
  explicit NavigationPreloadMetrics(ServiceWorkerURLRequestJob* owner)
      : owner_(owner) {}
  ~NavigationPreloadMetrics() {}

  // Called when service worker preparation finished.
  void ReportWorkerPreparationFinished() {
    DCHECK(!owner_->worker_start_time_.is_null());
    DCHECK(!owner_->worker_ready_time_.is_null());
    switch (phase_) {
      case Phase::INITIAL:
        phase_ = Phase::WORKER_PREPARATION_FINISHED;
        break;
      case Phase::NAV_PRELOAD_FINISHED:
        phase_ = Phase::BOTH_FINISHED;
        Complete();
        break;
      case Phase::DO_NOT_RECORD:
        return;
      case Phase::BOTH_FINISHED:
      case Phase::WORKER_PREPARATION_FINISHED:
      case Phase::RECORDED:
        NOTREACHED();
    }
  }

  // Called when the navigation preload response arrived.
  void ReportNavigationPreloadFinished() {
    navigation_preload_response_time_ = base::TimeTicks::Now();
    switch (phase_) {
      case Phase::INITIAL:
        phase_ = Phase::NAV_PRELOAD_FINISHED;
        break;
      case Phase::WORKER_PREPARATION_FINISHED:
        phase_ = Phase::BOTH_FINISHED;
        Complete();
        break;
      case Phase::DO_NOT_RECORD:
        return;
      case Phase::BOTH_FINISHED:
      case Phase::NAV_PRELOAD_FINISHED:
      case Phase::RECORDED:
        NOTREACHED();
    }
  }

  // After Abort() is called, no navigation preload UMA will be recorded for
  // this navigation.
  void Abort() {
    DCHECK_NE(phase_, Phase::RECORDED);
    phase_ = Phase::DO_NOT_RECORD;
  }

 private:
  enum class Phase {
    INITIAL,
    WORKER_PREPARATION_FINISHED,
    NAV_PRELOAD_FINISHED,
    BOTH_FINISHED,
    RECORDED,
    DO_NOT_RECORD
  };

  void Complete() {
    DCHECK_EQ(phase_, Phase::BOTH_FINISHED);
    ServiceWorkerMetrics::RecordNavigationPreloadResponse(
        owner_->worker_ready_time_ - owner_->worker_start_time_,
        navigation_preload_response_time_ - owner_->worker_start_time_,
        owner_->initial_worker_status_, owner_->worker_start_situation_,
        owner_->resource_type_);
    phase_ = Phase::RECORDED;
  }

  // Owns and must outlive |this|.
  ServiceWorkerURLRequestJob* owner_;

  base::TimeTicks navigation_preload_response_time_;
  Phase phase_ = Phase::INITIAL;

  DISALLOW_COPY_AND_ASSIGN(NavigationPreloadMetrics);
};

bool ServiceWorkerURLRequestJob::Delegate::RequestStillValid(
    ServiceWorkerMetrics::URLRequestJobResult* result) {
  return true;
}

ServiceWorkerURLRequestJob::ServiceWorkerURLRequestJob(
    net::URLRequest* request,
    net::NetworkDelegate* network_delegate,
    const std::string& client_id,
    base::WeakPtr<storage::BlobStorageContext> blob_storage_context,
    const ResourceContext* resource_context,
    network::mojom::FetchRequestMode request_mode,
    network::mojom::FetchCredentialsMode credentials_mode,
    network::mojom::FetchRedirectMode redirect_mode,
    const std::string& integrity,
    bool keepalive,
    ResourceType resource_type,
    blink::mojom::RequestContextType request_context_type,
    network::mojom::RequestContextFrameType frame_type,
    scoped_refptr<network::ResourceRequestBody> body,
    Delegate* delegate)
    : net::URLRequestJob(request, network_delegate),
      delegate_(delegate),
      response_type_(ResponseType::NOT_DETERMINED),
      is_started_(false),
      fetch_response_type_(network::mojom::FetchResponseType::kDefault),
      client_id_(client_id),
      blob_storage_context_(blob_storage_context),
      resource_context_(resource_context),
      request_mode_(request_mode),
      credentials_mode_(credentials_mode),
      redirect_mode_(redirect_mode),
      integrity_(integrity),
      keepalive_(keepalive),
      resource_type_(resource_type),
      request_context_type_(request_context_type),
      frame_type_(frame_type),
      fall_back_required_(false),
      body_(body),
      weak_factory_(this) {
  DCHECK(delegate_) << "ServiceWorkerURLRequestJob requires a delegate";
}

ServiceWorkerURLRequestJob::~ServiceWorkerURLRequestJob() {
  data_pipe_reader_.reset();
  file_size_resolver_.reset();

  if (!ShouldRecordResult())
    return;

  ServiceWorkerMetrics::URLRequestJobResult result =
      ServiceWorkerMetrics::REQUEST_JOB_ERROR_KILLED;
  if (response_body_type_ == STREAM)
    result = ServiceWorkerMetrics::REQUEST_JOB_ERROR_KILLED_WITH_STREAM;
  else if (response_body_type_ == BLOB)
    result = ServiceWorkerMetrics::REQUEST_JOB_ERROR_KILLED_WITH_BLOB;
  RecordResult(result);
}

void ServiceWorkerURLRequestJob::FallbackToNetwork() {
  DCHECK_EQ(ResponseType::NOT_DETERMINED, response_type_);
  DCHECK(!IsFallbackToRendererNeeded());
  response_type_ = ResponseType::FALLBACK_TO_NETWORK;
  MaybeStartRequest();
}

void ServiceWorkerURLRequestJob::FallbackToNetworkOrRenderer() {
  DCHECK_EQ(ResponseType::NOT_DETERMINED, response_type_);
  if (IsFallbackToRendererNeeded()) {
    response_type_ = ResponseType::FALLBACK_TO_RENDERER;
  } else {
    response_type_ = ResponseType::FALLBACK_TO_NETWORK;
  }
  MaybeStartRequest();
}

void ServiceWorkerURLRequestJob::ForwardToServiceWorker() {
  DCHECK_EQ(ResponseType::NOT_DETERMINED, response_type_);
  response_type_ = ResponseType::FORWARD_TO_SERVICE_WORKER;
  MaybeStartRequest();
}

void ServiceWorkerURLRequestJob::FailDueToLostController() {
  DCHECK_EQ(ResponseType::NOT_DETERMINED, response_type_);
  response_type_ = ResponseType::FAIL_DUE_TO_LOST_CONTROLLER;
  MaybeStartRequest();
}

void ServiceWorkerURLRequestJob::Start() {
  is_started_ = true;
  MaybeStartRequest();
}

void ServiceWorkerURLRequestJob::Kill() {
  net::URLRequestJob::Kill();
  data_pipe_reader_.reset();
  fetch_dispatcher_.reset();
  blob_reader_.reset();
  weak_factory_.InvalidateWeakPtrs();
}

net::LoadState ServiceWorkerURLRequestJob::GetLoadState() const {
  // TODO(kinuko): refine this for better debug.
  return net::URLRequestJob::GetLoadState();
}

bool ServiceWorkerURLRequestJob::GetCharset(std::string* charset) {
  if (!http_info())
    return false;
  return http_info()->headers->GetCharset(charset);
}

bool ServiceWorkerURLRequestJob::GetMimeType(std::string* mime_type) const {
  if (!http_info())
    return false;
  return http_info()->headers->GetMimeType(mime_type);
}

void ServiceWorkerURLRequestJob::GetResponseInfo(net::HttpResponseInfo* info) {
  if (!http_info())
    return;
  const base::Time request_time = info->request_time;
  *info = *http_info();
  info->request_time = request_time;
  info->response_time = response_time_;
}

void ServiceWorkerURLRequestJob::GetLoadTimingInfo(
    net::LoadTimingInfo* load_timing_info) const {
  *load_timing_info = load_timing_info_;
}

void ServiceWorkerURLRequestJob::SetExtraRequestHeaders(
    const net::HttpRequestHeaders& headers) {
  std::string range_header;
  std::vector<net::HttpByteRange> ranges;
  if (!headers.GetHeader(net::HttpRequestHeaders::kRange, &range_header) ||
      !net::HttpUtil::ParseRangeHeader(range_header, &ranges)) {
    return;
  }

  // We don't support multiple range requests in one single URL request.
  if (ranges.size() == 1U)
    byte_range_ = ranges[0];
}

int ServiceWorkerURLRequestJob::ReadRawData(net::IOBuffer* buf, int buf_size) {
  DCHECK(buf);
  DCHECK_GE(buf_size, 0);

  if (data_pipe_reader_)
    return data_pipe_reader_->ReadRawData(buf, buf_size);
  if (blob_reader_)
    return blob_reader_->ReadRawData(buf, buf_size);

  return 0;
}

void ServiceWorkerURLRequestJob::OnResponseStarted() {
  if (response_time_.is_null())
    response_time_ = base::Time::Now();
  CommitResponseHeader();
}

void ServiceWorkerURLRequestJob::OnReadRawDataComplete(int bytes_read) {
  ReadRawDataComplete(bytes_read);
}

void ServiceWorkerURLRequestJob::RecordResult(
    ServiceWorkerMetrics::URLRequestJobResult result) {
  // It violates style guidelines to handle a NOTREACHED() failure but if there
  // is a bug don't let it corrupt UMA results by double-counting.
  if (!ShouldRecordResult()) {
    NOTREACHED();
    return;
  }
  did_record_result_ = true;
  ServiceWorkerMetrics::RecordURLRequestJobResult(IsMainResourceLoad(), result);
  request()->net_log().AddEvent(RequestJobResultToNetEventType(result));
}

base::WeakPtr<ServiceWorkerURLRequestJob>
ServiceWorkerURLRequestJob::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

const net::HttpResponseInfo* ServiceWorkerURLRequestJob::http_info() const {
  if (!http_response_info_)
    return nullptr;
  if (range_response_info_)
    return range_response_info_.get();
  return http_response_info_.get();
}

void ServiceWorkerURLRequestJob::MaybeStartRequest() {
  if (is_started_ && response_type_ != ResponseType::NOT_DETERMINED) {
    // Start asynchronously.
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(&ServiceWorkerURLRequestJob::StartRequest,
                                  weak_factory_.GetWeakPtr()));
  }
}

void ServiceWorkerURLRequestJob::StartRequest() {
  request()->net_log().AddEvent(
      net::NetLogEventType::SERVICE_WORKER_START_REQUEST);

  switch (response_type_) {
    case ResponseType::NOT_DETERMINED:
      NOTREACHED();
      return;

    case ResponseType::FAIL_DUE_TO_LOST_CONTROLLER:
      request()->net_log().AddEvent(
          net::NetLogEventType::SERVICE_WORKER_ERROR_NO_ACTIVE_VERSION);
      NotifyStartError(net::URLRequestStatus::FromError(net::ERR_FAILED));
      return;

    case ResponseType::FALLBACK_TO_NETWORK:
      FinalizeFallbackToNetwork();
      return;

    case ResponseType::FALLBACK_TO_RENDERER:
      FinalizeFallbackToRenderer();
      return;

    case ResponseType::FORWARD_TO_SERVICE_WORKER:
      if (HasRequestBody()) {
        DCHECK(!file_size_resolver_);
        file_size_resolver_.reset(new FileSizeResolver(this));
        file_size_resolver_->Resolve(base::BindOnce(
            &ServiceWorkerURLRequestJob::RequestBodyFileSizesResolved,
            GetWeakPtr()));
        return;
      }

      RequestBodyFileSizesResolved(true);
      return;
  }

  NOTREACHED();
}

std::unique_ptr<network::ResourceRequest>
ServiceWorkerURLRequestJob::CreateResourceRequest() {
  // The network::ResourceRequest will be passed to the renderer and be
  // converted to blink::WebServiceWorkerRequest in
  // ServiceWorkerContextClient::ToWebServiceWorkerRequest. So make sure the
  // fields set here are consistent with the fields read there.
  auto request = std::make_unique<network::ResourceRequest>();
  request->url = request_->url();
  request->method = request_->method();

  for (net::HttpRequestHeaders::Iterator it(request_->extra_request_headers());
       it.GetNext();) {
    request->headers.SetHeader(it.name(), it.value());
  }

  request->referrer = GURL(request_->referrer());
  request->referrer_policy = request_->referrer_policy();
  request->fetch_request_mode = request_mode_;
  request->resource_type = resource_type_;
  request->fetch_credentials_mode = credentials_mode_;
  request->load_flags = request_->load_flags();
  request->fetch_redirect_mode = redirect_mode_;
  request->fetch_request_context_type = static_cast<int>(request_context_type_);
  request->fetch_frame_type = frame_type_;
  const ResourceRequestInfo* info = ResourceRequestInfo::ForRequest(request_);
  if (info)
    request->transition_type = info->GetPageTransition();
  request->fetch_integrity = integrity_;
  request->keepalive = keepalive_;
  return request;
}

blink::mojom::BlobPtr ServiceWorkerURLRequestJob::CreateRequestBodyBlob(
    std::string* blob_uuid,
    uint64_t* blob_size) {
  DCHECK(HasRequestBody());
  auto blob_builder =
      std::make_unique<storage::BlobDataBuilder>(base::GenerateGUID());
  for (const network::DataElement& element : (*body_->elements())) {
    blob_builder->AppendIPCDataElement(element,
                                       blob_storage_context_->registry());
  }

  *blob_uuid = blob_builder->uuid();
  request_body_blob_data_handle_ =
      blob_storage_context_->AddFinishedBlob(std::move(blob_builder));
  *blob_size = request_body_blob_data_handle_->size();

  blink::mojom::BlobPtr blob_ptr;
  storage::BlobImpl::Create(std::make_unique<storage::BlobDataHandle>(
                                *request_body_blob_data_handle_),
                            MakeRequest(&blob_ptr));
  return blob_ptr;
}

bool ServiceWorkerURLRequestJob::ShouldRecordNavigationMetrics(
    const ServiceWorkerVersion* version) const {
  // Don't record navigation metrics in the following situations.
  // 1) The worker was in state INSTALLED or ACTIVATING, and the browser had to
  //    wait for it to become ACTIVATED. This is to avoid including the time to
  //    execute the activate event handlers in the worker's script.
  if (!worker_already_activated_) {
    return false;
  }
  // 2) The worker was started for the fetch AND DevTools was attached during
  //    startup. This is intended to avoid including the time for debugging.
  if (version->skip_recording_startup_time() &&
      initial_worker_status_ != EmbeddedWorkerStatus::RUNNING) {
    return false;
  }
  // 3) The request is for New Tab Page. This is because it tends to dominate
  //    the stats and makes the results largely skewed.
  if (ServiceWorkerMetrics::ShouldExcludeSiteFromHistogram(
          version->site_for_uma())) {
    return false;
  }

  return true;
}

void ServiceWorkerURLRequestJob::DidPrepareFetchEvent(
    scoped_refptr<ServiceWorkerVersion> version) {
  worker_ready_time_ = base::TimeTicks::Now();
  load_timing_info_.send_start = worker_ready_time_;
  worker_start_situation_ = version->embedded_worker()->start_situation();

  if (!ShouldRecordNavigationMetrics(version.get())) {
    nav_preload_metrics_->Abort();
    return;
  }
  if (resource_type_ == RESOURCE_TYPE_MAIN_FRAME) {
    // Record the time taken for the browser to find and possibly start an
    // active worker to which to dispatch a FetchEvent for a main frame resource
    // request. For context, a FetchEvent can only be dispatched to an ACTIVATED
    // worker that is running (it has been successfully started). The
    // measurements starts when the browser process receives the request. The
    // browser then finds the worker appropriate for this request (if there is
    // none, this metric is not recorded). If that worker is already started,
    // the browser process can send the request to it, so the measurement ends
    // quickly. Otherwise the browser process has to start the worker and the
    // measurement ends when the worker is successfully started.
    ServiceWorkerMetrics::RecordActivatedWorkerPreparationForMainFrame(
        worker_ready_time_ - request()->creation_time(), initial_worker_status_,
        worker_start_situation_, did_navigation_preload_, request_->url());
  }
  nav_preload_metrics_->ReportWorkerPreparationFinished();
}

void ServiceWorkerURLRequestJob::DidDispatchFetchEvent(
    blink::ServiceWorkerStatusCode status,
    ServiceWorkerFetchDispatcher::FetchEventResult fetch_result,
    blink::mojom::FetchAPIResponsePtr response,
    blink::mojom::ServiceWorkerStreamHandlePtr body_as_stream,
    blink::mojom::ServiceWorkerFetchEventTimingPtr timing,
    scoped_refptr<ServiceWorkerVersion> version) {
  // Do not clear |fetch_dispatcher_| if it has dispatched a navigation preload
  // request to keep the network::mojom::URLLoader related objects in it,
  // because the preload response might still need to be streamed even after
  // calling respondWith().
  if (!did_navigation_preload_) {
    fetch_dispatcher_.reset();
  }
  if (IsMainResourceLoad()) {
    ReportDestination(
        ServiceWorkerMetrics::MainResourceRequestDestination::kServiceWorker);
  }
  ServiceWorkerMetrics::RecordFetchEventStatus(IsMainResourceLoad(), status);

  ServiceWorkerMetrics::URLRequestJobResult result =
      ServiceWorkerMetrics::REQUEST_JOB_ERROR_BAD_DELEGATE;
  if (!delegate_->RequestStillValid(&result)) {
    RecordResult(result);
    DeliverErrorResponse();
    return;
  }

  if (status != blink::ServiceWorkerStatusCode::kOk) {
    RecordResult(ServiceWorkerMetrics::REQUEST_JOB_ERROR_FETCH_EVENT_DISPATCH);
    if (IsMainResourceLoad()) {
      // Using the service worker failed, so fallback to network.
      delegate_->MainResourceLoadFailed();
      FinalizeFallbackToNetwork();
    } else {
      DeliverErrorResponse();
    }
    return;
  }

  if (fetch_result ==
      ServiceWorkerFetchDispatcher::FetchEventResult::kShouldFallback) {
    ServiceWorkerMetrics::RecordFallbackedRequestMode(request_mode_);
    if (IsFallbackToRendererNeeded()) {
      FinalizeFallbackToRenderer();
    } else {
      FinalizeFallbackToNetwork();
    }
    return;
  }

  // We should have a response now.
  DCHECK_EQ(ServiceWorkerFetchDispatcher::FetchEventResult::kGotResponse,
            fetch_result);

  // A response with status code 0 is Blink telling us to respond with network
  // error.
  if (response->status_code == 0) {
    RecordStatusZeroResponseError(response->error);
    NotifyStartError(ServiceWorkerResponseErrorToNetStatus(response->error));
    return;
  }

  load_timing_info_.send_end = base::TimeTicks::Now();

  // Creates a new HttpResponseInfo using the the ServiceWorker script's
  // HttpResponseInfo to show HTTPS padlock.
  // TODO(horo): When we support mixed-content (HTTP) no-cors requests from a
  // ServiceWorker, we have to check the security level of the responses.
  DCHECK(!http_response_info_);
  DCHECK(version);
  const net::HttpResponseInfo* main_script_http_info =
      version->GetMainScriptHttpResponseInfo();
  DCHECK(main_script_http_info);
  http_response_info_.reset(new net::HttpResponseInfo(*main_script_http_info));

  // Process stream using Mojo's data pipe.
  if (!body_as_stream.is_null()) {
    SetResponseBodyType(STREAM);
    SetResponse(std::move(response));
    data_pipe_reader_.reset(new ServiceWorkerDataPipeReader(
        this, version, std::move(body_as_stream)));
    data_pipe_reader_->Start();
    return;
  }

  // Set up a request for reading the blob.
  // TODO(falken): Can we just read |response->blob->blob| directly like in
  // ServiceWorkerNavigationLoader?
  if (response->blob && blob_storage_context_) {
    DCHECK(!response->blob->uuid.empty());
    DCHECK(response->blob->blob.is_valid());
    SetResponseBodyType(BLOB);
    std::unique_ptr<storage::BlobDataHandle> blob_data_handle =
        blob_storage_context_->GetBlobDataFromUUID(response->blob->uuid);
    if (!blob_data_handle) {
      // The renderer gave us a bad blob UUID.
      RecordResult(ServiceWorkerMetrics::REQUEST_JOB_ERROR_BAD_BLOB);
      DeliverErrorResponse();
      return;
    }
    blob_reader_.reset(new ServiceWorkerBlobReader(this));
    blob_reader_->Start(std::move(blob_data_handle), request()->context());
  }

  SetResponse(std::move(response));
  if (!blob_reader_) {
    RecordResult(ServiceWorkerMetrics::REQUEST_JOB_HEADERS_ONLY_RESPONSE);
    CommitResponseHeader();
  }
}

void ServiceWorkerURLRequestJob::SetResponse(
    blink::mojom::FetchAPIResponsePtr response) {
  response_url_list_ = std::move(response->url_list);
  fetch_response_type_ = response->response_type;
  cors_exposed_header_names_ = std::move(response->cors_exposed_header_names);
  response_time_ = response->response_time;
  CreateResponseHeader(response->status_code, response->status_text,
                       std::move(response->headers));
  load_timing_info_.receive_headers_end = base::TimeTicks::Now();

  response_is_in_cache_storage_ = response->is_in_cache_storage;
  if (response->cache_storage_cache_name) {
    response_cache_storage_cache_name_ =
        std::move(*(response->cache_storage_cache_name));
  } else {
    response_cache_storage_cache_name_.clear();
  }
}

void ServiceWorkerURLRequestJob::CreateResponseHeader(
    int status_code,
    const std::string& status_text,
    ResponseHeaderMap headers) {
  // Build a string instead of using HttpResponseHeaders::AddHeader on
  // each header, since AddHeader has O(n^2) performance.
  std::string buf(base::StringPrintf("HTTP/1.1 %d %s\r\n", status_code,
                                     status_text.c_str()));
  for (auto& item : headers) {
    buf.append(std::move(item.first));
    buf.append(": ");
    buf.append(std::move(item.second));
    buf.append("\r\n");
  }
  buf.append("\r\n");
  http_response_headers_ = new net::HttpResponseHeaders(
      net::HttpUtil::AssembleRawHeaders(buf.c_str(), buf.size()));
}

void ServiceWorkerURLRequestJob::CommitResponseHeader() {
  if (!http_response_info_)
    http_response_info_.reset(new net::HttpResponseInfo());
  http_response_info_->headers.swap(http_response_headers_);
  http_response_info_->vary_data = net::HttpVaryData();
  http_response_info_->metadata =
      blob_reader_ ? blob_reader_->response_metadata() : nullptr;
  NotifyHeadersComplete();
}

void ServiceWorkerURLRequestJob::DeliverErrorResponse() {
  // TODO(falken): Print an error to the console of the ServiceWorker and of
  // the requesting page.
  CreateResponseHeader(500, "Service Worker Response Error",
                       ResponseHeaderMap());
  CommitResponseHeader();
}

void ServiceWorkerURLRequestJob::FinalizeFallbackToNetwork() {
  // Restart this request to create a new job. The default job (which will hit
  // network) will be created in the next time because our request handler will
  // return nullptr after restarting and this means our interceptor does not
  // intercept.
  if (ShouldRecordResult())
    RecordResult(ServiceWorkerMetrics::REQUEST_JOB_FALLBACK_RESPONSE);
  response_type_ = ResponseType::FALLBACK_TO_NETWORK;
  NotifyRestartRequired();
  return;
}

void ServiceWorkerURLRequestJob::FinalizeFallbackToRenderer() {
  fall_back_required_ = true;
  if (ShouldRecordResult())
    RecordResult(ServiceWorkerMetrics::REQUEST_JOB_FALLBACK_FOR_CORS);
  CreateResponseHeader(400, "Service Worker Fallback Required",
                       ResponseHeaderMap());
  response_type_ = ResponseType::FALLBACK_TO_RENDERER;
  CommitResponseHeader();
}

bool ServiceWorkerURLRequestJob::IsFallbackToRendererNeeded() const {
  // When the request_mode is |CORS| or |CORS-with-forced-preflight| and the
  // origin of the request URL is different from the security origin of the
  // document, we can't simply fallback to the network in the browser process.
  // It is because the CORS preflight logic is implemented in the renderer. So
  // we return a fall_back_required response to the renderer.
  return !IsMainResourceLoad() &&
         (request_mode_ == network::mojom::FetchRequestMode::kCORS ||
          request_mode_ ==
              network::mojom::FetchRequestMode::kCORSWithForcedPreflight) &&
         (!request()->initiator().has_value() ||
          !request()->initiator()->IsSameOriginWith(
              url::Origin::Create(request()->url())));
}

void ServiceWorkerURLRequestJob::SetResponseBodyType(ResponseBodyType type) {
  DCHECK_EQ(response_body_type_, UNKNOWN);
  DCHECK_NE(type, UNKNOWN);
  response_body_type_ = type;
}

bool ServiceWorkerURLRequestJob::ShouldRecordResult() {
  return !did_record_result_ && is_started_ &&
         response_type_ == ResponseType::FORWARD_TO_SERVICE_WORKER;
}

void ServiceWorkerURLRequestJob::RecordStatusZeroResponseError(
    blink::mojom::ServiceWorkerResponseError error) {
  // It violates style guidelines to handle a NOTREACHED() failure but if there
  // is a bug don't let it corrupt UMA results by double-counting.
  if (!ShouldRecordResult()) {
    NOTREACHED();
    return;
  }
  RecordResult(ServiceWorkerMetrics::REQUEST_JOB_ERROR_RESPONSE_STATUS_ZERO);
  ServiceWorkerMetrics::RecordStatusZeroResponseError(IsMainResourceLoad(),
                                                      error);
}

void ServiceWorkerURLRequestJob::NotifyHeadersComplete() {
  OnStartCompleted();
  URLRequestJob::NotifyHeadersComplete();
}

void ServiceWorkerURLRequestJob::NotifyStartError(
    net::URLRequestStatus status) {
  OnStartCompleted();
  URLRequestJob::NotifyStartError(status);
}

void ServiceWorkerURLRequestJob::NotifyRestartRequired() {
  ServiceWorkerResponseInfo::ForRequest(request_, true)
      ->OnPrepareToRestart(worker_start_time_, worker_ready_time_,
                           did_navigation_preload_);
  delegate_->OnPrepareToRestart();
  URLRequestJob::NotifyRestartRequired();
}

void ServiceWorkerURLRequestJob::OnStartCompleted() const {
  switch (response_type_) {
    case ResponseType::NOT_DETERMINED:
      NOTREACHED();
      return;
    case ResponseType::FAIL_DUE_TO_LOST_CONTROLLER:
    case ResponseType::FALLBACK_TO_NETWORK:
      // Indicate that the service worker did not respond to the request.
      ServiceWorkerResponseInfo::ForRequest(request_, true)
          ->OnStartCompleted(
              false /* was_fetched_via_service_worker */,
              false /* was_fallback_required */,
              std::vector<GURL>() /* url_list_via_service_worker */,
              network::mojom::FetchResponseType::kDefault,
              base::TimeTicks() /* service_worker_start_time */,
              base::TimeTicks() /* service_worker_ready_time */,
              false /* response_is_in_cache_storage */,
              std::string() /* response_cache_storage_cache_name */,
              ServiceWorkerHeaderList() /* cors_exposed_header_names */,
              did_navigation_preload_);
      break;
    case ResponseType::FALLBACK_TO_RENDERER:
    case ResponseType::FORWARD_TO_SERVICE_WORKER:
      // Indicate that the service worker responded to the request, which is
      // considered true if "fallback to renderer" was required since the
      // renderer expects that.
      ServiceWorkerResponseInfo::ForRequest(request_, true)
          ->OnStartCompleted(
              true /* was_fetched_via_service_worker */,
              fall_back_required_, response_url_list_, fetch_response_type_,
              worker_start_time_, worker_ready_time_,
              response_is_in_cache_storage_, response_cache_storage_cache_name_,
              cors_exposed_header_names_, did_navigation_preload_);
      break;
  }
}

bool ServiceWorkerURLRequestJob::IsMainResourceLoad() const {
  return ServiceWorkerUtils::IsMainResourceType(resource_type_);
}

bool ServiceWorkerURLRequestJob::HasRequestBody() {
  // URLRequest::has_upload() must be checked since its upload data may have
  // been cleared while handling a redirect.
  return request_->has_upload() && body_.get() && blob_storage_context_;
}

void ServiceWorkerURLRequestJob::RequestBodyFileSizesResolved(bool success) {
  file_size_resolver_.reset();
  if (!success) {
    RecordResult(
        ServiceWorkerMetrics::REQUEST_JOB_ERROR_REQUEST_BODY_BLOB_FAILED);
    if (IsMainResourceLoad()) {
      ReportDestination(ServiceWorkerMetrics::MainResourceRequestDestination::
                            kErrorRequestBodyFailed);
    }

    // TODO(falken): This and below should probably be NotifyStartError, not
    // DeliverErrorResponse. But changing it causes
    // ServiceWorkerURLRequestJobTest.DeletedProviderHostBeforeFetchEvent to
    // fail.
    DeliverErrorResponse();
    return;
  }

  ServiceWorkerMetrics::URLRequestJobResult result =
      ServiceWorkerMetrics::REQUEST_JOB_ERROR_BAD_DELEGATE;
  ServiceWorkerVersion* active_worker =
      delegate_->GetServiceWorkerVersion(&result);
  if (!active_worker) {
    RecordResult(result);
    if (IsMainResourceLoad()) {
      ReportDestination(ServiceWorkerMetrics::MainResourceRequestDestination::
                            kErrorNoActiveWorkerFromDelegate);
    }
    DeliverErrorResponse();
    return;
  }

  worker_already_activated_ =
      active_worker->status() == ServiceWorkerVersion::ACTIVATED;
  initial_worker_status_ = active_worker->running_status();

  std::unique_ptr<network::ResourceRequest> resource_request =
      CreateResourceRequest();
  std::string blob_uuid;
  uint64_t blob_size = 0;
  blink::mojom::BlobPtr blob;
  if (HasRequestBody()) {
    // TODO(falken): Could we just set |resource_request->request_body| to
    // |body_| directly, and not construct a new blob? But I think the
    // renderer-side might need to know the size of the body.
    blob = CreateRequestBodyBlob(&blob_uuid, &blob_size);
  }

  DCHECK(!fetch_dispatcher_);
  if (IsMainResourceLoad())
    delegate_->WillDispatchFetchEventForMainResource();
  fetch_dispatcher_ = std::make_unique<ServiceWorkerFetchDispatcher>(
      std::move(resource_request), blob_uuid, blob_size, std::move(blob),
      client_id_, base::WrapRefCounted(active_worker), request()->net_log(),
      base::BindOnce(&ServiceWorkerURLRequestJob::DidPrepareFetchEvent,
                     weak_factory_.GetWeakPtr(),
                     base::WrapRefCounted(active_worker)),
      base::BindOnce(&ServiceWorkerURLRequestJob::DidDispatchFetchEvent,
                     weak_factory_.GetWeakPtr()));
  worker_start_time_ = base::TimeTicks::Now();
  nav_preload_metrics_ = std::make_unique<NavigationPreloadMetrics>(this);
  if (simulate_navigation_preload_for_test_) {
    did_navigation_preload_ = true;
  } else {
    did_navigation_preload_ = fetch_dispatcher_->MaybeStartNavigationPreload(
        request(),
        base::BindOnce(&ServiceWorkerURLRequestJob::OnNavigationPreloadResponse,
                       weak_factory_.GetWeakPtr()));
  }
  fetch_dispatcher_->Run();
}

void ServiceWorkerURLRequestJob::OnNavigationPreloadResponse() {
  nav_preload_metrics_->ReportNavigationPreloadFinished();
}

void ServiceWorkerURLRequestJob::ReportDestination(
    ServiceWorkerMetrics::MainResourceRequestDestination destination) {
  DCHECK(IsMainResourceLoad());
  delegate_->ReportDestination(destination);
}

}  // namespace content
