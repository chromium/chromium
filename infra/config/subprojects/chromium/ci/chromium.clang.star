# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Definitions of builders in the chromium.clang builder group."""

load("@chromium-luci//args.star", "args")
load("@chromium-luci//branches.star", "branches")
load("@chromium-luci//builder_config.star", "builder_config")
load("@chromium-luci//builder_health_indicators.star", "health_spec")
load("@chromium-luci//builders.star", "builders", "cpu", "os")
load("@chromium-luci//ci.star", "ci")
load("@chromium-luci//consoles.star", "consoles")
load("@chromium-luci//gn_args.star", "gn_args")
load("@chromium-luci//html.star", "linkify_builder")
load("@chromium-luci//targets.star", "targets")
load("//lib/ci_constants.star", "ci_constants")
load("//lib/gardener_rotations.star", "gardener_rotations")
load("//lib/siso.star", "siso")
load("//lib/xcode.star", "xcode")

ci.defaults.set(
    executable = ci_constants.DEFAULT_EXECUTABLE,
    builder_group = "chromium.clang",
    pool = ci_constants.DEFAULT_POOL,
    builderless = True,
    cores = 32,
    os = os.LINUX_DEFAULT,
    gardener_rotations = gardener_rotations.CHROMIUM_CLANG,
    # Because these run ToT Clang, reclient is not used.
    # Naturally the runtime will be ~4-8h on average for basic builds.
    # Complex (e.g. sanitizer), CFI builds can take much longer.
    execution_timeout = 16 * time.hour,
    experiments = {
        "chromium_tests.resultdb_module": 100,
    },
    health_spec = health_spec.modified_default({
        "Unhealthy": health_spec.unhealthy_thresholds(
            fail_rate = struct(),
        ),
        "Low Value": health_spec.low_value_thresholds(
            fail_rate = struct(),
        ),
    }),
    properties = {
        "perf_dashboard_machine_group": "ChromiumClang",
        "$build/chromium": {
            "fail_build_on_clang_warnings": True,
        },
    },
    service_account = ci_constants.DEFAULT_SERVICE_ACCOUNT,
    shadow_service_account = ci_constants.DEFAULT_SHADOW_SERVICE_ACCOUNT,
    siso_project = siso.project.DEFAULT_TRUSTED,
)

targets.builder_defaults.set(
    mixins = [
        "chromium-tester-service-account",
    ],
)

consoles.console_view(
    name = "chromium.clang",
    ordering = {
        None: [
            "ToT Linux",
            "ToT Android",
            "ToT Mac",
            "ToT Windows",
            "ToT Code Coverage",
            "Rust ToT",
        ],
        "ToT Linux": consoles.ordering(
            short_names = ["rel", "ofi", "dbg", "asn", "fuz", "msn", "tsn"],
        ),
        "ToT Android": consoles.ordering(short_names = ["rel", "dbg", "x64"]),
        "ToT Mac": consoles.ordering(short_names = ["rel", "ofi", "dbg"]),
        "ToT Windows": consoles.ordering(
            categories = ["x64"],
            short_names = ["rel", "ofi"],
        ),
        "ToT Windows|x64": consoles.ordering(short_names = ["rel"]),
        "CFI|Win": consoles.ordering(short_names = ["x86", "x64"]),
        "iOS": ["public"],
        "iOS|public": consoles.ordering(short_names = ["sim", "dev"]),
    },
)

[branches.console_view_entry(
    console_view = "chromium.clang",
    builder = "chrome:ci/{}".format(name),
    category = category,
    short_name = short_name,
) for name, category, short_name in (
    ("ToTLinuxOfficial", "ToT Linux", "ofi"),
    ("ToTMacOfficial", "ToT Mac", "ofi"),
    ("ToTWinOfficial", "ToT Windows", "ofi"),
    ("ToTWinOfficial64", "ToT Windows|x64", "ofi"),
    ("clang-tot-device", "iOS|internal", "dev"),
)]

def tot_mac_builder(*, name, is_rust = False, **kwargs):
    if "gn_args" in kwargs:
        kwargs["gn_args"].configs.append("mac")
    desc_tool = "Rust" if is_rust else "Clang"
    return ci.builder(
        name = name,
        os = os.MAC_DEFAULT,
        ssd = True,
        cores = None,
        cpu = cpu.ARM64,
        properties = {
            # The Chromium build doesn't need system Xcode, but the ToT
            # bots also build clang and llvm and that build does need system
            # Xcode.
            "xcode_build_version": "14c18",
        },
        contact_team_email = "lexan@google.com",
        description_html = "Builder that builds ToT " + desc_tool + " and uses it to build Chromium",
        **kwargs
    )

def clang_tot_linux_builder(short_name, category = "ToT Linux", **kwargs):
    if "gn_args" in kwargs:
        kwargs["gn_args"].configs.append("linux")
    ci.builder(
        console_view_entry = consoles.console_view_entry(
            category = category,
            short_name = short_name,
        ),
        notifies = [luci.notifier(
            name = "ToT Linux notifier",
            notify_emails = ["thomasanderson@chromium.org"],
            on_new_status = ["FAILURE"],
        )],
        contact_team_email = "lexan@google.com",
        **kwargs
    )

ci.builder(
    name = "CFI Linux CF",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(config = "chromium"),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = ["mb"],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
            target_platform = builder_config.target_platform.LINUX,
        ),
        clusterfuzz_archive = builder_config.clusterfuzz_archive(
            archive_name_prefix = "cfi",
            gs_acl = "public-read",
            gs_bucket = "chromium-browser-cfi",
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "cfi_full",
            "cfi_icall",
            "cfi_diag",
            "cfi_recover",
            "thin_lto",
            "release_builder",
            "remoteexec",
            "linux",
            "x64",
        ],
    ),
    targets = targets.bundle(
        additional_compile_targets = [
            "chromium_builder_asan",
        ],
    ),
    console_view_entry = consoles.console_view_entry(
        category = "CFI|Linux",
        short_name = "CF",
    ),
    contact_team_email = "lexan@google.com",
    notifies = ["CFI Linux"],
)

