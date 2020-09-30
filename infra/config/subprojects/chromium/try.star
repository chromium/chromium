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
    subproject_list_view = "luci.chromium.try",
)

# Automatically maintained consoles

try_.list_view(
    name = "luci.chromium.try",
    branch_selector = branches.ALL_RELEASES,
)

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
    branch_selector = branches.ALL_RELEASES,
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
    name = "linux-blink-optional-highdpi-rel",
    goma_backend = goma.backend.RBE_PROD,
)

try_.blink_builder(
    name = "linux-blink-rel",
    branch_selector = branches.STANDARD_RELEASES,
    goma_backend = goma.backend.RBE_PROD,
    main_list_view = settings.main_list_view_name,
    tryjob = try_.job(
        location_regexp = [
            ".+/[+]/cc/.+",
            ".+/[+]/third_party/blink/renderer/core/paint/.+",
            ".+/[+]/third_party/blink/renderer/core/svg/.+",
            ".+/[+]/third_party/blink/renderer/platform/graphics/.+",
        ],
    ),
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

try_.blink_mac_builder(
    name = "mac11.0-blink-rel",
    builderless = False,
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
    name = "mac-official",
    branch_selector = branches.STANDARD_RELEASES,
    cores = None,
    os = os.MAC_ANY,
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
    name = "android-10-arm64-rel",
)

try_.chromium_android_builder(
    name = "android-asan",
)

try_.chromium_android_builder(
    name = "android-bfcache-rel",
)

try_.chromium_android_builder(
    name = "android-binary-size",
    branch_selector = branches.STANDARD_RELEASES,
    executable = "recipe:binary_size_trybot",
    goma_jobs = goma.jobs.J150,
    main_list_view = settings.main_list_view_name,
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
    main_list_view = settings.main_list_view_name,
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
    name = "android-cronet-marshmallow-arm64-rel",
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
    name = "android-lollipop-arm-rel",
    branch_selector = branches.STANDARD_RELEASES,
    goma_jobs = goma.jobs.J150,
    main_list_view = settings.main_list_view_name,
    tryjob = try_.job(),
)

try_.chromium_android_builder(
    name = "android-marshmallow-arm64-rel",
    branch_selector = branches.STANDARD_RELEASES,
    cores = 16,
    goma_jobs = goma.jobs.J300,
    main_list_view = settings.main_list_view_name,
    ssd = True,
    use_java_coverage = True,
    tryjob = try_.job(),
)

try_.chromium_android_builder(
    name = "android-marshmallow-x86-rel",
    branch_selector = branches.STANDARD_RELEASES,
    cores = 16,
    goma_jobs = goma.jobs.J300,
    ssd = True,
    tryjob = try_.job(
        experiment_percentage = 20,
    ),
)

try_.chromium_android_builder(
    name = "android-marshmallow-x86-rel-non-cq",
)

# TODO(crbug.com/1111436) Added it back once all Pixel 1s are flashed
# back to NJH47F
#try_.chromium_android_builder(
#    name = "android-nougat-arm64-rel",
#    branch_selector = branches.STANDARD_RELEASES,
#    goma_jobs = goma.jobs.J150,
#    main_list_view = settings.main_list_view_name,
#)

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
    name = "android-pie-arm64-dbg",
    branch_selector = branches.STANDARD_RELEASES,
    goma_jobs = goma.jobs.J300,
    main_list_view = settings.main_list_view_name,
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
    # TODO(crbug.com/1111436): Enable on CQ fully once the tests run fine.
    main_list_view = settings.main_list_view_name,
    tryjob = try_.job(
        experiment_percentage = 60,
    ),
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
    name = "android-pie-arm64-wpt-rel-non-cq",
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
    name = "android_compile_dbg",
    branch_selector = branches.STANDARD_RELEASES,
    goma_jobs = goma.jobs.J150,
    main_list_view = settings.main_list_view_name,
    tryjob = try_.job(),
)

