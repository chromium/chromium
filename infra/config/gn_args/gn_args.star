# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Execute this file to set up some common GN arg configs for Chromium builders.

load("//lib/gn_args.star", "gn_args")

gn_args.config(
    name = "afl",
    args = {
        "use_afl": True,
    },
)

gn_args.config(
    name = "amd64-generic",
    args_file = "//build/args/chromeos/amd64-generic.gni",
)

gn_args.config(
    name = "amd64-generic-crostoolchain",
    args_file = "//build/args/chromeos/amd64-generic-crostoolchain.gni",
)

gn_args.config(
    name = "amd64-generic-vm",
    args_file = "//build/args/chromeos/amd64-generic-vm.gni",
)

gn_args.config(
    name = "android",
    args = {
        "target_os": "android",
    },
)

gn_args.config(
    name = "android_asan",
    args = {
        "is_asan": True,
        "default_min_sdk_version": 27,
    },
)

# We build Android with codecs on most bots to ensure maximum test
# coverage, but use 'android_builder_without_codecs' on bots responsible for
# building publicly advertised non-Official Android builds --
# which are not allowed to have proprietary codecs enabled.
gn_args.config(
    name = "android_builder",
    configs = [
        "android_builder_without_codecs",
        "chrome_with_codecs",
    ],
)

# Builders never have a use for android:debuggable="true". They do not use
# JWDP (java debugger), and do not need it to access application files
# since they always use userdebug OS builds (which have root access).
# android:debuggable="true" causes ART to run more slowly, so tests run
# faster without it. https://crbug.com/1276429
gn_args.config(
    name = "android_builder_without_codecs",
    configs = ["android"],
    args = {
        "debuggable_apks": False,
    },
)

# For Android builds requiring is_desktop_android.
gn_args.config(
    name = "android_desktop",
    args = {
        "is_desktop_android": True,
    },
)

# Representative GN args for Android developer builds.
gn_args.config(
    name = "android_developer",
    configs = [
        "android",
        "arm64",
        "developer",
    ],
)

# It's significantly faster to build without static analysis checks.
gn_args.config(
    name = "android_fastbuild",
    args = {
        "android_static_analysis": "off",
    },
)

gn_args.config(
    name = "android_low_end_secondary_toolchain",
    args = {
        "is_high_end_android_secondary_toolchain": False,
    },
)

# TODO(crbug.com/40105916): This is temporary. We'd like to run a
# smoke test on android_binary_sizes to ensure coverage of proguard, at
# which point we can merge this into android_fastbuild. Until then, only
# disable proguard on a few bots to gather metrics on the effect on build
# times.
gn_args.config(
    name = "android_no_proguard",
    args = {
        "is_java_debug": True,
    },
)

gn_args.config(
    name = "angle_deqp_tests",
    args = {
        "build_angle_deqp_tests": True,
    },
)

gn_args.config(
    name = "arm",
    args = {
        "target_cpu": "arm",
    },
)

gn_args.config(
    name = "arm-generic",
    args_file = "//build/args/chromeos/arm-generic.gni",
)

gn_args.config(
    name = "arm-generic-crostoolchain",
    args_file = "//build/args/chromeos/arm-generic-crostoolchain.gni",
)

gn_args.config(
    name = "arm64",
    args = {
        "target_cpu": "arm64",
    },
)

gn_args.config(
    name = "arm64-generic",
    args_file = "//build/args/chromeos/arm64-generic.gni",
)

gn_args.config(
    name = "arm64-generic-crostoolchain",
    args_file = "//build/args/chromeos/arm64-generic-crostoolchain.gni",
)

gn_args.config(
    name = "arm64-generic-vm",
    args_file = "//build/args/chromeos/arm64-generic-vm.gni",
)

gn_args.config(
    name = "arm64_host",
    args = {
        "test_host_cpu": "arm64",
    },
    configs = [
        "arm64",
    ],
)

gn_args.config(
    name = "arm_no_neon",
    args = {
        "arm_use_neon": False,
    },
    configs = [
        "arm",
    ],
)

