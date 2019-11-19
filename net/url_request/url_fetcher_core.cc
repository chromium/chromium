// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/url_request/url_fetcher_core.h"

#include <stdint.h>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/sequenced_task_runner.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "net/base/elements_upload_data_stream.h"
#include "net/base/io_buffer.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/base/request_priority.h"
#include "net/base/upload_bytes_element_reader.h"
#include "net/base/upload_data_stream.h"
#include "net/base/upload_file_element_reader.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/redirect_info.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "net/url_request/url_fetcher_response_writer.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_throttler_manager.h"
#include "url/origin.h"

namespace {

const int kBufferSize = 4096;
const int kUploadProgressTimerInterval = 100;
bool g_ignore_certificate_requests = false;

}  // namespace

namespace net {

// URLFetcherCore::Registry ---------------------------------------------------

URLFetcherCore::Registry::Registry() = default;
URLFetcherCore::Registry::~Registry() = default;

void URLFetcherCore::Registry::AddURLFetcherCore(URLFetcherCore* core) {
  DCHECK(!base::Contains(fetchers_, core));
  fetchers_.insert(core);
}

void URLFetcherCore::Registry::RemoveURLFetcherCore(URLFetcherCore* core) {
  DCHECK(base::Contains(fetchers_, core));
  fetchers_.erase(core);
}

void URLFetcherCore::Registry::CancelAll() {
  while (!fetchers_.empty())
    (*fetchers_.begin())->CancelURLRequest(ERR_ABORTED);
}

// URLFetcherCore -------------------------------------------------------------

// static
base::LazyInstance<URLFetcherCore::Registry>::DestructorAtExit
    URLFetcherCore::g_registry = LAZY_INSTANCE_INITIALIZER;

URLFetcherCore::URLFetcherCore(
    URLFetcher* fetcher,
    const GURL& original_url,
    URLFetcher::RequestType request_type,
    URLFetcherDelegate* d,
    net::NetworkTrafficAnnotationTag traffic_annotation)
    : fetcher_(fetcher),
      original_url_(original_url),
      request_type_(request_type),
      delegate_(d),
      delegate_task_runner_(base::SequencedTaskRunnerHandle::Get()),
      load_flags_(LOAD_NORMAL),
      allow_credentials_(base::nullopt),
      response_code_(URLFetcher::RESPONSE_CODE_INVALID),
      url_request_data_key_(nullptr),
      was_cached_(false),
      received_response_content_length_(0),
      total_received_bytes_(0),
      upload_content_set_(false),
      upload_range_offset_(0),
      upload_range_length_(0),
      referrer_policy_(
          URLRequest::CLEAR_REFERRER_ON_TRANSITION_FROM_SECURE_TO_INSECURE),
      is_chunked_upload_(false),
      was_cancelled_(false),
      stop_on_redirect_(false),
      stopped_on_redirect_(false),
      automatically_retry_on_5xx_(true),
      num_retries_on_5xx_(0),
      max_retries_on_5xx_(0),
      num_retries_on_network_changes_(0),
      max_retries_on_network_changes_(0),
      current_upload_bytes_(-1),
      current_response_bytes_(0),
      total_response_bytes_(-1),
      traffic_annotation_(traffic_annotation) {
  CHECK(original_url_.is_valid());
}

void URLFetcherCore::Start() {
  DCHECK(delegate_task_runner_);
  DCHECK(request_context_getter_.get()) << "We need an URLRequestContext!";
  if (network_task_runner_.get()) {
    DCHECK_EQ(network_task_runner_,
              request_context_getter_->GetNetworkTaskRunner());
  } else {
    network_task_runner_ = request_context_getter_->GetNetworkTaskRunner();
  }
  DCHECK(network_task_runner_.get()) << "We need an IO task runner";

  network_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&URLFetcherCore::StartOnIOThread, this));
}

void URLFetcherCore::Stop() {
  if (delegate_task_runner_)  // May be NULL in tests.
    DCHECK(delegate_task_runner_->RunsTasksInCurrentSequence());

  delegate_ = nullptr;
  fetcher_ = nullptr;
  if (!network_task_runner_.get())
    return;
  if (network_task_runner_->RunsTasksInCurrentSequence()) {
    CancelURLRequest(ERR_ABORTED);
  } else {
    network_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&URLFetcherCore::CancelURLRequest, this, ERR_ABORTED));
  }
}

