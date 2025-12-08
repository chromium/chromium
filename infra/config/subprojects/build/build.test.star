# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Definitions of builders with test RBE instances."""

load("@chromium-luci//builder_config.star", "builder_config")
load("@chromium-luci//builders.star", "os")
load("@chromium-luci//ci.star", "ci")
load("@chromium-luci//consoles.star", "consoles")
load("@chromium-luci//gn_args.star", "gn_args")
load("@chromium-luci//html.star", "linkify_builder")
load("//lib/ci_constants.star", "ci_constants")
load("//lib/siso.star", "siso")
load("//project.star", "settings")

luci.gitiles_poller(
    name = "chrome-build-gitiles-trigger",
    bucket = "build",
    repo = "https://chromium.googlesource.com/chromium/src",
    refs = [settings.ref],
)

ci.defaults.set(
    bucket = "build",
    triggered_by = ["chrome-build-gitiles-trigger"],
    builder_group = "chromium.build.test",
    pool = ci_constants.DEFAULT_POOL,
    builderless = True,
    build_numbers = True,
    contact_team_email = "chrome-build-team@google.com",
    execution_timeout = 10 * time.hour,
    priority = ci_constants.DEFAULT_FYI_PRIORITY,
    resultdb_enable = False,
    service_account = "chromium-build-perf-ci-builder@chops-service-accounts.iam.gserviceaccount.com",
    siso_experiments = ["no-fallback"],
)

consoles.console_view(
    name = "chromium.build.test",
    repo = "https://chromium.googlesource.com/chromium/src",
)

def cq_build_perf_builder(**kwargs):
    # Use CQ RBE instance and high remote_jobs to simulate CQ builds.
    return ci.builder(
        siso_project = siso.project.TEST_UNTRUSTED,
        siso_remote_jobs = siso.remote_jobs.HIGH_JOBS_FOR_CQ,
        siso_remote_linking = True,
        use_clang_coverage = True,
        **kwargs
    )

def ci_build_perf_builder(**kwargs):
    # Use CI RBE instance to simulate CI builds.
    return ci.builder(
        siso_project = siso.project.TEST_TRUSTED,
        siso_remote_jobs = siso.remote_jobs.DEFAULT,
        **kwargs
    )

# Builders with rbe-chromium-untrusted-test.
cq_build_perf_builder(
    name = "linux-rbe-untrusted-test",
    description_html = "This builder builds Linux CQ build with rbe-chroimum-untrusted-test.<br/>" +
                       "The build configs and the bot specs should be in sync with " + linkify_builder("try", "linux-rel-compilator", "chromium"),
    executable = "recipe:chrome_build/build_perf_siso",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "siso_latest",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "mb",
            ],
            target_platform = builder_config.target_platform.LINUX,
        ),
    ),
    gn_args = {
        "builtin": "try/linux-rel",
        "no_clang_modules": gn_args.config(configs = ["try/linux-rel", "no_clang_modules"]),
    },
    os = os.LINUX_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "untrusted",
        short_name = "lin",
    ),
)

cq_build_perf_builder(
    name = "win-rbe-untrusted-test",
    description_html = "This builder builds Windows CQ build with rbe-chroimum-untrusted-test.<br/>" +
                       "The build configs and the bot specs should be in sync with " + linkify_builder("try", "win-rel-compilator", "chromium"),
    executable = "recipe:chrome_build/build_perf_siso",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "siso_latest",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "mb",
            ],
            target_platform = builder_config.target_platform.WIN,
        ),
    ),
    gn_args = {
        "builtin": "try/win-rel",
        "no_clang_modules": gn_args.config(configs = ["try/win-rel", "no_clang_modules"]),
    },
    os = os.WINDOWS_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "untrusted",
        short_name = "win",
    ),
    # Downloading with "minimum" strategy doesn't work
    # well for the win builder because some steps are missing inputs.
    # e.g. mini_installer.exe
    siso_output_local_strategy = "greedy",
)

# Builders with rbe-chromium-trusted-test.
ci_build_perf_builder(
    name = "linux-rbe-trusted-test",
    description_html = "This builder builds Linux CI build with rbe-chroimum-trusted-test.<br/>" +
                       "The build configs and the bot specs should be in sync with " + linkify_builder("ci", "Linux Builder", "chromium"),
    executable = "recipe:chrome_build/build_perf_siso",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "siso_latest",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "mb",
            ],
            target_platform = builder_config.target_platform.LINUX,
        ),
    ),
    gn_args = {
        "builtin": "ci/Linux Builder",
        "no_clang_modules": gn_args.config(configs = ["ci/Linux Builder", "no_clang_modules"]),
    },
    os = os.LINUX_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "trusted",
        short_name = "lin",
    ),
    siso_remote_linking = True,
)

ci_build_perf_builder(
    name = "win-rbe-trusted-test",
    description_html = "This builder builds Windows CI build with rbe-chroimum-trusted-test.<br/>" +
                       "The build configs and the bot specs should be in sync with " + linkify_builder("ci", "Win x64 Builder", "chromium"),
    executable = "recipe:chrome_build/build_perf_siso",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "siso_latest",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "mb",
            ],
            target_platform = builder_config.target_platform.WIN,
        ),
    ),
    gn_args = {
        "builtin": "ci/Win x64 Builder",
        "no_clang_modules": gn_args.config(configs = ["ci/Win x64 Builder", "no_clang_modules"]),
    },
    os = os.WINDOWS_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "trusted",
        short_name = "win",
    ),
)
