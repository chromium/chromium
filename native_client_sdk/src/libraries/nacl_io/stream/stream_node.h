// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBRARIES_NACL_IO_STREAM_STREAM_NODE_H_
#define LIBRARIES_NACL_IO_STREAM_STREAM_NODE_H_

#include <map>
#include <string>

#include "nacl_io/node.h"
#include "nacl_io/pipe/pipe_event_emitter.h"
#include "sdk_util/atomicops.h"

namespace nacl_io {

class StreamNode;
class StreamFs;

typedef sdk_util::ScopedRef<StreamNode> ScopedStreamNode;

enum StreamStateFlags {
  SSF_CONNECTING = 0x0001,
  SSF_SENDING = 0x0002,
  SSF_RECVING = 0x0004,
  SSF_CLOSING = 0x0008,
  SSF_LISTENING = 0x000f,
  SSF_CAN_SEND = 0x0020,
  SSF_CAN_RECV = 0x0040,
  SSF_CAN_ACCEPT = 0x0080,
  SSF_CAN_CONNECT = 0x00f0,
  SSF_NON_BLOCK = 0x1000,
  SSF_ERROR = 0x4000,
  SSF_CLOSED = 0x8000,
  SSF_RECV_ENDOFSTREAM = 0x10000,
};

class StreamNode : public Node {
 public:
  explicit StreamNode(Filesystem* fs);

  virtual Error Init(int open_flags);

  // Attempts to pump input and output
  virtual void QueueInput();
  virtual void QueueOutput();

  void SetStreamFlags(uint32_t bits);
  void ClearStreamFlags(uint32_t bits);
  uint32_t GetStreamFlags();
  bool TestStreamFlags(uint32_t bits);

  StreamFs* stream();

 protected:
  int read_timeout_;
  int write_timeout_;

 private:
  sdk_util::Atomic32 stream_state_flags_;
};

}  // namespace nacl_io

#endif  // LIBRARIES_NACL_IO_STREAM_STREAM_NODE_H_
