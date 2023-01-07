// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/cpp/private/pass_file_handle.h"

#ifdef _WIN32
# include <windows.h>
#else
# include <unistd.h>
#endif

namespace pp {

PassFileHandle::PassFileHandle()
    : handle_(PP_kInvalidFileHandle) {
}

PassFileHandle::PassFileHandle(PP_FileHandle handle)
    : handle_(handle) {
}

PassFileHandle::PassFileHandle(PassFileHandle& handle)
    : handle_(handle.Release()) {
}

PassFileHandle::~PassFileHandle() {
  Close();
}

PP_FileHandle PassFileHandle::Release() {
  PP_FileHandle released = handle_;
  handle_ = PP_kInvalidFileHandle;
  return released;
}

void PassFileHandle::Close() {
  if (handle_ != PP_kInvalidFileHandle) {
#ifdef _WIN32
    CloseHandle(handle_);
#else
    close(handle_);
#endif
    handle_ = PP_kInvalidFileHandle;
  }
}

}  // namespace pp
