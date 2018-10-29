// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/mojo_async_resource_handler.h"

#include <algorithm>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "content/browser/loader/resource_controller.h"
#include "content/browser/loader/resource_dispatcher_host_impl.h"
#include "content/browser/loader/resource_request_info_impl.h"
#include "content/public/browser/global_request_id.h"
#include "mojo/public/c/system/data_pipe.h"
#include "mojo/public/cpp/bindings/message.h"
#include "net/base/mime_sniffer.h"
#include "net/base/net_errors.h"
#include "net/url_request/redirect_info.h"
#include "services/network/public/cpp/resource_response.h"
#include "services/network/public/cpp/url_loader_completion_status.h"
#include "services/network/resource_scheduler.h"

namespace content {
namespace {

int g_allocation_size = MojoAsyncResourceHandler::kDefaultAllocationSize;

// MimeTypeResourceHandler *implicitly* requires that the buffer size
// returned from OnWillRead should be larger than certain size.
// TODO(yhirano): Fix MimeTypeResourceHandler.
constexpr size_t kMinAllocationSize = 2 * net::kMaxBytesToSniff;

constexpr size_t kMaxChunkSize = 32 * 1024;

// Time between sending the transfer size updates to renderer. This threshold is
// chosen as a compromise between sending too frequent updates and the limit its
// consumers (DevTools and page load metrics) expect.
constexpr base::TimeDelta kTransferSizeReportInterval =
    base::TimeDelta::FromMilliseconds(500);

bool ShouldReportTransferSize(
    const ResourceRequestInfoImpl* resource_request_info) {
  // Transfer size is reported only when report_raw_headers is set or the
  // renderer is allowed to receive the resource response metadata (e.g. by
  // Cross-Origin Read Blocking).
  return resource_request_info->ShouldReportRawHeaders() ||
         !resource_request_info->blocked_response_from_reaching_renderer();
}

}  // namespace

// This class is for sharing the ownership of a ScopedDataPipeProducerHandle
// between WriterIOBuffer and MojoAsyncResourceHandler.
class MojoAsyncResourceHandler::SharedWriter final
    : public base::RefCountedThreadSafe<SharedWriter> {
 public:
  explicit SharedWriter(mojo::ScopedDataPipeProducerHandle writer)
      : writer_(std::move(writer)) {}
  mojo::DataPipeProducerHandle writer() { return writer_.get(); }

 private:
  friend class base::RefCountedThreadSafe<SharedWriter>;
  ~SharedWriter() {}

  const mojo::ScopedDataPipeProducerHandle writer_;

  DISALLOW_COPY_AND_ASSIGN(SharedWriter);
};

// This class is a IOBuffer subclass for data gotten from a
// ScopedDataPipeProducerHandle.
class MojoAsyncResourceHandler::WriterIOBuffer final
    : public net::IOBufferWithSize {
 public:
  // |data| and |size| should be gotten from |writer| via BeginWriteData.
  // They will be accesible via IOBuffer methods. As |writer| is stored in this
  // instance, |data| will be kept valid as long as the following conditions
  // hold:
  //  1. |data| is not invalidated via EndWriteDataRaw.
  //  2. |this| instance is alive.
  WriterIOBuffer(scoped_refptr<SharedWriter> writer, void* data, size_t size)
      : net::IOBufferWithSize(static_cast<char*>(data), size),
        writer_(std::move(writer)) {}

 private:
  ~WriterIOBuffer() override {
    // Avoid deleting |data_| in the IOBuffer destructor.
    data_ = nullptr;
  }

  // This member is for keeping the writer alive.
  scoped_refptr<SharedWriter> writer_;

  DISALLOW_COPY_AND_ASSIGN(WriterIOBuffer);
};

MojoAsyncResourceHandler::MojoAsyncResourceHandler(
    net::URLRequest* request,
    ResourceDispatcherHostImpl* rdh,
    network::mojom::URLLoaderRequest mojo_request,
    network::mojom::URLLoaderClientPtr url_loader_client,
    ResourceType resource_type,
    uint32_t url_loader_options)
    : ResourceHandler(request),
      rdh_(rdh),
      binding_(this, std::move(mojo_request)),
      url_loader_options_(url_loader_options),
      handle_watcher_(FROM_HERE,
                      mojo::SimpleWatcher::ArmingPolicy::MANUAL,
                      base::SequencedTaskRunnerHandle::Get()),
      url_loader_client_(std::move(url_loader_client)),
      report_transfer_size_async_timer_(std::make_unique<base::OneShotTimer>()),
      weak_factory_(this) {
  DCHECK(IsResourceTypeFrame(resource_type) ||
         resource_type == RESOURCE_TYPE_SERVICE_WORKER ||
         !(url_loader_options_ &
           network::mojom::kURLLoadOptionSendSSLInfoWithResponse));
  DCHECK(resource_type == RESOURCE_TYPE_MAIN_FRAME ||
         !(url_loader_options_ &
           network::mojom::kURLLoadOptionSendSSLInfoForCertificateError));
  DCHECK(url_loader_client_);
  InitializeResourceBufferConstants();
  // This unretained pointer is safe, because |binding_| is owned by |this| and
  // the callback will never be called after |this| is destroyed.
  binding_.set_connection_error_with_reason_handler(base::BindOnce(
      &MojoAsyncResourceHandler::Cancel, base::Unretained(this)));
}

MojoAsyncResourceHandler::~MojoAsyncResourceHandler() {
  if (has_checked_for_sufficient_resources_)
    rdh_->FinishedWithResourcesForRequest(request());
}

void MojoAsyncResourceHandler::InitializeResourceBufferConstants() {
  static bool did_init = false;
  if (did_init)
    return;
  did_init = true;

  GetNumericArg("resource-buffer-size", &g_allocation_size);
}

void MojoAsyncResourceHandler::OnRequestRedirected(
    const net::RedirectInfo& redirect_info,
    network::ResourceResponse* response,
    std::unique_ptr<ResourceController> controller) {
  // Unlike OnResponseStarted, OnRequestRedirected will NOT be preceded by
  // OnWillRead.
  DCHECK(!has_controller());
  DCHECK(!shared_writer_);

  request()->LogBlockedBy("MojoAsyncResourceHandler");
  HoldController(std::move(controller));
  did_defer_on_redirect_ = true;

  response->head.encoded_data_length = request()->GetTotalReceivedBytes();
  response->head.request_start = request()->creation_time();
  response->head.response_start = base::TimeTicks::Now();
  // TODO(davidben): Is it necessary to pass the new first party URL for
  // cookies? The only case where it can change is top-level navigation requests
  // and hopefully those will eventually all be owned by the browser. It's
  // possible this is still needed while renderer-owned ones exist.
  url_loader_client_->OnReceiveRedirect(redirect_info, response->head);
}

void MojoAsyncResourceHandler::OnResponseStarted(
    network::ResourceResponse* response,
    std::unique_ptr<ResourceController> controller) {
  DCHECK(!has_controller());

  if (upload_progress_tracker_) {
    upload_progress_tracker_->OnUploadCompleted();
    upload_progress_tracker_ = nullptr;
  }

  response->head.encoded_data_length = request()->raw_header_size();
  reported_total_received_bytes_ = response->head.encoded_data_length;

  response->head.request_start = request()->creation_time();
  response->head.response_start = base::TimeTicks::Now();
  sent_received_response_message_ = true;
  response->head.was_fetched_via_cache = request()->was_cached();

  if ((url_loader_options_ &
       network::mojom::kURLLoadOptionSendSSLInfoWithResponse) &&
      request()->ssl_info().cert) {
    response->head.ssl_info = request()->ssl_info();
  }

  url_loader_client_->OnReceiveResponse(response->head);

  net::IOBufferWithSize* metadata = GetResponseMetadata(request());
  if (metadata) {
    const uint8_t* data = reinterpret_cast<const uint8_t*>(metadata->data());

    url_loader_client_->OnReceiveCachedMetadata(
        std::vector<uint8_t>(data, data + metadata->size()));
  }

  if (url_loader_options_ &
      network::mojom::kURLLoadOptionPauseOnResponseStarted) {
    did_defer_on_response_started_ = true;
    DCHECK(!has_controller());
    request()->LogBlockedBy("MojoAsyncResourceHandler");
    HoldController(std::move(controller));
    return;
  }

  controller->Resume();
}

void MojoAsyncResourceHandler::OnWillStart(
    const GURL& url,
    std::unique_ptr<ResourceController> controller) {
  if (GetRequestInfo()->is_upload_progress_enabled() &&
      request()->has_upload()) {
    upload_progress_tracker_ = CreateUploadProgressTracker(
        FROM_HERE,
        base::BindRepeating(&MojoAsyncResourceHandler::SendUploadProgress,
                            base::Unretained(this)));
  }

  controller->Resume();
}

void MojoAsyncResourceHandler::OnWillRead(
    scoped_refptr<net::IOBuffer>* buf,
    int* buf_size,
    std::unique_ptr<ResourceController> controller) {
  // |buffer_| is set to nullptr on successful read completion (Except for the
  // final 0-byte read, so this DCHECK will also catch OnWillRead being called
  // after OnReadCompelted(0)).
  DCHECK(!buffer_);
  DCHECK_EQ(0u, buffer_offset_);

  if (!CheckForSufficientResource()) {
    controller->CancelWithError(net::ERR_INSUFFICIENT_RESOURCES);
    return;
  }

  bool first_call = false;
  if (!shared_writer_) {
    first_call = true;
    MojoCreateDataPipeOptions options;
    options.struct_size = sizeof(MojoCreateDataPipeOptions);
    options.flags = MOJO_CREATE_DATA_PIPE_FLAG_NONE;
    options.element_num_bytes = 1;
    options.capacity_num_bytes = g_allocation_size;
    mojo::ScopedDataPipeProducerHandle producer;
    mojo::ScopedDataPipeConsumerHandle consumer;

    MojoResult result = mojo::CreateDataPipe(&options, &producer, &consumer);
    if (result != MOJO_RESULT_OK) {
      controller->CancelWithError(net::ERR_INSUFFICIENT_RESOURCES);
      return;
    }
    DCHECK(producer.is_valid());
    DCHECK(consumer.is_valid());

    response_body_consumer_handle_ = std::move(consumer);
    shared_writer_ = new SharedWriter(std::move(producer));
    handle_watcher_.Watch(shared_writer_->writer(), MOJO_HANDLE_SIGNAL_WRITABLE,
                          base::Bind(&MojoAsyncResourceHandler::OnWritable,
                                     base::Unretained(this)));
    handle_watcher_.ArmOrNotify();
  }

  bool defer = false;
  if (!AllocateWriterIOBuffer(&buffer_, &defer)) {
    controller->CancelWithError(net::ERR_INSUFFICIENT_RESOURCES);
    return;
  }

  if (defer) {
    DCHECK(!buffer_);
    parent_buffer_ = buf;
    parent_buffer_size_ = buf_size;
    HoldController(std::move(controller));
    request()->LogBlockedBy("MojoAsyncResourceHandler");
    did_defer_on_will_read_ = true;
    return;
  }

  // The first call to OnWillRead must return a buffer of at least
  // kMinAllocationSize. If the Mojo buffer is too small, need to allocate an
  // intermediary buffer.
  if (first_call && static_cast<size_t>(buffer_->size()) < kMinAllocationSize) {
    // The allocated buffer is too small, so need to create an intermediary one.
    if (EndWrite(0) != MOJO_RESULT_OK) {
      controller->CancelWithError(net::ERR_INSUFFICIENT_RESOURCES);
      return;
    }
    DCHECK(!is_using_io_buffer_not_from_writer_);
    is_using_io_buffer_not_from_writer_ = true;
    buffer_ = base::MakeRefCounted<net::IOBufferWithSize>(kMinAllocationSize);
  }

  *buf = buffer_;
  *buf_size = buffer_->size();
  controller->Resume();
}

void MojoAsyncResourceHandler::set_report_transfer_size_async_timer_for_testing(
    std::unique_ptr<base::OneShotTimer> timer) {
  report_transfer_size_async_timer_ = std::move(timer);
}

void MojoAsyncResourceHandler::SendTransferSizeUpdate() {
  int64_t transfer_size_diff = CalculateRecentlyReceivedBytes();
  if (transfer_size_diff > 0) {
    url_loader_client_->OnTransferSizeUpdated(transfer_size_diff);
  }
}

void MojoAsyncResourceHandler::EnsureTransferSizeUpdate() {
  auto current_time = base::TimeTicks::Now();
  if (earliest_time_next_transfer_size_report_.is_null() ||
      earliest_time_next_transfer_size_report_ <= current_time) {
    report_transfer_size_async_timer_->Stop();
    SendTransferSizeUpdate();
    earliest_time_next_transfer_size_report_ =
        current_time + kTransferSizeReportInterval;
  } else {
    // Ensure that a single transfer update will eventually occur even if reads
    // stop. Unretained is safe here because the callback will only live as long
    // as |report_transfer_size_async_timer_|.
    report_transfer_size_async_timer_->Start(
        FROM_HERE, kTransferSizeReportInterval,
        base::BindOnce(&MojoAsyncResourceHandler::SendTransferSizeUpdate,
                       base::Unretained(this)));
  }
}

void MojoAsyncResourceHandler::OnReadCompleted(
    int bytes_read,
    std::unique_ptr<ResourceController> controller) {
  DCHECK(!has_controller());
  DCHECK_GE(bytes_read, 0);
  DCHECK(buffer_);

  if (bytes_read == 0) {
    // Note that |buffer_| is not cleared here, which will cause a DCHECK on
    // subsequent OnWillRead calls.
    controller->Resume();
    return;
  }

  if (ShouldReportTransferSize(GetRequestInfo())) {
    EnsureTransferSizeUpdate();
  }

  if (response_body_consumer_handle_.is_valid()) {
    // Send the data pipe on the first OnReadCompleted call.
    url_loader_client_->OnStartLoadingResponseBody(
        std::move(response_body_consumer_handle_));
    response_body_consumer_handle_.reset();
  }

  if (is_using_io_buffer_not_from_writer_) {
    // Couldn't allocate a large enough buffer on the data pipe in OnWillRead.
    DCHECK_EQ(0u, buffer_bytes_read_);
    buffer_bytes_read_ = bytes_read;
    bool defer = false;
    if (!CopyReadDataToDataPipe(&defer)) {
      controller->CancelWithError(net::ERR_INSUFFICIENT_RESOURCES);
      return;
    }
    if (defer) {
      request()->LogBlockedBy("MojoAsyncResourceHandler");
      did_defer_on_writing_ = true;
      HoldController(std::move(controller));
      return;
    }
    controller->Resume();
    return;
  }

  if (EndWrite(bytes_read) != MOJO_RESULT_OK) {
    controller->Cancel();
    return;
  }

  buffer_ = nullptr;
  controller->Resume();
}

void MojoAsyncResourceHandler::FollowRedirect(
    const base::Optional<std::vector<std::string>>&
        to_be_removed_request_headers,
    const base::Optional<net::HttpRequestHeaders>& modified_request_headers) {
  if (!request()->status().is_success()) {
    DVLOG(1) << "FollowRedirect for invalid request";
    return;
  }
  if (!did_defer_on_redirect_) {
    DVLOG(1) << "Malformed FollowRedirect request";
    ReportBadMessage("Malformed FollowRedirect request");
    return;
  }

  DCHECK(!did_defer_on_will_read_);
  DCHECK(!did_defer_on_writing_);
  did_defer_on_redirect_ = false;
  request()->LogUnblocked();
  ResumeForRedirect(modified_request_headers);
}

void MojoAsyncResourceHandler::ProceedWithResponse() {
  DCHECK(did_defer_on_response_started_);

  request()->LogUnblocked();
  Resume();
}

void MojoAsyncResourceHandler::SetPriority(net::RequestPriority priority,
                                           int32_t intra_priority_value) {
  auto* scheduler = ResourceDispatcherHostImpl::Get()->scheduler();
  if (intra_priority_value == -1) {
    scheduler->ReprioritizeRequest(request(), priority);
  } else {
    scheduler->ReprioritizeRequest(request(), priority, intra_priority_value);
  }
}

void MojoAsyncResourceHandler::PauseReadingBodyFromNet() {
  ResourceHandler::PauseReadingBodyFromNet();
}

void MojoAsyncResourceHandler::ResumeReadingBodyFromNet() {
  ResourceHandler::ResumeReadingBodyFromNet();
}

void MojoAsyncResourceHandler::OnWritableForTesting() {
  OnWritable(MOJO_RESULT_OK);
}

void MojoAsyncResourceHandler::SetAllocationSizeForTesting(size_t size) {
  g_allocation_size = size;
}

MojoResult MojoAsyncResourceHandler::BeginWrite(void** data,
                                                uint32_t* available) {
  MojoResult result = shared_writer_->writer().BeginWriteData(
      data, available, MOJO_WRITE_DATA_FLAG_NONE);
  if (result == MOJO_RESULT_OK)
    *available = std::min(*available, static_cast<uint32_t>(kMaxChunkSize));
  else if (result == MOJO_RESULT_SHOULD_WAIT)
    handle_watcher_.ArmOrNotify();
  return result;
}

MojoResult MojoAsyncResourceHandler::EndWrite(uint32_t written) {
  MojoResult result = shared_writer_->writer().EndWriteData(written);
  if (result == MOJO_RESULT_OK) {
    total_written_bytes_ += written;
    handle_watcher_.ArmOrNotify();
  }
  return result;
}

net::IOBufferWithSize* MojoAsyncResourceHandler::GetResponseMetadata(
    net::URLRequest* request) {
  return request->response_info().metadata.get();
}

void MojoAsyncResourceHandler::OnResponseCompleted(
    const net::URLRequestStatus& request_status,
    std::unique_ptr<ResourceController> controller) {
  // Ensure sending the final upload progress message here, since
  // OnResponseCompleted can be called without OnResponseStarted on cancellation
  // or error cases.
  if (upload_progress_tracker_) {
    upload_progress_tracker_->OnUploadCompleted();
    upload_progress_tracker_ = nullptr;
  }

  shared_writer_ = nullptr;
  buffer_ = nullptr;
  handle_watcher_.Cancel();

  // TODO(gavinp): Remove this CHECK when we figure out the cause of
  // http://crbug.com/124680 . This check mirrors closely check in
  // WebURLLoaderImpl::OnCompletedRequest that routes this message to a WebCore
  // ResourceHandleInternal which asserts on its state and crashes. By crashing
  // when the message is sent, we should get better crash reports.
  CHECK(request_status.status() != net::URLRequestStatus::SUCCESS ||
        sent_received_response_message_);

  int error_code = request_status.error();

  DCHECK_NE(request_status.status(), net::URLRequestStatus::IO_PENDING);

  network::URLLoaderCompletionStatus loader_status;
  loader_status.error_code = error_code;
  if (error_code == net::ERR_QUIC_PROTOCOL_ERROR) {
    net::NetErrorDetails details;
    request()->PopulateNetErrorDetails(&details);
    loader_status.extended_error_code = details.quic_connection_error;
  } else if (error_code == net::ERR_BLOCKED_BY_CLIENT ||
             error_code == net::ERR_BLOCKED_BY_RESPONSE) {
    ResourceRequestInfoImpl* resource_request_info =
        ResourceRequestInfoImpl::ForRequest(request());
    auto maybe_reason =
        resource_request_info->GetResourceRequestBlockedReason();
    // Ideally, every blocked by client / blocked by response error
    // would be annotated with a blocked reason, but we can't guarantee it
    // here, so sometimes we won't populate extended_error_code which
    // corresonds ResourceRequestBlockedReason::kOther.
    if (maybe_reason) {
      loader_status.extended_error_code =
          static_cast<int>(maybe_reason.value());
    }
  }
  loader_status.exists_in_cache = request()->response_info().was_cached;
  loader_status.completion_time = base::TimeTicks::Now();
  loader_status.encoded_data_length = request()->GetTotalReceivedBytes();
  loader_status.encoded_body_length = request()->GetRawBodyBytes();
  loader_status.decoded_body_length = total_written_bytes_;
  loader_status.should_report_corb_blocking =
      GetRequestInfo()->should_report_corb_blocking();

  if ((url_loader_options_ &
       network::mojom::kURLLoadOptionSendSSLInfoForCertificateError) &&
      net::IsCertStatusError(request()->ssl_info().cert_status) &&
      !net::IsCertStatusMinorError(request()->ssl_info().cert_status)) {
    loader_status.ssl_info = request()->ssl_info();
  }

  if (ShouldReportTransferSize(GetRequestInfo())) {
    // All received bytes will be reported.
    report_transfer_size_async_timer_->Stop();
    SendTransferSizeUpdate();
  }

  url_loader_client_->OnComplete(loader_status);
  controller->Resume();
}

bool MojoAsyncResourceHandler::CopyReadDataToDataPipe(bool* defer) {
  while (buffer_bytes_read_ > 0) {
    scoped_refptr<net::IOBufferWithSize> dest;
    if (!AllocateWriterIOBuffer(&dest, defer))
      return false;
    if (*defer)
      return true;

    size_t copied_size =
        std::min(buffer_bytes_read_, static_cast<size_t>(dest->size()));
    memcpy(dest->data(), buffer_->data() + buffer_offset_, copied_size);
    buffer_offset_ += copied_size;
    buffer_bytes_read_ -= copied_size;
    if (EndWrite(copied_size) != MOJO_RESULT_OK)
      return false;
  }

  // All bytes are copied.
  buffer_ = nullptr;
  buffer_offset_ = 0;
  is_using_io_buffer_not_from_writer_ = false;
  return true;
}

bool MojoAsyncResourceHandler::AllocateWriterIOBuffer(
    scoped_refptr<net::IOBufferWithSize>* buf,
    bool* defer) {
  void* data = nullptr;
  uint32_t available = 0;
  MojoResult result = BeginWrite(&data, &available);
  if (result == MOJO_RESULT_SHOULD_WAIT) {
    *defer = true;
    return true;
  }
  if (result != MOJO_RESULT_OK)
    return false;
  DCHECK_GT(available, 0u);
  *buf = new WriterIOBuffer(shared_writer_, data, available);
  return true;
}

bool MojoAsyncResourceHandler::CheckForSufficientResource() {
  if (has_checked_for_sufficient_resources_)
    return true;
  has_checked_for_sufficient_resources_ = true;

  if (rdh_->HasSufficientResourcesForRequest(request()))
    return true;

  return false;
}

void MojoAsyncResourceHandler::OnWritable(MojoResult result) {
  if (did_defer_on_will_read_) {
    DCHECK(has_controller());
    DCHECK(!did_defer_on_writing_);
    DCHECK(!did_defer_on_redirect_);

    did_defer_on_will_read_ = false;

    scoped_refptr<net::IOBuffer>* parent_buffer = parent_buffer_;
    parent_buffer_ = nullptr;
    int* parent_buffer_size = parent_buffer_size_;
    parent_buffer_size_ = nullptr;

    request()->LogUnblocked();
    OnWillRead(parent_buffer, parent_buffer_size, ReleaseController());
    return;
  }

  if (!did_defer_on_writing_)
    return;
  DCHECK(has_controller());
  DCHECK(!did_defer_on_redirect_);
  did_defer_on_writing_ = false;

  DCHECK(is_using_io_buffer_not_from_writer_);
  // |buffer_| is set to a net::IOBufferWithSize. Write the buffer contents
  // to the data pipe.
  DCHECK_GT(buffer_bytes_read_, 0u);
  if (!CopyReadDataToDataPipe(&did_defer_on_writing_)) {
    CancelWithError(net::ERR_INSUFFICIENT_RESOURCES);
    return;
  }

  if (did_defer_on_writing_) {
    // Continue waiting.
    return;
  }
  request()->LogUnblocked();
  Resume();
}

void MojoAsyncResourceHandler::Cancel(uint32_t custom_reason,
                                      const std::string& description) {
  ResourceRequestInfoImpl* info = GetRequestInfo();

  if (custom_reason == network::mojom::URLLoader::kClientDisconnectReason)
    info->set_custom_cancel_reason(description);

  ResourceDispatcherHostImpl::Get()->CancelRequestFromRenderer(
      GlobalRequestID(info->GetChildID(), info->GetRequestID()));
}

int64_t MojoAsyncResourceHandler::CalculateRecentlyReceivedBytes() {
  int64_t total_received_bytes = request()->GetTotalReceivedBytes();
  int64_t bytes_to_report =
      total_received_bytes - reported_total_received_bytes_;
  reported_total_received_bytes_ = total_received_bytes;
  DCHECK_LE(0, bytes_to_report);
  return bytes_to_report;
}

void MojoAsyncResourceHandler::ReportBadMessage(const std::string& error) {
  mojo::ReportBadMessage(error);
}

std::unique_ptr<network::UploadProgressTracker>
MojoAsyncResourceHandler::CreateUploadProgressTracker(
    const base::Location& from_here,
    network::UploadProgressTracker::UploadProgressReportCallback callback) {
  return std::make_unique<network::UploadProgressTracker>(
      from_here, std::move(callback), request());
}

void MojoAsyncResourceHandler::SendUploadProgress(
    const net::UploadProgress& progress) {
  url_loader_client_->OnUploadProgress(
      progress.position(), progress.size(),
      base::BindOnce(&MojoAsyncResourceHandler::OnUploadProgressACK,
                     weak_factory_.GetWeakPtr()));
}

void MojoAsyncResourceHandler::OnUploadProgressACK() {
  if (upload_progress_tracker_)
    upload_progress_tracker_->OnAckReceived();
}

}  // namespace content
