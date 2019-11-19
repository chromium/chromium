// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/network_config_watcher_mac.h"

#include <algorithm>

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_pump_type.h"
#include "base/metrics/histogram_macros.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"

namespace net {

namespace {

// SCDynamicStore API does not exist on iOS.
#if !defined(OS_IOS)
const base::TimeDelta kRetryInterval = base::TimeDelta::FromSeconds(1);
const int kMaxRetry = 5;

// Maps SCError to an enum for UMA logging. These values are persisted to logs,
// and should not be renumbered. Added to investigate https://crbug.com/547877.
enum class SCStatusCode {
  // Unmapped error codes.
  SC_UNKNOWN = 0,

  // These map to the corresponding SCError.
  SC_OK = 1,
  SC_FAILED = 2,
  SC_INVALID_ARGUMENT = 3,
  SC_ACCESS_ERROR = 4,
  SC_NO_KEY = 5,
  SC_KEY_EXISTS = 6,
  SC_LOCKED = 7,
  SC_NEED_LOCK = 8,
  SC_NO_STORE_SESSION = 9,
  SC_NO_STORE_SERVER = 10,
  SC_NOTIFIER_ACTIVE = 11,
  SC_NO_PREFS_SESSION = 12,
  SC_PREFS_BUSY = 13,
  SC_NO_CONFIG_FILE = 14,
  SC_NO_LINK = 15,
  SC_STALE = 16,
  SC_MAX_LINK = 17,
  SC_REACHABILITY_UNKNOWN = 18,
  SC_CONNECTION_NO_SERVICE = 19,
  SC_CONNECTION_IGNORE = 20,

  // Maximum value for histogram bucket.
  SC_COUNT,
};

SCStatusCode ConvertToSCStatusCode(int sc_error) {
  switch (sc_error) {
    case kSCStatusOK:
      return SCStatusCode::SC_OK;
    case kSCStatusFailed:
      return SCStatusCode::SC_FAILED;
    case kSCStatusInvalidArgument:
      return SCStatusCode::SC_INVALID_ARGUMENT;
    case kSCStatusAccessError:
      return SCStatusCode::SC_ACCESS_ERROR;
    case kSCStatusNoKey:
      return SCStatusCode::SC_NO_KEY;
    case kSCStatusKeyExists:
      return SCStatusCode::SC_KEY_EXISTS;
    case kSCStatusLocked:
      return SCStatusCode::SC_LOCKED;
    case kSCStatusNeedLock:
      return SCStatusCode::SC_NEED_LOCK;
    case kSCStatusNoStoreSession:
      return SCStatusCode::SC_NO_STORE_SESSION;
    case kSCStatusNoStoreServer:
      return SCStatusCode::SC_NO_STORE_SERVER;
    case kSCStatusNotifierActive:
      return SCStatusCode::SC_NOTIFIER_ACTIVE;
    case kSCStatusNoPrefsSession:
      return SCStatusCode::SC_NO_PREFS_SESSION;
    case kSCStatusPrefsBusy:
      return SCStatusCode::SC_PREFS_BUSY;
    case kSCStatusNoConfigFile:
      return SCStatusCode::SC_NO_CONFIG_FILE;
    case kSCStatusNoLink:
      return SCStatusCode::SC_NO_LINK;
    case kSCStatusStale:
      return SCStatusCode::SC_STALE;
    case kSCStatusMaxLink:
      return SCStatusCode::SC_MAX_LINK;
    case kSCStatusReachabilityUnknown:
      return SCStatusCode::SC_REACHABILITY_UNKNOWN;
    case kSCStatusConnectionNoService:
      return SCStatusCode::SC_CONNECTION_NO_SERVICE;
    case kSCStatusConnectionIgnore:
      return SCStatusCode::SC_CONNECTION_IGNORE;
    default:
      return SCStatusCode::SC_UNKNOWN;
  }
}

// Called back by OS.  Calls OnNetworkConfigChange().
void DynamicStoreCallback(SCDynamicStoreRef /* store */,
                          CFArrayRef changed_keys,
                          void* config_delegate) {
  NetworkConfigWatcherMac::Delegate* net_config_delegate =
      static_cast<NetworkConfigWatcherMac::Delegate*>(config_delegate);
  net_config_delegate->OnNetworkConfigChange(changed_keys);
}
#endif  // !defined(OS_IOS)

}  // namespace

class NetworkConfigWatcherMacThread : public base::Thread {
 public:
  NetworkConfigWatcherMacThread(NetworkConfigWatcherMac::Delegate* delegate);
  ~NetworkConfigWatcherMacThread() override;

 protected:
  // base::Thread
  void Init() override;
  void CleanUp() override;

 private:
  // The SystemConfiguration calls in this function can lead to contention early
  // on, so we invoke this function later on in startup to keep it fast.
  void InitNotifications();

  // Returns whether initializing notifications has succeeded.
  bool InitNotificationsHelper();