ci.builder(
    name = "CFI Linux ToT",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = ["clang_tot"],
        ),
        chromium_config = builder_config.chromium_config(
            config = "clang_tot_linux",
            apply_configs = ["mb"],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
            target_platform = builder_config.target_platform.LINUX,
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "clang_tot_gn",
            "no_treat_warnings_as_errors",
            "cfi_full",
            "cfi_icall",
            "cfi_diag",
            "thin_lto",
            "release_builder",
            "dcheck_always_on",
            "linux",
            "x64",
        ],
    ),
    targets = targets.bundle(
        targets = [
            "clang_tot_gtests",
        ],
        mixins = [
            "linux-jammy",
        ],
    ),
    console_view_entry = consoles.console_view_entry(
        category = "CFI|Linux",
        short_name = "ToT",
    ),
    contact_team_email = "lexan@google.com",
    notifies = ["CFI Linux"],
)

ci.builder(
    name = "CrWinAsan",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = ["clang_tot"],
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium_win_clang_asan_tot",
            apply_configs = ["mb"],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 32,
            target_platform = builder_config.target_platform.WIN,
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "asan",
            "clang_tot_gn",
            "no_treat_warnings_as_errors",
            "fuzzer",
            "release_builder",
            "v8_heap",
            "minimal_symbols",
            "win",
            "x64",
        ],
    ),
    targets = targets.bundle(
        targets = [
            "clang_tot_gtests",
        ],
        additional_compile_targets = [
            "chromium_builder_asan",
        ],
        mixins = [
            "x86-64",
            "win10",
        ],
    ),
    cores = "64",
    os = os.WINDOWS_DEFAULT,
    free_space = builders.free_space.high,
    console_view_entry = consoles.console_view_entry(
        category = "ToT Windows|Asan",
        short_name = "asn",
    ),
    contact_team_email = "lexan@google.com",
)

ci.builder(
    name = "CrWinAsan(dll)",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = ["clang_tot"],
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium_win_clang_asan_tot",
            apply_configs = ["mb"],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 32,
            target_platform = builder_config.target_platform.WIN,
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "asan",
            "clang_tot_gn",
            "no_treat_warnings_as_errors",
            "shared",
            "release",
            "v8_heap",
            "minimal_symbols",
            "win",
            "x64",
        ],
    ),
    targets = targets.bundle(
        targets = [
            "clang_tot_gtests",
        ],
        additional_compile_targets = [
            "chromium_builder_asan",
        ],
        mixins = [
            "x86-64",
            "win10",
        ],
    ),
    cores = "64",
    os = os.WINDOWS_DEFAULT,
    free_space = builders.free_space.high,
    console_view_entry = consoles.console_view_entry(
        category = "ToT Windows|Asan",
        short_name = "dll",
    ),
    contact_team_email = "lexan@google.com",
)

ci.builder(
    name = "ToTAndroid",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "clang_tot",
                "android",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "clang_tot_android",
            apply_configs = ["mb"],
            build_config = builder_config.build_config.RELEASE,
            target_arch = builder_config.target_arch.ARM,
            target_bits = 32,
            target_platform = builder_config.target_platform.ANDROID,
        ),
        android_config = builder_config.android_config(config = "base_config"),
    ),
    gn_args = gn_args.config(
        configs = [
            "android_builder",
            "android_with_static_analysis",
            "release_builder",
            "minimal_symbols",
            "strip_debug_info",
            "clang_tot_gn",
            "no_treat_warnings_as_errors",
            "arm",
            "remoteexec",
        ],
    ),
    targets = targets.bundle(
        targets = [
            "clang_tot_gtests",
        ],
        additional_compile_targets = [
            "all",
        ],
        mixins = [
            "chromium_pixel_2_q",
            "has_native_resultdb_integration",
        ],
        per_test_modifications = {
            "base_unittests": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.device_10.tot.base_unittests.filter",
                ],
            ),
        },
    ),
    targets_settings = targets.settings(
        os_type = targets.os_type.ANDROID,
    ),
    console_view_entry = consoles.console_view_entry(
        category = "ToT Android",
        short_name = "rel",
    ),
    contact_team_email = "lexan@google.com",
)

ci.builder(
    name = "ToTAndroid (dbg)",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "clang_tot",
                "android",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "clang_tot_android",
            apply_configs = ["mb"],
            build_config = builder_config.build_config.DEBUG,
            target_arch = builder_config.target_arch.ARM,
            target_bits = 32,
            target_platform = builder_config.target_platform.ANDROID,
        ),
        android_config = builder_config.android_config(config = "base_config"),
    ),
    gn_args = gn_args.config(
        configs = [
            "android_builder_without_codecs",
            "android_with_static_analysis",
            "clang_tot_gn",
            "no_treat_warnings_as_errors",
            "shared",
            "debug",
            "arm",
            "remoteexec",
        ],
    ),
    targets = targets.bundle(
        additional_compile_targets = [
            "all",
        ],
    ),
    console_view_entry = consoles.console_view_entry(
        category = "ToT Android",
        short_name = "dbg",
    ),
    contact_team_email = "lexan@google.com",
)

ci.builder(
    name = "ToTAndroid x64",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "clang_tot",
                "android",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "clang_tot_android",
            apply_configs = ["mb"],
            build_config = builder_config.build_config.RELEASE,
            target_arch = builder_config.target_arch.INTEL,
            target_bits = 64,
            target_platform = builder_config.target_platform.ANDROID,
        ),
        android_config = builder_config.android_config(config = "base_config"),
    ),
    gn_args = gn_args.config(
        configs = [
            "android_builder_without_codecs",
            "android_with_static_analysis",
            "clang_tot_gn",
            "no_treat_warnings_as_errors",
            "shared",
            "release",
            "x64",
            "dcheck_always_on",
            "remoteexec",
        ],
    ),
    # TODO(crbug.com/41368235): Re-enable tests once there are devices to run them on.
    targets = targets.bundle(
        additional_compile_targets = [
            "all",
        ],
        mixins = [
            targets.mixin(
                swarming = targets.swarming(
                    dimensions = {
                        "device_type": "coho",
                        "os": "Android",
                    },
                ),
            ),
        ],
    ),
    console_view_entry = consoles.console_view_entry(
        category = "ToT Android",
        short_name = "x64",
    ),
    contact_team_email = "lexan@google.com",
)

