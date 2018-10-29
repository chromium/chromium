// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/download/download_request_core.h"

#include <string>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/format_macros.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/download/public/common/download_create_info.h"
#include "components/download/public/common/download_interrupt_reasons_utils.h"
#include "components/download/public/common/download_item.h"
#include "components/download/public/common/download_stats.h"
#include "components/download/public/common/download_task_runner.h"
#include "components/download/public/common/download_utils.h"
#include "content/browser/byte_stream.h"
#include "content/browser/download/download_manager_impl.h"
#include "content/browser/download/download_request_handle.h"
#include "content/browser/download/download_utils.h"
#include "content/browser/loader/resource_dispatcher_host_impl.h"
#include "content/browser/loader/resource_request_info_impl.h"
#include "content/browser/service_manager/service_manager_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/download_manager_delegate.h"
#include "content/public/browser/download_request_utils.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/resource_context.h"
#include "content/public/browser/web_contents.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "net/url_request/url_request_context_getter.h"
#include "services/device/public/mojom/constants.mojom.h"
#include "services/device/public/mojom/wake_lock_provider.mojom.h"
#include "services/service_manager/public/cpp/connector.h"

namespace content {

namespace {

// This is a UserData::Data that will be attached to a URLRequest as a
// side-channel for passing download parameters.
class DownloadRequestData : public base::SupportsUserData::Data {
 public:
  ~DownloadRequestData() override {}

  static void Attach(net::URLRequest* request,
                     download::DownloadUrlParameters* download_parameters,
                     bool is_new_download);
  static DownloadRequestData* Get(const net::URLRequest* request);
  static void Detach(net::URLRequest* request);

  std::unique_ptr<download::DownloadSaveInfo> TakeSaveInfo() {
    return std::move(save_info_);
  }
  bool is_new_download() const { return is_new_download_; }
  std::string guid() const { return guid_; }
  bool is_transient() const { return transient_; }
  bool fetch_error_body() const { return fetch_error_body_; }
  const download::DownloadUrlParameters::RequestHeadersType& request_headers()
      const {
    return request_headers_;
  }
  const download::DownloadUrlParameters::OnStartedCallback& callback() const {
    return on_started_callback_;
  }
  std::string request_origin() const { return request_origin_; }

 private:
  static const int kKey;

  std::unique_ptr<download::DownloadSaveInfo> save_info_;
  bool is_new_download_;
  std::string guid_;
  bool fetch_error_body_ = false;
  download::DownloadUrlParameters::RequestHeadersType request_headers_;
  bool transient_ = false;
  download::DownloadUrlParameters::OnStartedCallback on_started_callback_;
  std::string request_origin_;
};

// static
const int DownloadRequestData::kKey = 0;

// static
void DownloadRequestData::Attach(net::URLRequest* request,
                                 download::DownloadUrlParameters* parameters,
                                 bool is_new_download) {
  auto request_data = std::make_unique<DownloadRequestData>();
  request_data->save_info_.reset(
      new download::DownloadSaveInfo(parameters->GetSaveInfo()));
  request_data->is_new_download_ = is_new_download;
  request_data->guid_ = parameters->guid();
  request_data->fetch_error_body_ = parameters->fetch_error_body();
  request_data->request_headers_ = parameters->request_headers();
  request_data->transient_ = parameters->is_transient();
  request_data->on_started_callback_ = parameters->callback();
  request_data->request_origin_ = parameters->request_origin();
  request->SetUserData(&kKey, std::move(request_data));
}

// static
DownloadRequestData* DownloadRequestData::Get(const net::URLRequest* request) {
  return static_cast<DownloadRequestData*>(request->GetUserData(&kKey));
}

// static
void DownloadRequestData::Detach(net::URLRequest* request) {
  request->RemoveUserData(&kKey);
}

}  // namespace

const int DownloadRequestCore::kDownloadByteStreamSize = 100 * 1024;

// static
std::unique_ptr<net::URLRequest> DownloadRequestCore::CreateRequestOnIOThread(
    bool is_new_download,
    download::DownloadUrlParameters* params,
    scoped_refptr<net::URLRequestContextGetter> url_request_context_getter) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(is_new_download || !params->content_initiated())
      << "Content initiated downloads should be a new download";

