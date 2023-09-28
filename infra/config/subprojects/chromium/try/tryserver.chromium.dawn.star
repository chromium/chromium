# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Definitions of builders in the tryserver.chromium.swangle builder group."""

load("//lib/branches.star", "branches")
load("//lib/builders.star", "os", "reclient")
load("//lib/consoles.star", "consoles")
load("//lib/try.star", "try_")

try_.defaults.set(
    executable = try_.DEFAULT_EXECUTABLE,
    builder_group = "tryserver.chromium.dawn",
    pool = try_.DEFAULT_POOL,
    builderless = False,
    os = os.LINUX_DEFAULT,
    contact_team_email = "chrome-gpu-infra@google.com",
    execution_timeout = try_.DEFAULT_EXECUTION_TIMEOUT,
    reclient_instance = reclient.instance.DEFAULT_UNTRUSTED,
    reclient_jobs = reclient.jobs.LOW_JOBS_FOR_CQ,
    service_account = try_.gpu.SERVICE_ACCOUNT,
)

consoles.list_view(
    name = "tryserver.chromium.dawn",
    branch_selector = [
        branches.selector.ANDROID_BRANCHES,
        branches.selector.DESKTOP_BRANCHES,
    ],
)

try_.builder(
    name = "dawn-android-arm-deps-rel",
    mirrors = [
        "ci/Dawn Android arm DEPS Builder",
        "ci/Dawn Android arm DEPS Release (Nexus 5X)",
        "ci/Dawn Android arm DEPS Release (Pixel 4)",
    ],
    main_list_view = "try",
    test_presentation = resultdb.test_presentation(
        grouping_keys = ["status", "v.test_suite", "v.gpu"],
    ),
    tryjob = try_.job(
        location_filters = [
            cq.location_filter(path_regexp = "content/test/gpu/.+"),
            cq.location_filter(path_regexp = "gpu/.+"),
            cq.location_filter(path_regexp = "testing/buildbot/chromium.dawn.json"),
            cq.location_filter(path_regexp = "third_party/blink/renderer/modules/webgpu/.+"),
            cq.location_filter(path_regexp = "third_party/blink/web_tests/external/wpt/webgpu/.+"),
            cq.location_filter(path_regexp = "third_party/blink/web_tests/wpt_internal/webgpu/.+"),
            cq.location_filter(path_regexp = "third_party/blink/web_tests/WebGPUExpectations"),
            cq.location_filter(path_regexp = "third_party/dawn/.+"),
            cq.location_filter(path_regexp = "third_party/webgpu-cts/.+"),
            cq.location_filter(path_regexp = "tools/clang/scripts/update.py"),
            cq.location_filter(path_regexp = "ui/gl/features.gni"),
        ],
    ),
)

try_.builder(
    name = "dawn-android-arm64-deps-rel",
    mirrors = [
        "ci/Dawn Android arm64 DEPS Release (Pixel 6)",
    ],
    main_list_view = "try",
    test_presentation = resultdb.test_presentation(
        grouping_keys = ["status", "v.test_suite", "v.gpu"],
    ),
    tryjob = try_.job(
        experiment_percentage = 100,
        location_filters = [
            cq.location_filter(path_regexp = "content/test/gpu/.+"),
            cq.location_filter(path_regexp = "gpu/.+"),
            cq.location_filter(path_regexp = "testing/buildbot/chromium.dawn.json"),
            cq.location_filter(path_regexp = "third_party/blink/renderer/modules/webgpu/.+"),
            cq.location_filter(path_regexp = "third_party/blink/web_tests/external/wpt/webgpu/.+"),
            cq.location_filter(path_regexp = "third_party/blink/web_tests/wpt_internal/webgpu/.+"),
            cq.location_filter(path_regexp = "third_party/blink/web_tests/WebGPUExpectations"),
            cq.location_filter(path_regexp = "third_party/dawn/.+"),
            cq.location_filter(path_regexp = "third_party/webgpu-cts/.+"),
            cq.location_filter(path_regexp = "tools/clang/scripts/update.py"),
            cq.location_filter(path_regexp = "ui/gl/features.gni"),
        ],
    ),
)

