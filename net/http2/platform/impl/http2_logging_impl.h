// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP2_PLATFORM_IMPL_HTTP2_LOGGING_IMPL_H_
#define NET_HTTP2_PLATFORM_IMPL_HTTP2_LOGGING_IMPL_H_

#include "base/logging.h"
#include "build/build_config.h"
#include "net/base/net_export.h"

#define HTTP2_LOG_IMPL(severity) HTTP2_CHROMIUM_LOG_##severity
#define HTTP2_VLOG_IMPL(verbose_level) VLOG(verbose_level)
#define HTTP2_DLOG_IMPL(severity) HTTP2_CHROMIUM_DLOG_##severity
#define HTTP2_DLOG_IF_IMPL(severity, condition) \
  HTTP2_CHROMIUM_DLOG_IF_##severity(condition)
#define HTTP2_DVLOG_IMPL(verbose_level) DVLOG(verbose_level)
#define HTTP2_DVLOG_IF_IMPL(verbose_level, condition) \
  DVLOG_IF(verbose_level, condition)

#define HTTP2_CHROMIUM_LOG_INFO VLOG(1)
#define HTTP2_CHROMIUM_LOG_WARNING DLOG(WARNING)
#define HTTP2_CHROMIUM_LOG_ERROR DLOG(ERROR)
#define HTTP2_CHROMIUM_LOG_FATAL LOG(FATAL)
#define HTTP2_CHROMIUM_LOG_DFATAL LOG(DFATAL)

#define HTTP2_CHROMIUM_DLOG_INFO DVLOG(1)
#define HTTP2_CHROMIUM_DLOG_WARNING DLOG(WARNING)
#define HTTP2_CHROMIUM_DLOG_ERROR DLOG(ERROR)
#define HTTP2_CHROMIUM_DLOG_FATAL DLOG(FATAL)
#define HTTP2_CHROMIUM_DLOG_DFATAL DLOG(DFATAL)

#define HTTP2_CHROMIUM_LOG_IF_INFO(condition) VLOG_IF(1, condition)
#define HTTP2_CHROMIUM_LOG_IF_WARNING(condition) DLOG_IF(WARNING, condition)
#define HTTP2_CHROMIUM_LOG_IF_ERROR(condition) DLOG_IF(ERROR, condition)
#define HTTP2_CHROMIUM_LOG_IF_FATAL(condition) LOG_IF(FATAL, condition)
#define HTTP2_CHROMIUM_LOG_IF_DFATAL(condition) LOG_IF(DFATAL, condition)

#define HTTP2_CHROMIUM_DLOG_IF_INFO(condition) DVLOG_IF(1, condition)
#define HTTP2_CHROMIUM_DLOG_IF_WARNING(condition) DLOG_IF(WARNING, condition)
#define HTTP2_CHROMIUM_DLOG_IF_ERROR(condition) DLOG_IF(ERROR, condition)
#define HTTP2_CHROMIUM_DLOG_IF_FATAL(condition) DLOG_IF(FATAL, condition)
#define HTTP2_CHROMIUM_DLOG_IF_DFATAL(condition) DLOG_IF(DFATAL, condition)

#define HTTP2_LOG_INFO_IS_ON_IMPL() 0
#ifdef NDEBUG
#define HTTP2_LOG_WARNING_IS_ON_IMPL() 0
#define HTTP2_LOG_ERROR_IS_ON_IMPL() 0
#else
#define HTTP2_LOG_WARNING_IS_ON_IMPL() 1
#define HTTP2_LOG_ERROR_IS_ON_IMPL() 1
#endif
#define HTTP2_DLOG_INFO_IS_ON_IMPL() 0

#if defined(OS_WIN)
// wingdi.h defines ERROR to be 0. When we call HTTP2_DLOG(ERROR), it gets
// substituted with 0, and it expands to HTTP2_CHROMIUM_DLOG_0. To allow us to
// keep using this syntax, we define this macro to do the same thing as
// HTTP2_CHROMIUM_DLOG_ERROR.
#define HTTP2_CHROMIUM_LOG_0 HTTP2_CHROMIUM_LOG_ERROR
#define HTTP2_CHROMIUM_DLOG_0 HTTP2_CHROMIUM_DLOG_ERROR
#define HTTP2_CHROMIUM_LOG_IF_0 HTTP2_CHROMIUM_LOG_IF_ERROR
#define HTTP2_CHROMIUM_DLOG_IF_0 HTTP2_CHROMIUM_DLOG_IF_ERROR
#endif

#define HTTP2_PREDICT_FALSE_IMPL(x) x

#define HTTP2_NOTREACHED_IMPL() NOTREACHED()

#define HTTP2_PLOG_IMPL(severity) DVLOG(1)

#endif  // NET_HTTP2_PLATFORM_IMPL_HTTP2_LOGGING_IMPL_H_
