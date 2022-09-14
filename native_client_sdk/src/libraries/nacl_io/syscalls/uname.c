/* Copyright 2013 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

#include <stdio.h>
#include <string.h>
#include <sys/utsname.h>

#if !defined(_UTSNAME_LENGTH)
#if defined(__BIONIC__)
#define _UTSNAME_LENGTH SYS_NMLN
#endif
#if defined(__APPLE__)
#define _UTSNAME_LENGTH _SYS_NAMELEN
#endif
#endif

int uname(struct utsname* buf) {
  memset(buf, 0, sizeof(struct utsname));
  snprintf(buf->sysname, _UTSNAME_LENGTH, "NaCl");
  /* TODO(sbc): Fill out the other fields with useful information. */
  return 0;
}
