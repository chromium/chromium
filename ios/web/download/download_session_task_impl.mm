// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/download/download_session_task_impl.h"

#import <WebKit/WebKit.h>

#import "base/mac/foundation_util.h"
#import "base/strings/sys_string_conversions.h"
#import "base/task/bind_post_task.h"
#import "base/task/thread_pool.h"
#import "base/threading/sequenced_task_runner_handle.h"
#import "ios/net/cookies/system_cookie_util.h"
#import "ios/web/download/download_result.h"
#import "ios/web/public/browser_state.h"
#import "ios/web/public/download/download_task_observer.h"
#import "ios/web/public/thread/web_task_traits.h"
#import "ios/web/public/thread/web_thread.h"
#import "ios/web/public/web_state.h"
#import "ios/web/web_view/error_translation_util.h"
#import "net/base/completion_once_callback.h"
#import "net/base/data_url.h"
#import "net/base/io_buffer.h"
#import "net/base/mac/url_conversions.h"
#import "net/cookies/cookie_store.h"
#import "net/url_request/url_fetcher_response_writer.h"
#import "net/url_request/url_request_context.h"
#import "net/url_request/url_request_context_getter.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// A callback that will be invoked when -URLSession:task:didCompleteWithError:
// from NSURLSessionDataDelegate is called. In case of success, error will be
// nil.
using DoneCallback =
    base::RepeatingCallback<void(NSURLSessionTask* task, NSError* error)>;

// A callback that will be invoked when -URLSession:dataTask:didReceiveData:
// from NSURLSessionDataDelegate is called. The completion handler *must* be
// called at some point. This can be done asynchronously.
using DataCallback =
    base::RepeatingCallback<void(NSURLSessionTask* task,
                                 NSData* data,
                                 ProceduralBlock completion_handler)>;

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

// Asynchronously returns cookies for |context_getter|. Must
// be called on IO thread. The callback will be invoked on the UI thread.
void GetCookiesFromContextGetter(
    scoped_refptr<net::URLRequestContextGetter> context_getter,
    base::OnceCallback<void(NSArray<NSHTTPCookie*>*)> callback) {
  context_getter->GetURLRequestContext()->cookie_store()->GetAllCookiesAsync(
      base::BindOnce(&net::SystemCookiesFromCanonicalCookieList)
          .Then(std::move(callback)));
}

}  // namespace

// NSURLSessionDataDelegate that forwards data and properties task updates to
// the client. Client of this delegate can pass blocks to receive the updates.
@interface CRWURLSessionDelegate : NSObject <NSURLSessionDataDelegate>

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithTaskRunner:(scoped_refptr<base::TaskRunner>)taskRunner
                      dataCallback:(DataCallback)dataCallback
                      doneCallback:(DoneCallback)doneCallback
    NS_DESIGNATED_INITIALIZER;
@end

@implementation CRWURLSessionDelegate {
  scoped_refptr<base::TaskRunner> _taskRunner;
  DataCallback _dataCallback;
  DoneCallback _doneCallback;
}

- (instancetype)initWithTaskRunner:(scoped_refptr<base::TaskRunner>)taskRunner
                      dataCallback:(DataCallback)dataCallback
                      doneCallback:(DoneCallback)doneCallback {
  DCHECK(taskRunner);
  DCHECK(!dataCallback.is_null());
  DCHECK(!doneCallback.is_null());
  if ((self = [super init])) {
    _taskRunner = std::move(taskRunner);
    _dataCallback = std::move(dataCallback);
    _doneCallback = std::move(doneCallback);
  }
  return self;
}

- (void)URLSession:(NSURLSession*)session
                    task:(NSURLSessionTask*)task
    didCompleteWithError:(NSError*)error {
  _taskRunner->PostTask(FROM_HERE, base::BindOnce(_doneCallback, task, error));
}