ci.builder(
    name = "ToTAndroid x86",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "clang_tot",
                "android",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "clang_tot_android",
            apply_configs = ["mb"],
            build_config = builder_config.build_config.RELEASE,
            target_arch = builder_config.target_arch.INTEL,
            target_bits = 32,
            target_platform = builder_config.target_platform.ANDROID,
        ),
        android_config = builder_config.android_config(config = "base_config"),
    ),
    gn_args = gn_args.config(
        configs = [
            "android_builder_without_codecs",
            "android_with_static_analysis",
            "clang_tot_gn",
            "no_treat_warnings_as_errors",
            "shared",
            "release",
            "x86",
            "dcheck_always_on",
            "remoteexec",
        ],
    ),
    targets = targets.bundle(
        additional_compile_targets = [
            "all",
        ],
    ),
    console_view_entry = consoles.console_view_entry(
        category = "ToT Android",
        short_name = "x86",
    ),
    contact_team_email = "lexan@google.com",
)

ci.builder(
    name = "ToTAndroidCoverage x86",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "clang_tot",
                "android",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "clang_tot_android",
            apply_configs = ["mb"],
            build_config = builder_config.build_config.RELEASE,
            target_arch = builder_config.target_arch.INTEL,
            target_bits = 32,
            target_platform = builder_config.target_platform.ANDROID,
        ),
        android_config = builder_config.android_config(config = "base_config"),
    ),
    gn_args = gn_args.config(
        configs = [
            "android_builder_without_codecs",
            "android_with_static_analysis",
            "clang_tot_gn",
            "no_treat_warnings_as_errors",
            "shared",
            "release",
            "x86",
            "dcheck_always_on",
            "use_clang_coverage",
            "remoteexec",
        ],
    ),
    targets = targets.bundle(
        additional_compile_targets = [
            "all",
        ],
    ),
    console_view_entry = consoles.console_view_entry(
        category = "ToT Code Coverage",
        short_name = "and",
    ),
    contact_team_email = "lexan@google.com",
)

ci.builder(
    name = "ToTAndroid64",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "clang_tot",
                "android",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "clang_tot_android",
            apply_configs = ["mb"],
            build_config = builder_config.build_config.RELEASE,
            target_arch = builder_config.target_arch.ARM,
            target_bits = 64,
            target_platform = builder_config.target_platform.ANDROID,
        ),
        android_config = builder_config.android_config(config = "base_config"),
    ),
    gn_args = gn_args.config(
        configs = [
            "android_builder_without_codecs",
            "android_with_static_analysis",
            "clang_tot_gn",
            "no_treat_warnings_as_errors",
            "release",
            "arm64",
            "remoteexec",
        ],
    ),
    targets = targets.bundle(
        targets = [
            "chrome_sizes_android",
        ],
        additional_compile_targets = [
            "all",
        ],
    ),
    console_view_entry = consoles.console_view_entry(
        category = "ToT Android",
        short_name = "a64",
    ),
    contact_team_email = "lexan@google.com",
)

ci.builder(
    name = "ToTAndroidOfficial",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "checkout_pgo_profiles",
                "clang_tot",
                "android",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "clang_tot_android",
            apply_configs = ["mb"],
            build_config = builder_config.build_config.RELEASE,
            target_arch = builder_config.target_arch.ARM,
            target_bits = 32,
            target_platform = builder_config.target_platform.ANDROID,
        ),
        android_config = builder_config.android_config(config = "base_config"),
    ),
    gn_args = gn_args.config(
        configs = [
            "android_builder",
            "release_builder",
            "minimal_symbols",
            "official_optimize",
            "clang_tot_gn",
            "no_treat_warnings_as_errors",
            "arm64",
            "remoteexec",
        ],
    ),
    targets = targets.bundle(
        targets = [
            "chrome_sizes_android",
        ],
        additional_compile_targets = [
            "all",
        ],
    ),
    console_view_entry = consoles.console_view_entry(
        category = "ToT Android",
        short_name = "off",
    ),
    contact_team_email = "lexan@google.com",
)

ci.builder(
    name = "ToTChromeOS",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "clang_tot",
                "chromeos",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "clang_tot_chromeos",
            apply_configs = ["mb"],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
            target_platform = builder_config.target_platform.CHROMEOS,
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "chromeos_on_linux",
            "release",
            "clang_tot_gn",
            "no_treat_warnings_as_errors",
            "x64",
            "remoteexec",
        ],
    ),
    targets = targets.bundle(
        additional_compile_targets = [
            "all",
        ],
    ),
    console_view_entry = consoles.console_view_entry(
        category = "ToT ChromeOS",
        short_name = "rel",
    ),
    contact_team_email = "lexan@google.com",
)

ci.builder(
    name = "ToTChromeOS (dbg)",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "clang_tot",
                "chromeos",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "clang_tot_chromeos",
            apply_configs = ["mb"],
            build_config = builder_config.build_config.DEBUG,
            target_bits = 64,
            target_platform = builder_config.target_platform.CHROMEOS,
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "chromeos_on_linux",
            "debug",
            "clang_tot_gn",
            "no_treat_warnings_as_errors",
            "x64",
            "remoteexec",
        ],
    ),
    targets = targets.bundle(
        additional_compile_targets = [
            "all",
        ],
    ),
    console_view_entry = consoles.console_view_entry(
        category = "ToT ChromeOS",
        short_name = "dbg",
    ),
    contact_team_email = "lexan@google.com",
)

ci.builder(
    name = "ToTFuchsia x64",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "clang_tot",
                "fuchsia_x64",
                "fuchsia_no_hooks",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "clang_tot_fuchsia",
            apply_configs = ["mb"],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
            target_platform = builder_config.target_platform.FUCHSIA,
        ),
        run_tests_serially = True,
    ),
    gn_args = gn_args.config(
        configs = [
            "fuchsia",
            "release_builder",
            "clang_tot_gn",
            "no_treat_warnings_as_errors",
            "cast_receiver_size_optimized",
            "x64",
            "dcheck_always_on",
            "remoteexec",
        ],
    ),
    targets = targets.bundle(
        targets = [
            "clang_tot_gtests",
            targets.bundle(
                targets = "fuchsia_isolated_scripts",
                mixins = "expand-as-isolated-script",
            ),
        ],
        additional_compile_targets = [
            "all",
        ],
        mixins = [
            targets.mixin(
                swarming = targets.swarming(
                    dimensions = {
                        "kvm": "1",
                    },
                ),
            ),
            "linux-jammy",
        ],
        per_test_modifications = {
            "blink_web_tests": targets.mixin(
                args = [
                    "--platform=fuchsia",
                ],
                swarming = targets.swarming(
                    shards = 1,
                ),
            ),
            "blink_wpt_tests": targets.mixin(
                args = [
                    "--platform=fuchsia",
                ],
                swarming = targets.swarming(
                    shards = 1,
                ),
            ),
            "chrome_wpt_tests": targets.remove(
                reason = "Wptrunner does not work on Fuchsia",
            ),
            "headless_shell_wpt_tests": targets.remove(
                reason = "Wptrunner does not work on Fuchsia",
            ),
        },
    ),
    targets_settings = targets.settings(
        browser_config = targets.browser_config.WEB_ENGINE_SHELL,
        os_type = targets.os_type.FUCHSIA,
    ),
    console_view_entry = [
        consoles.console_view_entry(
            category = "ToT Fuchsia",
            short_name = "x64",
        ),
    ],
    contact_team_email = "lexan@google.com",
)

