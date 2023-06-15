# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

load("//lib/branches.star", "branches")
load("//lib/builder_config.star", "builder_config")
load("//lib/builders.star", "os", "reclient", "xcode")
load("//lib/ci.star", "ci")
load("//lib/consoles.star", "consoles")
load("//project.star", "settings")

# crbug/1408581 - The code coverage CI builders are expected to be triggered
# off the same ref every 12 hours. This poller is configured with a schedule
# to ensure this - setting schedules on the builder configuration does not
# guarantee that they are triggered off the same ref.
luci.gitiles_poller(
    name = "code-coverage-gitiles-trigger",
    bucket = "ci",
    repo = "https://chromium.googlesource.com/chromium/src",
    refs = [settings.ref],
    schedule = "with 12h interval",
)

ci.defaults.set(
    executable = ci.DEFAULT_EXECUTABLE,
    builder_group = "chromium.coverage",
    pool = ci.DEFAULT_POOL,
    cores = 32,
    ssd = True,
    execution_timeout = 20 * time.hour,
    priority = ci.DEFAULT_FYI_PRIORITY,
    reclient_instance = reclient.instance.DEFAULT_TRUSTED,
    reclient_jobs = reclient.jobs.DEFAULT,
    service_account = ci.DEFAULT_SERVICE_ACCOUNT,
)

consoles.console_view(
    name = "chromium.coverage",
    title = "Code Coverage CI Builders",
)

def coverage_builder(**kwargs):
    return ci.builder(
        schedule = "triggered",
        triggered_by = ["code-coverage-gitiles-trigger"],
        # This should allow one to be pending should code coverage
        # builds take longer.
        triggering_policy = scheduler.greedy_batching(
            max_concurrent_invocations = 2,
        ),
        **kwargs
    )

coverage_builder(
    name = "android-code-coverage",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = ["android"],
        ),
        chromium_config = builder_config.chromium_config(
            config = "android",
            apply_configs = [
                "download_vr_test_apks",
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
            target_platform = builder_config.target_platform.ANDROID,
        ),
        android_config = builder_config.android_config(config = "main_builder"),
        build_gs_bucket = "chromium-fyi-archive",
    ),
    os = os.LINUX_DEFAULT,
    console_view_entry = [
        consoles.console_view_entry(
            category = "android",
            short_name = "arm64",
        ),
    ],
    coverage_test_types = ["overall", "unit"],
    export_coverage_to_zoss = True,
    generate_blame_list = True,
    reclient_jobs = reclient.jobs.HIGH_JOBS_FOR_CI,
    use_java_coverage = True,
)

coverage_builder(
    name = "android-x86-code-coverage",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = ["android", "enable_wpr_tests"],
        ),
        chromium_config = builder_config.chromium_config(
            config = "android",
            build_config = builder_config.build_config.RELEASE,
            target_bits = 32,
            target_platform = builder_config.target_platform.ANDROID,
        ),
        android_config = builder_config.android_config(
            config = "x86_builder_mb",
        ),
        build_gs_bucket = "chromium-fyi-archive",
    ),
    os = os.LINUX_DEFAULT,
    console_view_entry = [
        consoles.console_view_entry(
            category = "android",
            short_name = "x86",
        ),
    ],
    coverage_test_types = ["overall", "unit"],
    export_coverage_to_zoss = True,
    reclient_jobs = reclient.jobs.HIGH_JOBS_FOR_CI,
    use_java_coverage = True,
)

coverage_builder(
    name = "android-code-coverage-native",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "android",
                "use_clang_coverage",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "android",
            apply_configs = [
                "download_vr_test_apks",
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
            target_platform = builder_config.target_platform.ANDROID,
        ),
        android_config = builder_config.android_config(config = "main_builder"),
        build_gs_bucket = "chromium-fyi-archive",
    ),
    os = os.LINUX_DEFAULT,
    console_view_entry = [
        consoles.console_view_entry(
            category = "android",
            short_name = "ann",
        ),
    ],
    coverage_test_types = ["overall", "unit"],
    export_coverage_to_zoss = True,
    reclient_jobs = reclient.jobs.HIGH_JOBS_FOR_CI,
    use_clang_coverage = True,
)

# fuschia runs outside of chromium, so we do not enable zoss for it.
coverage_builder(
    name = "fuchsia-code-coverage",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "fuchsia_x64",
                "use_clang_coverage",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = ["mb"],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
            target_platform = builder_config.target_platform.FUCHSIA,
        ),
        build_gs_bucket = "chromium-fyi-archive",
    ),
    os = os.LINUX_DEFAULT,
    console_view_entry = [
        consoles.console_view_entry(
            category = "fuschia",
            short_name = "x64",
        ),
        consoles.console_view_entry(
            branch_selector = branches.selector.MAIN,
            console_view = "sheriff.fuchsia",
            category = "fyi|x64",
            short_name = "cov",
        ),
    ],
    coverage_test_types = ["overall", "unit"],
    use_clang_coverage = True,
)

