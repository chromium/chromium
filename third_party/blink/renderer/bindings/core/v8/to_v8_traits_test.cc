// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/bindings/core/v8/to_v8_traits.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/core/geometry/dom_point.h"

namespace blink {

namespace {

#define TEST_TOV8_TRAITS(scope, IDLType, expected, value) \
  TestToV8Traits<IDLType>(scope, expected, value, __FILE__, __LINE__)

template <typename IDLType, typename T>
void TestToV8Traits(const V8TestingScope& scope,
                    const String& expected,
                    T value,
                    const char* path,
                    int line_number) {
  v8::Local<v8::Value> actual;
  if (!ToV8Traits<IDLType>::ToV8(scope.GetScriptState(), value)
           .ToLocal(&actual)) {
    ADD_FAILURE_AT(path, line_number) << "ToV8 throws an exception.";
    return;
  }
  String actual_string =
      ToCoreString(actual->ToString(scope.GetContext()).ToLocalChecked());
  if (expected != actual_string) {
    ADD_FAILURE_AT(path, line_number)
        << "ToV8 returns an incorrect value.\n  Actual: "
        << actual_string.Utf8() << "\nExpected: " << expected;
    return;
  }
}

TEST(ToV8TraitsTest, Numeric) {
  const V8TestingScope scope;
  // Test type matching
  // Integer
  TEST_TOV8_TRAITS(scope, IDLByte, "0", static_cast<int8_t>(0));
  TEST_TOV8_TRAITS(scope, IDLByte, "1", static_cast<int8_t>(1));
  TEST_TOV8_TRAITS(scope, IDLByte, "-2", static_cast<int8_t>(-2));
  TEST_TOV8_TRAITS(scope, IDLShort, "0", static_cast<int16_t>(0));
  TEST_TOV8_TRAITS(scope, IDLLong, "0", static_cast<int32_t>(0));
  TEST_TOV8_TRAITS(scope, IDLLongLong, "0", static_cast<int64_t>(0));
  TEST_TOV8_TRAITS(scope, IDLOctet, "0", static_cast<uint8_t>(0));
  TEST_TOV8_TRAITS(scope, IDLUnsignedShort, "0", static_cast<uint16_t>(0));
  TEST_TOV8_TRAITS(scope, IDLUnsignedLong, "0", static_cast<uint32_t>(0));
  TEST_TOV8_TRAITS(scope, IDLUnsignedLongLong, "0", static_cast<uint64_t>(0));
  // [Clamp] Integer
  TEST_TOV8_TRAITS(scope, IDLByteClamp, "0", static_cast<int8_t>(0));
  TEST_TOV8_TRAITS(scope, IDLShortClamp, "0", static_cast<int16_t>(0));
  TEST_TOV8_TRAITS(scope, IDLLongClamp, "0", static_cast<int32_t>(0));
  TEST_TOV8_TRAITS(scope, IDLLongLongClamp, "0", static_cast<int64_t>(0));
  TEST_TOV8_TRAITS(scope, IDLOctetClamp, "0", static_cast<uint8_t>(0));
  TEST_TOV8_TRAITS(scope, IDLUnsignedShortClamp, "0", static_cast<uint16_t>(0));
  TEST_TOV8_TRAITS(scope, IDLUnsignedLongClamp, "0", static_cast<uint32_t>(0));
  TEST_TOV8_TRAITS(scope, IDLUnsignedLongLongClamp, "0",
                   static_cast<uint64_t>(0));
  // [EnforceRange] Integer
  TEST_TOV8_TRAITS(scope, IDLByteEnforceRange, "0", static_cast<int8_t>(0));
  TEST_TOV8_TRAITS(scope, IDLShortEnforceRange, "0", static_cast<int16_t>(0));
  TEST_TOV8_TRAITS(scope, IDLLongEnforceRange, "0", static_cast<int32_t>(0));
  TEST_TOV8_TRAITS(scope, IDLLongLongEnforceRange, "0",
                   static_cast<int64_t>(0));
  TEST_TOV8_TRAITS(scope, IDLOctetEnforceRange, "0", static_cast<uint8_t>(0));
  TEST_TOV8_TRAITS(scope, IDLUnsignedShortEnforceRange, "0",
                   static_cast<uint16_t>(0));
  TEST_TOV8_TRAITS(scope, IDLUnsignedLongEnforceRange, "0",
                   static_cast<uint32_t>(0));
  TEST_TOV8_TRAITS(scope, IDLUnsignedLongLongEnforceRange, "0",
                   static_cast<uint64_t>(0));

  // Test the maximum and the minimum integer in the range
  TEST_TOV8_TRAITS(scope, IDLLong, "-2147483648",
                   std::numeric_limits<int32_t>::min());
  TEST_TOV8_TRAITS(scope, IDLLong, "2147483647",
                   std::numeric_limits<int32_t>::max());
  TEST_TOV8_TRAITS(scope, IDLUnsignedLong, "4294967295",
                   std::numeric_limits<uint32_t>::max());

  // v8::Number can represent exact numbers in [-(2^53-1), 2^53-1].
  TEST_TOV8_TRAITS(scope, IDLLongLong, "-9007199254740991",
                   static_cast<int64_t>(-9007199254740991));  // -(2^53-1)
  TEST_TOV8_TRAITS(scope, IDLLongLong, "9007199254740991",
                   static_cast<int64_t>(9007199254740991));  // 2^53-1
  TEST_TOV8_TRAITS(scope, IDLUnsignedLongLong, "9007199254740991",
                   static_cast<uint64_t>(9007199254740991));  // 2^53-1
}

TEST(ToV8TraitsTest, String) {
  const V8TestingScope scope;
  const String string("string");
  const AtomicString atomic_string("atomicString");
  const char* const charptr_string = "arrayString";
  // ByteString
  TEST_TOV8_TRAITS(scope, IDLByteStringV2, "string", string);
  TEST_TOV8_TRAITS(scope, IDLByteStringV2, "atomicString", atomic_string);
  TEST_TOV8_TRAITS(scope, IDLByteStringV2, "arrayString", charptr_string);
  // DOMString
  TEST_TOV8_TRAITS(scope, IDLStringV2, "string", string);
  TEST_TOV8_TRAITS(scope, IDLStringV2, "atomicString", atomic_string);
  TEST_TOV8_TRAITS(scope, IDLStringV2, "arrayString", charptr_string);
  TEST_TOV8_TRAITS(scope, IDLStringTreatNullAsEmptyStringV2, "string", string);
  TEST_TOV8_TRAITS(scope, IDLStringTreatNullAsEmptyStringV2, "atomicString",
                   atomic_string);
  TEST_TOV8_TRAITS(scope, IDLStringTreatNullAsEmptyStringV2, "arrayString",
                   charptr_string);
  // USVString
  TEST_TOV8_TRAITS(scope, IDLUSVStringV2, "string", string);
  TEST_TOV8_TRAITS(scope, IDLUSVStringV2, "atomicString", atomic_string);
  TEST_TOV8_TRAITS(scope, IDLUSVStringV2, "arrayString", charptr_string);
  // [StringContext=TrustedHTML] DOMString
  TEST_TOV8_TRAITS(scope, IDLStringStringContextTrustedHTMLV2, "string",
                   string);
  TEST_TOV8_TRAITS(scope, IDLStringStringContextTrustedHTMLV2, "atomicString",
                   atomic_string);
  TEST_TOV8_TRAITS(scope, IDLStringStringContextTrustedHTMLV2, "arrayString",
                   charptr_string);
  TEST_TOV8_TRAITS(scope,
                   IDLStringStringContextTrustedHTMLTreatNullAsEmptyStringV2,
                   "string", string);
  TEST_TOV8_TRAITS(scope,
                   IDLStringStringContextTrustedHTMLTreatNullAsEmptyStringV2,
                   "atomicString", atomic_string);
  TEST_TOV8_TRAITS(scope,
                   IDLStringStringContextTrustedHTMLTreatNullAsEmptyStringV2,
                   "arrayString", charptr_string);
  // [StringContext=TrustedScript] DOMString
  TEST_TOV8_TRAITS(scope, IDLStringStringContextTrustedScriptV2, "string",
                   string);
  TEST_TOV8_TRAITS(scope, IDLStringStringContextTrustedScriptV2, "atomicString",
                   atomic_string);
  TEST_TOV8_TRAITS(scope, IDLStringStringContextTrustedScriptV2, "arrayString",
                   charptr_string);
  TEST_TOV8_TRAITS(scope,
                   IDLStringStringContextTrustedScriptTreatNullAsEmptyStringV2,
                   "string", string);
  TEST_TOV8_TRAITS(scope,
                   IDLStringStringContextTrustedScriptTreatNullAsEmptyStringV2,
                   "atomicString", atomic_string);
  TEST_TOV8_TRAITS(scope,
                   IDLStringStringContextTrustedScriptTreatNullAsEmptyStringV2,
                   "arrayString", charptr_string);
  // [StringContext=TrustedScriptURL] USVString
  TEST_TOV8_TRAITS(scope, IDLUSVStringStringContextTrustedScriptURLV2, "string",
                   string);
  TEST_TOV8_TRAITS(scope, IDLUSVStringStringContextTrustedScriptURLV2,
                   "atomicString", atomic_string);
  TEST_TOV8_TRAITS(scope, IDLUSVStringStringContextTrustedScriptURLV2,
                   "arrayString", charptr_string);
}

TEST(ToV8TraitsTest, EmptyString) {
  const V8TestingScope scope;
  const String empty_string("");
  TEST_TOV8_TRAITS(scope, IDLStringV2, "", empty_string);
  const AtomicString empty_atomic_string("");
  TEST_TOV8_TRAITS(scope, IDLStringV2, "", empty_atomic_string);
  const char* const empty = "";
  TEST_TOV8_TRAITS(scope, IDLStringV2, "", empty);
}

TEST(ToV8TraitsTest, NullStringInputForNoneNullableType) {
  const V8TestingScope scope;
  const String null_string;
  TEST_TOV8_TRAITS(scope, IDLStringV2, "", null_string);
  const AtomicString null_atomic_string;
  TEST_TOV8_TRAITS(scope, IDLStringV2, "", null_atomic_string);
  const char* const null = nullptr;
  TEST_TOV8_TRAITS(scope, IDLStringV2, "", null);
}

}  // namespace

}  // namespace blink
