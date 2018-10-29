// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/devtools_url_interceptor_request_job.h"

#include "base/base64.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "content/browser/devtools/protocol/network_handler.h"
#include "content/browser/devtools/protocol/page.h"
#include "content/browser/loader/download_utils_impl.h"
#include "content/browser/loader/resource_request_info_impl.h"
#include "content/public/browser/browser_task_traits.h"
#include "ipc/ipc_channel.h"
#include "net/base/completion_once_callback.h"
#include "net/base/elements_upload_data_stream.h"
#include "net/base/io_buffer.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/base/upload_bytes_element_reader.h"
#include "net/base/upload_element_reader.h"
#include "net/cert/cert_status_flags.h"
#include "net/cookies/cookie_options.h"
#include "net/cookies/cookie_store.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_util.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "net/url_request/url_request_context.h"
#include "third_party/blink/public/platform/resource_request_blocked_reason.h"

namespace {
static const int kInitialBufferSize = 4096;
static const int kMaxBufferSize = IPC::Channel::kMaximumMessageSize / 4;
}  // namespace

namespace content {

// DevToolsURLInterceptorRequestJob::SubRequest ---------------------

// If the request was either allowed or modified, a SubRequest will be used to
// perform the fetch and the results proxied to the original request. This
// gives us the flexibility to pretend redirects didn't happen if the user
// chooses to mock the response.  Note this SubRequest is ignored by the
// interceptor.
class DevToolsURLInterceptorRequestJob::SubRequest
    : public net::URLRequest::Delegate {
 public:
  SubRequest(DevToolsURLInterceptorRequestJob::RequestDetails& request_details,
             DevToolsURLInterceptorRequestJob* devtools_interceptor_request_job,
             DevToolsURLRequestInterceptor* interceptor);
  ~SubRequest() override;

  // net::URLRequest::Delegate methods:
  void OnAuthRequired(net::URLRequest* request,
                      net::AuthChallengeInfo* auth_info) override;
  void OnCertificateRequested(
      net::URLRequest* request,
      net::SSLCertRequestInfo* cert_request_info) override;
  void OnSSLCertificateError(net::URLRequest* request,
                             const net::SSLInfo& ssl_info,
                             bool fatal) override;
  void OnResponseStarted(net::URLRequest* request, int net_error) override;
  void OnReadCompleted(net::URLRequest* request, int bytes_read) override;
  void OnReceivedRedirect(net::URLRequest* request,
                          const net::RedirectInfo& redirect_info,
                          bool* defer_redirect) override;

  virtual int Read(net::IOBuffer* buf, int buf_size);
  void Cancel();

  net::URLRequest* request() const { return request_.get(); }

 protected:
  std::unique_ptr<net::URLRequest> request_;

  DevToolsURLInterceptorRequestJob*
      devtools_interceptor_request_job_;  // NOT OWNED.

  DevToolsURLRequestInterceptor* const interceptor_;
  bool was_cancelled_;
};

DevToolsURLInterceptorRequestJob::SubRequest::SubRequest(
    DevToolsURLInterceptorRequestJob::RequestDetails& request_details,
    DevToolsURLInterceptorRequestJob* devtools_interceptor_request_job,
    DevToolsURLRequestInterceptor* interceptor)
    : devtools_interceptor_request_job_(devtools_interceptor_request_job),
      interceptor_(interceptor),
      was_cancelled_(false) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("devtools_interceptor", R"(
        semantics {
          sender: "Developer Tools"
          description:
            "When user is debugging a page, all actions resulting in a network "
            "request are intercepted to enrich the debugging experience."
          trigger:
            "User triggers an action that requires network request (like "
            "navigation, download, etc.) while debugging the page."
          data:
            "Any data that user action sends."
          destination: WEBSITE
        }
        policy {
          cookies_allowed: YES
          cookies_store: "user"
          setting:
            "This feature cannot be disabled in settings, however it happens "
            "only when user is debugging a page."
          chrome_policy {
            DeveloperToolsAvailability {
              DeveloperToolsAvailability: 2
            }
          }
        })");
  request_ = request_details.url_request_context->CreateRequest(
      request_details.url, request_details.priority, this, traffic_annotation);
  request_->set_method(request_details.method);
  request_->SetExtraRequestHeaders(request_details.extra_request_headers);
  request_->SetReferrer(request_details.referrer);
  request_->set_referrer_policy(request_details.referrer_policy);
  request_->SetRequestHeadersCallback(
      devtools_interceptor_request_job->request_headers_callback_);
  request_->SetResponseHeadersCallback(
      devtools_interceptor_request_job->response_headers_callback_);

  net::URLRequest* original_request =
      devtools_interceptor_request_job_->request();
  request_->set_attach_same_site_cookies(
      original_request->attach_same_site_cookies());
  request_->set_site_for_cookies(original_request->site_for_cookies());
  request_->set_initiator(original_request->initiator());

  // Mimic the ResourceRequestInfoImpl of the original request.
  const ResourceRequestInfoImpl* resource_request_info =
      static_cast<const ResourceRequestInfoImpl*>(
          ResourceRequestInfo::ForRequest(
              devtools_interceptor_request_job->request()));
  ResourceRequestInfoImpl* extra_data = new ResourceRequestInfoImpl(
      resource_request_info->requester_info(),
      resource_request_info->GetRouteID(),
      resource_request_info->GetFrameTreeNodeId(),
      resource_request_info->GetPluginChildID(),
      resource_request_info->GetRequestID(),
      resource_request_info->GetRenderFrameID(),
      resource_request_info->IsMainFrame(),
      resource_request_info->GetResourceType(),
      resource_request_info->GetPageTransition(),
      resource_request_info->IsDownload(), resource_request_info->is_stream(),
      resource_request_info->allow_download(),
      resource_request_info->HasUserGesture(),
      resource_request_info->is_load_timing_enabled(),
      resource_request_info->is_upload_progress_enabled(),
      resource_request_info->do_not_prompt_for_login(),
      resource_request_info->keepalive(),
      resource_request_info->GetReferrerPolicy(),
      resource_request_info->IsPrerendering(),
      resource_request_info->GetContext(),
      resource_request_info->ShouldReportRawHeaders(),
      resource_request_info->ShouldReportSecurityInfo(),
      resource_request_info->IsAsync(),
      resource_request_info->GetPreviewsState(), resource_request_info->body(),
      resource_request_info->initiated_in_secure_context());
  extra_data->AssociateWithRequest(request_.get());

