# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Definitions of builders in chromium.build.fyi builder group."""

load("//lib/builder_config.star", "builder_config")
load("//lib/builders.star", "cpu", "os", "reclient")
load("//lib/ci.star", "ci")
load("//lib/consoles.star", "consoles")

ci.defaults.set(
    bucket = "build",
    executable = ci.DEFAULT_EXECUTABLE,
    triggered_by = ["chrome-build-gitiles-trigger"],
    builder_group = "chromium.build.fyi",
    pool = ci.DEFAULT_POOL,
    builderless = True,
    build_numbers = True,
    contact_team_email = "chrome-build-team@google.com",
    execution_timeout = 10 * time.hour,
    priority = ci.DEFAULT_FYI_PRIORITY,
    reclient_instance = reclient.instance.DEFAULT_TRUSTED,
    reclient_jobs = reclient.jobs.DEFAULT,
    service_account = ci.DEFAULT_SERVICE_ACCOUNT,
    shadow_reclient_instance = reclient.instance.DEFAULT_UNTRUSTED,
    shadow_service_account = ci.DEFAULT_SHADOW_SERVICE_ACCOUNT,
    siso_enabled = True,
)

consoles.console_view(
    name = "chromium.build.fyi",
    repo = "https://chromium.googlesource.com/chromium/src",
)

ci.builder(
    name = "Mac Builder Siso FYI",
    description_html = "This builder is intended to test the latest Siso version on Mac.",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                # This is necessary due to a child builder running the
                # telemetry_perf_unittests suite.
                "chromium_with_telemetry_dependencies",
                "use_clang_coverage",
                "siso_latest",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_arch = builder_config.target_arch.INTEL,
            target_bits = 64,
            target_platform = builder_config.target_platform.MAC,
        ),
        build_gs_bucket = "chromium-fyi-archive",
    ),
    gn_args = "ci/Mac Builder",
    cores = None,
    os = os.MAC_DEFAULT,
    cpu = cpu.ARM64,
    console_view_entry = consoles.console_view_entry(
        category = "mac",
        short_name = "build",
    ),
)

ci.thin_tester(
    name = "Mac13 Tests Siso FYI",
    description_html = """\
This builder is intended to shadow <a href="https://ci.chromium.org/ui/p/chromium/builders/ci/Mac13%20Tests">Mac13 Tests</a>.<br/>\
But, the tests are built by <a ref="https://ci.chromium.org/ui/p/chromium/builders/build/Mac%20Builder%20Siso%20FYI">Mac Builder Siso FYI</a>.\
""",
    triggered_by = ["build/Mac Builder Siso FYI"],
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
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
            target_platform = builder_config.target_platform.MAC,
        ),
        build_gs_bucket = "chromium-fyi-archive",
    ),
    console_view_entry = consoles.console_view_entry(
        category = "mac",
        short_name = "test",
    ),
)
