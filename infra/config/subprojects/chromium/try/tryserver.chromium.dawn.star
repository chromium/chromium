# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Definitions of builders in the tryserver.chromium.swangle builder group."""

load("//lib/branches.star", "branches")
load("//lib/builders.star", "goma", "os", "reclient")
load("//lib/consoles.star", "consoles")
load("//lib/try.star", "try_")

try_.defaults.set(
    builder_group = "tryserver.chromium.dawn",
    builderless = False,
    executable = try_.DEFAULT_EXECUTABLE,
    execution_timeout = try_.DEFAULT_EXECUTION_TIMEOUT,
    goma_backend = goma.backend.RBE_PROD,
    os = os.LINUX_DEFAULT,
    pool = try_.DEFAULT_POOL,
    reclient_instance = reclient.instance.DEFAULT_UNTRUSTED,
    reclient_jobs = reclient.jobs.LOW_JOBS_FOR_CQ,
    service_account = try_.gpu.SERVICE_ACCOUNT,
)

consoles.list_view(
    name = "tryserver.chromium.dawn",
    branch_selector = branches.DESKTOP_EXTENDED_STABLE_MILESTONE,
)

try_.builder(
    name = "dawn-android-arm-deps-rel",
    branch_selector = branches.STANDARD_MILESTONE,
    mirrors = [
        "ci/Dawn Android arm DEPS Release (Pixel 4)",
    ],
    main_list_view = "try",
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
    test_presentation = resultdb.test_presentation(
        grouping_keys = ["status", "v.test_suite", "v.gpu"],
    ),
)

try_.builder(
    name = "dawn-linux-x64-deps-rel",
    branch_selector = branches.STANDARD_MILESTONE,
    mirrors = [
        "ci/Dawn Linux x64 DEPS Builder",
        "ci/Dawn Linux x64 DEPS Release (Intel UHD 630)",
        "ci/Dawn Linux x64 DEPS Release (NVIDIA)",
    ],
    main_list_view = "try",
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
    test_presentation = resultdb.test_presentation(
        grouping_keys = ["status", "v.test_suite", "v.gpu"],
    ),
    goma_backend = None,
)

try_.builder(
    name = "dawn-mac-x64-deps-rel",
    branch_selector = branches.DESKTOP_EXTENDED_STABLE_MILESTONE,
    mirrors = [
        "ci/Dawn Mac x64 DEPS Builder",
        # Not enough capacity on Mac AMD https://crbug.com/1380184.
        # "ci/Dawn Mac x64 DEPS Release (AMD)",
        "ci/Dawn Mac x64 DEPS Release (Intel)",
    ],
    main_list_view = "try",
    os = os.MAC_ANY,
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
    test_presentation = resultdb.test_presentation(
        grouping_keys = ["status", "v.test_suite", "v.gpu"],
    ),
)

try_.builder(
    name = "dawn-win10-x64-deps-rel",
    branch_selector = branches.DESKTOP_EXTENDED_STABLE_MILESTONE,
    mirrors = [
        "ci/Dawn Win10 x64 DEPS Builder",
        "ci/Dawn Win10 x64 DEPS Release (Intel HD 630)",
        "ci/Dawn Win10 x64 DEPS Release (NVIDIA)",
    ],
    main_list_view = "try",
    os = os.WINDOWS_ANY,
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
    test_presentation = resultdb.test_presentation(
        grouping_keys = ["status", "v.test_suite", "v.gpu"],
    ),
)

try_.builder(
    name = "dawn-win10-x86-deps-rel",
    branch_selector = branches.DESKTOP_EXTENDED_STABLE_MILESTONE,
    mirrors = [
        "ci/Dawn Win10 x86 DEPS Builder",
        "ci/Dawn Win10 x86 DEPS Release (Intel HD 630)",
        "ci/Dawn Win10 x86 DEPS Release (NVIDIA)",
    ],
    main_list_view = "try",
    os = os.WINDOWS_ANY,
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
    test_presentation = resultdb.test_presentation(
        grouping_keys = ["status", "v.test_suite", "v.gpu"],
    ),
)

try_.builder(
    name = "android-dawn-arm-rel",
    mirrors = [
        "ci/Dawn Android arm Release (Pixel 4)",
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
    os = os.MAC_ANY,
    mirrors = [
        "ci/Dawn Mac x64 Builder",
        # Not enough capacity on Mac AMD https://crbug.com/1380184.
        # "ci/Dawn Mac x64 Release (AMD)",
        "ci/Dawn Mac x64 Release (Intel)",
    ],
    test_presentation = resultdb.test_presentation(
        grouping_keys = ["status", "v.test_suite", "v.gpu"],
    ),
)

try_.builder(
    name = "dawn-try-mac-amd-exp",
    builderless = True,
    os = os.MAC_ANY,
    pool = "luci.chromium.gpu.mac.retina.amd.try",
    mirrors = [
        "ci/Dawn Mac x64 Builder",
        "ci/Dawn Mac x64 Experimental Release (AMD)",
    ],
    test_presentation = resultdb.test_presentation(
        grouping_keys = ["status", "v.test_suite", "v.gpu"],
    ),
)

try_.builder(
    name = "dawn-try-mac-intel-exp",
    builderless = True,
    os = os.MAC_ANY,
    pool = "luci.chromium.gpu.mac.mini.intel.try",
    mirrors = [
        "ci/Dawn Mac x64 Builder",
        "ci/Dawn Mac x64 Experimental Release (Intel)",
    ],
    test_presentation = resultdb.test_presentation(
        grouping_keys = ["status", "v.test_suite", "v.gpu"],
    ),
)

try_.builder(
    name = "win-dawn-rel",
    os = os.WINDOWS_ANY,
    mirrors = [
        "ci/Dawn Win10 x64 Builder",
        "ci/Dawn Win10 x64 Release (Intel HD 630)",
        "ci/Dawn Win10 x64 Release (NVIDIA)",
    ],
    test_presentation = resultdb.test_presentation(
        grouping_keys = ["status", "v.test_suite", "v.gpu"],
    ),
)

try_.builder(
    name = "dawn-try-win10-x86-rel",
    os = os.WINDOWS_ANY,
    mirrors = [
        "ci/Dawn Win10 x86 Builder",
        "ci/Dawn Win10 x86 Release (Intel HD 630)",
        "ci/Dawn Win10 x86 Release (NVIDIA)",
    ],
    test_presentation = resultdb.test_presentation(
        grouping_keys = ["status", "v.test_suite", "v.gpu"],
    ),
)

try_.builder(
    name = "dawn-try-win10-x64-asan-rel",
    os = os.WINDOWS_ANY,
    mirrors = [
        "ci/Dawn Win10 x64 ASAN Release",
    ],
    test_presentation = resultdb.test_presentation(
        grouping_keys = ["status", "v.test_suite", "v.gpu"],
    ),
)
