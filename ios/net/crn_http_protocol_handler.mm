// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/net/crn_http_protocol_handler.h"

#include <stdint.h>

#include <memory>
#include <utility>
#include <vector>

#include "base/apple/foundation_util.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#import "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#import "base/task/single_thread_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "ios/net/chunked_data_stream_uploader.h"
#import "ios/net/clients/crn_network_client_protocol.h"
#import "ios/net/crn_http_protocol_handler_proxy_with_client_thread.h"
#import "ios/net/http_protocol_logging.h"
#include "ios/net/nsurlrequest_util.h"
#import "ios/net/protocol_handler_util.h"
#import "net/base/apple/url_conversions.h"
#include "net/base/auth.h"
#include "net/base/elements_upload_data_stream.h"
#include "net/base/io_buffer.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/base/upload_bytes_element_reader.h"
#include "net/http/http_request_headers.h"
#include "net/url_request/redirect_info.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"

namespace net {
class HttpProtocolHandlerCore;
}

namespace {

// Minimum size of the buffer used to read the net::URLRequest.
const int kIOBufferMinSize = 64 * 1024;

// Maximum size of the buffer used to read the net::URLRequest.
const int kIOBufferMaxSize = 16 * kIOBufferMinSize;  // 1MB

// Global instance of the HTTPProtocolHandlerDelegate.
net::HTTPProtocolHandlerDelegate* g_protocol_handler_delegate = nullptr;

// Global instance of the MetricsDelegate.
net::MetricsDelegate* g_metrics_delegate = nullptr;

}  // namespace

// Bridge class to forward NSStream events to the HttpProtocolHandlerCore.
// Lives on the IO thread.
@interface CRWHTTPStreamDelegate : NSObject<NSStreamDelegate> {
 @private
  // The object is owned by |_core| and has a weak reference to it.
  raw_ptr<net::HttpProtocolHandlerCore> _core;  // weak
}
- (instancetype)initWithHttpProtocolHandlerCore:
    (net::HttpProtocolHandlerCore*)core;
// NSStreamDelegate method.
- (void)stream:(NSStream*)theStream handleEvent:(NSStreamEvent)streamEvent;
@end

#pragma mark -
#pragma mark HttpProtocolHandlerCore