ci.builder(
    name = "ToTFuchsiaOfficial arm64",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "checkout_pgo_profiles",
                "clang_tot",
                "fuchsia_arm64",
                "fuchsia_arm64_host",
                "fuchsia_no_hooks",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "clang_tot_fuchsia",
            apply_configs = ["mb"],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
            target_platform = builder_config.target_platform.FUCHSIA,
        ),
        run_tests_serially = True,
    ),
    gn_args = gn_args.config(
        configs = [
            "official_optimize",
            "fuchsia",
            "arm64",
            "clang_tot_gn",
            "no_treat_warnings_as_errors",
            "static",
            "arm64_host",
            "cast_receiver_size_optimized",
            "dcheck_always_on",
            "remoteexec",
        ],
    ),
    targets = targets.bundle(
        targets = [
            "clang_tot_gtests",
            targets.bundle(
                targets = "fuchsia_arm64_isolated_scripts",
                mixins = "expand-as-isolated-script",
            ),
        ],
        mixins = [
            "arm64",
            "docker",
            "linux-jammy",
        ],
    ),
    console_view_entry = [
        consoles.console_view_entry(
            category = "ToT Fuchsia",
            short_name = "off",
        ),
    ],
    contact_team_email = "lexan@google.com",
)

clang_tot_linux_builder(
    name = "ToTLinux",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = ["clang_tot"],
        ),
        chromium_config = builder_config.chromium_config(
            config = "clang_tot_linux",
            apply_configs = ["mb"],
            build_config = builder_config.build_config.RELEASE,
            target_arch = builder_config.target_arch.INTEL,
            target_bits = 64,
            target_platform = builder_config.target_platform.LINUX,
        ),
    ),
    gn_args = gn_args.config(
        # Enable debug info, as on official builders, to catch issues with
        # optimized debug info.
        configs = [
            "clang_tot_gn",
            "no_treat_warnings_as_errors",
            "full_symbols",
            "shared",
            "release",
            "x64",
            "remoteexec",
        ],
    ),
    targets = targets.bundle(
        targets = [
            "clang_tot_gtests",
        ],
        additional_compile_targets = [
            "all",
        ],
        mixins = [
            "linux-jammy",
        ],
    ),
    short_name = "rel",
)

clang_tot_linux_builder(
    name = "ToTLinux (dbg)",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = ["clang_tot"],
        ),
        chromium_config = builder_config.chromium_config(
            config = "clang_tot_linux",
            apply_configs = ["mb"],
            build_config = builder_config.build_config.DEBUG,
            target_arch = builder_config.target_arch.INTEL,
            target_bits = 64,
            target_platform = builder_config.target_platform.LINUX,
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "clang_tot_gn",
            "no_treat_warnings_as_errors",
            "shared",
            "debug",
            "x64",
            "remoteexec",
        ],
    ),
    targets = targets.bundle(
        targets = [
        ],
        additional_compile_targets = [
            "all",
        ],
        mixins = [
            "linux-jammy",
        ],
    ),
    short_name = "dbg",
    siso_remote_jobs = siso.remote_jobs.HIGH_JOBS_FOR_CI,
)

clang_tot_linux_builder(
    name = "ToTLinuxASan",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = ["clang_tot"],
        ),
        chromium_config = builder_config.chromium_config(
            config = "clang_tot_linux_asan",
            apply_configs = ["mb"],
            build_config = builder_config.build_config.RELEASE,
            target_arch = builder_config.target_arch.INTEL,
            target_bits = 64,
            target_platform = builder_config.target_platform.LINUX,
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "clang_tot_gn",
            "no_treat_warnings_as_errors",
            "asan",
            "lsan",
            "release_builder",
            "x64",
            "remoteexec",
        ],
    ),
    targets = targets.bundle(
        targets = [
            "clang_tot_gtests",
        ],
        additional_compile_targets = [
            "all",
        ],
        mixins = [
            targets.mixin(
                args = [
                    "--test-launcher-print-test-stdio=always",
                ],
            ),
            "linux-jammy",
        ],
    ),
    short_name = "asn",
)

clang_tot_linux_builder(
    name = "ToTLinuxASanLibfuzzer",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = ["clang_tot"],
        ),
        chromium_config = builder_config.chromium_config(
            config = "clang_tot_linux_asan",
            apply_configs = ["mb"],
            build_config = builder_config.build_config.RELEASE,
            target_arch = builder_config.target_arch.INTEL,
            target_bits = 64,
            target_platform = builder_config.target_platform.LINUX,
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "libfuzzer",
            "asan",
            "remoteexec",
            "clang_tot_gn",
            "no_treat_warnings_as_errors",
            "shared",
            "release",
            "chromeos_codecs",
            "pdf_xfa",
            "optimize_for_fuzzing",
            "mojo_fuzzer",
            "x64",
        ],
    ),
    targets = targets.bundle(
        targets = [
        ],
        additional_compile_targets = [
            "all",
        ],
        mixins = [
            "linux-jammy",
        ],
    ),
    # Requires a large disk, so has a machine specifically devoted to it
    builderless = False,
    short_name = "fuz",
)

clang_tot_linux_builder(
    name = "ToTLinuxCoverage",
    executable = "recipe:chromium_clang_coverage_tot",
    gn_args = gn_args.config(
        configs = [
            "clang_tot_gn",
            "no_treat_warnings_as_errors",
            "use_clang_coverage",
            "minimal_symbols",
            "release",
            "x64",
            "remoteexec",
        ],
    ),
    category = "ToT Code Coverage",
    short_name = "linux",
)

