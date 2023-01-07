// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_THIRD_PARTY_QUICHE_OVERRIDES_QUICHE_PLATFORM_IMPL_QUICHE_IOVEC_IMPL_H_
#define NET_THIRD_PARTY_QUICHE_OVERRIDES_QUICHE_PLATFORM_IMPL_QUICHE_IOVEC_IMPL_H_

#include <stddef.h>

#include "build/build_config.h"

#if BUILDFLAG(IS_WIN)
/* Structure for scatter/gather I/O.  */
struct iovec {
  void* iov_base; /* Pointer to data.  */
  size_t iov_len; /* Length of data.  */
};
#elif BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
#include <sys/uio.h>
#endif  // BUILDFLAG(IS_WIN)

#endif  // NET_THIRD_PARTY_QUICHE_OVERRIDES_QUICHE_PLATFORM_IMPL_QUICHE_IOVEC_IMPL_H_