  if (request_details.post_data)
    request_->set_upload(std::move(request_details.post_data));

  interceptor_->RegisterSubRequest(request_.get());
  request_->Start();
}

DevToolsURLInterceptorRequestJob::SubRequest::~SubRequest() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  interceptor_->UnregisterSubRequest(request_.get());
}

void DevToolsURLInterceptorRequestJob::SubRequest::Cancel() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (was_cancelled_)
    return;

  was_cancelled_ = true;
  request_->Cancel();
}

void DevToolsURLInterceptorRequestJob::SubRequest::OnAuthRequired(
    net::URLRequest* request,
    net::AuthChallengeInfo* auth_info) {
  devtools_interceptor_request_job_->OnSubRequestAuthRequired(auth_info);
}

void DevToolsURLInterceptorRequestJob::SubRequest::OnCertificateRequested(
    net::URLRequest* request,
    net::SSLCertRequestInfo* cert_request_info) {
  devtools_interceptor_request_job_->NotifyCertificateRequested(
      cert_request_info);
}

void DevToolsURLInterceptorRequestJob::SubRequest::OnSSLCertificateError(
    net::URLRequest* request,
    const net::SSLInfo& ssl_info,
    bool fatal) {
  devtools_interceptor_request_job_->NotifySSLCertificateError(ssl_info, fatal);
}

void DevToolsURLInterceptorRequestJob::SubRequest::OnResponseStarted(
    net::URLRequest* request,
    int net_error) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK_NE(net::ERR_IO_PENDING, net_error);
  devtools_interceptor_request_job_->OnSubRequestResponseStarted(
      static_cast<net::Error>(net_error));
}

void DevToolsURLInterceptorRequestJob::SubRequest::OnReadCompleted(
    net::URLRequest* request,
    int bytes_read) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK_NE(bytes_read, net::ERR_IO_PENDING);
  // OnReadCompleted may get called while canceling the subrequest, in that
  // event theres no need to call ReadRawDataComplete.
  if (!was_cancelled_)
    devtools_interceptor_request_job_->ReadRawDataComplete(bytes_read);
}

void DevToolsURLInterceptorRequestJob::SubRequest::OnReceivedRedirect(
    net::URLRequest* request,
    const net::RedirectInfo& redirectinfo,
    bool* defer_redirect) {
  devtools_interceptor_request_job_->OnSubRequestRedirectReceived(
      *request, redirectinfo, defer_redirect);
}

int DevToolsURLInterceptorRequestJob::SubRequest::Read(net::IOBuffer* buf,
                                                       int buf_size) {
  return request_->Read(buf, buf_size);
}

// DevToolsURLInterceptorRequestJob::InterceptedRequest ---------------------

class DevToolsURLInterceptorRequestJob::InterceptedRequest
    : public DevToolsURLInterceptorRequestJob::SubRequest {
 public:
  InterceptedRequest(
      DevToolsURLInterceptorRequestJob::RequestDetails& request_details,
      DevToolsURLInterceptorRequestJob* devtools_interceptor_request_job,
      DevToolsURLRequestInterceptor* interceptor);
  ~InterceptedRequest() override {}

  // net::URLRequest::Delegate methods.
  void OnResponseStarted(net::URLRequest* request, int net_error) override;
  void OnReadCompleted(net::URLRequest* request, int bytes_read) override;

  int Read(net::IOBuffer* buf, int buf_size) override;

  // Can only call FetchResponseBody() after OnInterceptedRequestResponseStarted
  // has been fired and before call to Read().
  void FetchResponseBody();

 private:
  // |this| may be deleted if this method returns false.
  bool ProcessChunkRead(int result);
  void ReadIntoBuffer();

  scoped_refptr<net::GrowableIOBuffer> response_buffer_;
  int read_response_result_;
  bool read_started_;
};

DevToolsURLInterceptorRequestJob::InterceptedRequest::InterceptedRequest(
    DevToolsURLInterceptorRequestJob::RequestDetails& request_details,
    DevToolsURLInterceptorRequestJob* devtools_interceptor_request_job,
    DevToolsURLRequestInterceptor* interceptor)
    : SubRequest(request_details,
                 devtools_interceptor_request_job,
                 interceptor),
      response_buffer_(base::MakeRefCounted<net::GrowableIOBuffer>()),
      read_response_result_(0),
      read_started_(false) {}

void DevToolsURLInterceptorRequestJob::InterceptedRequest::OnResponseStarted(
    net::URLRequest* request,
    int net_error) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK_NE(net::ERR_IO_PENDING, net_error);

  if (net_error != net::OK) {
    // If we have an error here, we cannot read the body, so we
    // flag it here as already read and set the result.
    DCHECK_EQ(read_response_result_, 0);
    read_started_ = true;
    read_response_result_ = net_error;
  }
  response_buffer_->SetCapacity(kInitialBufferSize);

  devtools_interceptor_request_job_->OnInterceptedRequestResponseStarted(
      static_cast<net::Error>(net_error));
}

