// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ppapi/tests/test_char_set.h"

#include "ppapi/c/dev/ppb_char_set_dev.h"
#include "ppapi/c/trusted/ppb_char_set_trusted.h"
#include "ppapi/cpp/dev/memory_dev.h"
#include "ppapi/cpp/module.h"
#include "ppapi/tests/testing_instance.h"

REGISTER_TEST_CASE(CharSet);

TestCharSet::TestCharSet(TestingInstance* instance)
    : TestCase(instance),
      char_set_interface_(NULL) {
}

bool TestCharSet::Init() {
  char_set_interface_ = static_cast<const PPB_CharSet_Dev*>(
      pp::Module::Get()->GetBrowserInterface(PPB_CHAR_SET_DEV_INTERFACE));
  char_set_trusted_interface_ = static_cast<const PPB_CharSet_Trusted*>(
      pp::Module::Get()->GetBrowserInterface(PPB_CHARSET_TRUSTED_INTERFACE));
  return char_set_interface_ && char_set_trusted_interface_;
}

void TestCharSet::RunTests(const std::string& filter) {
  RUN_TEST(UTF16ToCharSetDeprecated, filter);
  RUN_TEST(UTF16ToCharSet, filter);
  RUN_TEST(CharSetToUTF16Deprecated, filter);
  RUN_TEST(CharSetToUTF16, filter);
  RUN_TEST(GetDefaultCharSet, filter);
}

// TODO(brettw) remove this when the old interface is removed.
std::string TestCharSet::TestUTF16ToCharSetDeprecated() {
  // Empty string.
  std::vector<uint16_t> utf16;
  utf16.push_back(0);
  uint32_t utf8result_len = 0;
  pp::Memory_Dev memory;
  char* utf8result = char_set_interface_->UTF16ToCharSet(
      instance_->pp_instance(), &utf16[0], 0, "latin1",
      PP_CHARSET_CONVERSIONERROR_SUBSTITUTE, &utf8result_len);
  ASSERT_TRUE(utf8result);
  ASSERT_TRUE(utf8result[0] == 0);
  ASSERT_TRUE(utf8result_len == 0);
  memory.MemFree(utf8result);

  // Try round-tripping some English & Chinese from UTF-8 through UTF-16
  std::string utf8source("Hello, world. \xe4\xbd\xa0\xe5\xa5\xbd");
  utf16 = UTF8ToUTF16(utf8source);
  utf8result = char_set_interface_->UTF16ToCharSet(
      instance_->pp_instance(), &utf16[0], static_cast<uint32_t>(utf16.size()),
      "Utf-8", PP_CHARSET_CONVERSIONERROR_FAIL, &utf8result_len);
  ASSERT_TRUE(utf8source == std::string(utf8result, utf8result_len));
  memory.MemFree(utf8result);

  // Test an un-encodable character with various modes.
  utf16 = UTF8ToUTF16("h\xe4\xbd\xa0i");

  // Fail mode.
  utf8result_len = 1234;  // Test that this gets 0'ed on failure.
  utf8result = char_set_interface_->UTF16ToCharSet(
      instance_->pp_instance(), &utf16[0], static_cast<uint32_t>(utf16.size()),
      "latin1", PP_CHARSET_CONVERSIONERROR_FAIL, &utf8result_len);
  ASSERT_TRUE(utf8result_len == 0);
  ASSERT_TRUE(utf8result == NULL);

  // Skip mode.
  utf8result = char_set_interface_->UTF16ToCharSet(
      instance_->pp_instance(), &utf16[0], static_cast<uint32_t>(utf16.size()),
      "latin1", PP_CHARSET_CONVERSIONERROR_SKIP, &utf8result_len);
  ASSERT_TRUE(utf8result_len == 2);
  ASSERT_TRUE(utf8result[0] == 'h' && utf8result[1] == 'i' &&
              utf8result[2] == 0);
  memory.MemFree(utf8result);

  // Substitute mode.
  utf8result = char_set_interface_->UTF16ToCharSet(
      instance_->pp_instance(), &utf16[0], static_cast<uint32_t>(utf16.size()),
      "latin1", PP_CHARSET_CONVERSIONERROR_SUBSTITUTE, &utf8result_len);
  ASSERT_TRUE(utf8result_len == 3);
  ASSERT_TRUE(utf8result[0] == 'h' && utf8result[1] == '?' &&
              utf8result[2] == 'i' && utf8result[3] == 0);
  memory.MemFree(utf8result);

  // Try some invalid input encoding.
  utf16.clear();
  utf16.push_back(0xD800);  // High surrogate.
  utf16.push_back('A');  // Not a low surrogate.
  utf8result = char_set_interface_->UTF16ToCharSet(
      instance_->pp_instance(), &utf16[0], static_cast<uint32_t>(utf16.size()),
      "latin1", PP_CHARSET_CONVERSIONERROR_SUBSTITUTE, &utf8result_len);
  ASSERT_TRUE(utf8result_len == 2);
  ASSERT_TRUE(utf8result[0] == '?' && utf8result[1] == 'A' &&
              utf8result[2] == 0);
  memory.MemFree(utf8result);

  // Invalid encoding name.
  utf8result = char_set_interface_->UTF16ToCharSet(
      instance_->pp_instance(), &utf16[0], static_cast<uint32_t>(utf16.size()),
      "poopiepants", PP_CHARSET_CONVERSIONERROR_SUBSTITUTE, &utf8result_len);
  ASSERT_TRUE(!utf8result);
  ASSERT_TRUE(utf8result_len == 0);

  PASS();
}