namespace net {

MetricsDelegate::Metrics::Metrics() = default;
MetricsDelegate::Metrics::~Metrics() = default;

// static
void HTTPProtocolHandlerDelegate::SetInstance(
    HTTPProtocolHandlerDelegate* delegate) {
  g_protocol_handler_delegate = delegate;
}

// static
void MetricsDelegate::SetInstance(MetricsDelegate* delegate) {
  g_metrics_delegate = delegate;
}

// The HttpProtocolHandlerCore class is the bridge between the URLRequest
// and the NSURLProtocolClient.
// Threading and ownership details:
// - The HttpProtocolHandlerCore is owned by the HttpProtocolHandler
// - The HttpProtocolHandler is owned by the system and can be deleted anytime
// - All the methods of HttpProtocolHandlerCore must be called on the IO thread,
//   except its constructor that can be called from any thread.

// Implementation notes from Apple's "Read Me About CustomHttpProtocolHandler":
//
// An NSURLProtocol subclass is expected to call the various methods of the
// NSURLProtocolClient from the loading thread, including all of the following:
//  -URLProtocol:wasRedirectedToRequest:redirectResponse:
//  -URLProtocol:didReceiveResponse:cacheStoragePolicy:
//  -URLProtocol:didLoadData:
//  -URLProtocol:didFinishLoading:
//  -URLProtocol:didFailWithError:
//  -URLProtocol:didReceiveAuthenticationChallenge:
//  -URLProtocol:didCancelAuthenticationChallenge:
//
// The NSURLProtocol subclass must call the client callbacks in the expected
// order. This breaks down into three phases:
//  o pre-response -- In the initial phase the NSURLProtocol can make any number
//    of -URLProtocol:wasRedirectedToRequest:redirectResponse: and
//    -URLProtocol:didReceiveAuthenticationChallenge: callbacks.
//  o response -- It must then call
//    -URLProtocol:didReceiveResponse:cacheStoragePolicy: to indicate the
//    arrival of a definitive response.
//  o post-response -- After receive a response it may then make any number of
//    -URLProtocol:didLoadData: callbacks, followed by a
//    -URLProtocolDidFinishLoading: callback.
//
// The -URLProtocol:didFailWithError: callback can be made at any time
// (although keep in mind the following point).
//
// The NSProtocol subclass must only send one authentication challenge to the
// client at a time. After calling
// -URLProtocol:didReceiveAuthenticationChallenge:, it must wait for the client
// to resolve the challenge before calling any callbacks other than
// -URLProtocol:didCancelAuthenticationChallenge:. This means that, if the
// connection fails while there is an outstanding authentication challenge, the
// NSURLProtocol subclass must call
// -URLProtocol:didCancelAuthenticationChallenge: before calling
// -URLProtocol:didFailWithError:.
class HttpProtocolHandlerCore
    : public base::RefCountedThreadSafe<HttpProtocolHandlerCore,
                                        HttpProtocolHandlerCore>,
      public URLRequest::Delegate,
      public ChunkedDataStreamUploader::Delegate {
 public:
  explicit HttpProtocolHandlerCore(NSURLRequest* request);
  explicit HttpProtocolHandlerCore(NSURLSessionTask* task);

  HttpProtocolHandlerCore(const HttpProtocolHandlerCore&) = delete;
  HttpProtocolHandlerCore& operator=(const HttpProtocolHandlerCore&) = delete;

  // Starts the network request, and forwards the data downloaded from the
  // network to |base_client|.
  void Start(id<CRNNetworkClientProtocol> base_client);
  // Cancels the request.
  void Cancel();
  // Called by NSStreamDelegate. Used for POST requests having a HTTPBodyStream.
  void HandleStreamEvent(NSStream* stream, NSStreamEvent event);

  // URLRequest::Delegate methods:
  void OnReceivedRedirect(URLRequest* request,
                          const RedirectInfo& new_url,
                          bool* defer_redirect) override;
  void OnAuthRequired(URLRequest* request,
                      const AuthChallengeInfo& auth_info) override;
  void OnCertificateRequested(URLRequest* request,
                              SSLCertRequestInfo* cert_request_info) override;
  void OnSSLCertificateError(URLRequest* request,
                             int net_error,
                             const SSLInfo& ssl_info,
                             bool fatal) override;
  void OnResponseStarted(URLRequest* request, int net_error) override;
  void OnReadCompleted(URLRequest* request, int bytes_read) override;

  // ChunkedDataStreamUploader::Delegate method:
  int OnRead(char* buffer, int buffer_length) override;

 private:
  friend class base::RefCountedThreadSafe<HttpProtocolHandlerCore,
                                          HttpProtocolHandlerCore>;
  friend class base::DeleteHelper<HttpProtocolHandlerCore>;
  ~HttpProtocolHandlerCore() override;

  // RefCountedThreadSafe traits implementation:
  static void Destruct(const HttpProtocolHandlerCore* x);

  void SetLoadFlags();
  void StopNetRequest();
  // Stop listening the delegate on the IO run loop.
  void StopListeningStream(NSStream* stream);
  NSInteger IOSErrorCode(int os_error);
  void StopRequestWithError(NSInteger ns_error_code, int net_error_code);
  void StripPostSpecificHeaders(NSMutableURLRequest* request);
  void CancelAfterSSLError();
  void StartReading();
  void AllocateReadBuffer(int last_read_data_size);

  THREAD_CHECKER(thread_checker_);

  // The NSURLProtocol client.
  id<CRNNetworkClientProtocol> client_ = nil;
  std::unique_ptr<char, base::FreeDeleter> read_buffer_;
  int read_buffer_size_ = kIOBufferMinSize;
  scoped_refptr<WrappedIOBuffer> read_buffer_wrapper_;
  NSMutableURLRequest* request_ = nil;
  NSURLSessionTask* task_ = nil;
  // The stream has data to upload.
  NSInputStream* http_body_stream_ = nil;
  // Stream delegate to read the HTTPBodyStream.
  CRWHTTPStreamDelegate* http_body_stream_delegate_ = nullptr;
  // Vector of readers used to accumulate a POST data stream.
  std::vector<std::unique_ptr<UploadElementReader>> post_data_readers_;

  // This cannot be a scoped pointer because it must be deleted on the IO
  // thread.
  raw_ptr<URLRequest> net_request_ = nullptr;

  // It is a weak pointer because the owner of the uploader is the URLRequest.
  base::WeakPtr<ChunkedDataStreamUploader> chunked_uploader_;
};

HttpProtocolHandlerCore::HttpProtocolHandlerCore(NSURLRequest* request) {
  // The request will be accessed from another thread. It is safer to make a
  // copy to avoid conflicts.
  // The copy is mutable, because that request will be given to the client in
  // case of a redirect, but with a different URL. The URL must be created
  // from the absoluteString of the original URL, because mutableCopy only
  // shallowly copies the request, and just retains the non-threadsafe NSURL.
  DETACH_FROM_THREAD(thread_checker_);
  task_ = nil;
  request_ = [request mutableCopy];
  // Will allocate read buffer with size |kIOBufferMinSize|.
  AllocateReadBuffer(0);
  [request_ setURL:request.URL];
}

HttpProtocolHandlerCore::HttpProtocolHandlerCore(NSURLSessionTask* task)
    : HttpProtocolHandlerCore([task currentRequest]) {
  task_ = task;
}

void HttpProtocolHandlerCore::HandleStreamEvent(NSStream* stream,
                                                NSStreamEvent event) {
  DVLOG(2) << "HandleStreamEvent " << stream << " event " << event;
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(http_body_stream_ == stream);
  DCHECK(http_body_stream_delegate_);
  switch (event) {
    case NSStreamEventErrorOccurred:
      DLOG(ERROR)
          << "Failed to read POST data: "
          << base::SysNSStringToUTF8([[stream streamError] description]);
      StopListeningStream(stream);
      StopRequestWithError(NSURLErrorUnknown, ERR_UNEXPECTED);
      break;
    case NSStreamEventEndEncountered:
      StopListeningStream(stream);
      if (chunked_uploader_) {
        chunked_uploader_->UploadWhenReady(true);
        break;
      }

      if (!post_data_readers_.empty()) {
        // NOTE: This call will result in |post_data_readers_| being cleared,
        // which is the desired behavior.
        net_request_->set_upload(std::make_unique<ElementsUploadDataStream>(
            std::move(post_data_readers_), 0));
        DCHECK(post_data_readers_.empty());
      }
      net_request_->Start();
      break;
    case NSStreamEventHasBytesAvailable: {
      if (chunked_uploader_) {
        chunked_uploader_->UploadWhenReady(false);
        break;
      }

      NSInteger length;
      // TODO(crbug.com/41327992): Dynamically change the size of the read
      // buffer to improve the read (POST) performance, see
      // AllocateReadBuffer(), & avoid unnecessary data copy.
      length = [base::apple::ObjCCastStrict<NSInputStream>(stream)
               read:reinterpret_cast<unsigned char*>(read_buffer_.get())
          maxLength:read_buffer_size_];
      if (length > 0) {
        std::vector<char> owned_data(read_buffer_.get(),
                                     read_buffer_.get() + length);
        post_data_readers_.push_back(
            std::make_unique<UploadOwnedBytesElementReader>(&owned_data));
      } else if (length < 0) {  // Error
        StopRequestWithError(stream.streamError.code, ERR_FAILED);
      }
      break;
    }
    case NSStreamEventNone:
    case NSStreamEventOpenCompleted:
    case NSStreamEventHasSpaceAvailable:
      break;
    default:
      NOTREACHED_IN_MIGRATION() << "Unexpected stream event: " << event;
      break;
  }
}

#pragma mark URLRequest::Delegate methods

void HttpProtocolHandlerCore::OnReceivedRedirect(
    URLRequest* request,
    const RedirectInfo& redirect_info,
    bool* /* defer_redirect */) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // Cancel the request and notify UIWebView.
  // If we did nothing, the network stack would follow the redirect
  // automatically, however we do not want this because we need the UIWebView to
  // be notified. The UIWebView will then issue a new request following the
  // redirect.
  DCHECK(request_);
  GURL new_url = redirect_info.new_url;

