// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_THUNK_PPB_BROWSER_FONT_TRUSTED_API_H_
#define PPAPI_THUNK_PPB_BROWSER_FONT_TRUSTED_API_H_

#include <stdint.h>

#include "ppapi/c/trusted/ppb_browser_font_trusted.h"
#include "ppapi/thunk/ppapi_thunk_export.h"

namespace ppapi {
namespace thunk {

// API for font resources.
class PPAPI_THUNK_EXPORT PPB_BrowserFont_Trusted_API {
 public:
  virtual ~PPB_BrowserFont_Trusted_API() {}

  virtual PP_Bool Describe(PP_BrowserFont_Trusted_Description* description,
                           PP_BrowserFont_Trusted_Metrics* metrics) = 0;
  virtual PP_Bool DrawTextAt(PP_Resource image_data,
                             const PP_BrowserFont_Trusted_TextRun* text,
                             const PP_Point* position,
                             uint32_t color,
                             const PP_Rect* clip,
                             PP_Bool image_data_is_opaque) = 0;
  virtual int32_t MeasureText(const PP_BrowserFont_Trusted_TextRun* text) = 0;
  virtual uint32_t CharacterOffsetForPixel(
      const PP_BrowserFont_Trusted_TextRun* text,
      int32_t pixel_position) = 0;
  virtual int32_t PixelOffsetForCharacter(
      const PP_BrowserFont_Trusted_TextRun* text,
      uint32_t char_offset) = 0;
};

}  // namespace thunk
}  // namespace ppapi

#endif  // PPAPI_THUNK_PPB_BROWSER_FONT_TRUSTED_API_H_