std::string TestCharSet::TestUTF16ToCharSet() {
  // Empty string.
  std::vector<uint16_t> utf16;
  utf16.push_back(0);
  std::string output_buffer;
  output_buffer.resize(1);
  uint32_t utf8result_len = 0;
  PP_Bool result = char_set_trusted_interface_->UTF16ToCharSet(
      &utf16[0], 0, "latin1", PP_CHARSET_TRUSTED_CONVERSIONERROR_SUBSTITUTE,
      &output_buffer[0], &utf8result_len);
  ASSERT_TRUE(result == PP_TRUE);
  ASSERT_TRUE(utf8result_len == 0);

  // No output buffer returns length of string.
  utf16 = UTF8ToUTF16("hello");
  utf8result_len = 0;
  result = char_set_trusted_interface_->UTF16ToCharSet(
      &utf16[0], static_cast<uint32_t>(utf16.size()), "latin1",
      PP_CHARSET_TRUSTED_CONVERSIONERROR_SUBSTITUTE, NULL, &utf8result_len);
  ASSERT_TRUE(result == PP_TRUE);
  ASSERT_TRUE(utf8result_len == 5);

  // Giving too small of a buffer just fills in that many items and gives us
  // the desired size.
  output_buffer.resize(100);
  utf8result_len = 2;
  output_buffer[utf8result_len] = '$';  // Barrier character.
  result = char_set_trusted_interface_->UTF16ToCharSet(
      &utf16[0], static_cast<uint32_t>(utf16.size()), "latin1",
      PP_CHARSET_TRUSTED_CONVERSIONERROR_SUBSTITUTE,
      &output_buffer[0], &utf8result_len);
  ASSERT_TRUE(result == PP_TRUE);
  ASSERT_TRUE(utf8result_len == 5);
  ASSERT_TRUE(output_buffer[0] == 'h' && output_buffer[1] == 'e' &&
              output_buffer[2] == '$');

  // Try round-tripping some English & Chinese from UTF-8 through UTF-16
  std::string utf8source("Hello, world. \xe4\xbd\xa0\xe5\xa5\xbd");
  utf16 = UTF8ToUTF16(utf8source);
  output_buffer.resize(100);
  utf8result_len = static_cast<uint32_t>(output_buffer.size());
  result = char_set_trusted_interface_->UTF16ToCharSet(
      &utf16[0], static_cast<uint32_t>(utf16.size()),
      "Utf-8", PP_CHARSET_TRUSTED_CONVERSIONERROR_FAIL,
      &output_buffer[0], &utf8result_len);
  ASSERT_TRUE(result == PP_TRUE);
  output_buffer.resize(utf8result_len);
  ASSERT_TRUE(utf8source == output_buffer);

  // Test an un-encodable character with various modes.
  utf16 = UTF8ToUTF16("h\xe4\xbd\xa0i");

  // Fail mode, size should get 0'ed on failure.
  output_buffer.resize(100);
  utf8result_len = static_cast<uint32_t>(output_buffer.size());
  result = char_set_trusted_interface_->UTF16ToCharSet(
      &utf16[0], static_cast<uint32_t>(utf16.size()),
      "latin1", PP_CHARSET_TRUSTED_CONVERSIONERROR_FAIL,
      &output_buffer[0], &utf8result_len);
  ASSERT_TRUE(result == PP_FALSE);
  ASSERT_TRUE(utf8result_len == 0);

  // Skip mode.
  output_buffer.resize(100);
  utf8result_len = static_cast<uint32_t>(output_buffer.size());
  result = char_set_trusted_interface_->UTF16ToCharSet(
      &utf16[0], static_cast<uint32_t>(utf16.size()),
      "latin1", PP_CHARSET_TRUSTED_CONVERSIONERROR_SKIP,
      &output_buffer[0], &utf8result_len);
  ASSERT_TRUE(result == PP_TRUE);
  ASSERT_TRUE(utf8result_len == 2);
  ASSERT_TRUE(output_buffer[0] == 'h' && output_buffer[1] == 'i');

  // Substitute mode.
  output_buffer.resize(100);
  utf8result_len = static_cast<uint32_t>(output_buffer.size());
  result = char_set_trusted_interface_->UTF16ToCharSet(
      &utf16[0], static_cast<uint32_t>(utf16.size()),
      "latin1", PP_CHARSET_TRUSTED_CONVERSIONERROR_SUBSTITUTE,
      &output_buffer[0], &utf8result_len);
  ASSERT_TRUE(result == PP_TRUE);
  ASSERT_TRUE(utf8result_len == 3);
  output_buffer.resize(utf8result_len);
  ASSERT_TRUE(output_buffer == "h?i");

  // Try some invalid input encoding.
  output_buffer.resize(100);
  utf8result_len = static_cast<uint32_t>(output_buffer.size());
  utf16.clear();
  utf16.push_back(0xD800);  // High surrogate.
  utf16.push_back('A');  // Not a low surrogate.
  result = char_set_trusted_interface_->UTF16ToCharSet(
      &utf16[0], static_cast<uint32_t>(utf16.size()),
      "latin1", PP_CHARSET_TRUSTED_CONVERSIONERROR_SUBSTITUTE,
      &output_buffer[0], &utf8result_len);
  ASSERT_TRUE(result == PP_TRUE);
  ASSERT_TRUE(utf8result_len == 2);
  ASSERT_TRUE(output_buffer[0] == '?' && output_buffer[1] == 'A');

  // Invalid encoding name.
  output_buffer.resize(100);
  utf8result_len = static_cast<uint32_t>(output_buffer.size());
  result = char_set_trusted_interface_->UTF16ToCharSet(
      &utf16[0], static_cast<uint32_t>(utf16.size()),
      "poopiepants", PP_CHARSET_TRUSTED_CONVERSIONERROR_SUBSTITUTE,
      &output_buffer[0], &utf8result_len);
  ASSERT_TRUE(result == PP_FALSE);
  ASSERT_TRUE(utf8result_len == 0);

  PASS();
}