  if (!new_url.is_valid()) {
    StopRequestWithError(NSURLErrorBadURL, ERR_INVALID_URL);
    return;
  }

  DCHECK(new_url.is_valid());
  NSURL* new_nsurl = NSURLWithGURL(new_url);
  // Stash the original URL in case we need to report it in an error.
  [request_ setURL:new_nsurl];

  if (http_body_stream_)
    StopListeningStream(http_body_stream_);

  // TODO(droger): See if we can share some code with URLRequest::Redirect() in
  // net/net_url_request/url_request.cc.

  // For 303 redirects, all request methods except HEAD are converted to GET,
  // as per the latest httpbis draft.  The draft also allows POST requests to
  // be converted to GETs when following 301/302 redirects, for historical
  // reasons. Most major browsers do this and so shall we.
  // See:
  // https://tools.ietf.org/html/draft-ietf-httpbis-p2-semantics-17#section-7.3
  const int http_status_code = request->GetResponseCode();
  NSString* method = [request_ HTTPMethod];
  const bool was_post = [method isEqualToString:@"POST"];
  if ((http_status_code == 303 && ![method isEqualToString:@"HEAD"]) ||
      ((http_status_code == 301 || http_status_code == 302) && was_post)) {
    [request_ setHTTPMethod:@"GET"];
    [request_ setHTTPBody:nil];
    [request_ setHTTPBodyStream:nil];
    if (was_post) {
      // If being switched from POST to GET, must remove headers that were
      // specific to the POST and don't have meaning in GET. For example
      // the inclusion of a multipart Content-Type header in GET can cause
      // problems with some servers:
      // http://code.google.com/p/chromium/issues/detail?id=843
      StripPostSpecificHeaders(request_);
    }
  }

