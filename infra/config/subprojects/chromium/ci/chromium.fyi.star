# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Definitions of builders in the chromium.fuzz builder group."""

load("//lib/branches.star", "branches")
load("//lib/builder_config.star", "builder_config")
load("//lib/builders.star", "builders", "cpu", "goma", "os", "reclient", "xcode")
load("//lib/ci.star", "ci")
load("//lib/consoles.star", "consoles")
load("//lib/structs.star", "structs")

ci.defaults.set(
    builder_group = "chromium.fyi",
    cores = 8,
    executable = ci.DEFAULT_EXECUTABLE,
    execution_timeout = 10 * time.hour,
    goma_backend = goma.backend.RBE_PROD,
    pool = ci.DEFAULT_POOL,
    service_account = ci.DEFAULT_SERVICE_ACCOUNT,
)

consoles.console_view(
    name = "chromium.fyi",
    branch_selector = branches.STANDARD_MILESTONE,
    ordering = {
        None: [
            "code_coverage",
            "cronet",
            "mac",
            "deterministic",
            "fuchsia",
            "chromeos",
            "iOS",
            "infra",
            "linux",
            "recipe",
            "site_isolation",
            "network",
            "viz",
            "win10",
            "win11",
            "win32",
            "backuprefptr",
            "buildperf",
        ],
        "code_coverage": consoles.ordering(
            short_names = ["and", "ann", "lnx", "lcr", "jcr", "mac"],
        ),
        "mac": consoles.ordering(short_names = ["bld", "15", "herm"]),
        "deterministic|mac": consoles.ordering(short_names = ["rel", "dbg"]),
        "iOS|iOS13": consoles.ordering(short_names = ["dev", "sim"]),
        "linux|blink": consoles.ordering(short_names = ["TD"]),
    },
)

def fyi_celab_builder(*, name, **kwargs):
    kwargs.setdefault("executable", "recipe:celab")
    kwargs.setdefault("execution_timeout", ci.DEFAULT_EXECUTION_TIMEOUT)
    kwargs.setdefault("os", os.WINDOWS_ANY)
    kwargs.setdefault("properties", {
        "exclude": "chrome_only",
        "pool_name": "celab-chromium-ci",
        "pool_size": 20,
        "tests": "*",
    })
    return ci.builder(name = name, **kwargs)

def fyi_coverage_builder(*, name, **kwargs):
    kwargs.setdefault("cores", 32)
    kwargs.setdefault("execution_timeout", 20 * time.hour)
    kwargs.setdefault("ssd", True)
    return ci.builder(name = name, **kwargs)

def fyi_ios_builder(*, name, **kwargs):
    kwargs.setdefault("cores", None)
    if kwargs.get("builderless", False):
        kwargs.setdefault("os", os.MAC_DEFAULT)
    kwargs.setdefault("xcode", xcode.x14main)
    return ci.builder(name = name, **kwargs)

def fyi_mac_builder(*, name, **kwargs):
    kwargs.setdefault("cores", 4)
    kwargs.setdefault("os", os.MAC_DEFAULT)
    return ci.builder(name = name, **kwargs)

ci.builder(
    name = "Linux Viz",
    console_view_entry = consoles.console_view_entry(
        category = "viz",
    ),
    goma_backend = None,
    os = os.LINUX_DEFAULT,
    reclient_jobs = reclient.jobs.DEFAULT,
    reclient_instance = reclient.instance.DEFAULT_TRUSTED,
)

ci.builder(
    name = "Site Isolation Android",
    console_view_entry = consoles.console_view_entry(
        category = "site_isolation",
    ),
    notifies = ["Site Isolation Android"],
    goma_backend = None,
    os = os.LINUX_DEFAULT,
    reclient_jobs = reclient.jobs.DEFAULT,
    reclient_instance = reclient.instance.DEFAULT_TRUSTED,
)

ci.builder(
    name = "VR Linux",
    branch_selector = branches.STANDARD_MILESTONE,
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "enable_reclient",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
        ),
        test_results_config = builder_config.test_results_config(
            config = "staging_server",
        ),
        build_gs_bucket = "chromium-fyi-archive",
    ),
    console_view_entry = consoles.console_view_entry(
        category = "linux",
    ),
    cq_mirrors_console_view = "mirrors",
    os = os.LINUX_DEFAULT,
    goma_backend = None,
    reclient_jobs = reclient.jobs.HIGH_JOBS_FOR_CI,
    reclient_instance = reclient.instance.DEFAULT_TRUSTED,
)

ci.builder(
    name = "android-backuprefptr-arm-fyi-rel",
    builderless = True,
    console_view_entry = consoles.console_view_entry(
        category = "backuprefptr|android",
        short_name = "32rel",
    ),
    notifies = ["chrome-memory-safety"],
    os = os.LINUX_DEFAULT,
    goma_backend = None,
    reclient_jobs = reclient.jobs.DEFAULT,
    reclient_instance = reclient.instance.DEFAULT_TRUSTED,
)

ci.builder(
    name = "android-backuprefptr-arm64-fyi-rel",
    builderless = True,
    console_view_entry = consoles.console_view_entry(
        category = "backuprefptr|android",
        short_name = "64rel",
    ),
    notifies = ["chrome-memory-safety"],
    os = os.LINUX_DEFAULT,
    goma_backend = None,
    reclient_jobs = reclient.jobs.DEFAULT,
    reclient_instance = reclient.instance.DEFAULT_TRUSTED,
)

ci.builder(
    name = "fuchsia-fyi-arm64-emu-arg",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "fuchsia_arm64",
                "fuchsia_arm64_host",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_arch = builder_config.target_arch.ARM,
            target_bits = 64,
            target_platform = builder_config.target_platform.FUCHSIA,
        ),
        test_results_config = builder_config.test_results_config(
            config = "staging_server",
        ),
        build_gs_bucket = "chromium-fuchsia-archive",
        run_tests_serially = True,
    ),
    console_view_entry = [
        consoles.console_view_entry(
            category = "fuchsia|a64",
            short_name = "emu-arg",
        ),
    ],
    notifies = ["cr-fuchsia-engprod"],
    os = os.LINUX_DEFAULT,
)

ci.builder(
    name = "fuchsia-fyi-arm64-rel",
    console_view_entry = [
        consoles.console_view_entry(
            category = "fuchsia|a64",
            short_name = "rel",
        ),
        consoles.console_view_entry(
            branch_selector = branches.MAIN,
            console_view = "sheriff.fuchsia",
            category = "fyi",
            short_name = "a64",
        ),
    ],
    notifies = ["cr-fuchsia"],
    os = os.LINUX_DEFAULT,
)

ci.builder(
    name = "fuchsia-fyi-cfv2-script",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "fuchsia",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
            target_platform = "fuchsia",
        ),
        build_gs_bucket = "chromium-fuchsia-archive",
        run_tests_serially = True,
    ),
    console_view_entry = [
        consoles.console_view_entry(
            category = "fuchsia|x64",
            short_name = "cfv2",
        ),
    ],
    os = os.LINUX_DEFAULT,
)

ci.builder(
    name = "fuchsia-fyi-arm64-cfv2-script",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "fuchsia_arm64",
                "fuchsia_arm64_host",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_arch = builder_config.target_arch.ARM,
            target_bits = 64,
            target_platform = "fuchsia",
        ),
        build_gs_bucket = "chromium-fuchsia-archive",
        run_tests_serially = True,
    ),
    console_view_entry = [
        consoles.console_view_entry(
            category = "fuchsia|a64",
            short_name = "cfv2",
        ),
    ],
    os = os.LINUX_DEFAULT,
)

ci.builder(
    name = "fuchsia-fyi-x64-rel",
    console_view_entry = [
        consoles.console_view_entry(
            category = "fuchsia|x64",
            short_name = "rel",
        ),
        consoles.console_view_entry(
            branch_selector = branches.MAIN,
            console_view = "sheriff.fuchsia",
            category = "fyi",
            short_name = "x64",
        ),
    ],
    notifies = ["cr-fuchsia"],
    os = os.LINUX_DEFAULT,
)

