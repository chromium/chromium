# Copyright 2024 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

load("@chromium-luci//xcode.star", _xcode = "xcode")

# Keep this in-sync with the versions of bots in //ios/build/bots/.
xcode = struct(
    # Default Xcode Version (Xcode 27 beta 6)
    xcode_default = _xcode.for_ios("27a5252f"),

    # Xcode beta version used on beta bots.
    xcode_beta = _xcode.for_ios("27a5252f"),
)