  base::ScopedCFTypeRef<CFRunLoopSourceRef> run_loop_source_;
  NetworkConfigWatcherMac::Delegate* const delegate_;
#if !defined(OS_IOS)
  int num_retry_;
#endif  // !defined(OS_IOS)
  base::WeakPtrFactory<NetworkConfigWatcherMacThread> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(NetworkConfigWatcherMacThread);
};

NetworkConfigWatcherMacThread::NetworkConfigWatcherMacThread(
    NetworkConfigWatcherMac::Delegate* delegate)
    : base::Thread("NetworkConfigWatcher"),
      delegate_(delegate),
#if !defined(OS_IOS)
      num_retry_(0),
#endif  // !defined(OS_IOS)
      weak_factory_(this) {
}

NetworkConfigWatcherMacThread::~NetworkConfigWatcherMacThread() {
  // This is expected to be invoked during shutdown.
  base::ScopedAllowBaseSyncPrimitivesOutsideBlockingScope allow_thread_join;
  Stop();
}

void NetworkConfigWatcherMacThread::Init() {
  base::ThreadRestrictions::SetIOAllowed(true);
  delegate_->Init();

  // TODO(willchan): Look to see if there's a better signal for when it's ok to
  // initialize this, rather than just delaying it by a fixed time.
  const base::TimeDelta kInitializationDelay = base::TimeDelta::FromSeconds(1);
  task_runner()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&NetworkConfigWatcherMacThread::InitNotifications,
                     weak_factory_.GetWeakPtr()),
      kInitializationDelay);
}

void NetworkConfigWatcherMacThread::CleanUp() {
  if (!run_loop_source_.get())
    return;

  CFRunLoopRemoveSource(CFRunLoopGetCurrent(), run_loop_source_.get(),
                        kCFRunLoopCommonModes);
  run_loop_source_.reset();
}

void NetworkConfigWatcherMacThread::InitNotifications() {
  // If initialization fails, retry after a 1s delay.
  bool success = InitNotificationsHelper();

#if !defined(OS_IOS)
  if (!success && num_retry_ < kMaxRetry) {
    LOG(ERROR) << "Retrying SystemConfiguration registration in 1 second.";
    task_runner()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&NetworkConfigWatcherMacThread::InitNotifications,
                       weak_factory_.GetWeakPtr()),
        kRetryInterval);
    num_retry_++;
    return;
  }

  // There are kMaxRetry + 2 buckets. The 0 bucket is where no retry is
  // performed. The kMaxRetry + 1 bucket is where all retries have failed.
  int histogram_bucket = num_retry_;
  if (!success) {
    DCHECK_EQ(kMaxRetry, num_retry_);
    histogram_bucket = kMaxRetry + 1;
  }
  UMA_HISTOGRAM_EXACT_LINEAR(
      "Net.NetworkConfigWatcherMac.SCDynamicStore.NumRetry", histogram_bucket,
      kMaxRetry + 2);
#else
  DCHECK(success);
#endif  // !defined(OS_IOS)
}

bool NetworkConfigWatcherMacThread::InitNotificationsHelper() {
#if !defined(OS_IOS)
  // SCDynamicStore API does not exist on iOS.
  // Add a run loop source for a dynamic store to the current run loop.
  SCDynamicStoreContext context = {
    0,          // Version 0.
    delegate_,  // User data.
    NULL,       // This is not reference counted.  No retain function.
    NULL,       // This is not reference counted.  No release function.
    NULL,       // No description for this.
  };
  base::ScopedCFTypeRef<SCDynamicStoreRef> store(SCDynamicStoreCreate(
      NULL, CFSTR("org.chromium"), DynamicStoreCallback, &context));
  if (!store) {
    int error = SCError();
    LOG(ERROR) << "SCDynamicStoreCreate failed with Error: " << error << " - "
               << SCErrorString(error);
    UMA_HISTOGRAM_ENUMERATION(
        "Net.NetworkConfigWatcherMac.SCDynamicStore.Create",
        ConvertToSCStatusCode(error), SCStatusCode::SC_COUNT);
    return false;
  }
  run_loop_source_.reset(SCDynamicStoreCreateRunLoopSource(
      NULL, store.get(), 0));
  if (!run_loop_source_) {
    int error = SCError();
    LOG(ERROR) << "SCDynamicStoreCreateRunLoopSource failed with Error: "
               << error << " - " << SCErrorString(error);
    UMA_HISTOGRAM_ENUMERATION(
        "Net.NetworkConfigWatcherMac.SCDynamicStore.Create.RunLoopSource",
        ConvertToSCStatusCode(error), SCStatusCode::SC_COUNT);
    return false;
  }
  CFRunLoopAddSource(CFRunLoopGetCurrent(), run_loop_source_.get(),
                     kCFRunLoopCommonModes);
#endif  // !defined(OS_IOS)

  // Set up notifications for interface and IP address changes.
  delegate_->StartReachabilityNotifications();
#if !defined(OS_IOS)
  delegate_->SetDynamicStoreNotificationKeys(store.get());
#endif  // !defined(OS_IOS)
  return true;
}

NetworkConfigWatcherMac::NetworkConfigWatcherMac(Delegate* delegate)
    : notifier_thread_(new NetworkConfigWatcherMacThread(delegate)) {
  // We create this notifier thread because the notification implementation
  // needs a thread with a CFRunLoop, and there's no guarantee that
  // MessageLoopCurrent::Get() meets that criterion.
  base::Thread::Options thread_options(base::MessagePumpType::UI, 0);
  notifier_thread_->StartWithOptions(thread_options);
}

NetworkConfigWatcherMac::~NetworkConfigWatcherMac() {}

}  // namespace net
