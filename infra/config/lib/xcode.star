# Copyright 2024 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

def xcode_enum(version):
    return struct(
        version = version,
        cache = swarming.cache(
            name = "xcode_ios_{}".format(version),
            path = "xcode_ios_{}.app".format(version),
        ),
    )

# Keep this in-sync with the versions of bots in //ios/build/bots/.
xcode = struct(
    # Default Xcode Version
    xcode_default = xcode_enum("16a242d"),

    # Default Xcode 13 for chromium iOS.
    x13main = xcode_enum("13c100"),
    # A newer Xcode 13 version used on beta bots.
    x13betabots = xcode_enum("13f17a"),
    # Xcode14 RC will be used to build Main iOS
    x14main = xcode_enum("14c18"),
    # A newer Xcode 14 RC  used on beta bots.
    x14betabots = xcode_enum("14e222b"),
    # Default Xcode 15 for chromium iOS
    x15main = xcode_enum("15f31d"),
    # A newer Xcode 15 version used on beta bots.
    x15betabots = xcode_enum("15f31d"),
    # Xcode 16 beta version used on beta bots.
    x16betabots = xcode_enum("16a242d"),
    # Temporary Xcode16.1 beta 3 version.
    x16_1betabots = xcode_enum("16b5029d"),
    # in use by ios-webkit-tot
    x14wk = xcode_enum("14c18wk"),
)
