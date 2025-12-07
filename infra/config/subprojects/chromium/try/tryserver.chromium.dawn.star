# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Definitions of builders in the tryserver.chromium.swangle builder group."""

load("@chromium-luci//branches.star", "branches")
load("@chromium-luci//builder_config.star", "builder_config")
load("@chromium-luci//builders.star", "builders", "cpu", "os")
load("@chromium-luci//consoles.star", "consoles")
load("@chromium-luci//try.star", "try_")
load("//lib/gpu.star", "gpu")
load("//lib/siso.star", "siso")
load("//lib/try_constants.star", "try_constants")

try_.defaults.set(
    executable = try_constants.DEFAULT_EXECUTABLE,
    builder_group = "tryserver.chromium.dawn",
    pool = try_constants.DEFAULT_POOL,
    builderless = False,
    os = os.LINUX_DEFAULT,
    # These builders test GPU configurations. These configurations have very
    # limited hardware, due to the hardware needing specific GPUs. The pool of
    # machines to run builds for these builders is intentionally limited to
    # avoid concurrent builds from oversubscribing the test capacity. As a
    # consequence, pending times are expected for these builders. These builder
    # haven't been long poles in the CQ, and developers haven't complained about
    # them, so there's no need to page for them.
    alerts_enabled = False,
    check_for_flakiness = False,
    check_for_flakiness_with_resultdb = False,
    contact_team_email = "chrome-gpu-infra@google.com",
    execution_timeout = try_constants.DEFAULT_EXECUTION_TIMEOUT,
    experiments = {
        "chromium_tests.resultdb_module": 100,
    },
    service_account = gpu.try_.SERVICE_ACCOUNT,
    siso_project = siso.project.DEFAULT_UNTRUSTED,
    siso_remote_jobs = siso.remote_jobs.LOW_JOBS_FOR_CQ,
)

consoles.list_view(
    name = "tryserver.chromium.dawn",
    branch_selector = [
        branches.selector.ANDROID_BRANCHES,
        branches.selector.DESKTOP_BRANCHES,
    ],
)

def dawn_mac_builder(*, name, **kwargs):
    kwargs.setdefault("cpu", None)
    return try_.builder(
        name = name,
        builderless = True,
        cores = None,
        free_space = None,
        os = os.MAC_ANY,
        ssd = None,
        pool = "luci.chromium.gpu.try",
        max_concurrent_builds = 1,
        **kwargs
    )

def dawn_win_builderless_builder(*, name, **kwargs):
    kwargs.setdefault(
        "ssd",
        builders.with_expiration(True, expiration = 5 * time.minute),
    )
    kwargs.setdefault("max_concurrent_builds", 1)
    kwargs.setdefault("free_space", None)
    return try_.builder(
        name = name,
        builderless = True,
        os = os.WINDOWS_ANY,
        pool = "luci.chromium.gpu.try",
        **kwargs
    )

try_.builder(
    name = "dawn-chromium-presubmit",
    branch_selector = [
        branches.selector.ANDROID_BRANCHES,
        branches.selector.DESKTOP_BRANCHES,
    ],
    description_html = "Runs Chromium presubmit tests on Dawn CLs",
    mirrors = [
        "ci/Dawn Chromium Presubmit",
    ],
    builder_config_settings = builder_config.try_settings(
        analyze_names = [
            "dawn_chromium_presubmit",
        ],
        retry_failed_shards = False,
        retry_without_patch = False,
    ),
    gn_args = "ci/Dawn Chromium Presubmit",
    pool = "luci.chromium.gpu.try",
    builderless = True,
    os = os.LINUX_DEFAULT,
    ssd = None,
    free_space = None,
    execution_timeout = 30 * time.minute,
    main_list_view = "try",
    max_concurrent_builds = 3,
)

