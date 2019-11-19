// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_SYNC_CALL_RESTRICTIONS_H_
#define MOJO_PUBLIC_CPP_BINDINGS_SYNC_CALL_RESTRICTIONS_H_

#include "base/component_export.h"
#include "base/macros.h"
#include "base/threading/thread_restrictions.h"

#if (!defined(NDEBUG) || defined(DCHECK_ALWAYS_ON))
#define ENABLE_SYNC_CALL_RESTRICTIONS 1
#else
#define ENABLE_SYNC_CALL_RESTRICTIONS 0
#endif

namespace sync_preferences {
class PrefServiceSyncable;
}

namespace prefs {
class PersistentPrefStoreClient;
}

namespace ui {
class HostContextFactoryPrivate;
}  // namespace ui

namespace viz {
class HostFrameSinkManager;
}

namespace mojo {
class ScopedAllowSyncCallForTesting;

// In some processes, sync calls are disallowed. For example, in the browser
// process we don't want any sync calls to child processes for performance,
// security and stability reasons. SyncCallRestrictions helps to enforce such
// rules.
//
// Before processing a sync call, the bindings call
// SyncCallRestrictions::AssertSyncCallAllowed() to check whether sync calls are
// allowed. By default sync calls are allowed but they may be globally
// disallowed within a process by calling DisallowSyncCall().
//
// If globally disallowed but you but you have a very compelling reason to
// disregard that (which should be very very rare), you can override it by
// constructing a ScopedAllowSyncCall object which allows making sync calls on
// the current sequence during its lifetime.
class COMPONENT_EXPORT(MOJO_CPP_BINDINGS) SyncCallRestrictions {
 public:
#if ENABLE_SYNC_CALL_RESTRICTIONS
  // Checks whether the current sequence is allowed to make sync calls, and
  // causes a DCHECK if not.
  static void AssertSyncCallAllowed();

  // Disables sync calls within the calling process. Any caller who wishes to
  // make sync calls once this has been invoked must do so within the extent of
  // a ScopedAllowSyncCall or ScopedAllowSyncCallForTesting.
  static void DisallowSyncCall();

#else
  // Inline the empty definitions of functions so that they can be compiled out.
  static void AssertSyncCallAllowed() {}
  static void DisallowSyncCall() {}
#endif

 private:
  // DO NOT ADD ANY OTHER FRIEND STATEMENTS, talk to mojo/OWNERS first.
  // BEGIN ALLOWED USAGE.
  // SynchronousCompositorHost is used for Android webview.
  friend class content::SynchronousCompositorHost;
  // Pref service connection is sync at startup.
  friend class prefs::PersistentPrefStoreClient;
  // Incognito pref service instances are created synchronously.
  friend class sync_preferences::PrefServiceSyncable;
  friend class mojo::ScopedAllowSyncCallForTesting;
  // For destroying the GL context/surface that draw to a platform window before
  // the platform window is destroyed.
  friend class viz::HostFrameSinkManager;
  // For preventing frame swaps of wrong size during resize on Windows.
  // (https://crbug.com/811945)
  friend class ui::HostContextFactoryPrivate;
  // END ALLOWED USAGE.

#if ENABLE_SYNC_CALL_RESTRICTIONS
  static void IncreaseScopedAllowCount();
  static void DecreaseScopedAllowCount();
#else
  static void IncreaseScopedAllowCount() {}
  static void DecreaseScopedAllowCount() {}
#endif

  // If a process is configured to disallow sync calls in general, constructing
  // a ScopedAllowSyncCall object temporarily allows making sync calls on the
  // current sequence. Doing this is almost always incorrect, which is why we
  // limit who can use this through friend. If you find yourself needing to use
  // this, talk to mojo/OWNERS.
  class ScopedAllowSyncCall {
   public:
    ScopedAllowSyncCall() { IncreaseScopedAllowCount(); }
    ~ScopedAllowSyncCall() { DecreaseScopedAllowCount(); }

   private:
#if ENABLE_SYNC_CALL_RESTRICTIONS
    base::ScopedAllowBaseSyncPrimitivesOutsideBlockingScope allow_wait_;
#endif

    DISALLOW_COPY_AND_ASSIGN(ScopedAllowSyncCall);
  };

  DISALLOW_IMPLICIT_CONSTRUCTORS(SyncCallRestrictions);
};

class ScopedAllowSyncCallForTesting {
 public:
  ScopedAllowSyncCallForTesting() {}
  ~ScopedAllowSyncCallForTesting() {}

 private:
  SyncCallRestrictions::ScopedAllowSyncCall scoped_allow_sync_call_;

  DISALLOW_COPY_AND_ASSIGN(ScopedAllowSyncCallForTesting);
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_SYNC_CALL_RESTRICTIONS_H_
