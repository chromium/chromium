// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBRARIES_NACL_IO_DEVFS_JSPIPE_NODE_H_
#define LIBRARIES_NACL_IO_DEVFS_JSPIPE_NODE_H_

#include <ppapi/c/pp_var.h>
#include <string>

#include "nacl_io/devfs/jspipe_event_emitter.h"
#include "nacl_io/stream/stream_node.h"

namespace nacl_io {

/**
 * JSPipeNode represents a two-way channel for communicating with JavaScript
 * via calls to PostMessage.  In order to use these some amount of logic on
 * the JavaScript side is also required.  The protocol to the communication
 * looks the same in both directions and consists of two message types:
 * 'write' and 'ack'.
 * The messages are formated as JavaScript dictionary objects and take the
 * following form:
 * {
 *   pipe: <pipe_name>,
 *   operation: <operation_name>,
 *   payload: <operations_payload>
 * }
 * The payload for 'write' message is a ArrayBuffer containing binary data.
 * The payload for 'ack' messages is the total number of bytes received at
 * the other end.
 * For example: { pipe: 'jspipe1', operation: 'ack', payload: 234 }
 *
 * Messages coming from JavaScript must be delivered using the
 * NACL_IOC_HANDLEMESSAGE ioctl on the file handle.
 */
class JSPipeNode : public Node {
 public:
  explicit JSPipeNode(Filesystem* filesystem);

  virtual void Destroy() {
    LOG_TRACE("JSPipeNode: Destroy");
  }

  virtual JSPipeEventEmitter* GetEventEmitter();

  virtual Error VIoctl(int request, va_list args);

  virtual Error Read(const HandleAttr& attr,
                     void* buf,
                     size_t count,
                     int* out_bytes);
  virtual Error Write(const HandleAttr& attr,
                      const void* buf,
                      size_t count,
                      int* out_bytes);

 protected:
  Error SendAck();

  ScopedJSPipeEventEmitter pipe_;
};

}  // namespace nacl_io

#endif  // LIBRARIES_NACL_IO_DEVFS_JSPIPE_NODE_H_