clang_tot_linux_builder(
    name = "ToTLinuxMSan",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = ["clang_tot"],
        ),
        chromium_config = builder_config.chromium_config(
            config = "clang_tot_linux",
            apply_configs = ["mb"],
            build_config = builder_config.build_config.RELEASE,
            target_arch = builder_config.target_arch.INTEL,
            target_bits = 64,
            target_platform = builder_config.target_platform.LINUX,
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "clang_tot_gn",
            "no_treat_warnings_as_errors",
            "msan",
            "release",
            "x64",
            # TODO(crbug.com/450862240) enable "remoteexec" here
        ],
    ),
    targets = targets.bundle(
        targets = [
            "clang_tot_gtests",
        ],
        additional_compile_targets = [
            "all",
        ],
        mixins = [
            "linux-jammy",
        ],
    ),
    builderless = False,
    ssd = True,
    short_name = "msn",
)

clang_tot_linux_builder(
    name = "ToTLinuxPGO",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = ["clang_tot"],
        ),
        chromium_config = builder_config.chromium_config(
            config = "clang_tot_linux",
            apply_configs = ["mb"],
            build_config = builder_config.build_config.RELEASE,
            target_arch = builder_config.target_arch.INTEL,
            target_bits = 64,
            target_platform = builder_config.target_platform.LINUX,
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "clang_tot_gn",
            "no_treat_warnings_as_errors",
            "official_optimize",
            "no_symbols",
            "pgo_phase_1",
            "x64",
            "remoteexec",
        ],
    ),
    targets = targets.bundle(
        targets = [
            "clang_tot_gtests",
        ],
        additional_compile_targets = [
            "all",
        ],
        mixins = [
            "linux-jammy",
        ],
    ),
    short_name = "pgo",
)

clang_tot_linux_builder(
    name = "ToTLinuxTSan",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = ["clang_tot"],
        ),
        chromium_config = builder_config.chromium_config(
            config = "clang_tot_linux",
            apply_configs = ["mb"],
            build_config = builder_config.build_config.RELEASE,
            target_arch = builder_config.target_arch.INTEL,
            target_bits = 64,
            target_platform = builder_config.target_platform.LINUX,
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "clang_tot_gn",
            "no_treat_warnings_as_errors",
            "tsan",
            "release",
            "x64",
            "remoteexec",
        ],
    ),
    targets = targets.bundle(
        targets = [
            "clang_tot_gtests",
        ],
        additional_compile_targets = [
            "all",
        ],
        mixins = [
            "linux-jammy",
        ],
    ),
    short_name = "tsn",
)

clang_tot_linux_builder(
    name = "ToTLinuxUBSanVptr",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = ["clang_tot"],
        ),
        chromium_config = builder_config.chromium_config(
            config = "clang_tot_linux_ubsan_vptr",
            apply_configs = ["mb"],
            build_config = builder_config.build_config.RELEASE,
            target_arch = builder_config.target_arch.INTEL,
            target_bits = 64,
            target_platform = builder_config.target_platform.LINUX,
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "clang_tot_gn",
            "no_treat_warnings_as_errors",
            "ubsan_vptr_no_recover_hack",
            "release_builder",
            "x64",
            "remoteexec",
        ],
    ),
    targets = targets.bundle(
        targets = [
            "clang_tot_gtests",
        ],
        additional_compile_targets = [
            "all",
        ],
        mixins = [
            "linux-jammy",
        ],
    ),
    short_name = "usn",
)

ci.builder(
    name = "ToTWin",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = ["clang_tot"],
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium_win_clang_tot",
            apply_configs = ["mb"],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 32,
            target_platform = builder_config.target_platform.WIN,
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "clang_tot_gn",
            "no_treat_warnings_as_errors",
            "minimal_symbols",
            "release_builder",
            "x86",
            "win",
        ],
    ),
    targets = targets.bundle(
        targets = [
            "clang_tot_gtests",
            "win_specific_isolated_scripts_and_sizes",
        ],
        additional_compile_targets = [
            "all",
        ],
        mixins = [
            "win10",
        ],
    ),
    cores = "64",
    os = os.WINDOWS_DEFAULT,
    free_space = builders.free_space.high,
    console_view_entry = consoles.console_view_entry(
        category = "ToT Windows",
        short_name = "rel",
    ),
    contact_team_email = "lexan@google.com",
    # Clang ToT Win compiles get timeouts often.
    siso_configs = [
        "builder",
        "no-remote-timeout",
    ],
)

ci.builder(
    name = "ToTWin(dbg)",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = ["clang_tot"],
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium_win_clang_tot",
            apply_configs = ["mb"],
            build_config = builder_config.build_config.DEBUG,
            target_bits = 32,
            target_platform = builder_config.target_platform.WIN,
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "clang_tot_gn",
            "no_treat_warnings_as_errors",
            "shared",
            "debug",
            "x86",
            "win",
        ],
    ),
    targets = targets.bundle(
        targets = [
            "clang_tot_gtests",
            # Doesn't run win_specific_isolated_scripts because the mini
            # installer isn't hooked up in 32-bit debug builds.
        ],
        additional_compile_targets = [
            "all",
        ],
        mixins = [
            "win10",
        ],
    ),
    cores = "64",
    os = os.WINDOWS_DEFAULT,
    free_space = builders.free_space.high,
    console_view_entry = consoles.console_view_entry(
        category = "ToT Windows",
        short_name = "dbg",
    ),
    contact_team_email = "lexan@google.com",
)

ci.builder(
    name = "ToTWin(dll)",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = ["clang_tot"],
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium_win_clang_tot",
            apply_configs = ["mb"],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 32,
            target_platform = builder_config.target_platform.WIN,
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "clang_tot_gn",
            "no_treat_warnings_as_errors",
            "minimal_symbols",
            "shared",
            "release",
            "x86",
            "dcheck_always_on",
            "win",
        ],
    ),
    targets = targets.bundle(
        targets = [
            "clang_tot_gtests",
            "win_specific_isolated_scripts_and_sizes",
        ],
        additional_compile_targets = [
            "all",
        ],
        mixins = [
            "win10",
        ],
    ),
    cores = "64",
    os = os.WINDOWS_DEFAULT,
    free_space = builders.free_space.high,
    console_view_entry = consoles.console_view_entry(
        category = "ToT Windows",
        short_name = "dll",
    ),
    contact_team_email = "lexan@google.com",
)

