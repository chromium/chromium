// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webcodecs/hardware_preference.h"

#include "base/notreached.h"

namespace blink {

HardwarePreference StringToHardwarePreference(const String& value) {
  if (value == "no-preference")
    return HardwarePreference::kNoPreference;

  if (value == "prefer-hardware")
    return HardwarePreference::kPreferHardware;

  if (value == "prefer-software")
    return HardwarePreference::kPreferSoftware;

  NOTREACHED_IN_MIGRATION();
  return HardwarePreference::kNoPreference;
}

String HardwarePreferenceToString(HardwarePreference hw_pref) {
  switch (hw_pref) {
    case HardwarePreference::kNoPreference:
      return "no-preference";
    case HardwarePreference::kPreferHardware:
      return "prefer-hardware";
    case HardwarePreference::kPreferSoftware:
      return "prefer-software";
  }
}

}  // namespace blink