try_.builder(
    name = "dawn-linux-x64-deps-rel",
    branch_selector = branches.selector.LINUX_BRANCHES,
    mirrors = [
        "ci/Dawn Linux x64 DEPS Builder",
        "ci/Dawn Linux x64 DEPS Release (Intel UHD 630)",
        "ci/Dawn Linux x64 DEPS Release (NVIDIA)",
    ],
    main_list_view = "try",
    test_presentation = resultdb.test_presentation(
        grouping_keys = ["status", "v.test_suite", "v.gpu"],
    ),
    tryjob = try_.job(
        location_filters = [
            cq.location_filter(path_regexp = "content/test/gpu/.+"),
            cq.location_filter(path_regexp = "gpu/.+"),
            cq.location_filter(path_regexp = "testing/buildbot/chromium.dawn.json"),
            cq.location_filter(path_regexp = "third_party/blink/renderer/modules/webgpu/.+"),
            cq.location_filter(path_regexp = "third_party/blink/web_tests/external/wpt/webgpu/.+"),
            cq.location_filter(path_regexp = "third_party/blink/web_tests/wpt_internal/webgpu/.+"),
            cq.location_filter(path_regexp = "third_party/blink/web_tests/WebGPUExpectations"),
            cq.location_filter(path_regexp = "third_party/dawn/.+"),
            cq.location_filter(path_regexp = "third_party/webgpu-cts/.+"),
            cq.location_filter(path_regexp = "tools/clang/scripts/update.py"),
            cq.location_filter(path_regexp = "ui/gl/features.gni"),
        ],
    ),
)

try_.builder(
    name = "dawn-mac-x64-deps-rel",
    branch_selector = branches.selector.MAC_BRANCHES,
    mirrors = [
        "ci/Dawn Mac x64 DEPS Builder",
        "ci/Dawn Mac x64 DEPS Release (AMD)",
        "ci/Dawn Mac x64 DEPS Release (Intel)",
    ],
    os = os.MAC_ANY,
    main_list_view = "try",
    test_presentation = resultdb.test_presentation(
        grouping_keys = ["status", "v.test_suite", "v.gpu"],
    ),
    tryjob = try_.job(
        location_filters = [
            cq.location_filter(path_regexp = "content/test/gpu/.+"),
            cq.location_filter(path_regexp = "gpu/.+"),
            cq.location_filter(path_regexp = "testing/buildbot/chromium.dawn.json"),
            cq.location_filter(path_regexp = "third_party/blink/renderer/modules/webgpu/.+"),
            cq.location_filter(path_regexp = "third_party/blink/web_tests/external/wpt/webgpu/.+"),
            cq.location_filter(path_regexp = "third_party/blink/web_tests/wpt_internal/webgpu/.+"),
            cq.location_filter(path_regexp = "third_party/blink/web_tests/WebGPUExpectations"),
            cq.location_filter(path_regexp = "third_party/dawn/.+"),
            cq.location_filter(path_regexp = "third_party/webgpu-cts/.+"),
            cq.location_filter(path_regexp = "tools/clang/scripts/update.py"),
            cq.location_filter(path_regexp = "ui/gl/features.gni"),
        ],
    ),
)

try_.builder(
    name = "dawn-win10-x64-deps-rel",
    branch_selector = branches.selector.WINDOWS_BRANCHES,
    mirrors = [
        "ci/Dawn Win10 x64 DEPS Builder",
        "ci/Dawn Win10 x64 DEPS Release (Intel)",
        "ci/Dawn Win10 x64 DEPS Release (NVIDIA)",
    ],
    os = os.WINDOWS_ANY,
    main_list_view = "try",
    test_presentation = resultdb.test_presentation(
        grouping_keys = ["status", "v.test_suite", "v.gpu"],
    ),
    tryjob = try_.job(
        location_filters = [
            cq.location_filter(path_regexp = "content/test/gpu/.+"),
            cq.location_filter(path_regexp = "gpu/.+"),
            cq.location_filter(path_regexp = "testing/buildbot/chromium.dawn.json"),
            cq.location_filter(path_regexp = "third_party/blink/renderer/modules/webgpu/.+"),
            cq.location_filter(path_regexp = "third_party/blink/web_tests/external/wpt/webgpu/.+"),
            cq.location_filter(path_regexp = "third_party/blink/web_tests/wpt_internal/webgpu/.+"),
            cq.location_filter(path_regexp = "third_party/blink/web_tests/WebGPUExpectations"),
            cq.location_filter(path_regexp = "third_party/dawn/.+"),
            cq.location_filter(path_regexp = "third_party/webgpu-cts/.+"),
            cq.location_filter(path_regexp = "tools/clang/scripts/update.py"),
            cq.location_filter(path_regexp = "ui/gl/features.gni"),
        ],
    ),
)

