// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/342213636): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "content/renderer/pepper/ppapi_preferences_builder.h"

#include "gpu/config/gpu_feature_info.h"
#include "ppapi/shared_impl/ppapi_preferences.h"
#include "third_party/blink/public/common/web_preferences/web_preferences.h"

namespace content {

ppapi::Preferences PpapiPreferencesBuilder::Build(
    const blink::web_pref::WebPreferences& prefs,
    const gpu::GpuFeatureInfo& gpu_feature_info) {
  ppapi::Preferences ppapi_prefs;
  ppapi_prefs.standard_font_family_map = prefs.standard_font_family_map;
  ppapi_prefs.fixed_font_family_map = prefs.fixed_font_family_map;
  ppapi_prefs.serif_font_family_map = prefs.serif_font_family_map;
  ppapi_prefs.sans_serif_font_family_map = prefs.sans_serif_font_family_map;
  ppapi_prefs.default_font_size = prefs.default_font_size;
  ppapi_prefs.default_fixed_font_size = prefs.default_fixed_font_size;
  ppapi_prefs.number_of_cpu_cores = prefs.number_of_cpu_cores;
  ppapi_prefs.is_3d_supported = false;
  ppapi_prefs.is_stage3d_supported = false;
  ppapi_prefs.is_stage3d_baseline_supported = false;
  ppapi_prefs.is_accelerated_video_decode_enabled =
      (prefs.accelerated_video_decode_enabled &&
       (gpu_feature_info
            .status_values[gpu::GPU_FEATURE_TYPE_ACCELERATED_VIDEO_DECODE] ==
        gpu::kGpuFeatureStatusEnabled));
  return ppapi_prefs;
}

}  // namespace content
