// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/dns_reloader.h"

#include "build/build_config.h"

// If we're not on a POSIX system, it's not even safe to try to include resolv.h
// - there's not guarantee it exists at all. :(
#if BUILDFLAG(IS_POSIX)

#include <resolv.h>

// This code only works on systems where the C library provides res_ninit(3) and
// res_nclose(3), which requires __RES >= 19991006 (most libcs at this point,
// but not all).
//
// This code is also not used on either macOS or iOS, even though both platforms
// have res_ninit(3). On iOS, /etc/hosts is immutable so there's no reason for
// us to watch it; on macOS, there is a system mechanism for listening to DNS
// changes which does not require use to do this kind of reloading. See
// //net/dns/dns_config_watcher_mac.cc.
//
// It *also* is not used on Android, because Android handles nameserver changes
// for us and has no /etc/resolv.conf. Despite that, Bionic does export these
// interfaces, so we need to not use them.
//
// It is also also not used on Fuchsia. Regrettably, Fuchsia's resolv.h has
// __RES set to 19991006, but does not actually provide res_ninit(3). This was
// an old musl bug that was fixed by musl c8fdcfe5, but Fuchsia's SDK doesn't
// have that change.
#if defined(__RES) && __RES >= 19991006 && !BUILDFLAG(IS_APPLE) && \
    !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_FUCHSIA)
// We define this so we don't need to restate the complex condition here twice
// below - it would be easy for the copies below to get out of sync.
#define USE_RES_NINIT
#endif  // defined(_RES) && ...
#endif  // BUILDFLAG(IS_POSIX)

#if defined(USE_RES_NINIT)

#include "base/lazy_instance.h"
#include "base/notreached.h"
#include "base/synchronization/lock.h"
#include "base/task/current_thread.h"
#include "base/threading/thread_local.h"
#include "net/base/network_change_notifier.h"

namespace net {

namespace {

// On Linux/BSD, changes to /etc/resolv.conf can go unnoticed thus resulting
// in DNS queries failing either because nameservers are unknown on startup
// or because nameserver info has changed as a result of e.g. connecting to
// a new network. Some distributions patch glibc to stat /etc/resolv.conf
// to try to automatically detect such changes but these patches are not
// universal and even patched systems such as Jaunty appear to need calls
// to res_ninit to reload the nameserver information in different threads.
//
// To fix this, on systems with FilePathWatcher support, we use
// NetworkChangeNotifier::DNSObserver to monitor /etc/resolv.conf to
// enable us to respond to DNS changes and reload the resolver state.
//
// Android does not have /etc/resolv.conf. The system takes care of nameserver
// changes, so none of this is needed.
//
// TODO(crbug.com/40630884): Convert to SystemDnsConfigChangeNotifier because
// this really only cares about system DNS config changes, not Chrome effective
// config changes.

class DnsReloader : public NetworkChangeNotifier::DNSObserver {
 public:
  DnsReloader(const DnsReloader&) = delete;
  DnsReloader& operator=(const DnsReloader&) = delete;

  // NetworkChangeNotifier::DNSObserver:
  void OnDNSChanged() override {
    base::AutoLock lock(lock_);
    resolver_generation_++;
  }

  void MaybeReload() {
    ReloadState* reload_state = tls_reload_state_.Get();
    base::AutoLock lock(lock_);

    if (!reload_state) {
      auto new_reload_state = std::make_unique<ReloadState>();
      new_reload_state->resolver_generation = resolver_generation_;
      res_ninit(&_res);
      tls_reload_state_.Set(std::move(new_reload_state));
    } else if (reload_state->resolver_generation != resolver_generation_) {
      reload_state->resolver_generation = resolver_generation_;
      // It is safe to call res_nclose here since we know res_ninit will have
      // been called above.
      res_nclose(&_res);
      res_ninit(&_res);
    }
  }

 private:
  struct ReloadState {
    ~ReloadState() { res_nclose(&_res); }

    int resolver_generation;
  };

  DnsReloader() { NetworkChangeNotifier::AddDNSObserver(this); }

  ~DnsReloader() override {
    NOTREACHED_IN_MIGRATION();  // LeakyLazyInstance is not destructed.
  }

  base::Lock lock_;  // Protects resolver_generation_.
  int resolver_generation_ = 0;
  friend struct base::LazyInstanceTraitsBase<DnsReloader>;

  // We use thread local storage to identify which ReloadState to interact with.
  base::ThreadLocalOwnedPointer<ReloadState> tls_reload_state_;
};

base::LazyInstance<DnsReloader>::Leaky
    g_dns_reloader = LAZY_INSTANCE_INITIALIZER;

}  // namespace

void EnsureDnsReloaderInit() {
  g_dns_reloader.Pointer();
}

void DnsReloaderMaybeReload() {
  // This routine can be called by any of the DNS worker threads.
  DnsReloader* dns_reloader = g_dns_reloader.Pointer();
  dns_reloader->MaybeReload();
}

}  // namespace net

#else  // !USE_RES_NINIT

namespace net {

void EnsureDnsReloaderInit() {}

void DnsReloaderMaybeReload() {}

}  // namespace net

#endif  // defined(USE_RES_NINIT)
