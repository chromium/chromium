// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webui/url_data_manager_backend.h"

#include <set>
#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/memory/ref_counted_memory.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/task/post_task.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/trace_event.h"
#include "content/browser/blob_storage/chrome_blob_storage_context.h"
#include "content/browser/net/view_blob_internals_job_factory.h"
#include "content/browser/resource_context_impl.h"
#include "content/browser/webui/shared_resources_data_source.h"
#include "content/browser/webui/url_data_source_impl.h"
#include "content/browser/webui/web_ui_data_source_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/resource_request_info.h"
#include "content/public/common/url_constants.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/filter/gzip_source_stream.h"
#include "net/filter/source_stream.h"
#include "net/http/http_status_code.h"
#include "net/log/net_log_util.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_error_job.h"
#include "net/url_request/url_request_job.h"
#include "net/url_request/url_request_job_factory.h"
#include "ui/base/template_expressions.h"
#include "ui/base/webui/i18n_source_stream.h"
#include "url/url_util.h"

namespace content {

namespace {

const char kChromeURLContentSecurityPolicyHeaderBase[] =
    "Content-Security-Policy: ";

const char kChromeURLXFrameOptionsHeader[] = "X-Frame-Options: DENY";
const char kNetworkErrorKey[] = "netError";

bool SchemeIsInSchemes(const std::string& scheme,
                       const std::vector<std::string>& schemes) {
  return base::ContainsValue(schemes, scheme);
}

// Returns a value of 'Origin:' header for the |request| if the header is set.
// Otherwise returns an empty string.
std::string GetOriginHeaderValue(const net::URLRequest* request) {
  std::string result;
  if (request->extra_request_headers().GetHeader(
          net::HttpRequestHeaders::kOrigin, &result))
    return result;
  net::HttpRequestHeaders headers;
  if (request->GetFullRequestHeaders(&headers))
    headers.GetHeader(net::HttpRequestHeaders::kOrigin, &result);
  return result;
}

// Copy data from source buffer into IO buffer destination.
// TODO(groby): Very similar to URLRequestSimpleJob, unify at some point.
void CopyData(const scoped_refptr<net::IOBuffer>& buf,
              int buf_size,
              const scoped_refptr<base::RefCountedMemory>& data,
              int64_t data_offset) {
  memcpy(buf->data(), data->front() + data_offset, buf_size);
}

}  // namespace

// URLRequestChromeJob is a net::URLRequestJob that manages running
// chrome-internal resource requests asynchronously.
// It hands off URL requests to ChromeURLDataManager, which asynchronously
// calls back once the data is available.
class URLRequestChromeJob : public net::URLRequestJob {
 public:
  URLRequestChromeJob(net::URLRequest* request,
                      net::NetworkDelegate* network_delegate,
                      URLDataManagerBackend* backend);

  // net::URLRequestJob implementation.
  void Start() override;
  void Kill() override;
  int ReadRawData(net::IOBuffer* buf, int buf_size) override;
  bool GetMimeType(std::string* mime_type) const override;
  void GetResponseInfo(net::HttpResponseInfo* info) override;
  std::unique_ptr<net::SourceStream> SetUpSourceStream() override;

  // Used to notify that the requested data's |mime_type| is ready.
  void MimeTypeAvailable(const std::string& mime_type);

  // Called by ChromeURLDataManager to notify us that the data blob is ready
  // for us.  |bytes| may be null, indicating an error.
  void DataAvailable(base::RefCountedMemory* bytes);

  void set_is_gzipped(bool is_gzipped) {
    is_gzipped_ = is_gzipped;
  }

  void SetSource(scoped_refptr<URLDataSourceImpl> source) { source_ = source; }

 private:
  ~URLRequestChromeJob() override;

  // Helper for Start(), to let us start asynchronously.
  // (This pattern is shared by most net::URLRequestJob implementations.)
  void StartAsync();

  // Due to a race condition, DevTools relies on a legacy thread hop to the UI
  // thread before calling StartAsync.
  // TODO(caseq): Fix the race condition and remove this thread hop in
  // https://crbug.com/616641.
  static void DelayStartForDevTools(
      const base::WeakPtr<URLRequestChromeJob>& job);

