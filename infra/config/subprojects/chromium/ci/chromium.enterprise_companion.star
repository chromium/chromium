# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Definitions of builders in the chromium.enterprise_companion group."""

load("//lib/builder_config.star", "builder_config")
load("//lib/builders.star", "builders", "cpu", "os", "siso")
load("//lib/ci.star", "ci")
load("//lib/consoles.star", "consoles")
load("//lib/gn_args.star", "gn_args")
load("//lib/builder_health_indicators.star", "health_spec")
load("//lib/html.star", "linkify")

ci.defaults.set(
    executable = ci.DEFAULT_EXECUTABLE,
    builder_group = "chromium.enterprise_companion",
    pool = ci.DEFAULT_POOL,
    cores = 8,
    os = os.LINUX_DEFAULT,
    execution_timeout = ci.DEFAULT_EXECUTION_TIMEOUT,
    health_spec = health_spec.DEFAULT,
    service_account = ci.DEFAULT_SERVICE_ACCOUNT,
    shadow_service_account = ci.DEFAULT_SHADOW_SERVICE_ACCOUNT,
    siso_enabled = True,
    siso_project = siso.project.DEFAULT_TRUSTED,
    siso_remote_jobs = siso.remote_jobs.DEFAULT,
)

consoles.console_view(
    name = "chromium.enterprise_companion",
)

ci.builder(
    name = "linux-enterprise-companion-builder-dbg",
    description_html = linkify("https://source.chromium.org/chromium/chromium/src/+/main:chrome/enterprise_companion/README.md", "Chrome Enterprise Companion App") + " Linux x64 Debug Builder.",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "mb",
            ],
            build_config = builder_config.build_config.DEBUG,
            target_bits = 64,
            target_platform = builder_config.target_platform.LINUX,
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "enterprise_companion",
            "debug_static_builder",
            "remoteexec",
            "linux",
            "x64",
        ],
    ),
    builderless = True,
    os = os.LINUX_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "debug|linux",
        short_name = "bld",
    ),
    contact_team_email = "omaha-client-dev@google.com",
)

ci.builder(
    name = "linux-enterprise-companion-builder-rel",
    description_html = linkify("https://source.chromium.org/chromium/chromium/src/+/main:chrome/enterprise_companion/README.md", "Chrome Enterprise Companion App") + " Linux x64 Release Builder.",
    builder_spec = builder_config.builder_spec(
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
            target_platform = builder_config.target_platform.LINUX,
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "enterprise_companion",
            "release_builder",
            "remoteexec",
            "linux",
            "x64",
        ],
    ),
    builderless = True,
    os = os.LINUX_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "release|linux",
        short_name = "bld",
    ),
    contact_team_email = "omaha-client-dev@google.com",
)

ci.thin_tester(
    name = "linux-enterprise-companion-tester-dbg",
    description_html = linkify("https://source.chromium.org/chromium/chromium/src/+/main:chrome/enterprise_companion/README.md", "Chrome Enterprise Companion App") + " Linux x64 Debug Tester.",
    triggered_by = ["linux-enterprise-companion-builder-dbg"],
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
            build_config = builder_config.build_config.DEBUG,
            target_bits = 64,
            target_platform = builder_config.target_platform.LINUX,
        ),
    ),
    console_view_entry = consoles.console_view_entry(
        category = "debug|linux",
        short_name = "test",
    ),
    contact_team_email = "omaha-client-dev@google.com",
)

ci.thin_tester(
    name = "linux-enterprise-companion-tester-rel",
    description_html = linkify("https://source.chromium.org/chromium/chromium/src/+/main:chrome/enterprise_companion/README.md", "Chrome Enterprise Companion App") + " Linux x64 Release Tester.",
    triggered_by = ["linux-enterprise-companion-builder-rel"],
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
            target_platform = builder_config.target_platform.LINUX,
        ),
    ),
    console_view_entry = consoles.console_view_entry(
        category = "release|linux",
        short_name = "test",
    ),
    contact_team_email = "omaha-client-dev@google.com",
)