ci.builder(
    name = "fuchsia-fyi-x64-reviver",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = ["fuchsia_x64"],
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = ["mb"],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
            target_platform = "fuchsia",
        ),
        build_gs_bucket = "chromium-fuchsia-archive",
    ),
    console_view_entry = [
        consoles.console_view_entry(
            category = "fuchsia|x64",
            short_name = "rev",
        ),
        consoles.console_view_entry(
            branch_selector = branches.MAIN,
            console_view = "sheriff.fuchsia",
            category = "fyi",
            short_name = "rev",
        ),
    ],
    notifies = ["cr-fuchsia"],
    os = os.LINUX_DEFAULT,
)

ci.builder(
    name = "fuchsia-fyi-x64-wst",
    console_view_entry = [
        consoles.console_view_entry(
            category = "fuchsia|x64",
            short_name = "wst",
        ),
        consoles.console_view_entry(
            branch_selector = branches.MAIN,
            console_view = "sheriff.fuchsia",
            category = "fyi",
            short_name = "wst",
        ),
    ],
    notifies = ["cr-fuchsia"],
    os = os.LINUX_DEFAULT,
)

ci.builder(
    name = "lacros-amd64-generic-rel-fyi",
    console_view_entry = consoles.console_view_entry(
        category = "lacros",
        short_name = "lcr",
    ),
    os = os.LINUX_DEFAULT,
    goma_backend = None,
    reclient_jobs = reclient.jobs.HIGH_JOBS_FOR_CI,
    reclient_instance = reclient.instance.DEFAULT_TRUSTED,
)

ci.builder(
    name = "lacros-amd64-generic-rel-skylab-fyi",
    console_view_entry = consoles.console_view_entry(
        category = "lacros",
        short_name = "lsf",
    ),
    os = os.LINUX_DEFAULT,
    # Some tests on this bot depend on being unauthenticated with GS, so
    # don't run the tests inside a luci-auth context to avoid having the
    # BOTO config setup for the task's service account.
    # TODO(crbug.com/1217155): Fix this.
    builder_spec = builder_config.builder_spec(
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = ["mb", "mb_no_luci_auth"],
            target_bits = 64,
            target_cros_boards = "eve",
            cros_boards_with_qemu_images = "amd64-generic",
            target_platform = "chromeos",
        ),
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "chromeos",
                "checkout_lacros_sdk",
            ],
        ),
        skylab_upload_location = builder_config.skylab_upload_location(
            gs_bucket = "lacros-amd64-generic-rel-skylab-try",
        ),
        test_results_config = builder_config.test_results_config(
            config = "staging_server",
        ),
        build_gs_bucket = "chromium-fyi-archive",
    ),
)

ci.builder(
    name = "linux-annotator-rel",
    console_view_entry = consoles.console_view_entry(
        category = "network|traffic|annotations",
        short_name = "lnx",
    ),
    notifies = ["annotator-rel"],
    os = os.LINUX_DEFAULT,
    goma_backend = None,
    reclient_jobs = reclient.jobs.HIGH_JOBS_FOR_CI,
    reclient_instance = reclient.instance.DEFAULT_TRUSTED,
)

ci.builder(
    name = "linux-chromeos-annotator-rel",
    builderless = True,
    branch_selector = branches.STANDARD_MILESTONE,
    console_view_entry = consoles.console_view_entry(
        category = "release",
        short_name = "rel",
    ),
    execution_timeout = 3 * time.hour,
    os = os.LINUX_DEFAULT,
    goma_backend = None,
    reclient_jobs = reclient.jobs.HIGH_JOBS_FOR_CI,
    reclient_instance = reclient.instance.DEFAULT_TRUSTED,
)

ci.builder(
    name = "linux-ash-chromium-builder-fyi-rel",
    console_view_entry = consoles.console_view_entry(
        category = "default",
        short_name = "lcr",
    ),
    os = os.LINUX_DEFAULT,
    properties = {
        # The format of these properties is defined at archive/properties.proto
        "$build/archive": {
            "archive_datas": [
                {
                    "files": [
                        "chrome",
                        "chrome_100_percent.pak",
                        "chrome_200_percent.pak",
                        "chrome_crashpad_handler",
                        "headless_lib_data.pak",
                        "headless_lib_strings.pak",
                        "icudtl.dat",
                        "libminigbm.so",
                        "nacl_helper",
                        "nacl_irt_x86_64.nexe",
                        "resources.pak",
                        "snapshot_blob.bin",
                        "test_ash_chrome",
                    ],
                    "dirs": ["locales", "swiftshader"],
                    "gcs_bucket": "ash-chromium-on-linux-prebuilts",
                    "gcs_path": "x86_64/{%position%}/ash-chromium.zip",
                    "archive_type": "ARCHIVE_TYPE_ZIP",
                    "latest_upload": {
                        "gcs_path": "x86_64/latest/ash-chromium.txt",
                        "gcs_file_content": "{%position%}",
                    },
                },
            ],
        },
    },
)

ci.builder(
    name = "linux-lacros-version-skew-fyi",
    console_view_entry = consoles.console_view_entry(
        category = "default",
    ),
    os = os.LINUX_DEFAULT,
    goma_backend = None,
    reclient_jobs = reclient.jobs.HIGH_JOBS_FOR_CI,
    reclient_instance = reclient.instance.DEFAULT_TRUSTED,
)

ci.builder(
    name = "linux-blink-animation-use-time-delta",
    console_view_entry = consoles.console_view_entry(
        category = "linux|blink",
        short_name = "TD",
    ),
    os = os.LINUX_DEFAULT,
)

ci.builder(
    name = "linux-blink-heap-verification",
    console_view_entry = consoles.console_view_entry(
        category = "linux|blink",
        short_name = "VF",
    ),
    notifies = ["linux-blink-fyi-bots"],
    os = os.LINUX_DEFAULT,
    goma_backend = None,
    reclient_jobs = reclient.jobs.HIGH_JOBS_FOR_CI,
    reclient_instance = reclient.instance.DEFAULT_TRUSTED,
)

ci.builder(
    name = "linux-blink-v8-sandbox-future-rel",
    console_view_entry = consoles.console_view_entry(
        category = "linux|blink",
        short_name = "SB",
    ),
    notifies = ["v8-sandbox-fyi-bots"],
    os = os.LINUX_DEFAULT,
    goma_backend = None,
    reclient_jobs = reclient.jobs.DEFAULT,
    reclient_instance = reclient.instance.DEFAULT_TRUSTED,
    builder_spec = builder_config.builder_spec(
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = ["mb"],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
        ),
        gclient_config = builder_config.gclient_config(
            config = "chromium",
        ),
    ),
)

ci.builder(
    name = "linux-example-builder",
    console_view_entry = consoles.console_view_entry(
        category = "linux",
    ),
    os = os.LINUX_DEFAULT,
    schedule = "with 12h interval",
    triggered_by = [],
    goma_backend = None,
    reclient_jobs = reclient.jobs.DEFAULT,
    reclient_instance = reclient.instance.DEFAULT_TRUSTED,
)

ci.builder(
    name = "linux-fieldtrial-rel",
    console_view_entry = consoles.console_view_entry(
        category = "linux",
    ),
    os = os.LINUX_DEFAULT,
)

ci.builder(
    name = "mac-fieldtrial-rel",
    builderless = False,
    console_view_entry = consoles.console_view_entry(
        category = "mac",
    ),
    builder_spec = builder_config.builder_spec(
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = ["mb"],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
        ),
        gclient_config = builder_config.gclient_config(
            config = "chromium",
        ),
    ),
    cores = None,
    os = os.MAC_DEFAULT,
)

ci.builder(
    name = "android-fieldtrial-rel",
    builderless = False,
    console_view_entry = consoles.console_view_entry(
        category = "android",
    ),
    os = os.LINUX_BIONIC,
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "android",
                "enable_reclient",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "android",
            apply_configs = [
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 32,
            target_platform = builder_config.target_platform.ANDROID,
        ),
        android_config = builder_config.android_config(
            config = "x86_builder",
        ),
        build_gs_bucket = "chromium-android-archive",
    ),
    reclient_jobs = reclient.jobs.DEFAULT,
    reclient_instance = reclient.instance.DEFAULT_TRUSTED,
)

