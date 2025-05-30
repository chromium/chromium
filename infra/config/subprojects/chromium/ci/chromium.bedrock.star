# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Definitions of builders in chromium.bedrock builder group."""

load("//lib/builder_config.star", "builder_config")
load("//lib/builders.star", "os")
load("//lib/ci.star", "ci")
load("//lib/consoles.star", "consoles")
load("//lib/gn_args.star", "gn_args")

ci.defaults.set(
    builder_group = "chromium.bedrock",
    pool = ci.DEFAULT_POOL,
    builderless = True,
    service_account = ci.DEFAULT_SERVICE_ACCOUNT,
)

consoles.console_view(
    name = "chromium.bedrock",
)

ci.builder(
    name = "linux-bedrock-browser-metrics",
    description_html = "This builder collects browser metrics for project bedrock.",
    executable = "recipe:chromium/generic_script_runner",
    # TODO: jwata - trigger builds routinely once works fine.
    schedule = "0 0 1 1 *",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            target_platform = builder_config.target_platform.LINUX,
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "linux",
            "x64",
        ],
    ),
    os = os.LINUX_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "bedrock",
    ),
    contact_team_email = "webui-everywhere@google.com",
)
