// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/ppapi_migration/pdfium_font_linux.h"

#include "base/check.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/private/pdf.h"
#include "ppapi/cpp/trusted/browser_font_trusted.h"
#include "third_party/blink/public/platform/web_font_description.h"

namespace chrome_pdf {

namespace {

PP_Instance g_last_instance_id;

constexpr PP_BrowserFont_Trusted_Weight ToPepperWeight(
    blink::WebFontDescription::Weight w) {
  return static_cast<PP_BrowserFont_Trusted_Weight>(w);
}

#define FONT_WEIGHT_MATCH_ASSERT(x)                                        \
  static_assert(PP_BROWSERFONT_TRUSTED_WEIGHT_##x ==                       \
                    ToPepperWeight(blink::WebFontDescription::kWeight##x), \
                "Font weight mismatch.")
FONT_WEIGHT_MATCH_ASSERT(100);
FONT_WEIGHT_MATCH_ASSERT(200);
FONT_WEIGHT_MATCH_ASSERT(300);
FONT_WEIGHT_MATCH_ASSERT(400);
FONT_WEIGHT_MATCH_ASSERT(500);
FONT_WEIGHT_MATCH_ASSERT(600);
FONT_WEIGHT_MATCH_ASSERT(700);
FONT_WEIGHT_MATCH_ASSERT(800);
FONT_WEIGHT_MATCH_ASSERT(900);
#undef FONT_WEIGHT_MATCH_ASSERT

}  // namespace

void* MapPepperFont(const blink::WebFontDescription& desc,
                    const std::string& font_family,
                    int charset) {
  DCHECK(pp::PDF::IsAvailable());
  DCHECK(desc.family.IsEmpty());

  pp::BrowserFontDescription description;

  if (desc.generic_family ==
      blink::WebFontDescription::kGenericFamilyMonospace) {
    description.set_family(PP_BROWSERFONT_TRUSTED_FAMILY_MONOSPACE);
  } else if (desc.generic_family ==
             blink::WebFontDescription::kGenericFamilySerif) {
    description.set_family(PP_BROWSERFONT_TRUSTED_FAMILY_SERIF);
  }

  description.set_face(font_family);
  description.set_weight(ToPepperWeight(desc.weight));
  description.set_italic(desc.italic);

  PP_Resource font_resource = pp::PDF::GetFontFileWithFallback(
      pp::InstanceHandle(g_last_instance_id),
      &description.pp_font_description(),
      static_cast<PP_PrivateFontCharset>(charset));
  long res_id = font_resource;
  return reinterpret_cast<void*>(res_id);
}

unsigned long GetPepperFontData(void* font_id,
                                unsigned int table,
                                unsigned char* buffer,
                                unsigned long buf_size) {
  DCHECK(pp::PDF::IsAvailable());

  uint32_t size = buf_size;
  long res_id = reinterpret_cast<long>(font_id);
  if (!pp::PDF::GetFontTableForPrivateFontFile(res_id, table, buffer, &size))
    return 0;
  return size;
}

void DeletePepperFont(void* font_id) {
  long res_id = reinterpret_cast<long>(font_id);
  pp::Module::Get()->core()->ReleaseResource(res_id);
}

void SetLastPepperInstance(pp::Instance* last_instance) {
  if (last_instance)
    g_last_instance_id = last_instance->pp_instance();
}

}  // namespace chrome_pdf