ci.builder(
    name = "ToTWin64",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = ["clang_tot"],
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium_win_clang_tot",
            apply_configs = ["mb"],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
            target_platform = builder_config.target_platform.WIN,
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "clang_tot_gn",
            "no_treat_warnings_as_errors",
            "minimal_symbols",
            "release_builder",
            "win",
            "x64",
        ],
    ),
    targets = targets.bundle(
        targets = [
            "clang_tot_gtests",
            "win_specific_isolated_scripts_and_sizes",
        ],
        additional_compile_targets = [
            "all",
        ],
        mixins = [
            "x86-64",
            "win10",
        ],
    ),
    cores = "64",
    os = os.WINDOWS_DEFAULT,
    free_space = builders.free_space.high,
    console_view_entry = consoles.console_view_entry(
        category = "ToT Windows|x64",
        short_name = "rel",
    ),
    contact_team_email = "lexan@google.com",
)

ci.builder(
    name = "ToTWin64(dbg)",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = ["clang_tot"],
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium_win_clang_tot",
            apply_configs = ["mb"],
            build_config = builder_config.build_config.DEBUG,
            target_bits = 64,
            target_platform = builder_config.target_platform.WIN,
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "clang_tot_gn",
            "no_treat_warnings_as_errors",
            "shared",
            "debug",
            "win",
            "x64",
        ],
    ),
    targets = targets.bundle(
        targets = [
            "clang_tot_gtests",
            "win_specific_isolated_scripts_and_sizes",
        ],
        additional_compile_targets = [
            "all",
        ],
        mixins = [
            "x86-64",
            "win10",
        ],
    ),
    cores = "64",
    os = os.WINDOWS_DEFAULT,
    free_space = builders.free_space.high,
    console_view_entry = consoles.console_view_entry(
        category = "ToT Windows|x64",
        short_name = "dbg",
    ),
    contact_team_email = "lexan@google.com",
)

ci.builder(
    name = "ToTWin64(dll)",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = ["clang_tot"],
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium_win_clang_tot",
            apply_configs = ["mb"],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
            target_platform = builder_config.target_platform.WIN,
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "clang_tot_gn",
            "no_treat_warnings_as_errors",
            "shared",
            "release",
            "dcheck_always_on",
            "win",
            "x64",
        ],
    ),
    targets = targets.bundle(
        targets = [
            "clang_tot_gtests",
            "win_specific_isolated_scripts_and_sizes",
        ],
        additional_compile_targets = [
            "all",
        ],
        mixins = [
            "x86-64",
            "win10",
        ],
    ),
    cores = "64",
    os = os.WINDOWS_DEFAULT,
    free_space = builders.free_space.high,
    console_view_entry = consoles.console_view_entry(
        category = "ToT Windows|x64",
        short_name = "dll",
    ),
    contact_team_email = "lexan@google.com",
)

ci.builder(
    name = "ToTWinASanLibfuzzer",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = ["clang_tot"],
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium_win_clang_asan_tot",
            apply_configs = ["mb"],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
            target_platform = builder_config.target_platform.WIN,
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "libfuzzer",
            "asan",
            "clang_tot_gn",
            "no_treat_warnings_as_errors",
            "release",
            "chrome_with_codecs",
            "pdf_xfa",
            "minimal_symbols",
            "win",
            "x64",
        ],
    ),
    targets = targets.bundle(
        additional_compile_targets = [
            "all",
        ],
        mixins = [
            "win10",
        ],
    ),
    cores = "64",
    os = os.WINDOWS_DEFAULT,
    free_space = builders.free_space.high,
    console_view_entry = consoles.console_view_entry(
        category = "ToT Windows|Asan",
        short_name = "fuz",
    ),
    contact_team_email = "lexan@google.com",
)

ci.builder(
    name = "ToTWinArm64PGO",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "clang_tot",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium_win_clang_tot",
            apply_configs = [
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_arch = builder_config.target_arch.ARM,
            target_bits = 64,
            target_platform = builder_config.target_platform.WIN,
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "clang_tot_gn",
            "no_treat_warnings_as_errors",
            "official_optimize",
            "no_symbols",
            "pgo_phase_1",
            "arm64",
            "win",
        ],
    ),
    targets = targets.bundle(
        additional_compile_targets = [
            "all",
        ],
        mixins = [
            "arm64",
            "win11",
        ],
    ),
    cores = "64",
    os = os.WINDOWS_DEFAULT,
    free_space = builders.free_space.high,
    gardener_rotations = args.ignore_default(None),
    console_view_entry = consoles.console_view_entry(
        category = "ToT Windows",
        short_name = "pgo-arm",
    ),
    contact_team_email = "lexan@google.com",
)

ci.builder(
    name = "ToTWindowsCoverage",
    executable = "recipe:chromium_clang_coverage_tot",
    gn_args = gn_args.config(
        configs = [
            "clang_tot_gn",
            "no_treat_warnings_as_errors",
            "use_clang_coverage",
            "minimal_symbols",
            "release",
            "win",
            "x64",
        ],
    ),
    cores = "64",
    os = os.WINDOWS_DEFAULT,
    free_space = builders.free_space.high,
    console_view_entry = consoles.console_view_entry(
        category = "ToT Code Coverage",
        short_name = "win",
    ),
    contact_team_email = "lexan@google.com",
)

ci.builder(
    name = "ToTWin64PGO",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = ["clang_tot"],
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium_win_clang_tot",
            apply_configs = ["mb"],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
            target_platform = builder_config.target_platform.WIN,
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "clang_tot_gn",
            "no_treat_warnings_as_errors",
            "official_optimize",
            "no_symbols",
            "pgo_phase_1",
            "win",
            "x64",
        ],
    ),
    targets = targets.bundle(
        targets = [
            "clang_tot_gtests",
        ],
        additional_compile_targets = [
            "all",
        ],
        mixins = [
            "x86-64",
            "win10",
        ],
    ),
    cores = "64",
    os = os.WINDOWS_DEFAULT,
    free_space = builders.free_space.high,
    console_view_entry = consoles.console_view_entry(
        category = "ToT Windows|x64",
        short_name = "pgo",
    ),
    contact_team_email = "lexan@google.com",
)