gn_args.config(
    name = "asan",
    args = {
        "is_asan": True,
    },
)

gn_args.config(
    name = "blink_enable_generated_code_formatting",
    args = {
        "blink_enable_generated_code_formatting": True,
    },
)

gn_args.config(
    name = "blink_symbol",
    args = {
        "blink_symbol_level": 1,
    },
)

gn_args.config(
    name = "cast_android",
    args_file = "//chromecast/build/args/config/android.gni",
    configs = [
        "cast_receiver",
        "android",
        "minimal_symbols",
        "static",
    ],
)

gn_args.config(
    name = "cast_linux",
    args_file = "//chromecast/build/args/config/linux.gni",
    configs = [
        "cast_receiver",
        "linux",
        "minimal_symbols",
        "static",
    ],
)

gn_args.config(
    name = "cast_debug",
    args = {
        "cast_is_debug": True,
    },
    configs = [
        "debug",
        "dcheck_always_on",
    ],
)

gn_args.config(
    name = "cast_java_debug",
    args = {
        "is_java_debug": True,
    },
)

gn_args.config(
    name = "cast_java_release",
    args = {
        "is_java_debug": False,
    },
)

gn_args.config(
    name = "cast_release",
    args = {
        "cast_is_debug": False,
    },
    configs = [
        "dcheck_off",
        "release",
    ],
)

gn_args.config(
    name = "cast_receiver",
    args = {
        "enable_cast_receiver": True,
    },
)

gn_args.config(
    name = "cast_receiver_size_optimized",
    args_file = "//build/config/fuchsia/size_optimized_cast_receiver_args.gn",
)

gn_args.config(
    name = "centipede",
    args = {
        "use_centipede": True,
    },
)

gn_args.config(
    name = "cfi",
    args = {
        "is_cfi": True,
    },
)

gn_args.config(
    name = "cfi_diag",
    args = {
        "use_cfi_diag": True,
    },
)

gn_args.config(
    name = "cfi_full",
    args = {
        "use_cfi_cast": True,
    },
    configs = [
        "cfi",
    ],
)

gn_args.config(
    name = "cfi_icall",
    args = {
        "use_cfi_icall": True,
    },
)

gn_args.config(
    name = "cfi_recover",
    args = {
        "use_cfi_recover": True,
    },
)

gn_args.config(
    name = "cfm",
    args = {
        "is_cfm": True,
    },
)

gn_args.config(
    name = "chrome_for_testing",
    args = {
        "is_chrome_for_testing": True,
    },
)

gn_args.config(
    name = "chrome_with_codecs",
    args = {
        "proprietary_codecs": True,
    },
    configs = [
        "ffmpeg_branding_chrome",
    ],
)

gn_args.config(
    name = "chromeos",
    args = {
        "target_os": "chromeos",
    },
)

gn_args.config(
    name = "chromeos_codecs",
    args = {
        "proprietary_codecs": True,
    },
    configs = [
        "ffmpeg_branding_chromeos",
    ],
)

gn_args.config(
    name = "chromeos_with_codecs",
    configs = [
        "chromeos",
        "chromeos_codecs",
    ],
)

gn_args.config(
    name = "chromeos_device",
    configs = [
        "chromeos",
    ],
    args = {
        "is_chromeos_device": True,
    },
)

gn_args.config(
    name = "clang",
    args = {
        "is_clang": True,
    },
)

gn_args.config(
    name = "clang_tot",
    args = {
        "llvm_force_head_revision": True,
    },
    configs = [
        "clang",
    ],
)
gn_args.config(
    name = "codesearch_builder",
    args = {
        "clang_use_chrome_plugins": False,
        "enable_kythe_annotations": True,
    },
    configs = [
        "blink_enable_generated_code_formatting",
    ],
)

gn_args.config(
    name = "compile_only",
    configs = [
        "no_symbols",
    ],
)

