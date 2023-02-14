# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

load("//console-header.star", "HEADER")

luci.console_view(
    name = "goma.latest",
    repo = "https://chromium.googlesource.com/chromium/src",
    header = HEADER,
    entries = [
        luci.console_view_entry(
            builder = "goma/linux-archive-rel-goma-rbe-latest",
            category = "rbe|linux|rel",
            short_name = "clb",
        ),
        luci.console_view_entry(
            builder = "goma/linux-archive-rel-goma-rbe-ats-latest",
            category = "rbe|linux|rel",
            short_name = "ats",
        ),
        luci.console_view_entry(
            builder = "goma/Linux Builder Goma RBE Latest Client",
            category = "rbe|linux|rel",
        ),
        luci.console_view_entry(
            builder = "goma/chromeos-amd64-generic-rel-goma-rbe-latest",
            category = "rbe|cros|rel",
        ),
        luci.console_view_entry(
            builder = "goma/android-archive-dbg-goma-rbe-latest",
            category = "rbe|android|dbg",
        ),
        luci.console_view_entry(
            builder = "goma/android-archive-dbg-goma-rbe-ats-latest",
            category = "rbe|android|dbg",
            short_name = "ats",
        ),
        luci.console_view_entry(
            builder = "goma/mac-archive-rel-goma-rbe-latest",
            category = "rbe|mac|rel",
            short_name = "clb",
        ),
        luci.console_view_entry(
            builder = "goma/Mac Builder (dbg) Goma RBE Latest Client (clobber)",
            category = "rbe|mac|dbg",
            short_name = "clb",
        ),
        luci.console_view_entry(
            builder = "goma/ios-device-goma-rbe-latest-clobber",
            category = "rbe|ios",
            short_name = "clb",
        ),
        luci.console_view_entry(
            builder = "goma/Win Builder Goma RBE Latest Client",
            category = "rbe|win|rel",
        ),
        luci.console_view_entry(
            builder = "goma/Win Builder (dbg) Goma RBE Latest Client",
            category = "rbe|win|dbg",
        ),
        luci.console_view_entry(
            builder = "goma/Win Builder Goma RBE ATS Latest Client",
            category = "rbe|win|rel",
            short_name = "ats",
        ),
        luci.console_view_entry(
            builder = "goma/Win Builder (dbg) Goma RBE ATS Latest Client",
            category = "rbe|win|dbg",
            short_name = "ats",
        ),
    ],
)
