// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DNS_HOST_RESOLVER_PROC_H_
#define NET_DNS_HOST_RESOLVER_PROC_H_

#include <string>

#include "base/memory/ref_counted.h"
#include "net/base/address_family.h"
#include "net/base/net_export.h"
#include "net/base/network_handle.h"

namespace net {

class AddressList;

// Interface for a getaddrinfo()-like procedure. This is used by tests
// to control the underlying resolutions in HostResolverManager.
// HostResolverProcs can be chained together; they fallback to the next
// procedure in the chain by calling ResolveUsingPrevious(). Unless
// `allow_fallback_to_system_or_default` is set to false, `default_proc_`
// (set via SetDefault()) is added to the end of the chain and the actual system
// resolver acts as the final fallback after the default proc.
//
// Note that implementations of HostResolverProc *MUST BE THREADSAFE*, since
// the HostResolver implementation using them can be multi-threaded.
class NET_EXPORT HostResolverProc
    : public base::RefCountedThreadSafe<HostResolverProc> {
 public:
  explicit HostResolverProc(scoped_refptr<HostResolverProc> previous,
                            bool allow_fallback_to_system_or_default = true);

  HostResolverProc(const HostResolverProc&) = delete;
  HostResolverProc& operator=(const HostResolverProc&) = delete;

  // Resolves |host| to an address list, restricting the results to addresses
  // in |address_family|. If successful returns OK and fills |addrlist| with
  // a list of socket addresses. Otherwise returns a network error code, and
  // fills |os_error| with a more specific error if it was non-NULL.
  virtual int Resolve(const std::string& host,
                      AddressFamily address_family,
                      HostResolverFlags host_resolver_flags,
                      AddressList* addrlist,
                      int* os_error) = 0;

  // Same as above but requires an additional `network` parameter. Differently
  // from above the lookup will be performed specifically for `network`.
  virtual int Resolve(const std::string& host,
                      AddressFamily address_family,
                      HostResolverFlags host_resolver_flags,
                      AddressList* addrlist,
                      int* os_error,
                      handles::NetworkHandle network);

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
  friend class HostResolverSystemTask;
  friend class MockHostResolverBase;
  friend class ScopedDefaultHostResolverProc;

  // Sets the previous procedure in the chain.  Aborts if this would result in a
  // cycle.
  void SetPreviousProc(scoped_refptr<HostResolverProc> proc);

  // Sets the last procedure in the chain, i.e. appends |proc| to the end of the
  // current chain.  Aborts if this would result in a cycle.
  void SetLastProc(scoped_refptr<HostResolverProc> proc);

  // Returns the last procedure in the chain starting at |proc|.  Will return
  // NULL iff |proc| is NULL.
  static HostResolverProc* GetLastProc(HostResolverProc* proc);

  // Sets the default host resolver procedure that is used by
  // HostResolverManager. This can be used through ScopedDefaultHostResolverProc
  // to set a catch-all DNS block in unit-tests (individual tests should use
  // MockHostResolver to prevent hitting the network).
  static HostResolverProc* SetDefault(HostResolverProc* proc);
  static HostResolverProc* GetDefault();

  bool allow_fallback_to_system_;
  scoped_refptr<HostResolverProc> previous_proc_;
  static HostResolverProc* default_proc_;
};

}  // namespace net

#endif  // NET_DNS_HOST_RESOLVER_PROC_H_
