// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBRARIES_NACL_IO_FIFO_INTERFACE_H_
#define LIBRARIES_NACL_IO_FIFO_INTERFACE_H_

#include <stdint.h>
#include <stdlib.h>

namespace nacl_io {

// FIFOInterface
//
// FIFOInterface provides a common interface for Emitters to update their
// signalled state.  FIFOs do not have any internal locking and instead
// reply on a parent (usually an emitter) to lock for them as appropriate.
class FIFOInterface {
 public:
  virtual ~FIFOInterface() {}

  virtual bool IsEmpty() = 0;
  virtual bool IsFull() = 0;
  virtual bool Resize(size_t len) = 0;

  virtual size_t ReadAvailable() = 0;
  virtual size_t WriteAvailable() = 0;
  virtual size_t Read(void* buf, size_t len) = 0;
  virtual size_t Write(const void* buf, size_t len) = 0;
};

}  // namespace nacl_io

#endif  // LIBRARIES_NACL_IO_FIFO_INTERFACE_H_