ci.builder(
    name = "linux-win-cross-clang-tot-rel",
    description_html = "Linux to Windows cross compile with Clang ToT. " +
                       "Previously at {}.".format(
                           linkify_builder("ci", "linux-win_cross-rel"),
                       ),
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "clang_tot",
                "win",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium_win_clang_tot",
            apply_configs = ["mb"],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
            target_platform = builder_config.target_platform.WIN,
            host_platform = builder_config.host_platform.LINUX,
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "clang_tot_gn",
            "no_treat_warnings_as_errors",
            "win_cross",
            "minimal_symbols",
            "shared",
            "release",
            "dcheck_always_on",
            "x64",
        ],
    ),
    targets = targets.bundle(
        targets = [
            "clang_tot_gtests",
        ],
        additional_compile_targets = [
            "all",
        ],
        mixins = [
            "win10",
        ],
    ),
    console_view_entry = consoles.console_view_entry(
        category = "ToT Windows",
        short_name = "lxw",
    ),
    contact_team_email = "lexan@google.com",
)

ci.builder(
    name = "ToTiOS",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "ios",
            apply_configs = ["clang_tot"],
        ),
        chromium_config = builder_config.chromium_config(
            config = "clang_tot_ios",
            apply_configs = [
                "mb",
                "mac_toolchain",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
            target_platform = builder_config.target_platform.IOS,
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "clang_tot_gn",
            "no_treat_warnings_as_errors",
            "ios_simulator",
            "arm64",
            "ios_disable_code_signing",
            "release_builder",
            "xctest",
        ],
    ),
    targets = targets.bundle(
        targets = [
            "ios_clang_tot_sim_tests",
        ],
        additional_compile_targets = [
            "all",
        ],
        mixins = [
            "expand-as-isolated-script",
            "has_native_resultdb_integration",
            "mac_default_arm64",
            "mac_toolchain",
            "out_dir_arg",
            "xcode_26_main",
            "xctest",
        ],
    ),
    builderless = False,
    cores = None,
    os = os.MAC_DEFAULT,
    cpu = cpu.ARM64,
    ssd = True,
    console_view_entry = consoles.console_view_entry(
        category = "iOS|public",
        short_name = "sim",
    ),
    contact_team_email = "lexan@google.com",
    xcode = xcode.xcode_default,
)

ci.builder(
    name = "ToTiOSDevice",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "ios",
            apply_configs = ["clang_tot"],
        ),
        chromium_config = builder_config.chromium_config(
            config = "clang_tot_ios",
            apply_configs = [
                "mb",
                "mac_toolchain",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
            target_platform = builder_config.target_platform.IOS,
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "clang_tot_gn",
            "no_treat_warnings_as_errors",
            "ios_device",
            "arm64",
            "release",
            "xctest",
        ],
    ),
    targets = targets.bundle(
        targets = [
            "ios_clang_tot_device_tests",
        ],
        additional_compile_targets = [
            "base_unittests",
            "boringssl_crypto_tests",
            "boringssl_ssl_tests",
            "components_unittests",
            "crypto_unittests",
            "gfx_unittests",
            "google_apis_unittests",
            "ios_chrome_unittests",
            "ios_credential_provider_extension_unittests",
            "ios_net_unittests",
            "ios_web_inttests",
            "ios_web_unittests",
            "ios_web_view_inttests",
            "net_unittests",
            "skia_unittests",
            "sql_unittests",
            "ui_base_unittests",
            "url_unittests",
        ],
        mixins = [
            "expand-as-isolated-script",
            "has_native_resultdb_integration",
            "ios_restart_device",
            "limited_capacity_bot",
            "mac_toolchain",
            "out_dir_arg",
            "xcode_26_main",
            "xctest",
        ],
    ),
    builderless = False,
    cores = None,
    os = os.MAC_DEFAULT,
    cpu = cpu.ARM64,
    ssd = True,
    console_view_entry = consoles.console_view_entry(
        category = "iOS|public",
        short_name = "dev",
    ),
    contact_team_email = "lexan@google.com",
    xcode = xcode.xcode_default,
)

tot_mac_builder(
    name = "ToTMac",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "clang_tot",
                # This is necessary due to this builder running the
                # telemetry_perf_unittests suite.
                "chromium_with_telemetry_dependencies",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "clang_tot_mac",
            apply_configs = ["mb"],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
            target_platform = builder_config.target_platform.MAC,
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "clang_tot_gn",
            "no_treat_warnings_as_errors",
            "minimal_symbols",
            "shared",
            "release",
            "x64",
        ],
    ),
    targets = targets.bundle(
        targets = [
            "clang_tot_gtests",
        ],
        additional_compile_targets = [
            "all",
        ],
        mixins = [
            "mac_default_x64",
        ],
    ),
    console_view_entry = consoles.console_view_entry(
        category = "ToT Mac",
        short_name = "rel",
    ),
    execution_timeout = 20 * time.hour,
)

tot_mac_builder(
    name = "ToTMac (dbg)",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = ["clang_tot"],
        ),
        chromium_config = builder_config.chromium_config(
            config = "clang_tot_mac",
            apply_configs = ["mb"],
            build_config = builder_config.build_config.DEBUG,
            target_bits = 64,
            target_platform = builder_config.target_platform.MAC,
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "clang_tot_gn",
            "no_treat_warnings_as_errors",
            "shared",
            "debug",
            "x64",
        ],
    ),
    targets = targets.bundle(
        additional_compile_targets = [
            "all",
        ],
        mixins = [
            "mac_default_x64",
        ],
    ),
    console_view_entry = consoles.console_view_entry(
        category = "ToT Mac",
        short_name = "dbg",
    ),
    execution_timeout = 20 * time.hour,
)

tot_mac_builder(
    name = "ToTMacASan",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = ["clang_tot"],
        ),
        chromium_config = builder_config.chromium_config(
            config = "clang_tot_mac_asan",
            apply_configs = ["mb"],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
            target_platform = builder_config.target_platform.MAC,
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "asan",
            "clang_tot_gn",
            "no_treat_warnings_as_errors",
            "minimal_symbols",
            "release_builder",
            "x64",
        ],
    ),
    targets = targets.bundle(
        targets = [
            "clang_tot_gtests",
        ],
        additional_compile_targets = [
            "chromium_builder_asan",
        ],
        mixins = [
            targets.mixin(
                args = [
                    "--test-launcher-print-test-stdio=always",
                ],
            ),
            "mac_default_x64",
        ],
    ),
    console_view_entry = consoles.console_view_entry(
        category = "ToT Mac",
        short_name = "asn",
    ),
    execution_timeout = 20 * time.hour,
)