# Keep in sync with //infra/build/recipes/recipe_modules/chromium_android/chromium_config.py
gn_args.config(
    name = "cronet_android",
    args = {
        "use_partition_alloc": False,
        "enable_reporting": True,
        "use_hashed_jni_names": True,
        "default_min_sdk_version": 21,
        "enable_base_tracing": False,
        "clang_use_default_sample_profile": False,
        "media_use_ffmpeg": False,
        # https://crbug.com/1136963
        "use_thin_lto": False,
        "enable_resource_allowlist_generation": False,
    },
    configs = [
        "android",
        "cronet_common",
    ],
)

gn_args.config(
    name = "cronet_android_mainline_clang",
    args = {
        "clang_base_path": "//third_party/cronet_android_mainline_clang/linux-amd64",
        "clang_use_chrome_plugins": False,
        "default_min_sdk_version": 29,
        # https://crbug.com/1481060
        "llvm_android_mainline": True,
    },
)

# Keep in sync with //infra/build/recipes/recipe_modules/chromium_android/chromium_config.py
gn_args.config(
    name = "cronet_common",
    args = {
        "disable_file_support": True,
        "enable_websockets": False,
        "include_transport_security_state_preload_list": False,
        "is_cronet_build": True,
        "use_platform_icu_alternatives": True,
    },
)

gn_args.config(
    name = "dawn_enable_opengles",
    args = {
        "dawn_enable_opengles": True,
    },
)

gn_args.config(
    name = "dawn_use_built_dxc",
    args = {
        "dawn_use_built_dxc": True,
    },
)

gn_args.config(
    name = "dcheck_always_on",
    args = {
        "dcheck_always_on": True,
    },
)

gn_args.config(
    name = "dcheck_off",
    args = {
        "dcheck_always_on": False,
    },
)

gn_args.config(
    name = "debug",
    args = {
        "is_debug": True,
    },
)

gn_args.config(
    name = "debug_builder",
    configs = [
        "debug",
        "shared",
        "minimal_symbols",
    ],
)

gn_args.config(
    name = "debug_try_builder",
    configs = [
        "debug_builder",
    ],
)

gn_args.config(
    name = "debug_static_builder",
    configs = [
        "debug",
        "static",
        "minimal_symbols",
    ],
)

gn_args.config(
    name = "developer",
    configs = [
        "debug",
        "full_symbols",
        "shared",
    ],
)

gn_args.config(
    name = "devtools_do_typecheck",
    args = {
        "devtools_skip_typecheck": False,
    },
)

gn_args.config(
    name = "disable_seed_corpus",
    args = {
        "archive_seed_corpus": False,
    },
)

gn_args.config(
    name = "enable_all_rust_features",
    args = {
        "enable_all_rust_features": True,
    },
)

# TODO(crbug.com/40101527): Explicitly enable DirectX 12.
gn_args.config(
    name = "dx12vk",
    configs = [
        "enable_vulkan",
    ],
)

# Enables backup ref ptr by changing the default value of the feature flag.
# This sets the default value of PartitionAllocBackupRefPtr to enabled, with
# enabled-processes = non-renderer:
# https://source.chromium.org/chromium/chromium/src/+/main:base/allocator/partition_alloc_features.cc;drc=ec53a834a53b2d2f780e83614036a8dc89a247b5;l=105
gn_args.config(
    name = "enable_backup_ref_ptr_feature_flag",
    args = {
        "enable_backup_ref_ptr_feature_flag": True,
    },
)

gn_args.config(
    name = "enable_blink_animation_use_time_delta",
    args = {
        "blink_animation_use_time_delta": True,
    },
)

gn_args.config(
    name = "enable_blink_heap_verification",
    args = {
        "cppgc_enable_verify_heap": True,
    },
)

# Enables dangling raw pointer detection.
# This configuration will silently deactivate the ref count cookie in:
# https://crsrc.org/c/base/allocator/partition_allocator/partition_alloc_config.h;l=208-216;drc=2d195004c75699bdd87c69cdb7e8d293249dcfdd
gn_args.config(
    name = "enable_dangling_raw_ptr_checks",
    args = {
        "enable_dangling_raw_ptr_checks": True,
    },
)