void DevToolsURLInterceptorRequestJob::InterceptedRequest::OnReadCompleted(
    net::URLRequest* request,
    int result) {
  // OnReadComplete may be called while request is being cancelled, in this
  // event the result should be |net::ERR_ABORTED| which should complete any
  // |pending_body_requests_|.
  if (ProcessChunkRead(result))
    ReadIntoBuffer();
}

bool DevToolsURLInterceptorRequestJob::InterceptedRequest::ProcessChunkRead(
    int result) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK_NE(result, net::ERR_IO_PENDING);
  if (result == 0) {
    read_response_result_ = response_buffer_->offset();
    // Response is done so, reset buffer to zero so it can be read from the
    // beginning as an IOBuffer.
    response_buffer_->set_offset(0);
  } else if (result < 0) {
    read_response_result_ = result;
  } else {
    response_buffer_->set_offset(response_buffer_->offset() + result);
  }

  if (response_buffer_->offset() > kMaxBufferSize) {
    response_buffer_->SetCapacity(0);
    read_response_result_ = net::ERR_FILE_TOO_BIG;
  }
  if (read_response_result_ != net::ERR_IO_PENDING) {
    devtools_interceptor_request_job_->OnInterceptedRequestResponseReady(
        *response_buffer_.get(), read_response_result_);
    return false;
  }
  return true;
}

int DevToolsURLInterceptorRequestJob::InterceptedRequest::Read(
    net::IOBuffer* buf,
    int buf_size) {
  DCHECK(read_started_);
  DCHECK_NE(read_response_result_, net::ERR_IO_PENDING);
  if (read_response_result_ <= 0)
    return read_response_result_;
  int read_size = std::min(buf_size, read_response_result_);
  std::memcpy(buf->data(), response_buffer_->data(), read_size);
  response_buffer_->set_offset(response_buffer_->offset() + read_size);
  read_response_result_ -= read_size;
  return read_size;
}

void DevToolsURLInterceptorRequestJob::InterceptedRequest::FetchResponseBody() {
  if (read_started_) {
    if (read_response_result_ != net::ERR_IO_PENDING) {
      devtools_interceptor_request_job_->OnInterceptedRequestResponseReady(
          *response_buffer_.get(), read_response_result_);
    }
    return;
  }
  if (was_cancelled_) {
    // Cannot request body on cancelled request.
    devtools_interceptor_request_job_->OnInterceptedRequestResponseReady(
        *response_buffer_.get(), net::ERR_ABORTED);
    return;
  }
  read_started_ = true;
  read_response_result_ = net::ERR_IO_PENDING;
  ReadIntoBuffer();
}

void DevToolsURLInterceptorRequestJob::InterceptedRequest::ReadIntoBuffer() {
  // OnReadCompleted may get called while canceling the subrequest, in that
  // event we cannot call URLRequest::Read().
  DCHECK(!was_cancelled_);
  int result;
  do {
    if (response_buffer_->RemainingCapacity() == 0)
      response_buffer_->SetCapacity(response_buffer_->capacity() * 2);
    result = request_->Read(response_buffer_.get(),
                            response_buffer_->RemainingCapacity());
  } while (result != net::ERR_IO_PENDING && ProcessChunkRead(result));
}

class DevToolsURLInterceptorRequestJob::MockResponseDetails {
 public:
  MockResponseDetails(scoped_refptr<net::HttpResponseHeaders> response_headers,
                      std::string response_bytes);

  ~MockResponseDetails();

  scoped_refptr<net::HttpResponseHeaders>& response_headers() {
    return response_headers_;
  }

  base::TimeTicks response_time() const { return response_time_; }

  int ReadRawData(net::IOBuffer* buf, int buf_size);

 private:
  scoped_refptr<net::HttpResponseHeaders> response_headers_;
  std::string response_bytes_;
  size_t read_offset_;
  base::TimeTicks response_time_;
};

DevToolsURLInterceptorRequestJob::MockResponseDetails::MockResponseDetails(
    scoped_refptr<net::HttpResponseHeaders> response_headers,
    std::string response_bytes)
    : response_headers_(std::move(response_headers)),
      response_bytes_(std::move(response_bytes)),
      read_offset_(0),
      response_time_(base::TimeTicks::Now()) {
  if (!response_headers) {
    static const char kDummyHeaders[] = "HTTP/1.1 200 OK\0\0";
    response_headers =
        base::MakeRefCounted<net::HttpResponseHeaders>(kDummyHeaders);
  }
}

DevToolsURLInterceptorRequestJob::MockResponseDetails::~MockResponseDetails() {}

int DevToolsURLInterceptorRequestJob::MockResponseDetails::ReadRawData(
    net::IOBuffer* buf,
    int buf_size) {
  size_t bytes_available = response_bytes_.size() - read_offset_;
  size_t bytes_to_copy =
      std::min(static_cast<size_t>(buf_size), bytes_available);
  if (bytes_to_copy > 0) {
    std::memcpy(buf->data(), &response_bytes_.data()[read_offset_],
                bytes_to_copy);
    read_offset_ += bytes_to_copy;
  }
  return bytes_to_copy;
}

namespace {

void SendPendingBodyRequestsOnUiThread(
    std::vector<std::unique_ptr<
        protocol::Network::Backend::GetResponseBodyForInterceptionCallback>>
        callbacks,
    std::string content) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  std::string encoded_response;
  base::Base64Encode(content, &encoded_response);
  for (auto&& callback : callbacks)
    callback->sendSuccess(encoded_response, true);
}

void SendPendingBodyRequestsWithErrorOnUiThread(
    std::vector<std::unique_ptr<
        protocol::Network::Backend::GetResponseBodyForInterceptionCallback>>
        callbacks,
    protocol::DispatchResponse error) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  for (auto&& callback : callbacks)
    callback->sendFailure(error);
}

