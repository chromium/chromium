// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/tests/test_var_deprecated.h"

#include <stdint.h>
#include <string.h>

#include <limits>

#include "ppapi/c/dev/ppb_var_deprecated.h"
#include "ppapi/c/pp_var.h"
#include "ppapi/cpp/dev/scriptable_object_deprecated.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/private/var_private.h"
#include "ppapi/cpp/var.h"
#include "ppapi/tests/testing_instance.h"

namespace {

uint32_t kInvalidLength = static_cast<uint32_t>(-1);

static const char kSetValueFunction[] = "SetValue";

// ScriptableObject used by the var tests.
class VarScriptableObject : public pp::deprecated::ScriptableObject {
 public:
  explicit VarScriptableObject(TestVarDeprecated* v)
      : test_var_deprecated_(v) {}

  // pp::deprecated::ScriptableObject overrides.
  bool HasMethod(const pp::Var& name, pp::Var* exception);
  pp::Var Call(const pp::Var& name,
               const std::vector<pp::Var>& args,
               pp::Var* exception);

 private:
  TestVarDeprecated* test_var_deprecated_;
};

bool VarScriptableObject::HasMethod(const pp::Var& name, pp::Var* exception) {
  if (!name.is_string())
    return false;
  return name.AsString() == kSetValueFunction;
}

pp::Var VarScriptableObject::Call(const pp::Var& method_name,
                                  const std::vector<pp::Var>& args,
                                  pp::Var* exception) {
  if (!method_name.is_string())
    return false;
  std::string name = method_name.AsString();

  if (name == kSetValueFunction) {
    if (args.size() != 1)
      *exception = pp::Var("Bad argument to SetValue(<value>)");
    else
      test_var_deprecated_->set_var_from_page(pp::VarPrivate(args[0]));
  }

  return pp::Var();
}

}  // namespace

REGISTER_TEST_CASE(VarDeprecated);

bool TestVarDeprecated::Init() {
  var_interface_ = static_cast<const PPB_Var_Deprecated*>(
      pp::Module::Get()->GetBrowserInterface(PPB_VAR_DEPRECATED_INTERFACE));
  return var_interface_ && CheckTestingInterface();
}

void TestVarDeprecated::RunTests(const std::string& filter) {
  RUN_TEST(BasicString, filter);
  RUN_TEST(InvalidAndEmpty, filter);
  RUN_TEST(InvalidUtf8, filter);
  RUN_TEST(NullInputInUtf8Conversion, filter);
  RUN_TEST(ValidUtf8, filter);
  RUN_TEST(Utf8WithEmbeddedNulls, filter);
  RUN_TEST(VarToUtf8ForWrongType, filter);
  RUN_TEST(HasPropertyAndMethod, filter);
  RUN_TEST(PassReference, filter);
}

pp::deprecated::ScriptableObject* TestVarDeprecated::CreateTestObject() {
  return new VarScriptableObject(this);
}

std::string TestVarDeprecated::TestBasicString() {
  uint32_t before_object = testing_interface_->GetLiveObjectsForInstance(
      instance_->pp_instance());
  {
    const char kStr[] = "Hello";
    const uint32_t kStrLen(sizeof(kStr) - 1);
    PP_Var str = var_interface_->VarFromUtf8(pp::Module::Get()->pp_module(),
                                             kStr, kStrLen);
    ASSERT_EQ(PP_VARTYPE_STRING, str.type);

    // Reading back the string should work.
    uint32_t len = 0;
    const char* result = var_interface_->VarToUtf8(str, &len);
    ASSERT_EQ(kStrLen, len);
    ASSERT_EQ(0, strncmp(kStr, result, kStrLen));

    // Destroy the string, readback should now fail.
    var_interface_->Release(str);
    result = var_interface_->VarToUtf8(str, &len);
    ASSERT_EQ(0, len);
    ASSERT_EQ(NULL, result);
  }

  // Make sure nothing leaked.
  ASSERT_TRUE(testing_interface_->GetLiveObjectsForInstance(
      instance_->pp_instance()) == before_object);

  PASS();
}

std::string TestVarDeprecated::TestInvalidAndEmpty() {
  PP_Var invalid_string;
  invalid_string.type = PP_VARTYPE_STRING;
  invalid_string.value.as_id = 31415926;

  // Invalid strings should give NULL as the return value.
  uint32_t len = std::numeric_limits<uint32_t>::max();
  const char* result = var_interface_->VarToUtf8(invalid_string, &len);
  ASSERT_EQ(0, len);
  ASSERT_EQ(NULL, result);

  // Same with vars that are not strings.
  len = std::numeric_limits<uint32_t>::max();
  pp::Var int_var(42);
  result = var_interface_->VarToUtf8(int_var.pp_var(), &len);
  ASSERT_EQ(0, len);
  ASSERT_EQ(NULL, result);

  // Empty strings should return non-NULL.
  pp::Var empty_string("");
  len = std::numeric_limits<uint32_t>::max();
  result = var_interface_->VarToUtf8(empty_string.pp_var(), &len);
  ASSERT_EQ(0, len);
  ASSERT_NE(NULL, result);

  PASS();
}

