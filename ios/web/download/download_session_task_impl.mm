// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/download/download_session_task_impl.h"

#import "base/check.h"
#import "base/mac/foundation_util.h"
#import "base/sequence_checker.h"
#import "base/strings/sys_string_conversions.h"
#import "base/task/bind_post_task.h"
#import "base/task/sequenced_task_runner.h"
#import "ios/net/cookies/system_cookie_util.h"
#import "ios/web/common/user_agent.h"
#import "ios/web/download/download_result.h"
#import "ios/web/download/download_session_cookie_storage.h"
#import "ios/web/public/browser_state.h"
#import "ios/web/public/thread/web_task_traits.h"
#import "ios/web/public/thread/web_thread.h"
#import "ios/web/public/web_client.h"
#import "ios/web/public/web_state.h"
#import "ios/web/web_view/error_translation_util.h"
#import "net/base/mac/url_conversions.h"
#import "net/cookies/cookie_store.h"
#import "net/url_request/url_request_context.h"
#import "net/url_request/url_request_context_getter.h"

namespace web {
namespace download {
namespace internal {

// Helper structure used to pass information about the NSURLSessionTask*
// to the DownloadSessionTaskImpl from the background sequence.
class TaskInfo {
 public:
  TaskInfo(int64_t total_bytes, int http_error_code, NSString* mime_type)
      : total_bytes_(total_bytes),
        http_error_code_(http_error_code),
        mime_type_(mime_type) {
    DCHECK(total_bytes_ == -1 || total_bytes >= 0);
    DCHECK(http_error_code_ == -1 || http_error_code_ > 0);
    DCHECK(!mime_type || mime_type.length != 0);
  }

  TaskInfo(TaskInfo&& other) = default;
  TaskInfo& operator=(TaskInfo&& other) = default;

  ~TaskInfo() = default;

  // Constructs a TaskInfo from `task`.
  static TaskInfo FromTask(NSURLSessionTask* task) {
    int http_code = -1;
    if ([task.response isKindOfClass:[NSHTTPURLResponse class]]) {
      http_code = base::mac::ObjCCastStrict<NSHTTPURLResponse>(task.response)
                      .statusCode;
    }

    return TaskInfo(
        task.countOfBytesExpectedToReceive, http_code,
        task.response.MIMEType.length != 0 ? task.response.MIMEType : nil);
  }

  int64_t total_bytes() const { return total_bytes_; }
  int http_error_code() const { return http_error_code_; }
  NSString* mime_type() const { return mime_type_; }

 private:
  int64_t total_bytes_ = -1;
  int http_error_code_ = -1;
  NSString* mime_type_ = nil;
};

}  // namespace internal
}  // namespace download
}  // namespace web

namespace {

// Invoked when data is received from the NSURLSessionTask*.
using DataReceivedHandler =
    base::RepeatingCallback<void(NSData* data,
                                 web::download::internal::TaskInfo task_info)>;

// Invoked when the NSURLSessionTask* terminates. If the task was successful,
// then `error` will be `net::OK` otherwise will reflect the download error.
using TaskFinishedHandler =
    base::RepeatingCallback<void(int error_code,
                                 web::download::internal::TaskInfo task_info)>;

}  // anonymous namespace

// NSURLSessionDataDelegate that forwards data and properties task updates to
// the client. Client of this delegate can pass blocks to receive the updates.
@interface CRWURLSessionDelegate : NSObject <NSURLSessionDataDelegate>

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithDataReceivedHandler:(DataReceivedHandler)dataReceived
                        taskFinishedHandler:(TaskFinishedHandler)taskFinished
    NS_DESIGNATED_INITIALIZER;

@end

@implementation CRWURLSessionDelegate {
  DataReceivedHandler _dataReceived;
  TaskFinishedHandler _taskFinished;
}

- (instancetype)initWithDataReceivedHandler:(DataReceivedHandler)dataReceived
                        taskFinishedHandler:(TaskFinishedHandler)taskFinished {
  if ((self = [super init])) {
    _dataReceived = dataReceived;
    _taskFinished = taskFinished;

    DCHECK(!_dataReceived.is_null());
    DCHECK(!_taskFinished.is_null());
  }
  return self;
}

