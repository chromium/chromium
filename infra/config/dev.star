#!/usr/bin/env lucicfg
# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# See https://chromium.googlesource.com/infra/luci/luci-go/+/HEAD/lucicfg/doc/README.md
# for information on starlark/lucicfg

load("@chromium-luci//branches.star", "branches")
load("@chromium-luci//builders.star", "os")
load("@chromium-luci//chromium_luci.star", "chromium_luci")
load("//lib/builder_exemptions.star", "exempted_from_contact_builders")
load("//project.star", "settings")

lucicfg.check_version(
    min = "1.44.1",
    message = "Update depot_tools",
)

# Use LUCI Scheduler BBv2 names and add Scheduler realms configs.
lucicfg.enable_experiment("crbug.com/1182002")

# Tell lucicfg what files it is allowed to touch
lucicfg.config(
    config_dir = "generated",
    tracked_files = [
        "builders-dev/*/*/*",
        "builders-dev/*/*/*/*",
        "builders-dev/gn_args_locations.json",
        "luci/cr-buildbucket-dev.cfg",
        "luci/luci-analysis-dev.cfg",
        "luci/luci-bisection-dev.cfg",
        "luci/luci-logdog-dev.cfg",
        "luci/luci-milo-dev.cfg",
        "luci/luci-scheduler-dev.cfg",
        "luci/realms-dev.cfg",
        "luci/testhaus-staging.cfg",
    ],
    fail_on_warnings = True,
)

# Just copy LUCI Analysis config to generated outputs.
lucicfg.emit(
    dest = "luci/luci-analysis-dev.cfg",
    data = io.read_file("luci-analysis-dev.cfg"),
)

# Just copy LUCI Bisection config to generated outputs.
lucicfg.emit(
    dest = "luci/luci-bisection-dev.cfg",
    data = io.read_file("luci-bisection-dev.cfg"),
)

# Just copy Testhaus config to generated outputs.
lucicfg.emit(
    dest = "luci/testhaus-staging.cfg",
    data = io.read_file("testhaus-staging.cfg"),
)

chromium_luci.configure_project(
    name = settings.project,
    is_main = settings.is_main,
    platforms = settings.platforms,
    experiments = [
        "builder_config.targets_spec_directory_relative_to_source_dir",
    ],
)

chromium_luci.configure_per_builder_outputs(
    root_dir = "builders-dev",
)

chromium_luci.configure_builders(
    os_dimension_overrides = {
        os.LINUX_DEFAULT: chromium_luci.os_dimension_overrides(
            default = os.LINUX_JAMMY,
            overrides = json.decode(io.read_file("//lib/linux-default.json")),
        ),
        os.MAC_DEFAULT: os.MAC_15,
        os.MAC_BETA: "Mac-15|Mac-26",
        os.WINDOWS_DEFAULT: os.WINDOWS_10,
    },
)

chromium_luci.configure_builder_health_indicators(
    unhealthy_period_days = 7,
    pending_time_p50_min = 20,
    exempted_from_contact_builders = exempted_from_contact_builders,
)

branches.exec("//dev/dev.star")