  // Post a task to copy |data_| to |buf_| on a worker thread, to avoid browser
  // jank. (|data_| might be mem-mapped, so a memcpy can trigger file ops).
  int PostReadTask(scoped_refptr<net::IOBuffer> buf, int buf_size);

  // The actual data we're serving.  NULL until it's been fetched.
  scoped_refptr<base::RefCountedMemory> data_;

  // The current offset into the data that we're handing off to our
  // callers via the Read interfaces.
  int data_offset_;

  // When DataAvailable() is called with a null argument, indicating an error,
  // this is set accordingly to a code for ReadRawData() to return.
  net::Error data_available_status_;

  // For async reads, we keep around a pointer to the buffer that
  // we're reading into.
  scoped_refptr<net::IOBuffer> pending_buf_;
  int pending_buf_size_;
  std::string mime_type_;

  // True when gzip encoding should be used. NOTE: this requires the original
  // resources in resources.pak use compress="gzip".
  bool is_gzipped_;

  // The URLDataSourceImpl that is servicing this request. This is a shared
  // pointer so that the request can continue to be served even if the source is
  // detached from the backend that initially owned it.
  scoped_refptr<URLDataSourceImpl> source_;

  // The backend is owned by net::URLRequestContext and always outlives us.
  URLDataManagerBackend* const backend_;

  base::WeakPtrFactory<URLRequestChromeJob> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(URLRequestChromeJob);
};

URLRequestChromeJob::URLRequestChromeJob(net::URLRequest* request,
                                         net::NetworkDelegate* network_delegate,
                                         URLDataManagerBackend* backend)
    : net::URLRequestJob(request, network_delegate),
      data_offset_(0),
      data_available_status_(net::OK),
      pending_buf_size_(0),
      is_gzipped_(false),
      backend_(backend),
      weak_factory_(this) {
  DCHECK(backend);
}

URLRequestChromeJob::~URLRequestChromeJob() {
  CHECK(!backend_->HasPendingJob(this));
}

void URLRequestChromeJob::Start() {
  const GURL url = request_->url();

  // Due to a race condition, DevTools relies on a legacy thread hop to the UI
  // thread before calling StartAsync.
  // TODO(caseq): Fix the race condition and remove this thread hop in
  // https://crbug.com/616641.
  if (url.SchemeIs(kChromeDevToolsScheme)) {
    base::PostTaskWithTraits(
        FROM_HERE, {BrowserThread::UI},
        base::BindOnce(&URLRequestChromeJob::DelayStartForDevTools,
                       weak_factory_.GetWeakPtr()));
    return;
  }

  // Start reading asynchronously so that all error reporting and data
  // callbacks happen as they would for network requests.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&URLRequestChromeJob::StartAsync,
                                weak_factory_.GetWeakPtr()));

  TRACE_EVENT_ASYNC_BEGIN1("browser", "DataManager:Request", this, "URL",
      url.possibly_invalid_spec());
}

void URLRequestChromeJob::Kill() {
  weak_factory_.InvalidateWeakPtrs();
  backend_->RemoveRequest(this);
  URLRequestJob::Kill();
}

bool URLRequestChromeJob::GetMimeType(std::string* mime_type) const {
  *mime_type = mime_type_;
  return !mime_type_.empty();
}

void URLRequestChromeJob::GetResponseInfo(net::HttpResponseInfo* info) {
  DCHECK(!info->headers.get());
  URLDataSourceImpl* source = backend_->GetDataSourceFromURL(request()->url());
  std::string path;
  URLDataManagerBackend::URLToRequestPath(request()->url(), &path);
  info->headers = URLDataManagerBackend::GetHeaders(
      source, path, GetOriginHeaderValue(request()));
  if (is_gzipped_)
    info->headers->AddHeader("Content-Encoding: gzip");
}