void URLFetcherCore::SetUploadData(const std::string& upload_content_type,
                                   const std::string& upload_content) {
  AssertHasNoUploadData();
  DCHECK(!is_chunked_upload_);
  DCHECK(upload_content_type_.empty());

  // Empty |upload_content_type| is allowed iff the |upload_content| is empty.
  DCHECK(upload_content.empty() || !upload_content_type.empty());

  upload_content_type_ = upload_content_type;
  upload_content_ = upload_content;
  upload_content_set_ = true;
}

void URLFetcherCore::SetUploadFilePath(
    const std::string& upload_content_type,
    const base::FilePath& file_path,
    uint64_t range_offset,
    uint64_t range_length,
    scoped_refptr<base::TaskRunner> file_task_runner) {
  AssertHasNoUploadData();
  DCHECK(!is_chunked_upload_);
  DCHECK_EQ(upload_range_offset_, 0ULL);
  DCHECK_EQ(upload_range_length_, 0ULL);
  DCHECK(upload_content_type_.empty());
  DCHECK(!upload_content_type.empty());

  upload_content_type_ = upload_content_type;
  upload_file_path_ = file_path;
  upload_range_offset_ = range_offset;
  upload_range_length_ = range_length;
  upload_file_task_runner_ = file_task_runner;
  upload_content_set_ = true;
}

void URLFetcherCore::SetUploadStreamFactory(
    const std::string& upload_content_type,
    const URLFetcher::CreateUploadStreamCallback& factory) {
  AssertHasNoUploadData();
  DCHECK(!is_chunked_upload_);
  DCHECK(upload_content_type_.empty());

  upload_content_type_ = upload_content_type;
  upload_stream_factory_ = factory;
  upload_content_set_ = true;
}

void URLFetcherCore::SetChunkedUpload(const std::string& content_type) {
  if (!is_chunked_upload_) {
    AssertHasNoUploadData();
    DCHECK(upload_content_type_.empty());
  }

  // Empty |content_type| is not allowed here, because it is impossible
  // to ensure non-empty upload content as it is not yet supplied.
  DCHECK(!content_type.empty());

  upload_content_type_ = content_type;
  base::STLClearObject(&upload_content_);
  is_chunked_upload_ = true;
}

void URLFetcherCore::AppendChunkToUpload(const std::string& content,
                                         bool is_last_chunk) {
  DCHECK(delegate_task_runner_);
  DCHECK(network_task_runner_.get());
  DCHECK(is_chunked_upload_);
  network_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&URLFetcherCore::CompleteAddingUploadDataChunk,
                                this, content, is_last_chunk));
}

void URLFetcherCore::SetLoadFlags(int load_flags) {
  load_flags_ = load_flags;
}

void URLFetcherCore::SetAllowCredentials(bool allow_credentials) {
  allow_credentials_ = base::make_optional<bool>(allow_credentials);
}

int URLFetcherCore::GetLoadFlags() const {
  return load_flags_;
}

void URLFetcherCore::SetReferrer(const std::string& referrer) {
  referrer_ = referrer;
}

void URLFetcherCore::SetReferrerPolicy(
    URLRequest::ReferrerPolicy referrer_policy) {
  referrer_policy_ = referrer_policy;
}

void URLFetcherCore::SetExtraRequestHeaders(
    const std::string& extra_request_headers) {
  extra_request_headers_.Clear();
  extra_request_headers_.AddHeadersFromString(extra_request_headers);
}

void URLFetcherCore::AddExtraRequestHeader(const std::string& header_line) {
  extra_request_headers_.AddHeaderFromString(header_line);
}

void URLFetcherCore::SetRequestContext(
    URLRequestContextGetter* request_context_getter) {
  DCHECK(!request_context_getter_.get());
  DCHECK(request_context_getter);
  request_context_getter_ = request_context_getter;
}

void URLFetcherCore::SetInitiator(
    const base::Optional<url::Origin>& initiator) {
  DCHECK(!initiator_.has_value());
  initiator_ = initiator;
}

