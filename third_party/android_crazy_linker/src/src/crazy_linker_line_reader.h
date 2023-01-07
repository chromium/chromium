// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRAZY_LINKER_LINE_READER_H
#define CRAZY_LINKER_LINE_READER_H

#include <string.h>

#include "crazy_linker_system.h"

namespace crazy {

// A class used to read text files line-by-line.
// Usage:
//    LineReader reader("/path/to/file");
//    while (reader.GetNextLine()) {
//       const char* line = reader.line();
//       size_t line_len = reader.length();
//       ... line is not necessarily zero-terminated.
//    }

class LineReader {
 public:
  // Construct new instance. |path| is the input file path, and
  // |capacity| is the initial internal buffer capacity. Use a larger capacity
  // if you expect the input file to be large, in order to speed up parsing.
  // If opening the file fails, GetNextLine() will simply return false on
  // the first call.
  LineReader(const char* path, size_t capacity = 128);

  ~LineReader();

  // Grab next line. Returns true on success, or false otherwise.
  bool GetNextLine();

  // Return the start of the current line, this is _not_ zero-terminated
  // and always contains a final newline (\n).
  // Only call this after a successful GetNextLine().
  const char* line() const;

  // Return the line length, this includes the final \n.
  // Only call this after a successful GetNextLine().
  size_t length() const;

 private:
  FileDescriptor fd_;
  bool eof_ = false;
  size_t line_start_ = 0;
  size_t line_len_ = 0;
  size_t buff_size_ = 0;
  size_t buff_capacity_ = 0;
  char* buff_ = nullptr;
};

}  // namespace crazy

#endif  // CRAZY_LINKER_LINE_READER_H
