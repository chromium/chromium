// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBRARIES_NACL_IO_DEVFS_TTY_NODE_H_
#define LIBRARIES_NACL_IO_DEVFS_TTY_NODE_H_

#include <poll.h>
#include <pthread.h>

#include <deque>

#include <ppapi/c/pp_var.h>

#include "nacl_io/char_node.h"
#include "nacl_io/ioctl.h"
#include "nacl_io/ostermios.h"

namespace nacl_io {

class TtyNode : public CharNode {
 public:
  explicit TtyNode(Filesystem* filesystem);

  virtual EventEmitter* GetEventEmitter();

  virtual Error VIoctl(int request, va_list args);

  virtual Error Read(const HandleAttr& attr,
                     void* buf,
                     size_t count,
                     int* out_bytes);

  virtual Error Write(const HandleAttr& attr,
                      const void* buf,
                      size_t count,
                      int* out_bytes);

  virtual Error Tcgetattr(struct termios* termios_p);
  virtual Error Tcsetattr(int optional_actions,
                          const struct termios* termios_p);

  virtual Error Isatty() { return 0; }

 private:
  ScopedEventEmitter emitter_;

  Error ProcessInput(PP_Var var);
  Error ProcessInput(const char* buffer, size_t num_bytes);
  Error Echo(const char* string, int count);
  void InitTermios();

  std::deque<char> input_buffer_;
  struct termios termios_;

  /// Current height of terminal in rows.  Set via ioctl(2).
  int rows_;
  /// Current width of terminal in columns.  Set via ioctl(2).
  int cols_;

  // Output handler for TTY.  This is set via ioctl(2).
  struct tioc_nacl_output output_handler_;
  // Lock to protect output_handler_.  This lock gets acquired whenever
  // output_handler_ is used or set.
  sdk_util::SimpleLock output_lock_;
};

}  // namespace nacl_io

#endif  // LIBRARIES_NACL_IO_DEVFS_TTY_NODE_H_
