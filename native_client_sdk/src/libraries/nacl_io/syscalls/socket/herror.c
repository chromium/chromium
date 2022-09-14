/* Copyright 2013 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

#include "nacl_io/ossocket.h"

#ifdef PROVIDES_SOCKET_API

#include <stdio.h>

void herror(const char* s) {
  if (s) {
    fprintf(stderr, "%s: ", s);
  }

  fprintf(stderr, "%s\n", hstrerror(h_errno));
}

#endif /* PROVIDES_SOCKET_API */
