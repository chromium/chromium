# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

load("//lib/builders.star", "builder", "cpu", "defaults", "goma", "os", "xcode_cache")

luci.bucket(
    name = "webrtc.fyi",
    acls = [
        acl.entry(
            roles = acl.BUILDBUCKET_READER,
            groups = "all",
        ),
        acl.entry(
            roles = acl.BUILDBUCKET_TRIGGERER,
            groups = "project-chromium-ci-schedulers",
        ),
        acl.entry(
            roles = acl.BUILDBUCKET_OWNER,
            groups = "google/luci-task-force@google.com",
        ),
        acl.entry(
            roles = acl.SCHEDULER_OWNER,
            groups = "project-webrtc-admins",
        ),
    ],
)

luci.gitiles_poller(
    name = "webrtc-gitiles-trigger-master",
    bucket = "webrtc",
    repo = "https://webrtc.googlesource.com/src/",
)

defaults.bucket.set("webrtc.fyi")
defaults.builder_group.set("chromium.webrtc.fyi")
defaults.builderless.set(None)
defaults.build_numbers.set(True)
defaults.cpu.set(cpu.X86_64)
defaults.executable.set("recipe:chromium")
defaults.execution_timeout.set(2 * time.hour)
defaults.os.set(os.LINUX_DEFAULT)
defaults.pool.set("luci.chromium.webrtc.fyi")
defaults.service_account.set("chromium-ci-builder@chops-service-accounts.iam.gserviceaccount.com")
defaults.swarming_tags.set(["vpython:native-python-wrapper"])
defaults.triggered_by.set(["webrtc-gitiles-trigger-master"])

# Builders are defined in lexicographic order by name

builder(
    name = "WebRTC Chromium FYI Android Builder",
    goma_backend = goma.backend.RBE_PROD,
)

builder(
    name = "WebRTC Chromium FYI Android Builder (dbg)",
    goma_backend = goma.backend.RBE_PROD,
)

builder(
    name = "WebRTC Chromium FYI Android Builder ARM64 (dbg)",
    goma_backend = goma.backend.RBE_PROD,
)

builder(
    name = "WebRTC Chromium FYI Android Tests (dbg) (L Nexus5)",
    triggered_by = ["WebRTC Chromium FYI Android Builder (dbg)"],
)

builder(
    name = "WebRTC Chromium FYI Android Tests (dbg) (M Nexus5X)",
    triggered_by = ["WebRTC Chromium FYI Android Builder ARM64 (dbg)"],
)

builder(
    name = "WebRTC Chromium FYI Linux Builder",
    goma_backend = goma.backend.RBE_PROD,
)

builder(
    name = "WebRTC Chromium FYI Linux Builder (dbg)",
    goma_backend = goma.backend.RBE_PROD,
)

builder(
    name = "WebRTC Chromium FYI Linux Tester",
    triggered_by = ["WebRTC Chromium FYI Linux Builder"],
)

builder(
    name = "WebRTC Chromium FYI Mac Builder",
    cores = 8,
    caches = [xcode_cache.x11c29],
    goma_backend = goma.backend.RBE_PROD,
    os = os.MAC_ANY,
    properties = {
        "xcode_build_version": "11c29",
    },
)

builder(
    name = "WebRTC Chromium FYI Mac Builder (dbg)",
    cores = 8,
    caches = [xcode_cache.x11c29],
    goma_backend = goma.backend.RBE_PROD,
    os = os.MAC_ANY,
    properties = {
        "xcode_build_version": "11c29",
    },
)

builder(
    name = "WebRTC Chromium FYI Mac Tester",
    caches = [xcode_cache.x11c29],
    os = os.MAC_ANY,
    properties = {
        "xcode_build_version": "11c29",
    },
    triggered_by = ["WebRTC Chromium FYI Mac Builder"],
)

builder(
    name = "WebRTC Chromium FYI Win Builder",
    goma_backend = goma.backend.RBE_PROD,
    os = os.WINDOWS_DEFAULT,
)

builder(
    name = "WebRTC Chromium FYI Win Builder (dbg)",
    goma_backend = goma.backend.RBE_PROD,
    os = os.WINDOWS_DEFAULT,
)

builder(
    name = "WebRTC Chromium FYI Win10 Tester",
    os = os.WINDOWS_DEFAULT,
    triggered_by = ["WebRTC Chromium FYI Win Builder"],
)

builder(
    name = "WebRTC Chromium FYI Win7 Tester",
    os = os.WINDOWS_7,
    triggered_by = ["WebRTC Chromium FYI Win Builder"],
)

builder(
    name = "WebRTC Chromium FYI Win8 Tester",
    os = os.WINDOWS_8_1,
    triggered_by = ["WebRTC Chromium FYI Win Builder"],
)

builder(
    name = "WebRTC Chromium FYI ios-device",
    caches = [xcode_cache.x12a7209],
    executable = "recipe:webrtc/chromium_ios",
    goma_backend = goma.backend.RBE_PROD,
    os = os.MAC_ANY,
    properties = {
        "xcode_build_version": "12a7209",
    },
)

builder(
    name = "WebRTC Chromium FYI ios-simulator",
    caches = [xcode_cache.x12a7209],
    executable = "recipe:webrtc/chromium_ios",
    goma_backend = goma.backend.RBE_PROD,
    os = os.MAC_ANY,
    properties = {
        "xcode_build_version": "12a7209",
    },
)
