// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/proxy_resolution/proxy_resolver_apple.h"

#include <CFNetwork/CFProxySupport.h>
#include <CoreFoundation/CoreFoundation.h>

#include <atomic>
#include <memory>
#include <set>
#include <utility>

#include "base/apple/foundation_util.h"
#include "base/apple/scoped_cftyperef.h"
#include "base/check.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_pump_type.h"
#include "base/no_destructor.h"
#include "base/sequence_checker.h"
#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/synchronization/lock.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread.h"
#include "base/threading/thread_checker.h"
#include "build/build_config.h"
#include "net/base/features.h"
#include "net/base/load_states.h"
#include "net/base/net_errors.h"
#include "net/base/url_util.h"
#include "net/proxy_resolution/proxy_chain_util_apple.h"
#include "net/proxy_resolution/proxy_info.h"
#include "net/proxy_resolution/proxy_list.h"
#include "net/proxy_resolution/proxy_resolver.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_IOS)
#include <CFNetwork/CFProxySupport.h>
#else
#include <CoreServices/CoreServices.h>
#endif

#if LEAK_SANITIZER
#include <sanitizer/lsan_interface.h>
#endif

namespace net {

class NetworkAnonymizationKey;

namespace {

// A lock shared by all ProxyResolverApple instances. It is used to synchronize
// the events of multiple CFNetworkExecuteProxyAutoConfigurationURL run loop
// sources. These events are:
// 1. Adding the source to the run loop.
// 2. Handling the source result.
// 3. Removing the source from the run loop.
base::Lock& GetCFNetworkPacRunloopLock() {
  static base::NoDestructor<base::Lock> lock;
  return *lock;
}

// Work around <rdar://problem/5530166>. This dummy call to
// `CFNetworkCopyProxiesForURL` initializes some state within CFNetwork that is
// required by `CFNetworkExecuteProxyAutoConfigurationURL`.
void EnsureCFNetworkProxyStateInitialized(CFURLRef query_url_ref) {
  base::apple::ScopedCFTypeRef<CFDictionaryRef> empty_dictionary(
      CFDictionaryCreate(nullptr, nullptr, nullptr, 0, nullptr, nullptr));
  base::apple::ScopedCFTypeRef<CFArrayRef> dummy_result(
      CFNetworkCopyProxiesForURL(query_url_ref, empty_dictionary.get()));
}

// Converts a CFProxy dictionary array returned by
// `CFNetworkExecuteProxyAutoConfigurationURL` into a `ProxyList`.
ProxyList CFArrayToProxyList(CFArrayRef proxy_array_ref) {
  DCHECK(proxy_array_ref);

  ProxyList proxy_list;

  CFIndex proxy_array_count = CFArrayGetCount(proxy_array_ref);
  for (CFIndex i = 0; i < proxy_array_count; ++i) {
    CFDictionaryRef proxy_dictionary = base::apple::CFCast<CFDictionaryRef>(
        CFArrayGetValueAtIndex(proxy_array_ref, i));
    if (!proxy_dictionary) {
      continue;
    }

    // The dictionary may have the following keys:
    // - kCFProxyTypeKey : The type of the proxy
    // - kCFProxyHostNameKey
    // - kCFProxyPortNumberKey : The meat we're after.
    // - kCFProxyUsernameKey
    // - kCFProxyPasswordKey : Despite the existence of these keys in the
    //                         documentation, they're never populated. Even if a
    //                         username/password were to be set in the network
    //                         proxy system preferences, we'd need to fetch it
    //                         from the Keychain ourselves. CFProxy is such a
    //                         tease.
    // - kCFProxyAutoConfigurationURLKey : If the PAC file specifies another
    //                                     PAC file, I'm going home.

    CFStringRef proxy_type = base::apple::GetValueFromDictionary<CFStringRef>(
        proxy_dictionary, kCFProxyTypeKey);
    if (!proxy_type) {
      continue;
    }
    ProxyChain proxy_chain =
        ProxyDictionaryToProxyChain(proxy_type, proxy_dictionary,
                                    kCFProxyHostNameKey, kCFProxyPortNumberKey);

    // No need to check if `proxy_chain` is valid, as AddProxyChain() ignores
    // invalid proxy chains.
    proxy_list.AddProxyChain(proxy_chain);
  }

  return proxy_list;
}

// Populates `results` with `proxy_list` if non-empty, leaving `results` in its
// default state otherwise.
void PopulateProxyInfoFromProxyList(const ProxyList& proxy_list,
                                    ProxyInfo* results) {
  DCHECK(results);
  if (!proxy_list.IsEmpty()) {
    results->UseProxyList(proxy_list);
  }
}

// Populates `results` from a CFProxy dictionary array returned by
// `CFNetworkExecuteProxyAutoConfigurationURL`.
void PopulateProxyInfoFromCFArray(CFArrayRef proxy_array_ref,
                                  ProxyInfo* results) {
  PopulateProxyInfoFromProxyList(CFArrayToProxyList(proxy_array_ref), results);
}

// Forward declaration of the callback function used by the
// SynchronizedRunLoopObserver class.
void RunLoopObserverCallBackFunc(CFRunLoopObserverRef observer,
                                 CFRunLoopActivity activity,
                                 void* info);

// Synchronous callback for CFNetworkExecuteProxyAutoConfigurationURL. `client`
// is a pointer to a CFTypeRef. This stashes either `error` or `proxies` in
// that location and stops the private run loop.
void ResultCallback(void* client, CFArrayRef proxies, CFErrorRef error) {
  DCHECK((proxies != nullptr) == (error == nullptr));

  CFTypeRef* result_ptr = reinterpret_cast<CFTypeRef*>(client);
  DCHECK(result_ptr != nullptr);
  DCHECK(*result_ptr == nullptr);

  if (error != nullptr) {
    *result_ptr = CFRetain(error);
  } else {
    *result_ptr = CFRetain(proxies);
  }
  CFRunLoopStop(CFRunLoopGetCurrent());
}

#pragma mark - SynchronizedRunLoopObserver
// A run loop observer that guarantees that no two run loop sources protected
// by the same lock will be fired concurrently in different threads.
// The observer does not prevent the parallel execution of the sources but only
// synchronizes the run loop events associated with the sources. In the context
// of proxy resolver, the observer is used to synchronize the execution of the
// callbacks function that handles the result of
// CFNetworkExecuteProxyAutoConfigurationURL execution.
class SynchronizedRunLoopObserver final {
 public:
  // Creates the instance of an observer that will synchronize the sources
  // using a given `lock`.
  explicit SynchronizedRunLoopObserver(base::Lock& lock);