- (void)URLSession:(NSURLSession*)session
          dataTask:(NSURLSessionDataTask*)task
    didReceiveData:(NSData*)data {
  // net::URLFetcherFileWriter does not support trying to write data when
  // a previous write is still pending. As this callback is called on a
  // background queue, block it until the write has completed on the IO
  // thread (in net::URLFetcherFileWriter::Write()).
  dispatch_semaphore_t semaphore = dispatch_semaphore_create(0);

  _taskRunner->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](dispatch_semaphore_t semaphore, DataCallback innerDataCallback,
             NSURLSessionTask* task, NSData* data) {
            if (innerDataCallback.IsCancelled()) {
              // If the callback is cancelled, then signal the semaphore to
              // allow the background queue to resume its progress. Cancel
              // the task to avoid unnecessarily consuming data from user.
              dispatch_semaphore_signal(semaphore);

              [task cancel];
              return;
            }

            innerDataCallback.Run(task, data, ^() {
              // Data was written to disk, unblock queue to read the
              // next chunk of downloaded data.
              dispatch_semaphore_signal(semaphore);
            });
          },
          semaphore, _dataCallback, task, data));

  // Block this background queue until the the data is written.
  dispatch_semaphore_wait(semaphore, DISPATCH_TIME_FOREVER);
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
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  [session_task_ cancel];
  session_task_ = nil;
}

NSData* DownloadSessionTaskImpl::GetResponseData() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
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
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
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
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
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
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  [session_task_ cancel];
  session_task_ = nil;
  DownloadTaskImpl::Cancel();
}

void DownloadSessionTaskImpl::ShutDown() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  [session_task_ cancel];
  session_task_ = nil;
  DownloadTaskImpl::ShutDown();
}

void DownloadSessionTaskImpl::OnWriterDownloadFinished(int error_code) {
  // If downloads manager's flag is enabled, keeps the downloaded file. The
  // writer deletes it if it owns it, that's why it shouldn't owns it anymore
  // when the current download is finished.
  // Check if writer_->AsFileWriter() is necessary because in some cases the
  // writer isn't a fileWriter as for PaFGsskit downloads for example.
  if (writer_->AsFileWriter())
    writer_->AsFileWriter()->DisownFile();
  session_task_ = nil;
  DownloadTaskImpl::OnDownloadFinished(DownloadResult(error_code));
}

void DownloadSessionTaskImpl::OnWriterInitialized(
    std::unique_ptr<net::URLFetcherResponseWriter> writer,
    int writer_initialization_status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(state_, State::kInProgress);
  writer_ = std::move(writer);
  DCHECK(writer_);

  if (writer_initialization_status != net::OK) {
    OnWriterDownloadFinished(writer_initialization_status);
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
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(identifier.length);

  base::WeakPtr<DownloadSessionTaskImpl> weak_this = weak_factory_.GetWeakPtr();
  DataCallback data_callback =
      base::BindRepeating(&DownloadSessionTaskImpl::OnTaskData, weak_this);
  DoneCallback done_callback =
      base::BindRepeating(&DownloadSessionTaskImpl::OnTaskDone, weak_this);

  id<NSURLSessionDataDelegate> session_delegate = [[CRWURLSessionDelegate alloc]
      initWithTaskRunner:base::SequencedTaskRunnerHandle::Get()
            dataCallback:std::move(data_callback)
            doneCallback:std::move(done_callback)];
  return delegate_->CreateSession(identifier, cookies, session_delegate,
                                  /*queue=*/nil);
}

void DownloadSessionTaskImpl::GetCookies(
    base::OnceCallback<void(NSArray<NSHTTPCookie*>*)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  scoped_refptr<net::URLRequestContextGetter> context_getter(
      web_state_->GetBrowserState()->GetRequestContext());

  // net::URLRequestContextGetter must be used in the IO thread.
  GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&GetCookiesFromContextGetter, context_getter,
                     base::BindPostTask(base::SequencedTaskRunnerHandle::Get(),
                                        std::move(callback))));
}