ci.builder(
    name = "mac-enterprise-companion-builder-dbg",
    description_html = linkify("https://source.chromium.org/chromium/chromium/src/+/main:chrome/enterprise_companion/README.md", "Chrome Enterprise Companion App") + " Mac x64 Debug Builder.",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "mb",
            ],
            build_config = builder_config.build_config.DEBUG,
            target_bits = 64,
            target_platform = builder_config.target_platform.MAC,
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "enterprise_companion",
            "debug_static_builder",
            "remoteexec",
            "mac",
            "x64",
        ],
    ),
    builderless = True,
    cores = None,
    os = os.MAC_ANY,
    cpu = cpu.ARM64,
    console_view_entry = consoles.console_view_entry(
        category = "debug|mac",
        short_name = "bld",
    ),
    contact_team_email = "omaha-client-dev@google.com",
)

ci.builder(
    name = "mac-enterprise-companion-builder-rel",
    description_html = linkify("https://source.chromium.org/chromium/chromium/src/+/main:chrome/enterprise_companion/README.md", "Chrome Enterprise Companion App") + " Mac x64 Release Builder.",
    builder_spec = builder_config.builder_spec(
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
    ),
    gn_args = gn_args.config(
        configs = [
            "enterprise_companion",
            "release_builder",
            "remoteexec",
            "mac",
            "x64",
        ],
    ),
    builderless = True,
    cores = None,
    os = os.MAC_ANY,
    cpu = cpu.ARM64,
    console_view_entry = consoles.console_view_entry(
        category = "release|mac",
        short_name = "bld",
    ),
    contact_team_email = "omaha-client-dev@google.com",
)

ci.builder(
    name = "mac-enterprise-companion-builder-arm64-dbg",
    description_html = linkify("https://source.chromium.org/chromium/chromium/src/+/main:chrome/enterprise_companion/README.md", "Chrome Enterprise Companion App") + " Mac ARM64 Debug Builder.",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "mb",
            ],
            build_config = builder_config.build_config.DEBUG,
            target_bits = 64,
            target_platform = builder_config.target_platform.MAC,
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "mac",
            "arm64",
            "enterprise_companion",
            "debug_static_builder",
            "remoteexec",
        ],
    ),
    builderless = True,
    cores = None,
    os = os.MAC_ANY,
    cpu = cpu.ARM64,
    console_view_entry = consoles.console_view_entry(
        category = "debug|mac",
        short_name = "bld",
    ),
    contact_team_email = "omaha-client-dev@google.com",
)

ci.builder(
    name = "mac-enterprise-companion-builder-arm64-rel",
    description_html = linkify("https://source.chromium.org/chromium/chromium/src/+/main:chrome/enterprise_companion/README.md", "Chrome Enterprise Companion App") + " Mac ARM64 Release Builder.",
    builder_spec = builder_config.builder_spec(
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
    ),
    gn_args = gn_args.config(
        configs = [
            "mac",
            "arm64",
            "enterprise_companion",
            "release_builder",
            "remoteexec",
        ],
    ),
    builderless = True,
    cores = None,
    os = os.MAC_ANY,
    cpu = cpu.ARM64,
    console_view_entry = consoles.console_view_entry(
        category = "release|mac",
        short_name = "bld",
    ),
    contact_team_email = "omaha-client-dev@google.com",
)

ci.builder(
    name = "mac-enterprise-companion-builder-asan-dbg",
    description_html = linkify("https://source.chromium.org/chromium/chromium/src/+/main:chrome/enterprise_companion/README.md", "Chrome Enterprise Companion App") + " Mac x64 ASAN Debug Builder.",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "mb",
            ],
            build_config = builder_config.build_config.DEBUG,
            target_bits = 64,
            target_platform = builder_config.target_platform.MAC,
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "enterprise_companion",
            "asan",
            "debug_static_builder",
            "remoteexec",
            "mac",
            "x64",
        ],
    ),
    builderless = True,
    cores = None,
    os = os.MAC_ANY,
    cpu = cpu.ARM64,
    console_view_entry = consoles.console_view_entry(
        category = "debug|mac",
        short_name = "bld-asan",
    ),
    contact_team_email = "omaha-client-dev@google.com",
)

