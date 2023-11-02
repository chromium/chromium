# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Execute this file to set up some common GN arg configs for Chromium builders.

load("//lib/gn_args.star", "gn_args")

gn_args.config(
    "also_build_lacros_chrome_for_architecture_amd64",
    args = {
        "also_build_lacros_chrome_for_architecture": "amd64",
    },
)

gn_args.config(
    "amd64-generic-vm",
    args_file = "//build/args/chromeos/amd64-generic-vm.gni",
)

gn_args.config(
    "arm64",
    args = {
        "target_cpu": "arm64",
    },
)

gn_args.config(
    "chromeos_device",
    args = {
        "is_chromeos_device": True,
    },
)

gn_args.config(
    "dcheck_off",
    args = {
        "dcheck_always_on": False,
    },
)

gn_args.config(
    "disable_nacl",
    args = {
        "enable_nacl": False,
    },
)

gn_args.config(
    "official_optimize",
    args = {
        "is_official_build": True,
    },
)

gn_args.config(
    "ozone_headless",
    args = {
        "ozone_platform_headless": True,
    },
)

gn_args.config(
    "reclient",
    args = {
        "use_remoteexec": True,
    },
)

gn_args.config(
    "minimal_symbols",
    args = {
        "symbol_level": 1,
    },
)

gn_args.config(
    "dcheck_always_on",
    args = {
        "dcheck_always_on": True,
    },
)

gn_args.config(
    "debug",
    args = {
        "is_debug": True,
    },
)

gn_args.config(
    "shared",
    args = {
        "is_component_build": True,
    },
)

gn_args.config(
    "goma",
    args = {
        "use_goma": True,
    },
)

gn_args.config(
    "ios",
    args = {
        "target_os": "ios",
    },
)

gn_args.config(
    "use_dummy_lastchange",
    args = {
        "use_dummy_lastchange": True,
    },
)

gn_args.config(
    "use_fake_dbus_clients",
    args = {
        "use_real_dbus_clients": False,
    },
)

gn_args.config(
    "release",
    args = {
        "is_debug": False,
        "dcheck_always_on": False,
    },
)

gn_args.config(
    "static",
    args = {
        "is_component_build": False,
    },
)

gn_args.config(
    "chrome_with_codecs",
    args = {
        "ffmpeg_branding": "Chrome",
        "proprietary_codecs": True,
    },
)

gn_args.config(
    "reclient_with_remoteexec_links",
    args = {
        "use_remoteexec_links": True,
        "concurrent_links": 50,
    },
    configs = ["reclient"],
)

gn_args.config(
    "gpu_tests",
    configs = [
        "chrome_with_codecs",
    ],
)

gn_args.config(
    "ios_simulator",
    args = {"target_environment": "simulator"},
    configs = ["ios"],
)

gn_args.config(
    "try_builder",
    args = {"dcheck_always_on": True},
    configs = [
        "minimal_symbols",
        "use_dummy_lastchange",
    ],
)

gn_args.config(
    "debug_build",
    configs = [
        "debug",
        "shared",
        "minimal_symbols",
    ],
)

gn_args.config(
    "release_builder",
    configs = [
        "release",
        "static",
    ],
)

gn_args.config(
    "x64",
    args = {
        "target_cpu": "x64",
    },
)

gn_args.config(
    "x86",
    args = {
        "target_cpu": "x86",
    },
)

gn_args.config(
    "xctest",
    args = {"enable_run_ios_unittests_with_xctest": True},
    configs = ["ios"],
)