  NSURLResponse* response = GetNSURLResponseForRequest(request);
#if !defined(NDEBUG)
  DVLOG(2) << "Redirect, to client:";
  LogNSURLResponse(response);
  DVLOG(2) << "Redirect, to client:";
  LogNSURLRequest(request_);
#endif  // !defined(NDEBUG)

  [client_ wasRedirectedToRequest:request_
                    nativeRequest:request
                 redirectResponse:response];
  // Don't use |request_| or |response| anymore, as the client may be using them
  // on another thread and they are not re-entrant. As |request_| is mutable, it
  // is also important that it is not modified.
  request_ = nil;
  request->Cancel();
  DCHECK_EQ(net_request_, request);
  StopNetRequest();
}

void HttpProtocolHandlerCore::OnAuthRequired(
    URLRequest* request,
    const AuthChallengeInfo& auth_info) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (net_request_ != nullptr) {
    net_request_->CancelAuth();
  }
}

void HttpProtocolHandlerCore::OnCertificateRequested(
    URLRequest* request,
    SSLCertRequestInfo* cert_request_info) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // TODO(ios): The network stack does not support SSL client authentication
  // on iOS yet. The request has to be canceled for now.
  request->Cancel();
  StopRequestWithError(NSURLErrorClientCertificateRequired,
                       ERR_SSL_PROTOCOL_ERROR);
}

void HttpProtocolHandlerCore::CancelAfterSSLError(void) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (net_request_ != nullptr) {
    // Cancel the request.
    net_request_->Cancel();

    // The request is signalled simply cancelled to the consumer, the
    // presentation of the SSL error will be done via the tracker.
    StopRequestWithError(NSURLErrorCancelled, ERR_BLOCKED_BY_CLIENT);
  }
}

void HttpProtocolHandlerCore::OnSSLCertificateError(URLRequest* request,
                                                    int net_error,
                                                    const SSLInfo& ssl_info,
                                                    bool fatal) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  CancelAfterSSLError();
}

void HttpProtocolHandlerCore::OnResponseStarted(URLRequest* request,
                                                int net_error) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK_NE(net::ERR_IO_PENDING, net_error);

  if (net_request_ == nullptr)
    return;

  if (net_error != net::OK) {
    StopRequestWithError(IOSErrorCode(net_error), net_error);
    return;
  }

  StartReading();
}

void HttpProtocolHandlerCore::StartReading() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (net_request_ == nullptr)
    return;

  NSURLResponse* response = GetNSURLResponseForRequest(net_request_);
#if !defined(NDEBUG)
  DVLOG(2) << "To client:";
  LogNSURLResponse(response);
#endif  // !defined(NDEBUG)

  // Don't call any function on the response from now on, as the client may be
  // using it and the object is not re-entrant.
  [client_ didReceiveResponse:response];

  int bytes_read =
      net_request_->Read(read_buffer_wrapper_.get(), read_buffer_size_);
  if (bytes_read == net::ERR_IO_PENDING)
    return;

  if (bytes_read >= 0) {
    OnReadCompleted(net_request_, bytes_read);
  } else {
    int error = bytes_read;
    StopRequestWithError(IOSErrorCode(error), error);
  }
}

