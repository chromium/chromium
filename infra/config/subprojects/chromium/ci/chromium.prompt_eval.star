# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Definitions of builders in the chromium.prompt_eval builder group."""

load("@chromium-luci//ci.star", "ci")
load("@chromium-luci//builder_health_indicators.star", "health_spec")
load("@chromium-luci//consoles.star", "consoles")
load("@chromium-luci//builders.star", "os")
load("@chromium-luci//branches.star", "branches")
load("@chromium-luci//builder_config.star", "builder_config")
load("//lib/ci_constants.star", "ci_constants")
load("//lib/siso.star", "siso")

ci.defaults.set(
    executable = "recipe:chromium/eval_prompts",
    builder_group = "chromium.prompt_eval",
    pool = ci_constants.DEFAULT_POOL,
    builderless = False,
    cores = 8,
    contact_team_email = "chrome-dev-infra-team@google.com",
    execution_timeout = ci_constants.DEFAULT_EXECUTION_TIMEOUT,
    experiments = {
        "chromium_tests.resultdb_module": 100,
    },
    health_spec = health_spec.default(),
    service_account = ci_constants.DEFAULT_SERVICE_ACCOUNT,
    shadow_service_account = ci_constants.DEFAULT_SHADOW_SERVICE_ACCOUNT,
    siso_project = siso.project.DEFAULT_TRUSTED,
)

consoles.console_view(
    name = "chromium.prompt_eval",
)

ci.builder(
    name = "linux-prompt-evals",
    branch_selector = branches.selector.MAIN,
    description_html = "Runs agent prompt evals on linux",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
            target_platform = builder_config.target_platform.LINUX,
        ),
    ),
    os = os.LINUX_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "linux",
    ),
)