tot_mac_builder(
    name = "ToTMacPGO",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "clang_tot",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "clang_tot_mac",
            apply_configs = ["mb"],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
            target_platform = builder_config.target_platform.MAC,
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "clang_tot_gn",
            "no_treat_warnings_as_errors",
            "official_optimize",
            "no_symbols",
            "pgo_phase_1",
            "x64",
        ],
    ),
    targets = targets.bundle(
        targets = [
            "clang_tot_gtests",
        ],
        additional_compile_targets = [
            "all",
        ],
        mixins = [
            "mac_default_x64",
        ],
    ),
    console_view_entry = consoles.console_view_entry(
        category = "ToT Mac",
        short_name = "pgo",
    ),
)

tot_mac_builder(
    name = "ToTMacArm64PGO",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "clang_tot",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "clang_tot_mac",
            apply_configs = ["mb"],
            build_config = builder_config.build_config.RELEASE,
            target_arch = builder_config.target_arch.ARM,
            target_bits = 64,
            target_platform = builder_config.target_platform.MAC,
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "clang_tot_gn",
            "no_treat_warnings_as_errors",
            "official_optimize",
            "no_symbols",
            "pgo_phase_1",
            "arm64",
        ],
    ),
    targets = targets.bundle(
        additional_compile_targets = [
            "all",
        ],
        mixins = [
            "mac_default_x64",
        ],
    ),
    console_view_entry = consoles.console_view_entry(
        category = "ToT Mac",
        short_name = "pgo-arm",
    ),
)

tot_mac_builder(
    name = "ToTMacArm64",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "clang_tot",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "clang_tot_mac",
            apply_configs = ["mb"],
            build_config = builder_config.build_config.RELEASE,
            target_arch = builder_config.target_arch.ARM,
            target_bits = 64,
            target_platform = builder_config.target_platform.MAC,
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "clang_tot_gn",
            "no_treat_warnings_as_errors",
            "minimal_symbols",
            "arm64",
            "release",
        ],
    ),
    targets = targets.bundle(
        additional_compile_targets = [
            "all",
        ],
        mixins = [
            "mac_default_x64",
        ],
    ),
    console_view_entry = consoles.console_view_entry(
        category = "ToT Mac",
        short_name = "arm",
    ),
)

tot_mac_builder(
    name = "ToTMacCoverage",
    executable = "recipe:chromium_clang_coverage_tot",
    gn_args = gn_args.config(
        configs = [
            "clang_tot_gn",
            "no_treat_warnings_as_errors",
            "use_clang_coverage",
            "minimal_symbols",
            "release",
            "arm64",
        ],
    ),
    console_view_entry = consoles.console_view_entry(
        category = "ToT Code Coverage",
        short_name = "mac",
    ),
)

### Rust builders: These are also on the clang/lexan waterfall and watched by
### the same gardening rotation

ci.builder(
    name = "ToTRustLinux(dbg)",
    description_html = "Builder that builds and tests chromium using ToT Rust," +
                       "built against ToT LLVM, on linux in debug mode.",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = ["rust_tot"],
        ),
        chromium_config = builder_config.chromium_config(
            config = "rust_tot_linux",
            apply_configs = ["mb"],
            build_config = builder_config.build_config.DEBUG,
            target_arch = builder_config.target_arch.INTEL,
            target_bits = 64,
            target_platform = builder_config.target_platform.LINUX,
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "rust_tot_gn",
            "no_treat_warnings_as_errors",
            "shared",
            "debug",
            "x64",
            "linux",
            "remoteexec",
        ],
    ),
    targets = targets.bundle(
        targets = [
        ],
        additional_compile_targets = [
            "all",
        ],
        mixins = [
            "linux-jammy",
        ],
    ),
    console_view_entry = consoles.console_view_entry(
        category = "Rust ToT",
        short_name = "lin",
    ),
    contact_team_email = "lexan@google.com",
)

ci.builder(
    name = "ToTRustWin(dbg)",
    description_html = "Builder that builds and tests chromium using ToT Rust," +
                       "built against ToT LLVM, on windows in debug mode.",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = ["rust_tot"],
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium_win_rust_tot",
            apply_configs = ["mb"],
            build_config = builder_config.build_config.DEBUG,
            target_bits = 32,
            target_platform = builder_config.target_platform.WIN,
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "rust_tot_gn",
            "no_treat_warnings_as_errors",
            "shared",
            "debug",
            "x86",
            "win",
        ],
    ),
    targets = targets.bundle(
        targets = [
            "clang_tot_gtests",
            # Doesn't run win_specific_isolated_scripts because the mini
            # installer isn't hooked up in 32-bit debug builds.
        ],
        additional_compile_targets = [
            "all",
        ],
        mixins = [
            "win10",
        ],
    ),
    cores = "64",
    os = os.WINDOWS_DEFAULT,
    free_space = builders.free_space.high,
    console_view_entry = consoles.console_view_entry(
        category = "Rust ToT",
        short_name = "win",
    ),
    contact_team_email = "lexan@google.com",
)

tot_mac_builder(
    name = "ToTRustMac(dbg)",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = ["rust_tot"],
        ),
        chromium_config = builder_config.chromium_config(
            config = "rust_tot_mac",
            apply_configs = ["mb"],
            build_config = builder_config.build_config.DEBUG,
            target_bits = 64,
            target_platform = builder_config.target_platform.MAC,
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "rust_tot_gn",
            "no_treat_warnings_as_errors",
            "shared",
            "debug",
            "x64",
        ],  # "mac" is added automatically since this is a `tot_mac_builder` call
    ),
    targets = targets.bundle(
        additional_compile_targets = [
            "all",
        ],
        mixins = [
            "mac_default_x64",
        ],
    ),
    console_view_entry = consoles.console_view_entry(
        category = "Rust ToT",
        short_name = "mac",
    ),
    execution_timeout = 20 * time.hour,
    is_rust = True,
)