class ProxyUploadElementReader : public net::UploadElementReader {
 public:
  explicit ProxyUploadElementReader(net::UploadElementReader* reader)
      : reader_(reader) {}

  ~ProxyUploadElementReader() override {}

  // net::UploadElementReader overrides:
  int Init(net::CompletionOnceCallback callback) override {
    return reader_->Init(std::move(callback));
  }

  uint64_t GetContentLength() const override {
    return reader_->GetContentLength();
  }

  uint64_t BytesRemaining() const override { return reader_->BytesRemaining(); }

  bool IsInMemory() const override { return reader_->IsInMemory(); }

  int Read(net::IOBuffer* buf,
           int buf_length,
           net::CompletionOnceCallback callback) override {
    return reader_->Read(buf, buf_length, std::move(callback));
  }

 private:
  net::UploadElementReader* reader_;  // NOT OWNED

  DISALLOW_COPY_AND_ASSIGN(ProxyUploadElementReader);
};

std::unique_ptr<net::UploadDataStream> GetUploadData(net::URLRequest* request) {
  if (!request->has_upload())
    return nullptr;

  const net::UploadDataStream* stream = request->get_upload();
  auto* readers = stream->GetElementReaders();
  if (!readers || readers->empty())
    return nullptr;

  std::vector<std::unique_ptr<net::UploadElementReader>> proxy_readers;
  proxy_readers.reserve(readers->size());
  for (auto& reader : *readers) {
    proxy_readers.push_back(
        std::make_unique<ProxyUploadElementReader>(reader.get()));
  }

  return std::make_unique<net::ElementsUploadDataStream>(
      std::move(proxy_readers), 0);
}

bool IsDownload(net::URLRequest* orig_request, net::URLRequest* subrequest) {
  auto* req_info = ResourceRequestInfoImpl::ForRequest(orig_request);
  // Only happens to downloads that are initiated by the download manager.
  if (req_info->IsDownload())
    return true;

  // Note this will not correctly identify a download for the MIME types
  // inferred with content sniffing. The new interception implementation
  // should not have this problem, as it's on top of MIME sniffer.
  std::string mime_type;
  subrequest->GetMimeType(&mime_type);
  return req_info->allow_download() &&
         download_utils::IsDownload(orig_request->url(),
                                    subrequest->response_headers(), mime_type);
}

}  // namespace

DevToolsURLInterceptorRequestJob::DevToolsURLInterceptorRequestJob(
    DevToolsURLRequestInterceptor* interceptor,
    const std::string& interception_id,
    intptr_t owning_entry_id,
    net::URLRequest* original_request,
    net::NetworkDelegate* original_network_delegate,
    const base::UnguessableToken& devtools_token,
    DevToolsNetworkInterceptor::RequestInterceptedCallback callback,
    ResourceType resource_type,
    InterceptionStage stage_to_intercept)
    : net::URLRequestJob(original_request, original_network_delegate),
      interceptor_(interceptor),
      request_details_(original_request->url(),
                       original_request->method(),
                       GetUploadData(original_request),
                       original_request->extra_request_headers(),
                       original_request->referrer(),
                       original_request->referrer_policy(),
                       original_request->priority(),
                       original_request->context()),
      waiting_for_user_response_(WaitingForUserResponse::NOT_WAITING),
      interception_id_(interception_id),
      owning_entry_id_(owning_entry_id),
      devtools_token_(devtools_token),
      callback_(callback),
      resource_type_(resource_type),
      stage_to_intercept_(stage_to_intercept),
      weak_ptr_factory_(this) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
}

DevToolsURLInterceptorRequestJob::~DevToolsURLInterceptorRequestJob() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  interceptor_->JobFinished(
      interception_id_,
      DevToolsURLRequestInterceptor::IsNavigationRequest(resource_type_));
}

// net::URLRequestJob implementation:
void DevToolsURLInterceptorRequestJob::SetExtraRequestHeaders(
    const net::HttpRequestHeaders& headers) {
  request_details_.extra_request_headers = headers;
}

void DevToolsURLInterceptorRequestJob::Start() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  auto* store = request_details_.url_request_context->cookie_store();
  if (!store || (request()->load_flags() & net::LOAD_DO_NOT_SEND_COOKIES)) {
    StartWithCookies(net::CookieList());
    return;
  }

  net::CookieOptions options;
  options.set_include_httponly();
  // The below is a copy of the logic in URLRequestHttpJob

  // Set SameSiteCookieMode according to the rules laid out in
  // https://tools.ietf.org/html/draft-ietf-httpbis-cookie-same-site:
  //
  // * Include both "strict" and "lax" same-site cookies if the request's
  //   |url|, |initiator|, and |site_for_cookies| all have the same
  //   registrable domain. Note: this also covers the case of a request
  //   without an initiator (only happens for browser-initiated main frame
  //   navigations).
  //
  // * Include only "lax" same-site cookies if the request's |URL| and
  //   |site_for_cookies| have the same registrable domain, _and_ the
  //   request's |method| is "safe" ("GET" or "HEAD").
  //
  //   Note that this will generally be the case only for cross-site requests
  //   which target a top-level browsing context.
  //
  // * Include both "strict" and "lax" same-site cookies if the request is
  //   tagged with a flag allowing it.
  //   Note that this can be the case for requests initiated by extensions,
  //   which need to behave as though they are made by the document itself,
  //   but appear like cross-site ones.
  //
  // * Otherwise, do not include same-site cookies.
  using namespace net::registry_controlled_domains;
  if (SameDomainOrHost(request()->url(), request()->site_for_cookies(),
                       INCLUDE_PRIVATE_REGISTRIES)) {
    if (!request()->initiator() ||
        SameDomainOrHost(request()->url(),
                         request()->initiator().value().GetURL(),
                         INCLUDE_PRIVATE_REGISTRIES) ||
        request()->attach_same_site_cookies()) {
      options.set_same_site_cookie_mode(
          net::CookieOptions::SameSiteCookieMode::INCLUDE_STRICT_AND_LAX);
    } else if (net::HttpUtil::IsMethodSafe(request()->method())) {
      options.set_same_site_cookie_mode(
          net::CookieOptions::SameSiteCookieMode::INCLUDE_LAX);
    }
  }

  store->GetCookieListWithOptionsAsync(
      request_details_.url, options,
      base::BindOnce(&DevToolsURLInterceptorRequestJob::StartWithCookies,
                     weak_ptr_factory_.GetWeakPtr()));
}

