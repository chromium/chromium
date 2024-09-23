// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/webui/url_data_manager_ios_backend.h"

#import <set>

#import "base/command_line.h"
#import "base/debug/alias.h"
#import "base/functional/bind.h"
#import "base/memory/raw_ptr.h"
#import "base/memory/ref_counted.h"
#import "base/memory/ref_counted_memory.h"
#import "base/memory/weak_ptr.h"
#import "base/strings/string_util.h"
#import "base/task/sequenced_task_runner.h"
#import "base/task/single_thread_task_runner.h"
#import "base/trace_event/trace_event.h"
#import "ios/web/public/browser_state.h"
#import "ios/web/public/thread/web_task_traits.h"
#import "ios/web/public/thread/web_thread.h"
#import "ios/web/public/web_client.h"
#import "ios/web/webui/shared_resources_data_source_ios.h"
#import "ios/web/webui/url_data_source_ios_impl.h"
#import "net/base/io_buffer.h"
#import "net/base/net_errors.h"
#import "net/filter/source_stream.h"
#import "net/http/http_response_headers.h"
#import "net/http/http_status_code.h"
#import "net/url_request/url_request.h"
#import "net/url_request/url_request_context.h"
#import "net/url_request/url_request_job.h"
#import "net/url_request/url_request_job_factory.h"
#import "ui/base/template_expressions.h"
#import "ui/base/webui/i18n_source_stream.h"
#import "url/url_util.h"

using web::WebThread;

namespace web {

namespace {

const char kContentSecurityPolicy[] = "Content-Security-Policy";
const char kChromeURLContentSecurityPolicyHeaderBase[] =
    "script-src chrome://resources 'self'; ";

const char kXFrameOptions[] = "X-Frame-Options";
const char kChromeURLXFrameOptionsHeader[] = "DENY";

const char kWebUIResourcesHost[] = "resources";

// Returns whether `url` passes some sanity checks and is a valid GURL.
bool CheckURLIsValid(const GURL& url) {
  std::vector<std::string> additional_schemes;
  DCHECK(GetWebClient()->IsAppSpecificURL(url) ||
         (GetWebClient()->GetAdditionalWebUISchemes(&additional_schemes),
          base::Contains(additional_schemes, url.scheme())));

  if (!url.is_valid()) {
    NOTREACHED_IN_MIGRATION();
    return false;
  }

  return true;
}

// Parse `url` to get the path which will be used to resolve the request. The
// path is the remaining portion after the scheme and hostname.
void URLToRequestPath(const GURL& url, std::string* path) {
  const std::string& spec = url.possibly_invalid_spec();
  const url::Parsed& parsed = url.parsed_for_possibly_invalid_spec();
  // + 1 to skip the slash at the beginning of the path.
  int offset = parsed.CountCharactersBefore(url::Parsed::PATH, false) + 1;

  if (offset < static_cast<int>(spec.size()))
    path->assign(spec.substr(offset));
}

// Checks for webui resources path inside the given `url` and return a
// fixed one if needed, or the original one otherwise. In js modules,
// The use of x/../../../../ui/webui/resources is mapped by webkit to
// x/ui/webui/resources so to not go out of scope of the module.
GURL RedirectWebUIResources(const GURL url) {
  static std::string kWebUIResources = "/ui/webui/resources";
  if (base::StartsWith(url.path(), kWebUIResources,
                       base::CompareCase::SENSITIVE)) {
    GURL::Replacements replacements;
    replacements.SetHostStr(kWebUIResourcesHost);
    replacements.SetPathStr(url.path().c_str() + kWebUIResources.size());
    return url.ReplaceComponents(replacements);
  }
  return url;
}

}  // namespace

// URLRequestChromeJob is a net::URLRequestJob that manages running
// chrome-internal resource requests asynchronously.
// It hands off URL requests to ChromeURLDataManagerIOS, which asynchronously
// calls back once the data is available.
class URLRequestChromeJob : public net::URLRequestJob {
 public:
  // `is_incognito` set when job is generated from an incognito profile.
  URLRequestChromeJob(net::URLRequest* request,
                      BrowserState* browser_state,
                      bool is_incognito);

