// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/core/ports/port_locker.h"

#include <algorithm>

#include "mojo/core/ports/port.h"

#if DCHECK_IS_ON()
#include "base/threading/thread_local.h"
#endif

namespace mojo {
namespace core {
namespace ports {

namespace {

#if DCHECK_IS_ON()
void UpdateTLS(PortLocker* old_locker, PortLocker* new_locker) {
  // Sanity check when DCHECK is on to make sure there is only ever one
  // PortLocker extant on the current thread.
  static auto* tls = new base::ThreadLocalPointer<PortLocker>();
  DCHECK_EQ(old_locker, tls->Get());
  tls->Set(new_locker);
}
#endif

}  // namespace

PortLocker::PortLocker(const PortRef** port_refs, size_t num_ports)
    : port_refs_(port_refs), num_ports_(num_ports) {
#if DCHECK_IS_ON()
  UpdateTLS(nullptr, this);
#endif

  // Sort the ports by address to lock them in a globally consistent order.
  std::sort(
      port_refs_, port_refs_ + num_ports_,
      [](const PortRef* a, const PortRef* b) { return a->port() < b->port(); });
  for (size_t i = 0; i < num_ports_; ++i) {
    // TODO(crbug.com/725605): Remove this CHECK.
    CHECK(port_refs_[i]->port());
    port_refs_[i]->port()->lock_.Acquire();
  }
}

PortLocker::~PortLocker() {
  for (size_t i = 0; i < num_ports_; ++i)
    port_refs_[i]->port()->lock_.Release();

#if DCHECK_IS_ON()
  UpdateTLS(this, nullptr);
#endif
}

#if DCHECK_IS_ON()
// static
void PortLocker::AssertNoPortsLockedOnCurrentThread() {
  // Forces a DCHECK if the TLS PortLocker is anything other than null.
  UpdateTLS(nullptr, nullptr);
}
#endif

SinglePortLocker::SinglePortLocker(const PortRef* port_ref)
    : port_ref_(port_ref), locker_(&port_ref_, 1) {}

SinglePortLocker::~SinglePortLocker() = default;

}  // namespace ports
}  // namespace core
}  // namespace mojo
