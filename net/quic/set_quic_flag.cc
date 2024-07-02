// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/set_quic_flag.h"

#include "base/strings/string_number_conversions.h"
#include "net/third_party/quiche/src/quiche/common/platform/api/quiche_flags.h"
#include "net/third_party/quiche/src/quiche/quic/platform/api/quic_flags.h"

namespace net {

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

void SetQuicFlagByName_float(float* flag, const std::string& value) {
  double val;
  if (base::StringToDouble(value, &val)) {
    *flag = static_cast<float>(val);
  }
}

void SetQuicFlagByName_uint32_t(uint32_t* flag, const std::string& value) {
  uint32_t val;
  if (base::StringToUint(value, &val)) {
    *flag = val;
  }
}

void SetQuicFlagByName_uint64_t(uint64_t* flag, const std::string& value) {
  uint64_t val;
  if (base::StringToUint64(value, &val)) {
    *flag = val;
  }
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
#define QUICHE_FLAG(type, flag, internal_value, external_value, doc) \
  if (flag_name == "FLAGS_" #flag) {                                 \
    SetQuicFlagByName_##type(&FLAGS_##flag, value);                  \
    return;                                                          \
  }
#include "net/third_party/quiche/src/quiche/common/quiche_feature_flags_list.h"
#undef QUICHE_FLAG

#define QUICHE_PROTOCOL_FLAG(type, flag, ...)       \
  if (flag_name == "FLAGS_" #flag) {                \
    SetQuicFlagByName_##type(&FLAGS_##flag, value); \
    return;                                         \
  }
#include "net/third_party/quiche/src/quiche/common/quiche_protocol_flags_list.h"
#undef QUICHE_PROTOCOL_FLAG
}

}  // namespace net