void DevToolsURLInterceptorRequestJob::StartWithCookies(
    const net::CookieList& cookie_list) {
  request_details_.cookie_line =
      net::CanonicalCookie::BuildCookieLine(cookie_list);

  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (stage_to_intercept_ == InterceptionStage::DONT_INTERCEPT) {
    sub_request_.reset(new SubRequest(request_details_, this, interceptor_));
    return;
  }

  if (stage_to_intercept_ == InterceptionStage::RESPONSE) {
    // We are only a response interception, we go right to dispatching the
    // request.
    sub_request_.reset(
        new InterceptedRequest(request_details_, this, interceptor_));
    return;
  }

  DCHECK(stage_to_intercept_ == InterceptionStage::REQUEST ||
         stage_to_intercept_ == InterceptionStage::BOTH);
  waiting_for_user_response_ = WaitingForUserResponse::WAITING_FOR_REQUEST_ACK;
  base::PostTaskWithTraits(FROM_HERE, {BrowserThread::UI},
                           base::BindOnce(callback_, BuildRequestInfo()));
}

void DevToolsURLInterceptorRequestJob::Kill() {
  sub_request_.reset();
  URLRequestJob::Kill();
}

int DevToolsURLInterceptorRequestJob::ReadRawData(net::IOBuffer* buf,
                                                  int buf_size) {
  if (mock_response_details_)
    return mock_response_details_->ReadRawData(buf, buf_size);

  CHECK(sub_request_);
  return sub_request_->Read(buf, buf_size);
}

int DevToolsURLInterceptorRequestJob::GetResponseCode() const {
  if (sub_request_) {
    return sub_request_->request()->GetResponseCode();
  } else {
    CHECK(mock_response_details_);
    return mock_response_details_->response_headers()->response_code();
  }
}

void DevToolsURLInterceptorRequestJob::GetResponseInfo(
    net::HttpResponseInfo* info) {
  // NOTE this can get called during URLRequestJob::NotifyStartError in which
  // case we might not have either a sub request or a mock response.
  if (sub_request_) {
    *info = sub_request_->request()->response_info();
  } else if (mock_response_details_) {
    info->headers = mock_response_details_->response_headers();
  }
}

const net::HttpResponseHeaders*
DevToolsURLInterceptorRequestJob::GetHttpResponseHeaders() const {
  if (sub_request_) {
    net::URLRequest* request = sub_request_->request();
    return request->response_info().headers.get();
  }
  CHECK(mock_response_details_);
  return mock_response_details_->response_headers().get();
}

bool DevToolsURLInterceptorRequestJob::GetMimeType(
    std::string* mime_type) const {
  if (sub_request_) {
    sub_request_->request()->GetMimeType(mime_type);
    return true;
  }
  const net::HttpResponseHeaders* response_headers = GetHttpResponseHeaders();
  if (response_headers)
    return response_headers->GetMimeType(mime_type);
  return false;
}

bool DevToolsURLInterceptorRequestJob::GetCharset(std::string* charset) {
  const net::HttpResponseHeaders* response_headers = GetHttpResponseHeaders();
  if (!response_headers)
    return false;
  return response_headers->GetCharset(charset);
}

void DevToolsURLInterceptorRequestJob::GetLoadTimingInfo(
    net::LoadTimingInfo* load_timing_info) const {
  if (sub_request_) {
    sub_request_->request()->GetLoadTimingInfo(load_timing_info);
  } else {
    CHECK(mock_response_details_);
    // Since this request is mocked most of the fields are irrelevant.
    load_timing_info->receive_headers_end =
        mock_response_details_->response_time();
  }
}

bool DevToolsURLInterceptorRequestJob::NeedsAuth() {
  return !!auth_info_;
}

void DevToolsURLInterceptorRequestJob::GetAuthChallengeInfo(
    scoped_refptr<net::AuthChallengeInfo>* auth_info) {
  *auth_info = auth_info_.get();
}

void DevToolsURLInterceptorRequestJob::SetAuth(
    const net::AuthCredentials& credentials) {
  sub_request_->request()->SetAuth(credentials);
  auth_info_ = nullptr;
}

void DevToolsURLInterceptorRequestJob::CancelAuth() {
  sub_request_->request()->CancelAuth();
  auth_info_ = nullptr;
}

void DevToolsURLInterceptorRequestJob::OnSubRequestAuthRequired(
    net::AuthChallengeInfo* auth_info) {
  auth_info_ = auth_info;

  if (stage_to_intercept_ == InterceptionStage::DONT_INTERCEPT) {
    // This should trigger default auth behavior.
    // See comment in ProcessAuthResponse.
    NotifyHeadersComplete();
    return;
  }

  // This notification came from the sub requests URLRequest::Delegate and
  // depending on what the protocol user wants us to do we must either cancel
  // the auth, provide the credentials or proxy it the original
  // URLRequest::Delegate.

  waiting_for_user_response_ = WaitingForUserResponse::WAITING_FOR_AUTH_ACK;

  std::unique_ptr<InterceptedRequestInfo> request_info = BuildRequestInfo();
  request_info->auth_challenge = auth_info;
  base::PostTaskWithTraits(FROM_HERE, {BrowserThread::UI},
                           base::BindOnce(callback_, std::move(request_info)));
}

