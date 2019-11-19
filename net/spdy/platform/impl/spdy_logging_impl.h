// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SPDY_PLATFORM_IMPL_SPDY_LOGGING_IMPL_H_
#define NET_SPDY_PLATFORM_IMPL_SPDY_LOGGING_IMPL_H_

#include "base/logging.h"
#include "build/build_config.h"
#include "net/base/net_export.h"

#define SPDY_LOG_IMPL(severity) SPDY_CHROMIUM_LOG_##severity
#define SPDY_VLOG_IMPL(verbose_level) VLOG(verbose_level)
#define SPDY_DLOG_IMPL(severity) SPDY_CHROMIUM_DLOG_##severity
#define SPDY_DLOG_IF_IMPL(severity, condition) \
  SPDY_CHROMIUM_DLOG_IF_##severity(condition)
#define SPDY_DVLOG_IMPL(verbose_level) DVLOG(verbose_level)
#define SPDY_DVLOG_IF_IMPL(verbose_level, condition) \
  DVLOG_IF(verbose_level, condition)

#define SPDY_CHROMIUM_LOG_INFO VLOG(1)
#define SPDY_CHROMIUM_LOG_WARNING DLOG(WARNING)
#define SPDY_CHROMIUM_LOG_ERROR DLOG(ERROR)
#define SPDY_CHROMIUM_LOG_FATAL LOG(FATAL)
#define SPDY_CHROMIUM_LOG_DFATAL LOG(DFATAL)

#define SPDY_CHROMIUM_DLOG_INFO DVLOG(1)
#define SPDY_CHROMIUM_DLOG_WARNING DLOG(WARNING)
#define SPDY_CHROMIUM_DLOG_ERROR DLOG(ERROR)
#define SPDY_CHROMIUM_DLOG_FATAL DLOG(FATAL)
#define SPDY_CHROMIUM_DLOG_DFATAL DLOG(DFATAL)

#define SPDY_CHROMIUM_LOG_IF_INFO(condition) VLOG_IF(1, condition)
#define SPDY_CHROMIUM_LOG_IF_WARNING(condition) DLOG_IF(WARNING, condition)
#define SPDY_CHROMIUM_LOG_IF_ERROR(condition) DLOG_IF(ERROR, condition)
#define SPDY_CHROMIUM_LOG_IF_FATAL(condition) LOG_IF(FATAL, condition)
#define SPDY_CHROMIUM_LOG_IF_DFATAL(condition) LOG_IF(DFATAL, condition)

#define SPDY_CHROMIUM_DLOG_IF_INFO(condition) DVLOG_IF(1, condition)
#define SPDY_CHROMIUM_DLOG_IF_WARNING(condition) DLOG_IF(WARNING, condition)
#define SPDY_CHROMIUM_DLOG_IF_ERROR(condition) DLOG_IF(ERROR, condition)
#define SPDY_CHROMIUM_DLOG_IF_FATAL(condition) DLOG_IF(FATAL, condition)
#define SPDY_CHROMIUM_DLOG_IF_DFATAL(condition) DLOG_IF(DFATAL, condition)

#define SPDY_LOG_INFO_IS_ON_IMPL() 0
#ifdef NDEBUG
#define SPDY_LOG_WARNING_IS_ON_IMPL() 0
#define SPDY_LOG_ERROR_IS_ON_IMPL() 0
#else
#define SPDY_LOG_WARNING_IS_ON_IMPL() 1
#define SPDY_LOG_ERROR_IS_ON_IMPL() 1
#endif
#define SPDY_DLOG_INFO_IS_ON_IMPL() 0

#if defined(OS_WIN)
// wingdi.h defines ERROR to be 0. When we call SPDY_DLOG(ERROR), it gets
// substituted with 0, and it expands to SPDY_CHROMIUM_DLOG_0. To allow us to
// keep using this syntax, we define this macro to do the same thing as
// SPDY_CHROMIUM_DLOG_ERROR.
#define SPDY_CHROMIUM_LOG_0 SPDY_CHROMIUM_LOG_ERROR
#define SPDY_CHROMIUM_DLOG_0 SPDY_CHROMIUM_DLOG_ERROR
#define SPDY_CHROMIUM_LOG_IF_0 SPDY_CHROMIUM_LOG_IF_ERROR
#define SPDY_CHROMIUM_DLOG_IF_0 SPDY_CHROMIUM_DLOG_IF_ERROR
#endif

#define SPDY_PREDICT_FALSE_IMPL(x) x

#define SPDY_NOTREACHED_IMPL() NOTREACHED()

#define SPDY_PLOG_IMPL(severity) DVLOG(1)

namespace spdy {
template <typename T>
NET_EXPORT_PRIVATE inline std::ostream& operator<<(std::ostream& out,
                                                   const std::vector<T>& v) {
  out << "[";
  const char* sep = "";
  for (size_t i = 0; i < v.size(); ++i) {
    out << sep << v[i];
    sep = ", ";
  }
  return out << "]";
}
}  // namespace spdy

#endif  // NET_SPDY_PLATFORM_IMPL_SPDY_LOGGING_IMPL_H_