fyi_ios_builder(
    name = "ios-fieldtrial-rel",
    builderless = False,
    console_view_entry = consoles.console_view_entry(
        category = "mac",
    ),
    builder_spec = builder_config.builder_spec(
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "mb",
                "mac_toolchain",
            ],
            build_config = builder_config.build_config.DEBUG,
            target_bits = 64,
            target_platform = "ios",
        ),
        gclient_config = builder_config.gclient_config(
            config = "ios",
        ),
    ),
)

ci.builder(
    name = "linux-lacros-builder-fyi-rel",
    console_view_entry = consoles.console_view_entry(
        category = "linux",
    ),
    os = os.LINUX_DEFAULT,
    goma_backend = None,
    reclient_jobs = reclient.jobs.HIGH_JOBS_FOR_CI,
    reclient_instance = reclient.instance.DEFAULT_TRUSTED,
)

ci.thin_tester(
    name = "linux-lacros-tester-fyi-rel",
    console_view_entry = consoles.console_view_entry(
        category = "linux",
    ),
    triggered_by = ["linux-lacros-builder-fyi-rel"],
)

ci.builder(
    name = "linux-lacros-dbg-fyi",
    console_view_entry = consoles.console_view_entry(
        category = "linux",
    ),
    os = os.LINUX_DEFAULT,
    goma_backend = None,
    reclient_jobs = reclient.jobs.HIGH_JOBS_FOR_CI,
    reclient_instance = reclient.instance.DEFAULT_TRUSTED,
)

ci.thin_tester(
    name = "linux-lacros-dbg-tests-fyi",
    console_view_entry = consoles.console_view_entry(
        category = "linux",
    ),
    triggered_by = ["linux-lacros-dbg-fyi"],
)

ci.builder(
    name = "linux-backuprefptr-x64-fyi-rel",
    builderless = True,
    console_view_entry = consoles.console_view_entry(
        category = "backuprefptr|linux",
        short_name = "64rel",
    ),
    notifies = ["chrome-memory-safety"],
    os = os.LINUX_DEFAULT,
    goma_backend = None,
    reclient_jobs = reclient.jobs.DEFAULT,
    reclient_instance = reclient.instance.DEFAULT_TRUSTED,
)

ci.builder(
    name = "linux-perfetto-rel",
    console_view_entry = consoles.console_view_entry(
        category = "linux",
    ),
    os = os.LINUX_DEFAULT,
    goma_backend = None,
    reclient_jobs = reclient.jobs.DEFAULT,
    reclient_instance = reclient.instance.DEFAULT_TRUSTED,
)

ci.builder(
    name = "linux-wpt-fyi-rel",
    console_view_entry = consoles.console_view_entry(
        category = "linux",
    ),
    experimental = True,
    os = os.LINUX_DEFAULT,
    goma_backend = None,
    reclient_jobs = reclient.jobs.DEFAULT,
    reclient_instance = reclient.instance.DEFAULT_TRUSTED,
)

ci.builder(
    name = "linux-wpt-identity-fyi-rel",
    console_view_entry = consoles.console_view_entry(
        category = "linux",
    ),
    experimental = True,
    os = os.LINUX_DEFAULT,
    goma_backend = None,
    reclient_jobs = reclient.jobs.DEFAULT,
    reclient_instance = reclient.instance.DEFAULT_TRUSTED,
)

ci.builder(
    name = "linux-wpt-input-fyi-rel",
    console_view_entry = consoles.console_view_entry(
        category = "linux",
    ),
    experimental = True,
    os = os.LINUX_DEFAULT,
    goma_backend = None,
    reclient_jobs = reclient.jobs.DEFAULT,
    reclient_instance = reclient.instance.DEFAULT_TRUSTED,
)

# This is launching & collecting entirely isolated tests.
# OS shouldn't matter.
ci.thin_tester(
    name = "mac-osxbeta-rel",
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(
            config = "chromium",
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "mb",
                "goma_use_local",  # to mitigate compile step timeout (crbug.com/1056935)
            ],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
            target_platform = builder_config.target_platform.MAC,
        ),
        test_results_config = builder_config.test_results_config(
            config = "staging_server",
        ),
        build_gs_bucket = "chromium-fyi-archive",
    ),
    console_view_entry = consoles.console_view_entry(
        category = "mac",
        short_name = "beta",
    ),
    main_console_view = None,
    builderless = False,
    os = os.MAC_DEFAULT,
    cores = 12,
    triggered_by = ["ci/Mac Builder (dbg)"],
)

ci.builder(
    name = "linux-headless-shell-rel",
    console_view_entry = consoles.console_view_entry(
        category = "linux",
        short_name = "hdls",
    ),
    notifies = ["headless-owners"],
    os = os.LINUX_DEFAULT,
    goma_backend = None,
    reclient_jobs = reclient.jobs.DEFAULT,
    reclient_instance = reclient.instance.DEFAULT_TRUSTED,
)

# TODO(crbug.com/1320004): Remove this builder after experimentation.
ci.builder(
    name = "linux-rel-no-external-ip",
    builderless = False,
    console_view_entry = consoles.console_view_entry(
        category = "linux",
    ),
    os = os.LINUX_DEFAULT,
    builder_spec = builder_config.copy_from(
        "ci/Linux Builder",
    ),
    # Limited test pool is likely to cause long build times.
    execution_timeout = 24 * time.hour,
    goma_backend = None,
    reclient_jobs = reclient.jobs.DEFAULT,
    reclient_instance = reclient.instance.DEFAULT_TRUSTED,
)

ci.builder(
    name = "mac-backuprefptr-x64-fyi-rel",
    builderless = True,
    console_view_entry = consoles.console_view_entry(
        category = "backuprefptr|mac",
        short_name = "64rel",
    ),
    notifies = ["chrome-memory-safety"],
    os = os.MAC_ANY,
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
        test_results_config = builder_config.test_results_config(
            config = "staging_server",
        ),
        build_gs_bucket = "chromium-fyi-archive",
    ),
    goma_backend = None,
    reclient_jobs = reclient.jobs.DEFAULT,
    reclient_instance = reclient.instance.DEFAULT_TRUSTED,
)

ci.builder(
    name = "win-backuprefptr-x86-fyi-rel",
    builderless = True,
    console_view_entry = consoles.console_view_entry(
        category = "backuprefptr|win",
        short_name = "32rel",
    ),
    notifies = ["chrome-memory-safety"],
    os = os.WINDOWS_ANY,
    goma_backend = None,
    reclient_jobs = reclient.jobs.LOW_JOBS_FOR_CI,
    reclient_instance = reclient.instance.DEFAULT_TRUSTED,
)

ci.builder(
    name = "win-backuprefptr-x64-fyi-rel",
    builderless = True,
    console_view_entry = consoles.console_view_entry(
        category = "backuprefptr|win",
        short_name = "64rel",
    ),
    notifies = ["chrome-memory-safety"],
    os = os.WINDOWS_ANY,
    goma_backend = None,
    reclient_jobs = reclient.jobs.LOW_JOBS_FOR_CI,
    reclient_instance = reclient.instance.DEFAULT_TRUSTED,
)

# TODO(crbug.com/1320004): Remove this builder after experimentation.
ci.builder(
    name = "win10-rel-no-external-ip",
    builderless = False,
    console_view_entry = consoles.console_view_entry(
        category = "win",
    ),
    os = os.WINDOWS_ANY,
    builder_spec = builder_config.copy_from(
        "ci/Win x64 Builder",
    ),
    # Limited test pool is likely to cause long build times.
    execution_timeout = 24 * time.hour,
    goma_backend = None,
    reclient_jobs = reclient.jobs.DEFAULT,
    reclient_instance = reclient.instance.DEFAULT_TRUSTED,
)