# Changes the default of the dangling raw pointer detection feature flag,
# enabling it on all runs.
gn_args.config(
    name = "enable_dangling_raw_ptr_feature_flag",
    args = {
        "enable_dangling_raw_ptr_feature_flag": True,
    },
    configs = [
        "enable_dangling_raw_ptr_checks",
    ],
)

gn_args.config(
    name = "enable_vulkan",
    args = {
        "enable_vulkan": True,
    },
)

gn_args.config(
    name = "enterprise_companion",
    args = {
        "enable_enterprise_companion": True,
    },
)

gn_args.config(
    name = "extended_tracing",
    args = {
        "extended_tracing_enabled": True,
    },
)

gn_args.config(
    name = "no_fatal_linker_warnings",
    args = {
        "fatal_linker_warnings": False,
    },
)

gn_args.config(
    name = "fail_on_android_expectations",
    args = {
        "fail_on_android_expectations": True,
    },
)

gn_args.config(
    name = "fail_on_san_warnings",
    args = {
        "fail_on_san_warnings": True,
    },
)

gn_args.config(
    name = "ffmpeg_branding_chrome",
    args = {
        "ffmpeg_branding": "Chrome",
    },
)

gn_args.config(
    name = "ffmpeg_branding_chromeos",
    args = {
        "ffmpeg_branding": "ChromeOS",
    },
)

gn_args.config(
    name = "fuchsia",
    args = {
        "target_os": "fuchsia",
    },
)

gn_args.config(
    name = "fuchsia_code_coverage",
    args = {
        "fuchsia_code_coverage": True,
    },
)

gn_args.config(
    name = "fuchsia_smart_display",
    args = {
        "enable_cast_receiver": True,
        "cast_streaming_enable_remoting": True,
    },
    configs = [
        "fuchsia",
    ],
)

gn_args.config(
    name = "full_symbols",
    args = {
        "symbol_level": 2,
    },
)

gn_args.config(
    name = "fuzzer",
    args = {
        "enable_ipc_fuzzer": True,
    },
)

gn_args.config(
    name = "gpu_fyi_tests",
    configs = [
        "gpu_tests",
    ],
)

gn_args.config(
    name = "gpu_tests",
    configs = [
        "chrome_with_codecs",
    ],
)

gn_args.config(
    name = "headless",
    args_file = "//build/args/headless.gn",
)

gn_args.config(
    name = "headless_shell",
    configs = [
        "headless",
        "no_codecs",
    ],
)

gn_args.config(
    name = "include_unwind_tables",
    args = {
        "exclude_unwind_tables": False,
    },
)

gn_args.config(
    name = "ios",
    args = {
        "target_os": "ios",
    },
)

gn_args.config(
    name = "ios_build_chrome_false",
    args = {
        "ios_build_chrome": False,
    },
)

gn_args.config(
    name = "ios_catalyst",
    args = {
        "target_environment": "catalyst",
    },
    configs = [
        "ios",
    ],
)

gn_args.config(
    name = "ios_chromium_cert",
    args = {
        "ios_code_signing_identity_description": "iPhone Developer",
    },
)

gn_args.config(
    name = "ios_developer",
    configs = ["ios_simulator", "debug"],
)

gn_args.config(
    name = "ios_device",
    args = {"target_environment": "device"},
    configs = ["ios"],
)

# defaults to true under ios_sdk.gni
gn_args.config(
    name = "ios_disable_code_signing",
    args = {
        "ios_enable_code_signing": False,
    },
)

gn_args.config(
    name = "ios_google_cert",
    args = {
        "ios_code_signing_identity_description": "Apple Development",
    },
)

gn_args.config(
    name = "ios_simulator",
    args = {"target_environment": "simulator"},
    configs = ["ios"],
)

gn_args.config(
    name = "is_skylab",
    args = {
        "is_skylab": True,
    },
)

gn_args.config(
    name = "jacuzzi",
    args_file = "//build/args/chromeos/jacuzzi.gni",
)

gn_args.config(
    name = "lacros",
    args = {
        "target_os": "chromeos",
        "chromeos_is_browser_only": True,
    },
)