- (void)URLSession:(NSURLSession*)session
                    task:(NSURLSessionTask*)task
    didCompleteWithError:(NSError*)error {
  @synchronized(self) {
    if (_taskFinished.is_null())
      return;

    int error_code = net::OK;
    if (error) {
      NSURL* url = task.response.URL;
      if (!web::GetNetErrorFromIOSErrorCode(error.code, &error_code, url)) {
        error_code = net::ERR_FAILED;
      }
    }

    using web::download::internal::TaskInfo;
    _taskFinished.Run(error_code, TaskInfo::FromTask(task));
  }
}

- (void)URLSession:(NSURLSession*)session
          dataTask:(NSURLSessionDataTask*)task
    didReceiveData:(NSData*)data {
  @synchronized(self) {
    if (_dataReceived.is_null())
      return;

    using web::download::internal::TaskInfo;
    _dataReceived.Run(data, TaskInfo::FromTask(task));
  }
}

- (void)URLSession:(NSURLSession*)session
    didReceiveChallenge:(NSURLAuthenticationChallenge*)challenge
      completionHandler:(void (^)(NSURLSessionAuthChallengeDisposition,
                                  NSURLCredential*))handler {
  @synchronized(self) {
    // TODO(crbug.com/780911): use CRWCertVerificationController to get
    // CertAcceptPolicy for this `challenge`.
    handler(NSURLSessionAuthChallengeRejectProtectionSpace, nil);
  }
}

- (void)stop {
  @synchronized(self) {
    _taskFinished.Reset();
    _dataReceived.Reset();
  }
}

@end

