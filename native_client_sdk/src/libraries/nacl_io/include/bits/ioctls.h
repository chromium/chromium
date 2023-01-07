/* Copyright 2015 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

#ifndef _SYS_IOCTL_H
# error "Never use <bits/ioctls.h> directly; include <sys/ioctl.h> instead."
#endif

/*
 * This file overrides the default bits/ioctls.h that we ship with the
 * glibc toolchain.  With the arm version of the toolchain this file is
 * deliberately left empty, but nacl_io requires these ioctl to be defined.
 */

#define TIOCGWINSZ 0x5413
#define TIOCSWINSZ 0x5414