try_.builder(
    name = "dawn-win10-x86-deps-rel",
    branch_selector = branches.selector.WINDOWS_BRANCHES,
    mirrors = [
        "ci/Dawn Win10 x86 DEPS Builder",
        "ci/Dawn Win10 x86 DEPS Release (Intel)",
        "ci/Dawn Win10 x86 DEPS Release (NVIDIA)",
    ],
    os = os.WINDOWS_ANY,
    main_list_view = "try",
    test_presentation = resultdb.test_presentation(
        grouping_keys = ["status", "v.test_suite", "v.gpu"],
    ),
    tryjob = try_.job(
        location_filters = [
            cq.location_filter(path_regexp = "content/test/gpu/.+"),
            cq.location_filter(path_regexp = "gpu/.+"),
            cq.location_filter(path_regexp = "testing/buildbot/chromium.dawn.json"),
            cq.location_filter(path_regexp = "third_party/blink/renderer/modules/webgpu/.+"),
            cq.location_filter(path_regexp = "third_party/blink/web_tests/external/wpt/webgpu/.+"),
            cq.location_filter(path_regexp = "third_party/blink/web_tests/wpt_internal/webgpu/.+"),
            cq.location_filter(path_regexp = "third_party/blink/web_tests/WebGPUExpectations"),
            cq.location_filter(path_regexp = "third_party/dawn/.+"),
            cq.location_filter(path_regexp = "third_party/webgpu-cts/.+"),
            cq.location_filter(path_regexp = "tools/clang/scripts/update.py"),
            cq.location_filter(path_regexp = "ui/gl/features.gni"),
        ],
    ),
)

try_.builder(
    name = "android-dawn-arm-rel",
    mirrors = [
        "ci/Dawn Android arm Builder",
        "ci/Dawn Android arm Release (Nexus 5X)",
        "ci/Dawn Android arm Release (Pixel 4)",
    ],
    test_presentation = resultdb.test_presentation(
        grouping_keys = ["status", "v.test_suite", "v.gpu"],
    ),
)

try_.builder(
    name = "android-dawn-arm64-rel",
    mirrors = [
        "ci/Dawn Android arm64 Release (Pixel 6)",
    ],
    test_presentation = resultdb.test_presentation(
        grouping_keys = ["status", "v.test_suite", "v.gpu"],
    ),
)

try_.builder(
    name = "linux-dawn-rel",
    mirrors = [
        "ci/Dawn Linux x64 Builder",
        "ci/Dawn Linux x64 Release (Intel UHD 630)",
        "ci/Dawn Linux x64 Release (NVIDIA)",
    ],
    test_presentation = resultdb.test_presentation(
        grouping_keys = ["status", "v.test_suite", "v.gpu"],
    ),
)

try_.builder(
    name = "mac-dawn-rel",
    mirrors = [
        "ci/Dawn Mac x64 Builder",
        "ci/Dawn Mac x64 Release (AMD)",
        "ci/Dawn Mac x64 Release (Intel)",
    ],
    os = os.MAC_ANY,
    test_presentation = resultdb.test_presentation(
        grouping_keys = ["status", "v.test_suite", "v.gpu"],
    ),
)

try_.builder(
    name = "dawn-try-linux-tsan-rel",
    mirrors = [
        "ci/Dawn Linux TSAN Release",
    ],
    pool = "luci.chromium.gpu.linux.nvidia.try",
    builderless = True,
    test_presentation = resultdb.test_presentation(
        grouping_keys = ["status", "v.test_suite", "v.gpu"],
    ),
)

try_.builder(
    name = "dawn-try-mac-amd-exp",
    mirrors = [
        "ci/Dawn Mac x64 Builder",
        "ci/Dawn Mac x64 Experimental Release (AMD)",
    ],
    pool = "luci.chromium.gpu.mac.retina.amd.try",
    builderless = True,
    os = os.MAC_ANY,
    test_presentation = resultdb.test_presentation(
        grouping_keys = ["status", "v.test_suite", "v.gpu"],
    ),
)