void HttpProtocolHandlerCore::OnReadCompleted(URLRequest* request,
                                              int bytes_read) {
  DCHECK_NE(net::ERR_IO_PENDING, bytes_read);
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (net_request_ == nullptr)
    return;

  DCHECK_EQ(net_request_, request);

  // Read data from the socket until no bytes left to read.
  while (bytes_read > 0) {
    // The NSData will take the ownership of |read_buffer_|.
    NSData* data =
        [NSData dataWithBytesNoCopy:read_buffer_.release() length:bytes_read];
    // If the data is not encoded in UTF8, the NSString is nil.
    DVLOG(3) << "To client:" << std::endl
             << base::SysNSStringToUTF8([[NSString alloc]
                    initWithData:data
                        encoding:NSUTF8StringEncoding]);
    // Pass the read data to the client.
    [client_ didLoadData:data];

    // Allocate a new buffer and continue reading from the socket.
    AllocateReadBuffer(bytes_read);
    bytes_read = request->Read(read_buffer_wrapper_.get(), read_buffer_size_);
  }

  if (bytes_read == net::OK) {
    // If there is nothing more to read.
    StopNetRequest();
    [client_ didFinishLoading];
  } else if (bytes_read != net::ERR_IO_PENDING) {
    // If there was an error (not canceled).
    int error = bytes_read;
    StopRequestWithError(IOSErrorCode(error), error);
  }
}

void HttpProtocolHandlerCore::AllocateReadBuffer(int last_read_data_size) {
  if (last_read_data_size == read_buffer_size_) {
    // If the whole buffer was filled with data then increase the buffer size
    // for the next read but don't exceed |kIOBufferMaxSize|.
    read_buffer_size_ = std::min(read_buffer_size_ * 2, kIOBufferMaxSize);
  } else if (read_buffer_size_ / 2 >= last_read_data_size) {
    // If only a half or less of the buffer was filled with data then reduce
    // the buffer size for the next read but not make it smaller than
    // |kIOBufferMinSize|.
    read_buffer_size_ = std::max(read_buffer_size_ / 2, kIOBufferMinSize);
  }
  read_buffer_.reset(static_cast<char*>(malloc(read_buffer_size_)));
  read_buffer_wrapper_ = base::MakeRefCounted<WrappedIOBuffer>(base::make_span(
      read_buffer_.get(), static_cast<size_t>(read_buffer_size_)));
}

HttpProtocolHandlerCore::~HttpProtocolHandlerCore() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!net_request_);
  DCHECK(!http_body_stream_delegate_);
}

// static
void HttpProtocolHandlerCore::Destruct(const HttpProtocolHandlerCore* x) {
  scoped_refptr<base::SingleThreadTaskRunner> task_runner =
      g_protocol_handler_delegate->GetDefaultURLRequestContext()
          ->GetNetworkTaskRunner();
  if (task_runner->BelongsToCurrentThread())
    delete x;
  else
    task_runner->DeleteSoon(FROM_HERE, x);
}

void HttpProtocolHandlerCore::SetLoadFlags() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  int load_flags = LOAD_NORMAL;

  switch ([request_ cachePolicy]) {
    case NSURLRequestReloadIgnoringLocalAndRemoteCacheData:
      load_flags |= LOAD_BYPASS_CACHE;
      [[fallthrough]];
    case NSURLRequestReloadIgnoringLocalCacheData:
      load_flags |= LOAD_DISABLE_CACHE;
      break;
    case NSURLRequestReturnCacheDataElseLoad:
      load_flags |= LOAD_SKIP_CACHE_VALIDATION;
      break;
    case NSURLRequestReturnCacheDataDontLoad:
      load_flags |= LOAD_ONLY_FROM_CACHE | LOAD_SKIP_CACHE_VALIDATION;
      break;
    case NSURLRequestReloadRevalidatingCacheData:
      load_flags |= LOAD_VALIDATE_CACHE;
      break;
    case NSURLRequestUseProtocolCachePolicy:
      // Do nothing, normal load.
      break;
  }
  net_request_->SetLoadFlags(load_flags);
}

void HttpProtocolHandlerCore::Start(id<CRNNetworkClientProtocol> base_client) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!client_);
  DCHECK(base_client);
  client_ = base_client;
  GURL url = GURLWithNSURL([request_ URL]);

  // Now that all of the network clients are set up, if there was an error with
  // the URL, it can be raised and all of the clients will have a chance to
  // handle it.
  if (!url.is_valid()) {
    DLOG(ERROR) << "Trying to load an invalid URL: "
                << base::SysNSStringToUTF8([[request_ URL] absoluteString]);
    [client_ didFailWithNSErrorCode:NSURLErrorBadURL
                       netErrorCode:ERR_INVALID_URL];
    return;
  }

  const URLRequestContext* context =
      g_protocol_handler_delegate->GetDefaultURLRequestContext()
          ->GetURLRequestContext();
  DCHECK(context);

  net_request_ =
      context->CreateRequest(url, DEFAULT_PRIORITY, this).release();
  net_request_->set_method(base::SysNSStringToUTF8([request_ HTTPMethod]));

  net_request_->set_site_for_cookies(
      net::SiteForCookies::FromUrl(GURLWithNSURL([request_ mainDocumentURL])));

