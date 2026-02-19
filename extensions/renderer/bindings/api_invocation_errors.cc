// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/bindings/api_invocation_errors.h"

#include <string_view>
#include <vector>

#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"

namespace extensions {
namespace api_errors {

constexpr char kTypeString[] = "string";
constexpr char kTypeDouble[] = "number";
constexpr char kTypeBoolean[] = "boolean";
constexpr char kTypeInteger[] = "integer";
constexpr char kTypeObject[] = "object";
constexpr char kTypeList[] = "array";
constexpr char kTypeBinary[] = "binary";
constexpr char kTypeFunction[] = "function";
constexpr char kTypeUndefined[] = "undefined";
constexpr char kTypeNull[] = "null";
constexpr char kTypeAny[] = "any";

std::string InvalidEnumValue(const std::set<std::string>& valid_enums) {
  std::vector<std::string_view> options(valid_enums.begin(), valid_enums.end());
  std::string options_str = base::JoinString(options, ", ");
  return base::StringPrintf("Value must be one of %s.", options_str);
}

std::string MissingRequiredProperty(std::string_view property_name) {
  return base::StringPrintf("Missing required property '%s'.", property_name);
}

std::string UnexpectedProperty(std::string_view property_name) {
  return base::StringPrintf("Unexpected property: '%s'.", property_name);
}

std::string TooFewArrayItems(int minimum, int found) {
  return base::StringPrintf("Array must have at least %d items; found %d.",
                            minimum, found);
}

std::string TooManyArrayItems(int maximum, int found) {
  return base::StringPrintf("Array must have at most %d items; found %d.",
                            maximum, found);
}

std::string TooFewStringChars(int minimum, int found) {
  return base::StringPrintf(
      "String must have at least %d characters; found %d.", minimum, found);
}

std::string TooManyStringChars(int maximum, int found) {
  return base::StringPrintf("String must have at most %d characters; found %d.",
                            maximum, found);
}

std::string NumberTooSmall(int minimum) {
  return base::StringPrintf("Value must be at least %d.", minimum);
}

std::string NumberTooLarge(int maximum) {
  return base::StringPrintf("Value must be at most %d.", maximum);
}

std::string NumberIsNaNOrInfinity() {
  return "Value must not be NaN or Infinity.";
}

std::string InvalidType(std::string_view expected_type,
                        std::string_view actual_type) {
  return base::StringPrintf("Invalid type: expected %s, found %s.",
                            expected_type, actual_type);
}

std::string NotAnInstance(std::string_view instance_type) {
  return base::StringPrintf("Value must be an instance of %s.", instance_type);
}

std::string_view InvalidChoice() {
  return "Value did not match any choice.";
}

std::string_view UnserializableValue() {
  return "Value is unserializable.";
}

std::string_view ScriptThrewError() {
  return "Script threw an error.";
}

std::string_view TooManyArguments() {
  return "Too many arguments.";
}

std::string_view NoMatchingSignature() {
  return "No matching signature.";
}

std::string MissingRequiredArgument(std::string_view argument_name) {
  return base::StringPrintf("Missing required argument '%s'.", argument_name);
}

std::string IndexError(uint32_t index, std::string_view error) {
  return base::StringPrintf("Error at index %u: %s", index, error);
}

std::string PropertyError(std::string_view property_name,
                          std::string_view error) {
  return base::StringPrintf("Error at property '%s': %s", property_name, error);
}

std::string ArgumentError(std::string_view parameter_name,
                          std::string_view error) {
  return base::StringPrintf("Error at parameter '%s': %s", parameter_name,
                            error);
}

std::string InvocationError(std::string_view method_name,
                            std::string_view expected_signature,
                            std::string_view error) {
  return base::StringPrintf("Error in invocation of %s(%s): %s", method_name,
                            expected_signature, error);
}

}  // namespace api_errors
}  // namespace extensions