  URLRequestChromeJob(const URLRequestChromeJob&) = delete;
  URLRequestChromeJob& operator=(const URLRequestChromeJob&) = delete;

  ~URLRequestChromeJob() override;

  // net::URLRequestJob implementation.
  void Start() override;
  void Kill() override;
  int ReadRawData(net::IOBuffer* buf, int buf_size) override;
  bool GetMimeType(std::string* mime_type) const override;
  void GetResponseInfo(net::HttpResponseInfo* info) override;
  std::unique_ptr<net::SourceStream> SetUpSourceStream() override;

  // Used to notify that the requested data's `mime_type` is ready.
  void MimeTypeAvailable(URLDataSourceIOSImpl* source,
                         const std::string& mime_type);

  // Called by ChromeURLDataManagerIOS to notify us that the data blob is ready
  // for us.
  void DataAvailable(base::RefCountedMemory* bytes);

  void set_mime_type(const std::string& mime_type) { mime_type_ = mime_type; }

  void set_allow_caching(bool allow_caching) { allow_caching_ = allow_caching; }

  void set_add_content_security_policy(bool add_content_security_policy) {
    add_content_security_policy_ = add_content_security_policy;
  }

  void set_content_security_policy_object_source(const std::string& data) {
    content_security_policy_object_source_ = data;
  }

  void set_content_security_policy_frame_source(const std::string& data) {
    content_security_policy_frame_source_ = data;
  }

  void set_deny_xframe_options(bool deny_xframe_options) {
    deny_xframe_options_ = deny_xframe_options;
  }

  void set_source(scoped_refptr<URLDataSourceIOSImpl> source) {
    source_ = source;
  }

  void set_send_content_type_header(bool send_content_type_header) {
    send_content_type_header_ = send_content_type_header;
  }

  // Returns true when job was generated from an incognito profile.
  bool is_incognito() const { return is_incognito_; }

 private:
  friend class URLDataManagerIOSBackend;

  // Do the actual copy from data_ (the data we're serving) into `buf`.
  // Separate from ReadRawData so we can handle async I/O.
  int CompleteRead(net::IOBuffer* buf, int buf_size);

  // Called asynchronously to notify of an error occuring while trying to start
  // the job.
  void NotifyStartErrorAsync();

  // The actual data we're serving.  NULL until it's been fetched.
  scoped_refptr<base::RefCountedMemory> data_;
  // The current offset into the data that we're handing off to our
  // callers via the Read interfaces.
  int data_offset_;

  // For async reads, we keep around a pointer to the buffer that
  // we're reading into.
  scoped_refptr<net::IOBuffer> pending_buf_;
  int pending_buf_size_;
  std::string mime_type_;

  // If true, set a header in the response to prevent it from being cached.
  bool allow_caching_;

  // If true, set the Content Security Policy (CSP) header.
  bool add_content_security_policy_;

  // These are used with the CSP.
  std::string content_security_policy_object_source_;
  std::string content_security_policy_frame_source_;

  // If true, sets  the "X-Frame-Options: DENY" header.
  bool deny_xframe_options_;

  // The URLDataSourceIOSImpl that is servicing this request. This is a shared
  // pointer so that the request can continue to be served even if the source is
  // detached from the backend that initially owned it.
  scoped_refptr<URLDataSourceIOSImpl> source_;

  // If true, sets the "Content-Type: <mime-type>" header.
  bool send_content_type_header_;

  // True when job is generated from an incognito profile.
  const bool is_incognito_;

  // The BrowserState with which this job is associated.
  raw_ptr<BrowserState> browser_state_;

  // The backend is owned by the BrowserState and always outlives us. It is
  // obtained from the BrowserState on the IO thread.
  raw_ptr<URLDataManagerIOSBackend> backend_;

