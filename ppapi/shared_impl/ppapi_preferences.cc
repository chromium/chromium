// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/shared_impl/ppapi_preferences.h"

namespace ppapi {

Preferences::Preferences()
    : default_font_size(0),
      default_fixed_font_size(0),
      number_of_cpu_cores(0),
      is_3d_supported(true),
      is_stage3d_supported(false),
      is_stage3d_baseline_supported(false),
      is_accelerated_video_decode_enabled(false) {}

Preferences::~Preferences() {}

}  // namespace ppapi
