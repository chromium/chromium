# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

load("//lib/builders.star", "goma", "os", "xcode_cache")
load("//lib/try.star", "try_")
load("//project.star", "settings")

# Execute the versioned files to define all of the per-branch entities
# (bucket, builders, console, cq_group, etc.)
exec("../versioned/m84/buckets/try.star")
exec("../versioned/m85/buckets/try.star")

try_.set_defaults(
    settings,
    add_to_list_view = True,
)

# *** After this point everything is trunk only ***

# Builders are sorted first lexicographically by the function used to define
# them, then lexicographically by their name

try_.blink_builder(
    name = "linux-blink-optional-highdpi-rel",
    goma_backend = goma.backend.RBE_PROD,
)

try_.blink_builder(
    name = "win10-blink-rel",
    goma_backend = goma.backend.RBE_PROD,
    os = os.WINDOWS_ANY,
    builderless = True,
)

try_.blink_builder(
    name = "win7-blink-rel",
    goma_backend = goma.backend.RBE_PROD,
    os = os.WINDOWS_ANY,
    builderless = True,
)

try_.blink_mac_builder(
    name = "mac10.12-blink-rel",
)

try_.blink_mac_builder(
    name = "mac10.13-blink-rel",
)

try_.blink_mac_builder(
    name = "mac10.14-blink-rel",
)

try_.blink_mac_builder(
    name = "mac10.15-blink-rel",
)

try_.chromium_android_builder(
    name = "android-asan",
)

try_.chromium_android_builder(
    name = "android-bfcache-rel",
)

try_.chromium_android_builder(
    name = "android-deterministic-dbg",
    executable = "recipe:swarming/deterministic_build",
    execution_timeout = 6 * time.hour,
)

try_.chromium_android_builder(
    name = "android-deterministic-rel",
    executable = "recipe:swarming/deterministic_build",
    execution_timeout = 6 * time.hour,
)

try_.chromium_android_builder(
    name = "android-marshmallow-x86-fyi-rel",
)

try_.chromium_android_builder(
    name = "android-opus-arm-rel",
)

try_.chromium_android_builder(
    name = "android-oreo-arm64-cts-networkservice-dbg",
)

try_.chromium_android_builder(
    name = "android-oreo-arm64-dbg",
)

try_.chromium_android_builder(
    name = "android-pie-x86-rel",
    goma_jobs = goma.jobs.J150,
)

try_.chromium_android_builder(
    name = "android-pie-arm64-coverage-rel",
    cores = 16,
    goma_jobs = goma.jobs.J300,
    ssd = True,
    use_clang_coverage = True,
)

try_.chromium_android_builder(
    name = "android-10-arm64-rel",
)

try_.chromium_android_builder(
    name = "android-weblayer-pie-arm64-fyi-rel",
)

try_.chromium_android_builder(
    name = "android-weblayer-pie-x86-fyi-rel",
)

try_.chromium_android_builder(
    name = "android-webview-marshmallow-arm64-dbg",
)

try_.chromium_android_builder(
    name = "android-webview-nougat-arm64-dbg",
)

try_.chromium_android_builder(
    name = "android-webview-oreo-arm64-dbg",
)

try_.chromium_android_builder(
    name = "android-webview-pie-arm64-dbg",
)

try_.chromium_android_builder(
    name = "android-webview-pie-arm64-fyi-rel",
)

try_.chromium_android_builder(
    name = "android_archive_rel_ng",
)

try_.chromium_android_builder(
    name = "android_arm64_dbg_recipe",
    goma_jobs = goma.jobs.J300,
)

try_.chromium_android_builder(
    name = "android_blink_rel",
)

try_.chromium_android_builder(
    name = "android_cfi_rel_ng",
    cores = 32,
)

try_.chromium_android_builder(
    name = "android_clang_dbg_recipe",
    goma_jobs = goma.jobs.J300,
)

try_.chromium_android_builder(
    name = "android_mojo",
)

try_.chromium_android_builder(
    name = "android_n5x_swarming_dbg",
)

try_.chromium_android_builder(
    name = "android_unswarmed_pixel_aosp",
)

try_.chromium_android_builder(
    name = "linux_android_dbg_ng",
)

try_.chromium_android_builder(
    name = "try-nougat-phone-tester",
)

try_.chromium_angle_builder(
    name = "android_angle_deqp_rel_ng",
)

try_.chromium_angle_builder(
    name = "android_angle_rel_ng",
)

try_.chromium_angle_builder(
    name = "android_angle_vk32_deqp_rel_ng",
)

try_.chromium_angle_builder(
    name = "android_angle_vk32_rel_ng",
)

try_.chromium_angle_builder(
    name = "android_angle_vk64_deqp_rel_ng",
)

try_.chromium_angle_builder(
    name = "android_angle_vk64_rel_ng",
)