ci.thin_tester(
    name = "mac11-arm64-enterprise-companion-tester-dbg",
    description_html = linkify("https://source.chromium.org/chromium/chromium/src/+/main:chrome/enterprise_companion/README.md", "Chrome Enterprise Companion App") + " MacOS 11 ARM64 Debug Tester.",
    triggered_by = ["mac-enterprise-companion-builder-arm64-dbg"],
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
            build_config = builder_config.build_config.DEBUG,
            target_bits = 64,
            target_platform = builder_config.target_platform.MAC,
        ),
    ),
    console_view_entry = consoles.console_view_entry(
        category = "debug|mac",
        short_name = "11 arm64",
    ),
    contact_team_email = "omaha-client-dev@google.com",
)

ci.thin_tester(
    name = "mac11-arm64-enterprise-companion-tester-rel",
    description_html = linkify("https://source.chromium.org/chromium/chromium/src/+/main:chrome/enterprise_companion/README.md", "Chrome Enterprise Companion App") + " MacOS 11 ARM64 Release Tester.",
    triggered_by = ["mac-enterprise-companion-builder-arm64-rel"],
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
    ),
    console_view_entry = consoles.console_view_entry(
        category = "release|mac",
        short_name = "11 arm64",
    ),
    contact_team_email = "omaha-client-dev@google.com",
)

ci.thin_tester(
    name = "mac11-x64-enterprise-companion-tester-dbg",
    description_html = linkify("https://source.chromium.org/chromium/chromium/src/+/main:chrome/enterprise_companion/README.md", "Chrome Enterprise Companion App") + " MacOS 11 x64 Debug Tester.",
    triggered_by = ["mac-enterprise-companion-builder-dbg"],
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
            build_config = builder_config.build_config.DEBUG,
            target_bits = 64,
            target_platform = builder_config.target_platform.MAC,
        ),
    ),
    console_view_entry = consoles.console_view_entry(
        category = "debug|mac",
        short_name = "11",
    ),
    contact_team_email = "omaha-client-dev@google.com",
)

ci.thin_tester(
    name = "mac11-x64-enterprise-companion-tester-rel",
    description_html = linkify("https://source.chromium.org/chromium/chromium/src/+/main:chrome/enterprise_companion/README.md", "Chrome Enterprise Companion App") + " MacOS 11 x64 Release Tester.",
    triggered_by = ["mac-enterprise-companion-builder-rel"],
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
    ),
    console_view_entry = consoles.console_view_entry(
        category = "release|mac",
        short_name = "11",
    ),
    contact_team_email = "omaha-client-dev@google.com",
)

ci.thin_tester(
    name = "mac12-arm64-enterprise-companion-tester-rel",
    description_html = linkify("https://source.chromium.org/chromium/chromium/src/+/main:chrome/enterprise_companion/README.md", "Chrome Enterprise Companion App") + " MacOS 12 ARM64 Release Tester.",
    triggered_by = ["mac-enterprise-companion-builder-arm64-rel"],
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
    ),
    console_view_entry = consoles.console_view_entry(
        category = "release|mac",
        short_name = "12 arm64",
    ),
    contact_team_email = "omaha-client-dev@google.com",
)

ci.thin_tester(
    name = "mac12-x64-enterprise-companion-tester-asan-dbg",
    description_html = linkify("https://source.chromium.org/chromium/chromium/src/+/main:chrome/enterprise_companion/README.md", "Chrome Enterprise Companion App") + " MacOS 12 x64 ASAN Debug Tester.",
    triggered_by = ["mac-enterprise-companion-builder-asan-dbg"],
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
            build_config = builder_config.build_config.DEBUG,
            target_bits = 64,
            target_platform = builder_config.target_platform.MAC,
        ),
    ),
    console_view_entry = consoles.console_view_entry(
        category = "debug|mac",
        short_name = "12 asan",
    ),
    contact_team_email = "omaha-client-dev@google.com",
)