std::unique_ptr<net::SourceStream> URLRequestChromeJob::SetUpSourceStream() {
  std::unique_ptr<net::SourceStream> source_stream =
      net::URLRequestJob::SetUpSourceStream();

  if (is_gzipped_) {
    source_stream = net::GzipSourceStream::Create(std::move(source_stream),
                                                  net::SourceStream::TYPE_GZIP);
  }

  // The URLRequestJob and the SourceStreams we are creating are owned by the
  // same parent URLRequest, thus it is safe to pass the replacements via a raw
  // pointer.
  const ui::TemplateReplacements* replacements = nullptr;
  if (source_)
    replacements = source_->GetReplacements();
  if (replacements) {
    // It is safe to pass the raw replacements directly to the source stream, as
    // both this URLRequestChromeJob and the I18nSourceStream are owned by the
    // same root URLRequest. The replacements are owned by the URLDataSourceImpl
    // which we keep alive via |source_|, ensuring its lifetime is also bound
    // to the safe URLRequest.
    source_stream = ui::I18nSourceStream::Create(
        std::move(source_stream), net::SourceStream::TYPE_NONE, replacements);
  }

  return source_stream;
}

void URLRequestChromeJob::MimeTypeAvailable(const std::string& mime_type) {
  mime_type_ = mime_type;
}

void URLRequestChromeJob::DataAvailable(base::RefCountedMemory* bytes) {
  TRACE_EVENT_ASYNC_END0("browser", "DataManager:Request", this);
  DCHECK(!data_);

  if (bytes)
    set_expected_content_size(bytes->size());

  // We notify headers are complete unusually late for these jobs, because we
  // need to have |bytes| first to report an accurate expected content size.
  // Otherwise, we cannot support <video> streaming.
  NotifyHeadersComplete();

  // The job can be cancelled after sending the headers.
  if (is_done())
    return;

  // All further requests will be satisfied from the passed-in data.
  data_ = bytes;
  if (!bytes)
    data_available_status_ = net::ERR_FAILED;

  if (pending_buf_) {
    // The request has already been marked async.
    int result = bytes ? PostReadTask(pending_buf_, pending_buf_size_)
                       : data_available_status_;
    pending_buf_ = nullptr;
    if (result != net::ERR_IO_PENDING)
      ReadRawDataComplete(result);
  }
}

int URLRequestChromeJob::ReadRawData(net::IOBuffer* buf, int buf_size) {
  DCHECK(!pending_buf_.get());

  // Handle the cases when DataAvailable() has already been called.
  if (data_available_status_ != net::OK)
    return data_available_status_;
  if (data_)
    return PostReadTask(buf, buf_size);

  // DataAvailable() has not been called yet.  Mark the request as async.
  pending_buf_ = buf;
  pending_buf_size_ = buf_size;
  return net::ERR_IO_PENDING;
}

int URLRequestChromeJob::PostReadTask(scoped_refptr<net::IOBuffer> buf,
                                      int buf_size) {
  DCHECK(buf);
  DCHECK(data_);
  CHECK(buf->data());

  int remaining = data_->size() - data_offset_;
  if (buf_size > remaining)
    buf_size = remaining;

  if (buf_size == 0)
    return 0;

  base::PostTaskWithTraitsAndReply(
      FROM_HERE, {base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&CopyData, base::RetainedRef(buf), buf_size, data_,
                     data_offset_),
      base::BindOnce(&URLRequestChromeJob::ReadRawDataComplete,
                     weak_factory_.GetWeakPtr(), buf_size));
  data_offset_ += buf_size;

  return net::ERR_IO_PENDING;
}

void URLRequestChromeJob::DelayStartForDevTools(
    const base::WeakPtr<URLRequestChromeJob>& job) {
  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(&URLRequestChromeJob::StartAsync, job));
}

void URLRequestChromeJob::StartAsync() {
  if (!request_)
    return;

  if (!backend_->StartRequest(request_, this)) {
    NotifyStartError(net::URLRequestStatus(net::URLRequestStatus::FAILED,
                                           net::ERR_INVALID_URL));
  }
}

