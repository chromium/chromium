// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webcodecs/hardware_preference.h"

#include "base/notreached.h"

namespace blink {

HardwarePreference StringToHardwarePreference(const String& value) {
  if (value == "allow")
    return HardwarePreference::kAllow;

  if (value == "require")
    return HardwarePreference::kRequire;

  if (value == "deny")
    return HardwarePreference::kDeny;

  NOTREACHED();
  return HardwarePreference::kAllow;
}

}  // namespace blink
