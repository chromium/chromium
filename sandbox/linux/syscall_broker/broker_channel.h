// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_LINUX_SYSCALL_BROKER_BROKER_CHANNEL_H_
#define SANDBOX_LINUX_SYSCALL_BROKER_BROKER_CHANNEL_H_

#include "base/files/scoped_file.h"
#include "base/macros.h"
#include "sandbox/sandbox_export.h"

namespace sandbox {

namespace syscall_broker {

// A small class to create a pipe-like communication channel. It is based on a
// SOCK_SEQPACKET unix socket, which is connection-based and guaranteed to
// preserve message boundaries.
class SANDBOX_EXPORT BrokerChannel {
 public:
  typedef base::ScopedFD EndPoint;
  static void CreatePair(EndPoint* reader, EndPoint* writer);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(BrokerChannel);
};

}  // namespace syscall_broker

}  // namespace sandbox

#endif  // SANDBOX_LINUX_SYSCALL_BROKER_BROKER_CHANNEL_H_
