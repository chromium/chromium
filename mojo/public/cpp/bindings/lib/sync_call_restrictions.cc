// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/sync_call_restrictions.h"

#include "base/check_op.h"
#include "base/debug/leak_annotations.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/synchronization/lock.h"
#include "base/threading/sequence_local_storage_map.h"
#include "base/threading/sequence_local_storage_slot.h"
#include "mojo/public/c/system/core.h"

namespace mojo {

namespace {

// Sync call interrupts are enabled by default.
bool g_enable_sync_call_interrupts = true;

#if ENABLE_SYNC_CALL_RESTRICTIONS

class GlobalSyncCallSettings {
 public:
  GlobalSyncCallSettings() = default;

  GlobalSyncCallSettings(const GlobalSyncCallSettings&) = delete;
  GlobalSyncCallSettings& operator=(const GlobalSyncCallSettings&) = delete;

  ~GlobalSyncCallSettings() = default;

  bool sync_call_allowed_by_default() const {
    base::AutoLock lock(lock_);
    return sync_call_allowed_by_default_;
  }

  void DisallowSyncCallByDefault() {
    base::AutoLock lock(lock_);
    sync_call_allowed_by_default_ = false;
  }

 private:
  mutable base::Lock lock_;
  bool sync_call_allowed_by_default_ = true;
};

GlobalSyncCallSettings& GetGlobalSettings() {
  static base::NoDestructor<GlobalSyncCallSettings> global_settings;
  return *global_settings;
}

size_t& GetSequenceLocalScopedAllowCount() {
  static base::SequenceLocalStorageSlot<size_t> count;
  return count.GetOrCreateValue();
}

// Sometimes sync calls need to be made while sequence-local storage is not
// initialized. In particular this can occur during thread tear-down while TLS
// objects (including SequenceLocalStorageMap itself) are being destroyed. We
// can't track a sequence-local policy in such cases, so we don't enforce one.
bool SyncCallRestrictionsEnforceable() {
  return base::internal::SequenceLocalStorageMap::IsSetForCurrentThread();
}

#endif  // ENABLE_SYNC_CALL_RESTRICTIONS

}  // namespace

#if ENABLE_SYNC_CALL_RESTRICTIONS

// static
void SyncCallRestrictions::AssertSyncCallAllowed() {
  if (GetGlobalSettings().sync_call_allowed_by_default() ||
      !SyncCallRestrictionsEnforceable()) {
    return;
  }

  if (GetSequenceLocalScopedAllowCount() > 0)
    return;

  LOG(FATAL) << "Mojo sync calls are not allowed in this process because "
             << "they can lead to jank and deadlock. If you must make an "
             << "exception, please see "
             << "SyncCallRestrictions::ScopedAllowSyncCall and consult "
             << "mojo/OWNERS.";
}

// static
void SyncCallRestrictions::DisallowSyncCall() {
  GetGlobalSettings().DisallowSyncCallByDefault();
}

// static
void SyncCallRestrictions::IncreaseScopedAllowCount() {
  if (!SyncCallRestrictionsEnforceable())
    return;

  ++GetSequenceLocalScopedAllowCount();
}

// static
void SyncCallRestrictions::DecreaseScopedAllowCount() {
  if (!SyncCallRestrictionsEnforceable())
    return;

  DCHECK_GT(GetSequenceLocalScopedAllowCount(), 0u);
  --GetSequenceLocalScopedAllowCount();
}

#endif  // ENABLE_SYNC_CALL_RESTRICTIONS

// static
void SyncCallRestrictions::DisableSyncCallInterrupts() {
  g_enable_sync_call_interrupts = false;
}

// static
void SyncCallRestrictions::EnableSyncCallInterruptsForTesting() {
  g_enable_sync_call_interrupts = true;
}

// static
bool SyncCallRestrictions::AreSyncCallInterruptsEnabled() {
  return g_enable_sync_call_interrupts;
}

}  // namespace mojo
