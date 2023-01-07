// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "nacl_io/devfs/jspipe_node.h"

#include <cstring>

#include "nacl_io/devfs/dev_fs.h"
#include "nacl_io/error.h"
#include "nacl_io/ioctl.h"
#include "nacl_io/kernel_handle.h"
#include "nacl_io/log.h"
#include "nacl_io/pepper_interface.h"

#define TRACE(format, ...) LOG_TRACE("jspipe: " format, ##__VA_ARGS__)
#define ERROR(format, ...) LOG_TRACE("jspipe: " format, ##__VA_ARGS__)

namespace {
const size_t kPostMessageBufferSize = 512 * 1024;
}

namespace nacl_io {

JSPipeNode::JSPipeNode(Filesystem* filesystem)
    : Node(filesystem),
      pipe_(new JSPipeEventEmitter(filesystem_->ppapi(),
                                   kPostMessageBufferSize)) {
}

JSPipeEventEmitter* JSPipeNode::GetEventEmitter() {
  return pipe_.get();
}

Error JSPipeNode::Read(const HandleAttr& attr,
                       void* buf,
                       size_t count,
                       int* out_bytes) {
  int ms = attr.IsBlocking() ? -1 : 0;

  EventListenerLock wait(GetEventEmitter());
  Error err = wait.WaitOnEvent(POLLIN, ms);
  if (err == ETIMEDOUT)
    err = EWOULDBLOCK;
  if (err)
    return err;

  return GetEventEmitter()->Read_Locked(static_cast<char*>(buf), count,
                                        out_bytes);
}

Error JSPipeNode::Write(const HandleAttr& attr,
                        const void* buf,
                        size_t count,
                        int* out_bytes) {
  int ms = attr.IsBlocking() ? -1 : 0;
  TRACE("write timeout=%d", ms);

  EventListenerLock wait(GetEventEmitter());
  Error err = wait.WaitOnEvent(POLLOUT, ms);
  if (err == ETIMEDOUT)
    err = EWOULDBLOCK;
  if (err)
    return err;

  return GetEventEmitter()->Write_Locked(static_cast<const char*>(buf),
                                         count, out_bytes);
}

Error JSPipeNode::VIoctl(int request, va_list args) {
  AUTO_LOCK(node_lock_);

  switch (request) {
    case NACL_IOC_PIPE_SETNAME: {
      const char* new_name = va_arg(args, char*);
      return GetEventEmitter()->SetName(new_name);
    }
    case NACL_IOC_PIPE_GETISPACE: {
      int* space = va_arg(args, int*);
      *space = GetEventEmitter()->GetISpace();
      return 0;
    }
    case NACL_IOC_PIPE_GETOSPACE: {
      int* space = va_arg(args, int*);
      *space = GetEventEmitter()->GetOSpace();
      return 0;
    }
    case NACL_IOC_HANDLEMESSAGE: {
      struct PP_Var* message = va_arg(args, struct PP_Var*);
      return GetEventEmitter()->HandleJSMessage(*message);
    }
    default:
      TRACE("unknown ioctl: %#x", request);
      break;
  }

  return EINVAL;
}

}  // namespace nacl_io