  base::WeakPtrFactory<URLRequestChromeJob> weak_factory_;
};

URLRequestChromeJob::URLRequestChromeJob(net::URLRequest* request,
                                         BrowserState* browser_state,
                                         bool is_incognito)
    : net::URLRequestJob(request),
      data_offset_(0),
      pending_buf_size_(0),
      allow_caching_(true),
      add_content_security_policy_(true),
      content_security_policy_object_source_("object-src 'none';"),
      content_security_policy_frame_source_("frame-src 'none';"),
      deny_xframe_options_(true),
      send_content_type_header_(false),
      is_incognito_(is_incognito),
      browser_state_(browser_state),
      backend_(nullptr),
      weak_factory_(this) {
  DCHECK(browser_state_);
}

URLRequestChromeJob::~URLRequestChromeJob() {
  if (backend_) {
    CHECK(!backend_->HasPendingJob(this));
  }
}

void URLRequestChromeJob::Start() {
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN1("browser", "DataManager:Request",
                                    TRACE_ID_LOCAL(this), "URL",
                                    request_->url().possibly_invalid_spec());

  if (!request_)
    return;
  DCHECK(browser_state_);

  // Obtain the URLDataManagerIOSBackend instance that is associated with
  // `browser_state_`. Note that this *must* be done on the IO thread.
  backend_ = browser_state_->GetURLDataManagerIOSBackendOnIOThread();
  DCHECK(backend_);

  if (!backend_->StartRequest(request_, this)) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&URLRequestChromeJob::NotifyStartErrorAsync,
                                  weak_factory_.GetWeakPtr()));
  }
}

void URLRequestChromeJob::Kill() {
  weak_factory_.InvalidateWeakPtrs();
  if (backend_)
    backend_->RemoveRequest(this);
  URLRequestJob::Kill();
}

bool URLRequestChromeJob::GetMimeType(std::string* mime_type) const {
  *mime_type = mime_type_;
  return !mime_type_.empty();
}

void URLRequestChromeJob::GetResponseInfo(net::HttpResponseInfo* info) {
  DCHECK(!info->headers.get());
  // Set the headers so that requests serviced by ChromeURLDataManagerIOS
  // return a status code of 200. Without this they return a 0, which makes the
  // status indistiguishable from other error types. Instant relies on getting
  // a 200.
  info->headers = new net::HttpResponseHeaders("HTTP/1.1 200 OK");

  // Determine the least-privileged content security policy header, if any,
  // that is compatible with a given WebUI URL, and append it to the existing
  // response headers.
  if (add_content_security_policy_) {
    std::string base = kChromeURLContentSecurityPolicyHeaderBase;
    base.append(content_security_policy_object_source_);
    base.append(content_security_policy_frame_source_);
    info->headers->AddHeader(kContentSecurityPolicy, base);
  }

  if (deny_xframe_options_)
    info->headers->AddHeader(kXFrameOptions, kChromeURLXFrameOptionsHeader);

  if (!allow_caching_)
    info->headers->AddHeader("Cache-Control", "no-cache");

  if (send_content_type_header_ && !mime_type_.empty())
    info->headers->AddHeader(net::HttpRequestHeaders::kContentType, mime_type_);
}

std::unique_ptr<net::SourceStream> URLRequestChromeJob::SetUpSourceStream() {
  std::unique_ptr<net::SourceStream> source_stream =
      net::URLRequestJob::SetUpSourceStream();

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
    // which we keep alive via `source_`, ensuring its lifetime is also bound
    // to the safe URLRequest.
    source_stream = ui::I18nSourceStream::Create(
        std::move(source_stream), net::SourceStream::TYPE_NONE, replacements);
  }

  return source_stream;
}

void URLRequestChromeJob::MimeTypeAvailable(URLDataSourceIOSImpl* source,
                                            const std::string& mime_type) {
  set_mime_type(mime_type);

  if (mime_type == "text/html" || (mime_type == "application/javascript" &&
                                   source->ShouldReplaceI18nInJS())) {
    set_source(source);
  }

  NotifyHeadersComplete();
}

void URLRequestChromeJob::DataAvailable(base::RefCountedMemory* bytes) {
  TRACE_EVENT_NESTABLE_ASYNC_END0("browser", "DataManager:Request",
                                  TRACE_ID_LOCAL(this));
  if (bytes) {
    data_ = bytes;
    if (pending_buf_.get()) {
      CHECK(pending_buf_->data());
      int rv = CompleteRead(pending_buf_.get(), pending_buf_size_);
      pending_buf_.reset();
      ReadRawDataComplete(rv);
    }
  } else {
    ReadRawDataComplete(net::ERR_FAILED);
  }
}