try_.builder(
    name = "dawn-try-mac-arm64-deps-rel",
    mirrors = [
        "ci/Dawn Mac arm64 DEPS Release (Apple M2)",
    ],
    # TODO(crbug.com/1435476): Switch to a dedicated M2 pool once we have
    # allocated machines.
    pool = "luci.chromium.gpu.mac.arm64.apple.m1.try",
    builderless = True,
    os = os.MAC_ANY,
    test_presentation = resultdb.test_presentation(
        grouping_keys = ["status", "v.test_suite", "v.gpu"],
    ),
)

try_.builder(
    name = "dawn-try-mac-arm64-rel",
    mirrors = [
        "ci/Dawn Mac arm64 Release (Apple M2)",
    ],
    # TODO(crbug.com/1435476): Switch to a dedicated M2 pool once we have
    # allocated machines.
    pool = "luci.chromium.gpu.mac.arm64.apple.m1.try",
    builderless = True,
    os = os.MAC_ANY,
    test_presentation = resultdb.test_presentation(
        grouping_keys = ["status", "v.test_suite", "v.gpu"],
    ),
)

try_.builder(
    name = "dawn-try-mac-intel-exp",
    mirrors = [
        "ci/Dawn Mac x64 Builder",
        "ci/Dawn Mac x64 Experimental Release (Intel)",
    ],
    pool = "luci.chromium.gpu.mac.mini.intel.try",
    builderless = True,
    os = os.MAC_ANY,
    test_presentation = resultdb.test_presentation(
        grouping_keys = ["status", "v.test_suite", "v.gpu"],
    ),
)

try_.builder(
    name = "dawn-try-win-x64-intel-exp",
    mirrors = [
        "ci/Dawn Win10 x64 Builder",
        "ci/Dawn Win10 x64 Experimental Release (Intel)",
    ],
    pool = "luci.chromium.gpu.win10.intel.try",
    builderless = True,
    os = os.WINDOWS_ANY,
    test_presentation = resultdb.test_presentation(
        grouping_keys = ["status", "v.test_suite", "v.gpu"],
    ),
)

try_.builder(
    name = "dawn-try-win-x86-intel-exp",
    mirrors = [
        "ci/Dawn Win10 x86 Builder",
        "ci/Dawn Win10 x86 Experimental Release (Intel)",
    ],
    pool = "luci.chromium.gpu.win10.intel.try",
    builderless = True,
    os = os.WINDOWS_ANY,
    test_presentation = resultdb.test_presentation(
        grouping_keys = ["status", "v.test_suite", "v.gpu"],
    ),
)

try_.builder(
    name = "win-dawn-rel",
    mirrors = [
        "ci/Dawn Win10 x64 Builder",
        "ci/Dawn Win10 x64 Release (Intel)",
        "ci/Dawn Win10 x64 Release (NVIDIA)",
    ],
    os = os.WINDOWS_ANY,
    test_presentation = resultdb.test_presentation(
        grouping_keys = ["status", "v.test_suite", "v.gpu"],
    ),
)

try_.builder(
    name = "dawn-try-win10-x86-rel",
    mirrors = [
        "ci/Dawn Win10 x86 Builder",
        "ci/Dawn Win10 x86 Release (Intel)",
        "ci/Dawn Win10 x86 Release (NVIDIA)",
    ],
    os = os.WINDOWS_ANY,
    test_presentation = resultdb.test_presentation(
        grouping_keys = ["status", "v.test_suite", "v.gpu"],
    ),
)

try_.builder(
    name = "dawn-try-win10-x64-intel-asan",
    mirrors = [
        "ci/Dawn Win10 x64 ASAN Builder",
        "ci/Dawn Win10 x64 ASAN Release (Intel)",
    ],
    pool = "luci.chromium.gpu.win10.intel.try",
    builderless = True,
    os = os.WINDOWS_ANY,
    test_presentation = resultdb.test_presentation(
        grouping_keys = ["status", "v.test_suite", "v.gpu"],
    ),
)

try_.builder(
    name = "dawn-try-win10-x64-nvidia-asan",
    mirrors = [
        "ci/Dawn Win10 x64 ASAN Builder",
        "ci/Dawn Win10 x64 ASAN Release (NVIDIA)",
    ],
    pool = "luci.chromium.gpu.win10.nvidia.try",
    builderless = True,
    os = os.WINDOWS_ANY,
    test_presentation = resultdb.test_presentation(
        grouping_keys = ["status", "v.test_suite", "v.gpu"],
    ),
)