coverage_builder(
    name = "ios-simulator-code-coverage",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "ios",
            apply_configs = ["use_clang_coverage"],
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "mb",
                "mac_toolchain",
            ],
            build_config = builder_config.build_config.DEBUG,
            target_bits = 64,
            target_platform = builder_config.target_platform.IOS,
        ),
        build_gs_bucket = "chromium-fyi-archive",
    ),
    cores = None,
    os = os.MAC_DEFAULT,
    console_view_entry = [
        consoles.console_view_entry(
            category = "ios",
            short_name = "sim",
        ),
    ],
    coverage_exclude_sources = "ios_test_files_and_test_utils",
    coverage_test_types = ["overall", "unit"],
    export_coverage_to_zoss = True,
    use_clang_coverage = True,
    xcode = xcode.x14main,
)

coverage_builder(
    name = "linux-chromeos-code-coverage",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "chromeos",
                "use_clang_coverage",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = ["mb"],
            build_config = builder_config.build_config.RELEASE,
            target_arch = builder_config.target_arch.INTEL,
            target_bits = 64,
        ),
        build_gs_bucket = "chromium-fyi-archive",
    ),
    os = os.LINUX_DEFAULT,
    console_view_entry = [
        consoles.console_view_entry(
            category = "chromeos",
            short_name = "lnx",
        ),
    ],
    coverage_test_types = ["overall", "unit"],
    export_coverage_to_zoss = True,
    reclient_jobs = reclient.jobs.HIGH_JOBS_FOR_CI,
    use_clang_coverage = True,
)

coverage_builder(
    name = "linux-js-code-coverage",
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
            target_arch = builder_config.target_arch.INTEL,
            target_bits = 64,
        ),
        build_gs_bucket = "chromium-fyi-archive",
    ),
    os = os.LINUX_DEFAULT,
    console_view_entry = [
        consoles.console_view_entry(
            category = "linux",
            short_name = "js",
        ),
    ],
    export_coverage_to_zoss = True,
    reclient_jobs = reclient.jobs.HIGH_JOBS_FOR_CI,
    use_javascript_coverage = True,
)

coverage_builder(
    name = "chromeos-js-code-coverage",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "chromeos",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
            target_platform = builder_config.target_platform.CHROMEOS,
        ),
        build_gs_bucket = "chromium-fyi-archive",
    ),
    os = os.LINUX_DEFAULT,
    console_view_entry = [
        consoles.console_view_entry(
            category = "linux",
            short_name = "js",
        ),
    ],
    export_coverage_to_zoss = True,
    reclient_jobs = reclient.jobs.HIGH_JOBS_FOR_CI,
    use_javascript_coverage = True,
)

# Experimental builder. Does not export_coverage_to_zoss.
coverage_builder(
    name = "linux-fuzz-coverage",
    executable = "recipe:chromium/fuzz",
    builderless = True,
    os = os.LINUX_DEFAULT,
    console_view_entry = [
        consoles.console_view_entry(
            category = "linux-fuzz",
            short_name = "lnx-fuzz",
        ),
    ],
)

coverage_builder(
    name = "linux-code-coverage",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = ["use_clang_coverage"],
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = ["mb"],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
        ),
        build_gs_bucket = "chromium-fyi-archive",
    ),
    os = os.LINUX_DEFAULT,
    console_view_entry = [
        consoles.console_view_entry(
            category = "linux",
            short_name = "lnx",
        ),
    ],
    coverage_test_types = ["overall", "unit"],
    export_coverage_to_zoss = True,
    use_clang_coverage = True,
)

coverage_builder(
    name = "linux-lacros-code-coverage",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "chromeos",
                "use_clang_coverage",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = ["mb"],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
        ),
        build_gs_bucket = "chromium-fyi-archive",
    ),
    os = os.LINUX_DEFAULT,
    console_view_entry = [
        consoles.console_view_entry(
            category = "lacros",
            short_name = "lnx",
        ),
    ],
    coverage_test_types = ["overall", "unit"],
    reclient_jobs = reclient.jobs.HIGH_JOBS_FOR_CI,
    use_clang_coverage = True,
)

coverage_builder(
    name = "mac-code-coverage",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = ["use_clang_coverage"],
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = ["mb"],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
        ),
        build_gs_bucket = "chromium-fyi-archive",
    ),
    builderless = True,
    cores = 12,
    os = os.MAC_ANY,
    console_view_entry = [
        consoles.console_view_entry(
            category = "mac",
            short_name = "mac",
        ),
    ],
    coverage_test_types = ["overall", "unit"],
    export_coverage_to_zoss = True,
    reclient_jobs = reclient.jobs.HIGH_JOBS_FOR_CI,
    use_clang_coverage = True,
)

coverage_builder(
    name = "win10-code-coverage",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = ["use_clang_coverage"],
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = ["mb"],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
        ),
        build_gs_bucket = "chromium-fyi-archive",
    ),
    builderless = True,
    os = os.WINDOWS_DEFAULT,
    console_view_entry = [
        consoles.console_view_entry(
            category = "win",
            short_name = "win10",
        ),
    ],
    coverage_test_types = ["overall", "unit"],
    export_coverage_to_zoss = True,
    use_clang_coverage = True,
)
