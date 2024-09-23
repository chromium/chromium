// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_THIRD_PARTY_QUICHE_OVERRIDES_QUICHE_PLATFORM_IMPL_QUICHE_LOGGING_IMPL_H_
#define NET_THIRD_PARTY_QUICHE_OVERRIDES_QUICHE_PLATFORM_IMPL_QUICHE_LOGGING_IMPL_H_

#include <vector>

#include "base/check_op.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "build/build_config.h"
#include "quiche/common/platform/api/quiche_export.h"
#include "third_party/abseil-cpp/absl/base/optimization.h"

#define QUICHE_LOG_IMPL(severity) QUICHE_CHROMIUM_LOG_##severity
#define QUICHE_VLOG_IMPL(verbose_level) VLOG(verbose_level)
#define QUICHE_LOG_EVERY_N_SEC_IMPL(severity, seconds) QUICHE_LOG_IMPL(severity)
#define QUICHE_LOG_FIRST_N_IMPL(severity, n) QUICHE_LOG_IMPL(severity)
#define QUICHE_DLOG_IMPL(severity) QUICHE_CHROMIUM_DLOG_##severity
#define QUICHE_DLOG_IF_IMPL(severity, condition) \
  QUICHE_CHROMIUM_DLOG_IF_##severity(condition)
#define QUICHE_LOG_IF_IMPL(severity, condition) \
  QUICHE_CHROMIUM_LOG_IF_##severity(condition)

#define QUICHE_CHROMIUM_LOG_INFO VLOG(1)
#define QUICHE_CHROMIUM_LOG_WARNING DLOG(WARNING)
#define QUICHE_CHROMIUM_LOG_ERROR DLOG(ERROR)
// TODO(pbos): Make QUICHE_LOG(FATAL) [[noreturn]] when quiche can build with
// -Wunreachable-code-aggressive if LOG(FATAL) is [[noreturn]] which will need
// to be resolved upstream
#define QUICHE_CHROMIUM_LOG_FATAL \
  LAZY_STREAM(LOG_STREAM(FATAL),  \
              ::logging::ShouldCreateLogMessage(::logging::LOGGING_FATAL))
#define QUICHE_CHROMIUM_LOG_DFATAL LOG(DFATAL)

#define QUICHE_CHROMIUM_DLOG_INFO DVLOG(1)
#define QUICHE_CHROMIUM_DLOG_WARNING DLOG(WARNING)
#define QUICHE_CHROMIUM_DLOG_ERROR DLOG(ERROR)
#define QUICHE_CHROMIUM_DLOG_FATAL DLOG(FATAL)
#define QUICHE_CHROMIUM_DLOG_DFATAL DLOG(DFATAL)

#define QUICHE_CHROMIUM_LOG_IF_INFO(condition) VLOG_IF(1, condition)
#define QUICHE_CHROMIUM_LOG_IF_WARNING(condition) DLOG_IF(WARNING, condition)
#define QUICHE_CHROMIUM_LOG_IF_ERROR(condition) DLOG_IF(ERROR, condition)
#define QUICHE_CHROMIUM_LOG_IF_FATAL(condition) LOG_IF(FATAL, condition)
#define QUICHE_CHROMIUM_LOG_IF_DFATAL(condition) LOG_IF(DFATAL, condition)

#define QUICHE_CHROMIUM_DLOG_IF_INFO(condition) DVLOG_IF(1, condition)
#define QUICHE_CHROMIUM_DLOG_IF_WARNING(condition) DLOG_IF(WARNING, condition)
#define QUICHE_CHROMIUM_DLOG_IF_ERROR(condition) DLOG_IF(ERROR, condition)
#define QUICHE_CHROMIUM_DLOG_IF_FATAL(condition) DLOG_IF(FATAL, condition)
#define QUICHE_CHROMIUM_DLOG_IF_DFATAL(condition) DLOG_IF(DFATAL, condition)