std::string TestVarDeprecated::TestInvalidUtf8() {
  // utf8じゃない (japanese for "is not utf8") in shift-jis encoding.
  static const char kSjisString[] = "utf8\x82\xb6\x82\xe1\x82\xc8\x82\xa2";
  pp::Var sjis(kSjisString);
  if (!sjis.is_null())
    return "Non-UTF8 string was permitted erroneously.";

  PASS();
}

std::string TestVarDeprecated::TestNullInputInUtf8Conversion() {
  // This test talks directly to the C interface to access edge cases that
  // cannot be exercised via the C++ interface.
  PP_Var converted_string;

  // 0-length string should not dereference input string, and should produce
  // an empty string.
  converted_string = var_interface_->VarFromUtf8(
      pp::Module::Get()->pp_module(), NULL, 0);
  if (converted_string.type != PP_VARTYPE_STRING) {
    return "Expected 0 length to return empty string.";
  }

  // Now convert it back.
  uint32_t length = kInvalidLength;
  const char* result = NULL;
  result = var_interface_->VarToUtf8(converted_string, &length);
  if (length != 0) {
    return "Expected 0 length string on conversion.";
  }
  if (result == NULL) {
    return "Expected a non-null result for 0-lengthed string from VarToUtf8.";
  }
  var_interface_->Release(converted_string);

  // Should not crash, and make an empty string.
  const char* null_string = NULL;
  pp::Var null_var(null_string);
  if (!null_var.is_string() || !null_var.AsString().empty()) {
    return "Expected NULL input to make an empty string Var.";
  }

  PASS();
}

std::string TestVarDeprecated::TestValidUtf8() {
  // From UTF8 string -> PP_Var.
  // Chinese for "I am utf8."
  static const char kValidUtf8[] = "\xe6\x88\x91\xe6\x98\xafutf8.";
  pp::Var converted_string(kValidUtf8);

  if (converted_string.is_null())
    return "Unable to convert valid utf8 to var.";

  // Since we're already here, test PP_Var back to UTF8 string.
  std::string returned_string = converted_string.AsString();

  // We need to check against 1 less than sizeof because the resulting string
  // is technically not NULL terminated by API design.
  if (returned_string.size() != sizeof(kValidUtf8) - 1) {
    return "Unable to convert utf8 string back from var.";
  }
  if (returned_string != kValidUtf8) {
    return "String mismatches on conversion back from PP_Var.";
  }

  PASS();
}

std::string TestVarDeprecated::TestUtf8WithEmbeddedNulls() {
  // From UTF8 string with embedded nulls -> PP_Var.
  // Chinese for "also utf8."
  static const char kUtf8WithEmbededNull[] = "\xe6\xb9\x9f\xe6\x98\xaf\0utf8.";
  std::string orig_string(kUtf8WithEmbededNull,
                          sizeof(kUtf8WithEmbededNull) -1);
  pp::Var converted_string(orig_string);

  if (converted_string.is_null())
    return "Unable to convert utf8 with embedded nulls to var.";

  // Since we're already here, test PP_Var back to UTF8 string.
  std::string returned_string = converted_string.AsString();

  if (returned_string.size() != orig_string.size()) {
    return "Unable to convert utf8 with embedded nulls back from var.";
  }
  if (returned_string != orig_string) {
    return "String mismatches on conversion back from PP_Var.";
  }

  PASS();
}

std::string TestVarDeprecated::TestVarToUtf8ForWrongType() {
  uint32_t length = kInvalidLength;
  const char* result = NULL;
  result = var_interface_->VarToUtf8(PP_MakeUndefined(), &length);
  if (length != 0) {
    return "Expected 0 on string conversion from Void var.";
  }
  if (result != NULL) {
    return "Expected NULL on string conversion from Void var.";
  }

  length = kInvalidLength;
  result = NULL;
  result = var_interface_->VarToUtf8(PP_MakeNull(), &length);
  if (length != 0) {
    return "Expected 0 on string conversion from Null var.";
  }
  if (result != NULL) {
    return "Expected NULL on string conversion from Null var.";
  }

  length = kInvalidLength;
  result = NULL;
  result = var_interface_->VarToUtf8(PP_MakeBool(PP_TRUE), &length);
  if (length != 0) {
    return "Expected 0 on string conversion from Bool var.";
  }
  if (result != NULL) {
    return "Expected NULL on string conversion from Bool var.";
  }

  length = kInvalidLength;
  result = NULL;
  result = var_interface_->VarToUtf8(PP_MakeInt32(1), &length);
  if (length != 0) {
    return "Expected 0 on string conversion from Int32 var.";
  }
  if (result != NULL) {
    return "Expected NULL on string conversion from Int32 var.";
  }

  length = kInvalidLength;
  result = NULL;
  result = var_interface_->VarToUtf8(PP_MakeDouble(1.0), &length);
  if (length != 0) {
    return "Expected 0 on string conversion from Double var.";
  }
  if (result != NULL) {
    return "Expected NULL on string conversion from Double var.";
  }

  PASS();
}

