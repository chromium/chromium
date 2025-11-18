# Copyright 2024 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

load("@chromium-luci//xcode.star", _xcode = "xcode")

# Keep this in-sync with the versions of bots in //ios/build/bots/.
xcode = struct(
    # Default Xcode Version (Xcode 26.0.1 Release)
    xcode_default = _xcode.for_ios("17a400"),

    # Xcode 16 beta version used on beta bots.
    x16betabots = _xcode.for_ios("16f6"),
    # Xcode 26 beta version used on beta bots.
    x26betabots = _xcode.for_ios("17c5013i"),
    # in use by ios-webkit-tot
    x14wk = _xcode.for_ios("14c18wk"),
)