gn_args.config(
    name = "lacros_on_linux",
    args = {
        "chromeos_is_browser_only": True,
    },
    configs = [
        "chromeos",
    ],
)

gn_args.config(
    name = "libfuzzer",
    args = {
        "use_libfuzzer": True,
    },
)

gn_args.config(
    name = "linux",
    args = {
        "target_os": "linux",
    },
)

gn_args.config(
    name = "linux_wayland",
    configs = [
        "linux",
    ],
    args = {
        "ozone_auto_platforms": False,
        "ozone_platform_wayland": True,
        "ozone_platform": "wayland",
        "use_bundled_weston": True,
    },
)

gn_args.config(
    name = "lld",
    args = {
        "use_lld": True,
    },
)

gn_args.config(
    name = "lsan",
    args = {
        "is_lsan": True,
    },
)

gn_args.config(
    name = "mac",
    args = {
        "target_os": "mac",
    },
)

gn_args.config(
    name = "mac_strip",
    args = {
        "enable_stripping": True,
    },
)

gn_args.config(
    name = "mbi_mode_per_render_process_host",
    args = {
        "mbi_mode": "per_render_process_host",
    },
)

gn_args.config(
    name = "minimal_symbols",
    args = {
        "symbol_level": 1,
    },
)

gn_args.config(
    name = "mojo_fuzzer",
    args = {
        "enable_mojom_fuzzer": True,
    },
)

gn_args.config(
    name = "msan",
    args = {
        "is_msan": True,
        "msan_track_origins": 2,
    },
)

gn_args.config(
    name = "msan_no_origins",
    args = {
        "is_msan": True,
        "msan_track_origins": 0,
    },
)

gn_args.config(
    name = "no_clang",
    args = {
        "is_clang": False,
    },
)

gn_args.config(
    name = "no_codecs",
    args = {
        "media_use_libvpx": False,
        "media_use_ffmpeg": False,
        "proprietary_codecs": False,
        "enable_ffmpeg_video_decoders": False,
    },
)

gn_args.config(
    name = "no_com_init_hooks",
    args = {
        "com_init_check_hook_disabled": True,
    },
)

gn_args.config(
    name = "no_dsyms",
    args = {
        "enable_dsyms": False,
    },
)

gn_args.config(
    name = "no_lld",
    args = {
        "use_lld": False,
    },
)

gn_args.config(
    name = "no_reclient",
    args = {
        "use_reclient": False,
    },
)

gn_args.config(
    name = "no_remoteexec",
    args = {
        "use_remoteexec": False,
    },
)

gn_args.config(
    name = "no_remoting",
    args = {
        "enable_remoting": False,
    },
)

gn_args.config(
    name = "no_resource_allowlisting",
    args = {
        "enable_resource_allowlist_generation": False,
    },
)

gn_args.config(
    name = "no_secondary_abi",
    args = {
        "skip_secondary_abi_for_cq": True,
        # A chromium build with "skip_secondary_abi_for_cq" enabled in a
        # checkout that has src-internal fails if enable_chrome_android_internal
        # is not set to false.
        # TODO(crbug.com/361540497): Can remove this when the build is fixed.
        "enable_chrome_android_internal": False,
    },
)

gn_args.config(
    name = "no_siso",
    args = {
        "use_siso": False,
    },
)

gn_args.config(
    name = "no_symbols",
    args = {
        "symbol_level": 0,
    },
)

gn_args.config(
    name = "octopus",
    args_file = "//build/args/chromeos/octopus.gni",
)

gn_args.config(
    name = "official_optimize",
    args = {
        "is_official_build": True,
    },
)

gn_args.config(
    name = "optimize_for_fuzzing",
    args = {
        "optimize_for_fuzzing": True,
    },
)

gn_args.config(
    name = "optimize_webui_off",
    args = {
        "optimize_webui": False,
    },
)

gn_args.config(
    name = "ozone_headless",
    args = {
        "ozone_platform_headless": True,
    },
)

