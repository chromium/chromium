// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/port_allocator_factory.h"

#include "third_party/webrtc/p2p/base/port_allocator.h"

namespace remoting::protocol {

PortAllocatorFactory::CreatePortAllocatorResult::CreatePortAllocatorResult() =
    default;
PortAllocatorFactory::CreatePortAllocatorResult::CreatePortAllocatorResult(
    CreatePortAllocatorResult&&) = default;
PortAllocatorFactory::CreatePortAllocatorResult::~CreatePortAllocatorResult() =
    default;

}  // namespace remoting::protocol
