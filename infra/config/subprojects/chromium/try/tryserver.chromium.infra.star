# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Definitions of builders in the tryserver.chromium.infra builder group."""

load("//lib/builders.star", "os")
load("//lib/try.star", "try_")
load("//lib/consoles.star", "consoles")

try_.defaults.set(
    builder_group = "tryserver.chromium.infra",
    pool = try_.DEFAULT_POOL,
    execution_timeout = try_.DEFAULT_EXECUTION_TIMEOUT,
    service_account = "chromium-cipd-try-builder@chops-service-accounts.iam.gserviceaccount.com",
)

consoles.list_view(
    name = "tryserver.chromium.infra",
)

try_.builder(
    name = "3pp-linux-amd64-packager",
    executable = "recipe:chromium_3pp",
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
    executable = "recipe:chromium_3pp",
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

try_.builder(
    name = "3pp-windows-amd64-packager",
    description_html = "3PP Packager for Windows",
    executable = "recipe:chromium_3pp",
    builderless = True,
    os = os.WINDOWS_DEFAULT,
    contact_team_email = "chrome-browser-infra-team@google.com",
    properties = {
        "$build/chromium_3pp": {
            "platform": "windows-amd64",
            "package_prefix": "chromium_3pp",
            "gclient_config": "chromium",
        },
    },
)

try_.builder(
    name = "mega-cq-launcher",
    # TODO(crbug.com/1227778): Document the Mega-CQ somewhere in markdown, then
    # link to it in the description here.
    description_html = "Triggers all builders needed for Chromium's Mega CQ.",
    executable = "recipe:chromium/mega_cq_launcher",
    builderless = True,
    cores = 2,
    os = os.LINUX_DEFAULT,
    contact_team_email = "chrome-browser-infra-team@google.com",
    execution_timeout = 36 * time.hour,  # We expect it can take a while.
    service_account = try_.DEFAULT_SERVICE_ACCOUNT,
)
