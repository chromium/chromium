// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBRARIES_NACL_IO_PIPE_PIPE_NODE_H_
#define LIBRARIES_NACL_IO_PIPE_PIPE_NODE_H_

#include <map>
#include <string>

#include "nacl_io/pipe/pipe_event_emitter.h"
#include "nacl_io/stream/stream_node.h"

namespace nacl_io {

class PipeNode : public StreamNode {
 public:
  explicit PipeNode(Filesystem* fs);

  virtual PipeEventEmitter* GetEventEmitter();
  virtual Error Read(const HandleAttr& attr,
                     void* buf,
                     size_t count,
                     int* out_bytes);
  virtual Error Write(const HandleAttr& attr,
                      const void* buf,
                      size_t count,
                      int* out_bytes);

 protected:
  ScopedPipeEventEmitter pipe_;

  friend class KernelProxy;
  friend class StreamFs;
};

}  // namespace nacl_io

#endif  // LIBRARIES_NACL_IO_PIPE_PIPE_NODE_H_
