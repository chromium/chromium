// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/net/cookies/wk_http_system_cookie_store.h"

#import <objc/runtime.h>

#import "base/functional/bind.h"
#import "base/functional/callback_helpers.h"
#import "base/ios/block_types.h"
#import "base/memory/weak_ptr.h"
#import "base/scoped_observation.h"
#import "base/sequence_checker.h"
#import "base/task/bind_post_task.h"
#import "base/task/sequenced_task_runner.h"
#import "ios/net/cookies/cookie_creation_time_manager.h"
#import "ios/net/cookies/system_cookie_util.h"
#import "ios/web/net/cookies/crw_wk_http_cookie_store.h"
#import "ios/web/public/thread/web_task_traits.h"
#import "ios/web/public/thread/web_thread.h"
#import "ios/web/web_state/ui/wk_web_view_configuration_provider.h"
#import "net/base/apple/url_conversions.h"
#import "net/cookies/canonical_cookie.h"
#import "net/cookies/cookie_constants.h"
#import "url/gurl.h"

namespace {

// Some aliases for callbacks, blocks and SequencedTaskRunner to make the
// Objective-C wrapper using those types easier to read.

using NoParamCallback = base::OnceCallback<void(void)>;
using CookiesCallback = base::OnceCallback<void(NSArray<NSHTTPCookie*>*)>;

using NoParamBlock = void (^)(void);
using CookiesBlock = void (^)(NSArray<NSHTTPCookie*>*);

using ScopedSequencedTaskRunnerPtr = scoped_refptr<base::SequencedTaskRunner>;

// Key used to attach the associated object.
const char kWKHTTPSystemCookieCallbackConfigCreatedRegistrationKey = '\0';

}  // namespace

// Holds a base::CallbackListSubscription and destroy it when deallocated.
//
// This allow attaching a base::CallbackListSubscription as an associated
// object to CRWWKHTTPCookieStore. This ensures the subscription lives as
// long as the object it forwards the notification to and it is destroyed
// on the correct sequence.
@interface WKHTTPSystemCookieCallbackConfigCreatedRegistration : NSObject

+ (void)registerCookieStore:(CRWWKHTTPCookieStore*)store
               withProvider:(web::WKWebViewConfigurationProvider*)provider;

@end

@implementation WKHTTPSystemCookieCallbackConfigCreatedRegistration {
  base::CallbackListSubscription _subscription;
  SEQUENCE_CHECKER(_sequenceChecker);
}

- (instancetype)initWithSubscription:
    (base::CallbackListSubscription)subscription {
  if ((self = [super init])) {
    _subscription = std::move(subscription);
  }
  return self;
}

- (void)dealloc {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
}

+ (void)registerCookieStore:(CRWWKHTTPCookieStore*)store
               withProvider:(web::WKWebViewConfigurationProvider*)provider {
  __weak CRWWKHTTPCookieStore* weak_store = store;
  base::CallbackListSubscription subscription =
      provider->RegisterConfigurationCreatedCallback(
          base::BindRepeating(^(WKWebViewConfiguration* configuration) {
            weak_store.websiteDataStore = configuration.websiteDataStore;
          }));

  WKHTTPSystemCookieCallbackConfigCreatedRegistration* wrapper =
      [[WKHTTPSystemCookieCallbackConfigCreatedRegistration alloc]
          initWithSubscription:std::move(subscription)];

  objc_setAssociatedObject(
      store, &kWKHTTPSystemCookieCallbackConfigCreatedRegistrationKey, wrapper,
      OBJC_ASSOCIATION_RETAIN_NONATOMIC);
}

@end

// Represents a pending operation that can be cancelled.
//
// The cancellation or the invocation must happen on the same sequence,
// but it is safe to keep reference to those objects and pass them from
// sequence to sequence until invoked.
@protocol WKHTTPSystemCookieStoreCancelableTask

- (void)cancel;

@end

// Helper class that converts callbacks to blocks that weakly retain the
// callback (i.e. the callback can be destroyed before running the block
// which will then be a no-op).
//
// The WKHTTPSystemCookieStore lives on the IO sequence and delegate its
// job to CRWWKHTTPCookieStore which lives on the UI sequence. There is
// needs to be able to send blocks to CRWWKHTTPCookieStore but they must
// be cancelled if not completed when the IO sequence is destroyed.
//
// The implementation uses a WKHTTPSystemCookieStoreCancelableTask to
// store the callback and a block with a weak reference to the task is
// returned. This instance keeps a list of all pending tasks, remove them
// when invoked, which allows to cancel them by resetting their callback.
//
// Additionally, the code ensure that the cancellation and the invocation
// runs on the same sequence since the implementation is sequence-affine.
@interface WKHTTPSystemCookieStoreCancelableTaskHelper
    : NSObject <WKHTTPSystemCookieStoreCancelableTask>

