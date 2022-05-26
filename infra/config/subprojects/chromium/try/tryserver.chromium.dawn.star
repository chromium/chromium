# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Definitions of builders in the tryserver.chromium.swangle builder group."""

load("//lib/branches.star", "branches")
load("//lib/builders.star", "goma", "os")
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
    service_account = try_.gpu.SERVICE_ACCOUNT,
)

consoles.list_view(
    name = "tryserver.chromium.dawn",
    branch_selector = branches.DESKTOP_EXTENDED_STABLE_MILESTONE,
)

try_.builder(
    name = "dawn-linux-x64-deps-rel",
    branch_selector = branches.STANDARD_MILESTONE,
    mirrors = [
        "ci/Dawn Linux x64 DEPS Builder",
        "ci/Dawn Linux x64 DEPS Release (Intel HD 630)",
        "ci/Dawn Linux x64 DEPS Release (NVIDIA)",
    ],
    main_list_view = "try",
    tryjob = try_.job(
        location_regexp = [
            ".+/[+]/content/test/gpu/.+",
            ".+/[+]/gpu/.+",
            ".+/[+]/testing/buildbot/chromium.dawn.json",
            ".+/[+]/third_party/blink/renderer/modules/webgpu/.+",
            ".+/[+]/third_party/blink/web_tests/external/wpt/webgpu/.+",
            ".+/[+]/third_party/blink/web_tests/wpt_internal/webgpu/.+",
            ".+/[+]/third_party/blink/web_tests/WebGPUExpectations",
            ".+/[+]/third_party/dawn/.+",
            ".+/[+]/third_party/webgpu-cts/.+",
            ".+/[+]/tools/clang/scripts/update.py",
            ".+/[+]/ui/gl/features.gni",
        ],
    ),
)

try_.builder(
    name = "dawn-mac-x64-deps-rel",
    branch_selector = branches.DESKTOP_EXTENDED_STABLE_MILESTONE,
    mirrors = [
        "ci/Dawn Mac x64 DEPS Builder",
        "ci/Dawn Mac x64 DEPS Release (AMD)",
        "ci/Dawn Mac x64 DEPS Release (Intel)",
    ],
    main_list_view = "try",
    os = os.MAC_ANY,
    tryjob = try_.job(
        location_regexp = [
            ".+/[+]/content/test/gpu/.+",
            ".+/[+]/gpu/.+",
            ".+/[+]/testing/buildbot/chromium.dawn.json",
            ".+/[+]/third_party/blink/renderer/modules/webgpu/.+",
            ".+/[+]/third_party/blink/web_tests/external/wpt/webgpu/.+",
            ".+/[+]/third_party/blink/web_tests/wpt_internal/webgpu/.+",
            ".+/[+]/third_party/blink/web_tests/WebGPUExpectations",
            ".+/[+]/third_party/dawn/.+",
            ".+/[+]/third_party/webgpu-cts/.+",
            ".+/[+]/tools/clang/scripts/update.py",
            ".+/[+]/ui/gl/features.gni",
        ],
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
        location_regexp = [
            ".+/[+]/content/test/gpu/.+",
            ".+/[+]/gpu/.+",
            ".+/[+]/testing/buildbot/chromium.dawn.json",
            ".+/[+]/third_party/blink/renderer/modules/webgpu/.+",
            ".+/[+]/third_party/blink/web_tests/external/wpt/webgpu/.+",
            ".+/[+]/third_party/blink/web_tests/wpt_internal/webgpu/.+",
            ".+/[+]/third_party/blink/web_tests/WebGPUExpectations",
            ".+/[+]/third_party/dawn/.+",
            ".+/[+]/third_party/webgpu-cts/.+",
            ".+/[+]/tools/clang/scripts/update.py",
            ".+/[+]/ui/gl/features.gni",
        ],
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
        location_regexp = [
            ".+/[+]/content/test/gpu/.+",
            ".+/[+]/gpu/.+",
            ".+/[+]/testing/buildbot/chromium.dawn.json",
            ".+/[+]/third_party/blink/renderer/modules/webgpu/.+",
            ".+/[+]/third_party/blink/web_tests/external/wpt/webgpu/.+",
            ".+/[+]/third_party/blink/web_tests/wpt_internal/webgpu/.+",
            ".+/[+]/third_party/blink/web_tests/WebGPUExpectations",
            ".+/[+]/third_party/dawn/.+",
            ".+/[+]/third_party/webgpu-cts/.+",
            ".+/[+]/tools/clang/scripts/update.py",
            ".+/[+]/ui/gl/features.gni",
        ],
    ),
)

try_.builder(
    name = "linux-dawn-rel",
)

try_.builder(
    name = "mac-dawn-rel",
    os = os.MAC_ANY,
)

try_.builder(
    name = "dawn-try-mac-amd-exp",
    builderless = True,
    os = os.MAC_ANY,
    pool = "luci.chromium.gpu.mac.retina.amd.try",
)

try_.builder(
    name = "dawn-try-mac-intel-exp",
    builderless = True,
    os = os.MAC_ANY,
    pool = "luci.chromium.gpu.mac.mini.intel.try",
)

try_.builder(
    name = "win-dawn-rel",
    os = os.WINDOWS_ANY,
)

try_.builder(
    name = "dawn-try-win10-x86-rel",
    os = os.WINDOWS_ANY,
)

try_.builder(
    name = "dawn-try-win10-x64-asan-rel",
    os = os.WINDOWS_ANY,
)
