// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "mojo/core/ports/port_locker.h"

#include <algorithm>

#include "base/dcheck_is_on.h"
#include "mojo/core/ports/port.h"

#if DCHECK_IS_ON()
#include "base/check_op.h"
#endif

namespace mojo {
namespace core {
namespace ports {

#if DCHECK_IS_ON()
namespace {

constinit thread_local const PortLocker* port_locker = nullptr;

}  // namespace
#endif

PortLocker::PortLocker(const PortRef** port_refs, size_t num_ports)
    :
#if DCHECK_IS_ON()
      resetter_(&port_locker, this, nullptr),
#endif
      port_refs_(port_refs),
      num_ports_(num_ports) {
  // Sort the ports by address to lock them in a globally consistent order.
  std::sort(
      port_refs_, port_refs_ + num_ports_,
      [](const PortRef* a, const PortRef* b) { return a->port() < b->port(); });

  for (size_t i = 0; i < num_ports_; ++i) {
    // TODO(crbug.com/40522227): Remove this CHECK.
    CHECK(port_refs_[i]->port());
    port_refs_[i]->port()->lock_.Acquire();
  }
}

PortLocker::~PortLocker() {
  for (size_t i = 0; i < num_ports_; ++i)
    port_refs_[i]->port()->lock_.Release();

#if DCHECK_IS_ON()
  DCHECK_EQ(port_locker, this);
#endif
}

#if DCHECK_IS_ON()
// static
void PortLocker::AssertNoPortsLockedOnCurrentThread() {
  DCHECK_EQ(port_locker, nullptr);
}
#endif

SinglePortLocker::SinglePortLocker(const PortRef* port_ref)
    : port_ref_(port_ref), locker_(&port_ref_, 1) {}

SinglePortLocker::~SinglePortLocker() = default;

}  // namespace ports
}  // namespace core
}  // namespace mojo