  SynchronizedRunLoopObserver(const SynchronizedRunLoopObserver&) = delete;
  SynchronizedRunLoopObserver& operator=(const SynchronizedRunLoopObserver&) =
      delete;

  // Destructor.
  ~SynchronizedRunLoopObserver();
  // Adds the observer to the current run loop for a given run loop mode.
  // This method should always be paired with `RemoveFromCurrentRunLoop`.
  void AddToCurrentRunLoop(const CFStringRef mode);
  // Removes the observer from the current run loop for a given run loop mode.
  // This method should always be paired with `AddToCurrentRunLoop`.
  void RemoveFromCurrentRunLoop(const CFStringRef mode);
  // Callback function that is called when an observable run loop event occurs.
  void RunLoopObserverCallBack(CFRunLoopObserverRef observer,
                               CFRunLoopActivity activity);

 private:
  // Lock to use to synchronize the run loop sources.
  const raw_ref<base::Lock> lock_;
  // Indicates whether the current observer holds the lock. It is used to
  // avoid double locking and releasing.
  bool lock_acquired_ = false;
  // The underlying CFRunLoopObserverRef structure wrapped by this instance.
  base::apple::ScopedCFTypeRef<CFRunLoopObserverRef> observer_;
  // Validates that all methods of this class are executed on the same thread.
  base::ThreadChecker thread_checker_;
};

SynchronizedRunLoopObserver::SynchronizedRunLoopObserver(base::Lock& lock)
    : lock_(lock) {
  CFRunLoopObserverContext observer_context = {0, this, nullptr, nullptr,
                                               nullptr};
  observer_.reset(CFRunLoopObserverCreate(
      kCFAllocatorDefault,
      kCFRunLoopBeforeSources | kCFRunLoopBeforeWaiting | kCFRunLoopExit, true,
      0, RunLoopObserverCallBackFunc, &observer_context));
}

SynchronizedRunLoopObserver::~SynchronizedRunLoopObserver() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!lock_acquired_);
}