- (instancetype)initWithTaskRunner:(ScopedSequencedTaskRunnerPtr)taskRunner
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

- (NoParamBlock)wrapNoParamCallback:(NoParamCallback)callback;
- (CookiesBlock)wrapCookiesCallback:(CookiesCallback)callback;

@end

// Used to wrap a base::OnceCallback<void()> allowing it to be cancelled.
@interface WKHTTPSystemCookieStoreCancelableTaskNoParam
    : NSObject <WKHTTPSystemCookieStoreCancelableTask>

- (instancetype)initWithCallback:(NoParamCallback)completion
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

- (void)invoke;

@end

// Used to wrap a base::OnceCallback<void(NSArray<NSHTTPCookie*>*)> allowing
// it to be cancelled.
@interface WKHTTPSystemCookieStoreCancelableTaskCookies
    : NSObject <WKHTTPSystemCookieStoreCancelableTask>

- (instancetype)initWithCallback:(CookiesCallback)completion
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

- (void)invoke:(NSArray<NSHTTPCookie*>*)cookies;

@end

@implementation WKHTTPSystemCookieStoreCancelableTaskHelper {
  scoped_refptr<base::SequencedTaskRunner> _taskRunner;
  NSMutableArray<NSObject<WKHTTPSystemCookieStoreCancelableTask>*>* _tasks;
  SEQUENCE_CHECKER(_sequenceChecker);
}

- (instancetype)initWithTaskRunner:(ScopedSequencedTaskRunnerPtr)taskRunner {
  if ((self = [super init])) {
    _taskRunner = taskRunner;
    _tasks = [[NSMutableArray alloc] init];
    DETACH_FROM_SEQUENCE(_sequenceChecker);
  }
  return self;
}

- (void)dealloc {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  [self cancel];
}

- (void)cancel {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  for (NSObject<WKHTTPSystemCookieStoreCancelableTask>* task in _tasks) {
    [task cancel];
  }
  _tasks = nil;
}

- (void)insertTask:(NSObject<WKHTTPSystemCookieStoreCancelableTask>*)task {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  [_tasks addObject:task];
}

- (void)removeTask:(NSObject<WKHTTPSystemCookieStoreCancelableTask>*)task {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  [_tasks removeObject:task];
}

- (NoParamBlock)wrapNoParamCallback:(NoParamCallback)callback {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  WKHTTPSystemCookieStoreCancelableTaskNoParam* task =
      [[WKHTTPSystemCookieStoreCancelableTaskNoParam alloc]
          initWithCallback:std::move(callback)];
  [self insertTask:task];

  __weak __typeof(task) weakTask = task;
  __weak __typeof(self) weakSelf = self;
  NoParamBlock block = ^{
    [weakTask invoke];
    [weakSelf removeTask:weakTask];
  };

  return base::CallbackToBlock(
      base::BindPostTask(_taskRunner, base::BindOnce(block)));
}

- (CookiesBlock)wrapCookiesCallback:(CookiesCallback)callback {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  WKHTTPSystemCookieStoreCancelableTaskCookies* task =
      [[WKHTTPSystemCookieStoreCancelableTaskCookies alloc]
          initWithCallback:std::move(callback)];
  [self insertTask:task];

  __weak __typeof(task) weakTask = task;
  __weak __typeof(self) weakSelf = self;
  CookiesBlock block = ^(NSArray<NSHTTPCookie*>* cookies) {
    [weakTask invoke:cookies];
    [weakSelf removeTask:weakTask];
  };

  return base::CallbackToBlock(
      base::BindPostTask(_taskRunner, base::BindOnce(block)));
}

@end

@implementation WKHTTPSystemCookieStoreCancelableTaskNoParam {
  NoParamCallback _callback;
  SEQUENCE_CHECKER(_sequenceChecker);
}

- (instancetype)initWithCallback:(NoParamCallback)callback {
  if ((self = [super init])) {
    _callback = std::move(callback);
  }
  return self;
}

- (void)invoke {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  if (!_callback.is_null()) {
    std::move(_callback).Run();
  }
}

- (void)cancel {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  _callback = NoParamCallback{};
}

@end

@implementation WKHTTPSystemCookieStoreCancelableTaskCookies {
  CookiesCallback _callback;
  SEQUENCE_CHECKER(_sequenceChecker);
}

