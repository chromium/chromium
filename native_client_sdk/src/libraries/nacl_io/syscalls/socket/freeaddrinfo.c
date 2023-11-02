/* Copyright 2014 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

#include "nacl_io/kernel_intercept.h"

void freeaddrinfo(struct addrinfo *res) {
  ki_freeaddrinfo(res);
}
