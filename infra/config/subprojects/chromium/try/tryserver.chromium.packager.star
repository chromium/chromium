# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Definitions of builders in the tryserver.chromium.packager builder group."""

load("//lib/builders.star", "os")
load("//lib/try.star", "try_")
load("//lib/consoles.star", "consoles")

try_.defaults.set(
    executable = "recipe:chromium_3pp",
    builder_group = "tryserver.chromium.packager",
    pool = try_.DEFAULT_POOL,
    execution_timeout = try_.DEFAULT_EXECUTION_TIMEOUT,
    service_account = "chromium-cipd-try-builder@chops-service-accounts.iam.gserviceaccount.com",
)

consoles.list_view(
    name = "tryserver.chromium.packager",
)

try_.builder(
    name = "3pp-linux-amd64-packager",
    builderless = False,
    cores = 8,
    os = os.LINUX_DEFAULT,
    properties = {
        "$build/chromium_3pp": {
            "platform": "linux-amd64",
            "package_prefix": "chromium_3pp",
            "preprocess": [{
                "name": "third_party/android_deps",
                "cmd": [
                    "{CHECKOUT}/src/third_party/android_deps/fetch_all.py",
                    "-v",
                    "--ignore-vulnerabilities",
                ],
            }],
            "gclient_config": "chromium",
            "gclient_apply_config": ["android"],
        },
    },
    tryjob = try_.job(
        location_filters = [
            # Enable for CLs touching files under "3pp" directories which are
            # two level deep or more from the repo root.
            ".+/3pp/.+",
        ],
    ),
)

try_.builder(
    name = "3pp-mac-amd64-packager",
    builderless = True,
    os = os.MAC_DEFAULT,
    properties = {
        "$build/chromium_3pp": {
            "platform": "mac-amd64",
            "package_prefix": "chromium_3pp",
            "gclient_config": "chromium",
        },
    },
)
