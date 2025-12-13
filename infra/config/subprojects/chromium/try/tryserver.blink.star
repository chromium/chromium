# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Definitions of builders in the tryserver.blink builder group."""

load("@chromium-luci//branches.star", "branches")
load("@chromium-luci//builder_config.star", "builder_config")
load("@chromium-luci//builders.star", "cpu", "os")
load("@chromium-luci//consoles.star", "consoles")
load("@chromium-luci//gn_args.star", "gn_args")
load("@chromium-luci//html.star", "linkify")
load("@chromium-luci//targets.star", "targets")
load("@chromium-luci//try.star", "try_")
load("//lib/siso.star", "siso")
load("//lib/try_constants.star", "try_constants")

try_.defaults.set(
    executable = try_constants.DEFAULT_EXECUTABLE,
    builder_group = "tryserver.blink",
    pool = try_constants.DEFAULT_POOL,
    cores = 8,
    contact_team_email = "chrome-blink-engprod@google.com",
    execution_timeout = try_constants.DEFAULT_EXECUTION_TIMEOUT,
    experiments = {
        "chromium_tests.resultdb_module": 100,
    },
    service_account = try_constants.DEFAULT_SERVICE_ACCOUNT,
    siso_project = siso.project.DEFAULT_UNTRUSTED,
    siso_remote_jobs = siso.remote_jobs.LOW_JOBS_FOR_CQ,
)

targets.builder_defaults.set(
    mixins = [
        "chromium-tester-service-account",
    ],
)

consoles.list_view(
    name = "tryserver.blink",
    branch_selector = branches.selector.DESKTOP_BRANCHES,
)

def _mac_rebaseline_builder(*, name, **kwargs):
    kwargs.setdefault("branch_selector", branches.selector.MAC_BRANCHES)
    kwargs.setdefault("builderless", True)
    kwargs.setdefault("cores", None)
    kwargs.setdefault("os", os.MAC_DEFAULT)
    kwargs.setdefault("ssd", True)
    return _rebaseline_builder(name = name, **kwargs)

def _rebaseline_builder(*, name, **kwargs):
    description_html = "Standalone try builder that {} for web platform changes.".format(
        linkify(
            "https://chromium.googlesource.com/chromium/src/+/HEAD/docs/testing/web_test_expectations.md#rebaselining-using-try-jobs",
            "generates new expectations",
        ),
    )
    kwargs.setdefault("description_html", description_html)
    return try_.builder(name = name, **kwargs)

_rebaseline_builder(
    name = "linux-blink-rel",
    branch_selector = branches.selector.LINUX_BRANCHES,
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
    builder_config_settings = builder_config.try_settings(
        retry_failed_shards = False,
    ),
    gn_args = gn_args.config(
        configs = [
            "release_builder",
            "remoteexec",
            "chrome_with_codecs",
            "minimal_symbols",
            "linux",
            "x64",
        ],
    ),
    # Should be kept in sync with v8_linux_blink_rel in tryserver.v8
    targets = targets.bundle(
        targets = [
            "chromium_linux_blink_rel_isolated_scripts",
        ],
        mixins = [
            "linux-jammy",
        ],
        per_test_modifications = {
            "blink_wpt_tests": targets.mixin(
                swarming = targets.swarming(
                    hard_timeout_sec = 2400,
                ),
            ),
            "blink_web_tests": targets.mixin(
                swarming = targets.swarming(
                    hard_timeout_sec = 2400,
                ),
            ),
        },
    ),
    os = os.LINUX_DEFAULT,
    main_list_view = "try",
)

# `linux-wpt-chromium-rel` (tests chrome) is distinct from `linux-blink-rel`
# (tests content shell) to avoid coupling their build configurations.
try_.builder(
    name = "linux-wpt-chromium-rel",
    description_html = "Runs {} against Chrome.".format(
        linkify("https://web-platform-tests.org", "web platform tests"),
    ),
    mirrors = ["ci/linux-wpt-chromium-rel"],
    builder_config_settings = builder_config.try_settings(
        retry_failed_shards = False,
    ),
    gn_args = "ci/linux-wpt-chromium-rel",
    os = os.LINUX_DEFAULT,
    main_list_view = "try",
)

