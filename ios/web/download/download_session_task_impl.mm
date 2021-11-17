// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/download/download_session_task_impl.h"

#import <WebKit/WebKit.h>

#include "base/strings/sys_string_conversions.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#import "ios/net/cookies/system_cookie_util.h"
#include "ios/web/public/browser_state.h"
#import "ios/web/public/download/download_task_observer.h"
#include "ios/web/public/thread/web_task_traits.h"
#include "ios/web/public/thread/web_thread.h"
#import "ios/web/public/web_state.h"
#import "ios/web/web_view/error_translation_util.h"
#include "net/base/completion_once_callback.h"
#include "net/base/data_url.h"
#include "net/base/io_buffer.h"
#import "net/base/mac/url_conversions.h"
#include "net/cookies/cookie_store.h"
#include "net/url_request/url_fetcher_response_writer.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using web::WebThread;

namespace {

// Updates DownloadSessionTaskImpl properties. |terminal_callback| is true if
// this is the last update for this DownloadSessionTaskImpl.
using PropertiesBlock = void (^)(NSURLSessionTask*,
                                 NSError*,
                                 bool terminal_callback);
// Writes buffer and calls |completionHandler| when done.
using DataBlock = void (^)(scoped_refptr<net::IOBufferWithSize> buffer,
                           void (^completionHandler)());

// Translates an CFNetwork error code to a net error code. Returns 0 if |error|
// is nil.
int GetNetErrorCodeFromNSError(NSError* error, NSURL* url) {
  int error_code = 0;
  if (error) {
    if (!web::GetNetErrorFromIOSErrorCode(error.code, &error_code, url)) {
      error_code = net::ERR_FAILED;
    }
  }
  return error_code;
}

// Creates a new buffer from raw |data| and |size|.
scoped_refptr<net::IOBufferWithSize> GetBuffer(const void* data, size_t size) {
  auto buffer = base::MakeRefCounted<net::IOBufferWithSize>(size);
  memcpy(buffer->data(), data, size);
  return buffer;
}

// Percent complete for the given NSURLSessionTask within [0..100] range.
int GetTaskPercentComplete(NSURLSessionTask* task) {
  DCHECK(task);
  if (!task.countOfBytesExpectedToReceive) {
    return 100;
  }
  if (task.countOfBytesExpectedToReceive == -1) {
    return -1;
  }
  DCHECK_GE(task.countOfBytesExpectedToReceive, task.countOfBytesReceived);
  return 100.0 * task.countOfBytesReceived / task.countOfBytesExpectedToReceive;
}

}  // namespace

// NSURLSessionDataDelegate that forwards data and properties task updates to
// the client. Client of this delegate can pass blocks to receive the updates.
@interface CRWURLSessionDelegate : NSObject <NSURLSessionDataDelegate>

// Called when DownloadSessionTaskImpl should update its properties (is_done,
// error_code, total_bytes, and percent_complete) and call OnDownloadUpdated
// callback.
@property(nonatomic, readonly) PropertiesBlock propertiesBlock;

// Called when DownloadSessionTaskImpl should write a chunk of downloaded data.
@property(nonatomic, readonly) DataBlock dataBlock;

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithPropertiesBlock:(PropertiesBlock)propertiesBlock
                              dataBlock:(DataBlock)dataBlock
    NS_DESIGNATED_INITIALIZER;
@end

@implementation CRWURLSessionDelegate

@synthesize propertiesBlock = _propertiesBlock;
@synthesize dataBlock = _dataBlock;

- (instancetype)initWithPropertiesBlock:(PropertiesBlock)propertiesBlock
                              dataBlock:(DataBlock)dataBlock {
  DCHECK(propertiesBlock);
  DCHECK(dataBlock);
  if ((self = [super init])) {
    _propertiesBlock = propertiesBlock;
    _dataBlock = dataBlock;
  }
  return self;
}

