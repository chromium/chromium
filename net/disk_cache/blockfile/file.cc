// Copyright 2010 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/blockfile/file.h"

namespace disk_cache {

// Cross platform constructors. Platform specific code is in
// file_{win,posix}.cc.

File::File() : init_(false), mixed_(false) {}

File::File(bool mixed_mode) : init_(false), mixed_(mixed_mode) {}

bool File::Read(void* buffer, size_t buffer_len, size_t offset) {
  return UNSAFE_TODO(
      Read(base::span(static_cast<uint8_t*>(buffer), buffer_len), offset));
}

bool File::Write(const void* buffer, size_t buffer_len, size_t offset) {
  return UNSAFE_TODO(Write(
      base::span(static_cast<const uint8_t*>(buffer), buffer_len), offset));
}

bool File::Read(void* buffer,
                size_t buffer_len,
                size_t offset,
                FileIOCallback* callback,
                bool* completed) {
  return UNSAFE_TODO(Read(base::span(static_cast<uint8_t*>(buffer), buffer_len),
                          offset, callback, completed));
}

bool File::Write(const void* buffer,
                 size_t buffer_len,
                 size_t offset,
                 FileIOCallback* callback,
                 bool* completed) {
  return UNSAFE_TODO(
      Write(base::span(static_cast<const uint8_t*>(buffer), buffer_len), offset,
            callback, completed));
}

}  // namespace disk_cache
