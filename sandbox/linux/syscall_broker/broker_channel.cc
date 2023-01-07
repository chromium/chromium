// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/linux/syscall_broker/broker_channel.h"

#include <sys/socket.h>
#include <sys/types.h>

#include "base/check.h"

namespace sandbox {

namespace syscall_broker {

// static
void BrokerChannel::CreatePair(EndPoint* reader, EndPoint* writer) {
  DCHECK(reader);
  DCHECK(writer);
  int socket_pair[2];
  // Use SOCK_SEQPACKET, to preserve message boundaries but we also want to be
  // notified (recvmsg should return and not block) when the connection has
  // been broken which could mean that the other end has been closed.
  PCHECK(0 == socketpair(AF_UNIX, SOCK_SEQPACKET, 0, socket_pair));

  reader->reset(socket_pair[0]);
  PCHECK(0 == shutdown(reader->get(), SHUT_WR));

  writer->reset(socket_pair[1]);
  PCHECK(0 == shutdown(writer->get(), SHUT_RD));
}

}  // namespace syscall_broker

}  // namespace sandbox
