# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

load("//console-header.star", "HEADER")

luci.console_view(
    name = "android.packager",
    header = HEADER,
    repo = "https://chromium.googlesource.com/chromium/src",
    entries = [
        luci.console_view_entry(
            builder = "ci/android-avd-packager",
            short_name = "avd",
        ),
        luci.console_view_entry(
            builder = "ci/android-sdk-packager",
            short_name = "sdk",
        ),
        luci.console_view_entry(
            builder = "ci/android-androidx-packager",
            short_name = "androidx",
        ),
    ],
)