ci.builder(
    name = "linux-upload-perfetto",
    console_view_entry = consoles.console_view_entry(
        category = "perfetto",
        short_name = "lnx",
    ),
    os = os.LINUX_DEFAULT,
    goma_backend = None,
    reclient_jobs = reclient.jobs.DEFAULT,
    reclient_instance = reclient.instance.DEFAULT_TRUSTED,
)

ci.builder(
    name = "mac-upload-perfetto",
    builderless = True,
    console_view_entry = consoles.console_view_entry(
        category = "perfetto",
        short_name = "mac",
    ),
    cores = None,
    os = os.MAC_DEFAULT,
    schedule = "with 3h interval",
    triggered_by = [],
)

ci.builder(
    name = "win-upload-perfetto",
    builderless = True,
    console_view_entry = consoles.console_view_entry(
        category = "perfetto",
        short_name = "win",
    ),
    os = os.WINDOWS_DEFAULT,
    schedule = "with 3h interval",
    triggered_by = [],
    goma_backend = None,
    reclient_jobs = reclient.jobs.LOW_JOBS_FOR_CI,
    reclient_instance = reclient.instance.DEFAULT_TRUSTED,
)

ci.builder(
    name = "Comparison Android (reclient)",
    console_view_entry = consoles.console_view_entry(
        category = "android",
        short_name = "cmp",
    ),
    description_html = """\
This builder measures Android build performance with goma vs reclient.<br/>\
The bot specs should be in sync with <a href="https://ci.chromium.org/p/chromium/builders/ci/Deterministic%20Android%20(dbg)">Deterministic Android (dbg)</a>.\
""",
    goma_jobs = 250,
    executable = "recipe:reclient_goma_comparison",
    execution_timeout = 15 * time.hour,
    reclient_cache_silo = "Comparison Android - cache siloed",
    reclient_instance = reclient.instance.DEFAULT_TRUSTED,
    reclient_jobs = 250,
    os = os.LINUX_DEFAULT,
    # Target luci-chromium-ci-bionic-us-central1-b-ssd-16-*.
    ssd = True,
    cores = 16,
)

ci.builder(
    name = "Comparison Linux (reclient)",
    console_view_entry = consoles.console_view_entry(
        category = "linux",
        short_name = "cmp",
    ),
    goma_jobs = 250,
    executable = "recipe:reclient_goma_comparison",
    execution_timeout = 6 * time.hour,
    reclient_cache_silo = "Comparison Linux - cache siloed",
    reclient_instance = reclient.instance.DEFAULT_TRUSTED,
    reclient_jobs = 250,
    os = os.LINUX_DEFAULT,
)

ci.builder(
    name = "Comparison Mac (reclient)",
    builderless = True,
    console_view_entry = consoles.console_view_entry(
        category = "mac",
        short_name = "cmp",
    ),
    goma_jobs = 250,
    executable = "recipe:reclient_goma_comparison",
    execution_timeout = 10 * time.hour,
    reclient_cache_silo = "Comparison Mac - cache siloed",
    reclient_instance = reclient.instance.TEST_TRUSTED,
    reclient_jobs = 250,
    os = os.MAC_DEFAULT,
    cores = None,
)

ci.builder(
    name = "Comparison Mac arm64 (reclient)",
    builderless = True,
    console_view_entry = consoles.console_view_entry(
        category = "mac",
        short_name = "cmp",
    ),
    goma_jobs = 250,
    executable = "recipe:reclient_goma_comparison",
    execution_timeout = 10 * time.hour,
    reclient_cache_silo = "Comparison Mac - cache siloed",
    reclient_instance = reclient.instance.TEST_TRUSTED,
    reclient_jobs = 250,
    os = os.MAC_DEFAULT,
    cores = None,
)

ci.builder(
    name = "Comparison Windows (8 cores) (reclient)",
    builderless = True,
    console_view_entry = consoles.console_view_entry(
        category = "win",
        short_name = "re",
    ),
    cores = 8,
    goma_jobs = 80,
    executable = "recipe:reclient_goma_comparison",
    reclient_cache_silo = "Comparison Windows 8 cores - cache siloed",
    reclient_instance = reclient.instance.DEFAULT_TRUSTED,
    reclient_jobs = 80,
    os = os.WINDOWS_DEFAULT,
    free_space = builders.free_space.high,
)

ci.builder(
    name = "Comparison Windows (reclient)",
    builderless = True,
    console_view_entry = consoles.console_view_entry(
        category = "win",
        short_name = "re",
    ),
    cores = 32,
    goma_jobs = 250,
    executable = "recipe:reclient_goma_comparison",
    execution_timeout = 6 * time.hour,
    reclient_cache_silo = "Comparison Windows - cache siloed",
    reclient_instance = reclient.instance.DEFAULT_TRUSTED,
    reclient_jobs = 250,
    os = os.WINDOWS_DEFAULT,
    free_space = builders.free_space.high,
)

ci.builder(
    name = "Comparison Simple Chrome (reclient)",
    builderless = True,
    console_view_entry = consoles.console_view_entry(
        category = "cros x64",
        short_name = "cmp",
    ),
    goma_jobs = 250,
    executable = "recipe:reclient_goma_comparison",
    execution_timeout = 10 * time.hour,
    reclient_cache_silo = "Comparison Simple Chrome - cache siloed",
    reclient_instance = reclient.instance.DEFAULT_TRUSTED,
    reclient_jobs = 250,
    os = os.LINUX_DEFAULT,
)

ci.builder(
    name = "Comparison ios (reclient)",
    builderless = True,
    console_view_entry = consoles.console_view_entry(
        category = "ios",
        short_name = "cmp",
    ),
    goma_jobs = 250,
    executable = "recipe:reclient_goma_comparison",
    execution_timeout = 10 * time.hour,
    reclient_cache_silo = "Comparison ios - cache siloed",
    reclient_instance = reclient.instance.TEST_TRUSTED,
    reclient_jobs = 250,
    os = os.MAC_DEFAULT,
    cores = None,
    xcode = xcode.x14main,
)

ci.builder(
    name = "Comparison Android (reclient)(CQ)",
    console_view_entry = consoles.console_view_entry(
        category = "android|cq",
        short_name = "cmp",
    ),
    description_html = """\
This builder measures Android build performance with goma vs reclient in cq configuration.<br/>\
The bot specs should be in sync with <a href="https://ci.chromium.org/p/chromium/builders/try/android-pie-arm64-rel-compilator">android-pie-arm64-rel-compilator</a>.\
""",
    goma_jobs = goma.jobs.J300,
    executable = "recipe:reclient_goma_comparison",
    execution_timeout = 15 * time.hour,
    reclient_cache_silo = "Comparison Android CQ - cache siloed",
    reclient_instance = reclient.instance.TEST_UNTRUSTED,
    reclient_jobs = 300,
    os = os.LINUX_DEFAULT,
    cores = 32,
    ssd = True,
)

ci.builder(
    name = "Comparison Linux (reclient)(CQ)",
    console_view_entry = consoles.console_view_entry(
        category = "linux|cq",
        short_name = "cmp",
    ),
    description_html = """\
This builder measures Linux build performance with goma vs reclient in cq configuration.<br/>\
The bot specs should be in sync with <a href="https://ci.chromium.org/p/chromium/builders/try/linux-rel-compilator">linux-rel-compilator</a>.\
""",
    goma_jobs = 150,
    executable = "recipe:reclient_goma_comparison",
    execution_timeout = 6 * time.hour,
    reclient_cache_silo = "Comparison Linux CQ - cache siloed",
    reclient_instance = reclient.instance.TEST_UNTRUSTED,
    reclient_jobs = 150,
    os = os.LINUX_DEFAULT,
    cores = 16,
    ssd = True,
)