std::string TestVarDeprecated::TestHasPropertyAndMethod() {
  pp::VarPrivate window = instance_->GetWindowObject();
  ASSERT_TRUE(window.is_object());

  // Regular property.
  pp::Var exception;
  ASSERT_TRUE(window.HasProperty("scrollX", &exception));
  ASSERT_TRUE(exception.is_undefined());
  ASSERT_FALSE(window.HasMethod("scrollX", &exception));
  ASSERT_TRUE(exception.is_undefined());

  // Regular method (also counts as HasProperty).
  ASSERT_TRUE(window.HasProperty("find", &exception));
  ASSERT_TRUE(exception.is_undefined());
  ASSERT_TRUE(window.HasMethod("find", &exception));
  ASSERT_TRUE(exception.is_undefined());

  // Nonexistant ones should return false and not set the exception.
  ASSERT_FALSE(window.HasProperty("superEvilBit", &exception));
  ASSERT_TRUE(exception.is_undefined());
  ASSERT_FALSE(window.HasMethod("superEvilBit", &exception));
  ASSERT_TRUE(exception.is_undefined());

  // Check exception and return false on invalid property name.
  ASSERT_FALSE(window.HasProperty(3.14159, &exception));
  ASSERT_FALSE(exception.is_undefined());

  exception = pp::Var();
  ASSERT_FALSE(window.HasMethod(3.14159, &exception));
  ASSERT_FALSE(exception.is_undefined());

  // Try to use something not an object.
  exception = pp::Var();
  pp::VarPrivate string_object("asdf");
  ASSERT_FALSE(string_object.HasProperty("find", &exception));
  ASSERT_FALSE(exception.is_undefined());
  exception = pp::Var();
  ASSERT_FALSE(string_object.HasMethod("find", &exception));
  ASSERT_FALSE(exception.is_undefined());

  // Try to use an invalid object (need to use the C API).
  PP_Var invalid_object;
  invalid_object.type = PP_VARTYPE_OBJECT;
  invalid_object.value.as_id = static_cast<int64_t>(-1234567);
  PP_Var exception2 = PP_MakeUndefined();
  ASSERT_FALSE(var_interface_->HasProperty(invalid_object,
                                           pp::Var("find").pp_var(),
                                           &exception2));
  ASSERT_NE(PP_VARTYPE_UNDEFINED, exception2.type);
  var_interface_->Release(exception2);

  exception2 = PP_MakeUndefined();
  ASSERT_FALSE(var_interface_->HasMethod(invalid_object,
                                         pp::Var("find").pp_var(),
                                         &exception2));
  ASSERT_NE(PP_VARTYPE_UNDEFINED, exception2.type);
  var_interface_->Release(exception2);

  // Getting a valid property/method when the exception is set returns false.
  exception = pp::Var("Bad something-or-other exception");
  ASSERT_FALSE(window.HasProperty("find", &exception));
  ASSERT_FALSE(exception.is_undefined());
  ASSERT_FALSE(window.HasMethod("find", &exception));
  ASSERT_FALSE(exception.is_undefined());

  PASS();
}

// Tests that when the page sends an object to the plugin via a function call,
// that the refcounting works properly (bug 79813).
std::string TestVarDeprecated::TestPassReference() {
  var_from_page_ = pp::Var();

  // Send a JS object from the page to the plugin.
  pp::Var exception;
  pp::Var ret = instance_->ExecuteScript(
      "document.getElementById('plugin').SetValue(function(arg) {"
          "return 'works' + arg;"
      "})",
      &exception);
  ASSERT_TRUE(exception.is_undefined());

  // We should have gotten an object set for our var_from_page.
  ASSERT_TRUE(var_from_page_.is_object());

  // If the reference counting works, the object should be valid. We can test
  // this by executing it (it was a function we defined above) and it should
  // return "works" concatenated with the argument.
  pp::VarPrivate function(var_from_page_);
  pp::Var result = var_from_page_.Call(pp::Var(), "nice");
  ASSERT_TRUE(result.is_string());
  ASSERT_TRUE(result.AsString() == "worksnice");

  // Reset var_from_page_ so it doesn't seem like a leak to the var leak
  // checking code.
  var_from_page_ = pp::Var();

  PASS();
}

