# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

load("//console-header.star", "HEADER")

luci.console_view(
    name = "luci.chromium.goma",
    repo = "https://chromium.googlesource.com/chromium/src",
    header = HEADER,
    entries = [
        luci.console_view_entry(
            builder = "goma/Chromium Linux Goma RBE Staging (clobber)",
            category = "rbe|rel",
            short_name = "clb",
        ),
        luci.console_view_entry(
            builder = "goma/Chromium Linux Goma RBE Staging",
            category = "rbe|rel",
        ),
        luci.console_view_entry(
            builder = "goma/Chromium Linux Goma RBE Staging (dbg) (clobber)",
            category = "rbe|debug",
            short_name = "clb",
        ),
        luci.console_view_entry(
            builder = "goma/Chromium Linux Goma RBE Staging (dbg)",
            category = "rbe|debug",
        ),
    ],
    include_experimental_builds = True,
)
