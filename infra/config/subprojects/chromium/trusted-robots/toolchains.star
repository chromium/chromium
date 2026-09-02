# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Definitions of builders in the chromium.trusted-robots builder group for toolchains."""

load("@chromium-luci//ci.star", "ci")
load("@chromium-luci//consoles.star", "consoles")

ci.builder(
    name = "linux_clang",
    description_html = "Builder for Clang toolchain",
    executable = "recipe:chromium_toolchain/trusted_packaging",
    schedule = "0 19 * * *",  # 7pm UTC / 5am AEST / 11am PST / 12pm PDT
    cores = 2,
    console_view_entry = consoles.console_view_entry(
        console_view = "chromium.trusted-robots",
        short_name = "lnx",
    ),
    contact_team_email = "dlf@google.com",
    execution_timeout = 6 * time.hour,
    properties = {
        "toolchain": "CLANG",
        "trusted_build_instance": "lexan-release-infra-linux-prod-postsubmit",
        "trusted_build_instance_env": "prod",
        "trusted_build_instance_pool": "default",
        "trusted_build_instance_project": "lexan-release-infra-prod",
    },
    service_account = "lexan-swarming-prod@lexan-release-infra-prod.iam.gserviceaccount.com",
)

ci.builder(
    name = "staging.linux_clang",
    description_html = "Staging builder for Clang toolchain",
    executable = "recipe:chromium_toolchain/trusted_packaging",
    schedule = "0 19 * * *",  # 7pm UTC / 5am AEST / 11am PST / 12pm PDT
    cores = 2,
    console_view_entry = consoles.console_view_entry(
        console_view = "chromium.trusted-robots",
        short_name = "stg.lnx",
    ),
    contact_team_email = "dlf@google.com",
    execution_timeout = 6 * time.hour,
    properties = {
        "toolchain": "CLANG",
        "trusted_build_instance": "lexan-release-infra-linux-staging-postsubmit",
        "trusted_build_instance_env": "staging",
        "trusted_build_instance_pool": "default",
        "trusted_build_instance_project": "lexan-release-infra-staging",
    },
    service_account = "lexan-swarming-staging@lexan-release-infra-staging.iam.gserviceaccount.com",
)