void DevToolsURLInterceptorRequestJob::OnSubRequestResponseStarted(
    const net::Error& net_error) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (net_error != net::OK) {
    sub_request_->Cancel();
    NotifyStartError(
        net::URLRequestStatus(net::URLRequestStatus::FAILED, net_error));
    return;
  }

  NotifyHeadersComplete();
}

void DevToolsURLInterceptorRequestJob::OnSubRequestRedirectReceived(
    const net::URLRequest& request,
    const net::RedirectInfo& redirectinfo,
    bool* defer_redirect) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(sub_request_);

  // If we're not intercepting results or are a response then cancel this
  // redirect and tell the parent request it was redirected through |redirect_|.
  if (!(stage_to_intercept_ & InterceptionStage::RESPONSE)) {
    *defer_redirect = false;
    ProcessRedirect(redirectinfo.status_code, redirectinfo.new_url.spec());
    redirect_.reset();
    sub_request_.reset();
    return;
  }

  // Otherwise we will need to ask what to do via DevTools protocol.
  *defer_redirect = true;

  redirect_.reset(new net::RedirectInfo(redirectinfo));

  waiting_for_user_response_ = WaitingForUserResponse::WAITING_FOR_REQUEST_ACK;

  std::unique_ptr<InterceptedRequestInfo> request_info = BuildRequestInfo();
  request_info->response_headers = request.response_headers();
  request_info->redirect_url = redirectinfo.new_url.spec();
  base::PostTaskWithTraits(FROM_HERE, {BrowserThread::UI},
                           base::BindOnce(callback_, std::move(request_info)));
  sub_request_.reset();
}

void DevToolsURLInterceptorRequestJob::OnInterceptedRequestResponseStarted(
    const net::Error& net_error) {
  DCHECK_NE(waiting_for_user_response_,
            WaitingForUserResponse::WAITING_FOR_RESPONSE_ACK);
  if (stage_to_intercept_ == InterceptionStage::DONT_INTERCEPT) {
    static_cast<InterceptedRequest*>(sub_request_.get())->FetchResponseBody();
    return;
  }
  waiting_for_user_response_ = WaitingForUserResponse::WAITING_FOR_RESPONSE_ACK;

  std::unique_ptr<InterceptedRequestInfo> request_info = BuildRequestInfo();
  if (net_error < 0) {
    request_info->response_error_code = net_error;
  } else {
    std::unique_ptr<protocol::DictionaryValue> headers_dict(
        protocol::DictionaryValue::create());
    request_info->response_headers =
        sub_request_->request()->response_headers();
    request_info->is_download = IsDownload(request(), sub_request_->request());
  }
  base::PostTaskWithTraits(FROM_HERE, {BrowserThread::UI},
                           base::BindOnce(callback_, std::move(request_info)));
}

// If result is < 0 it means error.
void DevToolsURLInterceptorRequestJob::OnInterceptedRequestResponseReady(
    const net::IOBuffer& buf,
    int result) {
  DCHECK(sub_request_);
  if (result < 0) {
    sub_request_->Cancel();
    base::PostTaskWithTraits(
        FROM_HERE, {BrowserThread::UI},
        base::BindOnce(
            &SendPendingBodyRequestsWithErrorOnUiThread,
            std::move(pending_body_requests_),
            protocol::Response::Error(base::StringPrintf(
                "Could not get response body because of error code: %d",
                result))));
  } else {
    base::PostTaskWithTraits(FROM_HERE, {BrowserThread::UI},
                             base::BindOnce(&SendPendingBodyRequestsOnUiThread,
                                            std::move(pending_body_requests_),
                                            std::string(buf.data(), result)));
  }
  if (request_->status().status() == net::URLRequestStatus::CANCELED ||
      waiting_for_user_response_ != WaitingForUserResponse::NOT_WAITING) {
    return;
  }
  if (result < 0) {
    NotifyStartError(net::URLRequestStatus::FromError(result));
  } else {
    // This call may consume the buffer.
    NotifyHeadersComplete();
  }
}

void DevToolsURLInterceptorRequestJob::StopIntercepting() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  stage_to_intercept_ = InterceptionStage::DONT_INTERCEPT;
  callback_.Reset();

  // Allow the request to continue if we're waiting for user input.
  switch (waiting_for_user_response_) {
    case WaitingForUserResponse::NOT_WAITING:
      return;

    case WaitingForUserResponse::WAITING_FOR_RESPONSE_ACK:
    // Fallthough.
    case WaitingForUserResponse::WAITING_FOR_REQUEST_ACK:
      ProcessInterceptionResponse(
          std::make_unique<DevToolsNetworkInterceptor::Modifications>());
      return;
    case WaitingForUserResponse::WAITING_FOR_AUTH_ACK:
      ProcessAuthResponse(DevToolsNetworkInterceptor::AuthChallengeResponse(
          DevToolsNetworkInterceptor::AuthChallengeResponse::kDefault));
      return;

    default:
      NOTREACHED();
      return;
  }
}

