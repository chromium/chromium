// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/viz/public/cpp/crash_keys.h"

#include <string_view>

#include "components/crash/core/common/crash_key.h"

namespace viz {

void SetDeserializationCrashKeyString(std::string_view str) {
  static crash_reporter::CrashKeyString<128> key("viz_deserialization");
  key.Set(str);
}

}  // namespace viz