void URLFetcherCore::SetURLRequestUserData(
    const void* key,
    const URLFetcher::CreateDataCallback& create_data_callback) {
  DCHECK(key);
  DCHECK(!create_data_callback.is_null());
  url_request_data_key_ = key;
  url_request_create_data_callback_ = create_data_callback;
}

void URLFetcherCore::SetStopOnRedirect(bool stop_on_redirect) {
  stop_on_redirect_ = stop_on_redirect;
}

void URLFetcherCore::SetAutomaticallyRetryOn5xx(bool retry) {
  automatically_retry_on_5xx_ = retry;
}

void URLFetcherCore::SetMaxRetriesOn5xx(int max_retries) {
  max_retries_on_5xx_ = max_retries;
}

int URLFetcherCore::GetMaxRetriesOn5xx() const {
  return max_retries_on_5xx_;
}

base::TimeDelta URLFetcherCore::GetBackoffDelay() const {
  return backoff_delay_;
}

void URLFetcherCore::SetAutomaticallyRetryOnNetworkChanges(int max_retries) {
  max_retries_on_network_changes_ = max_retries;
}

void URLFetcherCore::SaveResponseToFileAtPath(
    const base::FilePath& file_path,
    scoped_refptr<base::SequencedTaskRunner> file_task_runner) {
  DCHECK(delegate_task_runner_->RunsTasksInCurrentSequence());
  SaveResponseWithWriter(std::unique_ptr<URLFetcherResponseWriter>(
      new URLFetcherFileWriter(file_task_runner, file_path)));
}

void URLFetcherCore::SaveResponseToTemporaryFile(
    scoped_refptr<base::SequencedTaskRunner> file_task_runner) {
  DCHECK(delegate_task_runner_->RunsTasksInCurrentSequence());
  SaveResponseWithWriter(std::unique_ptr<URLFetcherResponseWriter>(
      new URLFetcherFileWriter(file_task_runner, base::FilePath())));
}

void URLFetcherCore::SaveResponseWithWriter(
    std::unique_ptr<URLFetcherResponseWriter> response_writer) {
  DCHECK(delegate_task_runner_->RunsTasksInCurrentSequence());
  response_writer_ = std::move(response_writer);
}

HttpResponseHeaders* URLFetcherCore::GetResponseHeaders() const {
  return response_headers_.get();
}

// TODO(panayiotis): remote_endpoint_ is written in the IO thread,
// if this is accessed in the UI thread, this could result in a race.
// Same for response_headers_ above.
IPEndPoint URLFetcherCore::GetSocketAddress() const {
  return remote_endpoint_;
}

const ProxyServer& URLFetcherCore::ProxyServerUsed() const {
  return proxy_server_;
}

bool URLFetcherCore::WasCached() const {
  return was_cached_;
}

int64_t URLFetcherCore::GetReceivedResponseContentLength() const {
  return received_response_content_length_;
}

int64_t URLFetcherCore::GetTotalReceivedBytes() const {
  return total_received_bytes_;
}

const GURL& URLFetcherCore::GetOriginalURL() const {
  return original_url_;
}

const GURL& URLFetcherCore::GetURL() const {
  return url_;
}

const URLRequestStatus& URLFetcherCore::GetStatus() const {
  return status_;
}

int URLFetcherCore::GetResponseCode() const {
  return response_code_;
}

void URLFetcherCore::ReceivedContentWasMalformed() {
  DCHECK(delegate_task_runner_->RunsTasksInCurrentSequence());
  if (network_task_runner_.get()) {
    network_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&URLFetcherCore::NotifyMalformedContent, this));
  }
}

bool URLFetcherCore::GetResponseAsString(
    std::string* out_response_string) const {
  URLFetcherStringWriter* string_writer =
      response_writer_ ? response_writer_->AsStringWriter() : nullptr;
  if (!string_writer)
    return false;

  *out_response_string = string_writer->data();
  return true;
}