void SynchronizedRunLoopObserver::AddToCurrentRunLoop(const CFStringRef mode) {
  DCHECK(thread_checker_.CalledOnValidThread());
  CFRunLoopAddObserver(CFRunLoopGetCurrent(), observer_.get(), mode);
}

void SynchronizedRunLoopObserver::RemoveFromCurrentRunLoop(
    const CFStringRef mode) {
  DCHECK(thread_checker_.CalledOnValidThread());
  CFRunLoopRemoveObserver(CFRunLoopGetCurrent(), observer_.get(), mode);
}

void SynchronizedRunLoopObserver::RunLoopObserverCallBack(
    CFRunLoopObserverRef observer,
    CFRunLoopActivity activity) NO_THREAD_SAFETY_ANALYSIS {
  DCHECK(thread_checker_.CalledOnValidThread());
  // Acquire the lock when a source has been signaled and going to be fired.
  // In the context of the proxy resolver that happens when the proxy for a
  // given URL has been resolved and the callback function that handles the
  // result is going to be fired.
  // Release the lock when all source events have been handled.
  //
  // NO_THREAD_SAFETY_ANALYSIS: Runtime dependent locking.
  switch (activity) {
    case kCFRunLoopBeforeSources:
      if (!lock_acquired_) {
        lock_->Acquire();
        lock_acquired_ = true;
      }
      break;
    case kCFRunLoopBeforeWaiting:
    case kCFRunLoopExit:
      if (lock_acquired_) {
        lock_acquired_ = false;
        lock_->Release();
      }
      break;
  }
}

void RunLoopObserverCallBackFunc(CFRunLoopObserverRef observer,
                                 CFRunLoopActivity activity,
                                 void* info) {
  // Forward the call to the instance of SynchronizedRunLoopObserver
  // that is associated with the current CF run loop observer.
  SynchronizedRunLoopObserver* observerInstance =
      (SynchronizedRunLoopObserver*)info;
  observerInstance->RunLoopObserverCallBack(observer, activity);
}

// Runs a single `CFRunLoop` shared by all async requests. Unlike the legacy
// path (one blocked worker thread per in-flight request), PAC downloads
// proceed concurrently while the loop waits, and requests can be cancelled
// with `CFRunLoopSourceInvalidate` without blocking the IO thread. Wrapped in
// `base::NoDestructor` so it is never joined on shutdown.
class PacCFRunLoopThread final : public base::Thread {
 public:
  PacCFRunLoopThread() : base::Thread("CFNetworkPACRunLoop") {
    CHECK(StartWithOptions(
        base::Thread::Options(base::MessagePumpType::NS_RUNLOOP, 0)));
  }

  PacCFRunLoopThread(const PacCFRunLoopThread&) = delete;
  PacCFRunLoopThread& operator=(const PacCFRunLoopThread&) = delete;

 protected:
  void Init() override {
    // `SynchronizedRunLoopObserver` is only needed for mutual exclusion with
    // the legacy synchronous path and can be removed when that fallback is
    // deleted.
    // TODO(crbug.com/570599719): Remove `SynchronizedRunLoopObserver` and
    // `GetCFNetworkPacRunloopLock()` once `kAsyncProxyResolverApple` launches.
    observer_ = std::make_unique<SynchronizedRunLoopObserver>(
        GetCFNetworkPacRunloopLock());
    observer_->AddToCurrentRunLoop(kCFRunLoopCommonModes);
  }

 private:
  std::unique_ptr<SynchronizedRunLoopObserver> observer_;
};

