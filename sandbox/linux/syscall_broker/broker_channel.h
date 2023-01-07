// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_LINUX_SYSCALL_BROKER_BROKER_CHANNEL_H_
#define SANDBOX_LINUX_SYSCALL_BROKER_BROKER_CHANNEL_H_

#include "base/files/scoped_file.h"
#include "sandbox/sandbox_export.h"

namespace sandbox {

namespace syscall_broker {

// A small class to create a pipe-like communication channel. It is based on a
// SOCK_SEQPACKET unix socket, which is connection-based and guaranteed to
// preserve message boundaries.
class SANDBOX_EXPORT BrokerChannel {
 public:
  typedef base::ScopedFD EndPoint;

  BrokerChannel() = delete;
  BrokerChannel(const BrokerChannel&) = delete;
  BrokerChannel& operator=(const BrokerChannel&) = delete;

  static void CreatePair(EndPoint* reader, EndPoint* writer);
};

}  // namespace syscall_broker

}  // namespace sandbox

#endif  // SANDBOX_LINUX_SYSCALL_BROKER_BROKER_CHANNEL_H_
