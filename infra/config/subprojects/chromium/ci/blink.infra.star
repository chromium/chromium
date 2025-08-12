# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Definitions of builders in the blink.infra builder group."""

load("@chromium-luci//builder_health_indicators.star", "health_spec")
load("@chromium-luci//builders.star", "os")
load("@chromium-luci//ci.star", "ci")
load("@chromium-luci//consoles.star", "consoles")
load("//lib/ci_constants.star", "ci_constants")

ci.defaults.set(
    pool = ci_constants.DEFAULT_POOL,
    builderless = True,
    os = os.LINUX_DEFAULT,
    console_view = "blink.infra",
    execution_timeout = 10 * time.hour,
    health_spec = health_spec.default(),
)

consoles.console_view(
    name = "blink.infra",
)

ci.builder(
    name = "blink-flake-suppressor",
    description_html = "Runs Flake Suppressor on all gardened builders to generate test suppression cl.",
    executable = "recipe:chromium_expectation_files/expectation_file_scripts",
    # Run once at 4 PM Pacific on weekdays.
    schedule = "0 0 * * 1-5",
    triggered_by = [],
    cores = 8,
    console_view_entry = consoles.console_view_entry(
        short_name = "bfs",
    ),
    contact_team_email = "chrome-blink-engprod@google.com",
    properties = {
        "scripts": [
            {
                "step_name": "generate_test_suppression_cl",
                "script": "third_party/blink/tools/suppress_flakes.py",
                "script_type": "FLAKE_FINDER",
                "submit_type": "MANUAL",
                "reviewer_list": {
                    "reviewer": [
                        "jiesheng@google.com",
                    ],
                },
                "cl_title": "Blink web tests suppression",
                "args": [
                    # TODO(crbug.com/40237087): Create a new project to avoid
                    # capacity issue.
                    "--project",
                    "chrome-unexpected-pass-data",
                    "--no-prompt-for-user-input",
                    # Only suppress tests that in the past 7 days, causes
                    # build failures in 2 consecutive days, at least 10
                    # times total and in recent 2 days still causes build
                    # failures.
                    "--sample-period",
                    "7",
                    "--non-hidden-failures-only",
                    "--build-fail-consecutive-days-threshold",
                    "2",
                    "--build-fail-total-number-threshold",
                    "10",
                    "--build-fail-recent-days-threshold",
                    "2",
                ],
            },
        ],
    },
    service_account = "chromium-automated-expectation@chops-service-accounts.iam.gserviceaccount.com",
)

ci.builder(
    name = "blink-web-test-analyzer",
    description_html = "Runs all web test analyzers on web tests bugs.",
    executable = "recipe:chromium/generic_script_runner",
    # Run every 6 hours.
    schedule = "0 */6 * * *",
    triggered_by = [],
    cores = 8,
    console_view_entry = consoles.console_view_entry(
        short_name = "fda",
    ),
    contact_team_email = "chrome-blink-engprod@google.com",
    properties = {
        "scripts": [
            {
                "step_name": "analyze_flaky_image_web_tests",
                "script": "third_party/blink/tools/run_fuzzy_diff_analyzer.py",
                "args": [
                    "--project",
                    "chrome-unexpected-pass-data",
                    "--sample-period",
                    "7",
                    "--check-bugs-only",
                    "--attach-analysis-result",
                ],
            },
            {
                "step_name": "analyze_slow_web_tests",
                "script": "third_party/blink/tools/run_slow_test_analyzer.py",
                "args": [
                    "--project",
                    "chrome-unexpected-pass-data",
                    "--sample-period",
                    "7",
                    "--check-bugs-only",
                    "--attach-analysis-result",
                ],
            },
        ],
    },
    service_account = "chromium-automated-expectation@chops-service-accounts.iam.gserviceaccount.com",
)

ci.builder(
    name = "blink-virtual-test-suites-notifier",
    description_html = "Sends notifications for expired Virtual Test Suites",
    executable = "recipe:chromium/generic_script_runner",
    # Run once daily at 12 PM Pacific/7 PM UTC.
    schedule = "0 19 * * *",
    triggered_by = [],
    cores = 8,
    console_view_entry = consoles.console_view_entry(
        short_name = "vts-notify",
    ),
    contact_team_email = "chrome-experience-engprod@google.com",
    properties = {
        "scripts": [
            {
                "step_name": "notify_vts",
                "script": "third_party/blink/tools/notify_vts.py",
            },
        ],
    },
    service_account = "chromium-automated-expectation@chops-service-accounts.iam.gserviceaccount.com",
)