ci.thin_tester(
    name = "mac13-arm64-enterprise-companion-tester-dbg",
    description_html = linkify("https://source.chromium.org/chromium/chromium/src/+/main:chrome/enterprise_companion/README.md", "Chrome Enterprise Companion App") + " MacOS 13 ARM64 Debug Tester.",
    triggered_by = ["mac-enterprise-companion-builder-arm64-dbg"],
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
            build_config = builder_config.build_config.DEBUG,
            target_bits = 64,
            target_platform = builder_config.target_platform.MAC,
        ),
    ),
    console_view_entry = consoles.console_view_entry(
        category = "debug|mac",
        short_name = "13 arm64",
    ),
    contact_team_email = "omaha-client-dev@google.com",
)

ci.thin_tester(
    name = "mac13-x64-enterprise-companion-tester-rel",
    description_html = linkify("https://source.chromium.org/chromium/chromium/src/+/main:chrome/enterprise_companion/README.md", "Chrome Enterprise Companion App") + " MacOS 13 x64 Release Tester.",
    triggered_by = ["mac-enterprise-companion-builder-rel"],
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
    ),
    console_view_entry = consoles.console_view_entry(
        category = "release|mac",
        short_name = "13",
    ),
    contact_team_email = "omaha-client-dev@google.com",
)

ci.builder(
    name = "win-enterprise-companion-builder-dbg",
    description_html = linkify("https://source.chromium.org/chromium/chromium/src/+/main:chrome/enterprise_companion/README.md", "Chrome Enterprise Companion App") + " Windows x64 Debug Builder.",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "mb",
            ],
            build_config = builder_config.build_config.DEBUG,
            target_bits = 64,
            target_platform = builder_config.target_platform.WIN,
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "enterprise_companion",
            "debug_static_builder",
            "remoteexec",
            "win",
            "x64",
        ],
    ),
    builderless = True,
    os = os.WINDOWS_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "debug|win (64)",
        short_name = "bld",
    ),
    contact_team_email = "omaha-client-dev@google.com",
    siso_remote_jobs = siso.remote_jobs.LOW_JOBS_FOR_CI,
)

ci.builder(
    name = "win32-enterprise-companion-builder-dbg",
    description_html = linkify("https://source.chromium.org/chromium/chromium/src/+/main:chrome/enterprise_companion/README.md", "Chrome Enterprise Companion App") + " Windows x86 Debug Builder.",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "mb",
            ],
            build_config = builder_config.build_config.DEBUG,
            target_bits = 64,
            target_platform = builder_config.target_platform.WIN,
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "enterprise_companion",
            "debug_static_builder",
            "remoteexec",
            "win",
            "x86",
            "no_symbols",
        ],
    ),
    builderless = True,
    os = os.WINDOWS_DEFAULT,
    free_space = builders.free_space.high,
    console_view_entry = consoles.console_view_entry(
        category = "debug|win (32)",
        short_name = "bld",
    ),
    contact_team_email = "omaha-client-dev@google.com",
    execution_timeout = ci.DEFAULT_EXECUTION_TIMEOUT * 2,
    siso_remote_jobs = siso.remote_jobs.LOW_JOBS_FOR_CI,
)

ci.builder(
    name = "win-enterprise-companion-builder-rel",
    description_html = linkify("https://source.chromium.org/chromium/chromium/src/+/main:chrome/enterprise_companion/README.md", "Chrome Enterprise Companion App") + " Windows x64 Release Builder.",
    builder_spec = builder_config.builder_spec(
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
            target_platform = builder_config.target_platform.WIN,
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "enterprise_companion",
            "release_builder",
            "remoteexec",
            "win",
            "x64",
        ],
    ),
    builderless = True,
    os = os.WINDOWS_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "release|win (64)",
        short_name = "bld",
    ),
    contact_team_email = "omaha-client-dev@google.com",
    siso_remote_jobs = siso.remote_jobs.LOW_JOBS_FOR_CI,
)

