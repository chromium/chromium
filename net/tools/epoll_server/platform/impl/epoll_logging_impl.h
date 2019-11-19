// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_TOOLS_EPOLL_SERVER_PLATFORM_IMPL_EPOLL_LOGGING_IMPL_H_
#define NET_TOOLS_EPOLL_SERVER_PLATFORM_IMPL_EPOLL_LOGGING_IMPL_H_

#include "base/logging.h"

#define EPOLL_CHROMIUM_LOG_INFO VLOG(1)
#define EPOLL_CHROMIUM_LOG_WARNING DLOG(WARNING)
#define EPOLL_CHROMIUM_LOG_ERROR DLOG(ERROR)
#define EPOLL_CHROMIUM_LOG_FATAL LOG(FATAL)
#define EPOLL_CHROMIUM_LOG_DFATAL LOG(DFATAL)

#define EPOLL_LOG_IMPL(severity) EPOLL_CHROMIUM_LOG_##severity
#define EPOLL_VLOG_IMPL(verbose_level) VLOG(verbose_level)
#define EPOLL_DVLOG_IMPL(verbose_level) DVLOG(verbose_level)
#define EPOLL_PLOG_IMPL(severity) DVLOG(1)

#endif  // NET_TOOLS_EPOLL_SERVER_PLATFORM_IMPL_EPOLL_LOGGING_IMPL_H_
