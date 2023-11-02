// Copyright 2009 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is a convenience header to pull in the platform-specific headers
// that define at least:
//
//     struct addrinfo
//     struct sockaddr*
//     getaddrinfo()
//     freeaddrinfo()
//     AI_*
//     AF_*
//
// Prefer including this file instead of directly writing the #if / #else,
// since it avoids duplicating the platform-specific selections.
//
// Note that this header transitively includes windows.h on Windows, which
// pollutes the global namespace with thousands of macro definitions, so try to
// avoid including this in headers. Including windows.h can also add significant
// build overhead.

#ifndef NET_BASE_SYS_ADDRINFO_H_
#define NET_BASE_SYS_ADDRINFO_H_

#include "build/build_config.h"

#if BUILDFLAG(IS_WIN)
#include <winsock2.h>
#include <ws2tcpip.h>
#elif BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#endif

#endif  // NET_BASE_SYS_ADDRINFO_H_