- (instancetype)initWithCallback:(CookiesCallback)callback {
  if ((self = [super init])) {
    _callback = std::move(callback);
  }
  return self;
}

- (void)invoke:(NSArray<NSHTTPCookie*>*)cookies {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  if (!_callback.is_null()) {
    std::move(_callback).Run(cookies);
  }
}

- (void)cancel {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  _callback = CookiesCallback{};
}

@end

namespace web {
namespace {

// Returns wether `cookie` should be included for queries about `url`.
// To include `cookie` for `url`, all these conditions need to be met:
//   1- If the cookie is secure the URL needs to be secure.
//   2- `url` domain need to match the cookie domain.
//   3- `cookie` url path need to be on the path of the given `url`.
bool ShouldIncludeForRequestUrl(NSHTTPCookie* cookie, const GURL& url) {
  // CanonicalCookies already implements cookie selection for URLs, so instead
  // of rewriting the checks here, the function converts the NSHTTPCookie to
  // canonical cookie and provide it with dummy CookieOption, so when iOS starts
  // to support cookieOptions this function can be modified to support that.
  std::unique_ptr<net::CanonicalCookie> canonical_cookie =
      net::CanonicalCookieFromSystemCookie(cookie, base::Time());
  if (!canonical_cookie)
    return false;
  // Cookies handled by this method are app specific cookies, so it's safe to
  // use strict same site context.
  net::CookieOptions options = net::CookieOptions::MakeAllInclusive();
  net::CookieAccessSemantics cookie_access_semantics =
      net::CookieAccessSemantics::LEGACY;

  // Using `UNKNOWN` semantics to allow the experiment to switch between non
  // legacy (where cookies that don't have a specific same-site access policy
  // and not secure will not be included), and legacy mode.
  cookie_access_semantics = net::CookieAccessSemantics::UNKNOWN;

  // No extra trustworthy URLs.
  bool delegate_treats_url_as_trustworthy = false;
  net::CookieAccessParams params = {cookie_access_semantics,
                                    delegate_treats_url_as_trustworthy};
  return canonical_cookie->IncludeForRequestURL(url, options, params)
      .status.IsInclude();
}

// Helper method that insert a cookie in `weak_creation_time_manager`
// while ensuring the time is unique.
void SetCreationTimeEnsureUnique(
    base::WeakPtr<net::CookieCreationTimeManager> weak_creation_time_manager,
    NSHTTPCookie* cookie,
    base::Time creation_time) {
  if (net::CookieCreationTimeManager* creation_time_manager =
          weak_creation_time_manager.get()) {
    creation_time_manager->SetCreationTime(
        cookie, creation_time_manager->MakeUniqueCreationTime(creation_time));
  }
}

// Returns a closure that invokes `one` and then `two` unconditionally. If `two`
// is null, then returns `one`.
base::OnceClosure ChainClosure(base::OnceClosure one, base::OnceClosure two) {
  DCHECK(!one.is_null());
  if (two.is_null()) {
    return one;
  }

  return base::BindOnce(
      [](base::OnceClosure one, base::OnceClosure two) {
        std::move(one).Run();
        std::move(two).Run();
      },
      std::move(one), std::move(two));
}

}  // namespace

#pragma mark - WKHTTPSystemCookieStore::Helper

// Class wrapping a WKHTTPCookieStore and providing C++ based API to
// sends requests while dealing with the fact that WKHTTPCookieStore
// is only accessible on the UI thread while WKHTTPSystemCookieStore
// lives on the IO thread.
//
// This object uses scoped_refptr<base::SequencedTaskRunner> to keep
// references to the thread's TaskRunners. This allow to try to post
// tasks between threads even during shutdown (the PostTask will then
// fail but this won't crash).
class WKHTTPSystemCookieStore::Helper {
 public:
  explicit Helper(WKWebViewConfigurationProvider* provider);

  Helper(const Helper&) = delete;
  Helper& operator=(const Helper&) = delete;

  ~Helper();

  // Type of the callbacks used by the different methods.
  using DeleteCookieCallback = base::OnceCallback<void()>;
  using InsertCookieCallback = base::OnceCallback<void()>;
  using ClearCookiesCallback = base::OnceCallback<void()>;
  using FetchCookiesCallback =
      base::OnceCallback<void(NSArray<NSHTTPCookie*>*)>;

  // Deletes `cookie` from the WKHTTPCookieStore and invokes `callback` on
  // the IO thread when the operation completes.
  void DeleteCookie(NSHTTPCookie* cookie, DeleteCookieCallback callback);

