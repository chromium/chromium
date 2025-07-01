// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ppapi/shared_impl/private/ppb_char_set_shared.h"

#include <string.h>

#include <algorithm>

#include "base/i18n/icu_string_conversions.h"
#include "ppapi/c/dev/ppb_memory_dev.h"
#include "ppapi/thunk/thunk.h"
#include "third_party/icu/source/common/unicode/ucnv.h"
#include "third_party/icu/source/common/unicode/ucnv_cb.h"
#include "third_party/icu/source/common/unicode/ucnv_err.h"
#include "third_party/icu/source/common/unicode/ustring.h"

namespace ppapi {

namespace {

PP_CharSet_Trusted_ConversionError DeprecatedToConversionError(
    PP_CharSet_ConversionError on_error) {
  switch (on_error) {
    case PP_CHARSET_CONVERSIONERROR_SKIP:
      return PP_CHARSET_TRUSTED_CONVERSIONERROR_SKIP;
    case PP_CHARSET_CONVERSIONERROR_SUBSTITUTE:
      return PP_CHARSET_TRUSTED_CONVERSIONERROR_SUBSTITUTE;
    case PP_CHARSET_CONVERSIONERROR_FAIL:
    default:
      return PP_CHARSET_TRUSTED_CONVERSIONERROR_FAIL;
  }
}

// Converts the given PP error handling behavior to the version in base,
// placing the result in |*result| and returning true on success. Returns false
// if the enum is invalid.
bool PPToBaseConversionError(PP_CharSet_Trusted_ConversionError on_error,
                             base::OnStringConversionError::Type* result) {
  switch (on_error) {
    case PP_CHARSET_TRUSTED_CONVERSIONERROR_FAIL:
      *result = base::OnStringConversionError::FAIL;
      return true;
    case PP_CHARSET_TRUSTED_CONVERSIONERROR_SKIP:
      *result = base::OnStringConversionError::SKIP;
      return true;
    case PP_CHARSET_TRUSTED_CONVERSIONERROR_SUBSTITUTE:
      *result = base::OnStringConversionError::SUBSTITUTE;
      return true;
    default:
      return false;
  }
}

}  // namespace

// static
// The "substitution" behavior of this function does not match the
// implementation in base, so we partially duplicate the code from
// icu_string_conversions.cc with the correct error handling setup required
// by the PPAPI interface.
char* PPB_CharSet_Shared::UTF16ToCharSetDeprecated(
    const uint16_t* utf16,
    uint32_t utf16_len,
    const char* output_char_set,
    PP_CharSet_ConversionError deprecated_on_error,
    uint32_t* output_length) {
  *output_length = 0;
  PP_CharSet_Trusted_ConversionError on_error = DeprecatedToConversionError(
      deprecated_on_error);

  // Compute required length.
  uint32_t required_length = 0;
  UTF16ToCharSet(utf16, utf16_len, output_char_set, on_error, NULL,
                 &required_length);

  // Our output is null terminated, so need one more byte.
  char* ret_buf = static_cast<char*>(
      thunk::GetPPB_Memory_Dev_0_1_Thunk()->MemAlloc(required_length + 1));

  // Do the conversion into the buffer.
  PP_Bool result = UTF16ToCharSet(utf16, utf16_len, output_char_set, on_error,
                                  ret_buf, &required_length);
  if (result == PP_FALSE) {
    thunk::GetPPB_Memory_Dev_0_1_Thunk()->MemFree(ret_buf);
    return NULL;
  }
  ret_buf[required_length] = 0;  // Null terminate.
  *output_length = required_length;
  return ret_buf;
}

// static
PP_Bool PPB_CharSet_Shared::UTF16ToCharSet(
    const uint16_t utf16[],
    uint32_t utf16_len,
    const char* output_char_set,
    PP_CharSet_Trusted_ConversionError on_error,
    char* output_buffer,
    uint32_t* output_length) {
  if (!utf16 || !output_char_set || !output_length) {
    *output_length = 0;
    return PP_FALSE;
  }

  UErrorCode status = U_ZERO_ERROR;
  UConverter* converter = ucnv_open(output_char_set, &status);
  if (!U_SUCCESS(status)) {
    *output_length = 0;
    return PP_FALSE;
  }

  // Setup our error handler.
  switch (on_error) {
    case PP_CHARSET_TRUSTED_CONVERSIONERROR_FAIL:
      ucnv_setFromUCallBack(converter, UCNV_FROM_U_CALLBACK_STOP, 0,
                            NULL, NULL, &status);
      break;
    case PP_CHARSET_TRUSTED_CONVERSIONERROR_SKIP:
      ucnv_setFromUCallBack(converter, UCNV_FROM_U_CALLBACK_SKIP, 0,
                            NULL, NULL, &status);
      break;
    case PP_CHARSET_TRUSTED_CONVERSIONERROR_SUBSTITUTE: {
      // ICU sets the substitution char for some character sets (like latin1)
      // to be the ASCII "substitution character" (26). We want to use '?'
      // instead for backwards-compat with Windows behavior.
      char subst_chars[32];
      int8_t subst_chars_len = 32;
      ucnv_getSubstChars(converter, subst_chars, &subst_chars_len, &status);
      if (subst_chars_len == 1 && subst_chars[0] == 26) {
        // Override to the question mark character if possible. When using
        // setSubstString, the input is a Unicode character. The function will
        // try to convert it to the destination character set and fail if that
        // can not be converted to the destination character set.
        //
        // We just ignore any failure. If the dest char set has no
        // representation for '?', then we'll just stick to the ICU default
        // substitution character.
        UErrorCode subst_status = U_ZERO_ERROR;
        UChar question_mark = '?';
        ucnv_setSubstString(converter, &question_mark, 1, &subst_status);
      }

      ucnv_setFromUCallBack(converter, UCNV_FROM_U_CALLBACK_SUBSTITUTE, 0,
                            NULL, NULL, &status);
      break;
    }
    default:
      *output_length = 0;
      ucnv_close(converter);
      return PP_FALSE;
  }

  // ucnv_fromUChars returns required size not including terminating null.
  *output_length = static_cast<uint32_t>(ucnv_fromUChars(
      converter, output_buffer, output_buffer ? *output_length : 0,
      reinterpret_cast<const UChar*>(utf16), utf16_len, &status));

  ucnv_close(converter);
  if (status == U_BUFFER_OVERFLOW_ERROR) {
    // Don't treat this as a fatal error since we need to return the string
    // size.
    return PP_TRUE;
  } else if (!U_SUCCESS(status)) {
    *output_length = 0;
    return PP_FALSE;
  }
  return PP_TRUE;
}

// static
uint16_t* PPB_CharSet_Shared::CharSetToUTF16Deprecated(
    const char* input,
    uint32_t input_len,
    const char* input_char_set,
    PP_CharSet_ConversionError deprecated_on_error,
    uint32_t* output_length) {
  *output_length = 0;
  PP_CharSet_Trusted_ConversionError on_error = DeprecatedToConversionError(
      deprecated_on_error);

  // Compute required length.
  uint32_t required_length = 0;
  CharSetToUTF16(input, input_len, input_char_set, on_error, NULL,
                 &required_length);

  // Our output is null terminated, so need one more byte.
  uint16_t* ret_buf = static_cast<uint16_t*>(
      thunk::GetPPB_Memory_Dev_0_1_Thunk()->MemAlloc(
          (required_length + 1) * sizeof(uint16_t)));

  // Do the conversion into the buffer.
  PP_Bool result = CharSetToUTF16(input, input_len, input_char_set, on_error,
                                  ret_buf, &required_length);
  if (result == PP_FALSE) {
    thunk::GetPPB_Memory_Dev_0_1_Thunk()->MemFree(ret_buf);
    return NULL;
  }
  ret_buf[required_length] = 0;  // Null terminate.
  *output_length = required_length;
  return ret_buf;
}

PP_Bool PPB_CharSet_Shared::CharSetToUTF16(
    const char* input,
    uint32_t input_len,
    const char* input_char_set,
    PP_CharSet_Trusted_ConversionError on_error,
    uint16_t* output_buffer,
    uint32_t* output_utf16_length) {
  if (!input || !input_char_set || !output_utf16_length) {
    *output_utf16_length = 0;
    return PP_FALSE;
  }

  base::OnStringConversionError::Type base_on_error;
  if (!PPToBaseConversionError(on_error, &base_on_error)) {
    *output_utf16_length = 0;
    return PP_FALSE;  // Invalid enum value.
  }

  // We can convert this call to the implementation in base to avoid code
  // duplication, although this does introduce an extra copy of the data.
  std::u16string output;
  if (!base::CodepageToUTF16(std::string(input, input_len), input_char_set,
                             base_on_error, &output)) {
    *output_utf16_length = 0;
    return PP_FALSE;
  }

  if (output_buffer) {
    memcpy(output_buffer, output.c_str(),
           std::min(*output_utf16_length, static_cast<uint32_t>(output.size()))
           * sizeof(uint16_t));
  }
  *output_utf16_length = static_cast<uint32_t>(output.size());
  return PP_TRUE;
}

}  // namespace ppapi