ci.builder(
    name = "win32-enterprise-companion-builder-rel",
    description_html = linkify("https://source.chromium.org/chromium/chromium/src/+/main:chrome/enterprise_companion/README.md", "Chrome Enterprise Companion App") + " Windows x86 Release Builder.",
    builder_spec = builder_config.builder_spec(
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
            target_platform = builder_config.target_platform.WIN,
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "enterprise_companion",
            "release_builder",
            "remoteexec",
            "win",
            "x86",
        ],
    ),
    builderless = True,
    os = os.WINDOWS_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "release|win (32)",
        short_name = "bld",
    ),
    contact_team_email = "omaha-client-dev@google.com",
    siso_remote_jobs = siso.remote_jobs.LOW_JOBS_FOR_CI,
)

ci.thin_tester(
    name = "win10-enterprise-companion-tester-dbg",
    description_html = linkify("https://source.chromium.org/chromium/chromium/src/+/main:chrome/enterprise_companion/README.md", "Chrome Enterprise Companion App") + " Windows 10 x64 Debug Tester.",
    triggered_by = ["win-enterprise-companion-builder-dbg"],
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
            build_config = builder_config.build_config.DEBUG,
            target_bits = 64,
            target_platform = builder_config.target_platform.WIN,
        ),
    ),
    console_view_entry = consoles.console_view_entry(
        category = "debug|win (64)",
        short_name = "10",
    ),
    contact_team_email = "omaha-client-dev@google.com",
)

ci.thin_tester(
    name = "win10-32-on-64-enterprise-companion-tester-dbg",
    description_html = linkify("https://source.chromium.org/chromium/chromium/src/+/main:chrome/enterprise_companion/README.md", "Chrome Enterprise Companion App") + " Windows 10 x86-on-x64 Debug Tester.",
    triggered_by = ["win32-enterprise-companion-builder-dbg"],
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
            build_config = builder_config.build_config.DEBUG,
            target_bits = 32,
            target_platform = builder_config.target_platform.WIN,
        ),
    ),
    console_view_entry = consoles.console_view_entry(
        category = "debug|win (32)",
        short_name = "10 (x64)",
    ),
    contact_team_email = "omaha-client-dev@google.com",
)

ci.thin_tester(
    name = "win10-32-on-64-enterprise-companion-tester-rel",
    description_html = linkify("https://source.chromium.org/chromium/chromium/src/+/main:chrome/enterprise_companion/README.md", "Chrome Enterprise Companion App") + " Windows 10 x86-on-x64 Release Tester.",
    triggered_by = ["win32-enterprise-companion-builder-rel"],
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
            target_bits = 32,
            target_platform = builder_config.target_platform.WIN,
        ),
    ),
    console_view_entry = consoles.console_view_entry(
        category = "release|win (32)",
        short_name = "10 (x64)",
    ),
    contact_team_email = "omaha-client-dev@google.com",
)

ci.thin_tester(
    name = "win10-enterprise-companion-tester-rel",
    description_html = linkify("https://source.chromium.org/chromium/chromium/src/+/main:chrome/enterprise_companion/README.md", "Chrome Enterprise Companion App") + " Windows 10 x64 Release Tester.",
    triggered_by = ["win-enterprise-companion-builder-rel"],
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
            target_platform = builder_config.target_platform.WIN,
        ),
    ),
    console_view_entry = consoles.console_view_entry(
        category = "release|win (64)",
        short_name = "10",
    ),
    contact_team_email = "omaha-client-dev@google.com",
)

ci.thin_tester(
    name = "win11-enterprise-companion-tester-rel",
    description_html = linkify("https://source.chromium.org/chromium/chromium/src/+/main:chrome/enterprise_companion/README.md", "Chrome Enterprise Companion App") + " Windows 11 x64 Release Tester.",
    triggered_by = ["win-enterprise-companion-builder-rel"],
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
            target_platform = builder_config.target_platform.WIN,
        ),
    ),
    console_view_entry = consoles.console_view_entry(
        category = "release|win (64)",
        short_name = "11",
    ),
    contact_team_email = "omaha-client-dev@google.com",
)
