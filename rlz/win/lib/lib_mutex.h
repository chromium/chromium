// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef RLZ_WIN_LIB_LIB_MUTEX_H_
#define RLZ_WIN_LIB_LIB_MUTEX_H_

#include <windows.h>

namespace rlz_lib {
// Cross-process mutex to guarantee serialization of RLZ key accesses.
class LibMutex {
 public:
  LibMutex();
  ~LibMutex();

  bool failed() const { return !acquired_; }

 private:
  bool acquired_;
  HANDLE mutex_;
};

}  // namespace rlz_lib

#endif  // RLZ_WIN_LIB_LIB_MUTEX_H_
