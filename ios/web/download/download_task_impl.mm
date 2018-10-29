// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/download/download_task_impl.h"

#import <Foundation/Foundation.h>
#import <WebKit/WebKit.h>

#include "base/bind.h"
#include "base/strings/sys_string_conversions.h"
#include "base/task/post_task.h"
#import "ios/web/net/cookies/wk_cookie_util.h"
#include "ios/web/public/browser_state.h"
#import "ios/web/public/download/download_task_observer.h"
#import "ios/web/public/web_state/web_state.h"
#include "ios/web/public/web_task_traits.h"
#include "ios/web/public/web_thread.h"
#import "ios/web/web_state/error_translation_util.h"
#include "net/base/data_url.h"
#include "net/base/filename_util.h"
#include "net/base/io_buffer.h"
#import "net/base/mac/url_conversions.h"
#include "net/base/net_errors.h"
#include "net/url_request/url_fetcher_response_writer.h"
#include "url/url_constants.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using web::WebThread;

namespace {

// Updates DownloadTaskImpl properties.
using PropertiesBlock = void (^)(NSURLSessionTask*, NSError*);
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
@interface CRWURLSessionDelegate : NSObject<NSURLSessionDataDelegate>

// Called when DownloadTaskImpl should update its properties (is_done,
// error_code, total_bytes, and percent_complete) and call OnDownloadUpdated
// callback.
@property(nonatomic, readonly) PropertiesBlock propertiesBlock;

// Called when DownloadTaskImpl should write a chunk of downloaded data.
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
    didCompleteWithError:(nullable NSError*)error {
  __weak CRWURLSessionDelegate* weakSelf = self;
  base::PostTaskWithTraits(FROM_HERE, {WebThread::UI}, base::BindOnce(^{
                             CRWURLSessionDelegate* strongSelf = weakSelf;
                             if (strongSelf.propertiesBlock)
                               strongSelf.propertiesBlock(task, error);
                           }));
}

- (void)URLSession:(NSURLSession*)session
          dataTask:(NSURLSessionDataTask*)task
    didReceiveData:(NSData*)data {
  // Block this background queue until the the data is written.
  dispatch_semaphore_t semaphore = dispatch_semaphore_create(0);
  __weak CRWURLSessionDelegate* weakSelf = self;
  using Bytes = const void* _Nonnull;
  [data enumerateByteRangesUsingBlock:^(Bytes bytes, NSRange range, BOOL*) {
    auto buffer = GetBuffer(bytes, range.length);
    base::PostTaskWithTraits(FROM_HERE, {WebThread::UI}, base::BindOnce(^{
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
  base::PostTaskWithTraits(FROM_HERE, {WebThread::UI}, base::BindOnce(^{
                             CRWURLSessionDelegate* strongSelf = weakSelf;
                             if (strongSelf.propertiesBlock)
                               weakSelf.propertiesBlock(task, nil);
                           }));
}

- (void)URLSession:(NSURLSession*)session
    didReceiveChallenge:(NSURLAuthenticationChallenge*)challenge
      completionHandler:(void (^)(NSURLSessionAuthChallengeDisposition,
                                  NSURLCredential* _Nullable))handler {
  // TODO(crbug.com/780911): use CRWCertVerificationController to get
  // CertAcceptPolicy for this |challenge|.
  handler(NSURLSessionAuthChallengeRejectProtectionSpace, nil);
}

@end

namespace web {

DownloadTaskImpl::DownloadTaskImpl(const WebState* web_state,
                                   const GURL& original_url,
                                   const std::string& content_disposition,
                                   int64_t total_bytes,
                                   const std::string& mime_type,
                                   ui::PageTransition page_transition,
                                   NSString* identifier,
                                   Delegate* delegate)
    : original_url_(original_url),
      total_bytes_(total_bytes),
      content_disposition_(content_disposition),
      mime_type_(mime_type),
      page_transition_(page_transition),
      identifier_([identifier copy]),
      web_state_(web_state),
      delegate_(delegate),
      weak_factory_(this) {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  DCHECK(web_state_);
  DCHECK(delegate_);

  observer_ = [NSNotificationCenter.defaultCenter
      addObserverForName:UIApplicationWillResignActiveNotification
                  object:nil
                   queue:nil
              usingBlock:^(NSNotification* _Nonnull) {
                if (state_ == State::kInProgress) {
                  has_performed_background_download_ = true;
                }
              }];
}

DownloadTaskImpl::~DownloadTaskImpl() {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  [NSNotificationCenter.defaultCenter removeObserver:observer_];
  for (auto& observer : observers_)
    observer.OnDownloadDestroyed(this);

  if (delegate_) {
    delegate_->OnTaskDestroyed(this);
  }
  ShutDown();
}

void DownloadTaskImpl::ShutDown() {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  [session_task_ cancel];
  session_task_ = nil;
  delegate_ = nullptr;
}

DownloadTask::State DownloadTaskImpl::GetState() const {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  return state_;
}

void DownloadTaskImpl::Start(
    std::unique_ptr<net::URLFetcherResponseWriter> writer) {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  DCHECK_NE(state_, State::kInProgress);
  writer_ = std::move(writer);
  percent_complete_ = 0;
  received_bytes_ = 0;
  state_ = State::kInProgress;

  if (original_url_.SchemeIs(url::kDataScheme)) {
    StartDataUrlParsing();
  } else {
    GetCookies(base::Bind(&DownloadTaskImpl::StartWithCookies,
                          weak_factory_.GetWeakPtr()));
  }
}

void DownloadTaskImpl::Cancel() {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  [session_task_ cancel];
  session_task_ = nil;
  state_ = State::kCancelled;
  OnDownloadUpdated();
}

net::URLFetcherResponseWriter* DownloadTaskImpl::GetResponseWriter() const {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  return writer_.get();
}

NSString* DownloadTaskImpl::GetIndentifier() const {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  return identifier_;
}

const GURL& DownloadTaskImpl::GetOriginalUrl() const {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  return original_url_;
}

bool DownloadTaskImpl::IsDone() const {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  return state_ == State::kComplete || state_ == State::kCancelled;
}

int DownloadTaskImpl::GetErrorCode() const {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  return error_code_;
}

int DownloadTaskImpl::GetHttpCode() const {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  return http_code_;
}

int64_t DownloadTaskImpl::GetTotalBytes() const {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  return total_bytes_;
}

int64_t DownloadTaskImpl::GetReceivedBytes() const {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  return received_bytes_;
}

int DownloadTaskImpl::GetPercentComplete() const {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  return percent_complete_;
}

std::string DownloadTaskImpl::GetContentDisposition() const {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  return content_disposition_;
}

std::string DownloadTaskImpl::GetMimeType() const {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  return mime_type_;
}

ui::PageTransition DownloadTaskImpl::GetTransitionType() const {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  return page_transition_;
}

base::string16 DownloadTaskImpl::GetSuggestedFilename() const {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  return net::GetSuggestedFilename(GetOriginalUrl(), GetContentDisposition(),
                                   /*referrer_charset=*/std::string(),
                                   /*suggested_name=*/std::string(),
                                   /*mime_type=*/std::string(),
                                   /*default_name=*/"document");
}

bool DownloadTaskImpl::HasPerformedBackgroundDownload() const {
  return has_performed_background_download_;
}

void DownloadTaskImpl::AddObserver(DownloadTaskObserver* observer) {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  DCHECK(!observers_.HasObserver(observer));
  observers_.AddObserver(observer);
}

void DownloadTaskImpl::RemoveObserver(DownloadTaskObserver* observer) {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  DCHECK(observers_.HasObserver(observer));
  observers_.RemoveObserver(observer);
}

NSURLSession* DownloadTaskImpl::CreateSession(NSString* identifier) {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  DCHECK(identifier.length);
  base::WeakPtr<DownloadTaskImpl> weak_this = weak_factory_.GetWeakPtr();
  id<NSURLSessionDataDelegate> session_delegate = [[CRWURLSessionDelegate alloc]
      initWithPropertiesBlock:^(NSURLSessionTask* task, NSError* error) {
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

        if (task.state != NSURLSessionTaskStateCompleted) {
          OnDownloadUpdated();
          // Download is still in progress, nothing to do here.
          return;
        }

        // Download has finished, so finalize the writer and signal completion.
        auto callback = base::Bind(&DownloadTaskImpl::OnDownloadFinished,
                                   weak_factory_.GetWeakPtr());
        if (writer_->Finish(error_code_, callback) != net::ERR_IO_PENDING) {
          OnDownloadFinished(error_code_);
        }
      }
      dataBlock:^(scoped_refptr<net::IOBufferWithSize> buffer,
                  void (^completion_handler)()) {
        if (weak_this.get()) {
          net::CompletionCallback callback = base::BindRepeating(^(int) {
            completion_handler();
          });
          if (writer_->Write(buffer.get(), buffer->size(), callback) ==
              net::ERR_IO_PENDING) {
            return;
          }
        }
        completion_handler();
      }];
  return delegate_->CreateSession(identifier, session_delegate, /*queue=*/nil);
}

void DownloadTaskImpl::GetCookies(
    base::Callback<void(NSArray<NSHTTPCookie*>*)> callback) {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  if (@available(iOS 11, *)) {
    GetWKCookies(callback);
  } else {
    base::PostTaskWithTraits(FROM_HERE, {WebThread::UI}, base::BindOnce(^{
                               callback.Run([NSArray array]);
                             }));
  }
}

void DownloadTaskImpl::GetWKCookies(
    base::Callback<void(NSArray<NSHTTPCookie*>*)> callback) {
  DCHECK_CURRENTLY_ON(WebThread::UI);
  auto store = WKCookieStoreForBrowserState(web_state_->GetBrowserState());
  DCHECK(store);
  [store getAllCookies:^(NSArray<NSHTTPCookie*>* cookies) {
    // getAllCookies: callback is always called on UI thread.
    DCHECK_CURRENTLY_ON(WebThread::UI);
    callback.Run(cookies);
  }];
}

void DownloadTaskImpl::StartWithCookies(NSArray<NSHTTPCookie*>* cookies) {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  DCHECK(writer_);

  if (!session_) {
    session_ = CreateSession(identifier_);
    DCHECK(session_);
  }

  has_performed_background_download_ =
      UIApplication.sharedApplication.applicationState !=
      UIApplicationStateActive;

  NSURL* url = net::NSURLWithGURL(GetOriginalUrl());
  session_task_ = [session_ dataTaskWithURL:url];
  [session_.configuration.HTTPCookieStorage storeCookies:cookies
                                                 forTask:session_task_];
  [session_task_ resume];
  OnDownloadUpdated();
}

void DownloadTaskImpl::StartDataUrlParsing() {
  mime_type_.clear();
  std::string charset;
  std::string data;
  if (!net::DataURL::Parse(original_url_, &mime_type_, &charset, &data)) {
    OnDownloadFinished(net::ERR_INVALID_URL);
    return;
  }
  auto callback = base::BindOnce(&DownloadTaskImpl::OnDataUrlWritten,
                                 weak_factory_.GetWeakPtr());
  auto buffer = base::MakeRefCounted<net::IOBuffer>(data.size());
  memcpy(buffer->data(), data.c_str(), data.size());
  int written = writer_->Write(buffer.get(), data.size(), std::move(callback));
  if (written != net::ERR_IO_PENDING) {
    OnDataUrlWritten(written);
  }
}

void DownloadTaskImpl::OnDownloadUpdated() {
  for (auto& observer : observers_)
    observer.OnDownloadUpdated(this);
}

void DownloadTaskImpl::OnDownloadFinished(int error_code) {
  error_code_ = error_code;
  state_ = State::kComplete;
  session_task_ = nil;
  OnDownloadUpdated();
}

void DownloadTaskImpl::OnDataUrlWritten(int bytes_written) {
  percent_complete_ = 100;
  total_bytes_ = bytes_written;
  received_bytes_ = total_bytes_;
  auto callback = base::BindOnce(&DownloadTaskImpl::OnDownloadFinished,
                                 weak_factory_.GetWeakPtr());
  if (writer_->Finish(net::OK, std::move(callback)) != net::ERR_IO_PENDING) {
    OnDownloadFinished(net::OK);
  }
}

}  // namespace web