try_.chromium_angle_builder(
    name = "fuchsia-angle-rel",
)

try_.chromium_angle_builder(
    name = "linux-angle-rel",
)

try_.chromium_angle_builder(
    name = "linux_angle_deqp_rel_ng",
)

try_.chromium_angle_builder(
    name = "linux_angle_ozone_rel_ng",
)

try_.chromium_angle_builder(
    name = "mac-angle-rel",
    cores = None,
    os = os.MAC_ANY,
)

try_.chromium_angle_builder(
    name = "win-angle-deqp-rel-32",
    os = os.WINDOWS_ANY,
)

try_.chromium_angle_builder(
    name = "win-angle-deqp-rel-64",
    os = os.WINDOWS_ANY,
)

try_.chromium_angle_builder(
    name = "win-angle-rel-32",
    os = os.WINDOWS_ANY,
)

try_.chromium_angle_builder(
    name = "win-angle-rel-64",
    os = os.WINDOWS_ANY,
)

try_.chromium_chromiumos_builder(
    name = "chromeos-amd64-generic-cfi-thin-lto-rel",
)

try_.chromium_chromiumos_builder(
    name = "chromeos-arm-generic-dbg",
)

try_.chromium_chromiumos_builder(
    name = "chromeos-kevin-compile-rel",
)

try_.chromium_chromiumos_builder(
    name = "chromeos-kevin-rel",
    main_list_view = "try",
    tryjob = try_.job(
        location_regexp = [
            ".+/[+]/build/chromeos/.+",
            ".+/[+]/build/config/chromeos/.*",
            ".+/[+]/chromeos/CHROMEOS_LKGM",
        ],
    ),
)

# TODO(crbug.com/1116180): Clean this up once linux-lacros-rel is up.
try_.chromium_chromiumos_builder(
    name = "linux-lacros-compile-rel",
)

try_.chromium_chromiumos_builder(
    name = "linux-lacros-rel",
    main_list_view = "try",
    tryjob = try_.job(),
)

try_.chromium_chromiumos_builder(
    name = "linux-chromeos-dbg",
)

try_.chromium_dawn_builder(
    name = "linux-dawn-rel",
)

try_.chromium_dawn_builder(
    name = "mac-dawn-rel",
    os = os.MAC_ANY,
)

try_.chromium_dawn_builder(
    name = "win-dawn-rel",
    os = os.WINDOWS_ANY,
)

try_.chromium_dawn_builder(
    name = "dawn-try-win10-x86-rel",
    os = os.WINDOWS_ANY,
)

try_.chromium_dawn_builder(
    name = "dawn-try-win10-x64-asan-rel",
    os = os.WINDOWS_ANY,
)

try_.chromium_linux_builder(
    name = "cast_shell_audio_linux",
)

try_.chromium_linux_builder(
    name = "linux-lacros-fyi-rel",
)

try_.chromium_linux_builder(
    name = "fuchsia-compile-x64-dbg",
    tryjob = try_.job(
        experiment_percentage = 50,
    ),
)

try_.chromium_linux_builder(
    name = "fuchsia-fyi-arm64-dbg",
)

try_.chromium_linux_builder(
    name = "fuchsia-fyi-arm64-rel",
)

try_.chromium_linux_builder(
    name = "fuchsia-fyi-x64-dbg",
)

try_.chromium_linux_builder(
    name = "fuchsia-fyi-x64-rel",
)

try_.chromium_linux_builder(
    name = "layout_test_leak_detection",
)

try_.chromium_linux_builder(
    name = "leak_detection_linux",
)

try_.chromium_linux_builder(
    name = "linux-annotator-rel",
)

try_.chromium_linux_builder(
    name = "linux-bfcache-rel",
)

try_.chromium_linux_builder(
    name = "linux-blink-heap-concurrent-marking-tsan-rel",
)

try_.chromium_linux_builder(
    name = "linux-blink-heap-verification-try",
)

try_.chromium_linux_builder(
    name = "linux-clang-tidy-dbg",
    executable = "recipe:tricium_clang_tidy_wrapper",
    goma_jobs = goma.jobs.J150,
)

try_.chromium_linux_builder(
    name = "linux-clang-tidy-rel",
    executable = "recipe:tricium_clang_tidy_wrapper",
    goma_jobs = goma.jobs.J150,
)

try_.chromium_linux_builder(
    name = "linux-dcheck-off-rel",
)

try_.chromium_linux_builder(
    name = "linux-gcc-rel",
    goma_backend = None,
)

try_.chromium_linux_builder(
    name = "linux-perfetto-rel",
    tryjob = try_.job(
        experiment_percentage = 100,
        location_regexp = [
            ".+/[+]/base/trace_event/.+",
            ".+/[+]/base/tracing/.+",
            ".+/[+]/components/tracing/.+",
            ".+/[+]/content/browser/tracing/.+",
            ".+/[+]/services/tracing/.+",
        ],
    ),
)

