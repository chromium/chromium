#!/usr/bin/env lucicfg
# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# See https://chromium.googlesource.com/infra/luci/luci-go/+/HEAD/lucicfg/doc/README.md
# for information on starlark/lucicfg

load("//lib/branches.star", "branches")

lucicfg.check_version(
    min = "1.19.0",
    message = "Update depot_tools",
)

# Enable LUCI Realms support.
lucicfg.enable_experiment("crbug.com/1085650")

# Tell lucicfg what files it is allowed to touch
lucicfg.config(
    config_dir = "generated",
    tracked_files = [
        "cr-buildbucket-dev.cfg",
        "luci-logdog-dev.cfg",
        "luci-milo-dev.cfg",
        "luci-scheduler-dev.cfg",
        "realms-dev.cfg",
    ],
    fail_on_warnings = True,
)

branches.exec("//dev/dev.star")
