# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

load("//lib/branches.star", "branches")
load("//lib/builders.star", "goma", "os", "xcode_cache")
load("//lib/try.star", "try_")
load("//project.star", "settings")

try_.declare_bucket(settings, branch_selector = branches.ALL_RELEASES)

try_.set_defaults(
    settings,
    add_to_list_view = True,
    main_list_view = settings.main_list_view_name,
)

# Automatically maintained consoles

try_.list_view(
    name = "tryserver.blink",
    branch_selector = branches.STANDARD_RELEASES,
)

try_.list_view(
    name = "tryserver.chromium",
    branch_selector = branches.STANDARD_RELEASES,
)

try_.list_view(
    name = "tryserver.chromium.android",
    branch_selector = branches.STANDARD_RELEASES,
)

try_.list_view(
    name = "tryserver.chromium.angle",
)

try_.list_view(
    name = "tryserver.chromium.chromiumos",
    branch_selector = branches.STANDARD_RELEASES,
)

try_.list_view(
    name = "tryserver.chromium.codesearch",
)

try_.list_view(
    name = "tryserver.chromium.dawn",
    branch_selector = branches.STANDARD_RELEASES,
)

try_.list_view(
    name = "tryserver.chromium.linux",
    branch_selector = branches.STANDARD_RELEASES,
)

try_.list_view(
    name = "tryserver.chromium.mac",
    branch_selector = branches.STANDARD_RELEASES,
)

try_.list_view(
    name = "tryserver.chromium.swangle",
)

try_.list_view(
    name = "tryserver.chromium.win",
    branch_selector = branches.STANDARD_RELEASES,
)

# Builders are sorted first lexicographically by the function used to define
# them, then lexicographically by their name

try_.blink_builder(
    name = "linux-blink-rel",
    branch_selector = branches.STANDARD_RELEASES,
    goma_backend = goma.backend.RBE_PROD,
    tryjob = try_.job(
        location_regexp = [
            ".+/[+]/cc/.+",
            ".+/[+]/third_party/blink/renderer/core/paint/.+",
            ".+/[+]/third_party/blink/renderer/core/svg/.+",
            ".+/[+]/third_party/blink/renderer/platform/graphics/.+",
        ],
    ),
)

try_.chromium_builder(
    name = "android-official",
    branch_selector = branches.STANDARD_RELEASES,
    cores = 32,
)

try_.chromium_builder(
    name = "fuchsia-official",
    branch_selector = branches.STANDARD_RELEASES,
    cores = 32,
)

try_.chromium_builder(
    name = "linux-official",
    branch_selector = branches.STANDARD_RELEASES,
    cores = 32,
)

try_.chromium_builder(
    name = "win-official",
    branch_selector = branches.STANDARD_RELEASES,
    os = os.WINDOWS_DEFAULT,
    cores = 32,
)

try_.chromium_builder(
    name = "win32-official",
    branch_selector = branches.STANDARD_RELEASES,
    os = os.WINDOWS_DEFAULT,
    cores = 32,
)

try_.chromium_android_builder(
    name = "android-binary-size",
    branch_selector = branches.STANDARD_RELEASES,
    executable = "recipe:binary_size_trybot",
    goma_jobs = goma.jobs.J150,
    properties = {
        "$build/binary_size": {
            "analyze_targets": [
                "//chrome/android:validate_expectations",
                "//chrome/android:monochrome_public_minimal_apks",
                "//tools/binary_size:binary_size_trybot_py",
            ],
            "compile_targets": [
                "monochrome_public_minimal_apks",
                "monochrome_static_initializers",
                "validate_expectations",
            ],
        },
    },
    tryjob = try_.job(),
)

try_.chromium_android_builder(
    name = "android-cronet-arm-dbg",
    branch_selector = branches.STANDARD_RELEASES,
    tryjob = try_.job(
        location_regexp = [
            ".+/[+]/components/cronet/.+",
            ".+/[+]/components/grpc_support/.+",
            ".+/[+]/build/android/.+",
            ".+/[+]/build/config/android/.+",
        ],
        location_regexp_exclude = [
            ".+/[+]/components/cronet/ios/.+",
        ],
    ),
)

