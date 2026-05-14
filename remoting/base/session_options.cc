// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/session_options.h"

#include <string_view>
#include <vector>

#include "base/logging.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/values.h"

namespace remoting {

namespace {

static constexpr char kSeparator = ',';
static constexpr char kKeyValueSeparator = ':';

// Whether |value| is good to be added to SessionOptions as a value.
bool ValueIsValid(std::string_view value) {
  return !value.contains(kSeparator) && !value.contains(kKeyValueSeparator) &&
         base::IsStringASCII(value);
}

// Whether |key| is good to be added to SessionOptions as a key.
bool KeyIsValid(std::string_view key) {
  return !key.empty() && ValueIsValid(key);
}

}  // namespace

SessionOptions::SessionOptions() = default;
SessionOptions::SessionOptions(const SessionOptions& other) = default;
SessionOptions::SessionOptions(SessionOptions&& other) = default;

SessionOptions::SessionOptions(std::string_view parameter) {
  Import(parameter);
}

SessionOptions::SessionOptions(const base::DictValue& dict) {
  for (auto [key, value] : dict) {
    if (value.is_string() && KeyIsValid(key) &&
        ValueIsValid(value.GetString())) {
      Append(key, value.GetString());
    }
  }
}

SessionOptions::~SessionOptions() = default;

SessionOptions& SessionOptions::operator=(const SessionOptions& other) =
    default;
SessionOptions& SessionOptions::operator=(SessionOptions&& other) = default;

void SessionOptions::Append(std::string_view key, std::string_view value) {
  DCHECK(KeyIsValid(key));
  DCHECK(ValueIsValid(value));
  options_[key] = value;
}

std::optional<std::string> SessionOptions::Get(std::string_view key) const {
  auto it = options_.find(key);
  if (it == options_.end()) {
    return std::nullopt;
  }
  return it->second;
}

std::optional<bool> SessionOptions::GetBool(std::string_view key) const {
  std::optional<std::string> value = Get(key);
  if (!value) {
    return std::nullopt;
  }

  const std::string lowercase_value = base::ToLowerASCII(*value);
  if (lowercase_value.empty() || lowercase_value == "true" ||
      lowercase_value == "1") {
    return true;
  }
  if (lowercase_value == "false" || lowercase_value == "0") {
    return false;
  }
  LOG(WARNING) << "Unexpected option received " << *value
               << " which cannot be converted to bool.";
  return std::nullopt;
}

bool SessionOptions::GetBoolValue(std::string_view key) const {
  return GetBool(key).value_or(false);
}

std::optional<int> SessionOptions::GetInt(std::string_view key) const {
  std::optional<std::string> value = Get(key);
  if (!value) {
    return std::nullopt;
  }

  int result;
  if (base::StringToInt(*value, &result)) {
    return result;
  }
  LOG(WARNING) << "Unexpected option received " << *value
               << " which cannot be converted to integer.";
  return std::nullopt;
}

std::string SessionOptions::Export() const {
  std::string result;
  for (const auto& pair : options_) {
    if (!result.empty()) {
      result += kSeparator;
    }
    if (!pair.first.empty()) {
      base::StrAppend(
          &result,
          {pair.first, std::string_view(&kKeyValueSeparator, 1), pair.second});
    }
  }
  return result;
}

void SessionOptions::Import(std::string_view parameter) {
  options_.clear();
  std::vector<std::pair<std::string_view, std::string_view>> result;
  base::SplitStringIntoKeyValueViewPairs(parameter, kKeyValueSeparator,
                                         kSeparator, &result);
  for (const auto& pair : result) {
    if (KeyIsValid(pair.first) && ValueIsValid(pair.second)) {
      Append(pair.first, pair.second);
    }
  }
}

}  // namespace remoting
