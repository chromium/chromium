// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_SHARED_IMPL_PRIVATE_PPB_CHAR_SET_SHARED_H_
#define PPAPI_SHARED_IMPL_PRIVATE_PPB_CHAR_SET_SHARED_H_

#include <stdint.h>

#include "ppapi/c/dev/ppb_char_set_dev.h"
#include "ppapi/c/trusted/ppb_char_set_trusted.h"
#include "ppapi/shared_impl/ppapi_shared_export.h"

namespace ppapi {

// Contains the implementation of character set conversion that is shared
// between the proxy and the renderer.
class PPAPI_SHARED_EXPORT PPB_CharSet_Shared {
 public:
  static char* UTF16ToCharSetDeprecated(const uint16_t* utf16,
                                        uint32_t utf16_len,
                                        const char* output_char_set,
                                        PP_CharSet_ConversionError on_error,
                                        uint32_t* output_length);
  static PP_Bool UTF16ToCharSet(const uint16_t utf16[],
                                uint32_t utf16_len,
                                const char* output_char_set,
                                PP_CharSet_Trusted_ConversionError on_error,
                                char* output_buffer,
                                uint32_t* output_length);

  static uint16_t* CharSetToUTF16Deprecated(const char* input,
                                            uint32_t input_len,
                                            const char* input_char_set,
                                            PP_CharSet_ConversionError on_error,
                                            uint32_t* output_length);
  static PP_Bool CharSetToUTF16(const char* input,
                                uint32_t input_len,
                                const char* input_char_set,
                                PP_CharSet_Trusted_ConversionError on_error,
                                uint16_t* output_buffer,
                                uint32_t* output_utf16_length);
};

}  // namespace ppapi

#endif  // PPAPI_SHARED_IMPL_PRIVATE_PPB_CHAR_SET_SHARED_H_
