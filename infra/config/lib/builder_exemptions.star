# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""This file contains lists of builders exempted from certain restrictions.

************************************WARNING*************************************
NOTHING in this file should be added to. The builders here are merely those that
got grandfathered into being exempted from newer restrictions. So the amount of
builders listed here should not grow, and should *only* decrease.
************************************WARNING*************************************

"""

exempted_from_description_builders = {
    "ci": set([]),
    "codesearch": set([
        "gen-android-try",
        "gen-chromiumos-try",
        "gen-fuchsia-try",
        "gen-ios-try",
        "gen-linux-try",
        "gen-mac-try",
        "gen-webview-try",
        "gen-win-try",
    ]),
    "findit": set([
        "gofindit-culprit-verification",
        "test-single-revision",
    ]),
    "flaky-reproducer": set([
        "runner",
    ]),
    "infra": set([
        "autosharder",
    ]),
    "reviver": set([
        "android-coverage-launcher",
        "android-device-launcher",
        "android-launcher",
        "coverage-runner",
        "fuchsia-coordinator",
        "linux-launcher",
        "mac-launcher",
        "runner",
        "win-launcher",
    ]),
    "try": set([
        "3pp-linux-amd64-packager",
        "3pp-mac-amd64-packager",
        "android-binary-size",
        "android-clang-tidy-rel",
        "android-deterministic-dbg",
        "android-deterministic-rel",
        "android_blink_rel",
        "branch-config-verifier",
        "fuchsia-binary-size",
        "fuchsia-clang-tidy-rel",
        "fuchsia-deterministic-dbg",
        "linux-chromeos-clang-tidy-rel",
        "linux-clang-tidy-rel",
        "linux_chromium_clobber_deterministic",
        "linux_upload_clang",
        "linux_upload_rust",
        "mac-clang-tidy-rel",
        "mac_upload_clang",
        "mac_upload_clang_arm",
        "mac_upload_rust",
        "mac_upload_rust_arm",
        "reclient-config-deployment-verifier",
        "tricium-clang-tidy",
        "tricium-metrics-analysis",
        "tricium-oilpan-analysis",
        "win-presubmit",
        "win10-clang-tidy-rel",
        "win_upload_clang",
        "win_upload_rust",
    ]),
}

# This dict should NOT be added to. It contains a list of builders that are
# exempted from needing a contact_team_email field.
# It's intended as a stopgap for older builders. All new builders should have a
# contact_team_email field for the good of our code and CI system.
# Builders should be removed from here once their contact is assigned.
exempted_from_contact_builders = {
    "ci": [
        "3pp-linux-amd64-packager",
        "3pp-mac-amd64-packager",
        "ASAN Release Media",
        "Leak Detection Linux",
        "Linux Viz",
        "Mac ASAN Release Media",
        "Mac Builder Next",
        "Mac deterministic",
        "Mac deterministic (dbg)",
        "Site Isolation Android",
        "Win 10 Fast Ring",
        "android-11-x86-fyi-rel",
        "android-11-x86-rel",
        "android-12-x64-dbg-tests",
        "android-arm64-archive-rel",
        "android-avd-packager",
        "android-code-coverage",
        "android-code-coverage-native",
        "android-device-flasher",
        "android-fieldtrial-rel",
        "android-perfetto-rel",
        "android-sdk-packager",
        "android-webview-12-x64-dbg-tests",
        "android-webview-13-x64-dbg-tests",
        "chromeos-js-code-coverage",
        "fuchsia-code-coverage",
        "fuchsia-x64-accessibility-rel",
        "ios-blink-rel-fyi",
        "ios-catalyst",
        "ios-device",
        "ios-fieldtrial-rel",
        "ios-simulator",
        "ios-simulator-code-coverage",
        "ios-simulator-full-configs",
        "ios-simulator-noncq",
        "ios26-beta-simulator",
        "ios27-beta-simulator",
        "ios26-sdk-simulator",
        "ios27-sdk-simulator",
        "linux-blink-heap-verification",
        "linux-blink-web-tests-force-accessibility-rel",
        "linux-blink-wpt-reset-rel",
        "linux-chromeos-archive-rel",
        "linux-chromeos-code-coverage",
        "linux-code-coverage",
        "linux-fieldtrial-rel",
        "linux-fuzz-coverage",
        "linux-headless-shell-rel",
        "linux-js-code-coverage",
        "linux-local-ssd-rel-dev",
        "linux-perfetto-rel",
        "linux-rel-jammy-dev",
        "linux-rel-no-external-ip",
        "linux-remote-ssd-rel-dev",
        "linux-upload-perfetto",
        "linux-v4l2-codec-rel",
        "mac-arm-rel-dev",
        "mac-code-coverage",
        "mac-fieldtrial-tester",
        "mac-intel-on-arm64-rel",
        "mac-lsan-fyi-rel",
        "mac-perfetto-rel",
        "mac-rel-dev",
        "mac-ubsan-fyi-rel",
        "metadata-exporter",
        "rts-suite-analysis",
        "win-fieldtrial-rel",
        "win-local-ssd-rel-dev",
        "win-perfetto-rel",
        "win-rel-dev",
        "win-upload-perfetto",
        "win10-code-coverage",
        "win10-rel-no-external-ip",
        "win11-rel-dev",
        "win32-arm64-rel",
    ],
    "try": [
        "3pp-linux-amd64-packager",
        "3pp-mac-amd64-packager",
        "android-11-x86-rel",
        "android-binary-size",
        "android-clang-tidy-rel",
        "android-code-coverage",
        "android-code-coverage-native",
        "android-deterministic-dbg",
        "android-deterministic-rel",
        "android-fieldtrial-rel",
        "android-perfetto-rel",
        "android_blink_rel",
        "branch-config-verifier",
        "builder-config-verifier",
        "chromeos-js-code-coverage",
        "chromeos-js-coverage-rel",
        "fuchsia-binary-size",
        "fuchsia-clang-tidy-rel",
        "fuchsia-code-coverage",
        "fuchsia-deterministic-dbg",
        "fuchsia-x64-accessibility-rel",
        "ios-angle-try-intel",
        "ios-blink-rel-fyi",
        "ios-catalyst",
        "ios-device",
        "ios-fieldtrial-rel",
        "ios-simulator",
        "ios-simulator-code-coverage",
        "ios-simulator-full-configs",
        "ios-simulator-noncq",
        "ios26-beta-simulator",
        "ios27-beta-simulator",
        "ios26-sdk-simulator",
        "ios27-sdk-simulator",
        "leak_detection_linux",
        "linux-asan-media-rel",
        "linux-blink-heap-verification-try",
        "linux-blink-web-tests-force-accessibility-rel",
        "linux-chromeos-clang-tidy-rel",
        "linux-chromeos-code-coverage",
        "linux-clang-tidy-rel",
        "linux-code-coverage",
        "linux-fieldtrial-rel",
        "linux-headless-shell-rel",
        "linux-js-code-coverage",
        "linux-js-coverage-rel",
        "linux-perfetto-rel",
        "linux-v4l2-codec-rel",
        "linux-viz-rel",
        "linux_chromium_clobber_deterministic",
        "linux_upload_clang",
        "linux_upload_rust",
        "mac-asan-media-rel",
        "mac-builder-next",
        "mac-clang-tidy-rel",
        "mac-code-coverage",
        "mac-intel-on-arm64-rel",
        "mac-lsan-fyi-rel",
        "mac-perfetto-rel",
        "mac-ubsan-fyi-rel",
        "mac_upload_clang",
        "mac_upload_clang_arm",
        "mac_upload_rust",
        "mac_upload_rust_arm",
        "reclient-config-deployment-verifier",
        "requires-testing-checker",
        "targets-config-verifier",
        "tricium-clang-tidy",
        "tricium-metrics-analysis",
        "tricium-oilpan-analysis",
        "win-fieldtrial-rel",
        "win-perfetto-rel",
        "win-presubmit",
        "win10-clang-tidy-rel",
        "win10-code-coverage",
        "win_upload_clang",
        "win_upload_rust",
    ],
    "infra": [
        "autosharder",
    ],
    "codesearch": [
        "gen-android-try",
        "gen-chromiumos-try",
        "gen-fuchsia-try",
        "gen-ios-try",
        "gen-linux-try",
        "gen-mac-try",
        "gen-webview-try",
        "gen-win-try",
    ],
    "findit": [
        "gofindit-culprit-verification",
        "test-single-revision",
    ],
    "flaky-reproducer": [
        "runner",
    ],
    "reviver": [
        "android-coverage-launcher",
        "android-device-launcher",
        "android-launcher",
        "coverage-runner",
        "fuchsia-coordinator",
        "linux-launcher",
        "mac-launcher",
        "runner",
        "win-launcher",
    ],
}

mega_cq_excluded_builders = [
    # TODO(crbug.com/343505108): Remove the following libfuzzer
    # builders as trybots are created for them.
    "Libfuzzer Upload Linux ASan Debug",
    "Libfuzzer Upload Linux MSan",
    "Libfuzzer Upload Linux UBSan",
    "Libfuzzer Upload Linux V8-ARM64 ASan",
    "Libfuzzer Upload Linux V8-ARM64 ASan Debug",
    "Libfuzzer Upload Linux32 ASan",
    "Libfuzzer Upload Linux32 V8-ARM ASan",
    "Libfuzzer Upload Linux32 V8-ARM ASan Debug",
    "Libfuzzer Upload iOS Catalyst Debug",
    # TODO(crbug.com/40282196): Remove the following as trybots are
    # created for them.
    "android-arm64-archive-rel",
]

mega_cq_excluded_gardener_rotations = [
    # Most/all the clang bots build using clang built from HEAD.
    # Failures on them hopefully/rarely lead to reverts of random
    # CLs on the Chromium-side. So trybots for these aren't as
    # critical.
    "chromium.clang",
    # Some GPU and Dawn trybots share the same limited pool of bots,
    # so can't handle more than a few builds at a time.
    # TODO(crbug.com/413080339): Trigger these trybots on mega CQ only on CLs
    # that are likely to affect them, similarly to how optional GPU CQ bots
    # like "gpu-fyi-cq-android-arm64" and "dawn-mac-x64-deps-rel" are triggered
    # using "location_filters".
    "chromium.gpu",
    "dawn",
    # "cft" builders are very red.
    "cft",
]

# This dict should NOT be added to. It contains a list of ci builders that are
# already being mirrored in CQ but have no gardener rotation.
exempted_gardened_mirrors_in_cq_builders = [
    "ci/fuchsia-x64-accessibility-rel",
    "ci/linux-enterprise-companion-builder-dbg",
    "ci/linux-enterprise-companion-builder-rel",
    "ci/linux-enterprise-companion-tester-dbg",
    "ci/linux-enterprise-companion-tester-rel",
    "ci/linux-headless-shell-rel",
    "ci/linux-js-code-coverage",
    "ci/linux-perfetto-rel",
    "ci/linux-updater-builder-dbg",
    "ci/linux-updater-builder-rel",
    "ci/linux-updater-tester-dbg",
    "ci/linux-updater-tester-rel",
    "ci/mac-enterprise-companion-builder-arm64-dbg",
    "ci/mac-enterprise-companion-builder-rel",
    "ci/mac-updater-builder-arm64-dbg",
    "ci/mac-updater-builder-rel",
    "ci/mac13-arm64-enterprise-companion-tester-dbg",
    "ci/mac13-x64-enterprise-companion-tester-rel",
    "ci/mac13-arm64-updater-tester-dbg",
    "ci/mac13-x64-updater-tester-rel",
    "ci/mac14-tests",
    "ci/win-arm64-updater-builder-rel",
    "ci/win-enterprise-companion-builder-dbg",
    "ci/win-enterprise-companion-builder-rel",
    "ci/win-updater-builder-dbg",
    "ci/win-updater-builder-rel",
    "ci/win10-enterprise-companion-tester-dbg",
    "ci/win10-enterprise-companion-tester-rel",
    "ci/win10-updater-tester-dbg",
    "ci/win10-updater-tester-rel",
    "ci/win11-arm64-updater-tester-rel",
]

standalone_trybot_excluded_builders = [
    "android_blink_rel",  # Same reason 'tryserver.blink' is excluded below.
    # The GPU optional-CQ bots likely have some special coverage/testing
    # requirements. But being standalone shouldn't be a requirement.
    "android_optional_gpu_tests_rel",
    "linux_optional_gpu_tests_rel",
    "mac_optional_gpu_tests_rel",
    "win_optional_gpu_tests_rel",
    # The UTR-tester recipe doesn't currently support real CI-try mirroring.
    "linux-utr-tester",
    "mac-utr-tester",
    "win-utr-tester",
    # The autotest tester doesn't currently support CI-try mirroring.
    "linux-autotest-tester",
    "mac-autotest-tester",
    "win-autotest-tester",
]

standalone_trybot_excluded_builder_groups = [
    # These bots are used to generate new web-test expectations for pending
    # CLs, a use-case for which a CI mirror wouldn't make much sense. For more
    # info, see:
    # https://chromium.googlesource.com/chromium/src/+/HEAD/docs/testing/web_test_expectations.md#rebaselining-using-try-jobs
    "tryserver.blink",
]