  // Inserts `cookie` into the WKHTTPCookieStore and invokes `callback` on
  // the IO thread when the operation completes.
  void InsertCookie(NSHTTPCookie* cookie, InsertCookieCallback callback);

  // Clears all cookies from the WKHTTPCookieStore and invokes `callback`
  // on the IO thread when the operation completes.
  void ClearCookies(ClearCookiesCallback callback);

  // Fetches all cookies from the WKHTTPCookieStore and invokes `callback`
  // with them on the IO thread when the operation completes. If the store
  // is deleted, the callback will still be invoked with an empty array.
  void FetchCookies(FetchCookiesCallback callback);

 private:
  SEQUENCE_CHECKER(sequence_checker_);

  // The TaskRunner used to post message to the CRWWKHTTPCookieStore
  // and the helper object used to ensure the callback are destroyed
  // when the IO thread stops.
  scoped_refptr<base::SequencedTaskRunner> ui_task_runner_;
  __strong WKHTTPSystemCookieStoreCancelableTaskHelper* helper_ = nil;

  // The CRWWKHTTPCookieStore used to store the cookies. Should only
  // be accessed on the UI sequence (thus by posting tasks on the
  // `ui_task_runner_`).
  __strong CRWWKHTTPCookieStore* crw_cookie_store_ = nil;

