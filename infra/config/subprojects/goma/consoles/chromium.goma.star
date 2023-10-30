# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

load("//console-header.star", "HEADER")

luci.console_view(
    name = "chromium.goma",
    repo = "https://chromium.googlesource.com/chromium/src",
    header = HEADER,
    entries = [
        luci.console_view_entry(
            builder = "goma/Chromium Linux Goma RBE Staging",
            category = "rbe|staging|linux|rel",
        ),
        luci.console_view_entry(
            builder = "goma/Chromium Linux Goma RBE Staging (dbg)",
            category = "rbe|staging|linux|debug",
        ),
        luci.console_view_entry(
            builder = "goma/Chromium Mac Goma RBE Staging",
            category = "rbe|staging|mac|rel",
        ),
        luci.console_view_entry(
            builder = "goma/Chromium Mac Goma RBE Staging (dbg)",
            category = "rbe|staging|mac|debug",
        ),
        luci.console_view_entry(
            builder = "goma/Chromium Win Goma RBE Staging",
            category = "rbe|staging|win|rel",
        ),
        luci.console_view_entry(
            builder = "goma/chromeos-amd64-generic-rel-goma-rbe-staging",
            category = "rbe|staging|cros|rel",
        ),
    ],
    include_experimental_builds = True,
)