  std::unique_ptr<net::URLRequest> request =
      CreateURLRequestOnIOThread(params, std::move(url_request_context_getter));

  DownloadRequestData::Attach(request.get(), params, is_new_download);
  return request;
}

// static impl of DownloadRequestUtils
std::string DownloadRequestUtils::GetRequestOriginFromRequest(
    const net::URLRequest* request) {
  DownloadRequestData* data = DownloadRequestData::Get(request);
  if (data)
    return data->request_origin();
  return std::string();  // Empty string if data does not exist.
}

DownloadRequestCore::DownloadRequestCore(
    net::URLRequest* request,
    Delegate* delegate,
    bool is_parallel_request,
    const std::string& request_origin,
    download::DownloadSource download_source)
    : delegate_(delegate),
      request_(request),
      is_new_download_(true),
      fetch_error_body_(false),
      transient_(false),
      bytes_read_(0),
      pause_count_(0),
      was_deferred_(false),
      is_partial_request_(false),
      started_(false),
      abort_reason_(download::DOWNLOAD_INTERRUPT_REASON_NONE),
      request_origin_(request_origin),
      download_source_(download_source) {
  DCHECK(request_);
  DCHECK(delegate_);
  if (!is_parallel_request) {
    download::RecordDownloadCountWithSource(download::UNTHROTTLED_COUNT,
                                            download_source);
  }

  // Request Wake Lock.
  service_manager::Connector* connector =
      ServiceManagerContext::GetConnectorForIOThread();
  // |connector| might be nullptr in some testing contexts, in which the
  // service manager connection isn't initialized.
  if (connector) {
    device::mojom::WakeLockProviderPtr wake_lock_provider;
    connector->BindInterface(device::mojom::kServiceName,
                             mojo::MakeRequest(&wake_lock_provider));
    wake_lock_provider->GetWakeLockWithoutContext(
        device::mojom::WakeLockType::kPreventAppSuspension,
        device::mojom::WakeLockReason::kOther, "Download in progress",
        mojo::MakeRequest(&wake_lock_));

    wake_lock_->RequestWakeLock();
  }

  DownloadRequestData* request_data = DownloadRequestData::Get(request_);
  if (request_data) {
    save_info_ = request_data->TakeSaveInfo();
    is_new_download_ = request_data->is_new_download();
    guid_ = request_data->guid();
    fetch_error_body_ = request_data->fetch_error_body();
    request_headers_ = request_data->request_headers();
    transient_ = request_data->is_transient();
    on_started_callback_ = request_data->callback();
    DownloadRequestData::Detach(request_);
    is_partial_request_ = save_info_->offset > 0;
  } else {
    save_info_.reset(new download::DownloadSaveInfo);
  }
}

DownloadRequestCore::~DownloadRequestCore() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  // Remove output stream callback if a stream exists.
  if (stream_writer_)
    stream_writer_->RegisterCallback(base::RepeatingClosure());
}

std::unique_ptr<download::DownloadCreateInfo>
DownloadRequestCore::CreateDownloadCreateInfo(
    download::DownloadInterruptReason result) {
  DCHECK(!started_);
  started_ = true;
  std::unique_ptr<download::DownloadCreateInfo> create_info(
      new download::DownloadCreateInfo(base::Time::Now(),
                                       std::move(save_info_)));

  if (result == download::DOWNLOAD_INTERRUPT_REASON_NONE)
    create_info->remote_address = request()->GetSocketAddress().host();
  create_info->method = request()->method();
  create_info->connection_info = request()->response_info().connection_info;
  create_info->url_chain = request()->url_chain();
  create_info->referrer_url = GURL(request()->referrer());
  create_info->referrer_policy = request()->referrer_policy();
  create_info->result = result;
  create_info->is_new_download = is_new_download_;
  create_info->guid = guid_;
  create_info->transient = transient_;
  create_info->response_headers = request()->response_headers();
  create_info->offset = create_info->save_info->offset;
  create_info->fetch_error_body = fetch_error_body_;
  create_info->request_headers = request_headers_;
  create_info->request_origin = request_origin_;
  create_info->download_source = download_source_;
  return create_info;
}

