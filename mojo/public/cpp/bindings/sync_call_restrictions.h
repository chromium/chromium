// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_SYNC_CALL_RESTRICTIONS_H_
#define MOJO_PUBLIC_CPP_BINDINGS_SYNC_CALL_RESTRICTIONS_H_

#include "base/component_export.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"

#if (!defined(NDEBUG) || defined(DCHECK_ALWAYS_ON))
#define ENABLE_SYNC_CALL_RESTRICTIONS 1
#else
#define ENABLE_SYNC_CALL_RESTRICTIONS 0
#endif

namespace chromecast {
class CastCdmOriginProvider;
}  // namespace chromecast

namespace content {
class AndroidOverlaySyncHelper;
class DesktopCapturerLacros;
class StreamTextureFactory;
#if BUILDFLAG(IS_WIN)
class DCOMPTextureFactory;
#endif
}  // namespace content

namespace crosapi {
class ScopedAllowSyncCall;
}  // namespace crosapi

namespace gpu {
class CommandBufferProxyImpl;
class GpuChannelHost;
class SharedImageInterfaceProxy;
}  // namespace gpu

namespace ui {
class Compositor;
}  // namespace ui

namespace viz {
class GpuHostImpl;
class HostFrameSinkManager;
class HostGpuMemoryBufferManager;
}  // namespace viz

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
  SyncCallRestrictions() = delete;
  SyncCallRestrictions(const SyncCallRestrictions&) = delete;
  SyncCallRestrictions& operator=(const SyncCallRestrictions&) = delete;

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

  // Globally disables sync call interrupts. This means that all sync calls in
  // the current process will be strictly blocking until a reply is received,
  // and no incoming sync calls can dispatch on the blocking thread in interim.
  static void DisableSyncCallInterrupts();

  // Used only in tests to re-enable sync call interrupts after disabling them.
  static void EnableSyncCallInterruptsForTesting();

  // Indicates whether sync call interrupts are enabled in the calling process.
  // They're enabled by default, so any sync message that isn't marked [Sync]
  // may have its blocking call interrupted to dispatch other incoming sync
  // IPCs which target the blocking thread.
  static bool AreSyncCallInterruptsEnabled();

 private:
  // DO NOT ADD ANY OTHER FRIEND STATEMENTS, talk to mojo/OWNERS first.
  // BEGIN ALLOWED USAGE.
  // SynchronousCompositorHost is used for Android webview.
  friend class content::SynchronousCompositorHost;
  // Lacros-chrome is allowed to make sync calls to ash-chrome to mimic
  // cross-platform sync APIs.
  friend class content::DesktopCapturerLacros;
  friend class crosapi::ScopedAllowSyncCall;
  friend class mojo::ScopedAllowSyncCallForTesting;
  friend class viz::GpuHostImpl;
  // For destroying the GL context/surface that draw to a platform window before
  // the platform window is destroyed.
  friend class viz::HostFrameSinkManager;
  friend class viz::HostGpuMemoryBufferManager;
  // For preventing frame swaps of wrong size during resize on Windows.
  // (https://crbug.com/811945)
  friend class ui::Compositor;
  // For calling sync mojo API to get cdm origin. The service and the client are
  // running in the same process, so it won't block anything.
  // TODO(159346933) Remove once the origin isolation logic is moved outside of
  // cast media service.
  friend class chromecast::CastCdmOriginProvider;
  // Android requires synchronous processing when overlay surfaces are
  // destroyed, else behavior is undefined.
  friend class content::AndroidOverlaySyncHelper;
  // GPU client code uses a few sync IPCs, grandfathered in from legacy IPC.
  friend class gpu::GpuChannelHost;
  friend class gpu::CommandBufferProxyImpl;
  friend class gpu::SharedImageInterfaceProxy;
  friend class content::StreamTextureFactory;
#if BUILDFLAG(IS_WIN)
  friend class content::DCOMPTextureFactory;
#endif
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

    ScopedAllowSyncCall(const ScopedAllowSyncCall&) = delete;
    ScopedAllowSyncCall& operator=(const ScopedAllowSyncCall&) = delete;

    ~ScopedAllowSyncCall() { DecreaseScopedAllowCount(); }

   private:
    base::ScopedAllowBaseSyncPrimitivesOutsideBlockingScope allow_wait_;
  };
};

class ScopedAllowSyncCallForTesting {
 public:
  ScopedAllowSyncCallForTesting() {}

  ScopedAllowSyncCallForTesting(const ScopedAllowSyncCallForTesting&) = delete;
  ScopedAllowSyncCallForTesting& operator=(
      const ScopedAllowSyncCallForTesting&) = delete;

  ~ScopedAllowSyncCallForTesting() {}

 private:
  SyncCallRestrictions::ScopedAllowSyncCall scoped_allow_sync_call_;
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_SYNC_CALL_RESTRICTIONS_H_