namespace {

class ChromeProtocolHandler
    : public net::URLRequestJobFactory::ProtocolHandler {
 public:
  ChromeProtocolHandler(ResourceContext* resource_context,
                        ChromeBlobStorageContext* blob_storage_context)
      : resource_context_(resource_context),
        blob_storage_context_(blob_storage_context) {}
  ~ChromeProtocolHandler() override {}

  net::URLRequestJob* MaybeCreateJob(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate) const override {
    DCHECK(request);

    // Next check for chrome://blob-internals/, which uses its own job type.
    if (ViewBlobInternalsJobFactory::IsSupportedURL(request->url())) {
      return ViewBlobInternalsJobFactory::CreateJobForRequest(
          request, network_delegate, blob_storage_context_->context());
    }

    // Check for chrome://network-error/, which uses its own job type.
    if (request->url().SchemeIs(kChromeUIScheme) &&
        request->url().host_piece() == kChromeUINetworkErrorHost) {
      // Get the error code passed in via the request URL path.
      std::basic_string<char> error_code_string =
          request->url().path().substr(1);

      int error_code;
      if (base::StringToInt(error_code_string, &error_code)) {
        // Check for a valid error code.
        if (URLDataManagerBackend::IsValidNetworkErrorCode(error_code) &&
            error_code != net::Error::ERR_IO_PENDING) {
          return new net::URLRequestErrorJob(request, network_delegate,
                                             error_code);
        }
      }
    }

    // Check for chrome://dino which is an alias for chrome://network-error/-106
    if (request->url().SchemeIs(kChromeUIScheme) &&
        request->url().host() == kChromeUIDinoHost) {
      return new net::URLRequestErrorJob(request, network_delegate,
                                         net::Error::ERR_INTERNET_DISCONNECTED);
    }

    // Fall back to using a custom handler
    return new URLRequestChromeJob(
        request, network_delegate,
        GetURLDataManagerForResourceContext(resource_context_));
  }

  bool IsSafeRedirectTarget(const GURL& location) const override {
    return false;
  }

 private:
  // These members are owned by ProfileIOData, which owns this ProtocolHandler.
  ResourceContext* const resource_context_;

  ChromeBlobStorageContext* blob_storage_context_;

  DISALLOW_COPY_AND_ASSIGN(ChromeProtocolHandler);
};

}  // namespace

URLDataManagerBackend::URLDataManagerBackend()
    : next_request_id_(0), weak_factory_(this) {
  URLDataSource* shared_source = new SharedResourcesDataSource();
  AddDataSource(new URLDataSourceImpl(shared_source->GetSource(),
                                      base::WrapUnique(shared_source)));
}

URLDataManagerBackend::~URLDataManagerBackend() = default;

// static
std::unique_ptr<net::URLRequestJobFactory::ProtocolHandler>
URLDataManagerBackend::CreateProtocolHandler(
    ResourceContext* resource_context,
    ChromeBlobStorageContext* blob_storage_context) {
  DCHECK(resource_context);
  return std::make_unique<ChromeProtocolHandler>(resource_context,
                                                 blob_storage_context);
}

void URLDataManagerBackend::AddDataSource(
    URLDataSourceImpl* source) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!source->source()->ShouldReplaceExistingSource()) {
    auto i = data_sources_.find(source->source_name());
    if (i != data_sources_.end())
      return;
  }
  data_sources_[source->source_name()] = source;
  source->backend_ = weak_factory_.GetWeakPtr();
}

void URLDataManagerBackend::UpdateWebUIDataSource(
    const std::string& source_name,
    const base::DictionaryValue& update) {
  auto it = data_sources_.find(source_name);
  if (it == data_sources_.end() || !it->second->IsWebUIDataSourceImpl()) {
    NOTREACHED();
    return;
  }
  static_cast<WebUIDataSourceImpl*>(it->second.get())
      ->AddLocalizedStrings(update);
}

bool URLDataManagerBackend::HasPendingJob(
    URLRequestChromeJob* job) const {
  for (auto i = pending_requests_.begin(); i != pending_requests_.end(); ++i) {
    if (i->second == job)
      return true;
  }
  return false;
}

