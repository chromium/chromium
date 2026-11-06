// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_CORE_BROKER_MESSAGES_H_
#define MOJO_CORE_BROKER_MESSAGES_H_

#include <stdint.h>

#include <type_traits>

namespace mojo::core {

// The wire format used between a Broker and its BrokerHost over their dedicated
// socket. Out-of-tree Mojo peers also get a Broker, but never install the
// shared memory hooks (see InstallMojoIpczBaseSharedMemoryHooks()) that send
// requests through it. So every process that sends requests runs the same build
// as its BrokerHost, and this format needs no versioning.
//
// The protocol is strictly request/response: the Broker sends one
// BrokerBufferRequest and blocks until it has read the matching
// BrokerBufferResponse before sending another request. Values are fixed-size
// and copied directly to and from the socket as raw bytes.

// Asks the BrokerHost for a new writable shared memory region of `num_bytes`.
// No file descriptors are attached.
struct BrokerBufferRequest {
  uint32_t num_bytes;
};

// The BrokerHost's reply to a BrokerBufferRequest. Stream sockets require at
// least one data byte to carry SCM_RIGHTS descriptors and to distinguish a
// reply from end-of-stream.
enum class BrokerBufferResponse : uint8_t {
  // Allocation failed. No file descriptors are attached.
  kFailure = 0,
  // Region created. Accompanied by two file descriptors: the region's writable
  // descriptor followed by its read-only descriptor.
  kSuccess = 1,
};

static_assert(std::has_unique_object_representations_v<BrokerBufferRequest>);
static_assert(std::has_unique_object_representations_v<BrokerBufferResponse>);

}  // namespace mojo::core

#endif  // MOJO_CORE_BROKER_MESSAGES_H_
