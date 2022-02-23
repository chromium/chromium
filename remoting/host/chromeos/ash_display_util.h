// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_CHROMEOS_ASH_DISPLAY_UTIL_H_
#define REMOTING_HOST_CHROMEOS_ASH_DISPLAY_UTIL_H_

#include <cstdint>
#include <vector>

#include "base/callback_forward.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/display/display.h"

class SkBitmap;

namespace remoting {

using DisplayId = int64_t;

// Utility class that abstracts away all display related actions on ChromeOs,
// allowing us to inject a fake instance during unittests.
class AshDisplayUtil {
 public:
  static AshDisplayUtil& Get();

  // The caller is responsible to ensure this given instance lives long enough.
  // To unset call this method again with nullptr.
  static void SetInstanceForTesting(AshDisplayUtil* instance);

  // Convert the scale factor to DPI.
  static int ScaleFactorToDpi(float scale_factor);
  static int GetDpi(const display::Display& display);

  virtual ~AshDisplayUtil();

  virtual DisplayId GetPrimaryDisplayId() const = 0;

  virtual const std::vector<display::Display>& GetActiveDisplays() const = 0;

  virtual const display::Display* GetDisplayForId(
      DisplayId display_id) const = 0;

  using ScreenshotCallback = base::OnceCallback<void(absl::optional<SkBitmap>)>;
  virtual void TakeScreenshotOfDisplay(DisplayId display_id,
                                       ScreenshotCallback callback) = 0;
};

}  // namespace remoting

#endif  // REMOTING_HOST_CHROMEOS_ASH_DISPLAY_UTIL_H_
