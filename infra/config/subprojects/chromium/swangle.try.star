# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

load("//lib/builders.star", "cpu", "os")
load("//lib/try.star", "try_")

try_.defaults.set(
    bucket = "try",
    build_numbers = True,
    caches = [
        swarming.cache(
            name = "win_toolchain",
            path = "win_toolchain",
        ),
    ],
    configure_kitchen = True,
    cores = 8,
    cpu = cpu.X86_64,
    cq_group = "cq",
    executable = "recipe:chromium_trybot",
    execution_timeout = 2 * time.hour,
    # Max. pending time for builds. CQ considers builds pending >2h as timed
    # out: http://shortn/_8PaHsdYmlq. Keep this in sync.
    expiration_timeout = 2 * time.hour,
    os = os.LINUX_DEFAULT,
    pool = "luci.chromium.try",
    service_account = "chromium-try-gpu-builder@chops-service-accounts.iam.gserviceaccount.com",
    subproject_list_view = "luci.chromium.try",
    swarming_tags = ["vpython:native-python-wrapper"],
    task_template_canary_percentage = 5,
)

try_.chromium_swangle_linux_builder(
    name = "linux-swangle-chromium-try-x64",
    pool = "luci.chromium.swangle.chromium.linux.x64.try",
    execution_timeout = 6 * time.hour,
    pinned = False,
)

try_.chromium_swangle_linux_builder(
    name = "linux-swangle-try-tot-angle-x64",
    pool = "luci.chromium.swangle.angle.linux.x64.try",
)

try_.chromium_swangle_linux_builder(
    name = "linux-swangle-try-tot-angle-x86",
    pool = "luci.chromium.swangle.linux.x86.try",
)

try_.chromium_swangle_linux_builder(
    name = "linux-swangle-try-tot-swiftshader-x64",
    pool = "luci.chromium.swangle.sws.linux.x64.try",
)

try_.chromium_swangle_linux_builder(
    name = "linux-swangle-try-tot-swiftshader-x86",
    pool = "luci.chromium.swangle.linux.x86.try",
)

try_.chromium_swangle_linux_builder(
    name = "linux-swangle-try-x64",
    pool = "luci.chromium.swangle.deps.linux.x64.try",
    pinned = False,
)

try_.chromium_swangle_linux_builder(
    name = "linux-swangle-try-x86",
    pool = "luci.chromium.swangle.linux.x86.try",
    pinned = False,
)

try_.chromium_swangle_mac_builder(
    name = "mac-swangle-chromium-try-x64",
    pool = "luci.chromium.swangle.chromium.mac.x64.try",
    execution_timeout = 6 * time.hour,
    pinned = False,
)

try_.chromium_swangle_windows_builder(
    name = "win-swangle-chromium-try-x86",
    pool = "luci.chromium.swangle.chromium.win.x86.try",
    execution_timeout = 6 * time.hour,
    pinned = False,
)

try_.chromium_swangle_windows_builder(
    name = "win-swangle-try-tot-angle-x64",
    pool = "luci.chromium.swangle.win.x64.try",
)

try_.chromium_swangle_windows_builder(
    name = "win-swangle-try-tot-angle-x86",
    pool = "luci.chromium.swangle.angle.win.x86.try",
)

try_.chromium_swangle_windows_builder(
    name = "win-swangle-try-tot-swiftshader-x64",
    pool = "luci.chromium.swangle.win.x64.try",
)

try_.chromium_swangle_windows_builder(
    name = "win-swangle-try-tot-swiftshader-x86",
    pool = "luci.chromium.swangle.sws.win.x86.try",
)

try_.chromium_swangle_windows_builder(
    name = "win-swangle-try-x64",
    pool = "luci.chromium.swangle.win.x64.try",
    pinned = False,
)

try_.chromium_swangle_windows_builder(
    name = "win-swangle-try-x86",
    pool = "luci.chromium.swangle.deps.win.x86.try",
    pinned = False,
)
