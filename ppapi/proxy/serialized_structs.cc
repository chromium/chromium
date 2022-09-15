// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/serialized_structs.h"

#include "base/pickle.h"
#include "build/build_config.h"
#include "ppapi/c/pp_file_info.h"
#include "ppapi/c/pp_rect.h"
#include "ppapi/c/trusted/ppb_browser_font_trusted.h"
#include "ppapi/shared_impl/var.h"

namespace ppapi {
namespace proxy {

SerializedFontDescription::SerializedFontDescription()
    : face(),
      family(0),
      size(0),
      weight(0),
      italic(PP_FALSE),
      small_caps(PP_FALSE),
      letter_spacing(0),
      word_spacing(0) {
}

SerializedFontDescription::~SerializedFontDescription() {}

void SerializedFontDescription::SetFromPPBrowserFontDescription(
    const PP_BrowserFont_Trusted_Description& desc) {
  StringVar* string_var = StringVar::FromPPVar(desc.face);
  face = string_var ? string_var->value() : std::string();

  family = desc.family;
  size = desc.size;
  weight = desc.weight;
  italic = desc.italic;
  small_caps = desc.small_caps;
  letter_spacing = desc.letter_spacing;
  word_spacing = desc.word_spacing;
}

void SerializedFontDescription::SetToPPBrowserFontDescription(
    PP_BrowserFont_Trusted_Description* desc) const {
  desc->face = StringVar::StringToPPVar(face);
  desc->family = static_cast<PP_BrowserFont_Trusted_Family>(family);
  desc->size = size;
  desc->weight = static_cast<PP_BrowserFont_Trusted_Weight>(weight);
  desc->italic = italic;
  desc->small_caps = small_caps;
  desc->letter_spacing = letter_spacing;
  desc->word_spacing = word_spacing;
}

SerializedNetworkInfo::SerializedNetworkInfo()
    : type(PP_NETWORKLIST_TYPE_UNKNOWN),
      state(PP_NETWORKLIST_STATE_DOWN),
      mtu(0) {
}

SerializedNetworkInfo::~SerializedNetworkInfo() {}

}  // namespace proxy
}  // namespace ppapi
