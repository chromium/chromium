# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Definitions of builders in the blink.infra builder group."""

load("//lib/ci.star", "ci")
load("//lib/consoles.star", "consoles")

ci.defaults.set(
    console_view = "blink.infra",
    execution_timeout = 10 * time.hour,
    pool = ci.DEFAULT_POOL,
)

consoles.console_view(
    name = "blink.infra",
)

ci.builder(
    name = "Blink Unexpected Pass Finder",
    builderless = True,
    cores = 16,
    console_view_entry = consoles.console_view_entry(
        short_name = "upf",
    ),
    executable = "recipe:chromium_expectation_files/expectation_file_scripts",
    # Run once daily at 12 AM Pacific/7 AM UTC.
    schedule = "0 7 * * *",
    triggered_by = [],
    service_account = "chromium-automated-expectation@chops-service-accounts.iam.gserviceaccount.com",
    properties = {
        "scripts": [
            {
                "step_name": "remove_stale_blink_expectations",
                "script": "third_party/blink/tools/remove_stale_expectations.py",
                "script_type": "UNEXPECTED_PASS",
                "submit_type": "AUTO",
                "reviewer_list": {
                    # List pulled from volunteers from blink-dev@ that are
                    # familiar with web tests/web test expectations. A random
                    # one will be CCed on each generated CL.
                    "reviewer": [
                        "awillia@chromium.org",
                        "jiesheng@google.com",
                        "jonathanjlee@google.com",
                        "nsatragno@chromium.org",
                        "tkent@chromium.org",
                        "wangxianzhu@chromium.org",
                    ],
                },
                "cl_title": "Remove stale Blink expectations",
                "args": [
                    "--project",
                    "chrome-unexpected-pass-data",
                    "--no-include-internal-builders",
                    "--remove-stale-expectations",
                    "--large-query-mode",
                    "--num-samples",
                    "200",
                    # We need to limit the max number of parallel jobs in order
                    # to avoid having large memory usage spikes that can kill
                    # the bot due to swap space not being enabled.
                    "--jobs",
                    "2",
                ],
            },
        ],
    },
)