namespace web {
namespace download {
namespace internal {
namespace {

// Asynchronously returns cookies for `context_getter`. Must be called on IO
// thread (due to URLRequestContextGetter thread-affinity). The callback will
// be called on the IO thread too.
void GetCookiesFromContextGetter(
    scoped_refptr<net::URLRequestContextGetter> context_getter,
    base::OnceCallback<void(NSArray<NSHTTPCookie*>*)> callback) {
  context_getter->GetURLRequestContext()->cookie_store()->GetAllCookiesAsync(
      base::BindOnce(&net::SystemCookiesFromCanonicalCookieList)
          .Then(std::move(callback)));
}

// Creates a new file at `path` open for writing. If a file already exists,
// it will be overwritten. The newly created file object is returned. This
// function helps create the file on a background sequence.
base::File CreateFile(base::FilePath path) {
  DCHECK(!path.empty());
  return base::File(path,
                    base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);
}

// Closes `file`. This function helps closing the file on the background
// sequence.
void CloseFile(base::File file) {
  file.Close();
}

// This structure is used to pass the result of WriteDataHelper function back
// to the caller. In case of error, the file will be closed and `bytes_written`
// will be -1 (the error code can be found via `base::File::error_details()`).
struct WriteDataResult {
  base::File file;
  int64_t bytes_written;
};

WriteDataResult WriteDataHelper(base::File file, NSArray<NSData*>* array) {
  int64_t bytes_written = 0;
  for (NSData* data in array) {
    base::span<const uint8_t> span =
        base::make_span(static_cast<const uint8_t*>(data.bytes), data.length);

    // base::File does not set `error_details()` when write fails, but it
    // guarantee that `GetLastFileError()` will return the correct value.
    // In case of failure create a `base::File` to transmit the error code.
    if (!file.WriteAtCurrentPosAndCheck(span)) {
      base::File error_file(base::File::GetLastFileError());
      return WriteDataResult{std::move(error_file), -1};
    }

    bytes_written += data.length;
  }

  return WriteDataResult{std::move(file), bytes_written};
}

// Move the `base::File` out of `optional` and reset the `optional` to have
// no value (i.e. to be equal to `absl::nullopt`).
base::File take(absl::optional<base::File>& optional) {
  DCHECK(optional.has_value());
  base::File value = std::move(optional.value());
  optional = absl::nullopt;
  return value;
}

}  // anonymous namespace

// Helper class that manages a NSURLSession* and a NSURLSessionTask* to
// perform the download on a background sequence. The object itself has
// sequence-affinity.
//
// This is a separate object as it allow to simplify the logic of the
// DownloadSessionTaskImpl by moving the interactions with NSURLSession,
// NSURLSessionTask and writing to a file to a single object.
//
// DownloadSessionTaskImpl can simply destroy this object to cancel a
// download in progress, knowing that the session will be cancelled and
// the downloaded data deleted if not complete.
//
// The NSURLSession perform the download on a background queue, and the
// data is written to the disk on a background sequence. This means that
// the code in this class needs to interact with three sequences:
//
//   1. NSURLSession queue:
//
//      Receive data from the network and invoke it's delegate method
//      -URLSession:dataTask:didReceiveData: (when receiving data) or
//      -URLSession:task:didCompleteWithError: (when task complete).
//
//      The delegate invokes the correct callback. Those callbacks are
//      created with base::BindPostTask(...) and thus will be invoked
//      on the Session's sequence. The DataReceived or TaskTerminated
//      method will be called depending on the delegate method called.
//
//  2. Session sequence:
//
//
//      This is the same sequence as the DownloadSessionTaskImpl* that
//      owns the Session instance, so it is always safe to call method
//      on `owner_`.
//
//      - If TaskTerminated is invoked:
//
//        The error_code from the download will be propagated to the
//        owning DownloadSessionTaskImpl. The Session instance will
//        have ensured it has been deleted by this point, and thus
//        that the NSURLSession cancelled.
//
//        If there is a write in progress (i.e. `file_` is empty) or
//        pending (i.e. `pending_` is not nil), then if the download
//        was a success, the Session will wait until all the data has
//        been written. In case of error, there is no need to wait
//        for the write to complete and DownloadSessionTaskImpl can
//        be immediately notified of the failure.
//
//      - If DataReceived is invoked:
//
//        Depending on whether a write is in progress, the `file_` will
//        either be non-empty (no write in progress) or empty. If empty,
//        the data will be queued in `pending_`, otherwise a new write
//        will be initiated on the background queue.
//
//      - If DataWritten is invoked:
//
//        This means that a write has completed either successfully or
//        in error. In case of failure, the DownloadSessiontTaskImpl is
//        notified of the failure. In case of success, if there is any
//        pending data to write, a new write will be scheduled. If not,
//        the `base::File` will be stored back to `file_`.
//
//  3. Background sequence:
//
//      The write requests are performed on that sequence. They are
//      scheduled by extracting the `base::File` from `file_`, and
//      posting a task running `WriteDataHelper` on the background
//      sequence (via `task_runner_`).
//
//      As long as a write is in progress, `file_` of the instance
//      will be empty (which means that any data received from the
//      NSURLSession will be enqueued to `pending_`.
//
//      The task is configured to invoke DataWritten when the write
//      is complete, passing back the `base::File` object and the
//      amount of data written (the `file` may have been closed and
//      marked in error if the write failed, e.g. due to lack of
//      disk space).
//
class Session {
 public:
  Session(base::File file,
          const GURL& url,
          NSString* identifier,
          NSString* http_method,
          NSArray<NSHTTPCookie*>* cookies,
          DownloadSessionTaskImpl::SessionFactory session_factory,
          const scoped_refptr<base::SequencedTaskRunner>& task_runner,
          DownloadSessionTaskImpl* owner);

  Session(const Session&) = delete;
  Session& operator=(const Session&) = delete;

  ~Session();

 private:
  // Invoked when data is received from the NSURLSessionTask.
  void DataReceived(NSData* data, TaskInfo task_info);

  // Invoked when the NSURLSessionTask is complete with `error_code`.
  void TaskFinished(int error_code, TaskInfo task_info);

