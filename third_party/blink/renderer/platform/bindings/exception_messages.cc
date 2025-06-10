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

#include "base/notreached.h"
#include "third_party/blink/renderer/platform/bindings/exception_context.h"
#include "third_party/blink/renderer/platform/bindings/v8_binding.h"
#include "third_party/blink/renderer/platform/wtf/decimal.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/strcat.h"

namespace blink {

namespace {

String OptionalNameProperty(const String& property) {
  if (property.empty()) {
    return String();
  }
  return StrCat({" '", property, "'"});
}

String OptionalIndexProperty(const String& property) {
  if (!property) {
    return String();
  }
  return StrCat({" [", property, "]"});
}

}  //  namespace

String ExceptionMessages::AddContextToMessage(v8::ExceptionContext type,
                                              const char* class_name,
                                              const String& property_name,
                                              const String& message) {
  switch (type) {
    case v8::ExceptionContext::kConstructor:
      return ExceptionMessages::FailedToConstruct(class_name, message);
    case v8::ExceptionContext::kOperation:
      return ExceptionMessages::FailedToExecute(property_name, class_name,
                                                message);
    case v8::ExceptionContext::kAttributeGet:
      return ExceptionMessages::FailedToGet(property_name, class_name, message);
    case v8::ExceptionContext::kAttributeSet:
      return ExceptionMessages::FailedToSet(property_name, class_name, message);
    case v8::ExceptionContext::kNamedEnumerator:
      return ExceptionMessages::FailedToEnumerate(class_name, message);
    case v8::ExceptionContext::kIndexedGetter:
    case v8::ExceptionContext::kIndexedDescriptor:
    case v8::ExceptionContext::kIndexedQuery:
      return ExceptionMessages::FailedToGetIndexed(property_name, class_name,
                                                   message);
    case v8::ExceptionContext::kIndexedSetter:
    case v8::ExceptionContext::kIndexedDefiner:
      return ExceptionMessages::FailedToSetIndexed(property_name, class_name,
                                                   message);
    case v8::ExceptionContext::kIndexedDeleter:
      return ExceptionMessages::FailedToDeleteIndexed(property_name, class_name,
                                                      message);
    case v8::ExceptionContext::kNamedGetter:
    case v8::ExceptionContext::kNamedDescriptor:
    case v8::ExceptionContext::kNamedQuery:
      return ExceptionMessages::FailedToGetNamed(property_name, class_name,
                                                 message);
    case v8::ExceptionContext::kNamedSetter:
    case v8::ExceptionContext::kNamedDefiner:
      return ExceptionMessages::FailedToSetNamed(property_name, class_name,
                                                 message);
    case v8::ExceptionContext::kNamedDeleter:
      return ExceptionMessages::FailedToDeleteNamed(property_name, class_name,
                                                    message);
    case v8::ExceptionContext::kUnknown:
      return message;
  }
  NOTREACHED();
}

String ExceptionMessages::FailedToConvertJSValue(const char* type) {
  return StrCat({"Failed to convert value to '", type, "'."});
}

String ExceptionMessages::FailedToConstruct(const char* type,
                                            const String& detail) {
  String type_string = String(type);
  if (type_string.empty()) {
    return detail;
  }
  if (detail.empty()) {
    return StrCat({"Failed to construct '", type_string, "'"});
  }
  return StrCat({"Failed to construct '", type_string, "': ", detail});
}

String ExceptionMessages::FailedToEnumerate(const char* type,
                                            const String& detail) {
  if (detail.empty()) {
    return StrCat({"Failed to enumerate the properties of '", type, "'"});
  }
  return StrCat(
      {"Failed to enumerate the properties of '", type, "': ", detail});
}

String ExceptionMessages::FailedToExecute(const String& method,
                                          const char* type,
                                          const String& detail) {
  if (detail.empty()) {
    return StrCat({"Failed to execute '", method, "' on '", type, "'"});
  }
  return StrCat({"Failed to execute '", method, "' on '", type, "': ", detail});
}

String ExceptionMessages::FailedToGet(const String& property,
                                      const char* type,
                                      const String& detail) {
  return StrCat({"Failed to read the '", property, "' property from '", type,
                 "': ", detail});
}

String ExceptionMessages::FailedToSet(const String& property,
                                      const char* type,
                                      const String& detail) {
  return StrCat({"Failed to set the '", property, "' property on '", type,
                 "': ", detail});
}

String ExceptionMessages::FailedToDelete(const String& property,
                                         const char* type,
                                         const String& detail) {
  return StrCat({"Failed to delete the '", property, "' property from '", type,
                 "': ", detail});
}

String ExceptionMessages::FailedToGetIndexed(const String& property,
                                             const char* type,
                                             const String& detail) {
  return StrCat({"Failed to read an indexed property",
                 OptionalIndexProperty(property), " from '", type,
                 "': ", detail});
}

String ExceptionMessages::FailedToSetIndexed(const String& property,
                                             const char* type,
                                             const String& detail) {
  return StrCat({"Failed to set an indexed property",
                 OptionalIndexProperty(property), " on '", type,
                 "': ", detail});
}

String ExceptionMessages::FailedToDeleteIndexed(const String& property,
                                                const char* type,
                                                const String& detail) {
  return StrCat({"Failed to delete an indexed property",
                 OptionalIndexProperty(property), " from '", type,
                 "': ", detail});
}

String ExceptionMessages::FailedToGetNamed(const String& property,
                                           const char* type,
                                           const String& detail) {
  return StrCat({"Failed to read a named property",
                 OptionalNameProperty(property), " from '", type,
                 "': ", detail});
}

String ExceptionMessages::FailedToSetNamed(const String& property,
                                           const char* type,
                                           const String& detail) {
  return StrCat({"Failed to set a named property",
                 OptionalNameProperty(property), " on '", type, "': ", detail});
}

String ExceptionMessages::FailedToDeleteNamed(const String& property,
                                              const char* type,
                                              const String& detail) {
  return StrCat({"Failed to delete a named property",
                 OptionalNameProperty(property), " from '", type,
                 "': ", detail});
}

String ExceptionMessages::ConstructorNotCallableAsFunction(const char* type) {
  return FailedToConstruct(type,
                           "Please use the 'new' operator, this DOM object "
                           "constructor cannot be called as a function.");
}

String ExceptionMessages::ConstructorCalledAsFunction() {
  return (
      "Please use the 'new' operator, this DOM object constructor cannot "
      "be called as a function.");
}

String ExceptionMessages::IncorrectPropertyType(const String& property,
                                                const String& detail) {
  return StrCat({"The '", property, "' property ", detail});
}

String ExceptionMessages::InvalidArity(const char* expected,
                                       unsigned provided) {
  return StrCat({"Valid arities are: ", expected, ", but ",
                 String::Number(provided), " arguments provided."});
}

String ExceptionMessages::ArgumentNullOrIncorrectType(
    int argument_index,
    const String& expected_type) {
  return StrCat({"The ", OrdinalNumber(argument_index),
                 " argument provided is either null, or an invalid ",
                 expected_type, " object."});
}

String ExceptionMessages::ArgumentNotOfType(int argument_index,
                                            const char* expected_type) {
  return String::Format("parameter %d is not of type '%s'.", argument_index + 1,
                        expected_type);
}

String ExceptionMessages::NotASequenceTypeProperty(
    const String& property_name) {
  return StrCat(
      {"'", property_name,
       "' property is neither an array, nor does it have indexed properties."});
}

String ExceptionMessages::NotEnoughArguments(unsigned expected,
                                             unsigned provided) {
  return StrCat({String::Number(expected), " argument", expected > 1 ? "s" : "",
                 " required, but only ", String::Number(provided),
                 " present."});
}

String ExceptionMessages::NotAFiniteNumber(double value, const char* name) {
  DCHECK(!std::isfinite(value));
  return StrCat({"The ", name, " is ",
                 std::isinf(value) ? "infinite." : "not a number."});
}

String ExceptionMessages::NotAFiniteNumber(const Decimal& value,
                                           const char* name) {
  DCHECK(!value.IsFinite());
  return StrCat({"The ", name, " is ",
                 value.IsInfinity() ? "infinite." : "not a number."});
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
  return StrCat({String::Number(number), suffix});
}

String ExceptionMessages::IndexExceedsMaximumBound(const char* name,
                                                   bool eq,
                                                   const String& given,
                                                   const String& bound) {
  return StrCat({"The ", name, " provided (", given, ") is greater than ",
                 eq ? "or equal to " : "", "the maximum bound (", bound, ")."});
}

String ExceptionMessages::IndexExceedsMinimumBound(const char* name,
                                                   bool eq,
                                                   const String& given,
                                                   const String& bound) {
  return StrCat({"The ", name, " provided (", given, ") is less than ",
                 eq ? "or equal to " : "", "the minimum bound (", bound, ")."});
}

String ExceptionMessages::IndexOutsideRange(const char* name,
                                            const String& given,
                                            const String& lower_bound,
                                            BoundType lower_type,
                                            const String& upper_bound,
                                            BoundType upper_type) {
  return StrCat({"The ", name, " provided (", given, ") is outside the range ",
                 lower_type == kExclusiveBound ? "(" : "[", lower_bound, ", ",
                 upper_bound, upper_type == kExclusiveBound ? ")." : "]."});
}

String ExceptionMessages::ReadOnly(const char* detail) {
  DEFINE_STATIC_LOCAL(String, read_only, ("This object is read-only."));
  return detail ? StrCat({"This object is read-only, because ", detail, "."})
                : read_only;
}

String ExceptionMessages::SharedArrayBufferNotAllowed(
    const char* expected_type) {
  return StrCat({"The provided ", expected_type, " value must not be shared."});
}

String ExceptionMessages::ResizableArrayBufferNotAllowed(
    const char* expected_type) {
  return StrCat(
      {"The provided ", expected_type, " value must not be resizable."});
}

String ExceptionMessages::ValueNotOfType(const char* expected_type) {
  return StrCat({"The provided value is not of type '", expected_type, "'."});
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