gn_args.config(
    name = "ozone_linux",
    args = {
        "use_ozone": True,
        "ozone_platform": "headless",
        "use_bundled_weston": True,
    },
)

# TODO(anglebug.com/4977): Make angle understand what platform it should
# use. Otherwise, the ozone_platform_x11 && use_ozone config breaks Linux Ozone FYI (Intel) bot
# that exercises angle + ozone (though, it is ozone/drm in reality. We don't support
# angle on Linux Ozone/X11/Wayland yet).
gn_args.config(
    name = "ozone_linux_non_x11",
    args = {
        "ozone_platform_x11": False,
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
    name = "partial_code_coverage_instrumentation",
    args = {
        "coverage_instrumentation_input_file": "//.code-coverage/files_to_instrument.txt",
    },
)

gn_args.config(
    name = "pdf_xfa",
    args = {
        "pdf_enable_xfa": True,
    },
)

gn_args.config(
    name = "perfetto_zlib",
    args = {
        "enable_perfetto_zlib": True,
    },
)

gn_args.config(
    name = "pgo_phase_0",
    args = {
        "chrome_pgo_phase": 0,
    },
)

gn_args.config(
    name = "pgo_phase_1",
    args = {
        "chrome_pgo_phase": 1,
    },
    configs = [
        "v8_release_branch",
    ],
)

gn_args.config(
    name = "remoteexec",
    args = {
        "use_remoteexec": True,
    },
)

gn_args.config(
    name = "reclient_with_remoteexec_links",
    args = {
        "use_reclient_links": True,
        "concurrent_links": 50,
    },
    configs = ["remoteexec"],
)

gn_args.config(
    name = "release",
    args = {
        "is_debug": False,
        "dcheck_always_on": False,
    },
)

gn_args.config(
    name = "release_builder",
    configs = [
        "release",
        "static",
    ],
)

gn_args.config(
    name = "release_builder_blink",
    configs = [
        "release_builder",
        "chrome_with_codecs",
    ],
)

gn_args.config(
    name = "release_java",
    args = {
        "is_java_debug": False,
    },
)

gn_args.config(
    name = "release_try_builder",
    configs = [
        "release_builder",
        "try_builder",
        "no_symbols",
    ],
)

gn_args.config(
    name = "resource_allowlisting",
    args = {
        "enable_resource_allowlist_generation": True,
    },
)

gn_args.config(
    name = "riscv64",
    args = {
        "target_cpu": "riscv64",
    },
)

gn_args.config(
    name = "shared",
    args = {
        "is_component_build": True,
    },
)

gn_args.config(
    name = "save_lld_reproducers",
    args = {
        "save_reproducers_on_lld_crash": True,
    },
)

gn_args.config(
    name = "skip_generate_fuzzer_owners",
    args = {
        "generate_fuzzer_owners": False,
    },
)

gn_args.config(
    name = "stable_channel",
    args = {
        "android_channel": "stable",
    },
)

gn_args.config(
    name = "static",
    args = {
        "is_component_build": False,
    },
)

gn_args.config(
    name = "static_angle",
    args = {
        "use_static_angle": True,
    },
)

gn_args.config(
    name = "siso",
    args = {
        "use_siso": True,
    },
)

gn_args.config(
    name = "strip_debug_info",
    args = {
        "strip_debug_info": True,
    },
)

gn_args.config(
    name = "system_headers_in_deps",
    args = {
        "system_headers_in_deps": True,
    },
)

gn_args.config(
    name = "full_mte",
    args = {
        "use_full_mte": True,
    },
)

gn_args.config(
    name = "thin_lto",
    args = {
        "use_thin_lto": True,
    },
)

gn_args.config(
    name = "try_builder",
    configs = [
        "dcheck_always_on",
        "minimal_symbols",
    ],
)

gn_args.config(
    name = "tsan",
    args = {
        "is_tsan": True,
    },
)

gn_args.config(
    name = "ubsan",
    args = {
        "is_ubsan": True,
    },
)

gn_args.config(
    name = "ubsan_no_recover",
    args = {
        "is_ubsan_no_recover": True,
    },
    configs = [
        "ubsan",
    ],
)

gn_args.config(
    name = "ubsan_security_non_vptr",
    args = {
        "is_ubsan_security": True,
        "is_ubsan_vptr": False,
    },
)

gn_args.config(
    name = "ubsan_vptr",
    args = {
        "is_ubsan_vptr": True,
    },
)

# TODO(krasin): Remove when https://llvm.org/bugs/show_bug.cgi?id=25569
# is fixed and just use ubsan_vptr instead.
gn_args.config(
    name = "ubsan_vptr_no_recover_hack",
    args = {
        "is_ubsan_no_recover": True,
    },
    configs = [
        "ubsan_vptr",
    ],
)

gn_args.config(
    name = "updater",
    args = {
        "enable_updater": True,
    },
)

gn_args.config(
    name = "use_blink",
    args = {
        "use_blink": True,
    },
)

gn_args.config(
    name = "use_clang_coverage",
    args = {
        "use_clang_coverage": True,
    },
)

gn_args.config(
    name = "use_cups",
    args = {
        "use_cups": True,
    },
)

gn_args.config(
    name = "use_fake_dbus_clients",
    args = {
        "use_real_dbus_clients": False,
    },
)

gn_args.config(
    name = "use_java_coverage",
    args = {
        "use_jacoco_coverage": True,
    },
)

gn_args.config(
    name = "use_javascript_coverage",
    args = {
        "use_javascript_coverage": True,
    },
)

gn_args.config(
    name = "v4l2_codec",
    # The build system dislikes enabling both V4L2 and VA-API.
    # Be explicit about which one we want to avoid platform defaults.
    args = {
        "use_v4l2_codec": True,
        "use_vaapi": False,
    },
)

gn_args.config(
    name = "v8_heap",
    args = {
        "v8_enable_verify_heap": True,
    },
)

gn_args.config(
    name = "v8_hybrid",
    args = {
        "v8_target_cpu": "arm",
    },
    configs = [
        "x86",
    ],
)

# V8 flag that disables v8_enable_runtime_call_stats on release branches.
gn_args.config(
    name = "v8_release_branch",
    args = {
        "v8_is_on_release_branch": True,
    },
)

gn_args.config(
    name = "v8_simulate_arm",
    args = {
        "v8_target_cpu": "arm",
    },
    configs = [
        "x86",
    ],
)

gn_args.config(
    name = "v8_simulate_arm64",
    args = {
        "v8_target_cpu": "arm64",
    },
    configs = [
        "x64",
    ],
)

gn_args.config(
    name = "volteer",
    args_file = "//build/args/chromeos/volteer.gni",
)

gn_args.config(
    name = "webview_google",
    args = {
        "system_webview_package_name": "com.google.android.webview",
    },
)

# For Android N-P, only userdebug/eng
gn_args.config(
    name = "webview_monochrome",
    args = {
        "system_webview_package_name": "com.google.android.apps.chrome",
    },
)

# Mainly used by builders that use android emulator.
# See https://bit.ly/3B1cyyt for more details.
gn_args.config(
    name = "webview_shell",
    args = {
        "system_webview_shell_package_name": "org.chromium.my_webview_shell",
    },
)

# For Android >=Q, only userdebug/eng
gn_args.config(
    name = "webview_trichrome",
    args = {
        "system_webview_package_name": "com.google.android.webview.debug",
    },
)

gn_args.config(
    name = "win",
    args = {
        "target_os": "win",
    },
)

gn_args.config(
    name = "win_cross",
    args = {
        "target_os": "win",
    },
)

gn_args.config(
    name = "x64",
    args = {
        "target_cpu": "x64",
    },
)

gn_args.config(
    name = "x86",
    args = {
        "target_cpu": "x86",
    },
)

gn_args.config(
    name = "xctest",
    args = {"enable_run_ios_unittests_with_xctest": True},
    configs = ["ios"],
)

gn_args.config(
    name = "high_end_fuzzer_targets",
    args = {
        "high_end_fuzzer_targets": True,
    },
)