bool URLDataManagerBackend::StartRequest(const net::URLRequest* request,
                                         URLRequestChromeJob* job) {
  // NOTE: this duplicates code in web_ui_url_loader_factory.cc's
  // StartURLLoader.
  if (!CheckURLIsValid(request->url()))
    return false;

  URLDataSourceImpl* source = GetDataSourceFromURL(request->url());
  if (!source)
    return false;

  const ResourceRequestInfo* info = ResourceRequestInfo::ForRequest(request);
  if (!source->source()->ShouldServiceRequest(
          request->url(), info ? info->GetContext() : nullptr,
          info ? info->GetChildID() : -1)) {
    return false;
  }

  std::string path;
  URLToRequestPath(request->url(), &path);

  // Save this request so we know where to send the data.
  RequestID request_id = next_request_id_++;
  pending_requests_.insert(std::make_pair(request_id, job));

  job->set_is_gzipped(source->source()->IsGzipped(path));

  // TODO(dschuyler): improve filtering of which resource to run template
  // replacements upon.
  std::string mime_type = source->source()->GetMimeType(path);
  if (mime_type == "text/html")
    job->SetSource(source);

  job->MimeTypeAvailable(mime_type);

  // Look up additional request info to pass down.
  ResourceRequestInfo::WebContentsGetter wc_getter;
  if (info)
    wc_getter = info->GetWebContentsGetterForRequest();

  // Forward along the request to the data source.
  scoped_refptr<base::SingleThreadTaskRunner> target_runner =
      source->source()->TaskRunnerForRequestPath(path);
  if (!target_runner) {
    // The DataSource is agnostic to which thread StartDataRequest is called
    // on for this path.  Call directly into it from this thread, the IO
    // thread.
    source->source()->StartDataRequest(
        path, std::move(wc_getter),
        base::Bind(&URLDataSourceImpl::SendResponse, source, request_id));
  } else {
    // The DataSource wants StartDataRequest to be called on a specific thread,
    // usually the UI thread, for this path.
    target_runner->PostTask(
        FROM_HERE, base::BindOnce(&URLDataManagerBackend::CallStartRequest,
                                  base::RetainedRef(source), path,
                                  std::move(wc_getter), request_id));
  }
  return true;
}

URLDataSourceImpl* URLDataManagerBackend::GetDataSourceFromURL(
    const GURL& url) {
  // The input usually looks like: chrome://source_name/extra_bits?foo
  // so do a lookup using the host of the URL.
  auto i = data_sources_.find(url.host());
  if (i != data_sources_.end())
    return i->second.get();

  // No match using the host of the URL, so do a lookup using the scheme for
  // URLs on the form source_name://extra_bits/foo .
  i = data_sources_.find(url.scheme() + "://");
  if (i != data_sources_.end())
    return i->second.get();

  // No matches found, so give up.
  return nullptr;
}

void URLDataManagerBackend::CallStartRequest(
    scoped_refptr<URLDataSourceImpl> source,
    const std::string& path,
    const ResourceRequestInfo::WebContentsGetter& wc_getter,
    int request_id) {
  source->source()->StartDataRequest(
      path,
      wc_getter,
      base::Bind(&URLDataSourceImpl::SendResponse, source, request_id));
}

void URLDataManagerBackend::RemoveRequest(URLRequestChromeJob* job) {
  // Remove the request from our list of pending requests.
  // If/when the source sends the data that was requested, the data will just
  // be thrown away.
  for (auto i = pending_requests_.begin(); i != pending_requests_.end(); ++i) {
    if (i->second == job) {
      pending_requests_.erase(i);
      return;
    }
  }
}

void URLDataManagerBackend::DataAvailable(RequestID request_id,
                                          base::RefCountedMemory* bytes) {
  // Forward this data on to the pending net::URLRequest, if it exists.
  auto i = pending_requests_.find(request_id);
  if (i != pending_requests_.end()) {
    URLRequestChromeJob* job = i->second;
    pending_requests_.erase(i);
    job->DataAvailable(bytes);
  }
}

