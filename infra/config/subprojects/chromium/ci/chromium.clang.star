# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Definitions of builders in the chromium.clang builder group."""

load("//lib/args.star", "args")
load("//lib/branches.star", "branches")
load("//lib/builder_config.star", "builder_config")
load("//lib/builder_health_indicators.star", "health_spec")
load("//lib/builders.star", "builders", "cpu", "gardener_rotations", "os", "siso")
load("//lib/ci.star", "ci")
load("//lib/consoles.star", "consoles")
load("//lib/gn_args.star", "gn_args")
load("//lib/html.star", "linkify_builder")
load("//lib/targets.star", "targets")
load("//lib/xcode.star", "xcode")

ci.defaults.set(
    executable = ci.DEFAULT_EXECUTABLE,
    builder_group = "chromium.clang",
    pool = ci.DEFAULT_POOL,
    builderless = True,
    cores = 32,
    os = os.LINUX_DEFAULT,
    gardener_rotations = gardener_rotations.CHROMIUM_CLANG,
    # Because these run ToT Clang, reclient is not used.
    # Naturally the runtime will be ~4-8h on average, depending on config.
    # CFI builds will take even longer - around 11h.
    execution_timeout = 14 * time.hour,
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
    },
    service_account = ci.DEFAULT_SERVICE_ACCOUNT,
    shadow_service_account = ci.DEFAULT_SHADOW_SERVICE_ACCOUNT,
    siso_enabled = True,
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

