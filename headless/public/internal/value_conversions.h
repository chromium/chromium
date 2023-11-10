// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HEADLESS_PUBLIC_INTERNAL_VALUE_CONVERSIONS_H_
#define HEADLESS_PUBLIC_INTERNAL_VALUE_CONVERSIONS_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/values.h"
#include "headless/public/util/error_reporter.h"

namespace headless {
namespace internal {

// Generic conversion from a type to a base::Value. Implemented below
// (for composite and low level types) and and in types_DOMAIN.cc.
template <typename T>
base::Value ToValue(const T& value);

// Generic conversion from a base::Value to a type. Note that this generic
// variant is never defined. Instead, we declare a specific template
// specialization for all the used types.
template <typename T>
struct FromValue {
  static std::unique_ptr<T> Parse(const base::Value& value,
                                  ErrorReporter* errors);
};

template <>
inline base::Value ToValue(const int& value) {
  return base::Value(value);
}

template <>
inline base::Value ToValue(const double& value) {
  return base::Value(value);
}

template <>
inline base::Value ToValue(const bool& value) {
  return base::Value(value);
}

template <>
inline base::Value ToValue(const std::string& value) {
  return base::Value(value);
}

template <>
inline base::Value ToValue(const base::Value& value) {
  return value.Clone();
}

template <>
inline base::Value ToValue(const base::Value::Dict& value) {
  return base::Value(value.Clone());
}

// Note: Order of the two templates below is important to handle
// vectors of unique_ptr.
template <typename T>
base::Value ToValue(const std::unique_ptr<T>& value) {
  return ToValue(*value);
}

template <typename T>
base::Value ToValue(const std::vector<T>& vector_of_values) {
  base::Value::List result;
  for (const T& value : vector_of_values)
    result.Append(ToValue(value));
  return base::Value(std::move(result));
}

template <>
inline base::Value ToValue(const protocol::Binary& value) {
  return ToValue(value.toBase64());
}

// FromValue specializations for basic types.
template <>
struct FromValue<bool> {
  static bool Parse(const base::Value& value, ErrorReporter* errors) {
    if (!value.is_bool()) {
      errors->AddError("boolean value expected");
      return false;
    }
    return value.GetBool();
  }
};

template <>
struct FromValue<int> {
  static int Parse(const base::Value& value, ErrorReporter* errors) {
    if (!value.is_int()) {
      errors->AddError("integer value expected");
      return 0;
    }
    return value.GetInt();
  }
};

template <>
struct FromValue<double> {
  static double Parse(const base::Value& value, ErrorReporter* errors) {
    if (!value.is_double() && !value.is_int()) {
      errors->AddError("double value expected");
      return 0;
    }
    return value.GetDouble();
  }
};

template <>
struct FromValue<std::string> {
  static std::string Parse(const base::Value& value, ErrorReporter* errors) {
    if (!value.is_string()) {
      errors->AddError("string value expected");
      return "";
    }
    return value.GetString();
  }
};

template <>
struct FromValue<base::Value::Dict> {
  static std::optional<base::Value::Dict> Parse(const base::Value& value,
                                                ErrorReporter* errors) {
    if (!value.is_dict()) {
      errors->AddError("dictionary value expected");
      return std::nullopt;
    }
    return value.GetDict().Clone();
  }
};

template <>
struct FromValue<base::Value> {
  static std::unique_ptr<base::Value> Parse(const base::Value& value,
                                            ErrorReporter* errors) {
    return base::Value::ToUniquePtrValue(value.Clone());
  }
};

template <typename T>
struct FromValue<std::unique_ptr<T>> {
  static std::unique_ptr<T> Parse(const base::Value& value,
                                  ErrorReporter* errors) {
    return FromValue<T>::Parse(value, errors);
  }
};

template <>
struct FromValue<protocol::Binary> {
  static protocol::Binary Parse(const base::Value& value,
                                ErrorReporter* errors) {
    if (!value.is_string()) {
      errors->AddError("string value expected");
      return protocol::Binary();
    }
    bool success = false;
    protocol::Binary out =
        protocol::Binary::fromBase64(value.GetString(), &success);
    if (!success)
      errors->AddError("base64 decoding error");
    return out;
  }
};

template <typename T>
struct FromValue<std::vector<T>> {
  static std::vector<T> Parse(const base::Value& value, ErrorReporter* errors) {
    std::vector<T> result;
    if (!value.is_list()) {
      errors->AddError("list value expected");
      return result;
    }
    errors->Push();
    for (const auto& item : value.GetList())
      result.push_back(FromValue<T>::Parse(item, errors));
    errors->Pop();
    return result;
  }
};

}  // namespace internal
}  // namespace headless

#endif  // HEADLESS_PUBLIC_INTERNAL_VALUE_CONVERSIONS_H_