ci.builder(
    name = "Comparison Mac (reclient)(CQ)",
    builderless = True,
    console_view_entry = consoles.console_view_entry(
        category = "mac|cq",
        short_name = "cmp",
    ),
    description_html = """\
This builder measures Mac build performance with goma vs reclient in cq configuration.<br/>\
The bot specs should be in sync with <a href="https://ci.chromium.org/p/chromium/builders/try/mac-rel-compilator">mac-rel-compilator</a>.\
""",
    goma_jobs = 150,
    executable = "recipe:reclient_goma_comparison",
    execution_timeout = 10 * time.hour,
    reclient_cache_silo = "Comparison Mac CQ - cache siloed",
    reclient_instance = reclient.instance.TEST_UNTRUSTED,
    reclient_jobs = 150,
    os = os.MAC_DEFAULT,
    ssd = True,
    cores = None,
)

ci.builder(
    name = "Comparison Windows (reclient)(CQ)",
    builderless = True,
    console_view_entry = consoles.console_view_entry(
        category = "win|cq",
        short_name = "re",
    ),
    description_html = """\
This builder measures Windows build performance with goma vs reclient in cq configuration.<br/>\
The bot specs should be in sync with <a href="https://ci.chromium.org/p/chromium/builders/try/win10_chromium_x64_rel_ng-compilator">win10_chromium_x64_rel_ng-compilator</a>.\
""",
    goma_jobs = 300,
    executable = "recipe:reclient_goma_comparison",
    execution_timeout = 6 * time.hour,
    reclient_cache_silo = "Comparison Windows CQ - cache siloed",
    reclient_instance = reclient.instance.TEST_UNTRUSTED,
    reclient_jobs = 300,
    os = os.WINDOWS_DEFAULT,
    ssd = True,
    cores = 32,
)

ci.builder(
    name = "Comparison Simple Chrome (reclient)(CQ)",
    builderless = True,
    console_view_entry = consoles.console_view_entry(
        category = "cros x64|cq",
        short_name = "cmp",
    ),
    description_html = """\
This builder measures Simple Chrome build performance with goma vs reclient in cq configuration.<br/>\
The bot specs should be in sync with <a href="https://ci.chromium.org/p/chromium/builders/try/linux-chromeos-rel-compilator">linux-chromeos-rel-compilator</a>.\
""",
    goma_jobs = 300,
    executable = "recipe:reclient_goma_comparison",
    execution_timeout = 10 * time.hour,
    reclient_cache_silo = "Comparison Simple Chrome CQ - cache siloed",
    reclient_instance = reclient.instance.TEST_UNTRUSTED,
    reclient_jobs = 300,
    os = os.LINUX_DEFAULT,
    cores = 32,
    ssd = True,
)

ci.builder(
    name = "Comparison ios (reclient)(CQ)",
    builderless = True,
    console_view_entry = consoles.console_view_entry(
        category = "ios|cq",
        short_name = "cmp",
    ),
    description_html = """\
This builder measures iOS build performance with goma vs reclient in cq configuration.<br/>\
The bot specs should be in sync with <a href="https://ci.chromium.org/p/chromium/builders/try/ios-simulator">ios-simulator</a>.\
""",
    goma_jobs = 150,
    executable = "recipe:reclient_goma_comparison",
    execution_timeout = 10 * time.hour,
    reclient_cache_silo = "Comparison ios CQ - cache siloed",
    reclient_instance = reclient.instance.TEST_UNTRUSTED,
    reclient_jobs = 150,
    os = os.MAC_DEFAULT,
    cores = None,
    ssd = True,
    xcode = xcode.x14main,
)

# Build Perf builders use CQ reclient instance and high reclient jobs/cores and
# SSD to represent CQ build performance.

ci.builder(
    name = "build-perf-android",
    description_html = """\
This builder measures Android build performance with and without remote caches.<br/>\
The build configs and the bot specs should be in sync with <a href="https://ci.chromium.org/p/chromium/builders/try/android-pie-arm64-rel-compilator">android-pie-arm64-rel-compilator</a>.\
""",
    builderless = True,
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "android",
                "enable_reclient",
                "chromium_no_telemetry_dependencies",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "android",
            apply_configs = [
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
            target_platform = builder_config.target_platform.ANDROID,
        ),
        android_config = builder_config.android_config(
            config = "main_builder",
        ),
    ),
    console_view_entry = consoles.console_view_entry(
        category = "buildperf",
        short_name = "and",
    ),
    executable = "recipe:build_perf",
    execution_timeout = 10 * time.hour,
    service_account = "chromium-build-perf-ci-builder@chops-service-accounts.iam.gserviceaccount.com",
    reclient_instance = reclient.instance.DEFAULT_UNTRUSTED,
    reclient_jobs = reclient.jobs.HIGH_JOBS_FOR_CQ,
    # Target luci-chromium-ci-bionic-us-central1-c-1000-ssd-hm32-*.
    os = os.LINUX_DEFAULT,
    cores = 32,
    ssd = True,
)

ci.builder(
    name = "build-perf-linux",
    description_html = """\
This builder measures Linux build performance with and without remote caches.<br/>\
The build configs and the bot specs should be in sync with <a href="https://ci.chromium.org/p/chromium/builders/try/linux-rel-compilator">linux-rel-compilator</a>.\
""",
    builderless = True,
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "enable_reclient",
                "chromium_no_telemetry_dependencies",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "mb",
            ],
        ),
    ),
    console_view_entry = consoles.console_view_entry(
        category = "buildperf",
        short_name = "lnx",
    ),
    executable = "recipe:build_perf",
    execution_timeout = 6 * time.hour,
    service_account = "chromium-build-perf-ci-builder@chops-service-accounts.iam.gserviceaccount.com",
    reclient_instance = reclient.instance.DEFAULT_UNTRUSTED,
    reclient_jobs = reclient.jobs.LOW_JOBS_FOR_CQ,
    use_clang_coverage = True,
    # Target luci-chromium-ci-bionic-us-central1-b-ssd-16-*.
    os = os.LINUX_DEFAULT,
    cores = 16,
    ssd = True,
)

ci.builder(
    name = "build-perf-windows",
    description_html = """\
This builder measures Windows build performance with and without remote caches.<br/>\
The build configs and the bot specs should be in sync with <a href="https://ci.chromium.org/p/chromium/builders/try/win10_chromium_x64_rel_ng-compilator">win10_chromium_x64_rel_ng-compilator</a>.\
""",
    builderless = True,
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "enable_reclient",
                "chromium_no_telemetry_dependencies",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "mb",
            ],
        ),
    ),
    console_view_entry = consoles.console_view_entry(
        category = "buildperf",
        short_name = "win",
    ),
    executable = "recipe:build_perf",
    execution_timeout = 6 * time.hour,
    service_account = "chromium-build-perf-ci-builder@chops-service-accounts.iam.gserviceaccount.com",
    reclient_instance = reclient.instance.DEFAULT_UNTRUSTED,
    reclient_jobs = reclient.jobs.HIGH_JOBS_FOR_CQ,
    use_clang_coverage = True,
    # Target luci-chromium-ci-win10-ssd-32-*.
    os = os.WINDOWS_DEFAULT,
    cores = 32,
    ssd = True,
)

ci.builder(
    name = "Linux Builder (j-500) (reclient)",
    console_view_entry = consoles.console_view_entry(
        category = "linux",
        short_name = "re",
    ),
    goma_backend = None,
    reclient_rewrapper_env = {
        "RBE_platform": "container-image=docker://gcr.io/cloud-marketplace/google/rbe-ubuntu16-04@sha256:b4dad0bfc4951d619229ab15343a311f2415a16ef83bcaa55b44f4e2bf1cf635,pool=linux-e2-custom_0",
    },
    reclient_instance = reclient.instance.DEFAULT_TRUSTED,
    reclient_jobs = 500,
    os = os.LINUX_DEFAULT,
    schedule = "triggered",
)

ci.builder(
    name = "Linux Builder (reclient compare)",
    builder_spec = builder_config.copy_from(
        "ci/Linux Builder",
        lambda spec: structs.evolve(
            spec,
            gclient_config = structs.extend(
                spec.gclient_config,
                apply_configs = ["reclient_test"],
            ),
            build_gs_bucket = None,
        ),
    ),
    console_view_entry = consoles.console_view_entry(
        category = "linux",
        short_name = "re",
    ),
    cores = 32,
    goma_backend = None,
    reclient_rewrapper_env = {
        "RBE_compare": "true",
    },
    reclient_instance = reclient.instance.DEFAULT_TRUSTED,
    reclient_ensure_verified = True,
    os = os.LINUX_DEFAULT,
    execution_timeout = 14 * time.hour,
)

