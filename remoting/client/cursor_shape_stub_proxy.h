// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_CURSOR_SHAPE_STUB_PROXY_H_
#define REMOTING_CLIENT_CURSOR_SHAPE_STUB_PROXY_H_

#include "base/memory/weak_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "remoting/protocol/cursor_shape_stub.h"

namespace remoting {

// A helper class to forward CursorShapeStub function calls from one thread to
// another.
class CursorShapeStubProxy : public protocol::CursorShapeStub {
 public:
  // Function calls will be forwarded to |stub| on the thread of |task_runner|.
  CursorShapeStubProxy(
      base::WeakPtr<protocol::CursorShapeStub> stub,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);

  CursorShapeStubProxy(const CursorShapeStubProxy&) = delete;
  CursorShapeStubProxy& operator=(const CursorShapeStubProxy&) = delete;

  ~CursorShapeStubProxy() override;

  // CursorShapeStub override.
  void SetCursorShape(const protocol::CursorShapeInfo& cursor_shape) override;

 private:
  base::WeakPtr<protocol::CursorShapeStub> stub_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
};

}  // namespace remoting

#endif  // REMOTING_CLIENT_CURSOR_SHAPE_STUB_PROXY_H_
