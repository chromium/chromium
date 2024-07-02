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
#define QUICHE_FLAG(type, flag, internal_value, external_value, doc) \
  saved_##flag##_ = FLAGS_##flag;
#include "net/third_party/quiche/src/quiche/common/quiche_feature_flags_list.h"
#undef QUICHE_FLAG
#define QUICHE_PROTOCOL_FLAG(type, flag, ...) saved_##flag##_ = FLAGS_##flag;
#include "net/third_party/quiche/src/quiche/common/quiche_protocol_flags_list.h"
#undef QUICHE_PROTOCOL_FLAG
}

QuicFlagSaverImpl::~QuicFlagSaverImpl() {
#define QUICHE_FLAG(type, flag, internal_value, external_value, doc) \
  FLAGS_##flag = saved_##flag##_;
#include "net/third_party/quiche/src/quiche/common/quiche_feature_flags_list.h"
#undef QUICHE_FLAG
#define QUICHE_PROTOCOL_FLAG(type, flag, ...) FLAGS_##flag = saved_##flag##_;
#include "net/third_party/quiche/src/quiche/common/quiche_protocol_flags_list.h"
#undef QUICHE_PROTOCOL_FLAG
}

QuicFlagChecker::QuicFlagChecker() {
#define QUICHE_FLAG(type, flag, internal_value, external_value, doc)      \
  CHECK_EQ(external_value, FLAGS_##flag)                                  \
      << "Flag set to an unexpected value.  A prior test is likely "      \
      << "setting a flag without using a QuicFlagSaver. Use QuicTest to " \
         "avoid this issue.";
#include "net/third_party/quiche/src/quiche/common/quiche_feature_flags_list.h"
#undef QUICHE_FLAG

#define QUICHE_PROTOCOL_FLAG_CHECK(type, flag, value)                     \
  CHECK_EQ((type)value, FLAGS_##flag)                                     \
      << "Flag set to an unexpected value.  A prior test is likely "      \
      << "setting a flag without using a QuicFlagSaver. Use QuicTest to " \
         "avoid this issue.";
#define DEFINE_QUICHE_PROTOCOL_FLAG_SINGLE_VALUE(type, flag, value, doc) \
  QUICHE_PROTOCOL_FLAG_CHECK(type, flag, value);

#define DEFINE_QUICHE_PROTOCOL_FLAG_TWO_VALUES(type, flag, internal_value, \
                                               external_value, doc)        \
  QUICHE_PROTOCOL_FLAG_CHECK(type, flag, external_value);
#define GET_6TH_ARG(arg1, arg2, arg3, arg4, arg5, arg6, ...) arg6
#define QUICHE_PROTOCOL_FLAG_MACRO_CHOOSER(...)                    \
  GET_6TH_ARG(__VA_ARGS__, DEFINE_QUICHE_PROTOCOL_FLAG_TWO_VALUES, \
              DEFINE_QUICHE_PROTOCOL_FLAG_SINGLE_VALUE)
#define QUICHE_PROTOCOL_FLAG(...) \
  QUICHE_PROTOCOL_FLAG_MACRO_CHOOSER(__VA_ARGS__)(__VA_ARGS__)
#include "net/third_party/quiche/src/quiche/common/quiche_protocol_flags_list.h"
#undef QUICHE_PROTOCOL_FLAG
#undef QUICHE_PROTOCOL_FLAG_MACRO_CHOOSER
#undef GET_6TH_ARG
#undef DEFINE_QUICHE_PROTOCOL_FLAG_TWO_VALUES
#undef DEFINE_QUICHE_PROTOCOL_FLAG_SINGLE_VALUE
#undef QUICHE_PROTOCOL_FLAG_CHECK
}