// TODO(brettw) remove this when the old interface is removed.
std::string TestCharSet::TestCharSetToUTF16Deprecated() {
  pp::Memory_Dev memory;

  // Empty string.
  uint32_t utf16result_len;
  uint16_t* utf16result = char_set_interface_->CharSetToUTF16(
      instance_->pp_instance(), "", 0, "latin1",
      PP_CHARSET_CONVERSIONERROR_FAIL, &utf16result_len);
  ASSERT_TRUE(utf16result);
  ASSERT_TRUE(utf16result_len == 0);
  ASSERT_TRUE(utf16result[0] == 0);
  memory.MemFree(utf16result);

  // Basic Latin1.
  char latin1[] = "H\xef";
  utf16result = char_set_interface_->CharSetToUTF16(
      instance_->pp_instance(), latin1, 2, "latin1",
      PP_CHARSET_CONVERSIONERROR_FAIL, &utf16result_len);
  ASSERT_TRUE(utf16result);
  ASSERT_TRUE(utf16result_len == 2);
  ASSERT_TRUE(utf16result[0] == 'H' && utf16result[1] == 0xef &&
              utf16result[2] == 0);
  memory.MemFree(utf16result);

  // Invalid input encoding with FAIL.
  char badutf8[] = "A\xe4Z";
  utf16result = char_set_interface_->CharSetToUTF16(
      instance_->pp_instance(), badutf8, 3, "utf8",
      PP_CHARSET_CONVERSIONERROR_FAIL, &utf16result_len);
  ASSERT_TRUE(!utf16result);
  ASSERT_TRUE(utf16result_len == 0);
  memory.MemFree(utf16result);

  // Invalid input with SKIP.
  utf16result = char_set_interface_->CharSetToUTF16(
      instance_->pp_instance(), badutf8, 3, "utf8",
      PP_CHARSET_CONVERSIONERROR_SKIP, &utf16result_len);
  ASSERT_TRUE(utf16result);
  ASSERT_TRUE(utf16result_len == 2);
  ASSERT_TRUE(utf16result[0] == 'A' && utf16result[1] == 'Z' &&
              utf16result[2] == 0);
  memory.MemFree(utf16result);

  // Invalid input with SUBSTITUTE.
  utf16result = char_set_interface_->CharSetToUTF16(
      instance_->pp_instance(), badutf8, 3, "utf8",
      PP_CHARSET_CONVERSIONERROR_SUBSTITUTE, &utf16result_len);
  ASSERT_TRUE(utf16result);
  ASSERT_TRUE(utf16result_len == 3);
  ASSERT_TRUE(utf16result[0] == 'A' && utf16result[1] == 0xFFFD &&
              utf16result[2] == 'Z' && utf16result[3] == 0);
  memory.MemFree(utf16result);

  // Invalid encoding name.
  utf16result = char_set_interface_->CharSetToUTF16(
      instance_->pp_instance(), badutf8, 3, "poopiepants",
      PP_CHARSET_CONVERSIONERROR_SUBSTITUTE, &utf16result_len);
  ASSERT_TRUE(!utf16result);
  ASSERT_TRUE(utf16result_len == 0);
  memory.MemFree(utf16result);

  PASS();
}

