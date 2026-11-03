// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_HAPTICS_HAPTICS_MANAGER_H_
#define CONTENT_BROWSER_HAPTICS_HAPTICS_MANAGER_H_

#include "services/device/public/mojom/haptics.mojom-forward.h"

namespace content {

// Interface for platform Web Haptics backends.
class HapticsManager {
 public:
  virtual ~HapticsManager() = default;

  // Plays |effect| at |intensity| (expected pre-clamped to [0, 1]) on the most
  // recent input device.
  virtual void PlayHaptics(device::mojom::HapticEffect effect,
                           double intensity) = 0;
};

}  // namespace content

#endif  // CONTENT_BROWSER_HAPTICS_HAPTICS_MANAGER_H_
