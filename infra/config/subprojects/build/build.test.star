# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Definitions of builders with test RBE instances."""

load("//lib/builder_config.star", "builder_config")
load("//lib/builders.star", "os", "siso")
load("//lib/ci.star", "ci")
load("//lib/consoles.star", "consoles")
load("//lib/html.star", "linkify_builder")
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
    pool = ci.DEFAULT_POOL,
    builderless = True,
    build_numbers = True,
    contact_team_email = "chrome-build-team@google.com",
    execution_timeout = 10 * time.hour,
    priority = ci.DEFAULT_FYI_PRIORITY,
    resultdb_enable = False,
    service_account = ci.DEFAULT_SERVICE_ACCOUNT,
    shadow_service_account = ci.DEFAULT_SHADOW_SERVICE_ACCOUNT,
    siso_configs = ["builder", "remote-link"],
    siso_experiments = ["no-fallback"],
)

consoles.console_view(
    name = "chromium.build.test",
    repo = "https://chromium.googlesource.com/chromium/src",
)

def cq_build_perf_builder(**kwargs):
    # Use CQ RBE instance and high remote_jobs to simulate CQ builds.
    return ci.builder(
        siso_remote_jobs = siso.remote_jobs.HIGH_JOBS_FOR_CQ,
        siso_project = siso.project.TEST_UNTRUSTED,
        use_clang_coverage = True,
        **kwargs
    )

def ci_build_perf_builder(**kwargs):
    # Use CI RBE instance to simulate CI builds.
    return ci.builder(
        siso_remote_jobs = siso.remote_jobs.DEFAULT,
        siso_project = siso.project.TEST_TRUSTED,
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
    },
    os = os.WINDOWS_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "untrusted",
        short_name = "win",
    ),
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
    },
    os = os.LINUX_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "trusted",
        short_name = "lin",
    ),
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
    },
    os = os.WINDOWS_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "trusted",
        short_name = "win",
    ),
)
