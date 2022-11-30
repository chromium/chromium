/* Copyright 2013 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

#include "nacl_io/kernel_intercept.h"
#include "nacl_io/kernel_wrap.h"

int sigaction(int signum,
              const struct sigaction* act,
              struct sigaction* oldact) {
  return ki_sigaction(signum, act, oldact);
}
