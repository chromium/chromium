#!/usr/bin/env lucicfg
# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# See https://chromium.googlesource.com/infra/luci/luci-go/+/HEAD/lucicfg/doc/README.md
# for information on starlark/lucicfg

load("//lib/branches.star", "branches")

lucicfg.check_version(
    min = "1.28.0",
    message = "Update depot_tools",
)

# Tell lucicfg what files it is allowed to touch
lucicfg.config(
    config_dir = "generated",
    tracked_files = [
        "luci/chops-weetbix-dev.cfg",
        "luci/cr-buildbucket-dev.cfg",
        "luci/luci-logdog-dev.cfg",
        "luci/luci-milo-dev.cfg",
        "luci/luci-scheduler-dev.cfg",
        "luci/realms-dev.cfg",
    ],
    fail_on_warnings = True,
)

# Just copy chops-weetbix-dev.cfg to generated outputs.
lucicfg.emit(
    dest = "luci/chops-weetbix-dev.cfg",
    data = io.read_file("chops-weetbix-dev.cfg"),
)

branches.exec("//dev/dev.star")
