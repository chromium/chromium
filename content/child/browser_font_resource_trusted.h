// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_BROWSER_FONT_RESOURCE_TRUSTED_H_
#define CONTENT_CHILD_BROWSER_FONT_RESOURCE_TRUSTED_H_

#include <memory>

#include "cc/paint/paint_canvas.h"
#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_stdint.h"
#include "ppapi/proxy/plugin_resource.h"
#include "ppapi/shared_impl/ppapi_preferences.h"
#include "ppapi/thunk/ppb_browser_font_trusted_api.h"

namespace blink {
class WebFont;
}

namespace content {

class BrowserFontResource_Trusted
    : public ppapi::proxy::PluginResource,
      public ppapi::thunk::PPB_BrowserFont_Trusted_API {
 public:
  BrowserFontResource_Trusted(ppapi::proxy::Connection connection,
                              PP_Instance instance,
                              const PP_BrowserFont_Trusted_Description& desc,
                              const ppapi::Preferences& prefs);

  BrowserFontResource_Trusted(const BrowserFontResource_Trusted&) = delete;
  BrowserFontResource_Trusted& operator=(const BrowserFontResource_Trusted&) =
      delete;

  // Validates the parameters in thee description. Can be called on any thread.
  static bool IsPPFontDescriptionValid(
      const PP_BrowserFont_Trusted_Description& desc);

  // Resource override.
  ::ppapi::thunk::PPB_BrowserFont_Trusted_API* AsPPB_BrowserFont_Trusted_API()
      override;

  // PPB_BrowserFont_Trusted_API implementation.
  PP_Bool Describe(PP_BrowserFont_Trusted_Description* description,
                   PP_BrowserFont_Trusted_Metrics* metrics) override;
  PP_Bool DrawTextAt(PP_Resource image_data,
                     const PP_BrowserFont_Trusted_TextRun* text,
                     const PP_Point* position,
                     uint32_t color,
                     const PP_Rect* clip,
                     PP_Bool image_data_is_opaque) override;
  int32_t MeasureText(const PP_BrowserFont_Trusted_TextRun* text) override;
  uint32_t CharacterOffsetForPixel(const PP_BrowserFont_Trusted_TextRun* text,
                                   int32_t pixel_position) override;
  int32_t PixelOffsetForCharacter(const PP_BrowserFont_Trusted_TextRun* text,
                                  uint32_t char_offset) override;

 private:
  ~BrowserFontResource_Trusted() override;

  // Internal version of DrawTextAt that takes a mapped PaintCanvas.
  void DrawTextToCanvas(cc::PaintCanvas* destination,
                        const PP_BrowserFont_Trusted_TextRun& text,
                        const PP_Point* position,
                        uint32_t color,
                        const PP_Rect* clip);

 private:
  std::unique_ptr<blink::WebFont> font_;
};

}  // namespace content

#endif  // CONTENT_CHILD_BROWSER_FONT_RESOURCE_TRUSTED_H_
