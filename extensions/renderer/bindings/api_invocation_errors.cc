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

const char kTypeString[] = "string";
const char kTypeDouble[] = "number";
const char kTypeBoolean[] = "boolean";
const char kTypeInteger[] = "integer";
const char kTypeObject[] = "object";
const char kTypeList[] = "array";
const char kTypeBinary[] = "binary";
const char kTypeFunction[] = "function";
const char kTypeUndefined[] = "undefined";
const char kTypeNull[] = "null";
const char kTypeAny[] = "any";

std::string InvalidEnumValue(const std::set<std::string>& valid_enums) {
  std::vector<std::string_view> options(valid_enums.begin(), valid_enums.end());
  std::string options_str = base::JoinString(options, ", ");
  return base::StringPrintf("Value must be one of %s.", options_str.c_str());
}

std::string MissingRequiredProperty(const char* property_name) {
  return base::StringPrintf("Missing required property '%s'.", property_name);
}

std::string UnexpectedProperty(const char* property_name) {
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
  return base::StringPrintf("Value must not be NaN or Infinity.");
}

std::string InvalidType(const char* expected_type, const char* actual_type) {
  return base::StringPrintf("Invalid type: expected %s, found %s.",
                            expected_type, actual_type);
}

std::string NotAnInstance(const char* instance_type) {
  return base::StringPrintf("Value must be an instance of %s.", instance_type);
}

std::string InvalidChoice() {
  return "Value did not match any choice.";
}

std::string UnserializableValue() {
  return "Value is unserializable.";
}

std::string ScriptThrewError() {
  return "Script threw an error.";
}

std::string TooManyArguments() {
  return "Too many arguments.";
}

std::string NoMatchingSignature() {
  return "No matching signature.";
}

std::string MissingRequiredArgument(const char* argument_name) {
  return base::StringPrintf("Missing required argument '%s'.", argument_name);
}

std::string IndexError(uint32_t index, const std::string& error) {
  return base::StringPrintf("Error at index %u: %s", index, error.c_str());
}

std::string PropertyError(const char* property_name, const std::string& error) {
  return base::StringPrintf("Error at property '%s': %s", property_name,
                            error.c_str());
}

std::string ArgumentError(const std::string& parameter_name,
                          const std::string& error) {
  return base::StringPrintf("Error at parameter '%s': %s",
                            parameter_name.c_str(), error.c_str());
}

std::string InvocationError(const std::string& method_name,
                            const std::string& expected_signature,
                            const std::string& error) {
  return base::StringPrintf("Error in invocation of %s(%s): %s",
                            method_name.c_str(), expected_signature.c_str(),
                            error.c_str());
}

}  // namespace api_errors
}  // namespace extensions