try_.chromium_android_builder(
    name = "android_compile_x64_dbg",
    branch_selector = branches.STANDARD_RELEASES,
    main_list_view = settings.main_list_view_name,
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
    main_list_view = settings.main_list_view_name,
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
    main_list_view = settings.main_list_view_name,
    tryjob = try_.job(),
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
    name = "cast_shell_android",
    branch_selector = branches.STANDARD_RELEASES,
    main_list_view = settings.main_list_view_name,
    tryjob = try_.job(),
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
    name = "chromeos-amd64-generic-dbg",
    branch_selector = branches.STANDARD_RELEASES,
    main_list_view = settings.main_list_view_name,
    tryjob = try_.job(
        location_regexp = [
            ".+/[+]/content/gpu/.+",
            ".+/[+]/media/.+",
        ],
    ),
)

try_.chromium_chromiumos_builder(
    name = "chromeos-amd64-generic-rel",
    branch_selector = branches.ALL_RELEASES,
    main_list_view = settings.main_list_view_name,
    tryjob = try_.job(),
)

try_.chromium_chromiumos_builder(
    name = "chromeos-arm-generic-dbg",
)

try_.chromium_chromiumos_builder(
    name = "chromeos-arm-generic-rel",
    branch_selector = branches.ALL_RELEASES,
    main_list_view = settings.main_list_view_name,
    tryjob = try_.job(),
)

try_.chromium_chromiumos_builder(
    name = "linux-chromeos-compile-dbg",
    branch_selector = branches.STANDARD_RELEASES,
    main_list_view = settings.main_list_view_name,
    tryjob = try_.job(),
)

try_.chromium_chromiumos_builder(
    name = "chromeos-kevin-compile-rel",
)

try_.chromium_chromiumos_builder(
    name = "chromeos-kevin-rel",
    branch_selector = branches.ALL_RELEASES,
    main_list_view = settings.main_list_view_name,
    tryjob = try_.job(
        location_regexp = [
            ".+/[+]/build/chromeos/.+",
            ".+/[+]/build/config/chromeos/.*",
            ".+/[+]/chromeos/CHROMEOS_LKGM",
        ],
    ),
)

try_.chromium_chromiumos_builder(
    name = "linux-chromeos-rel",
    branch_selector = branches.ALL_RELEASES,
    goma_jobs = goma.jobs.J150,
    main_list_view = settings.main_list_view_name,
    tryjob = try_.job(cancel_stale = False),
    use_clang_coverage = True,
)

try_.chromium_chromiumos_builder(
    name = "linux-lacros-rel",
    main_list_view = settings.main_list_view_name,
    tryjob = try_.job(),
)

try_.chromium_chromiumos_builder(
    name = "linux-chromeos-dbg",
)

try_.chromium_dawn_builder(
    name = "dawn-linux-x64-deps-rel",
    branch_selector = branches.STANDARD_RELEASES,
    main_list_view = settings.main_list_view_name,
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
    main_list_view = settings.main_list_view_name,
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
    main_list_view = settings.main_list_view_name,
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
    main_list_view = settings.main_list_view_name,
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
    name = "cast_shell_linux",
    branch_selector = branches.STANDARD_RELEASES,
    main_list_view = settings.main_list_view_name,
    tryjob = try_.job(),
)

try_.chromium_linux_builder(
    name = "closure_compilation",
    branch_selector = branches.STANDARD_RELEASES,
    executable = "recipe:closure_compilation",
    main_list_view = settings.main_list_view_name,
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
    main_list_view = settings.main_list_view_name,
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
    main_list_view = settings.main_list_view_name,
    tryjob = try_.job(
        location_regexp = [
            ".+/[+]/chromecast/.+",
        ],
    ),
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
    name = "fuchsia-x64-cast",
    branch_selector = branches.STANDARD_RELEASES,
    main_list_view = settings.main_list_view_name,
    tryjob = try_.job(),
)

