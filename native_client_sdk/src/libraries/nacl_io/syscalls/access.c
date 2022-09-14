/* Copyright 2013 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

#include "nacl_io/kernel_intercept.h"
#include "nacl_io/kernel_wrap.h"

#if defined(__BIONIC__)
// Bionic only
int access(const char* path, int amode) {
  return ki_access(path, amode);
}
#endif