  // Invoked when data has been written to disk. The `result` object
  // will contains the base::File object (possibly in error).
  void DataWritten(WriteDataResult result);

  // Helper method to write multiple NSData* objects to `file`.
  void WriteData(base::File file, NSArray<NSData*>* array);

  // Cancels the NSURLSession and cleanup related objects.
  void CancelSession();

  SEQUENCE_CHECKER(sequence_checker_);

  // Used to manage the write requests. Depending on whether `file_` is
  // empty or not, the data is enqueued in `pending_` or a new task is
  // posted to the background sequence using `task_runner_`.
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  absl::optional<base::File> file_;
  NSMutableArray<NSData*>* pending_ = nil;

  // Stores the error code received from `TaskFinished`.
  absl::optional<int> error_code_;

  // References to the NSURLSession and NSURLSessionTask used to perform
  // the download in the background.
  __strong NSURLSession* session_ = nil;
  __strong NSURLSessionTask* task_ = nil;
  __strong CRWURLSessionDelegate* delegate_ = nil;

  // Pointer to the DownloadSessionTaskImpl that owns the Session instance.
  // Using a raw pointer is safe as the Session object will never outlive
  // the DownloadSessionTaskImpl instance.
  DownloadSessionTaskImpl* owner_ = nullptr;

  // The delegate methods are invoked on a background queue managed by
  // the iOS runtime. The callbacks passed to the delegate use a weak
  // pointer to ensure safety.
  base::WeakPtrFactory<Session> weak_factory_{this};
};

Session::Session(base::File file,
                 const GURL& url,
                 NSString* identifier,
                 NSString* http_method,
                 NSArray<NSHTTPCookie*>* cookies,
                 DownloadSessionTaskImpl::SessionFactory session_factory,
                 const scoped_refptr<base::SequencedTaskRunner>& task_runner,
                 DownloadSessionTaskImpl* owner)
    : task_runner_(task_runner), file_(std::move(file)), owner_(owner) {
  DCHECK(owner_);
  DCHECK(file_.has_value() && file_.value().IsValid());

  NSURLSessionConfiguration* configuration = [NSURLSessionConfiguration
      backgroundSessionConfigurationWithIdentifier:identifier];

  const NSHTTPCookieAcceptPolicy policy =
      NSHTTPCookieStorage.sharedHTTPCookieStorage.cookieAcceptPolicy;

  // Cookies have to be set in the session configuration before the session
  // is created (as once the session is created, the configuration object
  // can't be edited and configuration properties will return a copy of the
  // originally used configuration). The cookies are copied from the internal
  // WebSiteDataStore cookie store, so they should not have duplicates nor
  // invalid cookies.
  configuration.HTTPCookieStorage =
      [[DownloadSessionCookieStorage alloc] initWithCookies:cookies
                                         cookieAcceptPolicy:policy];

  const std::string user_agent =
      GetWebClient()->GetUserAgent(UserAgentType::MOBILE);
  configuration.HTTPAdditionalHeaders = @{
    base::SysUTF8ToNSString(net::HttpRequestHeaders::kUserAgent) :
        base::SysUTF8ToNSString(user_agent),
  };

  // Invoked when data is received from NSURLSessionTask.
  DataReceivedHandler data_received = base::BindPostTaskToCurrentDefault(
      base::BindRepeating(&Session::DataReceived, weak_factory_.GetWeakPtr()));

  // Invoked when NSURLSessionTask complete.
  TaskFinishedHandler task_finished = base::BindPostTaskToCurrentDefault(
      base::BindRepeating(&Session::TaskFinished, weak_factory_.GetWeakPtr()));

  // The delegate passed to NSURLSession. It is strongly retained by the
  // NSURLSession, so there is no need to retain it by this Session object.
  // The delegate will be invoked for all events for any NSURLSessionTask
  // created with the NSURLSession and is the reason why NSURLSession are
  // not re-used (as otherwise it is possible for the delegate to be called
  // for download that DownloadSessionTaskImpl consider as cancelled).
  delegate_ = [[CRWURLSessionDelegate alloc]
      initWithDataReceivedHandler:std::move(data_received)
              taskFinishedHandler:std::move(task_finished)];

  if (!session_factory.is_null()) {
    session_ = session_factory.Run(configuration, delegate_);
    DCHECK(session_) << "session_factory must not return nil!";
  } else {
    session_ = [NSURLSession sessionWithConfiguration:configuration
                                             delegate:delegate_
                                        delegateQueue:nil];
  }

  NSMutableURLRequest* request =
      [[NSMutableURLRequest alloc] initWithURL:net::NSURLWithGURL(url)];
  request.HTTPMethod = http_method;

  task_ = [session_ dataTaskWithRequest:request];
  [task_ resume];
}

Session::~Session() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CancelSession();

