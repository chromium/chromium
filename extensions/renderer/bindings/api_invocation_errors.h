// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_BINDINGS_API_INVOCATION_ERRORS_H_
#define EXTENSIONS_RENDERER_BINDINGS_API_INVOCATION_ERRORS_H_

#include <cstdint>
#include <set>
#include <string>
#include <string_view>

// A collection of error-related strings and utilities for parsing API
// invocations.
namespace extensions {
namespace api_errors {

// Strings for the expected types.
extern const char kTypeString[];
extern const char kTypeDouble[];
extern const char kTypeBoolean[];
extern const char kTypeInteger[];
extern const char kTypeObject[];
extern const char kTypeList[];
extern const char kTypeBinary[];
extern const char kTypeFunction[];
extern const char kTypeUndefined[];
extern const char kTypeNull[];
extern const char kTypeAny[];

// Methods to return a formatted string describing an error related to argument
// parsing.
std::string InvalidEnumValue(const std::set<std::string>& valid_enums);
std::string MissingRequiredProperty(std::string_view property_name);
std::string UnexpectedProperty(std::string_view property_name);
std::string TooFewArrayItems(int minimum, int found);
std::string TooManyArrayItems(int maximum, int found);
std::string TooFewStringChars(int minimum, int found);
std::string TooManyStringChars(int maximum, int found);
std::string NumberTooSmall(int minimum);
std::string NumberTooLarge(int maximum);
std::string NumberIsNaNOrInfinity();
std::string InvalidType(std::string_view expected_type,
                        std::string_view actual_type);
std::string NotAnInstance(std::string_view instance_type);
std::string_view InvalidChoice();
std::string_view UnserializableValue();
std::string_view ScriptThrewError();
std::string_view TooManyArguments();
std::string MissingRequiredArgument(std::string_view argument_name);
std::string_view NoMatchingSignature();

// Returns an message indicating an error was found while parsing a given index
// in an array.
std::string IndexError(uint32_t index, std::string_view error);

// Returns a message indicating that an error was found while parsing a given
// property on an object.
std::string PropertyError(std::string_view property_name,
                          std::string_view error);

// Returns a message indicating that an error was found while parsing a given
// parameter in an API signature.
std::string ArgumentError(std::string_view parameter_name,
                          std::string_view error);

// Returns a message indicating that an API method was called with an invalid
// signature.
std::string InvocationError(std::string_view method_name,
                            std::string_view expected_signature,
                            std::string_view error);

}  // namespace api_errors
}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_BINDINGS_API_INVOCATION_ERRORS_H_