#define QUICHE_DVLOG_IMPL(verbose_level) DVLOG(verbose_level)
#define QUICHE_DVLOG_IF_IMPL(verbose_level, condition) \
  DVLOG_IF(verbose_level, condition)

#define QUICHE_LOG_INFO_IS_ON_IMPL() 0
#ifdef NDEBUG
#define QUICHE_LOG_WARNING_IS_ON_IMPL() 0
#define QUICHE_LOG_ERROR_IS_ON_IMPL() 0
#else
#define QUICHE_LOG_WARNING_IS_ON_IMPL() 1
#define QUICHE_LOG_ERROR_IS_ON_IMPL() 1
#endif
#define QUICHE_DLOG_INFO_IS_ON_IMPL() 0

#if BUILDFLAG(IS_WIN)
// wingdi.h defines ERROR to be 0. When we call QUICHE_DLOG(ERROR), it gets
// substituted with 0, and it expands to QUICHE_CHROMIUM_DLOG_0. To allow us to
// keep using this syntax, we define this macro to do the same thing as
// QUICHE_CHROMIUM_DLOG_ERROR.
#define QUICHE_CHROMIUM_LOG_0 QUICHE_CHROMIUM_LOG_ERROR
#define QUICHE_CHROMIUM_DLOG_0 QUICHE_CHROMIUM_DLOG_ERROR
#define QUICHE_CHROMIUM_LOG_IF_0 QUICHE_CHROMIUM_LOG_IF_ERROR
#define QUICHE_CHROMIUM_DLOG_IF_0 QUICHE_CHROMIUM_DLOG_IF_ERROR
#endif

#define QUICHE_NOTREACHED_IMPL() NOTREACHED_IN_MIGRATION()

#define QUICHE_PLOG_IMPL(severity) DVLOG(1)

#define QUICHE_CHECK_IMPL(condition) CHECK(condition)
#define QUICHE_CHECK_EQ_IMPL(val1, val2) CHECK_EQ(val1, val2)
#define QUICHE_CHECK_NE_IMPL(val1, val2) CHECK_NE(val1, val2)
#define QUICHE_CHECK_LE_IMPL(val1, val2) CHECK_LE(val1, val2)
#define QUICHE_CHECK_LT_IMPL(val1, val2) CHECK_LT(val1, val2)
#define QUICHE_CHECK_GE_IMPL(val1, val2) CHECK_GE(val1, val2)
#define QUICHE_CHECK_GT_IMPL(val1, val2) CHECK_GT(val1, val2)
#define QUICHE_CHECK_OK_IMPL(value) CHECK((value).ok())

#define QUICHE_DCHECK_IMPL(condition) DCHECK(condition)
#define QUICHE_DCHECK_EQ_IMPL(val1, val2) DCHECK_EQ(val1, val2)
#define QUICHE_DCHECK_NE_IMPL(val1, val2) DCHECK_NE(val1, val2)
#define QUICHE_DCHECK_LE_IMPL(val1, val2) DCHECK_LE(val1, val2)
#define QUICHE_DCHECK_LT_IMPL(val1, val2) DCHECK_LT(val1, val2)
#define QUICHE_DCHECK_GE_IMPL(val1, val2) DCHECK_GE(val1, val2)
#define QUICHE_DCHECK_GT_IMPL(val1, val2) DCHECK_GT(val1, val2)

namespace quic {
template <typename T>
QUICHE_EXPORT inline std::ostream& operator<<(std::ostream& out,
                                              const std::vector<T>& v) {
  out << "[";
  const char* sep = "";
  for (size_t i = 0; i < v.size(); ++i) {
    out << sep << v[i];
    sep = ", ";
  }
  return out << "]";
}
}  // namespace quic

#endif  // NET_THIRD_PARTY_QUICHE_OVERRIDES_QUICHE_PLATFORM_IMPL_QUICHE_LOGGING_IMPL_H_