void DownloadSessionTaskImpl::StartWithCookies(
    NSArray<NSHTTPCookie*>* cookies) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
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
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  mime_type_.clear();
  std::string charset;
  std::string data;
  if (!net::DataURL::Parse(original_url_, &mime_type_, &charset, &data)) {
    OnWriterDownloadFinished(net::ERR_INVALID_URL);
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
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  percent_complete_ = 100;
  total_bytes_ = bytes_written;
  received_bytes_ = total_bytes_;
  auto callback =
      base::BindOnce(&DownloadSessionTaskImpl::OnWriterDownloadFinished,
                     weak_factory_.GetWeakPtr());
  if (writer_->Finish(net::OK, std::move(callback)) != net::ERR_IO_PENDING) {
    OnWriterDownloadFinished(net::OK);
  }
}

void DownloadSessionTaskImpl::OnTaskDone(NSURLSessionTask* task,
                                         NSError* error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (task != session_task_) {
    [task cancel];
    return;
  }

  download_result_ = DownloadResult(
      GetNetErrorCodeFromNSError(error, task.currentRequest.URL));
  OnTaskTick(task, /*notify_download_updated*/ false);

  // Forget the current NSURLSessionTask, so that if any other callbacks are
  // received they are dropped as calling net::URLFetcherFileWriter::Finish
  // put the writer in a invalid state.
  session_task_ = nil;

  const int result = writer_->Finish(
      download_result_.error_code(),
      base::BindOnce(&DownloadSessionTaskImpl::OnWriterDownloadFinished,
                     weak_factory_.GetWeakPtr()));

  if (result != net::ERR_IO_PENDING) {
    OnWriterDownloadFinished(download_result_.error_code());
  }
}

void DownloadSessionTaskImpl::OnTaskData(NSURLSessionTask* task,
                                         NSData* data,
                                         ProceduralBlock completion_handler) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (task != session_task_) {
    completion_handler();
    [task cancel];
    return;
  }

  scoped_refptr<net::IOBufferWithSize> buffer =
      GetBuffer(data.bytes, data.length);

  base::RepeatingClosure inner_closure = base::BindRepeating(
      &DownloadSessionTaskImpl::OnTaskTick, weak_factory_.GetWeakPtr(), task,
      /*notify_download_updated*/ true);

  // The buffer needs to be kept alive until net::URLFetcherFileWriter::Write
  // has completed the write. So capture a reference to the buffer in the bound
  // callback.
  base::RepeatingCallback<void(int)> write_callback = base::BindRepeating(
      [](base::RepeatingClosure inner_closure, ProceduralBlock completion_block,
         scoped_refptr<net::IOBuffer> buffer, int result) {
        if (!inner_closure.IsCancelled()) {
          inner_closure.Run();
        }
        completion_block();
      },
      inner_closure, completion_handler, buffer);

  const int result =
      writer_->Write(buffer.get(), buffer->size(), write_callback);

  if (result != net::ERR_IO_PENDING) {
    write_callback.Run(result);
  }
}

void DownloadSessionTaskImpl::OnTaskTick(NSURLSessionTask* task,
                                         bool notify_download_updated) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (task != session_task_) {
    [task cancel];
    return;
  }

  percent_complete_ = GetTaskPercentComplete(task);
  received_bytes_ = task.countOfBytesReceived;
  if (total_bytes_ == -1 || task.countOfBytesExpectedToReceive) {
    // countOfBytesExpectedToReceive can be 0 if the device if offline.
    // In that case total_bytes_ should remain unchanged if the total
    // bytes count is already known.
    total_bytes_ = task.countOfBytesExpectedToReceive;
  }

  if (task.response.MIMEType) {
    mime_type_ = base::SysNSStringToUTF8(task.response.MIMEType);
  }

  if ([task.response isKindOfClass:[NSHTTPURLResponse class]]) {
    http_code_ =
        base::mac::ObjCCastStrict<NSHTTPURLResponse>(task.response).statusCode;
  }

  if (notify_download_updated) {
    OnDownloadUpdated();
  }
}

}  // namespace web
