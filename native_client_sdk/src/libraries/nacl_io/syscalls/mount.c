/* Copyright 2013 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

#include "nacl_io/kernel_intercept.h"
#include "nacl_io/kernel_wrap.h"

int mount(const char* source, const char* target, const char* filesystemtype,
          unsigned long mountflags, const void* data) {
  return ki_mount(source, target, filesystemtype, mountflags, data);
}