try_.chromium_linux_builder(
    name = "fuchsia_arm64",
    branch_selector = branches.STANDARD_RELEASES,
    main_list_view = settings.main_list_view_name,
    tryjob = try_.job(),
)

try_.chromium_linux_builder(
    name = "fuchsia_x64",
    branch_selector = branches.STANDARD_RELEASES,
    main_list_view = settings.main_list_view_name,
    tryjob = try_.job(),
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
    name = "linux-autofill-assistant",
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
    name = "linux-inverse-fieldtrials-fyi-rel",
)

try_.chromium_linux_builder(
    name = "linux-lacros-fyi-rel",
)

try_.chromium_linux_builder(
    name = "linux-layout-tests-edit-ng",
)

try_.chromium_linux_builder(
    name = "linux-layout-tests-fragment-item",
)

try_.chromium_linux_builder(
    name = "linux-libfuzzer-asan-rel",
    branch_selector = branches.STANDARD_RELEASES,
    executable = "recipe:chromium_libfuzzer_trybot",
    main_list_view = settings.main_list_view_name,
    tryjob = try_.job(),
)

try_.chromium_linux_builder(
    name = "linux-ozone-rel",
    branch_selector = branches.STANDARD_RELEASES,
    main_list_view = settings.main_list_view_name,
    tryjob = try_.job(),
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
    name = "linux-rel",
    branch_selector = branches.STANDARD_RELEASES,
    goma_jobs = goma.jobs.J150,
    main_list_view = settings.main_list_view_name,
    tryjob = try_.job(),
    use_clang_coverage = True,
)

try_.chromium_linux_builder(
    name = "linux-rel-builderful",
    builderless = False,
    goma_jobs = goma.jobs.J150,
    tryjob = try_.job(experiment_percentage = 10),
    use_clang_coverage = True,
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
    name = "linux-wpt-fyi-rel",
)

try_.chromium_linux_builder(
    name = "linux-wpt-identity-fyi-rel",
)

try_.chromium_linux_builder(
    name = "linux-wpt-payments-fyi-rel",
)

try_.chromium_linux_builder(
    name = "linux_chromium_analysis",
)

try_.chromium_linux_builder(
    name = "linux_chromium_archive_rel_ng",
)

try_.chromium_linux_builder(
    name = "linux_chromium_asan_rel_ng",
    branch_selector = branches.STANDARD_RELEASES,
    goma_jobs = goma.jobs.J150,
    ssd = True,
    main_list_view = settings.main_list_view_name,
    tryjob = try_.job(),
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
    name = "linux_chromium_compile_dbg_ng",
    branch_selector = branches.STANDARD_RELEASES,
    caches = [
        swarming.cache(
            name = "builder",
            path = "linux_debug",
        ),
    ],
    goma_jobs = goma.jobs.J150,
    main_list_view = settings.main_list_view_name,
    tryjob = try_.job(),
)

try_.chromium_linux_builder(
    name = "linux_chromium_compile_rel_ng",
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
    main_list_view = settings.main_list_view_name,
    tryjob = try_.job(
        location_regexp = [
            ".+/[+]/build/.*check_gn_headers.*",
        ],
    ),
)

try_.chromium_linux_builder(
    name = "linux_chromium_msan_rel_ng",
    goma_jobs = goma.jobs.J150,
)

try_.chromium_linux_builder(
    name = "linux_chromium_tsan_rel_ng",
    branch_selector = branches.STANDARD_RELEASES,
    goma_jobs = goma.jobs.J150,
    main_list_view = settings.main_list_view_name,
    tryjob = try_.job(),
)

try_.chromium_linux_builder(
    name = "linux_chromium_ubsan_rel_ng",
)