try_.chromium_android_builder(
    name = "android-lollipop-arm-rel",
    branch_selector = branches.STANDARD_RELEASES,
    goma_jobs = goma.jobs.J150,
    tryjob = try_.job(),
)

try_.chromium_android_builder(
    name = "android-marshmallow-arm64-rel",
    branch_selector = branches.STANDARD_RELEASES,
    cores = 16,
    goma_jobs = goma.jobs.J300,
    ssd = True,
    use_java_coverage = True,
    tryjob = try_.job(),
)

try_.chromium_android_builder(
    name = "android-marshmallow-x86-rel",
    branch_selector = branches.STANDARD_RELEASES,
    goma_jobs = goma.jobs.J150,
    tryjob = try_.job(
        experiment_percentage = 5,
    ),
)

# TODO(crbug.com/1111436) Added it back once all Pixel 1s are flashed
# back to NJH47F
#try_.chromium_android_builder(
#    name = "android-nougat-arm64-rel",
#    branch_selector = branches.STANDARD_RELEASES,
#    goma_jobs = goma.jobs.J150,
#)

try_.chromium_android_builder(
    name = "android-pie-arm64-dbg",
    branch_selector = branches.STANDARD_RELEASES,
    goma_jobs = goma.jobs.J300,
    tryjob = try_.job(
        location_regexp = [
            ".+/[+]/chrome/android/features/vr/.+",
            ".+/[+]/chrome/android/java/src/org/chromium/chrome/browser/vr/.+",
            ".+/[+]/chrome/android/javatests/src/org/chromium/chrome/browser/vr/.+",
            ".+/[+]/chrome/browser/android/vr/.+",
            ".+/[+]/chrome/browser/vr/.+",
            ".+/[+]/content/browser/xr/.+",
            ".+/[+]/device/vr/android/.+",
            ".+/[+]/third_party/gvr-android-sdk/.+",
            ".+/[+]/third_party/arcore-android-sdk/.+",
            ".+/[+]/third_party/arcore-android-sdk-client/.+",
        ],
    ),
)

try_.chromium_android_builder(
    name = "android-pie-arm64-rel",
    branch_selector = branches.STANDARD_RELEASES,
    cores = 16,
    goma_jobs = goma.jobs.J300,
    ssd = True,
    # TODO(crbug.com/1111436): Enable on CQ once the maintenance for
    # Pixel 2 devices are finished.
    #tryjob = try_.job(),
)

try_.chromium_android_builder(
    name = "android_compile_dbg",
    branch_selector = branches.STANDARD_RELEASES,
    goma_jobs = goma.jobs.J150,
    tryjob = try_.job(),
)

try_.chromium_android_builder(
    name = "android_compile_x64_dbg",
    branch_selector = branches.STANDARD_RELEASES,
    tryjob = try_.job(
        location_regexp = [
            ".+/[+]/chrome/android/java/src/org/chromium/chrome/browser/vr/.+",
            ".+/[+]/chrome/browser/vr/.+",
            ".+/[+]/content/browser/xr/.+",
            ".+/[+]/sandbox/linux/seccomp-bpf/.+",
            ".+/[+]/sandbox/linux/seccomp-bpf-helpers/.+",
            ".+/[+]/sandbox/linux/system_headers/.+",
            ".+/[+]/sandbox/linux/tests/.+",
            ".+/[+]/third_party/gvr-android-sdk/.+",
        ],
    ),
)

