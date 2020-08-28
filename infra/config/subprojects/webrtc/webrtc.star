# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

load("//lib/builders.star", "builder", "cpu", "defaults", "goma", "os")

luci.bucket(
    name = "webrtc",
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

defaults.bucket.set("webrtc")
defaults.builder_group.set("chromium.webrtc")
defaults.builderless.set(False)
defaults.build_numbers.set(True)
defaults.cpu.set(cpu.X86_64)
defaults.executable.set("recipe:chromium")
defaults.execution_timeout.set(2 * time.hour)
defaults.os.set(os.LINUX_DEFAULT)
defaults.service_account.set("chromium-ci-builder@chops-service-accounts.iam.gserviceaccount.com")
defaults.swarming_tags.set(["vpython:native-python-wrapper"])
defaults.triggered_by.set(["master-gitiles-trigger"])

defaults.properties.set({
    "perf_dashboard_machine_group": "ChromiumWebRTC",
})

# Builders are defined in lexicographic order by name

builder(
    name = "WebRTC Chromium Android Builder",
    goma_backend = goma.backend.RBE_PROD,
)

builder(
    name = "WebRTC Chromium Android Tester",
    triggered_by = ["WebRTC Chromium Android Builder"],
)

builder(
    name = "WebRTC Chromium Linux Builder",
    goma_backend = goma.backend.RBE_PROD,
)

builder(
    name = "WebRTC Chromium Linux Tester",
    triggered_by = ["WebRTC Chromium Linux Builder"],
)

builder(
    name = "WebRTC Chromium Mac Builder",
    cores = 8,
    goma_backend = goma.backend.RBE_PROD,
    os = os.MAC_ANY,
)

builder(
    name = "WebRTC Chromium Mac Tester",
    os = os.MAC_ANY,
    triggered_by = ["WebRTC Chromium Mac Builder"],
)

builder(
    name = "WebRTC Chromium Win Builder",
    goma_backend = goma.backend.RBE_PROD,
    os = os.WINDOWS_ANY,
)

builder(
    name = "WebRTC Chromium Win10 Tester",
    os = os.WINDOWS_ANY,
    triggered_by = ["WebRTC Chromium Win Builder"],
)

builder(
    name = "WebRTC Chromium Win7 Tester",
    os = os.WINDOWS_ANY,
    triggered_by = ["WebRTC Chromium Win Builder"],
)

builder(
    name = "WebRTC Chromium Win8 Tester",
    os = os.WINDOWS_ANY,
    triggered_by = ["WebRTC Chromium Win Builder"],
)