scoped_refptr<base::SingleThreadTaskRunner> GetPacRunLoopTaskRunner() {
  static base::NoDestructor<PacCFRunLoopThread> pac_runloop_thread;
  return pac_runloop_thread->task_runner();
}

#pragma mark - ProxyResolverApple
class ProxyResolverApple : public ProxyResolver {
 public:
  explicit ProxyResolverApple(const scoped_refptr<PacFileData>& script_data);
  ~ProxyResolverApple() override;

  // ProxyResolver methods:
  int GetProxyForURL(const GURL& url,
                     const NetworkAnonymizationKey& network_anonymization_key,
                     ProxyInfo* results,
                     CompletionOnceCallback callback,
                     std::unique_ptr<Request>* request,
                     const NetLogWithSource& net_log) override;

 private:
  class RequestImpl;

  void AddActiveRequest(RequestImpl* request) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    active_requests_.insert(request);
  }

  void RemoveActiveRequest(RequestImpl* request) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    active_requests_.erase(request);
  }

  const scoped_refptr<PacFileData> script_data_;
  std::set<raw_ptr<RequestImpl>> active_requests_;
  SEQUENCE_CHECKER(sequence_checker_);
};

class ProxyResolverApple::RequestImpl : public ProxyResolver::Request {
 public:
  RequestImpl()
      : task_runner_(base::SequencedTaskRunner::GetCurrentDefault()) {}

  RequestImpl(const RequestImpl&) = delete;
  RequestImpl& operator=(const RequestImpl&) = delete;