int URLRequestChromeJob::ReadRawData(net::IOBuffer* buf, int buf_size) {
  if (!data_.get()) {
    DCHECK(!pending_buf_.get());
    CHECK(buf->data());
    pending_buf_ = buf;
    pending_buf_size_ = buf_size;
    return net::ERR_IO_PENDING;  // Tell the caller we're still waiting for
                                 // data.
  }

  // Otherwise, the data is available.
  return CompleteRead(buf, buf_size);
}

int URLRequestChromeJob::CompleteRead(net::IOBuffer* buf, int buf_size) {
  // http://crbug.com/373841
  char url_buf[128];
  base::strlcpy(url_buf, request_->url().spec().c_str(), std::size(url_buf));
  base::debug::Alias(url_buf);

  int remaining = data_->size() - data_offset_;
  if (buf_size > remaining)
    buf_size = remaining;
  if (buf_size > 0) {
    memcpy(buf->data(), data_->front() + data_offset_, buf_size);
    data_offset_ += buf_size;
  }
  return buf_size;
}

void URLRequestChromeJob::NotifyStartErrorAsync() {
  NotifyStartError(net::ERR_INVALID_URL);
}

namespace {

// Gets mime type for data that is available from `source` by `path`.
// After that, notifies `job` that mime type is available. This method
// should be called on the UI thread, but notification is performed on
// the IO thread.
void GetMimeTypeOnUI(URLDataSourceIOSImpl* source,
                     const std::string& path,
                     const base::WeakPtr<URLRequestChromeJob>& job) {
  DCHECK_CURRENTLY_ON(WebThread::UI);
  std::string mime_type = source->source()->GetMimeType(path);
  GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&URLRequestChromeJob::MimeTypeAvailable, job,
                                base::RetainedRef(source), mime_type));
}

}  // namespace

namespace {

class ChromeProtocolHandler
    : public net::URLRequestJobFactory::ProtocolHandler {
 public:
  // `is_incognito` should be set for incognito profiles.
  ChromeProtocolHandler(BrowserState* browser_state, bool is_incognito)
      : browser_state_(browser_state), is_incognito_(is_incognito) {}

  ChromeProtocolHandler(const ChromeProtocolHandler&) = delete;
  ChromeProtocolHandler& operator=(const ChromeProtocolHandler&) = delete;

  ~ChromeProtocolHandler() override {}

  std::unique_ptr<net::URLRequestJob> CreateJob(
      net::URLRequest* request) const override {
    DCHECK(request);

    return std::make_unique<URLRequestChromeJob>(request, browser_state_,
                                                 is_incognito_);
  }

  bool IsSafeRedirectTarget(const GURL& location) const override {
    return false;
  }

 private:
  raw_ptr<BrowserState> browser_state_;

  // True when generated from an incognito profile.
  const bool is_incognito_;
};

}  // namespace

URLDataManagerIOSBackend::URLDataManagerIOSBackend() : next_request_id_(0) {
  URLDataSourceIOS* shared_source = new SharedResourcesDataSourceIOS();
  URLDataSourceIOSImpl* source_impl =
      new URLDataSourceIOSImpl(shared_source->GetSource(), shared_source);
  AddDataSource(source_impl);
}

URLDataManagerIOSBackend::~URLDataManagerIOSBackend() {
  for (DataSourceMap::iterator i = data_sources_.begin();
       i != data_sources_.end(); ++i) {
    i->second->backend_ = nullptr;
  }
  data_sources_.clear();
}

// static
std::unique_ptr<net::URLRequestJobFactory::ProtocolHandler>
URLDataManagerIOSBackend::CreateProtocolHandler(BrowserState* browser_state) {
  DCHECK(browser_state);
  return std::make_unique<ChromeProtocolHandler>(
      browser_state, browser_state->IsOffTheRecord());
}

void URLDataManagerIOSBackend::AddDataSource(URLDataSourceIOSImpl* source) {
  DCHECK_CURRENTLY_ON(WebThread::IO);
  DataSourceMap::iterator i = data_sources_.find(source->source_name());
  if (i != data_sources_.end()) {
    if (!source->source()->ShouldReplaceExistingSource())
      return;
    i->second->backend_ = nullptr;
  }
  data_sources_[source->source_name()] = source;
  source->backend_ = this;
}