try_.builder(
    name = "linux-blink-tracing-rel",
    mirrors = ["ci/linux-blink-tracing-rel"],
    gn_args = "ci/linux-blink-tracing-rel",
    os = os.LINUX_DEFAULT,
    contact_team_email = "chrome-product-engprod@google.com",
    main_list_view = "try",
)

try_.builder(
    name = "win10-wpt-chromium-rel",
    mirrors = ["ci/win10-wpt-chromium-rel"],
    builder_config_settings = builder_config.try_settings(
        retry_failed_shards = False,
    ),
    gn_args = "ci/win10-wpt-chromium-rel",
    builderless = True,
    os = os.WINDOWS_10,
    main_list_view = "try",
)

_rebaseline_builder(
    name = "win10.20h2-blink-rel",
    branch_selector = branches.selector.WINDOWS_BRANCHES,
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
    builder_config_settings = builder_config.try_settings(
        retry_failed_shards = False,
    ),
    gn_args = gn_args.config(
        configs = [
            "release_builder",
            "remoteexec",
            "chrome_with_codecs",
            "win",
            "x64",
            "minimal_symbols",
        ],
    ),
    targets = targets.bundle(
        targets = [
            "chromium_webkit_isolated_scripts",
        ],
        mixins = [
            targets.mixin(
                swarming = targets.swarming(
                    hard_timeout_sec = 900,
                ),
            ),
            "win10",
        ],
        per_test_modifications = {
            "blink_wpt_tests": targets.mixin(
                swarming = targets.swarming(
                    hard_timeout_sec = 2400,
                ),
            ),
            "blink_web_tests": targets.mixin(
                swarming = targets.swarming(
                    hard_timeout_sec = 2400,
                    shards = 6,
                ),
            ),
        },
    ),
    builderless = True,
    os = os.WINDOWS_ANY,
)

_rebaseline_builder(
    name = "win11-arm64-blink-rel",
    branch_selector = branches.selector.WINDOWS_BRANCHES,
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
    builder_config_settings = builder_config.try_settings(
        retry_failed_shards = False,
    ),
    gn_args = gn_args.config(
        configs = [
            "release_builder",
            "remoteexec",
            "chrome_with_codecs",
            "win",
            "arm64",
            "minimal_symbols",
        ],
    ),
    targets = targets.bundle(
        targets = [
            "chromium_webkit_isolated_scripts",
        ],
        mixins = [
            targets.mixin(
                swarming = targets.swarming(
                    dimensions = {
                        "os": "Windows-11",
                    },
                    hard_timeout_sec = 900,
                ),
            ),
            "arm64",
        ],
        per_test_modifications = {
            "blink_wpt_tests": targets.mixin(
                swarming = targets.swarming(
                    hard_timeout_sec = 2400,
                ),
            ),
            "blink_web_tests": targets.mixin(
                swarming = targets.swarming(
                    hard_timeout_sec = 2400,
                ),
            ),
        },
    ),
    builderless = True,
    os = os.WINDOWS_ANY,
    siso_remote_linking = True,
)

_rebaseline_builder(
    name = "win11-blink-rel",
    branch_selector = branches.selector.WINDOWS_BRANCHES,
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
    builder_config_settings = builder_config.try_settings(
        retry_failed_shards = True,
    ),
    gn_args = gn_args.config(
        configs = [
            "release_builder",
            "remoteexec",
            "chrome_with_codecs",
            "win",
            "x64",
            "minimal_symbols",
        ],
    ),
    targets = targets.bundle(
        targets = [
            "chromium_webkit_isolated_scripts",
        ],
        mixins = [
            targets.mixin(
                swarming = targets.swarming(
                    hard_timeout_sec = 900,
                ),
            ),
            "win11-any",
        ],
        per_test_modifications = {
            "blink_wpt_tests": targets.mixin(
                swarming = targets.swarming(
                    hard_timeout_sec = 2400,
                ),
            ),
            "blink_web_tests": targets.mixin(
                swarming = targets.swarming(
                    hard_timeout_sec = 2400,
                ),
            ),
        },
    ),
    builderless = True,
    os = os.WINDOWS_ANY,
)

