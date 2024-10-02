# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Definitions of builders in the tryserver.chromium.infra builder group."""

load("//lib/builder_config.star", "builder_config")
load("//lib/builders.star", "os", "siso")
load("//lib/html.star", "linkify")
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
            # Also enable for cls that affect fetch_all.py or the groovy scripts
            # it runs unders buildSrc.
            "third_party/android_deps/fetch_all.py",
            "third_party/android_deps/buildSrc/src/main/groovy/.+",
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
    # TODO(crbug.com/40189365): Document the Mega-CQ somewhere in markdown, then
    # link to it in the description here.
    description_html = "Triggers all builders needed for Chromium's Mega CQ.",
    executable = "recipe:chromium/mega_cq_launcher",
    builderless = True,
    cores = 2,
    os = os.LINUX_DEFAULT,
    contact_team_email = "chrome-browser-infra-team@google.com",
    execution_timeout = 36 * time.hour,  # We expect it can take a while.
    service_account = try_.DEFAULT_SERVICE_ACCOUNT,
    tryjob = try_.job(
        custom_cq_run_modes = [try_.MEGA_CQ_DRY_RUN_NAME, try_.MEGA_CQ_FULL_RUN_NAME],
    ),
)

try_.builder(
    name = "linux-utr-tester",
    description_html = "Tests the {} against cli and recipe changes.".format(
        linkify(
            "https://chromium.googlesource.com/chromium/src/+/HEAD/tools/utr/README.md",
            "Universal Test Runner",
        ),
    ),
    executable = "recipe:chromium/universal_test_runner_test",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
            target_platform = builder_config.target_platform.LINUX,
        ),
    ),
    builderless = True,
    cores = 8,
    os = os.LINUX_DEFAULT,
    contact_team_email = "chrome-dev-infra-team@google.com",
    execution_timeout = 2 * time.hour,
    properties = {
        "builder_suites": [
            {
                "bucket": "try",
                "builder_name": "linux-rel",
                "test_names": [
                    "url_unittests",
                ],
                "build_dir": "out/linux-rel",
            },
            {
                "bucket": "ci",
                "builder_name": "Linux Tests",
                "test_names": [
                    "telemetry_gpu_unittests",
                ],
                "build_dir": "out/linux-rel",
            },
        ],
    },
    service_account = try_.DEFAULT_SERVICE_ACCOUNT,
    siso_project = siso.project.DEFAULT_UNTRUSTED,
    tryjob = try_.job(
        location_filters = [
            "tools/utr/.+",
            "tools/mb/.+",
        ],
    ),
)

try_.builder(
    name = "win-utr-tester",
    description_html = "Tests the {} against cli and recipe changes.".format(
        linkify(
            "https://chromium.googlesource.com/chromium/src/+/HEAD/tools/utr/README.md",
            "Universal Test Runner",
        ),
    ),
    executable = "recipe:chromium/universal_test_runner_test",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
            target_platform = builder_config.target_platform.WIN,
        ),
    ),
    builderless = True,
    cores = 8,
    os = os.WINDOWS_DEFAULT,
    contact_team_email = "chrome-dev-infra-team@google.com",
    execution_timeout = 2 * time.hour,
    properties = {
        "builder_suites": [
            {
                "bucket": "try",
                "builder_name": "win-rel",
                "test_names": [
                    "url_unittests",
                ],
                "build_dir": "out/win-rel",
            },
            {
                "bucket": "ci",
                "builder_name": "Win10 Tests x64",
                "test_names": [
                    "telemetry_gpu_unittests",
                ],
                "build_dir": "out/win-rel",
            },
        ],
    },
    service_account = try_.DEFAULT_SERVICE_ACCOUNT,
    siso_project = siso.project.DEFAULT_UNTRUSTED,
    tryjob = try_.job(
        location_filters = [
            "tools/utr/.+",
            "tools/mb/.+",
        ],
    ),
)
