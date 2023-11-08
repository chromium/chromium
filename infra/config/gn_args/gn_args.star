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
    "cast_audio",
    args = {
        "is_cast_audio_only": True,
    },
)

gn_args.config(
    "cast_os",
    args = {
        "is_castos": True,
    },
)

gn_args.config(
    "cast_receiver",
    args = {
        "enable_cast_receiver": True,
    },
)

gn_args.config(
    "chrome_with_codecs",
    args = {
        "proprietary_codecs": True,
    },
    configs = [
        "ffmpeg_branding_chrome",
    ],
)

gn_args.config(
    "chromeos_device",
    args = {
        "is_chromeos_device": True,
    },
)

gn_args.config(
    "dcheck_always_on",
    args = {
        "dcheck_always_on": True,
    },
)

gn_args.config(
    "dcheck_off",
    args = {
        "dcheck_always_on": False,
    },
)

gn_args.config(
    "debug",
    args = {
        "is_debug": True,
    },
)

gn_args.config(
    "debug_builder",
    configs = [
        "debug",
        "shared",
        "minimal_symbols",
    ],
)

gn_args.config(
    "devtools_do_typecheck",
    args = {
        "devtools_skip_typecheck": False,
    },
)

gn_args.config(
    "disable_nacl",
    args = {
        "enable_nacl": False,
    },
)

# Enables backup ref ptr by changing the default value of the feature flag.
# This sets the default value of PartitionAllocBackupRefPtr to enabled, with
# enabled-processes = non-renderer:
# https://source.chromium.org/chromium/chromium/src/+/main:base/allocator/partition_alloc_features.cc;drc=ec53a834a53b2d2f780e83614036a8dc89a247b5;l=105
gn_args.config(
    "enable_backup_ref_ptr_feature_flag",
    args = {
        "enable_backup_ref_ptr_feature_flag": True,
    },
)

# Enables dangling raw pointer detection.
# This configuration will silently deactivate the ref count cookie in:
# https://crsrc.org/c/base/allocator/partition_allocator/partition_alloc_config.h;l=208-216;drc=2d195004c75699bdd87c69cdb7e8d293249dcfdd
gn_args.config(
    "enable_dangling_raw_ptr_checks",
    args = {
        "enable_dangling_raw_ptr_checks": True,
    },
)

# Changes the default of the dangling raw pointer detection feature flag,
# enabling it on all runs.
gn_args.config(
    "enable_dangling_raw_ptr_feature_flag",
    args = {
        "enable_dangling_raw_ptr_feature_flag": True,
    },
    configs = [
        "enable_dangling_raw_ptr_checks",
    ],
)

gn_args.config(
    "extended_tracing",
    args = {
        "extended_tracing_enabled": True,
    },
)

gn_args.config(
    "ffmpeg_branding_chrome",
    args = {
        "ffmpeg_branding": "Chrome",
    },
)

gn_args.config(
    "goma",
    args = {
        "use_goma": True,
    },
)

gn_args.config(
    "gpu_tests",
    configs = [
        "chrome_with_codecs",
    ],
)

gn_args.config(
    "ios",
    args = {
        "target_os": "ios",
    },
)

gn_args.config(
    "ios_simulator",
    args = {"target_environment": "simulator"},
    configs = ["ios"],
)

gn_args.config(
    "linux_wayland",
    args = {
        "ozone_auto_platforms": False,
        "ozone_platform_wayland": True,
        "ozone_platform": "wayland",
        "use_bundled_weston": True,
    },
)

gn_args.config(
    "minimal_symbols",
    args = {
        "symbol_level": 1,
    },
)

gn_args.config(
    "no_clang",
    args = {
        "is_clang": False,
    },
)

gn_args.config(
    "no_goma",
    args = {
        "use_goma": False,
    },
)

gn_args.config(
    "no_symbols",
    args = {
        "symbol_level": 0,
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

# Used to pass the list of files to instrument for coverage to the compile
# wrapper. See:
# https://cs.chromium.org/chromium/build/scripts/slave/recipe_modules/code_coverage/api.py
# and
# https://cs.chromium.org/chromium/src/docs/clang_code_coverage_wrapper.md
# For Java, see:
# https://cs.chromium.org/chromium/src/build/android/gyp/jacoco_instr.py
gn_args.config(
    "partial_code_coverage_instrumentation",
    args = {
        "coverage_instrumentation_input_file": "//.code-coverage/files_to_instrument.txt",
    },
)

gn_args.config(
    "reclient",
    args = {
        "use_remoteexec": True,
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
    "release",
    args = {
        "is_debug": False,
        "dcheck_always_on": False,
    },
)

gn_args.config(
    "release_builder",
    configs = [
        "release",
        "static",
    ],
)

gn_args.config(
    "release_builder_blink",
    configs = [
        "release_builder",
        "chrome_with_codecs",
    ],
)

gn_args.config(
    "release_try_builder",
    configs = [
        "release_builder",
        "try_builder",
        "no_symbols",
    ],
)

gn_args.config(
    "shared",
    args = {
        "is_component_build": True,
    },
)

gn_args.config(
    "static",
    args = {
        "is_component_build": False,
    },
)

gn_args.config(
    "try_builder",
    configs = [
        "dcheck_always_on",
        "minimal_symbols",
        "use_dummy_lastchange",
    ],
)

gn_args.config(
    "use_clang_coverage",
    args = {
        "use_clang_coverage": True,
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
    "v4l2_codec",
    # The build system dislikes enabling both V4L2 and VA-API.
    # Be explicit about which one we want to avoid platform defaults.
    args = {
        "use_v4l2_codec": True,
        "use_vaapi": False,
    },
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
