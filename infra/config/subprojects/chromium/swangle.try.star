# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

load("//lib/builders.star", "cpu", "goma", "os")
load("//lib/consoles.star", "consoles")
load("//lib/try.star", "try_")

try_.defaults.set(
    bucket = "try",
    build_numbers = True,
    builderless = True,
    builder_group = "tryserver.chromium.swangle",
    caches = [
        swarming.cache(
            name = "win_toolchain",
            path = "win_toolchain",
        ),
    ],
    cpu = cpu.X86_64,
    cq_group = "cq",
    executable = "recipe:angle_chromium_trybot",
    execution_timeout = 2 * time.hour,
    # Max. pending time for builds. CQ considers builds pending >2h as timed
    # out: http://shortn/_8PaHsdYmlq. Keep this in sync.
    expiration_timeout = 2 * time.hour,
    goma_backend = goma.backend.RBE_PROD,
    os = os.LINUX_DEFAULT,
    pool = "luci.chromium.try",
    service_account = "chromium-try-gpu-builder@chops-service-accounts.iam.gserviceaccount.com",
    subproject_list_view = "luci.chromium.try",
    task_template_canary_percentage = 5,
    # TODO(crbug.com/1362440): remove this.
    omit_python2 = False,
)

consoles.list_view(
    name = "tryserver.chromium.swangle",
)

def swangle_linux_builder(*, name, **kwargs):
    kwargs.setdefault("cores", 8)
    kwargs.setdefault("os", os.LINUX_DEFAULT)
    return try_.builder(name = name, **kwargs)

def swangle_mac_builder(*, name, **kwargs):
    kwargs.setdefault("os", os.MAC_ANY)
    return try_.builder(name = name, **kwargs)

def swangle_windows_builder(*, name, **kwargs):
    kwargs.setdefault("cores", 8)
    kwargs.setdefault("os", os.WINDOWS_DEFAULT)
    return try_.builder(name = name, **kwargs)

swangle_linux_builder(
    name = "linux-swangle-chromium-try-x64",
    pool = "luci.chromium.swangle.chromium.linux.x64.try",
    executable = "recipe:chromium_trybot",
    execution_timeout = 6 * time.hour,
)

swangle_linux_builder(
    name = "linux-swangle-try-tot-swiftshader-x64",
    pool = "luci.chromium.swangle.sws.linux.x64.try",
)

swangle_linux_builder(
    name = "linux-swangle-try-x64",
    pool = "luci.chromium.swangle.deps.linux.x64.try",
    executable = "recipe:chromium_trybot",
)

swangle_mac_builder(
    name = "mac-swangle-chromium-try-x64",
    pool = "luci.chromium.swangle.chromium.mac.x64.try",
    executable = "recipe:chromium_trybot",
    execution_timeout = 6 * time.hour,
)

swangle_windows_builder(
    name = "win-swangle-chromium-try-x86",
    pool = "luci.chromium.swangle.chromium.win.x86.try",
    executable = "recipe:chromium_trybot",
    execution_timeout = 6 * time.hour,
)

swangle_windows_builder(
    name = "win-swangle-try-tot-swiftshader-x64",
    pool = "luci.chromium.swangle.win.x64.try",
)

swangle_windows_builder(
    name = "win-swangle-try-tot-swiftshader-x86",
    pool = "luci.chromium.swangle.sws.win.x86.try",
)

swangle_windows_builder(
    name = "win-swangle-try-x64",
    pool = "luci.chromium.swangle.win.x64.try",
    executable = "recipe:chromium_trybot",
)

swangle_windows_builder(
    name = "win-swangle-try-x86",
    pool = "luci.chromium.swangle.deps.win.x86.try",
    executable = "recipe:chromium_trybot",
)
