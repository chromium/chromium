// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "nacl_io/pipe/pipe_node.h"

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <string.h>

#include "nacl_io/ioctl.h"
#include "nacl_io/kernel_handle.h"
#include "nacl_io/pipe/pipe_event_emitter.h"

namespace {
const size_t kDefaultPipeSize = 512 * 1024;
}

namespace nacl_io {

PipeNode::PipeNode(Filesystem* fs)
    : StreamNode(fs), pipe_(new PipeEventEmitter(kDefaultPipeSize)) {
}

PipeEventEmitter* PipeNode::GetEventEmitter() {
  return pipe_.get();
}

Error PipeNode::Read(const HandleAttr& attr,
                     void* buf,
                     size_t count,
                     int* out_bytes) {
  int ms = attr.IsBlocking() ? read_timeout_ : 0;

  EventListenerLock wait(GetEventEmitter());
  Error err = wait.WaitOnEvent(POLLIN, ms);
  if (err == ETIMEDOUT)
    err = EWOULDBLOCK;
  if (err)
    return err;

  return GetEventEmitter()->Read_Locked(static_cast<char*>(buf), count,
                                        out_bytes);
}

Error PipeNode::Write(const HandleAttr& attr,
                      const void* buf,
                      size_t count,
                      int* out_bytes) {
  int ms = attr.IsBlocking() ? write_timeout_ : 0;

  EventListenerLock wait(GetEventEmitter());
  Error err = wait.WaitOnEvent(POLLOUT, ms);
  if (err == ETIMEDOUT)
    err = EWOULDBLOCK;
  if (err)
    return err;

  return GetEventEmitter()->Write_Locked(static_cast<const char*>(buf),
                                         count, out_bytes);
}

}  // namespace nacl_io
