// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/extensions/chromeos/system_extensions/window_management/cros_screen.h"

#include "third_party/blink/renderer/extensions/chromeos/system_extensions/window_management/cros_window_management.h"

namespace blink {

CrosScreen::CrosScreen(CrosWindowManagement* manager,
                       mojom::blink::CrosScreenInfoPtr screen)
    : window_management_(manager), screen_(std::move(screen)) {}

void CrosScreen::Trace(Visitor* visitor) const {
  visitor->Trace(window_management_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