try_.builder(
    name = "dawn-android-arm-deps-rel",
    mirrors = [
        "ci/Dawn Android arm DEPS Builder",
        "ci/Dawn Android arm DEPS Release (Pixel 4)",
    ],
    gn_args = "ci/Dawn Android arm DEPS Builder",
    pool = "luci.chromium.gpu.try",
    builderless = True,
    os = os.LINUX_DEFAULT,
    ssd = None,
    free_space = None,
    main_list_view = "try",
    max_concurrent_builds = 5,
    test_presentation = resultdb.test_presentation(
        grouping_keys = ["status", "v.test_suite", "v.gpu"],
    ),
    tryjob = try_.job(
        location_filters = [
            cq.location_filter(path_regexp = "content/test/gpu/.+"),
            cq.location_filter(path_regexp = "gpu/.+"),
            cq.location_filter(path_regexp = "testing/buildbot/chromium.dawn.json"),
            cq.location_filter(path_regexp = "third_party/blink/renderer/modules/webgpu/.+"),
            cq.location_filter(path_regexp = "third_party/blink/renderer/platform/graphics/gpu/.+"),
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
        "ci/Dawn Android arm64 DEPS Builder",
        "ci/Dawn Android arm64 DEPS Release (Pixel 6)",
    ],
    gn_args = "ci/Dawn Android arm64 DEPS Builder",
    pool = "luci.chromium.gpu.try",
    builderless = True,
    os = os.LINUX_DEFAULT,
    ssd = None,
    free_space = None,
    main_list_view = "try",
    max_concurrent_builds = 5,
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
            cq.location_filter(path_regexp = "third_party/blink/renderer/platform/graphics/gpu/.+"),
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
    gn_args = "ci/Dawn Linux x64 DEPS Builder",
    pool = "luci.chromium.gpu.try",
    builderless = True,
    os = os.LINUX_DEFAULT,
    ssd = None,
    free_space = None,
    main_list_view = "try",
    max_concurrent_builds = 5,
    test_presentation = resultdb.test_presentation(
        grouping_keys = ["status", "v.test_suite", "v.gpu"],
    ),
    tryjob = try_.job(
        location_filters = [
            cq.location_filter(path_regexp = "content/test/gpu/.+"),
            cq.location_filter(path_regexp = "gpu/.+"),
            cq.location_filter(path_regexp = "testing/buildbot/chromium.dawn.json"),
            cq.location_filter(path_regexp = "third_party/blink/renderer/modules/webgpu/.+"),
            cq.location_filter(path_regexp = "third_party/blink/renderer/platform/graphics/gpu/.+"),
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
    name = "dawn-mac-arm64-deps-rel",
    branch_selector = branches.selector.MAC_BRANCHES,
    description_html = "Runs Dawn tests on Apple silicon at Chromium's pinned Dawn revision",
    mirrors = [
        "ci/Dawn Mac arm64 DEPS Builder",
        "ci/Dawn Mac arm64 DEPS Release (Apple M2)",
    ],
    gn_args = "ci/Dawn Mac arm64 DEPS Builder",
    pool = "luci.chromium.gpu.try",
    builderless = True,
    os = os.MAC_ANY,
    cpu = cpu.ARM64,
    free_space = None,
    main_list_view = "try",
    max_concurrent_builds = 3,
    test_presentation = resultdb.test_presentation(
        grouping_keys = ["status", "v.test_suite", "v.gpu"],
    ),
    tryjob = try_.job(
        location_filters = [
            cq.location_filter(path_regexp = "content/test/gpu/.+"),
            cq.location_filter(path_regexp = "gpu/.+"),
            cq.location_filter(path_regexp = "testing/buildbot/chromium.dawn.json"),
            cq.location_filter(path_regexp = "third_party/blink/renderer/modules/webgpu/.+"),
            cq.location_filter(path_regexp = "third_party/blink/renderer/platform/graphics/gpu/.+"),
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
    gn_args = "ci/Dawn Mac x64 DEPS Builder",
    pool = "luci.chromium.gpu.try",
    builderless = True,
    os = os.MAC_ANY,
    cpu = None,
    free_space = None,
    main_list_view = "try",
    max_concurrent_builds = 5,
    test_presentation = resultdb.test_presentation(
        grouping_keys = ["status", "v.test_suite", "v.gpu"],
    ),
    tryjob = try_.job(
        location_filters = [
            cq.location_filter(path_regexp = "content/test/gpu/.+"),
            cq.location_filter(path_regexp = "gpu/.+"),
            cq.location_filter(path_regexp = "testing/buildbot/chromium.dawn.json"),
            cq.location_filter(path_regexp = "third_party/blink/renderer/modules/webgpu/.+"),
            cq.location_filter(path_regexp = "third_party/blink/renderer/platform/graphics/gpu/.+"),
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

dawn_win_builderless_builder(
    name = "dawn-win10-x64-deps-rel",
    branch_selector = branches.selector.WINDOWS_BRANCHES,
    mirrors = [
        "ci/Dawn Win10 x64 DEPS Builder",
        "ci/Dawn Win10 x64 DEPS Release (Intel)",
        "ci/Dawn Win10 x64 DEPS Release (NVIDIA)",
    ],
    gn_args = "ci/Dawn Win10 x64 DEPS Builder",
    main_list_view = "try",
    max_concurrent_builds = 5,
    test_presentation = resultdb.test_presentation(
        grouping_keys = ["status", "v.test_suite", "v.gpu"],
    ),
    tryjob = try_.job(
        location_filters = [
            cq.location_filter(path_regexp = "content/test/gpu/.+"),
            cq.location_filter(path_regexp = "gpu/.+"),
            cq.location_filter(path_regexp = "testing/buildbot/chromium.dawn.json"),
            cq.location_filter(path_regexp = "third_party/blink/renderer/modules/webgpu/.+"),
            cq.location_filter(path_regexp = "third_party/blink/renderer/platform/graphics/gpu/.+"),
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

dawn_win_builderless_builder(
    name = "dawn-win10-x86-deps-rel",
    branch_selector = branches.selector.WINDOWS_BRANCHES,
    mirrors = [
        "ci/Dawn Win10 x86 DEPS Builder",
        "ci/Dawn Win10 x86 DEPS Release (Intel)",
        "ci/Dawn Win10 x86 DEPS Release (NVIDIA)",
    ],
    gn_args = "ci/Dawn Win10 x86 DEPS Builder",
    check_for_flakiness = False,
    check_for_flakiness_with_resultdb = False,
    main_list_view = "try",
    max_concurrent_builds = 5,
    test_presentation = resultdb.test_presentation(
        grouping_keys = ["status", "v.test_suite", "v.gpu"],
    ),
    tryjob = try_.job(
        location_filters = [
            cq.location_filter(path_regexp = "content/test/gpu/.+"),
            cq.location_filter(path_regexp = "gpu/.+"),
            cq.location_filter(path_regexp = "testing/buildbot/chromium.dawn.json"),
            cq.location_filter(path_regexp = "third_party/blink/renderer/modules/webgpu/.+"),
            cq.location_filter(path_regexp = "third_party/blink/renderer/platform/graphics/gpu/.+"),
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

dawn_win_builderless_builder(
    name = "dawn-win11-arm64-deps-rel",
    branch_selector = branches.selector.WINDOWS_BRANCHES,
    mirrors = [
        "ci/Dawn Win11 arm64 DEPS Builder",
    ],
    gn_args = "ci/Dawn Win11 arm64 DEPS Builder",
    main_list_view = "try",
    max_concurrent_builds = 5,
    test_presentation = resultdb.test_presentation(
        grouping_keys = ["status", "v.test_suite", "v.gpu"],
    ),
    tryjob = try_.job(
        location_filters = [
            cq.location_filter(path_regexp = "content/test/gpu/.+"),
            cq.location_filter(path_regexp = "gpu/.+"),
            cq.location_filter(path_regexp = "testing/buildbot/chromium.dawn.json"),
            cq.location_filter(path_regexp = "third_party/blink/renderer/modules/webgpu/.+"),
            cq.location_filter(path_regexp = "third_party/blink/renderer/platform/graphics/gpu/.+"),
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
        "ci/Dawn Android arm Release (Pixel 4)",
    ],
    gn_args = "ci/Dawn Android arm Builder",
    pool = "luci.chromium.gpu.try",
    builderless = True,
    os = os.LINUX_DEFAULT,
    ssd = None,
    free_space = None,
    max_concurrent_builds = 3,
    test_presentation = resultdb.test_presentation(
        grouping_keys = ["status", "v.test_suite", "v.gpu"],
    ),
)

try_.builder(
    name = "android-dawn-arm64-rel",
    mirrors = [
        "ci/Dawn Android arm64 Builder",
        "ci/Dawn Android arm64 Release (Pixel 6)",
    ],
    gn_args = "ci/Dawn Android arm64 Builder",
    pool = "luci.chromium.gpu.try",
    builderless = True,
    os = os.LINUX_DEFAULT,
    ssd = None,
    free_space = None,
    max_concurrent_builds = 3,
    test_presentation = resultdb.test_presentation(
        grouping_keys = ["status", "v.test_suite", "v.gpu"],
    ),
)

try_.builder(
    name = "android-dawn-arm64-exp-rel",
    description_html = "Runs ToT Dawn tests on experimental Pixel 6 configs",
    mirrors = [
        "ci/Dawn Android arm64 Builder",
        "ci/Dawn Android arm64 Experimental Release (Pixel 6)",
    ],
    gn_args = "ci/Dawn Android arm64 Builder",
    pool = "luci.chromium.gpu.try",
    builderless = True,
    os = os.LINUX_DEFAULT,
    max_concurrent_builds = 1,
    test_presentation = resultdb.test_presentation(
        grouping_keys = ["status", "v.test_suite", "v.gpu"],
    ),
)

try_.builder(
    name = "android-dawn-arm64-p10-rel",
    description_html = "Runs ToT Dawn tests on Pixel 10 devices",
    mirrors = [
        "ci/Dawn Android arm64 Builder",
        "ci/Dawn Android arm64 Release (Pixel 10)",
    ],
    gn_args = "ci/Dawn Android arm64 Builder",
    pool = "luci.chromium.gpu.try",
    builderless = True,
    os = os.LINUX_DEFAULT,
    max_concurrent_builds = 1,
    test_presentation = resultdb.test_presentation(
        grouping_keys = ["status", "v.test_suite", "v.gpu"],
    ),
)

try_.builder(
    # This is not part of "android-dawn-arm64-rel" at the moment since there is
    # not sufficient S24 capacity for that.
    name = "android-dawn-arm64-s24-rel",
    description_html = "Runs ToT Dawn tests on Samsung S24 devices",
    mirrors = [
        "ci/Dawn Android arm64 Builder",
        "ci/Dawn Android arm64 Release (Samsung S24)",
    ],
    gn_args = "ci/Dawn Android arm64 Builder",
    pool = "luci.chromium.gpu.try",
    builderless = True,
    os = os.LINUX_DEFAULT,
    max_concurrent_builds = 1,
    test_presentation = resultdb.test_presentation(
        grouping_keys = ["status", "v.test_suite", "v.gpu"],
    ),
)

try_.builder(
    name = "linux-dawn-intel-exp-rel",
    description_html = "Runs ToT Dawn tests on experimental Linux/Intel configs",
    mirrors = [
        "ci/Dawn Linux x64 Builder",
        "ci/Dawn Linux x64 Experimental Release (Intel UHD 630)",
    ],
    gn_args = "ci/Dawn Linux x64 Builder",
    pool = "luci.chromium.gpu.try",
    builderless = True,
    os = os.LINUX_DEFAULT,
    max_concurrent_builds = 1,
    test_presentation = resultdb.test_presentation(
        grouping_keys = ["status", "v.test_suite", "v.gpu"],
    ),
)

try_.builder(
    name = "linux-dawn-nvidia-1660-exp-rel",
    description_html = "Runs ToT Dawn tests on experimental Linux/GTX 1660 configs",
    mirrors = [
        "ci/Dawn Linux x64 Builder",
        "ci/Dawn Linux x64 Experimental Release (NVIDIA GTX 1660)",
    ],
    gn_args = "ci/Dawn Linux x64 Builder",
    pool = "luci.chromium.gpu.try",
    builderless = True,
    os = os.LINUX_DEFAULT,
    max_concurrent_builds = 1,
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
    gn_args = "ci/Dawn Linux x64 Builder",
    pool = "luci.chromium.gpu.try",
    builderless = True,
    os = os.LINUX_DEFAULT,
    ssd = None,
    free_space = None,
    max_concurrent_builds = 3,
    test_presentation = resultdb.test_presentation(
        grouping_keys = ["status", "v.test_suite", "v.gpu"],
    ),
)

try_.builder(
    name = "mac-arm64-dawn-rel",
    description_html = "Runs Dawn tests on Apple silicon on Dawn ToT",
    mirrors = [
        "ci/Dawn Mac arm64 Builder",
        "ci/Dawn Mac arm64 Release (Apple M2)",
    ],
    gn_args = "ci/Dawn Mac arm64 Builder",
    pool = "luci.chromium.gpu.try",
    builderless = True,
    os = os.MAC_ANY,
    cpu = None,
    free_space = None,
    max_concurrent_builds = 5,
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
    gn_args = "ci/Dawn Mac x64 Builder",
    pool = "luci.chromium.gpu.try",
    builderless = True,
    os = os.MAC_ANY,
    cpu = cpu.ARM64,
    free_space = None,
    max_concurrent_builds = 3,
    test_presentation = resultdb.test_presentation(
        grouping_keys = ["status", "v.test_suite", "v.gpu"],
    ),
)

try_.builder(
    name = "dawn-try-linux-x64-intel-uhd770-rel",
    description_html = "Runs ToT Dawn tests on 12th gen Intel CPUs with UHD 770 GPUs",
    mirrors = [
        "ci/Dawn Linux x64 Builder",
        "ci/Dawn Linux x64 Release (Intel UHD 770)",
    ],
    gn_args = "ci/Dawn Linux x64 Builder",
    pool = "luci.chromium.gpu.try",
    builderless = True,
    max_concurrent_builds = 1,
    test_presentation = resultdb.test_presentation(
        grouping_keys = ["status", "v.test_suite", "v.gpu"],
    ),
)

try_.builder(
    name = "dawn-try-linux-tsan-rel",
    mirrors = [
        "ci/Dawn Linux TSAN Release",
    ],
    gn_args = "ci/Dawn Linux TSAN Release",
    pool = "luci.chromium.gpu.try",
    builderless = True,
    max_concurrent_builds = 1,
    test_presentation = resultdb.test_presentation(
        grouping_keys = ["status", "v.test_suite", "v.gpu"],
    ),
)

dawn_mac_builder(
    name = "dawn-try-mac-amd-555x-rel",
    mirrors = [
        "ci/Dawn Mac x64 Builder",
        "ci/Dawn Mac x64 Release (AMD Radeon Pro 555X)",
    ],
    gn_args = "ci/Dawn Mac x64 Builder",
    test_presentation = resultdb.test_presentation(
        grouping_keys = ["status", "v.test_suite", "v.gpu"],
    ),
)

dawn_mac_builder(
    name = "dawn-try-mac-amd-exp",
    mirrors = [
        "ci/Dawn Mac x64 Builder",
        "ci/Dawn Mac x64 Experimental Release (AMD)",
    ],
    gn_args = "ci/Dawn Mac x64 Builder",
    test_presentation = resultdb.test_presentation(
        grouping_keys = ["status", "v.test_suite", "v.gpu"],
    ),
)

dawn_mac_builder(
    name = "dawn-try-mac-arm64-deps-rel",
    mirrors = [
        "ci/Dawn Mac arm64 DEPS Builder",
        "ci/Dawn Mac arm64 DEPS Release (Apple M2)",
    ],
    gn_args = "ci/Dawn Mac arm64 DEPS Builder",
    test_presentation = resultdb.test_presentation(
        grouping_keys = ["status", "v.test_suite", "v.gpu"],
    ),
)

dawn_mac_builder(
    name = "dawn-try-mac-arm64-m2-exp",
    description_html = "Manual-only trybot for running ToT Dawn tests on experimental M2 machines",
    mirrors = [
        "ci/Dawn Mac arm64 Builder",
        "ci/Dawn Mac arm64 Experimental Release (Apple M2)",
    ],
    gn_args = "ci/Dawn Mac arm64 Builder",
    test_presentation = resultdb.test_presentation(
        grouping_keys = ["status", "v.test_suite", "v.gpu"],
    ),
)

dawn_mac_builder(
    name = "dawn-try-mac-arm64-rel",
    mirrors = [
        "ci/Dawn Mac arm64 Builder",
        "ci/Dawn Mac arm64 Release (Apple M2)",
    ],
    gn_args = "ci/Dawn Mac arm64 Builder",
    test_presentation = resultdb.test_presentation(
        grouping_keys = ["status", "v.test_suite", "v.gpu"],
    ),
)

dawn_mac_builder(
    name = "dawn-try-mac-intel-exp",
    mirrors = [
        "ci/Dawn Mac x64 Builder",
        "ci/Dawn Mac x64 Experimental Release (Intel)",
    ],
    gn_args = "ci/Dawn Mac x64 Builder",
    test_presentation = resultdb.test_presentation(
        grouping_keys = ["status", "v.test_suite", "v.gpu"],
    ),
)

# This will be moved into dawn-win11-arm64-deps-rel once the tests have been
# confirmed to be stable enough.
dawn_win_builderless_builder(
    name = "dawn-try-win11-arm64-snapdragon-x-elite-deps-rel",
    mirrors = [
        "ci/Dawn Win11 arm64 DEPS Builder",
        "ci/Dawn Win11 arm64 DEPS Release (Qualcomm Snapdragon X Elite)",
    ],
    gn_args = "ci/Dawn Win11 arm64 DEPS Builder",
    test_presentation = resultdb.test_presentation(
        grouping_keys = ["status", "v.test_suite", "v.gpu"],
    ),
)

dawn_win_builderless_builder(
    name = "dawn-try-win-x64-intel-exp",
    mirrors = [
        "ci/Dawn Win10 x64 Builder",
        "ci/Dawn Win10 x64 Experimental Release (Intel)",
    ],
    gn_args = "ci/Dawn Win10 x64 Builder",
    test_presentation = resultdb.test_presentation(
        grouping_keys = ["status", "v.test_suite", "v.gpu"],
    ),
)

dawn_win_builderless_builder(
    name = "dawn-try-win-x64-intel-uhd770-rel",
    description_html = "Runs ToT Dawn tests on 12th gen Intel CPUs with UHD 770 GPUs",
    mirrors = [
        "ci/Dawn Win10 x64 Builder",
        "ci/Dawn Win10 x64 Release (Intel UHD 770)",
    ],
    gn_args = "ci/Dawn Win10 x64 Builder",
    execution_timeout = 6 * time.hour,
    # Increased scheduling timeout due to resource contention with the mega CQ,
    # see crbug.com/384962295.
    expiration_timeout = 4 * time.hour,
    test_presentation = resultdb.test_presentation(
        grouping_keys = ["status", "v.test_suite", "v.gpu"],
    ),
)

dawn_win_builderless_builder(
    name = "dawn-try-win-x64-nvidia-exp",
    description_html = "Runs ToT Dawn tests on experimental NVIDIA configs",
    mirrors = [
        "ci/Dawn Win10 x64 Builder",
        "ci/Dawn Win10 x64 Experimental Release (NVIDIA)",
    ],
    gn_args = "ci/Dawn Win10 x64 Builder",
    test_presentation = resultdb.test_presentation(
        grouping_keys = ["status", "v.test_suite", "v.gpu"],
    ),
)

dawn_win_builderless_builder(
    name = "dawn-try-win-x86-intel-exp",
    mirrors = [
        "ci/Dawn Win10 x86 Builder",
        "ci/Dawn Win10 x86 Experimental Release (Intel)",
    ],
    gn_args = "ci/Dawn Win10 x86 Builder",
    test_presentation = resultdb.test_presentation(
        grouping_keys = ["status", "v.test_suite", "v.gpu"],
    ),
)

dawn_win_builderless_builder(
    name = "dawn-try-win-x86-nvidia-exp",
    description_html = "Runs ToT Dawn tests on experimental Win/NVIDIA/x86 configs",
    mirrors = [
        "ci/Dawn Win10 x86 Builder",
        "ci/Dawn Win10 x86 Experimental Release (NVIDIA)",
    ],
    gn_args = "ci/Dawn Win10 x86 Builder",
    test_presentation = resultdb.test_presentation(
        grouping_keys = ["status", "v.test_suite", "v.gpu"],
    ),
)

dawn_win_builderless_builder(
    name = "win-dawn-rel",
    mirrors = [
        "ci/Dawn Win10 x64 Builder",
        "ci/Dawn Win10 x64 Release (Intel)",
        "ci/Dawn Win10 x64 Release (NVIDIA)",
    ],
    gn_args = "ci/Dawn Win10 x64 Builder",
    max_concurrent_builds = 3,
    test_presentation = resultdb.test_presentation(
        grouping_keys = ["status", "v.test_suite", "v.gpu"],
    ),
)

dawn_win_builderless_builder(
    name = "win11-arm64-dawn-rel",
    mirrors = [
        "ci/Dawn Win11 arm64 Builder",
    ],
    gn_args = "ci/Dawn Win11 arm64 Builder",
    max_concurrent_builds = 6,
    test_presentation = resultdb.test_presentation(
        grouping_keys = ["status", "v.test_suite", "v.gpu"],
    ),
)

dawn_win_builderless_builder(
    name = "dawn-try-win10-x86-rel",
    mirrors = [
        "ci/Dawn Win10 x86 Builder",
        "ci/Dawn Win10 x86 Release (Intel)",
        "ci/Dawn Win10 x86 Release (NVIDIA)",
    ],
    gn_args = "ci/Dawn Win10 x86 Builder",
    test_presentation = resultdb.test_presentation(
        grouping_keys = ["status", "v.test_suite", "v.gpu"],
    ),
)

dawn_win_builderless_builder(
    name = "dawn-try-win10-x64-intel-asan",
    mirrors = [
        "ci/Dawn Win10 x64 ASAN Builder",
        "ci/Dawn Win10 x64 ASAN Release (Intel)",
    ],
    gn_args = "ci/Dawn Win10 x64 ASAN Builder",
    test_presentation = resultdb.test_presentation(
        grouping_keys = ["status", "v.test_suite", "v.gpu"],
    ),
)

dawn_win_builderless_builder(
    name = "dawn-try-win10-x64-nvidia-asan",
    mirrors = [
        "ci/Dawn Win10 x64 ASAN Builder",
        "ci/Dawn Win10 x64 ASAN Release (NVIDIA)",
    ],
    gn_args = "ci/Dawn Win10 x64 ASAN Builder",
    test_presentation = resultdb.test_presentation(
        grouping_keys = ["status", "v.test_suite", "v.gpu"],
    ),
)
