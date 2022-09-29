// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/host_resolver_proc.h"

#include <tuple>

#include "base/check.h"
#include "base/memory/scoped_refptr.h"
#include "base/notreached.h"
#include "build/build_config.h"
#include "net/base/address_family.h"
#include "net/base/address_list.h"
#include "net/base/net_errors.h"
#include "net/dns/host_resolver_system_task.h"

#if BUILDFLAG(IS_OPENBSD)
#define AI_ADDRCONFIG 0
#endif

namespace net {

HostResolverProc* HostResolverProc::default_proc_ = nullptr;

HostResolverProc::HostResolverProc(scoped_refptr<HostResolverProc> previous,
                                   bool allow_fallback_to_system_or_default)
    : allow_fallback_to_system_(allow_fallback_to_system_or_default) {
  SetPreviousProc(previous);

  // Implicitly fall-back to the global default procedure.
  if (!previous && allow_fallback_to_system_or_default)
    SetPreviousProc(default_proc_);
}

HostResolverProc::~HostResolverProc() = default;

int HostResolverProc::Resolve(const std::string& host,
                              AddressFamily address_family,
                              HostResolverFlags host_resolver_flags,
                              AddressList* addrlist,
                              int* os_error,
                              handles::NetworkHandle network) {
  if (network == handles::kInvalidNetworkHandle)
    return Resolve(host, address_family, host_resolver_flags, addrlist,
                   os_error);

  NOTIMPLEMENTED();
  return ERR_NOT_IMPLEMENTED;
}

int HostResolverProc::ResolveUsingPrevious(
    const std::string& host,
    AddressFamily address_family,
    HostResolverFlags host_resolver_flags,
    AddressList* addrlist,
    int* os_error) {
  if (previous_proc_.get()) {
    return previous_proc_->Resolve(
        host, address_family, host_resolver_flags, addrlist, os_error);
  }

  // If `allow_fallback_to_system_` is false there is no final fallback. It must
  // be ensured that the Procs can handle any allowed requests. If this check
  // fails while using MockHostResolver or RuleBasedHostResolverProc, it means
  // none of the configured rules matched a host resolution request.
  CHECK(allow_fallback_to_system_);

  // Final fallback is the system resolver.
  return SystemHostResolverCall(host, address_family, host_resolver_flags,
                                addrlist, os_error);
}

void HostResolverProc::SetPreviousProc(scoped_refptr<HostResolverProc> proc) {
  auto current_previous = std::move(previous_proc_);
  // Now that we've guaranteed |this| is the last proc in a chain, we can
  // detect potential cycles using GetLastProc().
  previous_proc_ = (GetLastProc(proc.get()) == this)
                       ? std::move(current_previous)
                       : std::move(proc);
}

void HostResolverProc::SetLastProc(scoped_refptr<HostResolverProc> proc) {
  GetLastProc(this)->SetPreviousProc(std::move(proc));
}

// static
HostResolverProc* HostResolverProc::GetLastProc(HostResolverProc* proc) {
  if (proc == nullptr)
    return nullptr;
  HostResolverProc* last_proc = proc;
  while (last_proc->previous_proc_.get() != nullptr)
    last_proc = last_proc->previous_proc_.get();
  return last_proc;
}

// static
HostResolverProc* HostResolverProc::SetDefault(HostResolverProc* proc) {
  HostResolverProc* old = default_proc_;
  default_proc_ = proc;
  return old;
}

// static
HostResolverProc* HostResolverProc::GetDefault() {
  return default_proc_;
}

}  // namespace net