scoped_refptr<net::HttpResponseHeaders> URLDataManagerBackend::GetHeaders(
    URLDataSourceImpl* source_impl,
    const std::string& path,
    const std::string& origin) {
  // Set the headers so that requests serviced by ChromeURLDataManager return a
  // status code of 200. Without this they return a 0, which makes the status
  // indistiguishable from other error types. Instant relies on getting a 200.
  auto headers =
      base::MakeRefCounted<net::HttpResponseHeaders>("HTTP/1.1 200 OK");
  if (!source_impl)
    return headers;

  URLDataSource* source = source_impl->source();
  // Determine the least-privileged content security policy header, if any,
  // that is compatible with a given WebUI URL, and append it to the existing
  // response headers.
  if (source->ShouldAddContentSecurityPolicy()) {
    std::string base = kChromeURLContentSecurityPolicyHeaderBase;
    base.append(source->GetContentSecurityPolicyScriptSrc());
    base.append(source->GetContentSecurityPolicyObjectSrc());
    base.append(source->GetContentSecurityPolicyChildSrc());
    base.append(source->GetContentSecurityPolicyStyleSrc());
    base.append(source->GetContentSecurityPolicyImgSrc());
    headers->AddHeader(base);
  }

  if (source->ShouldDenyXFrameOptions())
    headers->AddHeader(kChromeURLXFrameOptionsHeader);

  if (!source->AllowCaching())
    headers->AddHeader("Cache-Control: no-cache");

  std::string mime_type = source->GetMimeType(path);
  if (source->ShouldServeMimeTypeAsContentTypeHeader() && !mime_type.empty()) {
    std::string content_type = base::StringPrintf(
        "%s:%s", net::HttpRequestHeaders::kContentType, mime_type.c_str());
    headers->AddHeader(content_type);
  }

  if (!origin.empty()) {
    std::string header = source->GetAccessControlAllowOriginForOrigin(origin);
    DCHECK(header.empty() || header == origin || header == "*" ||
           header == "null");
    if (!header.empty()) {
      headers->AddHeader("Access-Control-Allow-Origin: " + header);
      headers->AddHeader("Vary: Origin");
    }
  }

  return headers;
}

bool URLDataManagerBackend::CheckURLIsValid(const GURL& url) {
  std::vector<std::string> additional_schemes;
  DCHECK(url.SchemeIs(kChromeUIScheme) ||
         (GetContentClient()->browser()->GetAdditionalWebUISchemes(
              &additional_schemes),
          SchemeIsInSchemes(url.scheme(), additional_schemes)));

  if (!url.is_valid()) {
    NOTREACHED();
    return false;
  }

  return true;
}

void URLDataManagerBackend::URLToRequestPath(const GURL& url,
                                             std::string* path) {
  const std::string& spec = url.possibly_invalid_spec();
  const url::Parsed& parsed = url.parsed_for_possibly_invalid_spec();
  // + 1 to skip the slash at the beginning of the path.
  int offset = parsed.CountCharactersBefore(url::Parsed::PATH, false) + 1;

  if (offset < static_cast<int>(spec.size()))
    path->assign(spec.substr(offset));
}

bool URLDataManagerBackend::IsValidNetworkErrorCode(int error_code) {
  std::unique_ptr<base::DictionaryValue> error_codes = net::GetNetConstants();
  const base::DictionaryValue* net_error_codes_dict = nullptr;

  for (base::DictionaryValue::Iterator itr(*error_codes); !itr.IsAtEnd();
       itr.Advance()) {
    if (itr.key() == kNetworkErrorKey) {
      itr.value().GetAsDictionary(&net_error_codes_dict);
      break;
    }
  }

  if (net_error_codes_dict != nullptr) {
    for (base::DictionaryValue::Iterator itr(*net_error_codes_dict);
         !itr.IsAtEnd(); itr.Advance()) {
      int net_error_code;
      itr.value().GetAsInteger(&net_error_code);
      if (error_code == net_error_code)
        return true;
    }
  }
  return false;
}

std::vector<std::string> URLDataManagerBackend::GetWebUISchemes() {
  std::vector<std::string> schemes;
  schemes.push_back(kChromeUIScheme);
  GetContentClient()->browser()->GetAdditionalWebUISchemes(&schemes);
  return schemes;
}

}  // namespace content