bool URLFetcherCore::GetResponseAsFilePath(bool take_ownership,
                                           base::FilePath* out_response_path) {
  DCHECK(delegate_task_runner_->RunsTasksInCurrentSequence());

  URLFetcherFileWriter* file_writer =
      response_writer_ ? response_writer_->AsFileWriter() : nullptr;
  if (!file_writer)
    return false;

  *out_response_path = file_writer->file_path();

  if (take_ownership) {
    // Intentionally calling a file_writer_ method directly without posting
    // the task to network_task_runner_.
    //
    // This is for correctly handling the case when file_writer_->DisownFile()
    // is soon followed by URLFetcherCore::Stop(). We have to make sure that
    // DisownFile takes effect before Stop deletes file_writer_.
    //
    // This direct call should be thread-safe, since DisownFile itself does no
    // file operation. It just flips the state to be referred in destruction.
    file_writer->DisownFile();
  }
  return true;
}

void URLFetcherCore::OnReceivedRedirect(URLRequest* request,
                                        const RedirectInfo& redirect_info,
                                        bool* defer_redirect) {
  DCHECK_EQ(request, request_.get());
  DCHECK(network_task_runner_->BelongsToCurrentThread());
  if (stop_on_redirect_) {
    stopped_on_redirect_ = true;
    url_ = redirect_info.new_url;
    response_code_ = request_->GetResponseCode();
    proxy_server_ = request_->proxy_server();
    was_cached_ = request_->was_cached();
    total_received_bytes_ += request_->GetTotalReceivedBytes();
    int result = request->Cancel();
    OnReadCompleted(request, result);
  }
}

void URLFetcherCore::OnResponseStarted(URLRequest* request, int net_error) {
  DCHECK_EQ(request, request_.get());
  DCHECK(network_task_runner_->BelongsToCurrentThread());
  DCHECK_NE(ERR_IO_PENDING, net_error);

  if (net_error == OK) {
    response_code_ = request_->GetResponseCode();
    response_headers_ = request_->response_headers();
    remote_endpoint_ = request_->GetResponseRemoteEndpoint();
    proxy_server_ = request_->proxy_server();
    was_cached_ = request_->was_cached();
    total_response_bytes_ = request_->GetExpectedContentSize();
  }

  DCHECK(!buffer_);
  if (request_type_ != URLFetcher::HEAD)
    buffer_ = base::MakeRefCounted<IOBuffer>(kBufferSize);
  ReadResponse();
}

void URLFetcherCore::OnCertificateRequested(
    URLRequest* request,
    SSLCertRequestInfo* cert_request_info) {
  DCHECK_EQ(request, request_.get());
  DCHECK(network_task_runner_->BelongsToCurrentThread());

  if (g_ignore_certificate_requests) {
    request->ContinueWithCertificate(nullptr, nullptr);
  } else {
    request->Cancel();
  }
}

void URLFetcherCore::OnReadCompleted(URLRequest* request,
                                     int bytes_read) {
  DCHECK_EQ(request, request_.get());
  DCHECK(network_task_runner_->BelongsToCurrentThread());

  if (!stopped_on_redirect_)
    url_ = request->url();
  URLRequestThrottlerManager* throttler_manager =
      request->context()->throttler_manager();
  if (throttler_manager)
    url_throttler_entry_ = throttler_manager->RegisterRequestUrl(url_);

  while (bytes_read > 0) {
    current_response_bytes_ += bytes_read;
    InformDelegateDownloadProgress();

    const int result = WriteBuffer(
        base::MakeRefCounted<DrainableIOBuffer>(buffer_, bytes_read));
    if (result < 0) {
      // Write failed or waiting for write completion.
      return;
    }
    bytes_read = request_->Read(buffer_.get(), kBufferSize);
  }

  // See comments re: HEAD requests in ReadResponse().
  if (bytes_read != ERR_IO_PENDING || request_type_ == URLFetcher::HEAD) {
    status_ = URLRequestStatus::FromError(bytes_read);
    received_response_content_length_ =
        request_->received_response_content_length();
    total_received_bytes_ += request_->GetTotalReceivedBytes();
    ReleaseRequest();

    // No more data to write.
    const int result = response_writer_->Finish(
        bytes_read > 0 ? OK : bytes_read,
        base::BindOnce(&URLFetcherCore::DidFinishWriting, this));
    if (result != ERR_IO_PENDING)
      DidFinishWriting(result);
  }
}

