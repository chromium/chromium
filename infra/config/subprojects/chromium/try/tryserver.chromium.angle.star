# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Definitions of builders in the tryserver.chromium.angle builder group."""

load("//lib/builders.star", "goma", "os")
load("//lib/consoles.star", "consoles")
load("//lib/try.star", "try_")

try_.defaults.set(
    executable = try_.DEFAULT_EXECUTABLE,
    builder_group = "tryserver.chromium.angle",
    pool = try_.DEFAULT_POOL,
    builderless = False,
    cores = 8,
    os = os.LINUX_DEFAULT,
    execution_timeout = try_.DEFAULT_EXECUTION_TIMEOUT,
    goma_backend = goma.backend.RBE_PROD,
    goma_jobs = goma.jobs.J150,
    service_account = try_.gpu.SERVICE_ACCOUNT,
)

consoles.list_view(
    name = "tryserver.chromium.angle",
)

try_.builder(
    name = "android-angle-chromium-try",
    executable = "recipe:angle_chromium_trybot",
)

try_.builder(
    name = "android-angle-try",
    executable = "recipe:angle_chromium_trybot",
)

try_.builder(
    name = "android_angle_rel_ng",
)

try_.builder(
    name = "fuchsia-angle-rel",
)

try_.builder(
    name = "fuchsia-angle-try",
    executable = "recipe:angle_chromium_trybot",
)

try_.builder(
    name = "linux-angle-rel",
)

try_.builder(
    name = "linux-angle-chromium-try",
    executable = "recipe:angle_chromium_trybot",
)

try_.builder(
    name = "linux-angle-try",
    executable = "recipe:angle_chromium_trybot",
)

try_.builder(
    name = "mac-angle-chromium-try",
    executable = "recipe:angle_chromium_trybot",
    cores = None,
    os = os.MAC_ANY,
)

try_.builder(
    name = "win-angle-chromium-x64-try",
    executable = "recipe:angle_chromium_trybot",
    os = os.WINDOWS_ANY,
)

try_.builder(
    name = "win-angle-chromium-x86-try",
    executable = "recipe:angle_chromium_trybot",
    os = os.WINDOWS_ANY,
)

try_.builder(
    name = "win-angle-x64-try",
    executable = "recipe:angle_chromium_trybot",
    os = os.WINDOWS_ANY,
)