try_.chromium_linux_builder(
    name = "linux-trusty-rel",
    goma_jobs = goma.jobs.J150,
    os = os.LINUX_TRUSTY,
)

try_.chromium_linux_builder(
    name = "linux-viz-rel",
)

try_.chromium_linux_builder(
    name = "linux-webkit-msan-rel",
)

try_.chromium_linux_builder(
    name = "linux_chromium_analysis",
)

try_.chromium_linux_builder(
    name = "linux_chromium_archive_rel_ng",
)

try_.chromium_linux_builder(
    name = "linux_chromium_cfi_rel_ng",
    cores = 32,
)

try_.chromium_linux_builder(
    name = "linux_chromium_chromeos_asan_rel_ng",
    goma_jobs = goma.jobs.J150,
)

try_.chromium_linux_builder(
    name = "linux_chromium_chromeos_msan_rel_ng",
    goma_jobs = goma.jobs.J150,
)

try_.chromium_linux_builder(
    name = "linux_chromium_clobber_deterministic",
    executable = "recipe:swarming/deterministic_build",
    execution_timeout = 6 * time.hour,
)

try_.chromium_linux_builder(
    name = "linux_chromium_clobber_rel_ng",
)

try_.chromium_linux_builder(
    name = "linux_chromium_compile_dbg_32_ng",
)

try_.chromium_linux_builder(
    name = "linux_chromium_compile_rel_ng",
)

try_.chromium_linux_builder(
    name = "linux_chromium_msan_rel_ng",
    goma_jobs = goma.jobs.J150,
)

try_.chromium_linux_builder(
    name = "linux_chromium_ubsan_rel_ng",
)

try_.chromium_linux_builder(
    name = "linux-layout-tests-edit-ng",
)

try_.chromium_linux_builder(
    name = "linux-autofill-assistant",
)

try_.chromium_linux_builder(
    name = "linux-layout-tests-fragment-item",
)

try_.chromium_linux_builder(
    name = "linux_mojo",
)

try_.chromium_linux_builder(
    name = "linux_mojo_chromeos",
)

try_.chromium_linux_builder(
    name = "linux_upload_clang",
    builderless = True,
    cores = 32,
    executable = "recipe:chromium_upload_clang",
    goma_backend = None,
    os = os.LINUX_TRUSTY,
)

try_.chromium_linux_builder(
    name = "linux-wpt-fyi-rel",
)

try_.chromium_linux_builder(
    name = "network_service_linux",
)

try_.chromium_linux_builder(
    name = "tricium-metrics-analysis",
    executable = "recipe:tricium_metrics",
)

try_.chromium_mac_builder(
    name = "mac-coverage-rel",
    use_clang_coverage = True,
    goma_jobs = goma.jobs.J150,
    tryjob = try_.job(experiment_percentage = 3),
)

try_.chromium_mac_builder(
    name = "mac-osxbeta-rel",
    os = os.MAC_DEFAULT,
)

# NOTE: the following trybots aren't sensitive to Mac version on which
# they are built, hence no additional dimension is specified.
# The 10.xx version translates to which bots will run isolated tests.
try_.chromium_mac_builder(
    name = "mac_chromium_10.10",
)

try_.chromium_mac_builder(
    name = "mac_chromium_10.12_rel_ng",
)

try_.chromium_mac_builder(
    name = "mac_chromium_10.13_rel_ng",
)

try_.chromium_mac_builder(
    name = "mac_chromium_10.14_rel_ng",
)

try_.chromium_mac_builder(
    name = "mac_chromium_10.15_rel_ng",
)

try_.chromium_mac_builder(
    name = "mac_chromium_archive_rel_ng",
)

try_.chromium_mac_builder(
    name = "mac_chromium_asan_rel_ng",
    goma_jobs = goma.jobs.J150,
)

try_.chromium_mac_builder(
    name = "mac_chromium_compile_rel_ng",
)

try_.chromium_mac_builder(
    name = "mac_chromium_dbg_ng",
)

try_.chromium_mac_builder(
    name = "mac_upload_clang",
    builderless = False,
    caches = [
        swarming.cache(
            name = "xcode_mac_9a235",
            path = "xcode_mac_9a235.app",
        ),
    ],
    executable = "recipe:chromium_upload_clang",
    execution_timeout = 6 * time.hour,
    goma_backend = None,  # Does not use Goma.
    properties = {
        "$depot_tools/osx_sdk": {
            "sdk_version": "9a235",
        },
    },
)

try_.chromium_mac_ios_builder(
    name = "ios-device",
    executable = "recipe:chromium_trybot",
)