  ~RequestImpl() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    Cancel();
  }

  LoadState GetLoadState() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return completed_or_cancelled_ ? LOAD_STATE_IDLE
                                   : LOAD_STATE_RESOLVING_PROXY_FOR_URL;
  }

  void Start(ProxyResolverApple* resolver,
             base::apple::ScopedCFTypeRef<CFURLRef> pac_url_ref,
             base::apple::ScopedCFTypeRef<CFURLRef> query_url_ref,
             ProxyInfo* results,
             CompletionOnceCallback callback) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    resolver_ = resolver;
    results_ = results;
    callback_ = std::move(callback);

    // Bind `WeakPtr` on the origin sequence before handing `Context` to
    // `PacCFRunLoopThread`, where calling `weak_factory_.GetWeakPtr()` is not
    // allowed.
    context_ = base::MakeRefCounted<Context>(
        task_runner_, weak_factory_.GetWeakPtr(), std::move(pac_url_ref),
        std::move(query_url_ref));

    resolver_->AddActiveRequest(this);

    GetPacRunLoopTaskRunner()->PostTask(
        FROM_HERE, base::BindOnce(&Context::StartOnPacThread, context_));
  }

  void Cancel() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (completed_or_cancelled_) {
      return;
    }
    callback_.Reset();
    TeardownAndReleaseContext();
  }

 private:
  // Ref-counted state shared between `RequestImpl` on the origin sequence and
  // the `CFRunLoopSourceRef` callbacks/tasks running on `PacCFRunLoopThread`.
  // All `CFRunLoop` source creation, scheduling, and invalidation happen
  // exclusively on `PacCFRunLoopThread` so that the origin (IO) thread never
  // acquires `GetCFNetworkPacRunloopLock()` or blocks on a stalled PAC script
  // evaluation.
  class Context : public base::RefCountedThreadSafe<Context> {
   public:
    Context(scoped_refptr<base::SequencedTaskRunner> origin_task_runner,
            base::WeakPtr<RequestImpl> request,
            base::apple::ScopedCFTypeRef<CFURLRef> pac_url_ref,
            base::apple::ScopedCFTypeRef<CFURLRef> query_url_ref)
        : origin_task_runner_(std::move(origin_task_runner)),
          request_(std::move(request)),
          pac_url_ref_(std::move(pac_url_ref)),
          query_url_ref_(std::move(query_url_ref)) {
      DETACH_FROM_SEQUENCE(pac_sequence_checker_);
    }

    Context(const Context&) = delete;
    Context& operator=(const Context&) = delete;

    void MarkCancelled() { cancelled_.store(true, std::memory_order_relaxed); }

    void StartOnPacThread() {
      DCHECK_CALLED_ON_VALID_SEQUENCE(pac_sequence_checker_);
      if (cancelled_.load(std::memory_order_relaxed)) {
        return;
      }

      // Tasks on this thread may run while `SynchronizedRunLoopObserver` holds
      // `GetCFNetworkPacRunloopLock()`; never acquire it here.
      EnsureCFNetworkProxyStateInitialized(query_url_ref_.get());

      // Intentionally pass `nullptr` for `retain` and `release`:
      // 1. `this` is an opaque cookie only dereferenced by
      //    `ResultCallbackOnPacThread` on `PacCFRunLoopThread` while
      //    `runloop_source_` is valid, and `Context` is kept alive until
      //    `CancelOnPacThread` invalidates `runloop_source_` on this same
      //    thread.
      // 2. CFNetwork holds its client context until it frees the source or
      //    until a hung socket closes, so a retained `Context` would leak
      //    (when the source leaks per FB12170226) or outlive cancellation.
      CFStreamClientContext client_context = {0, this, nullptr, nullptr,
                                              nullptr};
      runloop_source_.reset(CFNetworkExecuteProxyAutoConfigurationURL(
          pac_url_ref_.get(), query_url_ref_.get(),
          &Context::ResultCallbackOnPacThread, &client_context));
#if LEAK_SANITIZER
      // CFNetworkExecuteProxyAutoConfigurationURL leaks the returned
      // CFRunLoopSourceRef. Filed as FB12170226.
      __lsan_ignore_object(runloop_source_.get());
#endif
      if (!runloop_source_) {
        origin_task_runner_->PostTask(
            FROM_HERE, base::BindOnce(&RequestImpl::OnResult, request_,
                                      ERR_FAILED, ProxyList()));
        return;
      }

      CFRunLoopAddSource(CFRunLoopGetCurrent(), runloop_source_.get(),
                         kCFRunLoopCommonModes);
    }

    void CancelOnPacThread() {
      DCHECK_CALLED_ON_VALID_SEQUENCE(pac_sequence_checker_);
      if (!runloop_source_) {
        return;
      }
      // `CFRunLoopSourceInvalidate` also removes the source from all runloops
      // and modes on which it was scheduled.
      CFRunLoopSourceInvalidate(runloop_source_.get());
      runloop_source_.reset();
    }

   private:
    friend class base::RefCountedThreadSafe<Context>;
    ~Context() = default;

    static void ResultCallbackOnPacThread(void* client,
                                          CFArrayRef proxies,
                                          CFErrorRef error) {
      auto* self = static_cast<Context*>(client);
      if (!self) {
        return;
      }
      DCHECK_CALLED_ON_VALID_SEQUENCE(self->pac_sequence_checker_);

      // CFNetwork removes the source from the runloop on completion, but only
      // `CFRunLoopSourceInvalidate` runs its cancel hook and lets the source
      // and its internal context be freed (see FB12170226). Invalidating a
      // source from inside its own callout is safe because CFRunLoop retains
      // the source across the callout.
      DCHECK(self->runloop_source_);
      CFRunLoopSourceInvalidate(self->runloop_source_.get());
      self->runloop_source_.reset();

      if (self->cancelled_.load(std::memory_order_relaxed)) {
        return;
      }

      DCHECK((proxies != nullptr) == (error == nullptr));
      int net_error = ERR_FAILED;
      ProxyList proxy_list;
      if (error == nullptr) {
        if (CFArrayRef proxy_array_ref =
                base::apple::CFCast<CFArrayRef>(proxies)) {
          proxy_list = CFArrayToProxyList(proxy_array_ref);
          net_error = OK;
        }
      }

      // Always post: this callback runs on `PacCFRunLoopThread` while
      // `SynchronizedRunLoopObserver` holds `GetCFNetworkPacRunloopLock()`,
      // and `callback_` must never run synchronously before `GetProxyForURL()`
      // returns `ERR_IO_PENDING`.
      self->origin_task_runner_->PostTask(
          FROM_HERE, base::BindOnce(&RequestImpl::OnResult, self->request_,
                                    net_error, std::move(proxy_list)));
    }

    const scoped_refptr<base::SequencedTaskRunner> origin_task_runner_;
    const base::WeakPtr<RequestImpl> request_;
    const base::apple::ScopedCFTypeRef<CFURLRef> pac_url_ref_;
    const base::apple::ScopedCFTypeRef<CFURLRef> query_url_ref_;
    std::atomic<bool> cancelled_{false};
    base::apple::ScopedCFTypeRef<CFRunLoopSourceRef> runloop_source_;
    SEQUENCE_CHECKER(pac_sequence_checker_);
  };

  void TeardownAndReleaseContext() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    completed_or_cancelled_ = true;
    results_ = nullptr;
    if (resolver_) {
      resolver_->RemoveActiveRequest(this);
      resolver_ = nullptr;
    }
    weak_factory_.InvalidateWeakPtrs();
    if (context_) {
      context_->MarkCancelled();
      GetPacRunLoopTaskRunner()->PostTask(
          FROM_HERE,
          base::BindOnce(&Context::CancelOnPacThread, std::move(context_)));
    }
  }

  void OnResult(int net_error, ProxyList proxy_list) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (completed_or_cancelled_) {
      return;
    }

    if (net_error == OK) {
      PopulateProxyInfoFromProxyList(proxy_list, results_);
    }

    // Move `callback_` to a local and clear all pointers before invoking
    // `callback`, as `callback.Run()` may synchronously destroy `this`,
    // `results_`, or `resolver_`.
    CompletionOnceCallback callback = std::move(callback_);
    TeardownAndReleaseContext();

    std::move(callback).Run(net_error);
  }

  const scoped_refptr<base::SequencedTaskRunner> task_runner_;
  raw_ptr<ProxyResolverApple> resolver_ = nullptr;
  scoped_refptr<Context> context_;
  raw_ptr<ProxyInfo> results_ = nullptr;
  CompletionOnceCallback callback_;
  bool completed_or_cancelled_ = false;
  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<RequestImpl> weak_factory_{this};
};