try_.chromium_android_builder(
    name = "android_compile_x86_dbg",
    branch_selector = branches.STANDARD_RELEASES,
    tryjob = try_.job(
        location_regexp = [
            ".+/[+]/chrome/android/java/src/org/chromium/chrome/browser/vr/.+",
            ".+/[+]/chrome/browser/vr/.+",
            ".+/[+]/content/browser/xr/.+",
            ".+/[+]/sandbox/linux/seccomp-bpf/.+",
            ".+/[+]/sandbox/linux/seccomp-bpf-helpers/.+",
            ".+/[+]/sandbox/linux/system_headers/.+",
            ".+/[+]/sandbox/linux/tests/.+",
            ".+/[+]/third_party/gvr-android-sdk/.+",
        ],
    ),
)

try_.chromium_android_builder(
    name = "android_cronet",
    branch_selector = branches.STANDARD_RELEASES,
    tryjob = try_.job(),
)

try_.chromium_android_builder(
    name = "cast_shell_android",
    branch_selector = branches.STANDARD_RELEASES,
    tryjob = try_.job(),
)

try_.chromium_chromiumos_builder(
    name = "chromeos-amd64-generic-dbg",
    branch_selector = branches.STANDARD_RELEASES,
    tryjob = try_.job(
        location_regexp = [
            ".+/[+]/content/gpu/.+",
            ".+/[+]/media/.+",
        ],
    ),
)

try_.chromium_chromiumos_builder(
    name = "chromeos-amd64-generic-rel",
    branch_selector = branches.STANDARD_RELEASES,
    tryjob = try_.job(),
)

try_.chromium_chromiumos_builder(
    name = "chromeos-arm-generic-rel",
    branch_selector = branches.STANDARD_RELEASES,
    tryjob = try_.job(),
)

try_.chromium_chromiumos_builder(
    name = "linux-chromeos-compile-dbg",
    branch_selector = branches.STANDARD_RELEASES,
    tryjob = try_.job(),
)

try_.chromium_chromiumos_builder(
    name = "linux-chromeos-rel",
    branch_selector = branches.STANDARD_RELEASES,
    goma_jobs = goma.jobs.J150,
    tryjob = try_.job(cancel_stale = False),
    use_clang_coverage = True,
)

try_.chromium_dawn_builder(
    name = "dawn-linux-x64-deps-rel",
    branch_selector = branches.STANDARD_RELEASES,
    tryjob = try_.job(
        location_regexp = [
            ".+/[+]/gpu/.+",
            ".+/[+]/testing/buildbot/chromium.dawn.json",
            ".+/[+]/third_party/blink/renderer/modules/webgpu/.+",
            ".+/[+]/third_party/blink/web_tests/external/wpt/webgpu/.+",
            ".+/[+]/third_party/blink/web_tests/wpt_internal/webgpu/.+",
            ".+/[+]/third_party/blink/web_tests/WebGPUExpectations",
            ".+/[+]/third_party/dawn/.+",
            ".+/[+]/tools/clang/scripts/update.py",
            ".+/[+]/ui/gl/features.gni",
        ],
    ),
)

try_.chromium_dawn_builder(
    name = "dawn-mac-x64-deps-rel",
    branch_selector = branches.STANDARD_RELEASES,
    os = os.MAC_ANY,
    tryjob = try_.job(
        location_regexp = [
            ".+/[+]/gpu/.+",
            ".+/[+]/testing/buildbot/chromium.dawn.json",
            ".+/[+]/third_party/blink/renderer/modules/webgpu/.+",
            ".+/[+]/third_party/blink/web_tests/external/wpt/webgpu/.+",
            ".+/[+]/third_party/blink/web_tests/wpt_internal/webgpu/.+",
            ".+/[+]/third_party/blink/web_tests/WebGPUExpectations",
            ".+/[+]/third_party/dawn/.+",
            ".+/[+]/tools/clang/scripts/update.py",
            ".+/[+]/ui/gl/features.gni",
        ],
    ),
)