- (void)URLSession:(NSURLSession*)session
                    task:(NSURLSessionTask*)task
    didCompleteWithError:(NSError*)error {
  __weak CRWURLSessionDelegate* weakSelf = self;
  base::PostTask(FROM_HERE, {WebThread::UI}, base::BindOnce(^{
                   CRWURLSessionDelegate* strongSelf = weakSelf;
                   if (strongSelf.propertiesBlock)
                     strongSelf.propertiesBlock(task, error,
                                                /*terminal_callback=*/true);
                 }));
}

- (void)URLSession:(NSURLSession*)session
          dataTask:(NSURLSessionDataTask*)task
    didReceiveData:(NSData*)data {
  // Block this background queue until the the data is written.
  dispatch_semaphore_t semaphore = dispatch_semaphore_create(0);
  __weak CRWURLSessionDelegate* weakSelf = self;
  using Bytes = const void*;
  [data enumerateByteRangesUsingBlock:^(Bytes bytes, NSRange range, BOOL*) {
    auto buffer = GetBuffer(bytes, range.length);
    base::PostTask(FROM_HERE, {WebThread::UI}, base::BindOnce(^{
                     CRWURLSessionDelegate* strongSelf = weakSelf;
                     if (!strongSelf.dataBlock) {
                       dispatch_semaphore_signal(semaphore);
                       return;
                     }
                     strongSelf.dataBlock(std::move(buffer), ^{
                       // Data was written to disk, unblock queue to
                       // read the next chunk of downloaded data.
                       dispatch_semaphore_signal(semaphore);
                     });
                   }));
    dispatch_semaphore_wait(semaphore, DISPATCH_TIME_FOREVER);
  }];
  base::PostTask(FROM_HERE, {WebThread::UI}, base::BindOnce(^{
                   CRWURLSessionDelegate* strongSelf = weakSelf;
                   if (strongSelf.propertiesBlock)
                     weakSelf.propertiesBlock(task, nil,
                                              /*terminal_callback=*/false);
                 }));
}

- (void)URLSession:(NSURLSession*)session
    didReceiveChallenge:(NSURLAuthenticationChallenge*)challenge
      completionHandler:(void (^)(NSURLSessionAuthChallengeDisposition,
                                  NSURLCredential*))handler {
  // TODO(crbug.com/780911): use CRWCertVerificationController to get
  // CertAcceptPolicy for this |challenge|.
  handler(NSURLSessionAuthChallengeRejectProtectionSpace, nil);
}

@end

