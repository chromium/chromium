# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

load("//lib/branches.star", "branches")
load("//lib/builder_config.star", "builder_config")
load("//lib/builders.star", "os", "reclient", "xcode")
load("//lib/ci.star", "ci")
load("//lib/consoles.star", "consoles")

ci.defaults.set(
    # TODO - to change the builder group to chromium.coverage, there needs to be
    # some migration work in both tools/mb/mb_config.pyl and testing/buildbot/
    builder_group = "chromium.fyi",
    executable = ci.DEFAULT_EXECUTABLE,
    cores = 32,
    pool = ci.DEFAULT_POOL,
    service_account = ci.DEFAULT_SERVICE_ACCOUNT,
    execution_timeout = 20 * time.hour,
    priority = ci.DEFAULT_FYI_PRIORITY,
    ssd = True,
    reclient_instance = reclient.instance.DEFAULT_TRUSTED,
    reclient_jobs = reclient.jobs.DEFAULT,
)

consoles.console_view(
    name = "chromium.coverage",
    title = "Code Coverage CI Builders",
)

ci.builder(
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
            # TODO: This should be removed once these builders are moved to
            # chromium.coverage from chromium.fyi. The chromium.fyi console
            # view can also be removed then.
            console_view = "chromium.coverage",
            short_name = "and",
        ),
        consoles.console_view_entry(
            category = "code_coverage",
            short_name = "and",
        ),
    ],
    coverage_test_types = ["overall", "unit"],
    export_coverage_to_zoss = True,
    generate_blame_list = True,
    reclient_jobs = reclient.jobs.HIGH_JOBS_FOR_CI,
    schedule = "triggered",
    # TODO: This is currently triggered by an internal coordinating builder
    # that runs every 12 hours. This should be cleaned up s.t. all coverage
    # builders are triggered identically.
    triggered_by = [],
    use_java_coverage = True,
)

ci.builder(
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
            # TODO: This should be removed once these builders are moved to
            # chromium.coverage from chromium.fyi. The chromium.fyi console
            # view can also be removed then.
            console_view = "chromium.coverage",
            short_name = "ann",
        ),
        consoles.console_view_entry(
            category = "code_coverage",
            short_name = "ann",
        ),
    ],
    coverage_test_types = ["overall", "unit"],
    export_coverage_to_zoss = True,
    reclient_jobs = reclient.jobs.HIGH_JOBS_FOR_CI,
    use_clang_coverage = True,
)

ci.builder(
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
            # TODO: This should be removed once these builders are moved to
            # chromium.coverage from chromium.fyi. The chromium.fyi console
            # view can also be removed then.
            console_view = "chromium.coverage",
            short_name = "x64",
        ),
        consoles.console_view_entry(
            category = "code_coverage",
            short_name = "fx",
        ),
        consoles.console_view_entry(
            branch_selector = branches.MAIN,
            console_view = "sheriff.fuchsia",
            category = "fyi|x64",
            short_name = "cov",
        ),
    ],
    coverage_test_types = ["overall", "unit"],
    schedule = "triggered",
    # TODO: This is currently triggered by an internal coordinating builder
    # that runs every 12 hours. This should be cleaned up s.t. all coverage
    # builders are triggered identically.
    triggered_by = [],
    use_clang_coverage = True,
)

ci.builder(
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
    xcode = xcode.x14main,
    console_view_entry = [
        consoles.console_view_entry(
            category = "ios",
            # TODO: This should be removed once these builders are moved to
            # chromium.coverage from chromium.fyi. The chromium.fyi console
            # view can also be removed then.
            console_view = "chromium.coverage",
            short_name = "sim",
        ),
        consoles.console_view_entry(
            category = "code_coverage",
            short_name = "ios",
        ),
    ],
    coverage_exclude_sources = "ios_test_files_and_test_utils",
    coverage_test_types = ["overall", "unit"],
    export_coverage_to_zoss = True,
    use_clang_coverage = True,
)

ci.builder(
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
    triggered_by = [],
    os = os.LINUX_DEFAULT,
    console_view_entry = [
        consoles.console_view_entry(
            category = "chromeos",
            # TODO: This should be removed once these builders are moved to
            # chromium.coverage from chromium.fyi. The chromium.fyi console
            # view can also be removed then.
            console_view = "chromium.coverage",
            short_name = "lnx",
        ),
        consoles.console_view_entry(
            category = "code_coverage",
            short_name = "lcr",
        ),
    ],
    coverage_test_types = ["overall", "unit"],
    export_coverage_to_zoss = True,
    reclient_jobs = reclient.jobs.HIGH_JOBS_FOR_CI,
    schedule = "triggered",
    use_clang_coverage = True,
)

ci.builder(
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
    triggered_by = [],
    os = os.LINUX_DEFAULT,
    console_view_entry = [
        consoles.console_view_entry(
            category = "linux",
            # TODO: This should be removed once these builders are moved to
            # chromium.coverage from chromium.fyi. The chromium.fyi console
            # view can also be removed then.
            console_view = "chromium.coverage",
            short_name = "js",
        ),
        consoles.console_view_entry(
            category = "code_coverage",
            short_name = "jcr",
        ),
    ],
    reclient_jobs = reclient.jobs.HIGH_JOBS_FOR_CI,
    schedule = "triggered",
    use_javascript_coverage = True,
)

ci.builder(
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
    triggered_by = [],
    os = os.LINUX_DEFAULT,
    console_view_entry = [
        consoles.console_view_entry(
            category = "linux",
            # TODO: This should be removed once these builders are moved to
            # chromium.coverage from chromium.fyi. The chromium.fyi console
            # view can also be removed then.
            console_view = "chromium.coverage",
            short_name = "lnx",
        ),
        consoles.console_view_entry(
            category = "code_coverage",
            short_name = "lnx",
        ),
    ],
    coverage_test_types = ["overall", "unit"],
    export_coverage_to_zoss = True,
    use_clang_coverage = True,
)

ci.builder(
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
            # TODO: This should be removed once these builders are moved to
            # chromium.coverage from chromium.fyi. The chromium.fyi console
            # view can also be removed then.
            console_view = "chromium.coverage",
            short_name = "lnx",
        ),
        consoles.console_view_entry(
            category = "code_coverage",
            short_name = "lac",
        ),
    ],
    coverage_test_types = ["overall", "unit"],
    reclient_jobs = reclient.jobs.HIGH_JOBS_FOR_CI,
    use_clang_coverage = True,
)

ci.builder(
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
    cores = 24,
    os = os.MAC_ANY,
    console_view_entry = [
        consoles.console_view_entry(
            category = "mac",
            # TODO: This should be removed once these builders are moved to
            # chromium.coverage from chromium.fyi. The chromium.fyi console
            # view can also be removed then.
            console_view = "chromium.coverage",
            short_name = "mac",
        ),
        consoles.console_view_entry(
            category = "code_coverage",
            short_name = "mac",
        ),
    ],
    coverage_test_types = ["overall", "unit"],
    export_coverage_to_zoss = True,
    reclient_jobs = reclient.jobs.HIGH_JOBS_FOR_CI,
    use_clang_coverage = True,
)

ci.builder(
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
            # TODO: This should be removed once these builders are moved to
            # chromium.coverage from chromium.fyi. The chromium.fyi console
            # view can also be removed then.
            console_view = "chromium.coverage",
            short_name = "win10",
        ),
        consoles.console_view_entry(
            category = "code_coverage",
            short_name = "win",
        ),
    ],
    coverage_test_types = ["overall", "unit"],
    export_coverage_to_zoss = True,
    use_clang_coverage = True,
)