void URLFetcherCore::OnContextShuttingDown() {
  DCHECK(request_);
  CancelRequestAndInformDelegate(ERR_CONTEXT_SHUT_DOWN);
}

void URLFetcherCore::CancelAll() {
  g_registry.Get().CancelAll();
}

int URLFetcherCore::GetNumFetcherCores() {
  return g_registry.Get().size();
}

void URLFetcherCore::SetIgnoreCertificateRequests(bool ignored) {
  g_ignore_certificate_requests = ignored;
}

URLFetcherCore::~URLFetcherCore() {
  // |request_| should be NULL. If not, it's unsafe to delete it here since we
  // may not be on the IO thread.
  DCHECK(!request_.get());
}

void URLFetcherCore::StartOnIOThread() {
  DCHECK(network_task_runner_->BelongsToCurrentThread());

  // Create ChunkedUploadDataStream, if needed, so the consumer can start
  // appending data.  Have to do it here because StartURLRequest() may be called
  // asynchonously.
  if (is_chunked_upload_) {
    chunked_stream_.reset(new ChunkedUploadDataStream(0));
    chunked_stream_writer_ = chunked_stream_->CreateWriter();
  }

  if (!response_writer_)
    response_writer_.reset(new URLFetcherStringWriter);

  const int result = response_writer_->Initialize(
      base::BindOnce(&URLFetcherCore::DidInitializeWriter, this));
  if (result != ERR_IO_PENDING)
    DidInitializeWriter(result);
}

void URLFetcherCore::StartURLRequest() {
  DCHECK(network_task_runner_->BelongsToCurrentThread());

  if (was_cancelled_) {
    // Since StartURLRequest() is posted as a *delayed* task, it may
    // run after the URLFetcher was already stopped.
    return;
  }

  DCHECK(request_context_getter_);
  if (!request_context_getter_->GetURLRequestContext()) {
    CancelRequestAndInformDelegate(ERR_CONTEXT_SHUT_DOWN);
    return;
  }

  DCHECK(!request_.get());

  g_registry.Get().AddURLFetcherCore(this);
  current_response_bytes_ = 0;
  request_context_getter_->AddObserver(this);
  request_ = request_context_getter_->GetURLRequestContext()->CreateRequest(
      original_url_, DEFAULT_PRIORITY, this, traffic_annotation_);
  int flags = request_->load_flags() | load_flags_;

  // TODO(mmenke): This should really be with the other code to set the upload
  // body, below.
  if (chunked_stream_)
    request_->set_upload(std::move(chunked_stream_));

  request_->SetLoadFlags(flags);
  if (allow_credentials_) {
    request_->set_allow_credentials(allow_credentials_.value());
  }
  request_->SetReferrer(referrer_);
  request_->set_referrer_policy(referrer_policy_);
  request_->set_site_for_cookies(initiator_.has_value() &&
                                         !initiator_.value().opaque()
                                     ? initiator_.value().GetURL()
                                     : original_url_);
  request_->set_initiator(initiator_);
  if (url_request_data_key_ && !url_request_create_data_callback_.is_null()) {
    request_->SetUserData(url_request_data_key_,
                          url_request_create_data_callback_.Run());
  }

  switch (request_type_) {
    case URLFetcher::GET:
      break;

    case URLFetcher::POST:
    case URLFetcher::PUT:
    case URLFetcher::PATCH: {
      // Upload content must be set.
      DCHECK(is_chunked_upload_ || upload_content_set_);

      request_->set_method(
          request_type_ == URLFetcher::POST ? "POST" :
          request_type_ == URLFetcher::PUT ? "PUT" : "PATCH");
      if (!upload_content_type_.empty()) {
        extra_request_headers_.SetHeader(HttpRequestHeaders::kContentType,
                                         upload_content_type_);
      }
      if (!upload_content_.empty()) {
        std::unique_ptr<UploadElementReader> reader(
            new UploadBytesElementReader(upload_content_.data(),
                                         upload_content_.size()));
        request_->set_upload(
            ElementsUploadDataStream::CreateWithReader(std::move(reader), 0));
      } else if (!upload_file_path_.empty()) {
        std::unique_ptr<UploadElementReader> reader(new UploadFileElementReader(
            upload_file_task_runner_.get(), upload_file_path_,
            upload_range_offset_, upload_range_length_, base::Time()));
        request_->set_upload(
            ElementsUploadDataStream::CreateWithReader(std::move(reader), 0));
      } else if (!upload_stream_factory_.is_null()) {
        std::unique_ptr<UploadDataStream> stream = upload_stream_factory_.Run();
        DCHECK(stream);
        request_->set_upload(std::move(stream));
      }

      current_upload_bytes_ = -1;
      // TODO(kinaba): http://crbug.com/118103. Implement upload callback in the
      //  layer and avoid using timer here.
      upload_progress_checker_timer_.reset(new base::RepeatingTimer());
      upload_progress_checker_timer_->Start(
          FROM_HERE,
          base::TimeDelta::FromMilliseconds(kUploadProgressTimerInterval),
          this,
          &URLFetcherCore::InformDelegateUploadProgress);
      break;
    }

    case URLFetcher::HEAD:
      request_->set_method("HEAD");
      break;

    case URLFetcher::DELETE_REQUEST:
      request_->set_method("DELETE");
      break;

    default:
      NOTREACHED();
  }

  if (!extra_request_headers_.IsEmpty())
    request_->SetExtraRequestHeaders(extra_request_headers_);

  request_->Start();
}

