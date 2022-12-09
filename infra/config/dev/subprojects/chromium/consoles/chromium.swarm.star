# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

luci.console_view(
    name = "chromium.dev",
    header = "//dev/chromium-header.textpb",
    repo = "https://chromium.googlesource.com/chromium/src",
    entries = [
        luci.console_view_entry(builder = "ci/android-pie-arm64-rel-swarming"),
        luci.console_view_entry(builder = "ci/linux-rel-swarming"),
        luci.console_view_entry(builder = "ci/linux-ssd-rel-swarming"),
        luci.console_view_entry(builder = "ci/mac-rel-swarming"),
        luci.console_view_entry(builder = "ci/mac-arm-rel-swarming"),
        luci.console_view_entry(builder = "ci/win-rel-swarming"),
        luci.console_view_entry(builder = "ci/win11-rel-swarming"),
    ],
)