bool DownloadRequestCore::OnResponseStarted(
    const std::string& override_mime_type) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DVLOG(20) << __func__ << "() " << DebugString();

  download::DownloadInterruptReason result =
      request()->response_headers() ? download::HandleSuccessfulServerResponse(
                                          *request()->response_headers(),
                                          save_info_.get(), fetch_error_body_)
                                    : download::DOWNLOAD_INTERRUPT_REASON_NONE;

  if (request()->response_headers()) {
    download::RecordDownloadHttpResponseCode(
        request()->response_headers()->response_code());
  }

  std::unique_ptr<download::DownloadCreateInfo> create_info =
      CreateDownloadCreateInfo(result);
  if (result != download::DOWNLOAD_INTERRUPT_REASON_NONE) {
    delegate_->OnStart(std::move(create_info),
                       std::unique_ptr<ByteStreamReader>(),
                       base::ResetAndReturn(&on_started_callback_));
    return false;
  }

  // If it's a download, we don't want to poison the cache with it.
  request()->StopCaching();

  // Lower priority as well, so downloads don't contend for resources
  // with main frames.
  request()->SetPriority(net::IDLE);

  // Create the ByteStream for sending data to the download sink.
  std::unique_ptr<ByteStreamReader> stream_reader;
  CreateByteStream(base::ThreadTaskRunnerHandle::Get(),
                   download::GetDownloadTaskRunner(), kDownloadByteStreamSize,
                   &stream_writer_, &stream_reader);
  stream_writer_->RegisterCallback(
      base::BindRepeating(&DownloadRequestCore::ResumeRequest, AsWeakPtr()));

  if (!override_mime_type.empty())
    create_info->mime_type = override_mime_type;
  else
    request()->GetMimeType(&create_info->mime_type);

  // Get the last modified time and etag.
  const net::HttpResponseHeaders* headers = request()->response_headers();
  download::HandleResponseHeaders(headers, create_info.get());

  // If the content-length header is not present (or contains something other
  // than numbers), the incoming content_length is -1 (unknown size).
  // Set the content length to 0 to indicate unknown size to DownloadManager.
  create_info->total_bytes = request()->GetExpectedContentSize() > 0
                                 ? request()->GetExpectedContentSize()
                                 : 0;

  // GURL::GetOrigin() doesn't support getting the inner origin of a blob URL.
  // However, requesting a cross origin blob URL would have resulted in a
  // network error, so we'll just ignore them here. Furthermore, we consider
  // data: and about: schemes as same origin regardless of the initiator.
  if (request()->initiator().has_value() &&
      !create_info->url_chain.back().SchemeIsBlob() &&
      !create_info->url_chain.back().SchemeIs(url::kAboutScheme) &&
      !create_info->url_chain.back().SchemeIs(url::kDataScheme) &&
      request()->initiator()->GetURL() !=
          create_info->url_chain.back().GetOrigin()) {
    create_info->save_info->suggested_name.clear();
  }

  download::RecordDownloadContentDisposition(create_info->content_disposition);
  download::RecordDownloadSourcePageTransitionType(
      create_info->transition_type);

  delegate_->OnStart(std::move(create_info), std::move(stream_reader),
                     base::ResetAndReturn(&on_started_callback_));
  return true;
}

bool DownloadRequestCore::OnRequestRedirected() {
  DVLOG(20) << __func__ << "() " << DebugString();
  if (is_partial_request_) {
    // A redirect while attempting a partial resumption indicates a potential
    // middle box. Trigger another interruption so that the
    // download::DownloadItem can retry.
    abort_reason_ = download::DOWNLOAD_INTERRUPT_REASON_SERVER_UNREACHABLE;
    return false;
  }
  return true;
}

// Create a new buffer, which will be handed to the download thread for file
// writing and deletion.
bool DownloadRequestCore::OnWillRead(scoped_refptr<net::IOBuffer>* buf,
                                     int* buf_size) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(buf && buf_size);
  DCHECK(!read_buffer_.get());

  *buf_size = kReadBufSize;
  read_buffer_ = base::MakeRefCounted<net::IOBuffer>(*buf_size);
  *buf = read_buffer_.get();
  return true;
}

