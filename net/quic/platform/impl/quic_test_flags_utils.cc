// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdint>
#include <iostream>

#include "net/quic/platform/impl/quic_test_flags_utils.h"

#include "base/check_op.h"
#include "net/third_party/quiche/src/quiche/common/platform/api/quiche_flags.h"
#include "net/third_party/quiche/src/quiche/quic/platform/api/quic_flags.h"

QuicFlagSaverImpl::QuicFlagSaverImpl() {
#define QUIC_FLAG(flag, value) saved_##flag##_ = FLAGS_##flag;
#include "net/third_party/quiche/src/quiche/quic/core/quic_flags_list.h"
#undef QUIC_FLAG
#define QUIC_PROTOCOL_FLAG(type, flag, ...) saved_##flag##_ = FLAGS_##flag;
#include "net/third_party/quiche/src/quiche/quic/core/quic_protocol_flags_list.h"
#undef QUIC_PROTOCOL_FLAG
}

QuicFlagSaverImpl::~QuicFlagSaverImpl() {
#define QUIC_FLAG(flag, value) FLAGS_##flag = saved_##flag##_;
#include "net/third_party/quiche/src/quiche/quic/core/quic_flags_list.h"
#undef QUIC_FLAG
#define QUIC_PROTOCOL_FLAG(type, flag, ...) FLAGS_##flag = saved_##flag##_;
#include "net/third_party/quiche/src/quiche/quic/core/quic_protocol_flags_list.h"
#undef QUIC_PROTOCOL_FLAG
}

QuicFlagChecker::QuicFlagChecker() {
#define QUIC_FLAG(flag, value)                                            \
  CHECK_EQ(value, FLAGS_##flag)                                           \
      << "Flag set to an unexpected value.  A prior test is likely "      \
      << "setting a flag without using a QuicFlagSaver. Use QuicTest to " \
         "avoid this issue.";
#include "net/third_party/quiche/src/quiche/quic/core/quic_flags_list.h"
#undef QUIC_FLAG

#define QUIC_PROTOCOL_FLAG_CHECK(type, flag, value)                       \
  CHECK_EQ((type)value, FLAGS_##flag)                                     \
      << "Flag set to an unexpected value.  A prior test is likely "      \
      << "setting a flag without using a QuicFlagSaver. Use QuicTest to " \
         "avoid this issue.";
#define DEFINE_QUIC_PROTOCOL_FLAG_SINGLE_VALUE(type, flag, value, doc) \
  QUIC_PROTOCOL_FLAG_CHECK(type, flag, value);

#define DEFINE_QUIC_PROTOCOL_FLAG_TWO_VALUES(type, flag, internal_value, \
                                             external_value, doc)        \
  QUIC_PROTOCOL_FLAG_CHECK(type, flag, external_value);
#define GET_6TH_ARG(arg1, arg2, arg3, arg4, arg5, arg6, ...) arg6
#define QUIC_PROTOCOL_FLAG_MACRO_CHOOSER(...)                    \
  GET_6TH_ARG(__VA_ARGS__, DEFINE_QUIC_PROTOCOL_FLAG_TWO_VALUES, \
              DEFINE_QUIC_PROTOCOL_FLAG_SINGLE_VALUE)
#define QUIC_PROTOCOL_FLAG(...) \
  QUIC_PROTOCOL_FLAG_MACRO_CHOOSER(__VA_ARGS__)(__VA_ARGS__)
#include "net/third_party/quiche/src/quiche/quic/core/quic_protocol_flags_list.h"
#undef QUIC_PROTOCOL_FLAG
#undef QUIC_PROTOCOL_FLAG_MACRO_CHOOSER
#undef GET_6TH_ARG
#undef DEFINE_QUIC_PROTOCOL_FLAG_TWO_VALUES
#undef DEFINE_QUIC_PROTOCOL_FLAG_SINGLE_VALUE
#undef QUIC_PROTOCOL_FLAG_CHECK
}
