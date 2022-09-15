/* Copyright 2012 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#ifndef PPAPI_C_DEV_PPB_CHAR_SET_DEV_H_
#define PPAPI_C_DEV_PPB_CHAR_SET_DEV_H_

#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_macros.h"
#include "ppapi/c/pp_stdint.h"
#include "ppapi/c/pp_var.h"

#define PPB_CHAR_SET_DEV_INTERFACE_0_4 "PPB_CharSet(Dev);0.4"
#define PPB_CHAR_SET_DEV_INTERFACE PPB_CHAR_SET_DEV_INTERFACE_0_4

// Specifies the error behavior for the character set conversion functions.
// This will affect two cases: where the input is not encoded correctly, and
// when the requested character can not be converted to the destination
// character set.
enum PP_CharSet_ConversionError {
  // Causes the entire conversion to fail if an error is encountered. The
  // conversion function will return NULL.
  PP_CHARSET_CONVERSIONERROR_FAIL,

  // Silently skips over errors. Unrepresentable characters and input encoding
  // errors will be removed from the output.
  PP_CHARSET_CONVERSIONERROR_SKIP,

  // Replaces the error or unrepresentable character with a substitution
  // character. When converting to a Unicode character set (UTF-8 or UTF-16)
  // it will use the unicode "substitution character" U+FFFD. When converting
  // to another character set, the character will be charset-specific. For
  // many languages this will be the representation of the '?' character.
  PP_CHARSET_CONVERSIONERROR_SUBSTITUTE
};
PP_COMPILE_ASSERT_ENUM_SIZE_IN_BYTES(PP_CharSet_ConversionError, 4);

struct PPB_CharSet_Dev_0_4 {
  // Converts the UTF-16 string pointed to in |*utf16| to an 8-bit string in the
  // specified code page. |utf16_len| is measured in UTF-16 units, not bytes.
  // This value may not be NULL.
  //
  // The return value is a NULL-terminated 8-bit string corresponding to the
  // new character set, or NULL on failure. THIS STRING MUST BE FREED USING
  // PPB_Core::MemFree(). The length of the returned string, not including the
  // terminating NULL, will be placed into *output_length. When there is no
  // error, the result will always be non-NULL, even if the output is 0-length.
  // In this case, it will only contain the terminator. You must still call
  // MemFree any time the return value is non-NULL.
  //
  // This function will return NULL if there was an error converting the string
  // and you requested PP_CHARSET_CONVERSIONERROR_FAIL, or the output character
  // set was unknown.
  char* (*UTF16ToCharSet)(PP_Instance instance,
                          const uint16_t* utf16, uint32_t utf16_len,
                          const char* output_char_set,
                          enum PP_CharSet_ConversionError on_error,
                          uint32_t* output_length);

  // Same as UTF16ToCharSet except converts in the other direction. The input
  // is in the given charset, and the |input_len| is the number of bytes in
  // the |input| string. |*output_length| is the number of 16-bit values in
  // the output not counting the terminating NULL.
  //
  // Since UTF16 can represent every Unicode character, the only time the
  // replacement character will be used is if the encoding in the input string
  // is incorrect.
  uint16_t* (*CharSetToUTF16)(PP_Instance instance,
                              const char* input, uint32_t input_len,
                              const char* input_char_set,
                              enum PP_CharSet_ConversionError on_error,
                              uint32_t* output_length);

  // Returns a string var representing the current multi-byte character set of
  // the current system.
  //
  // WARNING: You really shouldn't be using this function unless you're dealing
  // with legacy data. You should be using UTF-8 or UTF-16 and you don't have
  // to worry about the character sets.
  struct PP_Var (*GetDefaultCharSet)(PP_Instance instance);
};

typedef struct PPB_CharSet_Dev_0_4 PPB_CharSet_Dev;

#endif  // PPAPI_C_DEV_PPB_CHAR_SET_DEV_H_
