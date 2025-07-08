// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webcodecs/hardware_preference.h"

#include "base/notreached.h"

namespace blink {

HardwarePreference IdlEnumToHardwarePreference(V8HardwarePreference value) {
  switch (value.AsEnum()) {
    case V8HardwarePreference::Enum::kNoPreference:
      return HardwarePreference::kNoPreference;
    case V8HardwarePreference::Enum::kPreferHardware:
      return HardwarePreference::kPreferHardware;
    case V8HardwarePreference::Enum::kPreferSoftware:
      return HardwarePreference::kPreferSoftware;
  }
}

V8HardwarePreference HardwarePreferenceToIdlEnum(HardwarePreference hw_pref) {
  switch (hw_pref) {
    case HardwarePreference::kNoPreference:
      return V8HardwarePreference(V8HardwarePreference::Enum::kNoPreference);
    case HardwarePreference::kPreferHardware:
      return V8HardwarePreference(V8HardwarePreference::Enum::kPreferHardware);
    case HardwarePreference::kPreferSoftware:
      return V8HardwarePreference(V8HardwarePreference::Enum::kPreferSoftware);
  }
}

}  // namespace blink
