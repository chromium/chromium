// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/sync_call_restrictions.h"

#if ENABLE_SYNC_CALL_RESTRICTIONS

#include "base/debug/leak_annotations.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/no_destructor.h"
#include "base/synchronization/lock.h"
#include "base/threading/sequence_local_storage_slot.h"
#include "mojo/public/c/system/core.h"

namespace mojo {

namespace {

class GlobalSyncCallSettings {
 public:
  GlobalSyncCallSettings() = default;
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

  DISALLOW_COPY_AND_ASSIGN(GlobalSyncCallSettings);
};

GlobalSyncCallSettings& GetGlobalSettings() {
  static base::NoDestructor<GlobalSyncCallSettings> global_settings;
  return *global_settings;
}

size_t& GetSequenceLocalScopedAllowCount() {
  static base::NoDestructor<base::SequenceLocalStorageSlot<size_t>> count;
  return count->GetOrCreateValue();
}

}  // namespace

// static
void SyncCallRestrictions::AssertSyncCallAllowed() {
  if (GetGlobalSettings().sync_call_allowed_by_default())
    return;
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
  ++GetSequenceLocalScopedAllowCount();
}

// static
void SyncCallRestrictions::DecreaseScopedAllowCount() {
  DCHECK_GT(GetSequenceLocalScopedAllowCount(), 0u);
  --GetSequenceLocalScopedAllowCount();
}

}  // namespace mojo

#endif  // ENABLE_SYNC_CALL_RESTRICTIONS