_mac_rebaseline_builder(
    name = "mac12.0-blink-rel",
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
    builder_config_settings = builder_config.try_settings(
        retry_failed_shards = False,
    ),
    gn_args = gn_args.config(
        configs = [
            "release_builder",
            "remoteexec",
            "chrome_with_codecs",
            "minimal_symbols",
            "mac",
            "x64",
        ],
    ),
    targets = targets.bundle(
        targets = [
            "chromium_webkit_isolated_scripts",
        ],
        mixins = [
            "mac_12_x64",
        ],
        per_test_modifications = {
            "blink_wpt_tests": targets.mixin(
                swarming = targets.swarming(
                    hard_timeout_sec = 2400,
                ),
            ),
            "blink_web_tests": targets.mixin(
                swarming = targets.swarming(
                    hard_timeout_sec = 2400,
                ),
            ),
        },
    ),
    cpu = cpu.ARM64,
)

_mac_rebaseline_builder(
    name = "mac12.0.arm64-blink-rel",
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
    builder_config_settings = builder_config.try_settings(
        retry_failed_shards = True,
    ),
    gn_args = gn_args.config(
        configs = [
            "release_builder",
            "remoteexec",
            "chrome_with_codecs",
            "mac",
            "arm64",
            "minimal_symbols",
        ],
    ),
    targets = targets.bundle(
        targets = [
            "chromium_webkit_isolated_scripts",
        ],
        mixins = [
            "mac_12_arm64",
        ],
        per_test_modifications = {
            "blink_wpt_tests": targets.mixin(
                swarming = targets.swarming(
                    hard_timeout_sec = 2400,
                ),
            ),
            "blink_web_tests": targets.mixin(
                swarming = targets.swarming(
                    hard_timeout_sec = 2400,
                ),
            ),
        },
    ),
    cores = None,
    cpu = cpu.ARM64,
)

_mac_rebaseline_builder(
    name = "mac13-blink-rel",
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
    builder_config_settings = builder_config.try_settings(
        retry_failed_shards = False,
    ),
    gn_args = gn_args.config(
        configs = [
            "release_builder",
            "remoteexec",
            "chrome_with_codecs",
            "minimal_symbols",
            "mac",
            "x64",
        ],
    ),
    targets = targets.bundle(
        targets = [
            "chromium_webkit_isolated_scripts",
        ],
        mixins = [
            "mac_13_x64",
        ],
    ),
    cores = None,
    cpu = cpu.ARM64,
)

try_.builder(
    name = "mac13-wpt-chromium-rel",
    mirrors = ["ci/mac13-wpt-chromium-rel"],
    builder_config_settings = builder_config.try_settings(
        retry_failed_shards = False,
    ),
    gn_args = "ci/mac13-wpt-chromium-rel",
    builderless = True,
    cores = None,
    os = os.MAC_ANY,
    cpu = cpu.ARM64,
    main_list_view = "try",
)

_mac_rebaseline_builder(
    name = "mac13.arm64-blink-rel",
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
    builder_config_settings = builder_config.try_settings(
        retry_failed_shards = True,
    ),
    gn_args = gn_args.config(
        configs = [
            "release_builder",
            "remoteexec",
            "chrome_with_codecs",
            "mac",
            "arm64",
            "minimal_symbols",
        ],
    ),
    targets = targets.bundle(
        targets = [
            "chromium_webkit_isolated_scripts",
        ],
        mixins = [
            "mac_13_arm64",
        ],
    ),
    cores = None,
    cpu = cpu.ARM64,
)

_mac_rebaseline_builder(
    name = "mac-skia-alt-arm64-blink-rel",
    branch_selector = None,
    mirrors = [
        "ci/mac-arm64-rel",
        "ci/mac-skia-alt-arm64-rel-tests",
    ],
    gn_args = gn_args.config(
        # TODO(crbug.com/40937352): Currently we override the gn args instead
        # of using mac-arm64-rel's gn args. Ideally Graphite should be tested
        # with dcheck on. However, mac-arm64-rel's gn args has dcheck off so
        # we override gn args here to enable dcheck via "release_try_builder".
        # In future, we should add a dedicated CI builder with dcheck enabled
        # and mirror it here.
        configs = [
            "release_try_builder",
            "remoteexec",
            "chrome_with_codecs",
            "mac",
            "arm64",
            "minimal_symbols",
        ],
    ),
    cores = None,
    cpu = cpu.ARM64,
    contact_team_email = "chrome-skia-graphite@google.com",
    main_list_view = "try",
)