try_.chromium_linux_builder(
    name = "linux_layout_tests_composite_after_paint",
    branch_selector = branches.STANDARD_RELEASES,
    main_list_view = settings.main_list_view_name,
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
    main_list_view = settings.main_list_view_name,
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
    name = "linux_vr",
    branch_selector = branches.STANDARD_RELEASES,
    main_list_view = settings.main_list_view_name,
    tryjob = try_.job(
        location_regexp = [
            ".+/[+]/chrome/browser/vr/.+",
            ".+/[+]/content/browser/xr/.+",
        ],
    ),
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
    builderless = False,
    use_clang_coverage = True,
    goma_jobs = goma.jobs.J150,
    tryjob = try_.job(experiment_percentage = 8),
)

try_.chromium_mac_builder(
    name = "mac-osxbeta-rel",
    os = os.MAC_DEFAULT,
)

try_.chromium_mac_builder(
    name = "mac-rel",
    branch_selector = branches.STANDARD_RELEASES,
    use_clang_coverage = True,
    goma_jobs = goma.jobs.J150,
    main_list_view = settings.main_list_view_name,
    os = os.MAC_DEFAULT,
    tryjob = try_.job(),
)

try_.chromium_mac_builder(
    name = "mac-arm64-rel",
    branch_selector = branches.STANDARD_RELEASES,
    goma_jobs = goma.jobs.J150,
    os = os.MAC_10_15,
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
    name = "mac_chromium_11.0_rel_ng",
    builderless = False,
)

try_.chromium_mac_builder(
    name = "mac_chromium_archive_rel_ng",
)

try_.chromium_mac_builder(
    name = "mac_chromium_asan_rel_ng",
    goma_jobs = goma.jobs.J150,
)

try_.chromium_mac_builder(
    name = "mac_chromium_compile_dbg_ng",
    branch_selector = branches.STANDARD_RELEASES,
    goma_jobs = goma.jobs.J150,
    os = os.MAC_DEFAULT,
    main_list_view = settings.main_list_view_name,
    tryjob = try_.job(cancel_stale = False),
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
)

try_.chromium_mac_ios_builder(
    name = "ios-simulator",
    branch_selector = branches.STANDARD_RELEASES,
    main_list_view = settings.main_list_view_name,
    tryjob = try_.job(),
)

try_.chromium_mac_ios_builder(
    name = "ios-simulator-code-coverage",
    use_clang_coverage = True,
    coverage_exclude_sources = "ios_test_files_and_test_utils",
    coverage_test_types = ["unit"],
    os = os.MAC_10_15,
    tryjob = try_.job(experiment_percentage = 3),
)

try_.chromium_mac_ios_builder(
    name = "ios-simulator-cr-recipe",
)

try_.chromium_mac_ios_builder(
    name = "ios-simulator-cronet",
    branch_selector = branches.STANDARD_RELEASES,
    caches = [xcode_cache.x11e146],
    main_list_view = settings.main_list_view_name,
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
    main_list_view = settings.main_list_view_name,
    tryjob = try_.job(
        location_regexp = [
            ".+/[+]/ios/.+",
        ],
    ),
)

try_.chromium_mac_ios_builder(
    name = "ios-simulator-multi-window",
)

try_.chromium_mac_ios_builder(
    name = "ios-simulator-noncq",
)

try_.chromium_mac_ios_builder(
    name = "ios13-beta-simulator",
)

try_.chromium_mac_ios_builder(
    name = "ios13-sdk-simulator",
)

try_.chromium_mac_ios_builder(
    name = "ios14-beta-simulator",
)