void DevToolsURLInterceptorRequestJob::ContinueInterceptedRequest(
    std::unique_ptr<DevToolsNetworkInterceptor::Modifications> modifications,
    std::unique_ptr<ContinueInterceptedRequestCallback> callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  switch (waiting_for_user_response_) {
    case WaitingForUserResponse::NOT_WAITING:
      base::PostTaskWithTraits(
          FROM_HERE, {BrowserThread::UI},
          base::BindOnce(&ContinueInterceptedRequestCallback::sendFailure,
                         std::move(callback),
                         protocol::Response::InvalidParams(
                             "Response already processed.")));
      break;

    case WaitingForUserResponse::WAITING_FOR_RESPONSE_ACK:
    // Fallthough.
    case WaitingForUserResponse::WAITING_FOR_REQUEST_ACK:
      if (modifications->auth_challenge_response) {
        base::PostTaskWithTraits(
            FROM_HERE, {BrowserThread::UI},
            base::BindOnce(&ContinueInterceptedRequestCallback::sendFailure,
                           std::move(callback),
                           protocol::Response::InvalidParams(
                               "authChallengeResponse not expected.")));
        break;
      }
      ProcessInterceptionResponse(std::move(modifications));
      base::PostTaskWithTraits(
          FROM_HERE, {BrowserThread::UI},
          base::BindOnce(&ContinueInterceptedRequestCallback::sendSuccess,
                         std::move(callback)));
      break;

    case WaitingForUserResponse::WAITING_FOR_AUTH_ACK:
      if (!modifications->auth_challenge_response) {
        base::PostTaskWithTraits(
            FROM_HERE, {BrowserThread::UI},
            base::BindOnce(&ContinueInterceptedRequestCallback::sendFailure,
                           std::move(callback),
                           protocol::Response::InvalidParams(
                               "authChallengeResponse required.")));
        break;
      }
      ProcessAuthResponse(*modifications->auth_challenge_response);
      base::PostTaskWithTraits(
          FROM_HERE, {BrowserThread::UI},
          base::BindOnce(&ContinueInterceptedRequestCallback::sendSuccess,
                         std::move(callback)));
      break;

    default:
      NOTREACHED();
      break;
  }
}

void DevToolsURLInterceptorRequestJob::ProcessRedirect(
    int status_code,
    const std::string& new_url) {
  // NOTE we don't append the text form of the status code because
  // net::HttpResponseHeaders doesn't need that.
  std::string raw_headers = base::StringPrintf("HTTP/1.1 %d", status_code);
  raw_headers.append(1, '\0');
  raw_headers.append("Location: ");
  raw_headers.append(new_url);
  raw_headers.append(2, '\0');
  mock_response_details_ = std::make_unique<MockResponseDetails>(
      base::MakeRefCounted<net::HttpResponseHeaders>(raw_headers), "");

  NotifyHeadersComplete();
}

void DevToolsURLInterceptorRequestJob::GetResponseBody(
    std::unique_ptr<GetResponseBodyForInterceptionCallback> callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  std::string error_reason;
  if (stage_to_intercept_ == InterceptionStage::REQUEST) {
    error_reason =
        "Can only get response body on HeadersReceived pattern matched "
        "requests.";
  } else if (waiting_for_user_response_ !=
             WaitingForUserResponse::WAITING_FOR_RESPONSE_ACK) {
    error_reason =
        "Can only get response body on requests captured after headers "
        "received.";
  }
  if (error_reason.size()) {
    base::PostTaskWithTraits(
        FROM_HERE, {BrowserThread::UI},
        base::BindOnce(
            &GetResponseBodyForInterceptionCallback::sendFailure,
            std::move(callback),
            protocol::Response::InvalidParams(std::move(error_reason))));
    return;
  }
  DCHECK(sub_request_);
  pending_body_requests_.push_back(std::move(callback));
  static_cast<InterceptedRequest*>(sub_request_.get())->FetchResponseBody();
}

std::unique_ptr<InterceptedRequestInfo>
DevToolsURLInterceptorRequestJob::BuildRequestInfo() {
  auto result = std::make_unique<InterceptedRequestInfo>();
  result->interception_id = interception_id_;
  result->network_request =
      protocol::NetworkHandler::CreateRequestFromURLRequest(
          request(), request_details_.cookie_line);
  result->frame_id = devtools_token_;
  result->resource_type = resource_type_;
  result->is_navigation =
      DevToolsURLRequestInterceptor::IsNavigationRequest(resource_type_);
  return result;
}

