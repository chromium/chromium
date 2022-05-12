# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Definitions of builders in the tryserver.chromium.updater builder group."""

load("//lib/builders.star", "goma", "os")
load("//lib/try.star", "try_")
load("//lib/consoles.star", "consoles")

try_.defaults.set(
    builder_group = "tryserver.chromium.updater",
    builderless = True,
    executable = try_.DEFAULT_EXECUTABLE,
    execution_timeout = try_.DEFAULT_EXECUTION_TIMEOUT,
    goma_backend = goma.backend.RBE_PROD,
    pool = try_.DEFAULT_POOL,
    service_account = try_.DEFAULT_SERVICE_ACCOUNT,
)

consoles.list_view(
    name = "tryserver.chromium.updater",
)

def updater_mac_builder(*, name, **kwargs):
    kwargs.setdefault("os", os.MAC_ANY)
    return try_.builder(name = name, **kwargs)

def updater_windows_builder(*, name, **kwargs):
    kwargs.setdefault("cores", 8)
    kwargs.setdefault("os", os.WINDOWS_DEFAULT)
    return try_.builder(name = name, **kwargs)

updater_mac_builder(
    name = "mac-updater-try-builder-dbg",
    main_list_view = "try",
    tryjob = try_.job(
        location_regexp = [
            ".+/[+]/chrome/updater/.+",
        ],
    ),
)

updater_mac_builder(
    name = "mac-updater-try-builder-rel",
    main_list_view = "try",
    tryjob = try_.job(
        location_regexp = [
            ".+/[+]/chrome/updater/.+",
        ],
    ),
)

updater_windows_builder(
    name = "win-updater-try-builder-dbg",
    main_list_view = "try",
    tryjob = try_.job(
        location_regexp = [
            ".+/[+]/chrome/updater/.+",
        ],
    ),
)

updater_windows_builder(
    name = "win-updater-try-builder-rel",
    mirrors = [
        "ci/win-updater-builder-rel",
        "ci/win10-updater-tester-rel",
    ],
    main_list_view = "try",
    tryjob = try_.job(
        location_regexp = [
            ".+/[+]/chrome/updater/.+",
        ],
    ),
)