def clang_mac_builder(*, name, cores = 12, **kwargs):
    if "gn_args" in kwargs:
        kwargs["gn_args"].configs.append("mac")
    return ci.builder(
        name = name,
        cores = cores,
        os = os.MAC_DEFAULT,
        ssd = True,
        properties = {
            # The Chromium build doesn't need system Xcode, but the ToT clang
            # bots also build clang and llvm and that build does need system
            # Xcode.
            "xcode_build_version": "14c18",
        },
        contact_team_email = "lexan@google.com",
        description_html = "Builder that builds ToT Clang and uses it to build Chromium",
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
        build_gs_bucket = "chromium-clang-archive",
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
    siso_remote_jobs = siso.remote_jobs.DEFAULT,
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
        build_gs_bucket = "chromium-clang-archive",
    ),
    gn_args = gn_args.config(
        configs = [
            "clang_tot",
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
        build_gs_bucket = "chromium-clang-archive",
    ),
    gn_args = gn_args.config(
        configs = [
            "asan",
            "clang_tot",
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
    os = os.WINDOWS_DEFAULT,
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
        build_gs_bucket = "chromium-clang-archive",
    ),
    gn_args = gn_args.config(
        configs = [
            "asan",
            "clang_tot",
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
    os = os.WINDOWS_DEFAULT,
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
        android_config = builder_config.android_config(config = "clang_builder_mb_x64"),
        build_gs_bucket = "chromium-clang-archive",
    ),
    gn_args = gn_args.config(
        configs = [
            "android_builder",
            "release_builder",
            "minimal_symbols",
            "strip_debug_info",
            "clang_tot",
            "arm",
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
            "chromium_pixel_2_pie",
            "has_native_resultdb_integration",
        ],
        per_test_modifications = {
            "base_unittests": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/android.pie_tot.base_unittests.filter",
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
        android_config = builder_config.android_config(config = "clang_builder_mb_x64"),
        build_gs_bucket = "chromium-clang-archive",
    ),
    gn_args = gn_args.config(
        configs = [
            "android_builder_without_codecs",
            "clang_tot",
            "shared",
            "debug",
            "arm",
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
        android_config = builder_config.android_config(config = "clang_builder_mb_x64"),
        build_gs_bucket = "chromium-clang-archive",
    ),
    gn_args = gn_args.config(
        configs = [
            "android_builder_without_codecs",
            "clang_tot",
            "shared",
            "release",
            "x64",
            "dcheck_always_on",
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
        android_config = builder_config.android_config(config = "clang_builder_mb_x64"),
        build_gs_bucket = "chromium-clang-archive",
    ),
    gn_args = gn_args.config(
        configs = [
            "android_builder_without_codecs",
            "clang_tot",
            "shared",
            "release",
            "x86",
            "dcheck_always_on",
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
        android_config = builder_config.android_config(config = "clang_builder_mb_x64"),
        build_gs_bucket = "chromium-clang-archive",
    ),
    gn_args = gn_args.config(
        configs = [
            "android_builder_without_codecs",
            "clang_tot",
            "shared",
            "release",
            "x86",
            "dcheck_always_on",
            "use_clang_coverage",
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
        android_config = builder_config.android_config(config = "clang_builder_mb_x64"),
        build_gs_bucket = "chromium-clang-archive",
    ),
    gn_args = gn_args.config(
        configs = [
            "android_builder_without_codecs",
            "clang_tot",
            "release",
            "arm64",
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
    name = "ToTAndroidASan",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "clang_tot",
                "android",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "clang_tot_android_asan",
            apply_configs = ["mb"],
            build_config = builder_config.build_config.RELEASE,
            target_arch = builder_config.target_arch.ARM,
            target_bits = 32,
            target_platform = builder_config.target_platform.ANDROID,
        ),
        android_config = builder_config.android_config(config = "asan_symbolize"),
        build_gs_bucket = "chromium-clang-archive",
    ),
    gn_args = gn_args.config(
        configs = [
            "android_builder_without_codecs",
            "clang_tot",
            "asan",
            "debug_builder",
            "strip_debug_info",
            "arm",
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
                swarming = targets.swarming(
                    dimensions = {
                        "device_os": "MMB29Q",
                        "device_type": "bullhead",
                        "os": "Android",
                    },
                ),
            ),
            "has_native_resultdb_integration",
        ],
    ),
    targets_settings = targets.settings(
        os_type = targets.os_type.ANDROID,
    ),
    console_view_entry = consoles.console_view_entry(
        category = "ToT Android",
        short_name = "asn",
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
        android_config = builder_config.android_config(config = "clang_builder_mb_x64"),
        build_gs_bucket = "chromium-clang-archive",
    ),
    gn_args = gn_args.config(
        configs = [
            "android_builder",
            "release_builder",
            "minimal_symbols",
            "official_optimize",
            "clang_tot",
            "arm64",
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
        build_gs_bucket = "chromium-clang-archive",
    ),
    gn_args = gn_args.config(
        configs = [
            "lacros_on_linux",
            "release",
            "clang_tot",
            "x64",
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
        build_gs_bucket = "chromium-clang-archive",
    ),
    gn_args = gn_args.config(
        configs = [
            "lacros_on_linux",
            "debug",
            "clang_tot",
            "x64",
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
        build_gs_bucket = "chromium-clang-archive",
        run_tests_serially = True,
    ),
    gn_args = gn_args.config(
        configs = [
            "fuchsia",
            "release_builder",
            "clang_tot",
            "cast_receiver_size_optimized",
            "x64",
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
        build_gs_bucket = "chromium-clang-archive",
        run_tests_serially = True,
    ),
    gn_args = gn_args.config(
        configs = [
            "official_optimize",
            "fuchsia",
            "arm64",
            "clang_tot",
            "static",
            "arm64_host",
            "cast_receiver_size_optimized",
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
            "linux-jammy-or-focal",
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
        build_gs_bucket = "chromium-clang-archive",
    ),
    gn_args = gn_args.config(
        # Enable debug info, as on official builders, to catch issues with
        # optimized debug info.
        configs = [
            "clang_tot",
            "full_symbols",
            "shared",
            "release",
            "x64",
        ],
    ),
    targets = targets.bundle(
        targets = [
            "clang_tot_gtests",
            "chrome_sizes_suite",
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
        build_gs_bucket = "chromium-clang-archive",
    ),
    gn_args = gn_args.config(
        configs = [
            "clang_tot",
            "shared",
            "debug",
            "x64",
        ],
    ),
    targets = targets.bundle(
        targets = [
            "chrome_sizes_suite",
        ],
        additional_compile_targets = [
            "all",
        ],
        mixins = [
            "linux-jammy",
        ],
    ),
    short_name = "dbg",
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
        build_gs_bucket = "chromium-clang-archive",
    ),
    gn_args = gn_args.config(
        configs = [
            "clang_tot",
            "asan",
            "lsan",
            "release_builder",
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
        build_gs_bucket = "chromium-clang-archive",
    ),
    gn_args = gn_args.config(
        configs = [
            "libfuzzer",
            "asan",
            "clang_tot",
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
            "chrome_sizes_suite",
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
            "clang_tot",
            "use_clang_coverage",
            "minimal_symbols",
            "release",
            "x64",
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
        build_gs_bucket = "chromium-clang-archive",
    ),
    gn_args = gn_args.config(
        configs = [
            "clang_tot",
            "msan",
            "release",
            "x64",
        ],
    ),
    targets = targets.bundle(
        targets = [
            "clang_tot_gtests",
            "chrome_sizes_suite",
        ],
        additional_compile_targets = [
            "all",
        ],
        mixins = [
            "linux-focal",
        ],
    ),
    os = os.LINUX_FOCAL,
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
        build_gs_bucket = "chromium-clang-archive",
    ),
    gn_args = gn_args.config(
        configs = [
            "clang_tot",
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
        build_gs_bucket = "chromium-clang-archive",
    ),
    gn_args = gn_args.config(
        configs = [
            "clang_tot",
            "tsan",
            "release",
            "x64",
        ],
    ),
    targets = targets.bundle(
        targets = [
            "clang_tot_gtests",
            "chrome_sizes_suite",
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
        build_gs_bucket = "chromium-clang-archive",
    ),
    gn_args = gn_args.config(
        configs = [
            "clang_tot",
            "ubsan_vptr_no_recover_hack",
            "release_builder",
            "x64",
        ],
    ),
    targets = targets.bundle(
        targets = [
            "clang_tot_gtests",
            "chrome_sizes_suite",
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
        build_gs_bucket = "chromium-clang-archive",
    ),
    gn_args = gn_args.config(
        configs = [
            "clang_tot",
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
    os = os.WINDOWS_DEFAULT,
    free_space = builders.free_space.high,
    console_view_entry = consoles.console_view_entry(
        category = "ToT Windows",
        short_name = "rel",
    ),
    contact_team_email = "lexan@google.com",
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
        build_gs_bucket = "chromium-clang-archive",
    ),
    gn_args = gn_args.config(
        configs = [
            "clang_tot",
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
            "chrome_sizes_suite",
        ],
        additional_compile_targets = [
            "all",
        ],
        mixins = [
            "win10",
        ],
    ),
    builderless = False,
    os = os.WINDOWS_DEFAULT,
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
        build_gs_bucket = "chromium-clang-archive",
    ),
    gn_args = gn_args.config(
        configs = [
            "clang_tot",
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
    os = os.WINDOWS_DEFAULT,
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
        build_gs_bucket = "chromium-clang-archive",
    ),
    gn_args = gn_args.config(
        configs = [
            "clang_tot",
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
    os = os.WINDOWS_DEFAULT,
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
        build_gs_bucket = "chromium-clang-archive",
    ),
    gn_args = gn_args.config(
        configs = [
            "clang_tot",
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
        build_gs_bucket = "chromium-clang-archive",
    ),
    gn_args = gn_args.config(
        configs = [
            "clang_tot",
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
        build_gs_bucket = "chromium-clang-archive",
    ),
    gn_args = gn_args.config(
        configs = [
            "libfuzzer",
            "asan",
            "clang_tot",
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
    builderless = False,
    os = os.WINDOWS_DEFAULT,
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
        build_gs_bucket = "chromium-clang-archive",
    ),
    gn_args = gn_args.config(
        configs = [
            "clang_tot",
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
    os = os.WINDOWS_DEFAULT,
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
            "clang_tot",
            "use_clang_coverage",
            "minimal_symbols",
            "release",
            "win",
            "x64",
        ],
    ),
    os = os.WINDOWS_DEFAULT,
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
        build_gs_bucket = "chromium-clang-archive",
    ),
    gn_args = gn_args.config(
        configs = [
            "clang_tot",
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
    os = os.WINDOWS_DEFAULT,
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
        ),
        build_gs_bucket = "chromium-clang-archive",
    ),
    gn_args = gn_args.config(
        configs = [
            "clang_tot",
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
        build_gs_bucket = "chromium-clang-archive",
    ),
    gn_args = gn_args.config(
        configs = [
            "clang_tot",
            "ios_simulator",
            "x64",
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
            "mac_default_x64",
            "mac_toolchain",
            "out_dir_arg",
            "xcode_16_main",
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
        build_gs_bucket = "chromium-clang-archive",
    ),
    gn_args = gn_args.config(
        configs = [
            "clang_tot",
            "ios_device",
            "arm64",
            "release",
            "ios_chromium_cert",
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
            "mac_default_x64",
            "mac_toolchain",
            "out_dir_arg",
            "xcode_16_main",
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

clang_mac_builder(
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
        build_gs_bucket = "chromium-clang-archive",
    ),
    gn_args = gn_args.config(
        configs = [
            "clang_tot",
            "minimal_symbols",
            "shared",
            "release",
            "x64",
        ],
    ),
    targets = targets.bundle(
        targets = [
            "clang_tot_gtests",
            "chromium_mac_rel_isolated_scripts_and_sizes",
        ],
        additional_compile_targets = [
            "all",
        ],
        mixins = [
            "mac_default_x64",
        ],
    ),
    cores = None,
    cpu = cpu.ARM64,
    console_view_entry = consoles.console_view_entry(
        category = "ToT Mac",
        short_name = "rel",
    ),
    execution_timeout = 20 * time.hour,
)

clang_mac_builder(
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
        build_gs_bucket = "chromium-clang-archive",
    ),
    gn_args = gn_args.config(
        configs = [
            "clang_tot",
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
    cores = None,
    cpu = cpu.ARM64,
    console_view_entry = consoles.console_view_entry(
        category = "ToT Mac",
        short_name = "dbg",
    ),
    execution_timeout = 20 * time.hour,
)

clang_mac_builder(
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
        build_gs_bucket = "chromium-clang-archive",
    ),
    gn_args = gn_args.config(
        configs = [
            "asan",
            "clang_tot",
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
    cores = None,
    cpu = cpu.ARM64,
    console_view_entry = consoles.console_view_entry(
        category = "ToT Mac",
        short_name = "asn",
    ),
    execution_timeout = 20 * time.hour,
)

clang_mac_builder(
    name = "ToTMacPGO",
    gn_args = gn_args.config(
        configs = [
            "clang_tot",
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

clang_mac_builder(
    name = "ToTMacArm64PGO",
    gn_args = gn_args.config(
        configs = [
            "clang_tot",
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

clang_mac_builder(
    name = "ToTMacArm64",
    gn_args = gn_args.config(
        configs = [
            "clang_tot",
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

clang_mac_builder(
    name = "ToTMacCoverage",
    executable = "recipe:chromium_clang_coverage_tot",
    gn_args = gn_args.config(
        configs = [
            "clang_tot",
            "use_clang_coverage",
            "minimal_symbols",
            "release",
            "x64",
        ],
    ),
    cores = None,
    cpu = cpu.ARM64,
    console_view_entry = consoles.console_view_entry(
        category = "ToT Code Coverage",
        short_name = "mac",
    ),
)
