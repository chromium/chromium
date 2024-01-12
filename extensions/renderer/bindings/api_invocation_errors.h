// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_BINDINGS_API_INVOCATION_ERRORS_H_
#define EXTENSIONS_RENDERER_BINDINGS_API_INVOCATION_ERRORS_H_

#include <cstdint>
#include <set>
#include <string>

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
std::string MissingRequiredProperty(const char* property_name);
std::string UnexpectedProperty(const char* property_name);
std::string TooFewArrayItems(int minimum, int found);
std::string TooManyArrayItems(int maximum, int found);
std::string TooFewStringChars(int minimum, int found);
std::string TooManyStringChars(int maximum, int found);
std::string NumberTooSmall(int minimum);
std::string NumberTooLarge(int maximum);
std::string NumberIsNaNOrInfinity();
std::string InvalidType(const char* expected_type, const char* actual_type);
std::string NotAnInstance(const char* instance_type);
std::string InvalidChoice();
std::string UnserializableValue();
std::string ScriptThrewError();
std::string TooManyArguments();
std::string MissingRequiredArgument(const char* argument_name);
std::string NoMatchingSignature();

// Returns an message indicating an error was found while parsing a given index
// in an array.
std::string IndexError(uint32_t index, const std::string& error);

// Returns a message indicating that an error was found while parsing a given
// property on an object.
std::string PropertyError(const char* property_name, const std::string& error);

// Returns a message indicating that an error was found while parsing a given
// parameter in an API signature.
std::string ArgumentError(const std::string& parameter_name,
                          const std::string& error);

// Returns a message indicating that an API method was called with an invalid
// signature.
std::string InvocationError(const std::string& method_name,
                            const std::string& expected_signature,
                            const std::string& error);

}  // namespace api_errors
}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_BINDINGS_API_INVOCATION_ERRORS_H_