try_.chromium_dawn_builder(
    name = "dawn-win10-x64-deps-rel",
    branch_selector = branches.STANDARD_RELEASES,
    os = os.WINDOWS_ANY,
    tryjob = try_.job(
        location_regexp = [
            ".+/[+]/gpu/.+",
            ".+/[+]/testing/buildbot/chromium.dawn.json",
            ".+/[+]/third_party/blink/renderer/modules/webgpu/.+",
            ".+/[+]/third_party/blink/web_tests/external/wpt/webgpu/.+",
            ".+/[+]/third_party/blink/web_tests/wpt_internal/webgpu/.+",
            ".+/[+]/third_party/blink/web_tests/WebGPUExpectations",
            ".+/[+]/third_party/dawn/.+",
            ".+/[+]/tools/clang/scripts/update.py",
            ".+/[+]/ui/gl/features.gni",
        ],
    ),
)

try_.chromium_dawn_builder(
    name = "dawn-win10-x86-deps-rel",
    branch_selector = branches.STANDARD_RELEASES,
    os = os.WINDOWS_ANY,
    tryjob = try_.job(
        location_regexp = [
            ".+/[+]/gpu/.+",
            ".+/[+]/testing/buildbot/chromium.dawn.json",
            ".+/[+]/third_party/blink/renderer/modules/webgpu/.+",
            ".+/[+]/third_party/blink/web_tests/external/wpt/webgpu/.+",
            ".+/[+]/third_party/blink/web_tests/wpt_internal/webgpu/.+",
            ".+/[+]/third_party/blink/web_tests/WebGPUExpectations",
            ".+/[+]/third_party/dawn/.+",
            ".+/[+]/tools/clang/scripts/update.py",
            ".+/[+]/ui/gl/features.gni",
        ],
    ),
)

try_.chromium_linux_builder(
    name = "cast_shell_linux",
    branch_selector = branches.STANDARD_RELEASES,
    tryjob = try_.job(),
)

try_.chromium_linux_builder(
    name = "closure_compilation",
    branch_selector = branches.STANDARD_RELEASES,
    executable = "recipe:closure_compilation",
    tryjob = try_.job(
        location_regexp = [
            ".+/[+]/third_party/closure_compiler/.+",
        ],
    ),
)

try_.chromium_linux_builder(
    name = "chromium_presubmit",
    branch_selector = branches.STANDARD_RELEASES,
    executable = "recipe:presubmit",
    goma_backend = None,
    properties = {
        "$depot_tools/presubmit": {
            "runhooks": True,
            "timeout_s": 480,
        },
        "repo_name": "chromium",
    },
    tryjob = try_.job(
        disable_reuse = True,
        add_default_excludes = False,
    ),
)

try_.chromium_linux_builder(
    name = "fuchsia-arm64-cast",
    branch_selector = branches.STANDARD_RELEASES,
    tryjob = try_.job(
        location_regexp = [
            ".+/[+]/chromecast/.+",
        ],
    ),
)

try_.chromium_linux_builder(
    name = "fuchsia-x64-cast",
    branch_selector = branches.STANDARD_RELEASES,
    tryjob = try_.job(),
)

try_.chromium_linux_builder(
    name = "fuchsia_arm64",
    branch_selector = branches.STANDARD_RELEASES,
    tryjob = try_.job(),
)

try_.chromium_linux_builder(
    name = "fuchsia_x64",
    branch_selector = branches.STANDARD_RELEASES,
    tryjob = try_.job(),
)

try_.chromium_linux_builder(
    name = "linux-libfuzzer-asan-rel",
    branch_selector = branches.STANDARD_RELEASES,
    executable = "recipe:chromium_libfuzzer_trybot",
    tryjob = try_.job(),
)

try_.chromium_linux_builder(
    name = "linux-ozone-rel",
    branch_selector = branches.STANDARD_RELEASES,
    tryjob = try_.job(),
)

try_.chromium_linux_builder(
    name = "linux-rel",
    branch_selector = branches.STANDARD_RELEASES,
    # TODO(https://crbug.com/1109276) Once support for mastername is removed, do
    # not explicitly set
    builder_group = "tryserver.chromium.linux",
    goma_jobs = goma.jobs.J150,
    tryjob = try_.job(),
    use_clang_coverage = True,
)