void URLFetcherCore::DidInitializeWriter(int result) {
  if (result != OK) {
    CancelRequestAndInformDelegate(result);
    return;
  }
  StartURLRequestWhenAppropriate();
}

void URLFetcherCore::StartURLRequestWhenAppropriate() {
  DCHECK(network_task_runner_->BelongsToCurrentThread());

  if (was_cancelled_)
    return;

  DCHECK(request_context_getter_.get());

  // Check if the request should be delayed, and if so, post a task to start it
  // after the delay has expired.  Otherwise, start it now.

  URLRequestContext* context = request_context_getter_->GetURLRequestContext();
  // If the context has been shut down, or there's no ThrottlerManager, just
  // start the request.  In the former case, StartURLRequest() will just inform
  // the URLFetcher::Delegate the request has been canceled.
  if (context && context->throttler_manager()) {
    if (!original_url_throttler_entry_.get()) {
      original_url_throttler_entry_ =
          context->throttler_manager()->RegisterRequestUrl(original_url_);
    }

    if (original_url_throttler_entry_.get()) {
      int64_t delay =
          original_url_throttler_entry_->ReserveSendingTimeForNextRequest(
              GetBackoffReleaseTime());
      if (delay != 0) {
        network_task_runner_->PostDelayedTask(
            FROM_HERE, base::BindOnce(&URLFetcherCore::StartURLRequest, this),
            base::TimeDelta::FromMilliseconds(delay));
        return;
      }
    }
  }

  StartURLRequest();
}

void URLFetcherCore::CancelURLRequest(int error) {
  DCHECK(network_task_runner_->BelongsToCurrentThread());

  if (request_.get()) {
    request_->CancelWithError(error);
    ReleaseRequest();
  }

  // Set the error manually.
  // Normally, calling URLRequest::CancelWithError() results in calling
  // OnReadCompleted() with bytes_read = -1 via an asynchronous task posted by
  // URLRequestJob::NotifyDone(). But, because the request was released
  // immediately after being canceled, the request could not call
  // OnReadCompleted() which overwrites |status_| with the error status.
  status_ = URLRequestStatus(URLRequestStatus::CANCELED, error);

  // Release the reference to the request context. There could be multiple
  // references to URLFetcher::Core at this point so it may take a while to
  // delete the object, but we cannot delay the destruction of the request
  // context.
  request_context_getter_ = nullptr;
  initiator_.reset();
  url_request_data_key_ = nullptr;
  url_request_create_data_callback_.Reset();
  was_cancelled_ = true;
}

void URLFetcherCore::OnCompletedURLRequest(
    base::TimeDelta backoff_delay) {
  DCHECK(delegate_task_runner_->RunsTasksInCurrentSequence());

  // Save the status and backoff_delay so that delegates can read it.
  if (delegate_) {
    backoff_delay_ = backoff_delay;
    InformDelegateFetchIsComplete();
  }
}

