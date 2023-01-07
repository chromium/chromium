// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBRARIES_NACL_IO_FIFO_CHAR_H_
#define LIBRARIES_NACL_IO_FIFO_CHAR_H_

#include <vector>

#include "nacl_io/fifo_interface.h"

#include "sdk_util/macros.h"

namespace nacl_io {

// FIFOChar
//
// A FIFOChar is a circular buffer, signalling FULL and EMPTY as appropriate.
class FIFOChar : public FIFOInterface {
 public:
  explicit FIFOChar(size_t size);

  FIFOChar(const FIFOChar&) = delete;
  FIFOChar& operator=(const FIFOChar&) = delete;

  virtual ~FIFOChar();

  virtual bool IsEmpty();
  virtual bool IsFull();
  virtual bool Resize(size_t len);

  virtual size_t ReadAvailable();
  virtual size_t WriteAvailable();

  // Reads out no more than the requested len without updating the tail.
  // Returns actual amount read.
  size_t Peek(void* buf, size_t len);

  // Reads out the data making room in the FIFO.  Returns actual amount
  // read.
  virtual size_t Read(void* buf, size_t len);

  // Writes into the FIFO no more than len bytes, returns actual amount
  // written.
  virtual size_t Write(const void* buf, size_t len);

 private:
  char* buffer_;
  size_t size_;   // Size of the FIFO
  size_t avail_;  // How much data is currently available
  size_t tail_;   // Next read location
};

}  // namespace nacl_io

#endif  // LIBRARIES_NACL_IO_FIFO_CHAR_H_