try_.chromium_linux_builder(
    name = "linux_chromium_asan_rel_ng",
    branch_selector = branches.STANDARD_RELEASES,
    goma_jobs = goma.jobs.J150,
    ssd = True,
    tryjob = try_.job(),
)

try_.chromium_linux_builder(
    name = "linux_chromium_compile_dbg_ng",
    branch_selector = branches.STANDARD_RELEASES,
    caches = [
        swarming.cache(
            name = "builder",
            path = "linux_debug",
        ),
    ],
    goma_jobs = goma.jobs.J150,
    tryjob = try_.job(),
)

try_.chromium_linux_builder(
    name = "linux_chromium_dbg_ng",
    branch_selector = branches.STANDARD_RELEASES,
    caches = [
        swarming.cache(
            name = "builder",
            path = "linux_debug",
        ),
    ],
    tryjob = try_.job(
        location_regexp = [
            ".+/[+]/build/.*check_gn_headers.*",
        ],
    ),
)

try_.chromium_linux_builder(
    name = "linux_chromium_tsan_rel_ng",
    branch_selector = branches.STANDARD_RELEASES,
    goma_jobs = goma.jobs.J150,
    tryjob = try_.job(),
)

try_.chromium_linux_builder(
    name = "linux_layout_tests_composite_after_paint",
    branch_selector = branches.STANDARD_RELEASES,
    tryjob = try_.job(
        location_regexp = [
            ".+/[+]/third_party/blink/renderer/core/paint/.+",
            ".+/[+]/third_party/blink/renderer/core/svg/.+",
            ".+/[+]/third_party/blink/renderer/platform/graphics/.+",
            ".+/[+]/third_party/blink/web_tests/.+",
        ],
    ),
)

try_.chromium_linux_builder(
    name = "linux_layout_tests_layout_ng_disabled",
    branch_selector = branches.STANDARD_RELEASES,
    tryjob = try_.job(
        location_regexp = [
            ".+/[+]/third_party/blink/renderer/core/editing/.+",
            ".+/[+]/third_party/blink/renderer/core/layout/.+",
            ".+/[+]/third_party/blink/renderer/core/paint/.+",
            ".+/[+]/third_party/blink/renderer/core/svg/.+",
            ".+/[+]/third_party/blink/renderer/platform/fonts/shaping/.+",
            ".+/[+]/third_party/blink/renderer/platform/graphics/.+",
            ".+/[+]/third_party/blink/web_tests/FlagExpectations/disable-layout-ng",
            ".+/[+]/third_party/blink/web_tests/flag-specific/disable-layout-ng/.+",
        ],
    ),
)

try_.chromium_linux_builder(
    name = "linux_vr",
    branch_selector = branches.STANDARD_RELEASES,
    tryjob = try_.job(
        location_regexp = [
            ".+/[+]/chrome/browser/vr/.+",
            ".+/[+]/content/browser/xr/.+",
        ],
    ),
)

try_.chromium_mac_builder(
    name = "mac-rel",
    branch_selector = branches.STANDARD_RELEASES,
    goma_jobs = goma.jobs.J150,
    os = os.MAC_10_13,
    tryjob = try_.job(),
)

try_.chromium_mac_builder(
    name = "mac-arm64-rel",
    branch_selector = branches.STANDARD_RELEASES,
    goma_jobs = goma.jobs.J150,
    os = os.MAC_10_15,
)

try_.chromium_mac_builder(
    name = "mac_chromium_compile_dbg_ng",
    branch_selector = branches.STANDARD_RELEASES,
    goma_jobs = goma.jobs.J150,
    os = os.MAC_10_13,
    tryjob = try_.job(),
)

try_.chromium_mac_ios_builder(
    name = "ios-simulator",
    branch_selector = branches.STANDARD_RELEASES,
    executable = "recipe:chromium_trybot",
    properties = {
        "xcode_build_version": "12a8189n",
    },
    tryjob = try_.job(),
)

