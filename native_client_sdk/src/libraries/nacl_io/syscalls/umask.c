/* Copyright 2014 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

#include "nacl_io/kernel_intercept.h"
#include "nacl_io/kernel_wrap.h"

mode_t umask(mode_t mask) {
  return ki_umask(mask);
}
