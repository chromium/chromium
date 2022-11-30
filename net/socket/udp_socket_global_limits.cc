// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <limits>

#include "base/atomic_ref_count.h"
#include "base/no_destructor.h"
#include "net/base/features.h"
#include "net/socket/udp_socket_global_limits.h"

namespace net {

namespace {

// Threadsafe singleton for tracking the process-wide count of UDP sockets.
class GlobalUDPSocketCounts {
 public:
  GlobalUDPSocketCounts() = default;

  ~GlobalUDPSocketCounts() = delete;

  static GlobalUDPSocketCounts& Get() {
    static base::NoDestructor<GlobalUDPSocketCounts> singleton;
    return *singleton;
  }

  [[nodiscard]] bool TryAcquireSocket() {
    int previous = count_.Increment(1);
    if (previous >= GetMax()) {
      count_.Increment(-1);
      return false;
    }

    return true;
  }

  int GetMax() {
    if (base::FeatureList::IsEnabled(features::kLimitOpenUDPSockets))
      return features::kLimitOpenUDPSocketsMax.Get();

    return std::numeric_limits<int>::max();
  }

  void ReleaseSocket() { count_.Increment(-1); }

  int GetCountForTesting() { return count_.SubtleRefCountForDebug(); }

 private:
  base::AtomicRefCount count_{0};
};

}  // namespace

OwnedUDPSocketCount::OwnedUDPSocketCount() : OwnedUDPSocketCount(true) {}

OwnedUDPSocketCount::OwnedUDPSocketCount(OwnedUDPSocketCount&& other) {
  *this = std::move(other);
}

OwnedUDPSocketCount& OwnedUDPSocketCount::operator=(
    OwnedUDPSocketCount&& other) {
  Reset();
  empty_ = other.empty_;
  other.empty_ = true;
  return *this;
}

OwnedUDPSocketCount::~OwnedUDPSocketCount() {
  Reset();
}

void OwnedUDPSocketCount::Reset() {
  if (!empty_) {
    GlobalUDPSocketCounts::Get().ReleaseSocket();
    empty_ = true;
  }
}

OwnedUDPSocketCount::OwnedUDPSocketCount(bool empty) : empty_(empty) {}

OwnedUDPSocketCount TryAcquireGlobalUDPSocketCount() {
  bool success = GlobalUDPSocketCounts::Get().TryAcquireSocket();
  return OwnedUDPSocketCount(!success);
}

int GetGlobalUDPSocketCountForTesting() {
  return GlobalUDPSocketCounts::Get().GetCountForTesting();
}

}  // namespace net