std::string TestCharSet::TestCharSetToUTF16() {
  std::vector<uint16_t> output_buffer;
  output_buffer.resize(100);

  // Empty string.
  output_buffer.resize(100);
  uint32_t utf16result_len = static_cast<uint32_t>(output_buffer.size());
  PP_Bool result = char_set_trusted_interface_->CharSetToUTF16(
      "", 0, "latin1", PP_CHARSET_TRUSTED_CONVERSIONERROR_FAIL,
      &output_buffer[0], &utf16result_len);
  ASSERT_TRUE(result);
  ASSERT_TRUE(utf16result_len == 0);
  ASSERT_TRUE(output_buffer[0] == 0);

  // Basic Latin1.
  output_buffer.resize(100);
  utf16result_len = static_cast<uint32_t>(output_buffer.size());
  char latin1[] = "H\xef";
  result = char_set_trusted_interface_->CharSetToUTF16(
      latin1, 2, "latin1", PP_CHARSET_TRUSTED_CONVERSIONERROR_FAIL,
      &output_buffer[0], &utf16result_len);
  ASSERT_TRUE(result);
  ASSERT_TRUE(utf16result_len == 2);
  ASSERT_TRUE(output_buffer[0] == 'H' && output_buffer[1] == 0xef);

  // Invalid input encoding with FAIL.
  output_buffer.resize(100);
  utf16result_len = static_cast<uint32_t>(output_buffer.size());
  char badutf8[] = "A\xe4Z";
  result = char_set_trusted_interface_->CharSetToUTF16(
      badutf8, 3, "utf8", PP_CHARSET_TRUSTED_CONVERSIONERROR_FAIL,
      &output_buffer[0], &utf16result_len);
  ASSERT_TRUE(!result);
  ASSERT_TRUE(utf16result_len == 0);

  // Invalid input with SKIP.
  output_buffer.resize(100);
  utf16result_len = static_cast<uint32_t>(output_buffer.size());
  result = char_set_trusted_interface_->CharSetToUTF16(
      badutf8, 3, "utf8", PP_CHARSET_TRUSTED_CONVERSIONERROR_SKIP,
      &output_buffer[0], &utf16result_len);
  ASSERT_TRUE(result);
  ASSERT_TRUE(utf16result_len == 2);
  ASSERT_TRUE(output_buffer[0] == 'A' && output_buffer[1] == 'Z');

  // Invalid input with SUBSTITUTE.
  output_buffer.resize(100);
  utf16result_len = static_cast<uint32_t>(output_buffer.size());
  result = char_set_trusted_interface_->CharSetToUTF16(
      badutf8, 3, "utf8", PP_CHARSET_TRUSTED_CONVERSIONERROR_SUBSTITUTE,
      &output_buffer[0], &utf16result_len);
  ASSERT_TRUE(result);
  ASSERT_TRUE(utf16result_len == 3);
  ASSERT_TRUE(output_buffer[0] == 'A' && output_buffer[1] == 0xFFFD &&
              output_buffer[2] == 'Z');

  // Invalid encoding name.
  output_buffer.resize(100);
  utf16result_len = static_cast<uint32_t>(output_buffer.size());
  result = char_set_trusted_interface_->CharSetToUTF16(
      badutf8, 3, "poopiepants", PP_CHARSET_TRUSTED_CONVERSIONERROR_SUBSTITUTE,
      &output_buffer[0], &utf16result_len);
  ASSERT_TRUE(!result);
  ASSERT_TRUE(utf16result_len == 0);

  PASS();
}

std::string TestCharSet::TestGetDefaultCharSet() {
  // Test invalid instance.
  pp::Var result(pp::PASS_REF, char_set_interface_->GetDefaultCharSet(0));
  ASSERT_TRUE(result.is_undefined());

  // Just make sure the default char set is a nonempty string.
  result = pp::Var(pp::PASS_REF,
      char_set_interface_->GetDefaultCharSet(instance_->pp_instance()));
  ASSERT_TRUE(result.is_string());
  ASSERT_FALSE(result.AsString().empty());

  PASS();
}

std::vector<uint16_t> TestCharSet::UTF8ToUTF16(const std::string& utf8) {
  uint32_t result_len = 0;
  uint16_t* result = char_set_interface_->CharSetToUTF16(
      instance_->pp_instance(), utf8.c_str(),
      static_cast<uint32_t>(utf8.size()),
      "utf-8", PP_CHARSET_CONVERSIONERROR_FAIL, &result_len);

  std::vector<uint16_t> result_vector;
  if (!result)
    return result_vector;

  result_vector.assign(result, &result[result_len]);
  pp::Memory_Dev memory;
  memory.MemFree(result);
  return result_vector;
}