# Start - Reclient migration, phase 2, block 1 shadow builders
ci.builder(
    name = "Linux CFI (reclient shadow)",
    console_view_entry = consoles.console_view_entry(
        category = "cfi",
        short_name = "lnx",
    ),
    cores = 32,
    # TODO(thakis): Remove once https://crbug.com/927738 is resolved.
    execution_timeout = 5 * time.hour,
    goma_backend = None,
    os = os.LINUX_DEFAULT,
    reclient_jobs = 400,
    reclient_instance = reclient.instance.DEFAULT_TRUSTED,
)
# End - Reclient migration, phase 2, block 1 shadow builders

ci.builder(
    name = "Win x64 Builder (reclient)",
    builderless = True,
    console_view_entry = consoles.console_view_entry(
        category = "win",
        short_name = "re",
    ),
    cores = 32,
    goma_backend = None,
    reclient_instance = reclient.instance.DEFAULT_TRUSTED,
    os = os.WINDOWS_DEFAULT,
)

ci.builder(
    name = "Win x64 Builder (reclient compare)",
    builderless = True,
    builder_spec = builder_config.builder_spec(
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = ["mb"],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
        ),
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = ["use_clang_coverage", "enable_reclient", "reclient_test"],
        ),
    ),
    console_view_entry = consoles.console_view_entry(
        category = "win",
        short_name = "re",
    ),
    cores = 32,
    goma_backend = None,
    reclient_instance = reclient.instance.DEFAULT_TRUSTED,
    reclient_rewrapper_env = {"RBE_compare": "true"},
    reclient_ensure_verified = True,
    description_html = "verify artifacts. should be removed after the migration. crbug.com/1260232",
    os = os.WINDOWS_DEFAULT,
)

ci.builder(
    name = "Win x64 Builder (reclient)(cross)",
    builderless = True,
    console_view_entry = consoles.console_view_entry(
        category = "win",
        short_name = "re x",
    ),
    cores = 32,
    goma_backend = None,
    reclient_instance = reclient.instance.DEFAULT_TRUSTED,
    reclient_profiler_service = "reclient-win",
    reclient_publish_trace = True,
    os = os.WINDOWS_DEFAULT,
)

ci.builder(
    name = "Win x64 Builder (py2 less)",
    description_html = "This is mirror of <a href=\"https://ci.chromium.org/p/chromium/builders/ci/Win%20x64%20Builder\">Win x64 Builder</a>, but runs on bots not having python2.",
    builder_spec = builder_config.copy_from("ci/Win x64 Builder", lambda spec: structs.evolve(
        spec,
        build_gs_bucket = "chromium-fyi-archive",
    )),
    builderless = True,
    console_view_entry = consoles.console_view_entry(
        category = "win",
        short_name = "py3",
    ),
    cores = 8,
    goma_backend = None,
    reclient_instance = reclient.instance.DEFAULT_TRUSTED,
    os = os.WINDOWS_DEFAULT,
    experiments = {
        "luci.buildbucket.omit_python2": 100,
    },
)

ci.builder(
    name = "Win10 Tests x64 (py2 less)",
    description_html = "This is mirror of <a href=\"https://ci.chromium.org/p/chromium/builders/ci/Win10%20Tests%20x64\">Win10 Tests x64</a>, but runs on bots not having python2.",
    builder_spec = builder_config.copy_from("ci/Win10 Tests x64", lambda spec: structs.evolve(
        spec,
        build_gs_bucket = "chromium-fyi-archive",
    )),
    builderless = True,
    console_view_entry = consoles.console_view_entry(
        category = "win",
        short_name = "py3",
    ),
    os = os.WINDOWS_DEFAULT,
    triggered_by = ["ci/Win x64 Builder"],
    experiments = {
        "luci.buildbucket.omit_python2": 100,
    },
)

fyi_mac_builder(
    name = "Mac Builder (reclient)",
    builderless = True,
    cores = None,  # crbug.com/1245114
    console_view_entry = consoles.console_view_entry(
        category = "mac",
        short_name = "re",
    ),
    goma_backend = None,
    reclient_instance = reclient.instance.DEFAULT_TRUSTED,
    description_html = "experiment reclient on mac. should be removed after the migration. crbug.com/1244441",
)

fyi_mac_builder(
    name = "Mac Builder (reclient compare)",
    builderless = True,
    cores = None,  # crbug.com/1245114
    console_view_entry = consoles.console_view_entry(
        category = "mac",
        short_name = "cmp",
    ),
    goma_backend = None,
    reclient_instance = reclient.instance.DEFAULT_TRUSTED,
    reclient_rewrapper_env = {"RBE_compare": "true"},
    reclient_ensure_verified = True,
    description_html = "verify artifacts. should be removed after the migration. crbug.com/1260232",
    execution_timeout = 14 * time.hour,
)

fyi_mac_builder(
    name = "Mac12 Tests (py2 less)",
    builder_spec = builder_config.copy_from("ci/Mac12 Tests", lambda spec: structs.evolve(
        spec,
        build_gs_bucket = "chromium-fyi-archive",
    )),
    console_view_entry = consoles.console_view_entry(
        category = "mac",
        short_name = "py3",
    ),
    description_html = "This is mirror of <a href=\"https://ci.chromium.org/p/chromium/builders/ci/Mac12%20Tests\">Mac12 Tests</a>, but runs on bots not having python2.",
    experiments = {
        "luci.buildbucket.omit_python2": 100,
    },
    triggered_by = ["ci/Mac Builder"],
)

fyi_mac_builder(
    name = "mac-arm64-on-arm64-rel-reclient",

    # same with mac-arm64-on-arm64-rel
    cores = None,  # crbug.com/1245114
    cpu = cpu.ARM64,
    os = os.MAC_12,
    console_view_entry = consoles.console_view_entry(
        category = "mac",
        short_name = "re",
    ),
    goma_backend = None,
    reclient_instance = reclient.instance.DEFAULT_TRUSTED,
    description_html = "experiment reclient on mac-arm. should be removed after the migration. crbug.com/1252626",
)

ci.builder(
    name = "chromeos-amd64-generic-rel (goma cache silo)",
    console_view_entry = consoles.console_view_entry(
        category = "cros x64",
        short_name = "cgc",
    ),
    os = os.LINUX_DEFAULT,
)

ci.builder(
    name = "chromeos-amd64-generic-rel (reclient)",
    console_view_entry = consoles.console_view_entry(
        category = "cros x64",
    ),
    goma_backend = None,
    reclient_instance = reclient.instance.DEFAULT_TRUSTED,
    os = os.LINUX_DEFAULT,
    reclient_rewrapper_env = {"RBE_cache_silo": "chromeos-amd64-generic-rel (reclient)"},
)

# TODO(crbug.com/1235218): remove after the migration.
ci.builder(
    name = "chromeos-amd64-generic-rel (reclient compare)",
    console_view_entry = consoles.console_view_entry(
        category = "cros x64",
        short_name = "cmp",
    ),
    goma_backend = None,
    reclient_instance = reclient.instance.DEFAULT_TRUSTED,
    os = os.LINUX_DEFAULT,
    reclient_rewrapper_env = {"RBE_compare": "true"},
    reclient_ensure_verified = True,
    description_html = "verify artifacts. should be removed after the migration. crbug.com/1235218",
    execution_timeout = 14 * time.hour,
)

ci.builder(
    name = "lacros-amd64-generic-rel (goma cache silo)",
    console_view_entry = consoles.console_view_entry(
        category = "lacros x64",
        short_name = "cgc",
    ),
    os = os.LINUX_DEFAULT,
)

