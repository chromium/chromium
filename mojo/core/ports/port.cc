// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/core/ports/port.h"

#ifdef OS_MAC
extern "C" void V8RecordReplayRegisterPointer(void* ptr);
extern "C" void V8RecordReplayUnregisterPointer(void* ptr);
#else
static void V8RecordReplayRegisterPointer(void* ptr) {}
static void V8RecordReplayUnregisterPointer(void* ptr) {}
#endif

namespace mojo {
namespace core {
namespace ports {

Port::Port(uint64_t next_sequence_num_to_send,
           uint64_t next_sequence_num_to_receive)
    : state(kUninitialized),
      next_sequence_num_to_send(next_sequence_num_to_send),
      last_sequence_num_acknowledged(next_sequence_num_to_send - 1),
      sequence_num_acknowledge_interval(0),
      last_sequence_num_to_receive(0),
      sequence_num_to_acknowledge(0),
      message_queue(next_sequence_num_to_receive),
      remove_proxy_on_last_message(false),
      peer_closed(false),
      peer_lost_unexpectedly(false),
      lock_("Port.lock_") {
  // Registering new ports is needed for sorting, see port_locker.cc
  V8RecordReplayRegisterPointer(this);
}

Port::~Port() {
  V8RecordReplayUnregisterPointer(this);
}

}  // namespace ports
}  // namespace core
}  // namespace mojo