  // Close the file on the background sequence if it is still open. This
  // is a best effort.
  if (file_.has_value()) {
    base::File file = take(file_);
    task_runner_->PostTask(FROM_HERE,
                           base::BindOnce(&CloseFile, std::move(file)));
  }
}

void Session::DataReceived(NSData* data, TaskInfo task_info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(data);

  owner_->ApplyTaskInfo(std::move(task_info));

  if (file_.has_value()) {
    WriteData(take(file_), @[ data ]);
    return;
  }

  if (!pending_) {
    pending_ = [[NSMutableArray alloc] init];
  }
  [pending_ addObject:data];
}

void Session::TaskFinished(int error_code, TaskInfo task_info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  owner_->ApplyTaskInfo(std::move(task_info));
  CancelSession();

  if (pending_ || !file_.has_value()) {
    DCHECK(!error_code_.has_value());
    error_code_ = error_code;
  } else {
    DCHECK(file_.has_value());
    base::File file = take(file_);
    task_runner_->PostTaskAndReply(
        FROM_HERE, base::BindOnce(&CloseFile, std::move(file)),
        base::BindOnce(&DownloadSessionTaskImpl::OnDownloadFinished,
                       owner_->weak_factory_.GetWeakPtr(),
                       DownloadResult(error_code)));
  }
}

void Session::DataWritten(WriteDataResult result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!file_.has_value());

  if (!result.file.IsValid()) {
    CancelSession();

    int error_code = net::FileErrorToNetError(result.file.error_details());
    owner_->OnDownloadFinished(DownloadResult(error_code));
    return;
  }

  if (pending_) {
    DCHECK_GT(pending_.count, 0u);
    NSMutableArray<NSData*>* array = nil;
    std::swap(array, pending_);

    WriteData(std::move(result.file), array);
  } else {
    file_ = std::move(result.file);
  }

  // No write pending and download complete, close the file, then notify the
  // DownloadSessionTaskImpl about the download completion. Do this before
  // calling `OnDownloadUpdated()` since the DownloadTaskImpl may be deleted
  // synchronously by one of the observer.
  if (file_.has_value() && error_code_.has_value()) {
    base::File file = take(file_);
    task_runner_->PostTaskAndReply(
        FROM_HERE, base::BindOnce(&CloseFile, std::move(file)),
        base::BindOnce(&DownloadSessionTaskImpl::OnDownloadFinished,
                       owner_->weak_factory_.GetWeakPtr(),
                       DownloadResult(error_code_.value())));
  }

  owner_->OnDataWritten(result.bytes_written);
  owner_->OnDownloadUpdated();
}

void Session::WriteData(base::File file, NSArray<NSData*>* array) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(file.IsValid());

  task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(&WriteDataHelper, std::move(file), array),
      base::BindOnce(&Session::DataWritten, weak_factory_.GetWeakPtr()));
}

void Session::CancelSession() {
  // Stop the delegate so that it stop forwarding events to this instance.
  // Do this before cancelling the NSURLSession as otherwise it may lead
  // to invoking the TaskFinished() method via the callback.

  [delegate_ stop];
  delegate_ = nil;

  [session_ invalidateAndCancel];
  session_ = nil;

  [task_ cancel];
  task_ = nil;
}

}  // namespace internal
}  // namespace download

