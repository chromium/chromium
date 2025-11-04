// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/test/fake_network_dispatcher.h"

#include <stddef.h>

#include "base/containers/contains.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/numerics/byte_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "net/base/io_buffer.h"

namespace remoting {

FakeNetworkDispatcher::FakeNetworkDispatcher() : allocated_address_(0) {}

FakeNetworkDispatcher::~FakeNetworkDispatcher() {
  CHECK(nodes_.empty());
}

webrtc::IPAddress FakeNetworkDispatcher::AllocateAddress() {
  in6_addr addr{};

  // fc00::/7 is reserved for unique local addresses.
  addr.s6_addr[0] = 0xfc;

  // Copy |allocated_address_| to the end of |addr|.
  ++allocated_address_;
  base::span<uint8_t>(addr.s6_addr)
      .last(sizeof(allocated_address_))
      .copy_from(base::U32ToBigEndian(allocated_address_));

  return webrtc::IPAddress(addr);
}

void FakeNetworkDispatcher::AddNode(Node* node) {
  DCHECK(node->GetThread()->BelongsToCurrentThread());

  base::AutoLock auto_lock(nodes_lock_);
  DCHECK(!base::Contains(nodes_, node->GetAddress()));
  nodes_[node->GetAddress()] = node;
}

void FakeNetworkDispatcher::RemoveNode(Node* node) {
  DCHECK(node->GetThread()->BelongsToCurrentThread());

  base::AutoLock auto_lock(nodes_lock_);
  DCHECK(nodes_[node->GetAddress()] == node);
  nodes_.erase(node->GetAddress());
}

void FakeNetworkDispatcher::DeliverPacket(
    const webrtc::SocketAddress& from,
    const webrtc::SocketAddress& to,
    const scoped_refptr<net::IOBuffer>& data,
    int data_size) {
  Node* node;
  {
    base::AutoLock auto_lock(nodes_lock_);

    auto node_it = nodes_.find(to.ipaddr());
    if (node_it == nodes_.end()) {
      LOG(ERROR) << "Tried to deliver packet to unknown target: "
                 << to.ToString();
      return;
    }

    node = node_it->second;

    // Check if |node| belongs to a different thread and post a task in that
    // case.
    scoped_refptr<base::SingleThreadTaskRunner> task_runner = node->GetThread();
    if (!task_runner->BelongsToCurrentThread()) {
      task_runner->PostTask(
          FROM_HERE, base::BindOnce(&FakeNetworkDispatcher::DeliverPacket, this,
                                    from, to, data, data_size));
      return;
    }
  }

  // Call ReceivePacket() without lock held. It's safe because at this point we
  // know that |node| belongs to the current thread.
  node->ReceivePacket(from, to, data, data_size);
}

}  // namespace remoting