#if !defined(NDEBUG)
  DVLOG(2) << "From client:";
  LogNSURLRequest(request_);
#endif  // !defined(NDEBUG)

  CopyHttpHeaders(request_, net_request_);

  [client_ didCreateNativeRequest:net_request_];
  SetLoadFlags();
  net_request_->set_allow_credentials([request_ HTTPShouldHandleCookies]);

  // https://crbug.com/979324 If the application app sets HTTPBody, then system
  // creates new NSInputStream every time HTTPBodyStream is called. Get the
  // stream here and hold on to it.
  http_body_stream_ = [request_ HTTPBodyStream];
  if (http_body_stream_) {
    DCHECK(![request_ HTTPBody]);
    http_body_stream_delegate_ =
        [[CRWHTTPStreamDelegate alloc] initWithHttpProtocolHandlerCore:this];
    [http_body_stream_ setDelegate:http_body_stream_delegate_];
    DVLOG(1) << "input_stream " << http_body_stream_ << " delegate "
             << [http_body_stream_ delegate];
    [http_body_stream_ scheduleInRunLoop:[NSRunLoop currentRunLoop]
                                 forMode:NSDefaultRunLoopMode];
    [http_body_stream_ open];

    if (net_request_->extra_request_headers().HasHeader(
            HttpRequestHeaders::kContentLength)) {
      // The request will be started when the stream is fully read.
      return;
    }

    std::unique_ptr<ChunkedDataStreamUploader> uploader =
        std::make_unique<ChunkedDataStreamUploader>(this);
    chunked_uploader_ = uploader->GetWeakPtr();
    net_request_->set_upload(std::move(uploader));
  } else if ([request_ HTTPBody]) {
    DVLOG(1) << "HTTPBody " << [request_ HTTPBody];
    NSData* body = [request_ HTTPBody];
    const NSUInteger body_length = [body length];
    if (body_length > 0) {
      const char* source_bytes = reinterpret_cast<const char*>([body bytes]);
      std::vector<char> owned_data(source_bytes, source_bytes + body_length);
      std::unique_ptr<UploadElementReader> reader(
          new UploadOwnedBytesElementReader(&owned_data));
      net_request_->set_upload(
          ElementsUploadDataStream::CreateWithReader(std::move(reader)));
    }
  }

  net_request_->Start();
}

void HttpProtocolHandlerCore::Cancel() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (net_request_ == nullptr)
    return;

  DVLOG(2) << "Client canceling request: " << net_request_->url().spec();
  net_request_->Cancel();
  StopNetRequest();
}

void HttpProtocolHandlerCore::StopNetRequest() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (g_metrics_delegate) {
    auto metrics = std::make_unique<net::MetricsDelegate::Metrics>();

    metrics->response_end_time = base::Time::Now();
    metrics->task = task_;
    metrics->response_info = net_request_->response_info();
    net_request_->GetLoadTimingInfo(&metrics->load_timing_info);

    g_metrics_delegate->OnStopNetRequest(std::move(metrics));
  }

  delete net_request_;
  net_request_ = nullptr;
  if (http_body_stream_)
    StopListeningStream(http_body_stream_);
}

void HttpProtocolHandlerCore::StopListeningStream(NSStream* stream) {
  DVLOG(1) << "StopListeningStream " << stream << " delegate "
           << [stream delegate];
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(stream);
  DCHECK(http_body_stream_delegate_);
  DCHECK([stream delegate] == http_body_stream_delegate_);
  if (stream != http_body_stream_)
    return;
  [stream setDelegate:nil];
  [stream removeFromRunLoop:[NSRunLoop currentRunLoop]
                    forMode:NSDefaultRunLoopMode];
  http_body_stream_delegate_ = nil;
  http_body_stream_ = nil;
  // Close the stream if needed.
  switch ([stream streamStatus]) {
    case NSStreamStatusOpening:
    case NSStreamStatusOpen:
    case NSStreamStatusReading:
    case NSStreamStatusWriting:
    case NSStreamStatusAtEnd:
      [stream close];
      break;
    case NSStreamStatusNotOpen:
    case NSStreamStatusClosed:
    case NSStreamStatusError:
      break;
    default:
      NOTREACHED_IN_MIGRATION()
          << "Unexpected stream status: " << [stream streamStatus];
      break;
  }
}