ProxyResolverApple::ProxyResolverApple(
    const scoped_refptr<PacFileData>& script_data)
    : script_data_(script_data) {
  // Under `MultiThreadedProxyResolver` (when `kAsyncProxyResolverApple` is
  // disabled), `ProxyResolverApple` is constructed and queried on a worker
  // thread, then destroyed on the origin (IO) thread after the worker thread is
  // joined. Detach in the constructor so `sequence_checker_` only binds when
  // `active_requests_` is accessed on the async path.
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

ProxyResolverApple::~ProxyResolverApple() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (auto& request : std::exchange(active_requests_, {})) {
    request->Cancel();
  }
}

// Gets the proxy information for a query URL from a PAC. Implementation
// inspired by http://developer.apple.com/samplecode/CFProxySupportTool/
int ProxyResolverApple::GetProxyForURL(
    const GURL& query_url,
    const NetworkAnonymizationKey& network_anonymization_key,
    ProxyInfo* results,
    CompletionOnceCallback callback,
    std::unique_ptr<Request>* request,
    const NetLogWithSource& net_log) {
  // OS X's system resolver does not support WebSocket URLs in proxy.pac, as of
  // version 10.13.5. See https://crbug.com/862121.
  GURL mutable_query_url =
      query_url.SchemeIsWSOrWSS()
          ? net::ChangeWebSocketSchemeToHttpScheme(query_url)
          : query_url;

  base::apple::ScopedCFTypeRef<CFStringRef> query_ref(
      base::SysUTF8ToCFStringRef(mutable_query_url.spec()));
  base::apple::ScopedCFTypeRef<CFURLRef> query_url_ref(
      CFURLCreateWithString(kCFAllocatorDefault, query_ref.get(), nullptr));
  if (!query_url_ref.get()) {
    return ERR_FAILED;
  }
  base::apple::ScopedCFTypeRef<CFStringRef> pac_ref(base::SysUTF8ToCFStringRef(
      script_data_->type() == PacFileData::TYPE_AUTO_DETECT
          ? std::string()
          : script_data_->url().spec()));
  base::apple::ScopedCFTypeRef<CFURLRef> pac_url_ref(
      CFURLCreateWithString(kCFAllocatorDefault, pac_ref.get(), nullptr));
  if (!pac_url_ref.get()) {
    return ERR_FAILED;
  }

  if (base::FeatureList::IsEnabled(features::kAsyncProxyResolverApple)) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    CHECK(callback);
    CHECK(request);
    auto req = std::make_unique<RequestImpl>();
    req->Start(this, std::move(pac_url_ref), std::move(query_url_ref), results,
               std::move(callback));
    *request = std::move(req);
    return ERR_IO_PENDING;
  }

  // Legacy synchronous fallback path.
  EnsureCFNetworkProxyStateInitialized(query_url_ref.get());

  CFTypeRef result = nullptr;
  CFStreamClientContext context = {0, &result, nullptr, nullptr, nullptr};
  base::apple::ScopedCFTypeRef<CFRunLoopSourceRef> runloop_source(
      CFNetworkExecuteProxyAutoConfigurationURL(
          pac_url_ref.get(), query_url_ref.get(), ResultCallback, &context));
