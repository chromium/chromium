# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Definitions of builders in the tryserver.chromium.linux builder group."""

load("@chromium-luci//try.star", "try_")
load("@chromium-luci//builders.star", "os")
load("@chromium-luci//consoles.star", "consoles")
load("//lib/try_constants.star", "try_constants")
load("//lib/siso.star", "siso")

try_.defaults.set(
    executable = "recipe:chromium/eval_prompts",
    builder_group = "tryserver.chromium.prompt_eval",
    pool = try_constants.DEFAULT_POOL,
    builderless = False,
    execution_timeout = try_constants.DEFAULT_EXECUTION_TIMEOUT,
    experiments = {
        "chromium_tests.resultdb_module": 100,
    },
    service_account = try_constants.DEFAULT_SERVICE_ACCOUNT,
    siso_project = siso.project.DEFAULT_UNTRUSTED,
)

consoles.list_view(
    name = "tryserver.chromium.prompt_eval",
)

try_.builder(
    name = "linux-prompt-evals",
    mirrors = ["ci/linux-prompt-evals"],
    os = os.LINUX_DEFAULT,
    contact_team_email = "chrome-dev-infra-team@google.com",
)