try_.chromium_mac_ios_builder(
    name = "ios14-sdk-simulator",
    caches = [xcode_cache.x12b5018i],
    properties = {
        "xcode_build_version": "12b5018i",
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
    name = "win-libfuzzer-asan-rel",
    branch_selector = branches.STANDARD_RELEASES,
    builderless = False,
    executable = "recipe:chromium_libfuzzer_trybot",
    main_list_view = settings.main_list_view_name,
    os = os.WINDOWS_ANY,
    tryjob = try_.job(cancel_stale = False),
)

try_.chromium_win_builder(
    name = "win_archive",
)

try_.chromium_win_builder(
    name = "win_chromium_compile_dbg_ng",
    branch_selector = branches.STANDARD_RELEASES,
    goma_jobs = goma.jobs.J150,
    main_list_view = settings.main_list_view_name,
    tryjob = try_.job(cancel_stale = False),
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

try_.chromium_win_builder(
    name = "win10_chromium_x64_1909_fyi_rel_ng",
    builderless = False,
    os = os.WINDOWS_10_1909,
)

try_.chromium_win_builder(
    name = "win10_chromium_x64_dbg_ng",
    os = os.WINDOWS_10,
)

try_.chromium_win_builder(
    name = "win10_chromium_x64_rel_ng",
    branch_selector = branches.STANDARD_RELEASES,
    goma_jobs = goma.jobs.J150,
    os = os.WINDOWS_10,
    ssd = True,
    use_clang_coverage = True,
    main_list_view = settings.main_list_view_name,
    tryjob = try_.job(cancel_stale = False),
)

try_.chromium_win_builder(
    name = "win10_chromium_x64_rel_ng_exp",
    builderless = False,
    os = os.WINDOWS_ANY,
)

try_.chromium_win_builder(
    name = "win7-rel",
    branch_selector = branches.STANDARD_RELEASES,
    execution_timeout = 4 * time.hour + 30 * time.minute,
    goma_jobs = goma.jobs.J300,
    main_list_view = settings.main_list_view_name,
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
    main_list_view = settings.main_list_view_name,
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
            ".+/[+]/media/renderers/.+",
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
    main_list_view = settings.main_list_view_name,
    tryjob = try_.job(
        location_regexp = [
            ".+/[+]/chrome/browser/vr/.+",
            ".+/[+]/content/browser/xr/.+",
            ".+/[+]/content/test/gpu/.+",
            ".+/[+]/gpu/.+",
            ".+/[+]/media/audio/.+",
            ".+/[+]/media/filters/.+",
            ".+/[+]/media/gpu/.+",
            ".+/[+]/media/renderers/.+",
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
    main_list_view = settings.main_list_view_name,
    tryjob = try_.job(
        location_regexp = [
            ".+/[+]/chrome/browser/vr/.+",
            ".+/[+]/content/browser/xr/.+",
            ".+/[+]/content/test/gpu/.+",
            ".+/[+]/gpu/.+",
            ".+/[+]/media/audio/.+",
            ".+/[+]/media/filters/.+",
            ".+/[+]/media/gpu/.+",
            ".+/[+]/media/renderers/.+",
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
    main_list_view = settings.main_list_view_name,
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
            ".+/[+]/media/renderers/.+",
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
    branches.cq_tryjob_verifier(
        builder = "chrome:try/" + builder,
        cq_group = "cq",
        includable_only = True,
        owner_whitelist = [
            "googlers",
            "project-chromium-robot-committers",
        ],
        **kwargs
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
    builder = "linux-chrome",
)

chrome_internal_verifier(
    builder = "linux-chrome-beta",
    branch_selector = branches.STANDARD_RELEASES,
)

chrome_internal_verifier(
    builder = "linux-chrome-stable",
    branch_selector = branches.STANDARD_RELEASES,
)

chrome_internal_verifier(
    builder = "linux-chromeos-chrome",
)

chrome_internal_verifier(
    builder = "linux-chromeos-chrome-easwa",
)

chrome_internal_verifier(
    builder = "mac-chrome",
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
    builder = "win-chrome",
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
    builder = "win64-chrome",
)

chrome_internal_verifier(
    builder = "win64-chrome-beta",
    branch_selector = branches.STANDARD_RELEASES,
)

chrome_internal_verifier(
    builder = "win64-chrome-stable",
    branch_selector = branches.STANDARD_RELEASES,
)