try_.chromium_mac_ios_builder(
    name = "ios-simulator-cronet",
    branch_selector = branches.STANDARD_RELEASES,
    caches = [xcode_cache.x11e146],
    executable = "recipe:chromium_trybot",
    properties = {
        "xcode_build_version": "11e146",
    },
    tryjob = try_.job(
        location_regexp = [
            ".+/[+]/components/cronet/.+",
            ".+/[+]/components/grpc_support/.+",
            ".+/[+]/ios/.+",
        ],
        location_regexp_exclude = [
            ".+/[+]/components/cronet/android/.+",
        ],
    ),
)

try_.chromium_mac_ios_builder(
    name = "ios-simulator-full-configs",
    branch_selector = branches.STANDARD_RELEASES,
    executable = "recipe:chromium_trybot",
    tryjob = try_.job(
        location_regexp = [
            ".+/[+]/ios/.+",
        ],
    ),
)

try_.chromium_win_builder(
    name = "win-libfuzzer-asan-rel",
    branch_selector = branches.STANDARD_RELEASES,
    builderless = False,
    executable = "recipe:chromium_libfuzzer_trybot",
    os = os.WINDOWS_ANY,
    tryjob = try_.job(cancel_stale = False),
)

try_.chromium_win_builder(
    name = "win_chromium_compile_dbg_ng",
    branch_selector = branches.STANDARD_RELEASES,
    goma_jobs = goma.jobs.J150,
    tryjob = try_.job(cancel_stale = False),
)

try_.chromium_win_builder(
    name = "win10_chromium_x64_rel_ng",
    branch_selector = branches.STANDARD_RELEASES,
    goma_jobs = goma.jobs.J150,
    os = os.WINDOWS_10,
    ssd = True,
    use_clang_coverage = True,
    tryjob = try_.job(cancel_stale = False),
)

try_.chromium_win_builder(
    name = "win7-rel",
    branch_selector = branches.STANDARD_RELEASES,
    execution_timeout = 4 * time.hour + 30 * time.minute,
    goma_jobs = goma.jobs.J300,
    ssd = True,
    tryjob = try_.job(
        location_regexp = [
            ".+/[+]/sandbox/win/.+",
        ],
    ),
)

try_.gpu_chromium_android_builder(
    name = "android_optional_gpu_tests_rel",
    branch_selector = branches.STANDARD_RELEASES,
    tryjob = try_.job(
        location_regexp = [
            ".+/[+]/cc/.+",
            ".+/[+]/chrome/browser/vr/.+",
            ".+/[+]/content/browser/xr/.+",
            ".+/[+]/components/viz/.+",
            ".+/[+]/content/test/gpu/.+",
            ".+/[+]/gpu/.+",
            ".+/[+]/media/audio/.+",
            ".+/[+]/media/filters/.+",
            ".+/[+]/media/gpu/.+",
            ".+/[+]/services/viz/.+",
            ".+/[+]/testing/trigger_scripts/.+",
            ".+/[+]/third_party/blink/renderer/modules/webgl/.+",
            ".+/[+]/third_party/blink/renderer/platform/graphics/gpu/.+",
            ".+/[+]/tools/clang/scripts/update.py",
            ".+/[+]/ui/gl/.+",
        ],
    ),
)

try_.gpu_chromium_linux_builder(
    name = "linux_optional_gpu_tests_rel",
    branch_selector = branches.STANDARD_RELEASES,
    tryjob = try_.job(
        location_regexp = [
            ".+/[+]/chrome/browser/vr/.+",
            ".+/[+]/content/browser/xr/.+",
            ".+/[+]/content/test/gpu/.+",
            ".+/[+]/gpu/.+",
            ".+/[+]/media/audio/.+",
            ".+/[+]/media/filters/.+",
            ".+/[+]/media/gpu/.+",
            ".+/[+]/testing/buildbot/chromium.gpu.fyi.json",
            ".+/[+]/testing/trigger_scripts/.+",
            ".+/[+]/third_party/blink/renderer/modules/webgl/.+",
            ".+/[+]/third_party/blink/renderer/platform/graphics/gpu/.+",
            ".+/[+]/tools/clang/scripts/update.py",
            ".+/[+]/ui/gl/.+",
        ],
    ),
)