DownloadSessionTaskImpl::DownloadSessionTaskImpl(
    WebState* web_state,
    const GURL& original_url,
    NSString* http_method,
    const std::string& content_disposition,
    int64_t total_bytes,
    const std::string& mime_type,
    NSString* identifier,
    const scoped_refptr<base::SequencedTaskRunner>& task_runner,
    DownloadSessionTaskImpl::SessionFactory session_factory)
    : DownloadTaskImpl(web_state,
                       original_url,
                       http_method,
                       content_disposition,
                       total_bytes,
                       mime_type,
                       identifier,
                       task_runner),
      session_factory_(std::move(session_factory)) {
  DCHECK(!original_url_.SchemeIs(url::kDataScheme));
}

DownloadSessionTaskImpl::~DownloadSessionTaskImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CancelInternal();
}

void DownloadSessionTaskImpl::StartInternal(const base::FilePath& path) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!path.empty());

  // Ensure that any previous session has been invalidated.
  CancelInternal();

  using download::internal::CreateFile;
  task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(&CreateFile, path),
      base::BindOnce(&DownloadSessionTaskImpl::OnFileCreated,
                     weak_factory_.GetWeakPtr()));
}

void DownloadSessionTaskImpl::CancelInternal() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  weak_factory_.InvalidateWeakPtrs();
  session_.reset();
}

void DownloadSessionTaskImpl::OnFileCreated(base::File file) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!file.IsValid()) {
    // Calling `OnDownloadFinished()` may cause the task to be deleted,
    // so this must no longer be accessed after that point.
    OnDownloadFinished(
        DownloadResult(net::FileErrorToNetError(file.error_details())));

    return;
  }

  scoped_refptr<net::URLRequestContextGetter> context_getter =
      web_state_->GetBrowserState()->GetRequestContext();

  // net::URLRequestContextGetter must be used on the IO thread.
  using download::internal::GetCookiesFromContextGetter;
  GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&GetCookiesFromContextGetter, context_getter,
                     base::BindPostTaskToCurrentDefault(base::BindOnce(
                         &DownloadSessionTaskImpl::OnCookiesFetched,
                         weak_factory_.GetWeakPtr(), std::move(file)))));
}

void DownloadSessionTaskImpl::OnCookiesFetched(
    base::File file,
    NSArray<NSHTTPCookie*>* cookies) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(file.IsValid());

  // Creating the Session object automatically starts the download.
  using download::internal::Session;
  session_ = std::make_unique<Session>(
      std::move(file), GetOriginalUrl(), GetIdentifier(), GetHttpMethod(),
      cookies, session_factory_, task_runner_, this);

  OnDownloadUpdated();
}

void DownloadSessionTaskImpl::ApplyTaskInfo(
    download::internal::TaskInfo task_info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (total_bytes_ == -1 || task_info.total_bytes() > 0) {
    // NSURLSessionTask -countOfBytesExpectedToReceive can be 0 if the device
    // is offline. In that case, total_bytes_ should remain unchanged if the
    // total bytes count is already known.
    total_bytes_ = task_info.total_bytes();
  }

  if (task_info.http_error_code() != -1) {
    http_code_ = task_info.http_error_code();
  }

  if (task_info.mime_type().length != 0) {
    mime_type_ = base::SysNSStringToUTF8(task_info.mime_type());
  }

  RecomputePercentCompleted();
}

void DownloadSessionTaskImpl::OnDataWritten(int64_t data_size) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  received_bytes_ += data_size;
  RecomputePercentCompleted();
}

void DownloadSessionTaskImpl::RecomputePercentCompleted() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  switch (total_bytes_) {
    case 0:
      percent_complete_ = 100;
      break;

    case -1:
      percent_complete_ = -1;
      break;

    default:
      DCHECK_GE(received_bytes_, 0);
      DCHECK_LE(received_bytes_, total_bytes_);
      percent_complete_ = (100. * received_bytes_) / total_bytes_;
      break;
  }
}

}  // namespace web