ci.builder(
    name = "lacros-amd64-generic-rel (reclient)",
    console_view_entry = consoles.console_view_entry(
        category = "lacros x64",
    ),
    goma_backend = None,
    reclient_instance = reclient.instance.DEFAULT_TRUSTED,
    os = os.LINUX_DEFAULT,
    reclient_rewrapper_env = {"RBE_cache_silo": "lacros-amd64-generic-rel (reclient)"},
)

ci.builder(
    name = "linux-lacros-builder-rel (goma cache silo)",
    console_view_entry = consoles.console_view_entry(
        category = "lacros rel",
        short_name = "cgc",
    ),
    os = os.LINUX_DEFAULT,
)

ci.builder(
    name = "linux-lacros-builder-rel (reclient)",
    console_view_entry = consoles.console_view_entry(
        category = "lacros rel",
    ),
    goma_backend = None,
    reclient_instance = reclient.instance.DEFAULT_TRUSTED,
    os = os.LINUX_DEFAULT,
    reclient_rewrapper_env = {"RBE_cache_silo": "linux-lacros-builder-rel (reclient)"},
)

fyi_celab_builder(
    name = "win-celab-builder-rel",
    console_view_entry = consoles.console_view_entry(
        category = "celab",
    ),
    schedule = "0 0,6,12,18 * * *",
    triggered_by = [],
    goma_backend = None,
    reclient_instance = reclient.instance.DEFAULT_TRUSTED,
    reclient_jobs = reclient.jobs.LOW_JOBS_FOR_CI,
)

fyi_celab_builder(
    name = "win-celab-tester-rel",
    console_view_entry = consoles.console_view_entry(
        category = "celab",
    ),
    triggered_by = ["win-celab-builder-rel"],
)

fyi_coverage_builder(
    name = "android-code-coverage",
    console_view_entry = consoles.console_view_entry(
        category = "code_coverage",
        short_name = "and",
    ),
    os = os.LINUX_DEFAULT,
    coverage_test_types = ["overall", "unit"],
    schedule = "triggered",
    triggered_by = [],
    use_java_coverage = True,
    goma_backend = None,
    reclient_jobs = reclient.jobs.HIGH_JOBS_FOR_CI,
    reclient_instance = reclient.instance.DEFAULT_TRUSTED,
)

fyi_coverage_builder(
    name = "android-code-coverage-native",
    console_view_entry = consoles.console_view_entry(
        category = "code_coverage",
        short_name = "ann",
    ),
    os = os.LINUX_DEFAULT,
    use_clang_coverage = True,
    coverage_test_types = ["overall", "unit"],
    goma_backend = None,
    export_coverage_to_zoss = True,
    reclient_jobs = reclient.jobs.HIGH_JOBS_FOR_CI,
    reclient_instance = reclient.instance.DEFAULT_TRUSTED,
)

fyi_coverage_builder(
    name = "fuchsia-code-coverage",
    console_view_entry = [
        consoles.console_view_entry(
            category = "code_coverage",
            short_name = "fsa",
        ),
        consoles.console_view_entry(
            branch_selector = branches.MAIN,
            console_view = "sheriff.fuchsia",
            category = "misc",
            short_name = "cov",
        ),
    ],
    os = os.LINUX_DEFAULT,
    coverage_test_types = ["overall", "unit"],
    use_clang_coverage = True,
    schedule = "triggered",
    triggered_by = [],
)

fyi_coverage_builder(
    name = "ios-simulator-code-coverage",
    console_view_entry = consoles.console_view_entry(
        category = "code_coverage",
        short_name = "ios",
    ),
    cores = None,
    os = os.MAC_DEFAULT,
    use_clang_coverage = True,
    coverage_exclude_sources = "ios_test_files_and_test_utils",
    coverage_test_types = ["overall", "unit"],
    export_coverage_to_zoss = True,
    xcode = xcode.x14main,
)

fyi_coverage_builder(
    name = "linux-chromeos-code-coverage",
    console_view_entry = consoles.console_view_entry(
        category = "code_coverage",
        short_name = "lcr",
    ),
    os = os.LINUX_DEFAULT,
    use_clang_coverage = True,
    coverage_test_types = ["overall", "unit"],
    schedule = "triggered",
    triggered_by = [],
    goma_backend = None,
    reclient_jobs = reclient.jobs.HIGH_JOBS_FOR_CI,
    reclient_instance = reclient.instance.DEFAULT_TRUSTED,
)

fyi_coverage_builder(
    name = "linux-chromeos-js-code-coverage",
    console_view_entry = consoles.console_view_entry(
        category = "code_coverage",
        short_name = "jcr",
    ),
    os = os.LINUX_DEFAULT,
    use_javascript_coverage = True,
    schedule = "triggered",
    triggered_by = [],
    goma_backend = None,
    reclient_jobs = reclient.jobs.HIGH_JOBS_FOR_CI,
    reclient_instance = reclient.instance.DEFAULT_TRUSTED,
)

fyi_coverage_builder(
    name = "linux-code-coverage",
    console_view_entry = consoles.console_view_entry(
        category = "code_coverage",
        short_name = "lnx",
    ),
    os = os.LINUX_DEFAULT,
    use_clang_coverage = True,
    coverage_test_types = ["overall", "unit"],
    export_coverage_to_zoss = True,
    triggered_by = [],
)

fyi_coverage_builder(
    name = "linux-lacros-code-coverage",
    console_view_entry = consoles.console_view_entry(
        category = "code_coverage",
        short_name = "lac",
    ),
    os = os.LINUX_DEFAULT,
    use_clang_coverage = True,
    coverage_test_types = ["overall", "unit"],
    goma_backend = None,
    reclient_jobs = reclient.jobs.HIGH_JOBS_FOR_CI,
    reclient_instance = reclient.instance.DEFAULT_TRUSTED,
)

fyi_coverage_builder(
    name = "mac-code-coverage",
    builderless = True,
    console_view_entry = consoles.console_view_entry(
        category = "code_coverage",
        short_name = "mac",
    ),
    cores = 24,
    os = os.MAC_ANY,
    coverage_test_types = ["overall", "unit"],
    export_coverage_to_zoss = True,
    use_clang_coverage = True,
)

fyi_coverage_builder(
    name = "win10-code-coverage",
    builderless = True,
    console_view_entry = consoles.console_view_entry(
        category = "code_coverage",
        short_name = "win",
    ),
    os = os.WINDOWS_DEFAULT,
    coverage_test_types = ["overall", "unit"],
    use_clang_coverage = True,
    goma_backend = None,
    reclient_jobs = reclient.jobs.DEFAULT,
    reclient_instance = reclient.instance.DEFAULT_TRUSTED,
    export_coverage_to_zoss = True,
)

fyi_ios_builder(
    name = "ios-m1-simulator",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "ios",
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = ["mb", "mac_toolchain"],
            build_config = builder_config.build_config.DEBUG,
            target_bits = 64,
            target_platform = builder_config.target_platform.IOS,
        ),
    ),
    console_view_entry = consoles.console_view_entry(
        category = "iOS|iOSM1",
        short_name = "iosM1",
    ),
    os = os.MAC_DEFAULT,
    cpu = cpu.ARM64,
    schedule = "0 1,5,9,13,17,21 * * *",
    triggered_by = [],
)

fyi_ios_builder(
    name = "ios-reclient",
    console_view_entry = consoles.console_view_entry(
        category = "iOS",
        short_name = "re",
    ),
    # Because of an error in the wrapper function implementation, this value was
    # not modifying the config. The goma property should have no effect if the
    # GN args to use goma isn't set, so commenting this out to avoid modifying
    # the generated config during the freeze.
    # goma_backend = None,
    reclient_instance = reclient.instance.DEFAULT_TRUSTED,
    description_html = "experiment reclient for ios. remove after the migration. crbug.com/1254986",
    builderless = True,
    os = os.MAC_DEFAULT,
)

fyi_ios_builder(
    name = "ios-simulator-cronet",
    branch_selector = branches.STANDARD_MILESTONE,
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "ios",
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
    console_view_entry = consoles.console_view_entry(
        category = "cronet",
        short_name = "intel",
    ),
    cq_mirrors_console_view = "mirrors",
    notifies = ["cronet"],
)

