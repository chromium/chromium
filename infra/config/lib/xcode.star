# Copyright 2024 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

load("@chromium-luci//xcode.star", _xcode = "xcode")

# Keep this in-sync with the versions of bots in //ios/build/bots/.
xcode = struct(
    # Default Xcode Version
    xcode_default = _xcode.for_ios("16c5032a"),

    # Default Xcode 13 for chromium iOS.
    x13main = _xcode.for_ios("13c100"),
    # A newer Xcode 13 version used on beta bots.
    x13betabots = _xcode.for_ios("13f17a"),
    # Xcode14 RC will be used to build Main iOS
    x14main = _xcode.for_ios("14c18"),
    # A newer Xcode 14 RC  used on beta bots.
    x14betabots = _xcode.for_ios("14e222b"),
    # Default Xcode 15 for chromium iOS
    x15main = _xcode.for_ios("15f31d"),
    # A newer Xcode 15 version used on beta bots.
    x15betabots = _xcode.for_ios("15f31d"),
    # Xcode 16 beta version used on beta bots.
    x16betabots = _xcode.for_ios("16e140"),
    # in use by ios-webkit-tot
    x14wk = _xcode.for_ios("14c18wk"),
)
