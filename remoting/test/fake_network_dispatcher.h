// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_TEST_FAKE_NETWORK_DISPATCHER_H_
#define REMOTING_TEST_FAKE_NETWORK_DISPATCHER_H_

#include <map>

#include "base/compiler_specific.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/lock.h"
#include "third_party/webrtc/rtc_base/ip_address.h"
#include "third_party/webrtc/rtc_base/socket_address.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace net {
class IOBuffer;
}  // namespace net

namespace remoting {

class FakeNetworkDispatcher
    : public base::RefCountedThreadSafe<FakeNetworkDispatcher> {
 public:
  class Node {
   public:
    virtual ~Node() {}

    // Return thread on which ReceivePacket() should be called.
    virtual const scoped_refptr<base::SingleThreadTaskRunner>& GetThread()
        const = 0;
    virtual const rtc::IPAddress& GetAddress() const = 0;

    // Deliver a packet sent by a different node.
    virtual void ReceivePacket(const rtc::SocketAddress& from,
                               const rtc::SocketAddress& to,
                               const scoped_refptr<net::IOBuffer>& data,
                               int data_size) = 0;
  };

  FakeNetworkDispatcher();

  FakeNetworkDispatcher(const FakeNetworkDispatcher&) = delete;
  FakeNetworkDispatcher& operator=(const FakeNetworkDispatcher&) = delete;

  rtc::IPAddress AllocateAddress();

  // Must be called on the thread that the |node| works on.
  void AddNode(Node* node);
  void RemoveNode(Node* node);

  void DeliverPacket(const rtc::SocketAddress& from,
                     const rtc::SocketAddress& to,
                     const scoped_refptr<net::IOBuffer>& data,
                     int data_size);

 private:
  typedef std::map<rtc::IPAddress, raw_ptr<Node, CtnExperimental>> NodesMap;

  friend class base::RefCountedThreadSafe<FakeNetworkDispatcher>;
  virtual ~FakeNetworkDispatcher();

  NodesMap nodes_;
  base::Lock nodes_lock_;

  // A counter used to allocate unique addresses in AllocateAddress().
  int allocated_address_;
};

}  // namespace remoting

#endif  // REMOTING_TEST_FAKE_NETWORK_DISPATCHER_H_