void URLFetcherCore::InformDelegateFetchIsComplete() {
  DCHECK(delegate_task_runner_->RunsTasksInCurrentSequence());
  if (delegate_)
    delegate_->OnURLFetchComplete(fetcher_);
}

void URLFetcherCore::NotifyMalformedContent() {
  DCHECK(network_task_runner_->BelongsToCurrentThread());
  if (url_throttler_entry_.get()) {
    int status_code = response_code_;
    if (status_code == URLFetcher::RESPONSE_CODE_INVALID) {
      // The status code will generally be known by the time clients
      // call the |ReceivedContentWasMalformed()| function (which ends up
      // calling the current function) but if it's not, we need to assume
      // the response was successful so that the total failure count
      // used to calculate exponential back-off goes up.
      status_code = 200;
    }
    url_throttler_entry_->ReceivedContentWasMalformed(status_code);
  }
}

void URLFetcherCore::DidFinishWriting(int result) {
  if (result != OK) {
    CancelRequestAndInformDelegate(result);
    return;
  }
  // If the file was successfully closed, then the URL request is complete.
  RetryOrCompleteUrlFetch();
}

void URLFetcherCore::RetryOrCompleteUrlFetch() {
  DCHECK(network_task_runner_->BelongsToCurrentThread());
  base::TimeDelta backoff_delay;

  // Checks the response from server.
  if (response_code_ >= 500 ||
      status_.error() == ERR_TEMPORARILY_THROTTLED) {
    // When encountering a server error, we will send the request again
    // after backoff time.
    ++num_retries_on_5xx_;

    // Note that backoff_delay may be 0 because (a) the
    // URLRequestThrottlerManager and related code does not
    // necessarily back off on the first error, (b) it only backs off
    // on some of the 5xx status codes, (c) not all URLRequestContexts
    // have a throttler manager.
    base::TimeTicks backoff_release_time = GetBackoffReleaseTime();
    backoff_delay = backoff_release_time - base::TimeTicks::Now();
    if (backoff_delay < base::TimeDelta())
      backoff_delay = base::TimeDelta();

    if (automatically_retry_on_5xx_ &&
        num_retries_on_5xx_ <= max_retries_on_5xx_) {
      StartOnIOThread();
      return;
    }
  } else {
    backoff_delay = base::TimeDelta();
  }

  // Retry if the request failed due to network changes.
  if (status_.error() == ERR_NETWORK_CHANGED &&
      num_retries_on_network_changes_ < max_retries_on_network_changes_) {
    ++num_retries_on_network_changes_;

    // Retry soon, after flushing all the current tasks which may include
    // further network change observers.
    network_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&URLFetcherCore::StartOnIOThread, this));
    return;
  }

  request_context_getter_ = nullptr;
  initiator_.reset();
  url_request_data_key_ = nullptr;
  url_request_create_data_callback_.Reset();
  bool posted = delegate_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&URLFetcherCore::OnCompletedURLRequest, this,
                                backoff_delay));

  // If the delegate message loop does not exist any more, then the delegate
  // should be gone too.
  DCHECK(posted || !delegate_);
}

void URLFetcherCore::CancelRequestAndInformDelegate(int result) {
  CancelURLRequest(result);
  delegate_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&URLFetcherCore::InformDelegateFetchIsComplete, this));
}

void URLFetcherCore::ReleaseRequest() {
  request_context_getter_->RemoveObserver(this);
  upload_progress_checker_timer_.reset();
  request_.reset();
  buffer_ = nullptr;
  g_registry.Get().RemoveURLFetcherCore(this);
}

base::TimeTicks URLFetcherCore::GetBackoffReleaseTime() {
  DCHECK(network_task_runner_->BelongsToCurrentThread());

  if (!original_url_throttler_entry_.get())
    return base::TimeTicks();

  base::TimeTicks original_url_backoff =
      original_url_throttler_entry_->GetExponentialBackoffReleaseTime();
  base::TimeTicks destination_url_backoff;
  if (url_throttler_entry_.get() &&
      original_url_throttler_entry_.get() != url_throttler_entry_.get()) {
    destination_url_backoff =
        url_throttler_entry_->GetExponentialBackoffReleaseTime();
  }

  return original_url_backoff > destination_url_backoff ?
      original_url_backoff : destination_url_backoff;
}