_mac_rebaseline_builder(
    name = "mac14-blink-rel",
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
    builder_config_settings = builder_config.try_settings(
        retry_failed_shards = False,
    ),
    gn_args = gn_args.config(
        configs = [
            "release_builder",
            "remoteexec",
            "chrome_with_codecs",
            "minimal_symbols",
            "mac",
            "x64",
        ],
    ),
    targets = targets.bundle(
        targets = [
            "chromium_webkit_isolated_scripts",
        ],
        mixins = [
            "mac_14_x64",
        ],
    ),
    cpu = cpu.ARM64,
)

_mac_rebaseline_builder(
    name = "mac14.arm64-blink-rel",
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
    builder_config_settings = builder_config.try_settings(
        retry_failed_shards = True,
    ),
    gn_args = gn_args.config(
        configs = [
            "release_builder",
            "remoteexec",
            "chrome_with_codecs",
            "mac",
            "arm64",
            "minimal_symbols",
        ],
    ),
    targets = targets.bundle(
        targets = [
            "chromium_webkit_isolated_scripts",
        ],
        mixins = [
            "mac_14_arm64",
        ],
    ),
    cpu = cpu.ARM64,
)

_mac_rebaseline_builder(
    name = "mac15-blink-rel",
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
    builder_config_settings = builder_config.try_settings(
        retry_failed_shards = False,
    ),
    gn_args = gn_args.config(
        configs = [
            "release_builder",
            "remoteexec",
            "chrome_with_codecs",
            "minimal_symbols",
            "mac",
            "x64",
        ],
    ),
    targets = targets.bundle(
        targets = [
            "chromium_webkit_isolated_scripts",
        ],
        mixins = [
            "mac_15_x64",
        ],
    ),
    cpu = cpu.ARM64,
)

_mac_rebaseline_builder(
    name = "mac15.arm64-blink-rel",
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
    builder_config_settings = builder_config.try_settings(
        retry_failed_shards = True,
    ),
    gn_args = gn_args.config(
        configs = [
            "release_builder",
            "remoteexec",
            "chrome_with_codecs",
            "mac",
            "arm64",
            "minimal_symbols",
        ],
    ),
    targets = targets.bundle(
        targets = [
            "chromium_webkit_isolated_scripts",
        ],
        mixins = [
            "mac_15_arm64",
        ],
    ),
    cpu = cpu.ARM64,
)

_rebaseline_builder(
    name = "android-15-chrome-blink-rel",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = ["android"],
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_arch = builder_config.target_arch.INTEL,
            target_bits = 64,
            target_platform = builder_config.target_platform.ANDROID,
        ),
        android_config = builder_config.android_config(
            config = "base_config",
        ),
    ),
    builder_config_settings = builder_config.try_settings(
        retry_failed_shards = False,
    ),
    gn_args = gn_args.config(
        configs = [
            "android_builder",
            "release_builder",
            "remoteexec",
            "minimal_symbols",
            "x64",
            "strip_debug_info",
            "android_fastbuild",
        ],
    ),
    targets = targets.bundle(
        targets = [
            "android_chrome_wpt_tests",
            "android_webdriver_wpt_tests",
        ],
        mixins = [
            "has_native_resultdb_integration",
            "15-x64-emulator",
            "emulator-8-cores",
            "linux-jammy",
            "x86-64",
        ],
        per_test_modifications = {
            "android_chrome_wpt_tests": targets.mixin(
                swarming = targets.swarming(
                    shards = 4,
                ),
            ),
            "android_webdriver_wpt_tests": targets.mixin(
                swarming = targets.swarming(
                    shards = 15,
                ),
            ),
        },
    ),
    targets_settings = targets.settings(
        os_type = targets.os_type.ANDROID,
    ),
    builderless = True,
    os = os.LINUX_DEFAULT,
    main_list_view = "try",
)
