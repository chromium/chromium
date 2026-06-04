# Copyright 2024 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

load("@chromium-luci//xcode.star", _xcode = "xcode")

# Keep this in-sync with the versions of bots in //ios/build/bots/.
xcode = struct(
    # Default Xcode Version (Xcode 26.5 Release)
    xcode_default = _xcode.for_ios("17f42"),

    # Xcode beta version used on beta bots.
    xcode_beta = _xcode.for_ios("17f42"),
)
