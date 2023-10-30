# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

load("//console-header.star", "HEADER")

luci.console_view(
    name = "chromium.goma.fyi",
    repo = "https://chromium.googlesource.com/chromium/src",
    header = HEADER,
    entries = [
        luci.console_view_entry(
            builder = "goma/linux-archive-rel-goma-rbe-canary",
            category = "rbe|linux|rel",
            short_name = "clb",
        ),
        luci.console_view_entry(
            builder = "goma/linux-archive-rel-goma-rbe-ats-canary",
            category = "rbe|linux|rel",
            short_name = "ats",
        ),
        luci.console_view_entry(
            builder = "goma/Linux Builder Goma RBE Canary",
            category = "rbe|linux|rel",
        ),
        luci.console_view_entry(
            builder = "goma/chromeos-amd64-generic-rel-goma-rbe-canary",
            category = "rbe|cros|rel",
        ),
        luci.console_view_entry(
            builder = "goma/mac-archive-rel-goma-rbe-canary",
            category = "rbe|mac|rel",
            short_name = "clb",
        ),
        luci.console_view_entry(
            builder = "goma/Mac Builder (dbg) Goma RBE Canary (clobber)",
            category = "rbe|mac|dbg",
            short_name = "clb",
        ),
        luci.console_view_entry(
            builder = "goma/Mac M1 Builder (dbg) Goma RBE Canary (clobber)",
            category = "rbe|mac|dbg",
            short_name = "m1",
        ),
        luci.console_view_entry(
            builder = "goma/ios-device-goma-rbe-canary-clobber",
            category = "rbe|ios",
            short_name = "clb",
        ),
        luci.console_view_entry(
            builder = "goma/Win Builder Goma RBE Canary",
            category = "rbe|win|rel",
        ),
        luci.console_view_entry(
            builder = "goma/Win Builder (dbg) Goma RBE Canary",
            category = "rbe|win|dbg",
        ),
    ],
    include_experimental_builds = True,
)