try_.gpu_chromium_mac_builder(
    name = "mac_optional_gpu_tests_rel",
    branch_selector = branches.STANDARD_RELEASES,
    tryjob = try_.job(
        location_regexp = [
            ".+/[+]/chrome/browser/vr/.+",
            ".+/[+]/content/browser/xr/.+",
            ".+/[+]/content/test/gpu/.+",
            ".+/[+]/gpu/.+",
            ".+/[+]/media/audio/.+",
            ".+/[+]/media/filters/.+",
            ".+/[+]/media/gpu/.+",
            ".+/[+]/services/shape_detection/.+",
            ".+/[+]/testing/buildbot/chromium.gpu.fyi.json",
            ".+/[+]/testing/trigger_scripts/.+",
            ".+/[+]/third_party/blink/renderer/modules/webgl/.+",
            ".+/[+]/third_party/blink/renderer/platform/graphics/gpu/.+",
            ".+/[+]/tools/clang/scripts/update.py",
            ".+/[+]/ui/gl/.+",
        ],
    ),
)

try_.gpu_chromium_win_builder(
    name = "win_optional_gpu_tests_rel",
    branch_selector = branches.STANDARD_RELEASES,
    builderless = True,
    os = os.WINDOWS_DEFAULT,
    tryjob = try_.job(
        location_regexp = [
            ".+/[+]/chrome/browser/vr/.+",
            ".+/[+]/content/browser/xr/.+",
            ".+/[+]/content/test/gpu/.+",
            ".+/[+]/device/vr/.+",
            ".+/[+]/gpu/.+",
            ".+/[+]/media/audio/.+",
            ".+/[+]/media/filters/.+",
            ".+/[+]/media/gpu/.+",
            ".+/[+]/testing/buildbot/chromium.gpu.fyi.json",
            ".+/[+]/testing/trigger_scripts/.+",
            ".+/[+]/third_party/blink/renderer/modules/vr/.+",
            ".+/[+]/third_party/blink/renderer/modules/webgl/.+",
            ".+/[+]/third_party/blink/renderer/modules/xr/.+",
            ".+/[+]/third_party/blink/renderer/platform/graphics/gpu/.+",
            ".+/[+]/tools/clang/scripts/update.py",
            ".+/[+]/ui/gl/.+",
        ],
    ),
)

# Used for listing chrome trybots in chromium's commit-queue.cfg without also
# adding them to chromium's cr-buildbucket.cfg. Note that the recipe these
# builders run allow only known roller accounts when triggered via the CQ.
def chrome_internal_verifier(
        *,
        builder,
        **kwargs):
    luci.cq_tryjob_verifier(
        builder = "chrome:try/" + builder,
        cq_group = settings.cq_group,
        includable_only = True,
        owner_whitelist = [
            "googlers",
            "project-chromium-robot-committers",
        ],
    )

chrome_internal_verifier(
    builder = "linux-chrome-beta",
)

chrome_internal_verifier(
    builder = "linux-chrome-stable",
)

chrome_internal_verifier(
    builder = "mac-chrome-beta",
    branch_selector = branches.STANDARD_RELEASES,
)

chrome_internal_verifier(
    builder = "mac-chrome-stable",
    branch_selector = branches.STANDARD_RELEASES,
)

chrome_internal_verifier(
    builder = "win-chrome-beta",
    branch_selector = branches.STANDARD_RELEASES,
)

chrome_internal_verifier(
    builder = "win-chrome-stable",
    branch_selector = branches.STANDARD_RELEASES,
)

chrome_internal_verifier(
    builder = "win64-chrome-beta",
    branch_selector = branches.STANDARD_RELEASES,
)

chrome_internal_verifier(
    builder = "win64-chrome-stable",
    branch_selector = branches.STANDARD_RELEASES,
)
