// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_APPLE_GUARDED_FD_H_
#define NET_BASE_APPLE_GUARDED_FD_H_

#include <stdint.h>

// "Guarded" file descriptors are a macOS SPI allowing a guard value to be
// assigned to a file descriptor, so as to prevent unwanted interference with
// its operation.
//
// Declarations from
// https://github.com/apple-oss-distributions/xnu/blob/rel/xnu-10002/bsd/sys/guarded.h

extern "C" {

using guardid_t = uint64_t;

const unsigned int GUARD_CLOSE = 1u << 0;
const unsigned int GUARD_DUP = 1u << 1;

int guarded_close_np(int fd,                     // in: file descriptor
                     const guardid_t* guard);    // in: current guard value
int change_fdguard_np(int fd,                    // in: file descriptor
                      const guardid_t* guard,    // in: current guard value
                      unsigned int guardflags,   // in: current guard flags
                      const guardid_t* nguard,   // in: new guard value
                      unsigned int nguardflags,  // in: new guard flags
                      int* fdflagsp);  // in/out: fd flags (fcntl:F_SETFD)

}  // extern "C"

#endif  // NET_BASE_APPLE_GUARDED_FD_H_