try_.chromium_mac_ios_builder(
    name = "ios-simulator-code-coverage",
    executable = "recipe:chromium_trybot",
    use_clang_coverage = True,
    coverage_exclude_sources = "ios_test_files_and_test_utils",
    coverage_test_types = ["unit"],
    os = os.MAC_10_15,
    properties = {
        "xcode_build_version": "12a8179i_audio",
    },
    tryjob = try_.job(experiment_percentage = 3),
)

try_.chromium_mac_ios_builder(
    name = "ios-simulator-cr-recipe",
    executable = "recipe:chromium_trybot",
)

try_.chromium_mac_ios_builder(
    name = "ios-simulator-eg",
)

try_.chromium_mac_ios_builder(
    name = "ios-simulator-multi-window",
    executable = "recipe:chromium_trybot",
)

try_.chromium_mac_ios_builder(
    name = "ios-simulator-noncq",
)

try_.chromium_mac_ios_builder(
    name = "ios13-beta-simulator",
    executable = "recipe:chromium_trybot",
    caches = [xcode_cache.x12a8179i_audio],
    os = os.MAC_10_15,
    properties = {
        "xcode_build_version": "12a8179i_audio",
    },
)

try_.chromium_mac_ios_builder(
    name = "ios13-sdk-simulator",
    executable = "recipe:chromium_trybot",
    caches = [xcode_cache.x12a8179i_audio],
    os = os.MAC_10_15,
    properties = {
        "xcode_build_version": "12a8179i_audio",
    },
)

try_.chromium_mac_ios_builder(
    name = "ios14-beta-simulator",
    executable = "recipe:chromium_trybot",
    caches = [xcode_cache.x12a8179i_audio],
    os = os.MAC_10_15,
    properties = {
        "xcode_build_version": "12a8179i_audio",
    },
)

try_.chromium_mac_ios_builder(
    name = "ios14-sdk-simulator",
    executable = "recipe:chromium_trybot",
    caches = [xcode_cache.x12a8189h],
    os = os.MAC_10_15,
    properties = {
        "xcode_build_version": "12a8179i_audio",
    },
)

try_.chromium_win_builder(
    name = "win-annotator-rel",
)

try_.chromium_win_builder(
    name = "win-asan",
    goma_jobs = goma.jobs.J150,
)

try_.chromium_win_builder(
    name = "win-celab-try-rel",
    executable = "recipe:celab",
    properties = {
        "exclude": "chrome_only",
        "pool_name": "celab-chromium-try",
        "pool_size": 20,
        "tests": "*",
    },
)

try_.chromium_win_builder(
    name = "win10_chromium_x64_dbg_ng",
    os = os.WINDOWS_10,
)

try_.chromium_win_builder(
    name = "win10_chromium_x64_rel_ng_exp",
    builderless = False,
    os = os.WINDOWS_ANY,
)

try_.chromium_win_builder(
    name = "win_archive",
)

try_.chromium_win_builder(
    name = "win_chromium_compile_rel_ng",
)

try_.chromium_win_builder(
    name = "win_chromium_dbg_ng",
)

try_.chromium_win_builder(
    name = "win_chromium_x64_rel_ng",
)

try_.chromium_win_builder(
    name = "win_mojo",
)

try_.chromium_win_builder(
    name = "win_upload_clang",
    builderless = False,
    cores = 32,
    executable = "recipe:chromium_upload_clang",
    goma_backend = None,
    os = os.WINDOWS_ANY,
)

try_.chromium_win_builder(
    name = "win_x64_archive",
)

# Used for listing chrome trybots in chromium's commit-queue.cfg without also
# adding them to chromium's cr-buildbucket.cfg. Note that the recipe these
# builders run allow only known roller accounts when triggered via the CQ.
def chrome_internal_verifier(
        *,
        builder):
    luci.cq_tryjob_verifier(
        builder = "chrome:try/" + builder,
        cq_group = "cq",
        includable_only = True,
        owner_whitelist = [
            "googlers",
            "project-chromium-robot-committers",
        ],
    )

chrome_internal_verifier(
    builder = "chromeos-betty-chrome",
)

chrome_internal_verifier(
    builder = "chromeos-betty-pi-arc-chrome",
)

chrome_internal_verifier(
    builder = "chromeos-eve-chrome",
)

chrome_internal_verifier(
    builder = "chromeos-eve-compile-chrome",
)

chrome_internal_verifier(
    builder = "chromeos-kevin-chrome",
)

chrome_internal_verifier(
    builder = "chromeos-kevin-compile-chrome",
)

chrome_internal_verifier(
    builder = "ipad-device",
)

chrome_internal_verifier(
    builder = "iphone-device",
)

chrome_internal_verifier(
    builder = "linux-chromeos-chrome",
)

chrome_internal_verifier(
    builder = "mac-chrome",
)

chrome_internal_verifier(
    builder = "win-chrome",
)

chrome_internal_verifier(
    builder = "win64-chrome",
)