  base::WeakPtrFactory<Helper> weak_factory_{this};
};

WKHTTPSystemCookieStore::Helper::Helper(
    WKWebViewConfigurationProvider* provider)
    : ui_task_runner_(base::SequencedTaskRunner::GetCurrentDefault()) {
  scoped_refptr<base::SequencedTaskRunner> io_task_runner =
      web::GetIOThreadTaskRunner({});

  crw_cookie_store_ = [[CRWWKHTTPCookieStore alloc] init];
  crw_cookie_store_.websiteDataStore =
      provider->GetWebViewConfiguration().websiteDataStore;

  helper_ = [[WKHTTPSystemCookieStoreCancelableTaskHelper alloc]
      initWithTaskRunner:io_task_runner];

  // Register a callback to update the WKWebViewConfiguration directly in the
  // CRWWKHTTPCookieStore when the WKWebViewConfigurationProvider creates a
  // new configuration (both object lives on the UI sequence, so this is safe).
  //
  // Store the subscription in Objective-C object and attach as an associated
  // object of the CRWWKHTTPCookieStore, ensuring it has the same lifetime and
  // is destroyed on the correct sequence.
  [WKHTTPSystemCookieCallbackConfigCreatedRegistration
      registerCookieStore:crw_cookie_store_
             withProvider:provider];

  // The object is created on the UI sequence but then moves to the IO
  // sequence. Detach from the current sequence, it will be reattached
  // when the first method is called on the IO sequence.
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

WKHTTPSystemCookieStore::Helper::~Helper() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Delete the CRWWKHTTPCookieStore on the UI sequence by posting a
  // task that takes ownership of the object. This is okay because
  // the current object is detroyed on IO sequence which is always
  // outlived by the UI sequence.
  ui_task_runner_->PostTask(
      FROM_HERE, base::BindOnce([](CRWWKHTTPCookieStore*) {},
                                std::exchange(crw_cookie_store_, nil)));

  [helper_ cancel];
}

void WKHTTPSystemCookieStore::Helper::DeleteCookie(
    NSHTTPCookie* cookie,
    DeleteCookieCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Convert the callback to a block and ensure it is invoked on the IO thread.
  NoParamBlock block = [helper_ wrapNoParamCallback:std::move(callback)];
  __weak CRWWKHTTPCookieStore* weak_cookie_store = crw_cookie_store_;
  ui_task_runner_->PostTask(FROM_HERE, base::BindOnce(^{
                              [weak_cookie_store deleteCookie:cookie
                                            completionHandler:block];
                            }));
}

void WKHTTPSystemCookieStore::Helper::InsertCookie(
    NSHTTPCookie* cookie,
    InsertCookieCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Convert the callback to a block and ensure it is invoked on the IO thread.
  NoParamBlock block = [helper_ wrapNoParamCallback:std::move(callback)];
  __weak CRWWKHTTPCookieStore* weak_cookie_store = crw_cookie_store_;
  ui_task_runner_->PostTask(FROM_HERE, base::BindOnce(^{
                              [weak_cookie_store setCookie:cookie
                                         completionHandler:block];
                            }));
}

void WKHTTPSystemCookieStore::Helper::ClearCookies(
    ClearCookiesCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Convert the callback to a block and ensure it is invoked on the IO thread.
  NoParamBlock block = [helper_ wrapNoParamCallback:std::move(callback)];
  __weak CRWWKHTTPCookieStore* weak_cookie_store = crw_cookie_store_;
  ui_task_runner_->PostTask(FROM_HERE, base::BindOnce(^{
                              [weak_cookie_store clearCookies:block];
                            }));
}

void WKHTTPSystemCookieStore::Helper::FetchCookies(
    FetchCookiesCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Convert the callback to a block and ensure it is invoked on the IO thread.
  CookiesBlock block = [helper_ wrapCookiesCallback:std::move(callback)];
  __weak CRWWKHTTPCookieStore* weak_cookie_store = crw_cookie_store_;
  ui_task_runner_->PostTask(FROM_HERE, base::BindOnce(^{
                              if (weak_cookie_store) {
                                [weak_cookie_store getAllCookies:block];
                              } else {
                                // If the store is nil, return an empty list.
                                block(@[]);
                              }
                            }));
}

#pragma mark - WKHTTPSystemCookieStore

WKHTTPSystemCookieStore::WKHTTPSystemCookieStore(
    WKWebViewConfigurationProvider* config_provider) {
  helper_ = std::make_unique<Helper>(config_provider);
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

WKHTTPSystemCookieStore::~WKHTTPSystemCookieStore() = default;

void WKHTTPSystemCookieStore::GetCookiesForURLAsync(
    const GURL& url,
    SystemCookieCallbackForCookies callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  helper_->FetchCookies(
      base::BindOnce(&WKHTTPSystemCookieStore::FilterAndSortCookies,
                     creation_time_manager_->GetWeakPtr(), url)
          .Then(std::move(callback)));
}

void WKHTTPSystemCookieStore::GetAllCookiesAsync(
    SystemCookieCallbackForCookies callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  GetCookiesForURLAsync(GURL(), std::move(callback));
}

void WKHTTPSystemCookieStore::DeleteCookieAsync(NSHTTPCookie* cookie,
                                                SystemCookieCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::OnceClosure closure =
      base::BindOnce(&net::CookieCreationTimeManager::DeleteCreationTime,
                     creation_time_manager_->GetWeakPtr(), cookie);

  helper_->DeleteCookie(cookie,
                        ChainClosure(std::move(closure), std::move(callback)));
}

void WKHTTPSystemCookieStore::SetCookieAsync(
    NSHTTPCookie* cookie,
    const base::Time* optional_creation_time,
    SystemCookieCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const base::Time creation_time =
      optional_creation_time ? *optional_creation_time : base::Time::Now();

  base::OnceClosure closure = base::BindOnce(
      &SetCreationTimeEnsureUnique, creation_time_manager_->GetWeakPtr(),
      cookie, creation_time);

  helper_->InsertCookie(cookie,
                        ChainClosure(std::move(closure), std::move(callback)));
}

void WKHTTPSystemCookieStore::ClearStoreAsync(SystemCookieCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::OnceClosure closure =
      base::BindOnce(&net::CookieCreationTimeManager::Clear,
                     creation_time_manager_->GetWeakPtr());

  helper_->ClearCookies(ChainClosure(std::move(closure), std::move(callback)));
}

NSHTTPCookieAcceptPolicy WKHTTPSystemCookieStore::GetCookieAcceptPolicy() {
  // TODO(crbug.com/41341295): Make sure there is no other way to return
  // WKHTTPCookieStore Specific cookieAcceptPolicy.
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return [[NSHTTPCookieStorage sharedHTTPCookieStorage] cookieAcceptPolicy];
}

#pragma mark private methods

// static
NSArray<NSHTTPCookie*>* WKHTTPSystemCookieStore::FilterAndSortCookies(
    base::WeakPtr<net::CookieCreationTimeManager> weak_time_manager,
    const GURL& include_url,
    NSArray<NSHTTPCookie*>* cookies) {
  if (include_url.is_valid()) {
    NSMutableArray<NSHTTPCookie*>* filtered_cookies =
        [[NSMutableArray alloc] initWithCapacity:cookies.count];

    for (NSHTTPCookie* cookie in cookies) {
      if (ShouldIncludeForRequestUrl(cookie, include_url)) {
        [filtered_cookies addObject:cookie];
      }
    }

    cookies = [filtered_cookies copy];
  }

  if (!weak_time_manager) {
    return cookies;
  }

  return
      [cookies sortedArrayUsingFunction:net::SystemCookieStore::CompareCookies
                                context:weak_time_manager.get()];
}

}  // namespace web
