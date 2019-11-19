// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DNS_HOST_RESOLVER_PROC_H_
#define NET_DNS_HOST_RESOLVER_PROC_H_

#include <string>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/time/time.h"
#include "net/base/address_family.h"
#include "net/base/net_export.h"

namespace net {

class AddressList;

// Interface for a getaddrinfo()-like procedure. This is used by unit-tests
// to control the underlying resolutions in HostResolverManager.
// HostResolverProcs can be chained together; they fallback to the next
// procedure in the chain by calling ResolveUsingPrevious().
//
// Note that implementations of HostResolverProc *MUST BE THREADSAFE*, since
// the HostResolver implementation using them can be multi-threaded.
class NET_EXPORT HostResolverProc
    : public base::RefCountedThreadSafe<HostResolverProc> {
 public:
  explicit HostResolverProc(HostResolverProc* previous);

  // Resolves |host| to an address list, restricting the results to addresses
  // in |address_family|. If successful returns OK and fills |addrlist| with
  // a list of socket addresses. Otherwise returns a network error code, and
  // fills |os_error| with a more specific error if it was non-NULL.
  virtual int Resolve(const std::string& host,
                      AddressFamily address_family,
                      HostResolverFlags host_resolver_flags,
                      AddressList* addrlist,
                      int* os_error) = 0;

 protected:
  friend class base::RefCountedThreadSafe<HostResolverProc>;

  virtual ~HostResolverProc();

  // Asks the fallback procedure (if set) to do the resolve.
  int ResolveUsingPrevious(const std::string& host,
                           AddressFamily address_family,
                           HostResolverFlags host_resolver_flags,
                           AddressList* addrlist,
                           int* os_error);

 private:
  friend class HostResolverManager;
  friend class MockHostResolverBase;
  friend class ScopedDefaultHostResolverProc;

  // Sets the previous procedure in the chain.  Aborts if this would result in a
  // cycle.
  void SetPreviousProc(HostResolverProc* proc);

  // Sets the last procedure in the chain, i.e. appends |proc| to the end of the
  // current chain.  Aborts if this would result in a cycle.
  void SetLastProc(HostResolverProc* proc);

  // Returns the last procedure in the chain starting at |proc|.  Will return
  // NULL iff |proc| is NULL.
  static HostResolverProc* GetLastProc(HostResolverProc* proc);

  // Sets the default host resolver procedure that is used by
  // HostResolverManager. This can be used through ScopedDefaultHostResolverProc
  // to set a catch-all DNS block in unit-tests (individual tests should use
  // MockHostResolver to prevent hitting the network).
  static HostResolverProc* SetDefault(HostResolverProc* proc);
  static HostResolverProc* GetDefault();

  scoped_refptr<HostResolverProc> previous_proc_;
  static HostResolverProc* default_proc_;

  DISALLOW_COPY_AND_ASSIGN(HostResolverProc);
};

// Resolves |host| to an address list, using the system's default host resolver.
// (i.e. this calls out to getaddrinfo()). If successful returns OK and fills
// |addrlist| with a list of socket addresses. Otherwise returns a
// network error code, and fills |os_error| with a more specific error if it
// was non-NULL.
NET_EXPORT_PRIVATE int SystemHostResolverCall(
    const std::string& host,
    AddressFamily address_family,
    HostResolverFlags host_resolver_flags,
    AddressList* addrlist,
    int* os_error);

// Wraps call to SystemHostResolverCall as an instance of HostResolverProc.
class NET_EXPORT_PRIVATE SystemHostResolverProc : public HostResolverProc {
 public:
  SystemHostResolverProc();
  int Resolve(const std::string& hostname,
              AddressFamily address_family,
              HostResolverFlags host_resolver_flags,
              AddressList* addr_list,
              int* os_error) override;

 protected:
  ~SystemHostResolverProc() override;

  DISALLOW_COPY_AND_ASSIGN(SystemHostResolverProc);
};

// Parameters for customizing HostResolverProc behavior in HostResolvers.
//
// |resolver_proc| is used to perform the actual resolves; it must be
// thread-safe since it may be run from multiple worker threads. If
// |resolver_proc| is NULL then the default host resolver procedure is
// used (which is SystemHostResolverProc except if overridden).
//
// For each attempt, we could start another attempt if host is not resolved
// within |unresponsive_delay| time. We keep attempting to resolve the host
// for |max_retry_attempts|. For every retry attempt, we grow the
// |unresponsive_delay| by the |retry_factor| amount (that is retry interval
// is multiplied by the retry factor each time). Once we have retried
// |max_retry_attempts|, we give up on additional attempts.
//
struct NET_EXPORT_PRIVATE ProcTaskParams {
  // Default delay between calls to the system resolver for the same hostname.
  // (Can be overridden by field trial.)
  static const base::TimeDelta kDnsDefaultUnresponsiveDelay;

  // Sets up defaults.
  ProcTaskParams(HostResolverProc* resolver_proc, size_t max_retry_attempts);

  ProcTaskParams(const ProcTaskParams& other);

  ~ProcTaskParams();

  // The procedure to use for resolving host names. This will be NULL, except
  // in the case of unit-tests which inject custom host resolving behaviors.
  scoped_refptr<HostResolverProc> resolver_proc;

  // Maximum number retry attempts to resolve the hostname.
  // Pass HostResolver::Options::kDefaultRetryAttempts to choose a default
  // value.
  size_t max_retry_attempts;

  // This is the limit after which we make another attempt to resolve the host
  // if the worker thread has not responded yet.
  base::TimeDelta unresponsive_delay;

  // Factor to grow |unresponsive_delay| when we re-re-try.
  uint32_t retry_factor;
};

}  // namespace net

#endif  // NET_DNS_HOST_RESOLVER_PROC_H_
