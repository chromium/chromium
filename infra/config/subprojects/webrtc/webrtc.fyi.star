# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

load("//lib/builders.star", "builder", "cpu", "defaults", "goma", "os", "xcode")

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
            groups = "project-chromium-admins",
        ),
        acl.entry(
            roles = acl.SCHEDULER_OWNER,
            groups = "project-webrtc-admins",
        ),
    ],
)

luci.gitiles_poller(
    name = "webrtc-gitiles-trigger",
    bucket = "webrtc",
    repo = "https://webrtc.googlesource.com/src/",
    refs = ["refs/heads/main"],
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
defaults.triggered_by.set(["webrtc-gitiles-trigger"])

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
    name = "WebRTC Chromium FYI Android Tests (dbg) (M Nexus5X)",
    triggered_by = ["WebRTC Chromium FYI Android Builder ARM64 (dbg)"],
)

builder(
    name = "WebRTC Chromium FYI Android Tests (dbg) (N Nexus5X)",
    triggered_by = ["WebRTC Chromium FYI Android Builder (dbg)"],
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
    goma_backend = goma.backend.RBE_PROD,
    os = os.MAC_ANY,
    xcode = xcode.x14main,
)

builder(
    name = "WebRTC Chromium FYI Mac Builder (dbg)",
    goma_backend = goma.backend.RBE_PROD,
    os = os.MAC_ANY,
)

builder(
    name = "WebRTC Chromium FYI Mac Tester",
    os = os.MAC_ANY,
    triggered_by = ["WebRTC Chromium FYI Mac Builder"],
    xcode = xcode.x14main,
)

builder(
    name = "WebRTC Chromium FYI Win Builder",
    goma_backend = goma.backend.RBE_PROD,
    goma_enable_ats = True,
    os = os.WINDOWS_DEFAULT,
)

builder(
    name = "WebRTC Chromium FYI Win Builder (dbg)",
    goma_backend = goma.backend.RBE_PROD,
    goma_enable_ats = True,
    os = os.WINDOWS_DEFAULT,
)

builder(
    name = "WebRTC Chromium FYI Win10 Tester",
    os = os.WINDOWS_DEFAULT,
    triggered_by = ["WebRTC Chromium FYI Win Builder"],
)

# Builders run on the default Win OS version offered
# in the Chrome infra however the tasks will be sharded
# to swarming bots with appropriate OS using swarming
# dimensions.
builder(
    name = "WebRTC Chromium FYI ios-device",
    goma_backend = goma.backend.RBE_PROD,
    os = os.MAC_ANY,
    xcode = xcode.x14main,
)

builder(
    name = "WebRTC Chromium FYI ios-simulator",
    goma_backend = goma.backend.RBE_PROD,
    os = os.MAC_ANY,
    xcode = xcode.x14main,
)