fyi_ios_builder(
    name = "ios-simulator-cronet (reclient shadow)",
    branch_selector = branches.STANDARD_MILESTONE,
    builder_spec = builder_config.copy_from(
        "ci/ios-simulator-cronet",
    ),
    console_view_entry = consoles.console_view_entry(
        category = "cronet",
        short_name = "rec",
    ),
    cq_mirrors_console_view = "mirrors",
    builderless = True,
    goma_backend = None,
    reclient_instance = reclient.instance.DEFAULT_TRUSTED,
    reclient_jobs = 40,
)

fyi_ios_builder(
    name = "ios-m1-simulator-cronet",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "ios",
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = ["mb", "mac_toolchain"],
            build_config = builder_config.build_config.DEBUG,
            target_bits = 64,
            target_platform = builder_config.target_platform.IOS,
        ),
    ),
    console_view_entry = consoles.console_view_entry(
        category = "cronet",
        short_name = "m1",
    ),
    os = os.MAC_12,
    cpu = cpu.ARM64,
    schedule = "0 1,5,9,13,17,21 * * *",
)

fyi_ios_builder(
    name = "ios-simulator-multi-window",
    console_view_entry = consoles.console_view_entry(
        category = "iOS",
        short_name = "mwd",
    ),
)

fyi_ios_builder(
    name = "ios-webkit-tot",
    console_view_entry = consoles.console_view_entry(
        category = "iOS",
        short_name = "wk",
    ),
    schedule = "0 1-23/6 * * *",
    triggered_by = [],
    xcode = xcode.x13wk,
)

fyi_ios_builder(
    name = "ios15-beta-simulator",
    console_view_entry = [
        consoles.console_view_entry(
            category = "iOS|iOS15",
            short_name = "ios15",
        ),
    ],
    os = os.MAC_12,
)

fyi_ios_builder(
    name = "ios15-sdk-device",
    console_view_entry = [
        consoles.console_view_entry(
            category = "iOS|iOS15",
            short_name = "dev",
        ),
    ],
    os = os.MAC_12,
)

fyi_ios_builder(
    name = "ios15-sdk-simulator",
    console_view_entry = [
        consoles.console_view_entry(
            category = "iOS|iOS15",
            short_name = "sdk15",
        ),
    ],
    os = os.MAC_12,
)

fyi_ios_builder(
    name = "ios16-beta-simulator",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "ios",
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
    console_view_entry = consoles.console_view_entry(
        category = "iOS|iOS16",
        short_name = "ios16",
    ),
    os = os.MAC_DEFAULT,
    schedule = "0 0,4,8,12,16,20 * * *",
    triggered_by = [],
)

fyi_ios_builder(
    name = "ios16-sdk-simulator",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "ios",
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
    console_view_entry = consoles.console_view_entry(
        category = "iOS|iOS16",
        short_name = "sdk16",
    ),
    os = os.MAC_DEFAULT,
    schedule = "0 2,6,10,14,18,22 * * *",
    triggered_by = [],
    xcode = xcode.x14betabots,
)

ci.builder(
    # An FYI version of the following builders that runs on Focal:
    # https://ci.chromium.org/p/chromium/builders/ci/Linux%20MSan%20Builder
    # https://ci.chromium.org/p/chromium/builders/ci/Linux%20MSan%20Tests
    # TODO(crbug.com/1260217): Remove this builder when the main MSAN builder
    # has migrated to focal.
    name = "Linux MSan Focal",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium_msan",
            apply_configs = ["mb"],
            build_config = builder_config.build_config.RELEASE,
        ),
    ),
    console_view_entry = consoles.console_view_entry(
        category = "msan",
        short_name = "lin",
    ),
    goma_backend = None,
    reclient_jobs = reclient.jobs.HIGH_JOBS_FOR_CI,
    reclient_instance = reclient.instance.DEFAULT_TRUSTED,
    os = os.LINUX_FOCAL,
    execution_timeout = 16 * time.hour,
)

ci.builder(
    # An FYI version of the following builders that runs on Focal:
    # https://ci.chromium.org/p/chromium/builders/ci/Linux%20ChromiumOS%20MSan%20Builder
    # https://ci.chromium.org/p/chromium/builders/ci/Linux%20ChromiumOS%20MSan%20Tests
    # TODO(crbug.com/1260217): Remove this builder when the main MSAN builder
    # has migrated to focal.
    name = "Linux ChromiumOS MSan Focal",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = ["chromeos"],
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium_msan",
            apply_configs = ["mb"],
            build_config = builder_config.build_config.RELEASE,
        ),
    ),
    console_view_entry = consoles.console_view_entry(
        category = "msan",
        short_name = "crs",
    ),
    goma_jobs = goma.jobs.MANY_JOBS_FOR_CI,
    os = os.LINUX_FOCAL,
    execution_timeout = 16 * time.hour,
)

fyi_mac_builder(
    name = "Mac Builder Next",
    console_view_entry = consoles.console_view_entry(
        category = "mac",
        short_name = "bld",
    ),
    cores = None,
    os = None,
)

fyi_mac_builder(
    name = "Mac deterministic",
    console_view_entry = consoles.console_view_entry(
        category = "deterministic|mac",
        short_name = "rel",
    ),
    cores = None,
    executable = "recipe:swarming/deterministic_build",
    execution_timeout = 6 * time.hour,
)

fyi_mac_builder(
    name = "Mac deterministic (dbg)",
    console_view_entry = consoles.console_view_entry(
        category = "deterministic|mac",
        short_name = "dbg",
    ),
    cores = None,
    executable = "recipe:swarming/deterministic_build",
    execution_timeout = 6 * time.hour,
    os = os.MAC_DEFAULT,
)

fyi_mac_builder(
    name = "Mac deterministic (reclient shadow)",
    console_view_entry = consoles.console_view_entry(
        category = "deterministic|mac",
        short_name = "rec",
    ),
    cores = None,
    executable = "recipe:swarming/deterministic_build",
    execution_timeout = 6 * time.hour,
    builderless = True,
    goma_backend = None,
    reclient_instance = reclient.instance.DEFAULT_TRUSTED,
    reclient_jobs = 40,
)

fyi_mac_builder(
    name = "mac-hermetic-upgrade-rel",
    console_view_entry = consoles.console_view_entry(
        category = "mac",
        short_name = "herm",
    ),
    cores = 12,
)

ci.builder(
    name = "Win 10 Fast Ring",
    console_view_entry = consoles.console_view_entry(
        category = "win10",
    ),
    os = os.WINDOWS_10,
    notifies = ["Win 10 Fast Ring"],
    goma_backend = None,
    reclient_jobs = reclient.jobs.LOW_JOBS_FOR_CI,
    reclient_instance = reclient.instance.DEFAULT_TRUSTED,
)

ci.builder(
    name = "win32-arm64-rel",
    console_view_entry = consoles.console_view_entry(
        category = "win32|arm64",
    ),
    cpu = cpu.X86,
    os = os.WINDOWS_DEFAULT,
    goma_backend = None,
    reclient_jobs = 150,
    reclient_instance = reclient.instance.DEFAULT_TRUSTED,
)

ci.builder(
    name = "win-fieldtrial-rel",
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
        build_gs_bucket = "chromium-fyi-archive",
    ),
    console_view_entry = consoles.console_view_entry(
        category = "win",
    ),
    os = os.WINDOWS_DEFAULT,
    reclient_jobs = reclient.jobs.LOW_JOBS_FOR_CI,
    reclient_instance = reclient.instance.DEFAULT_TRUSTED,
)

ci.builder(
    name = "win-annotator-rel",
    builderless = True,
    console_view_entry = consoles.console_view_entry(
        category = "network|traffic|annotations",
        short_name = "win",
    ),
    execution_timeout = 16 * time.hour,
    notifies = ["annotator-rel"],
    os = os.WINDOWS_DEFAULT,
    goma_backend = None,
    reclient_jobs = reclient.jobs.LOW_JOBS_FOR_CI,
    reclient_instance = reclient.instance.DEFAULT_TRUSTED,
)