NSInteger HttpProtocolHandlerCore::IOSErrorCode(int os_error) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  switch (os_error) {
    case ERR_SSL_PROTOCOL_ERROR:
      return NSURLErrorClientCertificateRequired;
    case ERR_CONNECTION_RESET:
    case ERR_NETWORK_CHANGED:
      return NSURLErrorNetworkConnectionLost;
    case ERR_UNEXPECTED:
      return NSURLErrorUnknown;
    default:
      return NSURLErrorCannotConnectToHost;
  }
}

void HttpProtocolHandlerCore::StopRequestWithError(NSInteger ns_error_code,
                                                   int net_error_code) {
  DCHECK(net_request_ != nullptr);
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // Don't show an error message on ERR_ABORTED because this is error is often
  // fired when switching profiles.
  DLOG_IF(ERROR, net_error_code != ERR_ABORTED)
      << "HttpProtocolHandlerCore - Network error: "
      << ErrorToString(net_error_code) << " (" << net_error_code << ")";

  [client_ didFailWithNSErrorCode:ns_error_code netErrorCode:net_error_code];
  StopNetRequest();
}

void HttpProtocolHandlerCore::StripPostSpecificHeaders(
    NSMutableURLRequest* request) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(request);
  [request setValue:nil forHTTPHeaderField:base::SysUTF8ToNSString(
      HttpRequestHeaders::kContentLength)];
  [request setValue:nil forHTTPHeaderField:base::SysUTF8ToNSString(
      HttpRequestHeaders::kContentType)];
  [request setValue:nil forHTTPHeaderField:base::SysUTF8ToNSString(
      HttpRequestHeaders::kOrigin)];
}

int HttpProtocolHandlerCore::OnRead(char* buffer, int buffer_length) {
  int bytes_read = 0;
  if (http_body_stream_) {
    // NSInputStream read() blocks the thread until there is at least one byte
    // available, so check the status before call read().
    if (![http_body_stream_ hasBytesAvailable])
      return ERR_IO_PENDING;

    bytes_read =
        [http_body_stream_ read:reinterpret_cast<unsigned char*>(buffer)
                      maxLength:buffer_length];
    // NSInputStream can read 0 byte when hasBytesAvailable is true, so do not
    // treat it as a failure.
    if (bytes_read < 0) {
      // If NSInputStream meets an error on read(), fail the request
      // immediately.
      DLOG(ERROR) << "Failed to read POST data: "
                  << base::SysNSStringToUTF8(
                         [[http_body_stream_ streamError] description]);
      StopListeningStream(http_body_stream_);
      StopRequestWithError(NSURLErrorUnknown, ERR_UNEXPECTED);
      return ERR_UNEXPECTED;
    }
  }
  return bytes_read;
}

}  // namespace net

#pragma mark -
#pragma mark CRWHTTPStreamDelegate

@implementation CRWHTTPStreamDelegate
- (instancetype)initWithHttpProtocolHandlerCore:
    (net::HttpProtocolHandlerCore*)core {
  DCHECK(core);
  self = [super init];
  if (self)
    _core = core;
  return self;
}

- (void)stream:(NSStream*)theStream handleEvent:(NSStreamEvent)streamEvent {
  _core->HandleStreamEvent(theStream, streamEvent);
}
@end

#pragma mark -
#pragma mark DeferredCancellation

// An object of class |DeferredCancellation| represents a deferred cancellation
// of a request. In principle this is a block posted to a thread's runloop, but
// since there is no performBlock:onThread:, this class wraps the desired
// behavior in an object.
@interface DeferredCancellation : NSObject

- (instancetype)initWithCore:(scoped_refptr<net::HttpProtocolHandlerCore>)core;
- (void)cancel;

@end

@implementation DeferredCancellation {
  scoped_refptr<net::HttpProtocolHandlerCore> _core;
}

- (instancetype)initWithCore:(scoped_refptr<net::HttpProtocolHandlerCore>)core {
  if ((self = [super init])) {
    _core = core;
  }
  return self;
}

- (void)cancel {
  g_protocol_handler_delegate->GetDefaultURLRequestContext()
      ->GetNetworkTaskRunner()
      ->PostTask(FROM_HERE,
                 base::BindOnce(&net::HttpProtocolHandlerCore::Cancel, _core));
}

@end

#pragma mark -
#pragma mark HttpProtocolHandler

@interface CRNHTTPProtocolHandler (Private)

- (void)ensureProtocolHandlerProxyCreated;
- (void)cancelRequest;