// Pass the buffer to the download file writer.
bool DownloadRequestCore::OnReadCompleted(int bytes_read, bool* defer) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(read_buffer_.get());

  if (!bytes_read)
    return true;
  bytes_read_ += bytes_read;
  DCHECK(read_buffer_.get());

  // Take the data ship it down the stream.  If the stream is full, pause the
  // request; the stream callback will resume it.
  if (!stream_writer_->Write(read_buffer_, bytes_read)) {
    PauseRequest();
    *defer = was_deferred_ = true;
  }

  read_buffer_ = nullptr;  // Drop our reference.

  if (pause_count_ > 0)
    *defer = was_deferred_ = true;

  return true;
}

void DownloadRequestCore::OnWillAbort(
    download::DownloadInterruptReason reason) {
  DVLOG(20) << __func__ << "() reason=" << reason << " " << DebugString();
  DCHECK(!started_);
  abort_reason_ = reason;
}

void DownloadRequestCore::OnResponseCompleted(
    const net::URLRequestStatus& status) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  int response_code = status.is_success() ? request()->GetResponseCode() : 0;
  DVLOG(20) << __func__ << "() " << DebugString()
            << " status.status() = " << status.status()
            << " status.error() = " << status.error()
            << " response_code = " << response_code;

  bool has_strong_validators = false;
  if (request()->response_headers()) {
    has_strong_validators =
        request()->response_headers()->HasStrongValidators();
  }

  net::Error error_code = net::OK;
  if (!status.is_success()) {
    error_code = static_cast<net::Error>(status.error());  // Normal case.
    // Make sure that at least the fact of failure comes through.
    if (error_code == net::OK)
      error_code = net::ERR_FAILED;
  }
  download::DownloadInterruptReason reason =
      download::HandleRequestCompletionStatus(error_code, has_strong_validators,
                                              request()->ssl_info().cert_status,
                                              abort_reason_);

  std::string accept_ranges;
  if (request()->response_headers()) {
    request()->response_headers()->EnumerateHeader(nullptr, "Accept-Ranges",
                                                   &accept_ranges);
  }

  // Send the info down the stream.  Conditional is in case we get
  // OnResponseCompleted without OnResponseStarted.
  if (stream_writer_)
    stream_writer_->Close(reason);

  // If the error mapped to something unknown, record it so that
  // we can drill down.
  if (reason == download::DOWNLOAD_INTERRUPT_REASON_NETWORK_FAILED) {
    base::UmaHistogramSparse("Download.MapErrorNetworkFailed",
                             std::abs(status.error()));
  }

  stream_writer_.reset();  // We no longer need the stream.
  read_buffer_ = nullptr;

  if (started_)
    return;

  // OnResponseCompleted() called without OnResponseStarted(). This should only
  // happen when the request was aborted.
  DCHECK_NE(reason, download::DOWNLOAD_INTERRUPT_REASON_NONE);
  std::unique_ptr<download::DownloadCreateInfo> create_info =
      CreateDownloadCreateInfo(reason);
  std::unique_ptr<ByteStreamReader> empty_byte_stream;
  delegate_->OnStart(std::move(create_info), std::move(empty_byte_stream),
                     base::ResetAndReturn(&on_started_callback_));
}

void DownloadRequestCore::PauseRequest() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  ++pause_count_;
}

void DownloadRequestCore::ResumeRequest() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK_LT(0, pause_count_);

  --pause_count_;

  if (!was_deferred_)
    return;
  if (pause_count_ > 0)
    return;

  was_deferred_ = false;

  delegate_->OnReadyToRead();
}

std::string DownloadRequestCore::DebugString() const {
  return base::StringPrintf(
      "{"
      " this=%p "
      " url_ = "
      "\"%s\""
      " }",
      reinterpret_cast<const void*>(this),
      request() ? request()->url().spec().c_str() : "<NULL request>");
}

}  // namespace content
