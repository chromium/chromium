// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SOCKET_EXTRA_SOCKET_DEFINES_H_
#define NET_SOCKET_EXTRA_SOCKET_DEFINES_H_

#include "build/build_config.h"

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID)
#include <netinet/in.h>
#include <sys/socket.h>

// Define values that might be missing in older kernel headers.
#ifndef SOL_UDP
#define SOL_UDP 17
#endif
#ifndef UDP_SEGMENT
#define UDP_SEGMENT 103
#endif
#ifndef UDP_GRO
#define UDP_GRO 104
#endif
#endif

#endif  // NET_SOCKET_EXTRA_SOCKET_DEFINES_H_