void DevToolsURLInterceptorRequestJob::ProcessInterceptionResponse(
    std::unique_ptr<DevToolsNetworkInterceptor::Modifications> modifications) {
  bool is_response_ack = waiting_for_user_response_ ==
                         WaitingForUserResponse::WAITING_FOR_RESPONSE_ACK;
  waiting_for_user_response_ = WaitingForUserResponse::NOT_WAITING;

  if (modifications->error_reason) {
    if (sub_request_) {
      sub_request_->Cancel();
      sub_request_.reset();
    }
    if (modifications->error_reason == net::ERR_BLOCKED_BY_CLIENT) {
      // So we know that these modifications originated from devtools
      // (also known as inspector), and can therefore annotate the
      // request. We only do this for one specific error code thus
      // far, to minimize risk of breaking other usages.
      ResourceRequestInfoImpl* resource_request_info =
          ResourceRequestInfoImpl::ForRequest(request());
      resource_request_info->SetResourceRequestBlockedReason(
          blink::ResourceRequestBlockedReason::kInspector);
    }
    NotifyStartError(net::URLRequestStatus(net::URLRequestStatus::FAILED,
                                           *modifications->error_reason));
    return;
  }

  if (modifications->response_headers || modifications->response_body) {
    mock_response_details_ = std::make_unique<MockResponseDetails>(
        std::move(modifications->response_headers),
        modifications->response_body ? std::move(*modifications->response_body)
                                     : "");

    // Set cookies in the network stack.
    net::CookieOptions options;
    options.set_include_httponly();
    base::Time response_date;
    if (!mock_response_details_->response_headers()->GetDateValue(
            &response_date)) {
      response_date = base::Time();
    }
    options.set_server_time(response_date);

    const base::StringPiece name("Set-Cookie");
    std::string cookie_line;
    size_t iter = 0;
    while (mock_response_details_->response_headers()->EnumerateHeader(
        &iter, name, &cookie_line)) {
      std::unique_ptr<net::CanonicalCookie> cookie =
          net::CanonicalCookie::Create(request_details_.url, cookie_line,
                                       base::Time::Now(), options);
      if (!cookie)
        continue;

      auto* store = request_details_.url_request_context->cookie_store();
      store->SetCanonicalCookieAsync(
          std::move(cookie), request_details_.url.SchemeIsCryptographic(),
          !options.exclude_httponly(), net::CookieStore::SetCookiesCallback());
    }

    if (sub_request_) {
      sub_request_->Cancel();
      sub_request_.reset();
    }
    if (response_headers_callback_) {
      response_headers_callback_.Run(
          mock_response_details_->response_headers());
    }
    NotifyHeadersComplete();
    return;
  }

  if (redirect_) {
    DCHECK(!is_response_ack);
    ProcessRedirect(
        redirect_->status_code,
        modifications->modified_url.fromMaybe(redirect_->new_url.spec()));
    redirect_.reset();
  } else if (is_response_ack) {
    DCHECK(sub_request_);
    // If we are continuing the request without change we fetch the body.
    // If the body is already ready we will get a
    // OnInterceptedRequestResponseReady event which will begin the read.
    static_cast<InterceptedRequest*>(sub_request_.get())->FetchResponseBody();
  } else {
    // Note this redirect is not visible to the caller by design. If they want a
    // visible redirect they can mock a response with a 302.
    if (modifications->modified_url.isJust())
      request_details_.url = GURL(modifications->modified_url.fromJust());

    if (modifications->modified_method.isJust())
      request_details_.method = modifications->modified_method.fromJust();

    if (modifications->modified_post_data.isJust()) {
      const std::string& post_data =
          modifications->modified_post_data.fromJust();
      std::vector<char> data(post_data.begin(), post_data.end());
      request_details_.post_data =
          net::ElementsUploadDataStream::CreateWithReader(
              std::make_unique<net::UploadOwnedBytesElementReader>(&data), 0);
    }

    if (modifications->modified_headers) {
      request_details_.extra_request_headers.Clear();
      for (const auto& entry : *modifications->modified_headers) {
        if (base::EqualsCaseInsensitiveASCII(
                entry.first, net::HttpRequestHeaders::kReferer)) {
          request_details_.referrer = entry.second;
          request_details_.referrer_policy =
              net::URLRequest::NEVER_CLEAR_REFERRER;
        } else {
          request_details_.extra_request_headers.SetHeader(entry.first,
                                                           entry.second);
        }
      }
    }

    // The reason we start a sub request is because we are in full control of it
    // and can choose to ignore it if, for example, the fetch encounters a
    // redirect that the user chooses to replace with a mock response.
    DCHECK(stage_to_intercept_ != InterceptionStage::RESPONSE);
    if (stage_to_intercept_ == InterceptionStage::BOTH) {
      sub_request_.reset(
          new InterceptedRequest(request_details_, this, interceptor_));
    } else {
      sub_request_.reset(new SubRequest(request_details_, this, interceptor_));
    }
  }
}

void DevToolsURLInterceptorRequestJob::ProcessAuthResponse(
    const DevToolsNetworkInterceptor::AuthChallengeResponse& response) {
  waiting_for_user_response_ = WaitingForUserResponse::NOT_WAITING;

  switch (response.response_type) {
    case DevToolsNetworkInterceptor::AuthChallengeResponse::kDefault:
      // The user wants the default behavior, we must proxy the auth request to
      // the original URLRequest::Delegate.  We can't do that directly but by
      // implementing NeedsAuth and calling NotifyHeadersComplete we trigger it.
      // To close the loop we also need to implement GetAuthChallengeInfo,
      // SetAuth and CancelAuth.
      NotifyHeadersComplete();
      break;
    case DevToolsNetworkInterceptor::AuthChallengeResponse::kCancelAuth:
      CancelAuth();
      break;
    case DevToolsNetworkInterceptor::AuthChallengeResponse::kProvideCredentials:
      SetAuth(response.credentials);
      break;
  }
}

void DevToolsURLInterceptorRequestJob::SetRequestHeadersCallback(
    net::RequestHeadersCallback callback) {
  request_headers_callback_ = std::move(callback);
}

void DevToolsURLInterceptorRequestJob::SetResponseHeadersCallback(
    net::ResponseHeadersCallback callback) {
  response_headers_callback_ = std::move(callback);
}

void DevToolsURLInterceptorRequestJob::ContinueDespiteLastError() {
  if (sub_request_)
    sub_request_->request()->ContinueDespiteLastError();
}

DevToolsURLInterceptorRequestJob::RequestDetails::RequestDetails(
    const GURL& url,
    const std::string& method,
    std::unique_ptr<net::UploadDataStream> post_data,
    const net::HttpRequestHeaders& extra_request_headers,
    const std::string& referrer,
    net::URLRequest::ReferrerPolicy referrer_policy,
    const net::RequestPriority& priority,
    const net::URLRequestContext* url_request_context)
    : url(url),
      method(method),
      post_data(std::move(post_data)),
      extra_request_headers(extra_request_headers),
      referrer(referrer),
      referrer_policy(referrer_policy),
      priority(priority),
      url_request_context(url_request_context) {}

DevToolsURLInterceptorRequestJob::RequestDetails::~RequestDetails() {}

}  // namespace content
