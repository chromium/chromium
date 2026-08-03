// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/session_options.h"

#include <string_view>

#include "base/containers/flat_map.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "build/build_config.h"
#include "remoting/base/session_options_constants.h"

namespace remoting {

namespace {

std::optional<bool> ParseBool(std::string_view value) {
  const std::string lowercase_value = base::ToLowerASCII(value);
  if (lowercase_value.empty() || lowercase_value == "true" ||
      lowercase_value == "1") {
    return true;
  }
  if (lowercase_value == "false" || lowercase_value == "0") {
    return false;
  }
  LOG(WARNING) << "Unexpected option value received " << value
               << " which cannot be converted to bool.";
  return std::nullopt;
}

std::optional<int> ParseInt(std::string_view value) {
  int result;
  if (base::StringToInt(value, &result)) {
    return result;
  }
  LOG(WARNING) << "Unexpected option value received " << value
               << " which cannot be converted to integer.";
  return std::nullopt;
}

template <typename T>
void PrintOptional(std::ostream& os, const std::optional<T>& opt) {
  if (!opt.has_value()) {
    os << "<unspecified>";
  } else {
    os << *opt;
  }
}

using BoolFieldMap =
    base::flat_map<std::string_view, std::optional<bool> SessionOptions::*>;
using IntFieldMap =
    base::flat_map<std::string_view, std::optional<int> SessionOptions::*>;

const BoolFieldMap& GetBoolFieldsMap() {
  static const base::NoDestructor<BoolFieldMap> kMap({
      {kSessionOptionDetectUpdatedRegion,
       &SessionOptions::detect_updated_region},
      {kSessionOptionCaptureVideoOnDedicatedThread,
       &SessionOptions::capture_video_on_dedicated_thread},
#if BUILDFLAG(IS_MAC)
      {kSessionOptionEnableSckCapturer, &SessionOptions::enable_sck_capturer},
#endif  // BUILDFLAG(IS_MAC)
#if BUILDFLAG(IS_WIN)
      {kSessionOptionAllowDxgiCapturer, &SessionOptions::allow_dxgi_capturer},
#endif  // BUILDFLAG(IS_WIN)
      {kSessionOptionDisableUdp, &SessionOptions::disable_udp},
      {kSessionOptionAv1ActiveMap, &SessionOptions::av1_active_map},
  });
  return *kMap;
}

const IntFieldMap& GetIntFieldsMap() {
  static const base::NoDestructor<IntFieldMap> kMap({
      {kSessionOptionVp9EncoderSpeed, &SessionOptions::vp9_encoder_speed},
      {kSessionOptionAv1EncoderSpeed, &SessionOptions::av1_encoder_speed},
  });
  return *kMap;
}

}  // namespace

SessionOptions::SessionOptions() = default;
SessionOptions::~SessionOptions() = default;

SessionOptions::SessionOptions(const SessionOptions& other) = default;
SessionOptions& SessionOptions::operator=(const SessionOptions& other) =
    default;
SessionOptions::SessionOptions(SessionOptions&& other) = default;
SessionOptions& SessionOptions::operator=(SessionOptions&& other) = default;

bool SessionOptions::operator==(const SessionOptions& other) const = default;

SessionOptions SessionOptions::Parse(const base::DictValue& dict) {
  const auto& bool_fields = GetBoolFieldsMap();
  const auto& int_fields = GetIntFieldsMap();

  SessionOptions options;
  for (auto [key, value] : dict) {
    if (!value.is_string()) {
      LOG(WARNING) << "Unexpected non-string value for option key " << key;
      continue;
    }
    const std::string& string_val = value.GetString();

    auto bool_it = bool_fields.find(key);
    if (bool_it != bool_fields.end()) {
      std::optional<bool> bool_val = ParseBool(string_val);
      if (!bool_val.has_value()) {
        continue;
      }
      options.*(bool_it->second) = bool_val;
      continue;
    }

    auto int_it = int_fields.find(key);
    if (int_it != int_fields.end()) {
      std::optional<int> int_val = ParseInt(string_val);
      if (!int_val.has_value()) {
        continue;
      }
      options.*(int_it->second) = int_val;
      continue;
    }

    LOG(WARNING) << "Unsupported session option key: " << key;
  }
  return options;
}

std::ostream& operator<<(std::ostream& os,
                         const SessionOptions& session_options) {
  os << "{ ";
  bool first = true;
  for (const auto& [key, field_ptr] : GetBoolFieldsMap()) {
    if (!first) {
      os << ", ";
    }
    first = false;
    os << key << ": ";
    PrintOptional(os, session_options.*field_ptr);
  }
  for (const auto& [key, field_ptr] : GetIntFieldsMap()) {
    if (!first) {
      os << ", ";
    }
    first = false;
    os << key << ": ";
    PrintOptional(os, session_options.*field_ptr);
  }
  os << " }";
  return os;
}

}  // namespace remoting