namespace web {
DownloadSessionTaskImpl::DownloadSessionTaskImpl(
    WebState* web_state,
    const GURL& original_url,
    NSString* http_method,
    const std::string& content_disposition,
    int64_t total_bytes,
    const std::string& mime_type,
    NSString* identifier,
    Delegate* delegate)
    : DownloadTaskImpl(web_state,
                       original_url,
                       http_method,
                       content_disposition,
                       total_bytes,
                       mime_type,
                       identifier,
                       delegate) {}

DownloadSessionTaskImpl::~DownloadSessionTaskImpl() {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  [session_task_ cancel];
  session_task_ = nil;
}

NSData* DownloadSessionTaskImpl::GetResponseData() const {
  DCHECK(writer_);
  net::URLFetcherStringWriter* string_writer = writer_->AsStringWriter();
  if (string_writer) {
    const std::string& data = string_writer->data();
    return [NSData dataWithBytes:data.c_str() length:data.size()];
  }

  net::URLFetcherFileWriter* file_writer = writer_->AsFileWriter();
  if (file_writer) {
    const base::FilePath& path = file_writer->file_path();
    return [NSData
        dataWithContentsOfFile:base::SysUTF8ToNSString(path.AsUTF8Unsafe())];
  }

  return nil;
}

const base::FilePath& DownloadSessionTaskImpl::GetResponsePath() const {
  DCHECK(writer_);
  net::URLFetcherFileWriter* file_writer = writer_->AsFileWriter();
  if (file_writer) {
    const base::FilePath& path = file_writer->file_path();
    return path;
  }

  static const base::FilePath kEmptyPath;
  return kEmptyPath;
}

void DownloadSessionTaskImpl::Start(const base::FilePath& path,
                                    Destination destination_hint) {
  DownloadTaskImpl::Start(path, destination_hint);
  if (destination_hint == Destination::kToMemory) {
    OnWriterInitialized(std::make_unique<net::URLFetcherStringWriter>(),
                        net::OK);
  } else {
    DCHECK(path != base::FilePath());
    auto task_runner = base::ThreadPool::CreateSequencedTaskRunner(
        {base::MayBlock(), base::TaskPriority::USER_VISIBLE});
    auto writer =
        std::make_unique<net::URLFetcherFileWriter>(task_runner, path);
    net::URLFetcherFileWriter* writer_ptr = writer.get();
    writer_ptr->Initialize(
        base::BindOnce(&DownloadSessionTaskImpl::OnWriterInitialized,
                       weak_factory_.GetWeakPtr(), std::move(writer)));
  }
}

void DownloadSessionTaskImpl::Cancel() {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  [session_task_ cancel];
  session_task_ = nil;
  DownloadTaskImpl::Cancel();
}

void DownloadSessionTaskImpl::ShutDown() {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  [session_task_ cancel];
  session_task_ = nil;
  DownloadTaskImpl::ShutDown();
}

void DownloadSessionTaskImpl::OnDownloadFinished(int error_code) {
  // If downloads manager's flag is enabled, keeps the downloaded file. The
  // writer deletes it if it owns it, that's why it shouldn't owns it anymore
  // when the current download is finished.
  // Check if writer_->AsFileWriter() is necessary because in some cases the
  // writer isn't a fileWriter as for PaFGsskit downloads for example.
  if (writer_->AsFileWriter())
    writer_->AsFileWriter()->DisownFile();
  session_task_ = nil;
  DownloadTaskImpl::OnDownloadFinished(error_code);
}

void DownloadSessionTaskImpl::OnWriterInitialized(
    std::unique_ptr<net::URLFetcherResponseWriter> writer,
    int writer_initialization_status) {
  DCHECK_EQ(state_, State::kInProgress);
  writer_ = std::move(writer);

  if (writer_initialization_status != net::OK) {
    OnDownloadFinished(writer_initialization_status);
  } else if (original_url_.SchemeIs(url::kDataScheme)) {
    StartDataUrlParsing();
  } else {
    GetCookies(base::BindRepeating(&DownloadSessionTaskImpl::StartWithCookies,
                                   weak_factory_.GetWeakPtr()));
  }
}

NSURLSession* DownloadSessionTaskImpl::CreateSession(
    NSString* identifier,
    NSArray<NSHTTPCookie*>* cookies) {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  DCHECK(identifier.length);
  base::WeakPtr<DownloadSessionTaskImpl> weak_this = weak_factory_.GetWeakPtr();
  id<NSURLSessionDataDelegate> session_delegate = [[CRWURLSessionDelegate alloc]
      initWithPropertiesBlock:^(NSURLSessionTask* task, NSError* error,
                                bool terminal_callback) {
        if (!weak_this.get()) {
          return;
        }

        error_code_ =
            GetNetErrorCodeFromNSError(error, task.currentRequest.URL);
        percent_complete_ = GetTaskPercentComplete(task);
        received_bytes_ = task.countOfBytesReceived;
        if (total_bytes_ == -1 || task.countOfBytesExpectedToReceive) {
          // countOfBytesExpectedToReceive can be 0 if the device is offline.
          // In that case total_bytes_ should remain unchanged if the total
          // bytes count is already known.
          total_bytes_ = task.countOfBytesExpectedToReceive;
        }
        if (task.response.MIMEType) {
          mime_type_ = base::SysNSStringToUTF8(task.response.MIMEType);
        }
        if ([task.response isKindOfClass:[NSHTTPURLResponse class]]) {
          http_code_ =
              static_cast<NSHTTPURLResponse*>(task.response).statusCode;
        }

        if (!terminal_callback) {
          OnDownloadUpdated();
          // Download is still in progress, nothing to do here.
          return;
        }

        // Download has finished, so finalize the writer and signal completion.
        auto callback =
            base::BindOnce(&DownloadSessionTaskImpl::OnDownloadFinished,
                           weak_factory_.GetWeakPtr());
        if (writer_->Finish(error_code_, std::move(callback)) !=
            net::ERR_IO_PENDING) {
          OnDownloadFinished(error_code_);
        }
      }
      dataBlock:^(scoped_refptr<net::IOBufferWithSize> buffer,
                  void (^completion_handler)()) {
        if (weak_this.get()) {
          net::CompletionOnceCallback callback = base::BindOnce(^(int) {
            completion_handler();
          });
          if (writer_->Write(buffer.get(), buffer->size(),
                             std::move(callback)) == net::ERR_IO_PENDING) {
            return;
          }
        }
        completion_handler();
      }];
  return delegate_->CreateSession(identifier, cookies, session_delegate,
                                  /*queue=*/nil);
}

void DownloadSessionTaskImpl::GetCookies(
    base::OnceCallback<void(NSArray<NSHTTPCookie*>*)> callback) {
  DCHECK_CURRENTLY_ON(WebThread::UI);
  scoped_refptr<net::URLRequestContextGetter> context_getter(
      web_state_->GetBrowserState()->GetRequestContext());

  // net::URLRequestContextGetter must be used in the IO thread.
  base::PostTask(
      FROM_HERE, {WebThread::IO},
      base::BindOnce(&DownloadSessionTaskImpl::GetCookiesFromContextGetter,
                     context_getter, std::move(callback)));
}

void DownloadSessionTaskImpl::GetCookiesFromContextGetter(
    scoped_refptr<net::URLRequestContextGetter> context_getter,
    base::OnceCallback<void(NSArray<NSHTTPCookie*>*)> callback) {
  DCHECK_CURRENTLY_ON(WebThread::IO);
  context_getter->GetURLRequestContext()->cookie_store()->GetAllCookiesAsync(
      base::BindOnce(
          [](base::OnceCallback<void(NSArray<NSHTTPCookie*>*)> callback,
             const net::CookieList& cookie_list) {
            NSArray<NSHTTPCookie*>* cookies =
                SystemCookiesFromCanonicalCookieList(cookie_list);
            base::PostTask(FROM_HERE, {WebThread::UI},
                           base::BindOnce(std::move(callback), cookies));
          },
          std::move(callback)));
}

void DownloadSessionTaskImpl::StartWithCookies(
    NSArray<NSHTTPCookie*>* cookies) {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  DCHECK(writer_);

  if (!session_) {
    session_ = CreateSession(identifier_, cookies);
    DCHECK(session_);
  }

  has_performed_background_download_ =
      UIApplication.sharedApplication.applicationState !=
      UIApplicationStateActive;

  NSURL* url = net::NSURLWithGURL(GetOriginalUrl());
  NSMutableURLRequest* request = [[NSMutableURLRequest alloc] initWithURL:url];
  request.HTTPMethod = GetHttpMethod();
  session_task_ = [session_ dataTaskWithRequest:request];
  [session_task_ resume];
  OnDownloadUpdated();
}

void DownloadSessionTaskImpl::StartDataUrlParsing() {
  mime_type_.clear();
  std::string charset;
  std::string data;
  if (!net::DataURL::Parse(original_url_, &mime_type_, &charset, &data)) {
    OnDownloadFinished(net::ERR_INVALID_URL);
    return;
  }
  auto callback = base::BindOnce(&DownloadSessionTaskImpl::OnDataUrlWritten,
                                 weak_factory_.GetWeakPtr());
  auto buffer = base::MakeRefCounted<net::IOBuffer>(data.size());
  memcpy(buffer->data(), data.c_str(), data.size());
  int written = writer_->Write(buffer.get(), data.size(), std::move(callback));
  if (written != net::ERR_IO_PENDING) {
    OnDataUrlWritten(written);
  }
}

void DownloadSessionTaskImpl::OnDataUrlWritten(int bytes_written) {
  percent_complete_ = 100;
  total_bytes_ = bytes_written;
  received_bytes_ = total_bytes_;
  auto callback = base::BindOnce(&DownloadSessionTaskImpl::OnDownloadFinished,
                                 weak_factory_.GetWeakPtr());
  if (writer_->Finish(net::OK, std::move(callback)) != net::ERR_IO_PENDING) {
    OnDownloadFinished(net::OK);
  }
}
}  // namespace web
