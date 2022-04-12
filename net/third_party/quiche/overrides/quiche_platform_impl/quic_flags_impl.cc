// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/third_party/quiche/overrides/quiche_platform_impl/quic_flags_impl.h"

#include <string>

#include "base/strings/string_number_conversions.h"

#define DEFINE_QUIC_PROTOCOL_FLAG_SINGLE_VALUE(type, flag, value, doc) \
  type FLAGS_##flag = value;

#define DEFINE_QUIC_PROTOCOL_FLAG_TWO_VALUES(type, flag, internal_value, \
                                             external_value, doc)        \
  type FLAGS_##flag = external_value;

// Preprocessor macros can only have one definition.
// Select the right macro based on the number of arguments.
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

namespace {

void SetQuicFlagByName_bool(bool* flag, const std::string& value) {
  if (value == "true" || value == "True")
    *flag = true;
  else if (value == "false" || value == "False")
    *flag = false;
}
void SetQuicFlagByName_double(double* flag, const std::string& value) {
  double val;
  if (base::StringToDouble(value, &val))
    *flag = val;
}

void SetQuicFlagByName_uint64_t(uint64_t* flag, const std::string& value) {
  uint64_t val;
  if (base::StringToUint64(value, &val) && val >= 0)
    *flag = val;
}

void SetQuicFlagByName_int32_t(int32_t* flag, const std::string& value) {
  int val;
  if (base::StringToInt(value, &val))
    *flag = val;
}

void SetQuicFlagByName_int64_t(int64_t* flag, const std::string& value) {
  int64_t val;
  if (base::StringToInt64(value, &val))
    *flag = val;
}

}  // namespace

void SetQuicFlagByName(const std::string& flag_name, const std::string& value) {
#define QUIC_FLAG(flag, default_value)    \
  if (flag_name == #flag) {               \
    SetQuicFlagByName_bool(&flag, value); \
    return;                               \
  }
#include "net/third_party/quiche/src/quiche/quic/core/quic_flags_list.h"
#undef QUIC_FLAG

#define QUIC_PROTOCOL_FLAG(type, flag, ...)         \
  if (flag_name == "FLAGS_" #flag) {                \
    SetQuicFlagByName_##type(&FLAGS_##flag, value); \
    return;                                         \
  }
#include "net/third_party/quiche/src/quiche/quic/core/quic_protocol_flags_list.h"
#undef QUIC_PROTOCOL_FLAG
}
