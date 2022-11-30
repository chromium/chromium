// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "nacl_io/stream/stream_node.h"

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <string.h>

#include "nacl_io/ioctl.h"
#include "nacl_io/stream/stream_fs.h"
#include "sdk_util/atomicops.h"

namespace nacl_io {

StreamNode::StreamNode(Filesystem* fs)
    : Node(fs), read_timeout_(-1), write_timeout_(-1), stream_state_flags_(0) {
}

Error StreamNode::Init(int open_flags) {
  Node::Init(open_flags);
  if (open_flags & O_NONBLOCK)
    SetStreamFlags(SSF_NON_BLOCK);

  return 0;
}

void StreamNode::SetStreamFlags(uint32_t bits) {
  sdk_util::AtomicOrFetch(&stream_state_flags_, bits);
}

void StreamNode::ClearStreamFlags(uint32_t bits) {
  sdk_util::AtomicAndFetch(&stream_state_flags_, ~bits);
}

uint32_t StreamNode::GetStreamFlags() {
  return stream_state_flags_;
}

bool StreamNode::TestStreamFlags(uint32_t bits) {
  return (stream_state_flags_ & bits) == bits;
}

void StreamNode::QueueInput() {
}
void StreamNode::QueueOutput() {
}

StreamFs* StreamNode::stream() {
  return static_cast<StreamFs*>(filesystem_);
}

}  // namespace nacl_io