#if LEAK_SANITIZER
  // CFNetworkExecuteProxyAutoConfigurationURL leaks the returned
  // CFRunLoopSourceRef. Filed as FB12170226.
  __lsan_ignore_object(runloop_source.get());
#endif
  if (!runloop_source) {
    return ERR_FAILED;
  }

  const CFStringRef private_runloop_mode =
      CFSTR("org.chromium.ProxyResolverApple");

  // Add the run loop observer to synchronize events of
  // CFNetworkExecuteProxyAutoConfigurationURL sources.
  SynchronizedRunLoopObserver observer(GetCFNetworkPacRunloopLock());
  observer.AddToCurrentRunLoop(private_runloop_mode);

  // Make sure that no CFNetworkExecuteProxyAutoConfigurationURL sources
  // are added to the run loop concurrently.
  {
    base::AutoLock lock(GetCFNetworkPacRunloopLock());
    CFRunLoopAddSource(CFRunLoopGetCurrent(), runloop_source.get(),
                       private_runloop_mode);
  }

  CFRunLoopRunInMode(private_runloop_mode, DBL_MAX, false);

  // Make sure that no CFNetworkExecuteProxyAutoConfigurationURL sources
  // are removed from the run loop concurrently.
  {
    base::AutoLock lock(GetCFNetworkPacRunloopLock());
    CFRunLoopRemoveSource(CFRunLoopGetCurrent(), runloop_source.get(),
                          private_runloop_mode);
  }
  observer.RemoveFromCurrentRunLoop(private_runloop_mode);

  if (!result) {
    return ERR_FAILED;
  }

  base::apple::ScopedCFTypeRef<CFTypeRef> scoped_result(result);
  if (CFGetTypeID(scoped_result.get()) == CFErrorGetTypeID()) {
    return ERR_FAILED;
  }
  CFArrayRef proxy_array_ref =
      base::apple::CFCast<CFArrayRef>(scoped_result.get());
  if (!proxy_array_ref) {
    return ERR_FAILED;
  }
  PopulateProxyInfoFromCFArray(proxy_array_ref, results);
  return OK;
}

}  // namespace

ProxyResolverFactoryApple::ProxyResolverFactoryApple()
    : ProxyResolverFactory(false /*expects_pac_bytes*/) {
}

int ProxyResolverFactoryApple::CreateProxyResolver(
    const scoped_refptr<PacFileData>& pac_script,
    std::unique_ptr<ProxyResolver>* resolver,
    CompletionOnceCallback callback,
    std::unique_ptr<Request>* request) {
  *resolver = std::make_unique<ProxyResolverApple>(pac_script);
  return OK;
}

}  // namespace net
