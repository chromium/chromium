// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/extensions/chromeos/chromeos.h"

#include "third_party/blink/renderer/extensions/chromeos/system_extensions/hid/cros_hid.h"
#include "third_party/blink/renderer/extensions/chromeos/system_extensions/window_management/cros_window_management.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"

namespace blink {

ChromeOS::ChromeOS(ExecutionContext* execution_context)
    : window_management_(
          MakeGarbageCollected<CrosWindowManagement>(execution_context)),
      hid_(MakeGarbageCollected<CrosHID>(execution_context)) {}

CrosWindowManagement* ChromeOS::windowManagement() {
  return window_management_;
}

CrosHID* ChromeOS::hid() {
  return hid_;
}

void ChromeOS::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
  visitor->Trace(window_management_);
  visitor->Trace(hid_);
}

}  // namespace blink
