/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/platform/bindings/exception_messages.h"

#include "third_party/blink/renderer/platform/wtf/decimal.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"

namespace blink {

String ExceptionMessages::FailedToConvertJSValue(const char* type) {
  return String::Format("Failed to convert value to '%s'.", type);
}

String ExceptionMessages::FailedToConstruct(const char* type,
                                            const String& detail) {
  return "Failed to construct '" + String(type) +
         (!detail.IsEmpty() ? String("': " + detail) : String("'"));
}

String ExceptionMessages::FailedToEnumerate(const char* type,
                                            const String& detail) {
  return "Failed to enumerate the properties of '" + String(type) +
         (!detail.IsEmpty() ? String("': " + detail) : String("'"));
}

String ExceptionMessages::FailedToExecute(const char* method,
                                          const char* type,
                                          const String& detail) {
  return "Failed to execute '" + String(method) + "' on '" + String(type) +
         (!detail.IsEmpty() ? String("': " + detail) : String("'"));
}

String ExceptionMessages::FailedToGet(const char* property,
                                      const char* type,
                                      const String& detail) {
  return "Failed to read the '" + String(property) + "' property from '" +
         String(type) + "': " + detail;
}

String ExceptionMessages::FailedToSet(const char* property,
                                      const char* type,
                                      const String& detail) {
  return "Failed to set the '" + String(property) + "' property on '" +
         String(type) + "': " + detail;
}

String ExceptionMessages::FailedToDelete(const char* property,
                                         const char* type,
                                         const String& detail) {
  return "Failed to delete the '" + String(property) + "' property from '" +
         String(type) + "': " + detail;
}

String ExceptionMessages::FailedToGetIndexed(const char* type,
                                             const String& detail) {
  return "Failed to read an indexed property from '" + String(type) +
         "': " + detail;
}

String ExceptionMessages::FailedToSetIndexed(const char* type,
                                             const String& detail) {
  return "Failed to set an indexed property on '" + String(type) +
         "': " + detail;
}

String ExceptionMessages::FailedToDeleteIndexed(const char* type,
                                                const String& detail) {
  return "Failed to delete an indexed property from '" + String(type) +
         "': " + detail;
}

String ExceptionMessages::ConstructorNotCallableAsFunction(const char* type) {
  return FailedToConstruct(type,
                           "Please use the 'new' operator, this DOM object "
                           "constructor cannot be called as a function.");
}

String ExceptionMessages::IncorrectPropertyType(const String& property,
                                                const String& detail) {
  return "The '" + property + "' property " + detail;
}

String ExceptionMessages::InvalidArity(const char* expected,
                                       unsigned provided) {
  return "Valid arities are: " + String(expected) + ", but " +
         String::Number(provided) + " arguments provided.";
}

String ExceptionMessages::ArgumentNullOrIncorrectType(
    int argument_index,
    const String& expected_type) {
  return "The " + OrdinalNumber(argument_index) +
         " argument provided is either null, or an invalid " + expected_type +
         " object.";
}

String ExceptionMessages::ArgumentNotOfType(int argument_index,
                                            const char* expected_type) {
  return String::Format("parameter %d is not of type '%s'.", argument_index + 1,
                        expected_type);
}

String ExceptionMessages::NotASequenceTypeProperty(
    const String& property_name) {
  return "'" + property_name +
         "' property is neither an array, nor does it have indexed properties.";
}

String ExceptionMessages::NotEnoughArguments(unsigned expected,
                                             unsigned provided) {
  return String::Number(expected) + " argument" + (expected > 1 ? "s" : "") +
         " required, but only " + String::Number(provided) + " present.";
}

String ExceptionMessages::NotAFiniteNumber(double value, const char* name) {
  DCHECK(!std::isfinite(value));
  return String::Format("The %s is %s.", name,
                        std::isinf(value) ? "infinite" : "not a number");
}

String ExceptionMessages::NotAFiniteNumber(const Decimal& value,
                                           const char* name) {
  DCHECK(!value.IsFinite());
  return String::Format("The %s is %s.", name,
                        value.IsInfinity() ? "infinite" : "not a number");
}

String ExceptionMessages::OrdinalNumber(int number) {
  String suffix("th");
  switch (number % 10) {
    case 1:
      if (number % 100 != 11)
        suffix = "st";
      break;
    case 2:
      if (number % 100 != 12)
        suffix = "nd";
      break;
    case 3:
      if (number % 100 != 13)
        suffix = "rd";
      break;
  }
  return String::Number(number) + suffix;
}

String ExceptionMessages::ReadOnly(const char* detail) {
  DEFINE_STATIC_LOCAL(String, read_only, ("This object is read-only."));
  return detail
             ? String::Format("This object is read-only, because %s.", detail)
             : read_only;
}

template <>
String ExceptionMessages::FormatNumber<float>(float number) {
  return FormatPotentiallyNonFiniteNumber(number);
}

template <>
String ExceptionMessages::FormatNumber<double>(double number) {
  return FormatPotentiallyNonFiniteNumber(number);
}

}  // namespace blink