void URLFetcherCore::CompleteAddingUploadDataChunk(
    const std::string& content, bool is_last_chunk) {
  DCHECK(is_chunked_upload_);
  DCHECK(!content.empty());
  chunked_stream_writer_->AppendData(
      content.data(), static_cast<int>(content.length()), is_last_chunk);
}

int URLFetcherCore::WriteBuffer(scoped_refptr<DrainableIOBuffer> data) {
  while (data->BytesRemaining() > 0) {
    const int result = response_writer_->Write(
        data.get(), data->BytesRemaining(),
        base::BindOnce(&URLFetcherCore::DidWriteBuffer, this, data));
    if (result < 0) {
      if (result != ERR_IO_PENDING)
        DidWriteBuffer(data, result);
      return result;
    }
    data->DidConsume(result);
  }
  return OK;
}

void URLFetcherCore::DidWriteBuffer(scoped_refptr<DrainableIOBuffer> data,
                                    int result) {
  if (result < 0) {  // Handle errors.
    response_writer_->Finish(result, base::DoNothing());
    CancelRequestAndInformDelegate(result);
    return;
  }

  // Continue writing.
  data->DidConsume(result);
  if (WriteBuffer(data) < 0)
    return;

  // Finished writing buffer_. Read some more, unless the request has been
  // cancelled and deleted.
  DCHECK_EQ(0, data->BytesRemaining());
  if (request_.get())
    ReadResponse();
}

void URLFetcherCore::ReadResponse() {
  // Some servers may treat HEAD requests as GET requests. To free up the
  // network connection as soon as possible, signal that the request has
  // completed immediately, without trying to read any data back (all we care
  // about is the response code and headers, which we already have).
  int bytes_read = 0;
  if (request_type_ != URLFetcher::HEAD)
    bytes_read = request_->Read(buffer_.get(), kBufferSize);

  OnReadCompleted(request_.get(), bytes_read);
}

void URLFetcherCore::InformDelegateUploadProgress() {
  DCHECK(network_task_runner_->BelongsToCurrentThread());
  if (request_.get()) {
    int64_t current = request_->GetUploadProgress().position();
    if (current_upload_bytes_ != current) {
      current_upload_bytes_ = current;
      int64_t total = -1;
      if (!is_chunked_upload_) {
        total = static_cast<int64_t>(request_->GetUploadProgress().size());
        // Total may be zero if the UploadDataStream::Init has not been called
        // yet. Don't send the upload progress until the size is initialized.
        if (!total)
          return;
      }
      delegate_task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(
              &URLFetcherCore::InformDelegateUploadProgressInDelegateSequence,
              this, current, total));
    }
  }
}

void URLFetcherCore::InformDelegateUploadProgressInDelegateSequence(
    int64_t current,
    int64_t total) {
  DCHECK(delegate_task_runner_->RunsTasksInCurrentSequence());
  if (delegate_)
    delegate_->OnURLFetchUploadProgress(fetcher_, current, total);
}

void URLFetcherCore::InformDelegateDownloadProgress() {
  DCHECK(network_task_runner_->BelongsToCurrentThread());
  delegate_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &URLFetcherCore::InformDelegateDownloadProgressInDelegateSequence,
          this, current_response_bytes_, total_response_bytes_,
          request_->GetTotalReceivedBytes()));
}

void URLFetcherCore::InformDelegateDownloadProgressInDelegateSequence(
    int64_t current,
    int64_t total,
    int64_t current_network_bytes) {
  DCHECK(delegate_task_runner_->RunsTasksInCurrentSequence());
  if (delegate_)
    delegate_->OnURLFetchDownloadProgress(fetcher_, current, total,
                                          current_network_bytes);
}

void URLFetcherCore::AssertHasNoUploadData() const {
  DCHECK(!upload_content_set_);
  DCHECK(upload_content_.empty());
  DCHECK(upload_file_path_.empty());
  DCHECK(upload_stream_factory_.is_null());
}

}  // namespace net
