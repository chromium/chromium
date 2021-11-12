// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_WIN_SANDBOX_POC_POCDLL_UTILS_H_
#define SANDBOX_WIN_SANDBOX_POC_POCDLL_UTILS_H_

#include <stdio.h>
#include <io.h>

// Class to convert a HANDLE to a FILE *. The FILE * is closed when the
// object goes out of scope
class HandleToFile {
 public:
  HandleToFile() { file_ = nullptr; }

  HandleToFile(const HandleToFile&) = delete;
  HandleToFile& operator=(const HandleToFile&) = delete;

  // Note: c_file_handle_ does not need to be closed because fclose does it.
  ~HandleToFile() {
    if (file_) {
      fflush(file_);
      fclose(file_);
    }
  }

  // Translates a HANDLE (handle) to a FILE * opened with the mode "mode".
  // The return value is the FILE * or NULL if there is an error.
  FILE* Translate(HANDLE handle, const char *mode) {
    if (file_) {
      return  NULL;
    }

    HANDLE new_handle;
    BOOL result = ::DuplicateHandle(::GetCurrentProcess(),
                                    handle,
                                    ::GetCurrentProcess(),
                                    &new_handle,
                                    0,  // Don't ask for a specific
                                        // desired access.
                                    FALSE,  // Not inheritable.
                                    DUPLICATE_SAME_ACCESS);

    if (!result) {
      return NULL;
    }

    int c_file_handle = _open_osfhandle(reinterpret_cast<LONG_PTR>(new_handle),
                                        0);  // No flags
    if (-1 == c_file_handle) {
      return NULL;
    }

    file_ = _fdopen(c_file_handle, mode);
    return file_;
  }
 private:
  // the FILE* returned. We need to closed it at the end.
  FILE* file_;
};

#endif  // SANDBOX_WIN_SANDBOX_POC_POCDLL_UTILS_H_
