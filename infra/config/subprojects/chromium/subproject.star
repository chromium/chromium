# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

load("//lib/branches.star", "branches")

exec("./ci.star")
exec("./try.star")
exec("./gpu.try.star")
exec("./swangle.try.star")

# Execute the M85 config
branches.exec("./versioned/m85/buckets/ci.star")
branches.exec("./versioned/m85/buckets/try.star")

# TODO(gbeaty) external_console_view uses new fields/types that aren't present
# in the version of the protobuf that lint-luci-milo uses, so update protos and
# then uncomment these (the main console is still reachable via the beta link in
# the header)
# luci.external_console_view(
#     name = "main-m86",
#     title = "Chromium M86 Main Console",
#     source = "chromium-m86:main",
# )

# luci.external_console_view(
#     name = "mirrors-m86",
#     title = "Chromium M86 CQ Mirrors Console",
#     source = "chromium-m86:mirrors",
# )

# luci.external_console_view(
#     name = "try-m86",
#     title = "Chromium M86 CQ Console",
#     source = "chromium-m86:try",
# )

branches.exec("./consoles/android.packager.star")
branches.exec("./consoles/metadata.exporter.star")
branches.exec("./consoles/sheriff.ios.star")

branches.exec("./fallback-cq.star")
