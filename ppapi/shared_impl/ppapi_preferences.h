// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_SHARED_IMPL_PPAPI_PREFERENCES_H_
#define PPAPI_SHARED_IMPL_PPAPI_PREFERENCES_H_

#include <map>
#include <string>

#include "ppapi/shared_impl/ppapi_shared_export.h"

namespace ppapi {

struct PPAPI_SHARED_EXPORT Preferences {
 public:
  typedef std::map<std::string, std::u16string> ScriptFontFamilyMap;

  Preferences();
  ~Preferences();

  ScriptFontFamilyMap standard_font_family_map;
  ScriptFontFamilyMap fixed_font_family_map;
  ScriptFontFamilyMap serif_font_family_map;
  ScriptFontFamilyMap sans_serif_font_family_map;
  int default_font_size;
  int default_fixed_font_size;
  int number_of_cpu_cores;
  bool is_3d_supported;
  bool is_stage3d_supported;
  bool is_stage3d_baseline_supported;
  bool is_accelerated_video_decode_enabled;
};

}  // namespace ppapi

#endif  // PPAPI_SHARED_IMPL_PPAPI_PREFERENCES_H_
