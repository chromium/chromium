#!/usr/bin/env lucicfg
# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# See https://chromium.googlesource.com/infra/luci/luci-go/+/HEAD/lucicfg/doc/README.md
# for information on starlark/lucicfg

load("//lib/branches.star", "branches")

lucicfg.check_version(
    min = "1.38.1",
    message = "Update depot_tools",
)

# Use LUCI Scheduler BBv2 names and add Scheduler realms configs.
lucicfg.enable_experiment("crbug.com/1182002")

# Tell lucicfg what files it is allowed to touch
lucicfg.config(
    config_dir = "generated",
    tracked_files = [
        "luci/chops-weetbix-dev.cfg",
        "luci/cr-buildbucket-dev.cfg",
        "luci/luci-analysis-dev.cfg",
        "luci/luci-logdog-dev.cfg",
        "luci/luci-milo-dev.cfg",
        "luci/luci-scheduler-dev.cfg",
        "luci/realms-dev.cfg",
    ],
    fail_on_warnings = True,
)

# Just copy LUCI Analysis config to generated outputs.
lucicfg.emit(
    dest = "luci/luci-analysis-dev.cfg",
    data = io.read_file("luci-analysis-dev.cfg"),
)

# TODO(b/270163072): Re-emit after staging is fixed.
# lucicfg.emit(
#     dest = "luci/chops-weetbix-dev.cfg",
#     data = io.read_file("chops-weetbix-dev.cfg"),
# )

branches.exec("//dev/dev.star")
