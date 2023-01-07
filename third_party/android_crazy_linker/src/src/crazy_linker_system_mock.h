// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRAZY_LINKER_SYSTEM_MOCK_H
#define CRAZY_LINKER_SYSTEM_MOCK_H

#include <stdint.h>

namespace crazy {

class SystemMock {
 public:
  // Create a new mock system instance and make ScopedFileDescriptor use it.
  // There can be only one mock system active at a given time.
  SystemMock();

  // Destroy a mock system instance.
  ~SystemMock();

  // Add a regular file to the mock file system. |path| is the entry's
  // path, and |data| and |data_size| are the data there. The data must
  // stay valid until the mock file system is destroyed.
  void AddRegularFile(const char* path, const char* data, size_t data_size);

  void AddEnvVariable(const char* var_name, const char* var_value);

  void SetCurrentDir(const char* path);
};

}  // namespace crazy

#endif  // CRAZY_LINKER_SYSTEM_MOCK_H
