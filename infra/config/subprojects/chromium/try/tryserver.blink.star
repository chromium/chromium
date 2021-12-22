# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Definitions of builders in the tryserver.blink builder group."""

load("//lib/builders.star", "goma", "os")
load("//lib/branches.star", "branches")
load("//lib/try.star", "try_")
load("//lib/consoles.star", "consoles")

try_.defaults.set(
    builder_group = "tryserver.blink",
    cores = 8,
    executable = try_.DEFAULT_EXECUTABLE,
    execution_timeout = try_.DEFAULT_EXECUTION_TIMEOUT,
    pool = try_.DEFAULT_POOL,
    service_account = try_.DEFAULT_SERVICE_ACCOUNT,
)

consoles.list_view(
    name = "tryserver.blink",
    branch_selector = branches.STANDARD_MILESTONE,
)

def blink_mac_builder(*, name, **kwargs):
    kwargs.setdefault("builderless", True)
    kwargs.setdefault("cores", None)
    kwargs.setdefault("goma_backend", goma.backend.RBE_PROD)
    kwargs.setdefault("os", os.MAC_ANY)
    kwargs.setdefault("ssd", True)
    return try_.builder(
        name = name,
        **kwargs
    )

try_.builder(
    name = "linux-blink-optional-highdpi-rel",
    goma_backend = goma.backend.RBE_PROD,
    os = os.LINUX_BIONIC_SWITCH_TO_DEFAULT,
)

try_.builder(
    name = "linux-blink-rel",
    branch_selector = branches.STANDARD_MILESTONE,
    goma_backend = goma.backend.RBE_PROD,
    main_list_view = "try",
    os = os.LINUX_BIONIC_SWITCH_TO_DEFAULT,
    tryjob = try_.job(
        location_regexp = [
            ".+/[+]/cc/.+",
            ".+/[+]/third_party/blink/renderer/core/paint/.+",
            ".+/[+]/third_party/blink/renderer/core/svg/.+",
            ".+/[+]/third_party/blink/renderer/platform/graphics/.+",
        ],
    ),
)

try_.builder(
    name = "win10.20h2-blink-rel",
    goma_backend = goma.backend.RBE_PROD,
    os = os.WINDOWS_ANY,
    builderless = True,
)

try_.builder(
    name = "win7-blink-rel",
    goma_backend = goma.backend.RBE_PROD,
    os = os.WINDOWS_ANY,
    builderless = True,
)

blink_mac_builder(
    name = "mac10.12-blink-rel",
)

blink_mac_builder(
    name = "mac10.13-blink-rel",
)

blink_mac_builder(
    name = "mac10.14-blink-rel",
)

blink_mac_builder(
    name = "mac10.15-blink-rel",
)

blink_mac_builder(
    name = "mac11.0-blink-rel",
    builderless = False,
)

blink_mac_builder(
    name = "mac11.0.arm64-blink-rel",
)