bool URLDataManagerIOSBackend::HasPendingJob(URLRequestChromeJob* job) const {
  for (PendingRequestMap::const_iterator i = pending_requests_.begin();
       i != pending_requests_.end(); ++i) {
    if (i->second == job)
      return true;
  }
  return false;
}

bool URLDataManagerIOSBackend::StartRequest(const net::URLRequest* request,
                                            URLRequestChromeJob* job) {
  if (!CheckURLIsValid(request->url()))
    return false;

  GURL url = RedirectWebUIResources(request->url());

  URLDataSourceIOSImpl* source = GetDataSourceFromURL(url);
  if (!source)
    return false;

  if (!source->source()->ShouldServiceRequest(url))
    return false;

  std::string path;
  URLToRequestPath(url, &path);

  // Save this request so we know where to send the data.
  RequestID request_id = next_request_id_++;
  pending_requests_.insert(std::make_pair(request_id, job));

  job->set_allow_caching(source->source()->AllowCaching());
  job->set_add_content_security_policy(true);
  job->set_content_security_policy_object_source(
      source->source()->GetContentSecurityPolicyObjectSrc());
  job->set_content_security_policy_frame_source("frame-src 'none';");
  job->set_deny_xframe_options(source->source()->ShouldDenyXFrameOptions());
  job->set_send_content_type_header(false);

  // Forward along the request to the data source.
  // URLRequestChromeJob should receive mime type before data. This
  // is guaranteed because request for mime type is placed in the
  // message loop before request for data. And correspondingly their
  // replies are put on the IO thread in the same order.
  scoped_refptr<base::SingleThreadTaskRunner> target_runner =
      web::GetUIThreadTaskRunner({});
  target_runner->PostTask(
      FROM_HERE, base::BindOnce(&GetMimeTypeOnUI, base::RetainedRef(source),
                                path, job->weak_factory_.GetWeakPtr()));

  target_runner->PostTask(
      FROM_HERE,
      base::BindOnce(&URLDataManagerIOSBackend::CallStartRequest,
                     base::WrapRefCounted(source), path, request_id));
  return true;
}

URLDataSourceIOSImpl* URLDataManagerIOSBackend::GetDataSourceFromURL(
    const GURL& url) {
  // The input usually looks like: chrome://source_name/extra_bits?foo
  // so do a lookup using the host of the URL.
  DataSourceMap::iterator i = data_sources_.find(url.host());
  if (i != data_sources_.end())
    return i->second.get();

  // No match using the host of the URL, so do a lookup using the scheme for
  // URLs on the form source_name://extra_bits/foo .
  i = data_sources_.find(url.scheme() + "://");
  if (i != data_sources_.end())
    return i->second.get();

  // No matches found, so give up.
  return NULL;
}

void URLDataManagerIOSBackend::CallStartRequest(
    scoped_refptr<URLDataSourceIOSImpl> source,
    const std::string& path,
    int request_id) {
  source->source()->StartDataRequest(
      path, base::BindRepeating(&URLDataSourceIOSImpl::SendResponse, source,
                                request_id));
}

void URLDataManagerIOSBackend::RemoveRequest(URLRequestChromeJob* job) {
  // Remove the request from our list of pending requests.
  // If/when the source sends the data that was requested, the data will just
  // be thrown away.
  for (PendingRequestMap::iterator i = pending_requests_.begin();
       i != pending_requests_.end(); ++i) {
    if (i->second == job) {
      pending_requests_.erase(i);
      return;
    }
  }
}

void URLDataManagerIOSBackend::DataAvailable(RequestID request_id,
                                             base::RefCountedMemory* bytes) {
  // Forward this data on to the pending net::URLRequest, if it exists.
  PendingRequestMap::iterator i = pending_requests_.find(request_id);
  if (i != pending_requests_.end()) {
    URLRequestChromeJob* job(i->second);
    pending_requests_.erase(i);
    job->DataAvailable(bytes);
  }
}

}  // namespace web
