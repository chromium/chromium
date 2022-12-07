// Copyright 2022 The Abseil Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef ABSL_LOG_INTERNAL_CHECK_IMPL_H_
#define ABSL_LOG_INTERNAL_CHECK_IMPL_H_

#include "absl/base/optimization.h"
#include "absl/log/internal/check_op.h"
#include "absl/log/internal/conditions.h"
#include "absl/log/internal/log_message.h"
#include "absl/log/internal/strip.h"

// CHECK
#define ABSL_CHECK_IMPL(condition)                                    \
  ABSL_LOG_INTERNAL_CONDITION_FATAL(STATELESS,                        \
                                    ABSL_PREDICT_FALSE(!(condition))) \
  ABSL_LOG_INTERNAL_CHECK(#condition).InternalStream()

#define ABSL_QCHECK_IMPL(condition)                                    \
  ABSL_LOG_INTERNAL_CONDITION_QFATAL(STATELESS,                        \
                                     ABSL_PREDICT_FALSE(!(condition))) \
  ABSL_LOG_INTERNAL_QCHECK(#condition).InternalStream()

#define ABSL_PCHECK_IMPL(condition) ABSL_CHECK_IMPL(condition).WithPerror()

#ifndef NDEBUG
#define ABSL_DCHECK_IMPL(condition) ABSL_CHECK_IMPL(condition)
#else
#define ABSL_DCHECK_IMPL(condition) ABSL_CHECK_IMPL(true || (condition))
#endif

// CHECK_EQ
#define ABSL_CHECK_EQ_IMPL(val1, val2) \
  ABSL_LOG_INTERNAL_CHECK_OP(Check_EQ, ==, val1, val2)
#define ABSL_CHECK_NE_IMPL(val1, val2) \
  ABSL_LOG_INTERNAL_CHECK_OP(Check_NE, !=, val1, val2)
#define ABSL_CHECK_LE_IMPL(val1, val2) \
  ABSL_LOG_INTERNAL_CHECK_OP(Check_LE, <=, val1, val2)
#define ABSL_CHECK_LT_IMPL(val1, val2) \
  ABSL_LOG_INTERNAL_CHECK_OP(Check_LT, <, val1, val2)
#define ABSL_CHECK_GE_IMPL(val1, val2) \
  ABSL_LOG_INTERNAL_CHECK_OP(Check_GE, >=, val1, val2)
#define ABSL_CHECK_GT_IMPL(val1, val2) \
  ABSL_LOG_INTERNAL_CHECK_OP(Check_GT, >, val1, val2)
#define ABSL_QCHECK_EQ_IMPL(val1, val2) \
  ABSL_LOG_INTERNAL_QCHECK_OP(Check_EQ, ==, val1, val2)
#define ABSL_QCHECK_NE_IMPL(val1, val2) \
  ABSL_LOG_INTERNAL_QCHECK_OP(Check_NE, !=, val1, val2)
#define ABSL_QCHECK_LE_IMPL(val1, val2) \
  ABSL_LOG_INTERNAL_QCHECK_OP(Check_LE, <=, val1, val2)
#define ABSL_QCHECK_LT_IMPL(val1, val2) \
  ABSL_LOG_INTERNAL_QCHECK_OP(Check_LT, <, val1, val2)
#define ABSL_QCHECK_GE_IMPL(val1, val2) \
  ABSL_LOG_INTERNAL_QCHECK_OP(Check_GE, >=, val1, val2)
#define ABSL_QCHECK_GT_IMPL(val1, val2) \
  ABSL_LOG_INTERNAL_QCHECK_OP(Check_GT, >, val1, val2)
#ifndef NDEBUG
#define ABSL_DCHECK_EQ_IMPL(val1, val2) ABSL_CHECK_EQ_IMPL(val1, val2)
#define ABSL_DCHECK_NE_IMPL(val1, val2) ABSL_CHECK_NE_IMPL(val1, val2)
#define ABSL_DCHECK_LE_IMPL(val1, val2) ABSL_CHECK_LE_IMPL(val1, val2)
#define ABSL_DCHECK_LT_IMPL(val1, val2) ABSL_CHECK_LT_IMPL(val1, val2)
#define ABSL_DCHECK_GE_IMPL(val1, val2) ABSL_CHECK_GE_IMPL(val1, val2)
#define ABSL_DCHECK_GT_IMPL(val1, val2) ABSL_CHECK_GT_IMPL(val1, val2)
#else  // ndef NDEBUG
#define ABSL_DCHECK_EQ_IMPL(val1, val2) ABSL_LOG_INTERNAL_DCHECK_NOP(val1, val2)
#define ABSL_DCHECK_NE_IMPL(val1, val2) ABSL_LOG_INTERNAL_DCHECK_NOP(val1, val2)
#define ABSL_DCHECK_LE_IMPL(val1, val2) ABSL_LOG_INTERNAL_DCHECK_NOP(val1, val2)
#define ABSL_DCHECK_LT_IMPL(val1, val2) ABSL_LOG_INTERNAL_DCHECK_NOP(val1, val2)
#define ABSL_DCHECK_GE_IMPL(val1, val2) ABSL_LOG_INTERNAL_DCHECK_NOP(val1, val2)
#define ABSL_DCHECK_GT_IMPL(val1, val2) ABSL_LOG_INTERNAL_DCHECK_NOP(val1, val2)
#endif  // def NDEBUG

// CHECK_OK
#define ABSL_CHECK_OK_IMPL(status) ABSL_LOG_INTERNAL_CHECK_OK(status)
#define ABSL_QCHECK_OK_IMPL(status) ABSL_LOG_INTERNAL_QCHECK_OK(status)
#ifndef NDEBUG
#define ABSL_DCHECK_OK_IMPL(status) ABSL_LOG_INTERNAL_CHECK_OK(status)
#else
#define ABSL_DCHECK_OK_IMPL(status) \
  ABSL_LOG_INTERNAL_DCHECK_NOP(status, nullptr)
#endif

// CHECK_STREQ
#define ABSL_CHECK_STREQ_IMPL(s1, s2) \
  ABSL_LOG_INTERNAL_CHECK_STROP(strcmp, ==, true, s1, s2)
#define ABSL_CHECK_STRNE_IMPL(s1, s2) \
  ABSL_LOG_INTERNAL_CHECK_STROP(strcmp, !=, false, s1, s2)
#define ABSL_CHECK_STRCASEEQ_IMPL(s1, s2) \
  ABSL_LOG_INTERNAL_CHECK_STROP(strcasecmp, ==, true, s1, s2)
#define ABSL_CHECK_STRCASENE_IMPL(s1, s2) \
  ABSL_LOG_INTERNAL_CHECK_STROP(strcasecmp, !=, false, s1, s2)
#define ABSL_QCHECK_STREQ_IMPL(s1, s2) \
  ABSL_LOG_INTERNAL_QCHECK_STROP(strcmp, ==, true, s1, s2)
#define ABSL_QCHECK_STRNE_IMPL(s1, s2) \
  ABSL_LOG_INTERNAL_QCHECK_STROP(strcmp, !=, false, s1, s2)
#define ABSL_QCHECK_STRCASEEQ_IMPL(s1, s2) \
  ABSL_LOG_INTERNAL_QCHECK_STROP(strcasecmp, ==, true, s1, s2)
#define ABSL_QCHECK_STRCASENE_IMPL(s1, s2) \
  ABSL_LOG_INTERNAL_QCHECK_STROP(strcasecmp, !=, false, s1, s2)
#ifndef NDEBUG
#define ABSL_DCHECK_STREQ_IMPL(s1, s2) ABSL_CHECK_STREQ_IMPL(s1, s2)
#define ABSL_DCHECK_STRCASEEQ_IMPL(s1, s2) ABSL_CHECK_STRCASEEQ_IMPL(s1, s2)
#define ABSL_DCHECK_STRNE_IMPL(s1, s2) ABSL_CHECK_STRNE_IMPL(s1, s2)
#define ABSL_DCHECK_STRCASENE_IMPL(s1, s2) ABSL_CHECK_STRCASENE_IMPL(s1, s2)
#else  // ndef NDEBUG
#define ABSL_DCHECK_STREQ_IMPL(s1, s2) ABSL_LOG_INTERNAL_DCHECK_NOP(s1, s2)
#define ABSL_DCHECK_STRCASEEQ_IMPL(s1, s2) ABSL_LOG_INTERNAL_DCHECK_NOP(s1, s2)
#define ABSL_DCHECK_STRNE_IMPL(s1, s2) ABSL_LOG_INTERNAL_DCHECK_NOP(s1, s2)
#define ABSL_DCHECK_STRCASENE_IMPL(s1, s2) ABSL_LOG_INTERNAL_DCHECK_NOP(s1, s2)
#endif  // def NDEBUG

#endif  // ABSL_LOG_INTERNAL_CHECK_IMPL_H_
