# Copyright 2022 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Definitions of builders in the presubmit.linux builder group."""

load("//lib/builders.star", "os")
load("//lib/ci.star", "ci")
load("//lib/consoles.star", "consoles")

ci.defaults.set(
    console_view = "checks",
    service_account = ci.DEFAULT_SERVICE_ACCOUNT,
)

consoles.console_view(
    name = "checks",
)

ci.builder(
    name = "linux-presubmit",
    console_view_entry = consoles.console_view_entry(
        console_view = "checks",
        category = "presubmit",
        short_name = "linux",
    ),
    cores = 32,
    builderless = True,
    execution_timeout = ci.DEFAULT_EXECUTION_TIMEOUT,
    os = os.LINUX_DEFAULT,
    executable = "recipe:presubmit",
    properties = {
        "$depot_tools/presubmit": {
            "runhooks": True,
            "timeout_s": 480000,
        },
        "repo_name": "chromium",
    },

    # TODO(crbug.com/1370463): remove this.
    omit_python2 = False,
)