@end

// The HttpProtocolHandler is called by the iOS system to handle the
// NSURLRequest.
@implementation CRNHTTPProtocolHandler {
  scoped_refptr<net::HttpProtocolHandlerCore> _core;
  id<CRNHTTPProtocolHandlerProxy> _protocolProxy;
  __weak NSThread* _clientThread;
  BOOL _supportedURL;
  NSURLSessionTask* _task;
}

#pragma mark NSURLProtocol methods

+ (BOOL)canInitWithRequest:(NSURLRequest*)request {
  DVLOG(5) << "canInitWithRequest " << net::FormatUrlRequestForLogging(request);
  return g_protocol_handler_delegate->CanHandleRequest(request);
}

+ (NSURLRequest*)canonicalRequestForRequest:(NSURLRequest*)request {
  // TODO(droger): Is this used if we disable the cache of UIWebView? If it is,
  // then we need a real implementation, even though Chrome network stack does
  // not need it (GURLs are automatically canonized).
  return request;
}

- (instancetype)initWithRequest:(NSURLRequest*)request
                 cachedResponse:(NSCachedURLResponse*)cachedResponse
                         client:(id<NSURLProtocolClient>)client {
  DCHECK(!cachedResponse);
  self = [super initWithRequest:request
                 cachedResponse:cachedResponse
                         client:client];
  if (self) {
    _supportedURL = g_protocol_handler_delegate->IsRequestSupported(request);
    _core = new net::HttpProtocolHandlerCore(request);
  }
  return self;
}

- (instancetype)initWithTask:(NSURLSessionTask*)task
              cachedResponse:(NSCachedURLResponse*)cachedResponse
                      client:(id<NSURLProtocolClient>)client {
  DCHECK(!cachedResponse);
  self = [super initWithTask:task cachedResponse:cachedResponse client:client];
  if (self) {
    _supportedURL =
        g_protocol_handler_delegate->IsRequestSupported(task.currentRequest);
    _core = new net::HttpProtocolHandlerCore(task);
    _task = task;
  }
  return self;
}

#pragma mark NSURLProtocol overrides.

- (NSCachedURLResponse*)cachedResponse {
  // We do not use the UIWebView cache.
  // TODO(droger): Disable the UIWebView cache.
  return nil;
}

- (void)startLoading {
  // If the scheme is not valid, just return an error right away.
  if (!_supportedURL) {
    NSMutableDictionary* dictionary = [NSMutableDictionary dictionary];

    // It is possible for URL to be nil, so check for that
    // before creating the error object. See http://crbug/349051
    NSURL* url = [[self request] URL];
    if (url)
      [dictionary setObject:url forKey:NSURLErrorKey];

    NSError* error = [NSError errorWithDomain:NSURLErrorDomain
                                         code:NSURLErrorUnsupportedURL
                                     userInfo:dictionary];
    [[self client] URLProtocol:self didFailWithError:error];
    return;
  }

  _clientThread = [NSThread currentThread];

  if (g_metrics_delegate) {
    g_metrics_delegate->OnStartNetRequest(_task);
  }

  // The closure passed to PostTask must retain the _protocolProxy. A call to
  // ensureProtocolHandlerProxyCreated before passing _protocolProxy ensure that
  // _protocolProxy is instantiated before passing it.
  [self ensureProtocolHandlerProxyCreated];
  DCHECK(_protocolProxy);
  g_protocol_handler_delegate->GetDefaultURLRequestContext()
      ->GetNetworkTaskRunner()
      ->PostTask(FROM_HERE, base::BindOnce(&net::HttpProtocolHandlerCore::Start,
                                           _core, _protocolProxy));
}

- (void)ensureProtocolHandlerProxyCreated {
  DCHECK_EQ([NSThread currentThread], _clientThread);
  if (!_protocolProxy) {
    _protocolProxy = [[CRNHTTPProtocolHandlerProxyWithClientThread alloc]
        initWithProtocol:self
            clientThread:_clientThread
             runLoopMode:[[NSRunLoop currentRunLoop] currentMode]];
  }
}

- (void)cancelRequest {
  g_protocol_handler_delegate->GetDefaultURLRequestContext()
      ->GetNetworkTaskRunner()
      ->PostTask(FROM_HERE,
                 base::BindOnce(&net::HttpProtocolHandlerCore::Cancel, _core));
  [_protocolProxy invalidate];
}

- (void)stopLoading {
  [self cancelRequest];
  _protocolProxy = nil;
}

@end
