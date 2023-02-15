# Copyright 2021 The Chromium Authors
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
    executable = ci.DEFAULT_EXECUTABLE,
    builder_group = "chromium.fyi",
    pool = ci.DEFAULT_POOL,
    cores = 8,
    execution_timeout = 10 * time.hour,
    goma_backend = goma.backend.RBE_PROD,
    priority = ci.DEFAULT_FYI_PRIORITY,
    service_account = ci.DEFAULT_SERVICE_ACCOUNT,
)

consoles.console_view(
    name = "chromium.fyi",
    branch_selector = [
        branches.selector.IOS_BRANCHES,
        branches.selector.LINUX_BRANCHES,
    ],
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
    os = os.LINUX_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "viz",
    ),
    goma_backend = None,
    reclient_instance = reclient.instance.DEFAULT_TRUSTED,
    reclient_jobs = reclient.jobs.DEFAULT,
)

ci.builder(
    name = "Site Isolation Android",
    os = os.LINUX_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "site_isolation",
    ),
    goma_backend = None,
    notifies = ["Site Isolation Android"],
    reclient_instance = reclient.instance.DEFAULT_TRUSTED,
    reclient_jobs = reclient.jobs.DEFAULT,
)

ci.builder(
    name = "VR Linux",
    branch_selector = branches.selector.LINUX_BRANCHES,
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
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
        build_gs_bucket = "chromium-fyi-archive",
        test_results_config = builder_config.test_results_config(
            config = "staging_server",
        ),
    ),
    os = os.LINUX_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "linux",
    ),
    cq_mirrors_console_view = "mirrors",
    goma_backend = None,
    reclient_instance = reclient.instance.DEFAULT_TRUSTED,
    reclient_jobs = reclient.jobs.HIGH_JOBS_FOR_CI,
)

ci.builder(
    name = "android-backuprefptr-arm-fyi-rel",
    builderless = True,
    os = os.LINUX_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "backuprefptr|android",
        short_name = "32rel",
    ),
    goma_backend = None,
    notifies = ["chrome-memory-safety"],
    reclient_instance = reclient.instance.DEFAULT_TRUSTED,
    reclient_jobs = reclient.jobs.DEFAULT,
)

ci.builder(
    name = "android-backuprefptr-arm64-fyi-rel",
    builderless = True,
    os = os.LINUX_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "backuprefptr|android",
        short_name = "64rel",
    ),
    goma_backend = None,
    notifies = ["chrome-memory-safety"],
    reclient_instance = reclient.instance.DEFAULT_TRUSTED,
    reclient_jobs = reclient.jobs.DEFAULT,
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
    os = os.LINUX_DEFAULT,
    console_view_entry = [
        consoles.console_view_entry(
            category = "fuchsia|x64",
            short_name = "cfv2",
        ),
        consoles.console_view_entry(
            branch_selector = branches.selector.MAIN,
            console_view = "sheriff.fuchsia",
            category = "fyi|x64",
            short_name = "cfv2",
        ),
    ],
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
    os = os.LINUX_DEFAULT,
    console_view_entry = [
        consoles.console_view_entry(
            category = "fuchsia|arm64",
            short_name = "cfv2",
        ),
        consoles.console_view_entry(
            branch_selector = branches.selector.MAIN,
            console_view = "sheriff.fuchsia",
            category = "fyi|arm64",
            short_name = "cfv2",
        ),
    ],
)

ci.builder(
    name = "lacros-amd64-generic-rel-fyi",
    os = os.LINUX_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "lacros",
        short_name = "lcr",
    ),
    goma_backend = None,
    reclient_instance = reclient.instance.DEFAULT_TRUSTED,
    reclient_jobs = reclient.jobs.HIGH_JOBS_FOR_CI,
)

ci.builder(
    name = "lacros-amd64-generic-rel-skylab-fyi",
    # Some tests on this bot depend on being unauthenticated with GS, so
    # don't run the tests inside a luci-auth context to avoid having the
    # BOTO config setup for the task's service account.
    # TODO(crbug.com/1217155): Fix this.
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "chromeos",
                "checkout_lacros_sdk",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = ["mb", "mb_no_luci_auth"],
            target_bits = 64,
            target_platform = "chromeos",
            target_cros_boards = "eve",
            cros_boards_with_qemu_images = "amd64-generic",
        ),
        build_gs_bucket = "chromium-fyi-archive",
        skylab_upload_location = builder_config.skylab_upload_location(
            gs_bucket = "lacros-amd64-generic-rel-skylab-try",
        ),
        test_results_config = builder_config.test_results_config(
            config = "staging_server",
        ),
    ),
    os = os.LINUX_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "lacros",
        short_name = "lsf",
    ),
)

ci.builder(
    name = "lacros-arm64-generic-rel-skylab-fyi",
    # Some tests on this bot depend on being unauthenticated with GS, so
    # don't run the tests inside a luci-auth context to avoid having the
    # BOTO config setup for the task's service account.
    # TODO(crbug.com/1217155): Fix this.
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "chromeos",
                "checkout_lacros_sdk",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = ["mb", "mb_no_luci_auth"],
            target_bits = 64,
            target_platform = "chromeos",
            target_cros_boards = "kevin",
            cros_boards_with_qemu_images = "arm64-generic",
        ),
        build_gs_bucket = "chromium-fyi-archive",
        skylab_upload_location = builder_config.skylab_upload_location(
            gs_bucket = "lacros-arm64-generic-rel-skylab-try",
        ),
        test_results_config = builder_config.test_results_config(
            config = "staging_server",
        ),
    ),
    os = os.LINUX_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "lacros",
        short_name = "larsf",
    ),
)

ci.builder(
    name = "linux-annotator-rel",
    os = os.LINUX_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "network|traffic|annotations",
        short_name = "lnx",
    ),
    goma_backend = None,
    notifies = ["annotator-rel"],
    reclient_instance = reclient.instance.DEFAULT_TRUSTED,
    reclient_jobs = reclient.jobs.HIGH_JOBS_FOR_CI,
)

ci.builder(
    name = "linux-chromeos-annotator-rel",
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
        ),
        build_gs_bucket = "chromium-fyi-archive",
        test_results_config = builder_config.test_results_config(
            config = "staging_server",
        ),
    ),
    builderless = True,
    os = os.LINUX_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "release",
        short_name = "rel",
    ),
    execution_timeout = 3 * time.hour,
    goma_backend = None,
    reclient_instance = reclient.instance.DEFAULT_TRUSTED,
    reclient_jobs = reclient.jobs.HIGH_JOBS_FOR_CI,
)

ci.builder(
    name = "linux-ash-chromium-builder-fyi-rel",
    os = os.LINUX_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "default",
        short_name = "lcr",
    ),
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
    name = "linux-lacros-tester-rel-reviver",
    # To avoid peak hours, we run it from 8PM TO 4AM PST. It is
    # 3 AM to 11 AM UTC.
    schedule = "0 3,5,7,9 * * *",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium_no_telemetry_dependencies",
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
            target_arch = builder_config.target_arch.INTEL,
            target_bits = 64,
        ),
        build_gs_bucket = "chromium-chromiumos-archive",
    ),
    os = os.LINUX_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "default",
        short_name = "rev",
    ),
)

ci.builder(
    name = "linux-lacros-version-skew-fyi",
    os = os.LINUX_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "default",
    ),
    goma_backend = None,
    reclient_instance = reclient.instance.DEFAULT_TRUSTED,
    reclient_jobs = reclient.jobs.HIGH_JOBS_FOR_CI,
)

ci.builder(
    name = "linux-blink-wpt-reset-rel",
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
        ),
    ),
    os = os.LINUX_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "linux|blink",
        short_name = "BIr",
    ),
    goma_backend = None,
    reclient_instance = reclient.instance.DEFAULT_TRUSTED,
    reclient_jobs = reclient.jobs.DEFAULT,
)

ci.builder(
    name = "linux-blink-animation-use-time-delta",
    os = os.LINUX_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "linux|blink",
        short_name = "TD",
    ),
)

ci.builder(
    name = "linux-blink-heap-verification",
    os = os.LINUX_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "linux|blink",
        short_name = "VF",
    ),
    goma_backend = None,
    notifies = ["linux-blink-fyi-bots"],
    reclient_instance = reclient.instance.DEFAULT_TRUSTED,
    reclient_jobs = reclient.jobs.HIGH_JOBS_FOR_CI,
)

ci.builder(
    name = "linux-blink-v8-sandbox-future-rel",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = ["mb"],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
        ),
    ),
    os = os.LINUX_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "linux|blink",
        short_name = "SB",
    ),
    goma_backend = None,
    notifies = ["v8-sandbox-fyi-bots"],
    reclient_instance = reclient.instance.DEFAULT_TRUSTED,
    reclient_jobs = reclient.jobs.DEFAULT,
)

ci.builder(
    name = "linux-example-builder",
    schedule = "with 12h interval",
    triggered_by = [],
    os = os.LINUX_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "linux",
    ),
    goma_backend = None,
    reclient_instance = reclient.instance.DEFAULT_TRUSTED,
    reclient_jobs = reclient.jobs.DEFAULT,
)

ci.builder(
    name = "linux-fieldtrial-rel",
    os = os.LINUX_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "linux",
    ),
)

ci.builder(
    name = "mac-fieldtrial-rel",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = ["mb"],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
        ),
    ),
    builderless = False,
    cores = None,
    os = os.MAC_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "mac",
    ),
)

ci.builder(
    name = "android-fieldtrial-rel",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "android",
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
    builderless = False,
    os = os.LINUX_BIONIC,
    console_view_entry = consoles.console_view_entry(
        category = "android",
    ),
    reclient_instance = reclient.instance.DEFAULT_TRUSTED,
    reclient_jobs = reclient.jobs.DEFAULT,
)

fyi_ios_builder(
    name = "ios-fieldtrial-rel",
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
            target_platform = "ios",
        ),
    ),
    builderless = False,
    console_view_entry = consoles.console_view_entry(
        category = "mac",
    ),
)

ci.builder(
    name = "linux-lacros-builder-fyi-rel",
    os = os.LINUX_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "linux",
    ),
    goma_backend = None,
    reclient_instance = reclient.instance.DEFAULT_TRUSTED,
    reclient_jobs = reclient.jobs.HIGH_JOBS_FOR_CI,
)

ci.thin_tester(
    name = "linux-lacros-tester-fyi-rel",
    triggered_by = ["linux-lacros-builder-fyi-rel"],
    console_view_entry = consoles.console_view_entry(
        category = "linux",
    ),
)

ci.builder(
    name = "linux-lacros-dbg-fyi",
    os = os.LINUX_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "linux",
    ),
    goma_backend = None,
    reclient_instance = reclient.instance.DEFAULT_TRUSTED,
    reclient_jobs = reclient.jobs.HIGH_JOBS_FOR_CI,
)

ci.thin_tester(
    name = "linux-lacros-dbg-tests-fyi",
    triggered_by = ["linux-lacros-dbg-fyi"],
    console_view_entry = consoles.console_view_entry(
        category = "linux",
    ),
)

ci.builder(
    name = "linux-backuprefptr-x64-fyi-rel",
    builderless = True,
    os = os.LINUX_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "backuprefptr|linux",
        short_name = "64rel",
    ),
    goma_backend = None,
    notifies = ["chrome-memory-safety"],
    reclient_instance = reclient.instance.DEFAULT_TRUSTED,
    reclient_jobs = reclient.jobs.DEFAULT,
)

ci.builder(
    name = "android-perfetto-rel",
    schedule = "triggered",
    triggered_by = [],
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = ["mb"],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
        ),
    ),
    builderless = True,
    os = os.LINUX_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "android",
    ),
    goma_backend = None,
    reclient_instance = reclient.instance.DEFAULT_TRUSTED,
    reclient_jobs = reclient.jobs.DEFAULT,
)

ci.builder(
    name = "linux-perfetto-rel",
    os = os.LINUX_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "linux",
    ),
    goma_backend = None,
    reclient_instance = reclient.instance.DEFAULT_TRUSTED,
    reclient_jobs = reclient.jobs.DEFAULT,
)

ci.builder(
    name = "mac-perfetto-rel",
    schedule = "triggered",
    triggered_by = [],
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = ["mb"],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
        ),
    ),
    builderless = True,
    os = os.MAC_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "mac",
    ),
    goma_backend = None,
    reclient_instance = reclient.instance.DEFAULT_TRUSTED,
    reclient_jobs = reclient.jobs.DEFAULT,
)

ci.builder(
    name = "linux-wpt-content-shell-fyi-rel",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = ["mb"],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
        ),
    ),
    os = os.LINUX_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "linux",
    ),
    experimental = True,
    goma_backend = None,
    reclient_instance = reclient.instance.DEFAULT_TRUSTED,
    reclient_jobs = reclient.jobs.DEFAULT,
)

ci.builder(
    name = "linux-wpt-fyi-rel",
    os = os.LINUX_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "linux",
    ),
    experimental = True,
    goma_backend = None,
    reclient_instance = reclient.instance.DEFAULT_TRUSTED,
    reclient_jobs = reclient.jobs.DEFAULT,
)

ci.builder(
    name = "linux-wpt-identity-fyi-rel",
    os = os.LINUX_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "linux",
    ),
    experimental = True,
    goma_backend = None,
    reclient_instance = reclient.instance.DEFAULT_TRUSTED,
    reclient_jobs = reclient.jobs.DEFAULT,
)

ci.builder(
    name = "linux-wpt-input-fyi-rel",
    os = os.LINUX_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "linux",
    ),
    experimental = True,
    goma_backend = None,
    reclient_instance = reclient.instance.DEFAULT_TRUSTED,
    reclient_jobs = reclient.jobs.DEFAULT,
)

fyi_ios_builder(
    name = "ios-wpt-fyi-rel",
    # TODO(crbug.com/1351820): Enable scheduler when machine has been allocated.
    schedule = "triggered",
    triggered_by = [],
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
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
            target_platform = "ios",
        ),
    ),
    builderless = False,
    console_view_entry = consoles.console_view_entry(
        category = "mac",
    ),
)

# This is launching & collecting entirely isolated tests.
# OS shouldn't matter.
ci.thin_tester(
    name = "mac-osxbeta-rel",
    triggered_by = ["ci/Mac Builder (dbg)"],
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
        build_gs_bucket = "chromium-fyi-archive",
        test_results_config = builder_config.test_results_config(
            config = "staging_server",
        ),
    ),
    builderless = False,
    cores = 12,
    os = os.MAC_13,
    console_view_entry = consoles.console_view_entry(
        category = "mac",
        short_name = "beta",
    ),
    main_console_view = None,
)

ci.builder(
    name = "linux-headless-shell-rel",
    os = os.LINUX_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "linux",
        short_name = "hdls",
    ),
    goma_backend = None,
    notifies = ["headless-owners"],
    reclient_instance = reclient.instance.DEFAULT_TRUSTED,
    reclient_jobs = reclient.jobs.DEFAULT,
)

# TODO(crbug.com/1320004): Remove this builder after experimentation.
ci.builder(
    name = "linux-rel-no-external-ip",
    builder_spec = builder_config.copy_from(
        "ci/Linux Builder",
    ),
    builderless = False,
    os = os.LINUX_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "linux",
    ),
    # Limited test pool is likely to cause long build times.
    execution_timeout = 24 * time.hour,
    goma_backend = None,
    reclient_instance = reclient.instance.DEFAULT_TRUSTED,
    reclient_jobs = reclient.jobs.DEFAULT,
)

ci.builder(
    name = "mac-backuprefptr-x64-fyi-rel",
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
        build_gs_bucket = "chromium-fyi-archive",
        test_results_config = builder_config.test_results_config(
            config = "staging_server",
        ),
    ),
    builderless = True,
    os = os.MAC_ANY,
    console_view_entry = consoles.console_view_entry(
        category = "backuprefptr|mac",
        short_name = "64rel",
    ),
    goma_backend = None,
    notifies = ["chrome-memory-safety"],
    reclient_instance = reclient.instance.DEFAULT_TRUSTED,
    reclient_jobs = reclient.jobs.DEFAULT,
)

ci.builder(
    name = "win-backuprefptr-x86-fyi-rel",
    builderless = True,
    os = os.WINDOWS_ANY,
    console_view_entry = consoles.console_view_entry(
        category = "backuprefptr|win",
        short_name = "32rel",
    ),
    goma_backend = None,
    notifies = ["chrome-memory-safety"],
    reclient_instance = reclient.instance.DEFAULT_TRUSTED,
    reclient_jobs = reclient.jobs.LOW_JOBS_FOR_CI,
)

ci.builder(
    name = "win-backuprefptr-x64-fyi-rel",
    builderless = True,
    os = os.WINDOWS_ANY,
    console_view_entry = consoles.console_view_entry(
        category = "backuprefptr|win",
        short_name = "64rel",
    ),
    goma_backend = None,
    notifies = ["chrome-memory-safety"],
    reclient_instance = reclient.instance.DEFAULT_TRUSTED,
    reclient_jobs = reclient.jobs.LOW_JOBS_FOR_CI,
)

ci.builder(
    name = "win-perfetto-rel",
    schedule = "triggered",
    triggered_by = [],
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = ["mb"],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
        ),
    ),
    builderless = True,
    os = os.WINDOWS_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "win",
    ),
    goma_backend = None,
    reclient_instance = reclient.instance.DEFAULT_TRUSTED,
    reclient_jobs = reclient.jobs.DEFAULT,
)

# TODO(crbug.com/1320004): Remove this builder after experimentation.
ci.builder(
    name = "win10-rel-no-external-ip",
    builder_spec = builder_config.copy_from(
        "ci/Win x64 Builder",
    ),
    builderless = False,
    os = os.WINDOWS_ANY,
    console_view_entry = consoles.console_view_entry(
        category = "win",
    ),
    # Limited test pool is likely to cause long build times.
    execution_timeout = 24 * time.hour,
    goma_backend = None,
    reclient_instance = reclient.instance.DEFAULT_TRUSTED,
    reclient_jobs = reclient.jobs.DEFAULT,
)

ci.builder(
    name = "linux-upload-perfetto",
    os = os.LINUX_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "perfetto",
        short_name = "lnx",
    ),
    goma_backend = None,
    reclient_instance = reclient.instance.DEFAULT_TRUSTED,
    reclient_jobs = reclient.jobs.DEFAULT,
)

ci.builder(
    name = "mac-upload-perfetto",
    schedule = "with 3h interval",
    triggered_by = [],
    builderless = True,
    cores = None,
    os = os.MAC_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "perfetto",
        short_name = "mac",
    ),
    goma_backend = None,
    reclient_instance = reclient.instance.DEFAULT_TRUSTED,
    reclient_jobs = reclient.jobs.DEFAULT,
)

ci.builder(
    name = "win-upload-perfetto",
    schedule = "with 3h interval",
    triggered_by = [],
    builderless = True,
    os = os.WINDOWS_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "perfetto",
        short_name = "win",
    ),
    goma_backend = None,
    reclient_instance = reclient.instance.DEFAULT_TRUSTED,
    reclient_jobs = reclient.jobs.LOW_JOBS_FOR_CI,
)

ci.builder(
    name = "Comparison Android (reclient)",
    description_html = """\
This builder measures Android build performance with goma vs reclient.<br/>\
The bot specs should be in sync with <a href="https://ci.chromium.org/p/chromium/builders/ci/Deterministic%20Android%20(dbg)">Deterministic Android (dbg)</a>.\
""",
    executable = "recipe:reclient_goma_comparison",
    cores = 16,
    os = os.LINUX_DEFAULT,
    # Target luci-chromium-ci-bionic-us-central1-b-ssd-16-*.
    ssd = True,
    console_view_entry = consoles.console_view_entry(
        category = "android",
        short_name = "cmp",
    ),
    execution_timeout = 15 * time.hour,
    goma_jobs = 250,
    reclient_cache_silo = "Comparison Android - cache siloed",
    reclient_instance = reclient.instance.DEFAULT_TRUSTED,
    reclient_jobs = 250,
)

ci.builder(
    name = "Comparison Android (reclient) (reproxy cache)",
    description_html = """\
This builder measures Android build performance with goma vs reclient using reproxy's deps cache.<br/>\
The bot specs should be in sync with <a href="https://ci.chromium.org/p/chromium/builders/ci/Comparison%20Android%20(reclient)">Comparison Android (reclient)</a>.\
""",
    executable = "recipe:reclient_goma_comparison",
    cores = 16,
    os = os.LINUX_DEFAULT,
    # Target luci-chromium-ci-bionic-us-central1-b-ssd-16-*.
    ssd = True,
    console_view_entry = consoles.console_view_entry(
        category = "android|expcache",
        short_name = "cmp",
    ),
    execution_timeout = 15 * time.hour,
    goma_jobs = 250,
    reclient_bootstrap_env = {
        "RBE_experimental_goma_deps_cache": "true",
    },
    reclient_cache_silo = "Comparison Android (reproxy cache) - cache siloed",
    reclient_instance = reclient.instance.DEFAULT_TRUSTED,
    reclient_jobs = 250,
)

ci.builder(
    name = "Comparison Linux (reclient)",
    executable = "recipe:reclient_goma_comparison",
    os = os.LINUX_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "linux",
        short_name = "cmp",
    ),
    execution_timeout = 6 * time.hour,
    goma_jobs = 250,
    reclient_cache_silo = "Comparison Linux - cache siloed",
    reclient_instance = reclient.instance.DEFAULT_TRUSTED,
    reclient_jobs = 250,
)

ci.builder(
    name = "Comparison Mac (reclient)",
    executable = "recipe:reclient_goma_comparison",
    builderless = True,
    cores = None,
    os = os.MAC_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "mac",
        short_name = "cmp",
    ),
    execution_timeout = 10 * time.hour,
    goma_jobs = 250,
    reclient_bootstrap_env = {
        "GLOG_vmodule": "bridge*=2",
    },
    reclient_cache_silo = "Comparison Mac - cache siloed",
    reclient_instance = reclient.instance.TEST_TRUSTED,
    reclient_jobs = 250,
)

ci.builder(
    name = "Comparison Mac arm64 (reclient)",
    executable = "recipe:reclient_goma_comparison",
    builderless = True,
    cores = None,
    os = os.MAC_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "mac",
        short_name = "cmp",
    ),
    execution_timeout = 10 * time.hour,
    goma_jobs = 250,
    reclient_bootstrap_env = {
        "GLOG_vmodule": "bridge*=2",
    },
    reclient_cache_silo = "Comparison Mac - cache siloed",
    reclient_instance = reclient.instance.TEST_TRUSTED,
    reclient_jobs = 250,
)

ci.builder(
    name = "Comparison Mac arm64 on arm64 (reclient)",
    executable = "recipe:reclient_goma_comparison",
    builderless = True,
    cores = None,
    os = os.MAC_DEFAULT,
    cpu = cpu.ARM64,
    console_view_entry = consoles.console_view_entry(
        category = "mac",
        short_name = "cmp",
    ),
    execution_timeout = 10 * time.hour,
    goma_jobs = 250,
    reclient_bootstrap_env = {
        "GLOG_vmodule": "bridge*=2",
    },
    reclient_cache_silo = "Comparison Mac - cache siloed",
    reclient_instance = reclient.instance.TEST_TRUSTED,
    reclient_jobs = 250,
)

ci.builder(
    name = "Comparison Windows (8 cores) (reclient)",
    executable = "recipe:reclient_goma_comparison",
    builderless = True,
    cores = 8,
    os = os.WINDOWS_DEFAULT,
    free_space = builders.free_space.high,
    console_view_entry = consoles.console_view_entry(
        category = "win",
        short_name = "re",
    ),
    goma_jobs = 80,
    reclient_cache_silo = "Comparison Windows 8 cores - cache siloed",
    reclient_instance = reclient.instance.DEFAULT_TRUSTED,
    reclient_jobs = 80,
)

ci.builder(
    name = "Comparison Windows (reclient)",
    executable = "recipe:reclient_goma_comparison",
    builderless = True,
    cores = 32,
    os = os.WINDOWS_DEFAULT,
    free_space = builders.free_space.high,
    console_view_entry = consoles.console_view_entry(
        category = "win",
        short_name = "re",
    ),
    execution_timeout = 6 * time.hour,
    goma_jobs = 250,
    reclient_cache_silo = "Comparison Windows - cache siloed",
    reclient_instance = reclient.instance.DEFAULT_TRUSTED,
    reclient_jobs = 250,
)

ci.builder(
    name = "Comparison Windows (reclient) (reproxy cache)",
    executable = "recipe:reclient_goma_comparison",
    builderless = True,
    cores = 32,
    os = os.WINDOWS_DEFAULT,
    free_space = builders.free_space.high,
    console_view_entry = consoles.console_view_entry(
        category = "win|expcache",
        short_name = "re",
    ),
    execution_timeout = 6 * time.hour,
    goma_jobs = 250,
    reclient_bootstrap_env = {
        "RBE_experimental_goma_deps_cache": "true",
    },
    reclient_cache_silo = "Comparison Windows (reproxy cache) - cache siloed",
    reclient_instance = reclient.instance.DEFAULT_TRUSTED,
    reclient_jobs = 250,
)

ci.builder(
    name = "Comparison Simple Chrome (reclient)",
    executable = "recipe:reclient_goma_comparison",
    builderless = True,
    os = os.LINUX_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "cros x64",
        short_name = "cmp",
    ),
    execution_timeout = 10 * time.hour,
    goma_jobs = 250,
    reclient_cache_silo = "Comparison Simple Chrome - cache siloed",
    reclient_instance = reclient.instance.DEFAULT_TRUSTED,
    reclient_jobs = 250,
)

ci.builder(
    name = "Comparison ios (reclient)",
    executable = "recipe:reclient_goma_comparison",
    builderless = True,
    cores = None,
    os = os.MAC_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "ios",
        short_name = "cmp",
    ),
    execution_timeout = 10 * time.hour,
    goma_jobs = 250,
    reclient_cache_silo = "Comparison ios - cache siloed",
    reclient_instance = reclient.instance.TEST_TRUSTED,
    reclient_jobs = 250,
    xcode = xcode.x14main,
)

ci.builder(
    name = "Comparison Android (reclient)(CQ)",
    description_html = """\
This builder measures Android build performance with goma vs reclient in cq configuration.<br/>\
The bot specs should be in sync with <a href="https://ci.chromium.org/p/chromium/builders/try/android-pie-arm64-rel-compilator">android-pie-arm64-rel-compilator</a>.\
""",
    executable = "recipe:reclient_goma_comparison",
    cores = 32,
    os = os.LINUX_DEFAULT,
    ssd = True,
    console_view_entry = consoles.console_view_entry(
        category = "android|cq",
        short_name = "cmp",
    ),
    execution_timeout = 15 * time.hour,
    goma_jobs = goma.jobs.J300,
    reclient_cache_silo = "Comparison Android CQ - cache siloed",
    reclient_instance = reclient.instance.TEST_UNTRUSTED,
    reclient_jobs = 300,
)

ci.builder(
    name = "Comparison Linux (reclient)(CQ)",
    description_html = """\
This builder measures Linux build performance with goma vs reclient in cq configuration.<br/>\
The bot specs should be in sync with <a href="https://ci.chromium.org/p/chromium/builders/try/linux-rel-compilator">linux-rel-compilator</a>.\
""",
    executable = "recipe:reclient_goma_comparison",
    cores = 16,
    os = os.LINUX_DEFAULT,
    ssd = True,
    console_view_entry = consoles.console_view_entry(
        category = "linux|cq",
        short_name = "cmp",
    ),
    execution_timeout = 6 * time.hour,
    goma_jobs = 150,
    reclient_cache_silo = "Comparison Linux CQ - cache siloed",
    reclient_instance = reclient.instance.TEST_UNTRUSTED,
    reclient_jobs = 150,
)

ci.builder(
    name = "Comparison Mac (reclient)(CQ)",
    description_html = """\
This builder measures Mac build performance with goma vs reclient in cq configuration.<br/>\
The bot specs should be in sync with <a href="https://ci.chromium.org/p/chromium/builders/try/mac-rel-compilator">mac-rel-compilator</a>.\
""",
    executable = "recipe:reclient_goma_comparison",
    builderless = True,
    cores = None,
    os = os.MAC_DEFAULT,
    ssd = True,
    console_view_entry = consoles.console_view_entry(
        category = "mac|cq",
        short_name = "cmp",
    ),
    execution_timeout = 10 * time.hour,
    goma_jobs = 150,
    reclient_bootstrap_env = {
        "GLOG_vmodule": "bridge*=2",
    },
    reclient_cache_silo = "Comparison Mac CQ - cache siloed",
    reclient_instance = reclient.instance.TEST_UNTRUSTED,
    reclient_jobs = 150,
)

ci.builder(
    name = "Comparison Windows (reclient)(CQ)",
    description_html = """\
This builder measures Windows build performance with goma vs reclient in cq configuration.<br/>\
The bot specs should be in sync with <a href="https://ci.chromium.org/p/chromium/builders/try/win10_chromium_x64_rel_ng-compilator">win10_chromium_x64_rel_ng-compilator</a>.\
""",
    executable = "recipe:reclient_goma_comparison",
    builderless = True,
    cores = 32,
    os = os.WINDOWS_DEFAULT,
    ssd = True,
    console_view_entry = consoles.console_view_entry(
        category = "win|cq",
        short_name = "re",
    ),
    execution_timeout = 6 * time.hour,
    goma_enable_ats = False,
    goma_jobs = 300,
    reclient_cache_silo = "Comparison Windows CQ - cache siloed",
    reclient_instance = reclient.instance.TEST_UNTRUSTED,
    reclient_jobs = 300,
)

ci.builder(
    name = "Comparison Simple Chrome (reclient)(CQ)",
    description_html = """\
This builder measures Simple Chrome build performance with goma vs reclient in cq configuration.<br/>\
The bot specs should be in sync with <a href="https://ci.chromium.org/p/chromium/builders/try/linux-chromeos-rel-compilator">linux-chromeos-rel-compilator</a>.\
""",
    executable = "recipe:reclient_goma_comparison",
    builderless = True,
    cores = 32,
    os = os.LINUX_DEFAULT,
    ssd = True,
    console_view_entry = consoles.console_view_entry(
        category = "cros x64|cq",
        short_name = "cmp",
    ),
    execution_timeout = 10 * time.hour,
    goma_jobs = 300,
    reclient_cache_silo = "Comparison Simple Chrome CQ - cache siloed",
    reclient_instance = reclient.instance.TEST_UNTRUSTED,
    reclient_jobs = 300,
)

ci.builder(
    name = "Comparison ios (reclient)(CQ)",
    description_html = """\
This builder measures iOS build performance with goma vs reclient in cq configuration.<br/>\
The bot specs should be in sync with <a href="https://ci.chromium.org/p/chromium/builders/try/ios-simulator">ios-simulator</a>.\
""",
    executable = "recipe:reclient_goma_comparison",
    builderless = True,
    cores = None,
    os = os.MAC_DEFAULT,
    ssd = True,
    console_view_entry = consoles.console_view_entry(
        category = "ios|cq",
        short_name = "cmp",
    ),
    execution_timeout = 10 * time.hour,
    goma_jobs = 150,
    reclient_cache_silo = "Comparison ios CQ - cache siloed",
    reclient_instance = reclient.instance.TEST_UNTRUSTED,
    reclient_jobs = 150,
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
    executable = "recipe:build_perf",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "android",
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
    builderless = True,
    cores = 32,
    # Target luci-chromium-ci-bionic-us-central1-c-1000-ssd-hm32-*.
    os = os.LINUX_DEFAULT,
    ssd = True,
    console_view_entry = consoles.console_view_entry(
        category = "buildperf",
        short_name = "and",
    ),
    execution_timeout = 10 * time.hour,
    reclient_instance = reclient.instance.DEFAULT_UNTRUSTED,
    reclient_jobs = reclient.jobs.HIGH_JOBS_FOR_CQ,
    service_account = "chromium-build-perf-ci-builder@chops-service-accounts.iam.gserviceaccount.com",
)

ci.builder(
    name = "build-perf-linux",
    description_html = """\
This builder measures Linux build performance with and without remote caches.<br/>\
The build configs and the bot specs should be in sync with <a href="https://ci.chromium.org/p/chromium/builders/try/linux-rel-compilator">linux-rel-compilator</a>.\
""",
    executable = "recipe:build_perf",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
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
    builderless = True,
    cores = 16,
    # Target luci-chromium-ci-bionic-us-central1-b-ssd-16-*.
    os = os.LINUX_DEFAULT,
    ssd = True,
    console_view_entry = consoles.console_view_entry(
        category = "buildperf",
        short_name = "lnx",
    ),
    execution_timeout = 6 * time.hour,
    reclient_instance = reclient.instance.DEFAULT_UNTRUSTED,
    reclient_jobs = reclient.jobs.LOW_JOBS_FOR_CQ,
    service_account = "chromium-build-perf-ci-builder@chops-service-accounts.iam.gserviceaccount.com",
    use_clang_coverage = True,
)

ci.builder(
    name = "build-perf-windows",
    description_html = """\
This builder measures Windows build performance with and without remote caches.<br/>\
The build configs and the bot specs should be in sync with <a href="https://ci.chromium.org/p/chromium/builders/try/win10_chromium_x64_rel_ng-compilator">win10_chromium_x64_rel_ng-compilator</a>.\
""",
    executable = "recipe:build_perf",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
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
    builderless = True,
    cores = 32,
    # Target luci-chromium-ci-win10-ssd-32-*.
    os = os.WINDOWS_DEFAULT,
    ssd = True,
    console_view_entry = consoles.console_view_entry(
        category = "buildperf",
        short_name = "win",
    ),
    execution_timeout = 6 * time.hour,
    reclient_instance = reclient.instance.DEFAULT_UNTRUSTED,
    reclient_jobs = reclient.jobs.HIGH_JOBS_FOR_CQ,
    service_account = "chromium-build-perf-ci-builder@chops-service-accounts.iam.gserviceaccount.com",
    use_clang_coverage = True,
)

ci.builder(
    name = "Linux Builder (j-500) (reclient)",
    schedule = "triggered",
    os = os.LINUX_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "linux",
        short_name = "re",
    ),
    goma_backend = None,
    reclient_instance = reclient.instance.DEFAULT_TRUSTED,
    reclient_jobs = 500,
    reclient_rewrapper_env = {
        "RBE_platform": "container-image=docker://gcr.io/cloud-marketplace/google/rbe-ubuntu16-04@sha256:b4dad0bfc4951d619229ab15343a311f2415a16ef83bcaa55b44f4e2bf1cf635,pool=linux-e2-custom_0",
    },
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
    cores = 32,
    os = os.LINUX_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "linux",
        short_name = "re",
    ),
    execution_timeout = 14 * time.hour,
    goma_backend = None,
    reclient_ensure_verified = True,
    reclient_instance = reclient.instance.DEFAULT_TRUSTED,
    reclient_rewrapper_env = {
        "RBE_compare": "true",
    },
)

# Start - Reclient migration, phase 2, block 1 shadow builders
ci.builder(
    name = "Linux CFI (reclient shadow)",
    cores = 32,
    os = os.LINUX_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "cfi",
        short_name = "lnx",
    ),
    # TODO(thakis): Remove once https://crbug.com/927738 is resolved.
    execution_timeout = 5 * time.hour,
    goma_backend = None,
    reclient_instance = reclient.instance.DEFAULT_TRUSTED,
    reclient_jobs = 400,
)
# End - Reclient migration, phase 2, block 1 shadow builders

ci.builder(
    name = "Win x64 Builder (reclient)",
    builderless = True,
    cores = 32,
    os = os.WINDOWS_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "win",
        short_name = "re",
    ),
    goma_backend = None,
    reclient_instance = reclient.instance.DEFAULT_TRUSTED,
)

ci.builder(
    name = "Win x64 Builder (reclient compare)",
    description_html = "verify artifacts. should be removed after the migration. crbug.com/1260232",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = ["use_clang_coverage", "reclient_test"],
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = ["mb"],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
        ),
    ),
    builderless = True,
    cores = 32,
    os = os.WINDOWS_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "win",
        short_name = "re",
    ),
    goma_backend = None,
    reclient_ensure_verified = True,
    reclient_instance = reclient.instance.DEFAULT_TRUSTED,
    reclient_rewrapper_env = {"RBE_compare": "true"},
)

ci.builder(
    name = "Win x64 Builder (reclient)(cross)",
    builderless = True,
    cores = 32,
    os = os.WINDOWS_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "win",
        short_name = "re x",
    ),
    goma_backend = None,
    reclient_instance = reclient.instance.DEFAULT_TRUSTED,
    reclient_profiler_service = "reclient-win",
    reclient_publish_trace = True,
)

fyi_mac_builder(
    name = "Mac Builder (reclient)",
    description_html = "experiment reclient on mac. should be removed after the migration. crbug.com/1244441",
    builderless = True,
    cores = None,  # crbug.com/1245114
    console_view_entry = consoles.console_view_entry(
        category = "mac",
        short_name = "re",
    ),
    goma_backend = None,
    reclient_instance = reclient.instance.DEFAULT_TRUSTED,
)

fyi_mac_builder(
    name = "Mac Builder (reclient compare)",
    description_html = "verify artifacts. should be removed after the migration. crbug.com/1260232",
    builderless = True,
    cores = None,  # crbug.com/1245114
    console_view_entry = consoles.console_view_entry(
        category = "mac",
        short_name = "cmp",
    ),
    execution_timeout = 14 * time.hour,
    goma_backend = None,
    reclient_ensure_verified = True,
    reclient_instance = reclient.instance.DEFAULT_TRUSTED,
    reclient_rewrapper_env = {"RBE_compare": "true"},
)

fyi_mac_builder(
    name = "mac-arm64-on-arm64-rel-reclient",
    description_html = "experiment reclient on mac-arm. should be removed after the migration. crbug.com/1252626",

    # same with mac-arm64-on-arm64-rel
    cores = None,  # crbug.com/1245114
    os = os.MAC_12,
    cpu = cpu.ARM64,
    console_view_entry = consoles.console_view_entry(
        category = "mac",
        short_name = "re",
    ),
    goma_backend = None,
    reclient_instance = reclient.instance.DEFAULT_TRUSTED,
)

ci.builder(
    name = "chromeos-amd64-generic-rel (goma cache silo)",
    os = os.LINUX_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "cros x64",
        short_name = "cgc",
    ),
)

ci.builder(
    name = "chromeos-amd64-generic-rel (reclient)",
    os = os.LINUX_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "cros x64",
    ),
    goma_backend = None,
    reclient_instance = reclient.instance.DEFAULT_TRUSTED,
    reclient_rewrapper_env = {"RBE_cache_silo": "chromeos-amd64-generic-rel (reclient)"},
)

# TODO(crbug.com/1235218): remove after the migration.
ci.builder(
    name = "chromeos-amd64-generic-rel (reclient compare)",
    description_html = "verify artifacts. should be removed after the migration. crbug.com/1235218",
    os = os.LINUX_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "cros x64",
        short_name = "cmp",
    ),
    execution_timeout = 14 * time.hour,
    goma_backend = None,
    reclient_ensure_verified = True,
    reclient_instance = reclient.instance.DEFAULT_TRUSTED,
    reclient_rewrapper_env = {"RBE_compare": "true"},
)

ci.builder(
    name = "lacros-amd64-generic-rel (goma cache silo)",
    os = os.LINUX_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "lacros x64",
        short_name = "cgc",
    ),
)

ci.builder(
    name = "lacros-amd64-generic-rel (reclient)",
    os = os.LINUX_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "lacros x64",
    ),
    goma_backend = None,
    reclient_instance = reclient.instance.DEFAULT_TRUSTED,
    reclient_rewrapper_env = {"RBE_cache_silo": "lacros-amd64-generic-rel (reclient)"},
)

ci.builder(
    name = "linux-lacros-builder-rel (goma cache silo)",
    os = os.LINUX_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "lacros rel",
        short_name = "cgc",
    ),
)

ci.builder(
    name = "linux-lacros-builder-rel (reclient)",
    os = os.LINUX_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "lacros rel",
    ),
    goma_backend = None,
    reclient_instance = reclient.instance.DEFAULT_TRUSTED,
    reclient_rewrapper_env = {"RBE_cache_silo": "linux-lacros-builder-rel (reclient)"},
)

fyi_celab_builder(
    name = "win-celab-builder-rel",
    schedule = "0 0,6,12,18 * * *",
    triggered_by = [],
    console_view_entry = consoles.console_view_entry(
        category = "celab",
    ),
    goma_backend = None,
    reclient_instance = reclient.instance.DEFAULT_TRUSTED,
    reclient_jobs = reclient.jobs.LOW_JOBS_FOR_CI,
)

fyi_celab_builder(
    name = "win-celab-tester-rel",
    triggered_by = ["win-celab-builder-rel"],
    console_view_entry = consoles.console_view_entry(
        category = "celab",
    ),
)

fyi_coverage_builder(
    name = "android-code-coverage",
    schedule = "triggered",
    triggered_by = [],
    os = os.LINUX_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "code_coverage",
        short_name = "and",
    ),
    coverage_test_types = ["overall", "unit"],
    export_coverage_to_zoss = True,
    goma_backend = None,
    reclient_instance = reclient.instance.DEFAULT_TRUSTED,
    reclient_jobs = reclient.jobs.HIGH_JOBS_FOR_CI,
    use_java_coverage = True,
)

fyi_coverage_builder(
    name = "android-code-coverage-native",
    os = os.LINUX_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "code_coverage",
        short_name = "ann",
    ),
    coverage_test_types = ["overall", "unit"],
    export_coverage_to_zoss = True,
    goma_backend = None,
    reclient_instance = reclient.instance.DEFAULT_TRUSTED,
    reclient_jobs = reclient.jobs.HIGH_JOBS_FOR_CI,
    use_clang_coverage = True,
)

fyi_coverage_builder(
    name = "fuchsia-code-coverage",
    schedule = "triggered",
    triggered_by = [],
    os = os.LINUX_DEFAULT,
    console_view_entry = [
        consoles.console_view_entry(
            category = "code_coverage",
            short_name = "fx",
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

fyi_coverage_builder(
    name = "ios-simulator-code-coverage",
    cores = None,
    os = os.MAC_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "code_coverage",
        short_name = "ios",
    ),
    coverage_exclude_sources = "ios_test_files_and_test_utils",
    coverage_test_types = ["overall", "unit"],
    export_coverage_to_zoss = True,
    goma_backend = None,
    reclient_instance = reclient.instance.DEFAULT_TRUSTED,
    reclient_jobs = reclient.jobs.DEFAULT,
    use_clang_coverage = True,
    xcode = xcode.x14main,
)

fyi_coverage_builder(
    name = "linux-chromeos-code-coverage",
    schedule = "triggered",
    triggered_by = [],
    os = os.LINUX_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "code_coverage",
        short_name = "lcr",
    ),
    coverage_test_types = ["overall", "unit"],
    goma_backend = None,
    reclient_instance = reclient.instance.DEFAULT_TRUSTED,
    reclient_jobs = reclient.jobs.HIGH_JOBS_FOR_CI,
    use_clang_coverage = True,
)

fyi_coverage_builder(
    name = "linux-js-code-coverage",
    schedule = "triggered",
    triggered_by = [],
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
    console_view_entry = consoles.console_view_entry(
        category = "code_coverage",
        short_name = "jcr",
    ),
    goma_backend = None,
    reclient_instance = reclient.instance.DEFAULT_TRUSTED,
    reclient_jobs = reclient.jobs.HIGH_JOBS_FOR_CI,
    use_javascript_coverage = True,
)

fyi_coverage_builder(
    name = "linux-code-coverage",
    triggered_by = [],
    os = os.LINUX_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "code_coverage",
        short_name = "lnx",
    ),
    coverage_test_types = ["overall", "unit"],
    export_coverage_to_zoss = True,
    use_clang_coverage = True,
)

fyi_coverage_builder(
    name = "linux-lacros-code-coverage",
    os = os.LINUX_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "code_coverage",
        short_name = "lac",
    ),
    coverage_test_types = ["overall", "unit"],
    goma_backend = None,
    reclient_instance = reclient.instance.DEFAULT_TRUSTED,
    reclient_jobs = reclient.jobs.HIGH_JOBS_FOR_CI,
    use_clang_coverage = True,
)

fyi_coverage_builder(
    name = "mac-code-coverage",
    builderless = True,
    cores = 24,
    os = os.MAC_ANY,
    console_view_entry = consoles.console_view_entry(
        category = "code_coverage",
        short_name = "mac",
    ),
    coverage_test_types = ["overall", "unit"],
    export_coverage_to_zoss = True,
    goma_backend = None,
    reclient_instance = reclient.instance.DEFAULT_TRUSTED,
    reclient_jobs = reclient.jobs.HIGH_JOBS_FOR_CI,
    use_clang_coverage = True,
)

fyi_coverage_builder(
    name = "win10-code-coverage",
    builderless = True,
    os = os.WINDOWS_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "code_coverage",
        short_name = "win",
    ),
    coverage_test_types = ["overall", "unit"],
    export_coverage_to_zoss = True,
    goma_backend = None,
    reclient_instance = reclient.instance.DEFAULT_TRUSTED,
    reclient_jobs = reclient.jobs.DEFAULT,
    use_clang_coverage = True,
)

fyi_ios_builder(
    name = "ios-m1-simulator",
    schedule = "0 1,5,9,13,17,21 * * *",
    triggered_by = [],
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
    os = os.MAC_DEFAULT,
    cpu = cpu.ARM64,
    console_view_entry = consoles.console_view_entry(
        category = "iOS|iOSM1",
        short_name = "iosM1",
    ),
)

fyi_ios_builder(
    name = "ios-reclient",
    description_html = "experiment reclient for ios. remove after the migration. crbug.com/1254986",
    builderless = True,
    os = os.MAC_DEFAULT,
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
)

fyi_ios_builder(
    name = "ios-simulator-cronet",
    branch_selector = branches.selector.IOS_BRANCHES,
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
    goma_backend = None,
    notifies = ["cronet"],
    reclient_instance = reclient.instance.DEFAULT_TRUSTED,
    reclient_jobs = reclient.jobs.DEFAULT,
)

fyi_ios_builder(
    name = "ios-m1-simulator-cronet",
    schedule = "0 1,5,9,13,17,21 * * *",
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
    os = os.MAC_12,
    cpu = cpu.ARM64,
    console_view_entry = consoles.console_view_entry(
        category = "cronet",
        short_name = "m1",
    ),
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
    schedule = "0 1-23/6 * * *",
    triggered_by = [],
    console_view_entry = consoles.console_view_entry(
        category = "iOS",
        short_name = "wk",
    ),
    goma_backend = None,
    reclient_instance = reclient.instance.DEFAULT_TRUSTED,
    reclient_jobs = reclient.jobs.DEFAULT,
    xcode = xcode.x13wk,
)

fyi_ios_builder(
    name = "ios15-beta-simulator",
    os = os.MAC_12,
    console_view_entry = [
        consoles.console_view_entry(
            category = "iOS|iOS15",
            short_name = "ios15",
        ),
    ],
    goma_backend = None,
    reclient_instance = reclient.instance.DEFAULT_TRUSTED,
    reclient_jobs = reclient.jobs.DEFAULT,
)

fyi_ios_builder(
    name = "ios15-sdk-simulator",
    os = os.MAC_12,
    console_view_entry = [
        consoles.console_view_entry(
            category = "iOS|iOS15",
            short_name = "sdk15",
        ),
    ],
    goma_backend = None,
    reclient_instance = reclient.instance.DEFAULT_TRUSTED,
    reclient_jobs = reclient.jobs.DEFAULT,
)

fyi_ios_builder(
    name = "ios16-beta-simulator",
    schedule = "0 0,4,8,12,16,20 * * *",
    triggered_by = [],
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
    os = os.MAC_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "iOS|iOS16",
        short_name = "ios16",
    ),
)

fyi_ios_builder(
    name = "ios16-sdk-device",
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
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
            target_platform = builder_config.target_platform.IOS,
        ),
        build_gs_bucket = "chromium-fyi-archive",
    ),
    os = os.MAC_DEFAULT,
    console_view_entry = [
        consoles.console_view_entry(
            category = "iOS|iOS16",
            short_name = "dev",
        ),
    ],
    goma_backend = None,
    reclient_instance = reclient.instance.DEFAULT_TRUSTED,
    reclient_jobs = reclient.jobs.DEFAULT,
)

fyi_ios_builder(
    name = "ios16-sdk-simulator",
    schedule = "0 2,6,10,14,18,22 * * *",
    triggered_by = [],
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
    os = os.MAC_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "iOS|iOS16",
        short_name = "sdk16",
    ),
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
    os = os.LINUX_FOCAL,
    console_view_entry = consoles.console_view_entry(
        category = "msan",
        short_name = "lin",
    ),
    execution_timeout = 16 * time.hour,
    goma_backend = None,
    reclient_instance = reclient.instance.DEFAULT_TRUSTED,
    reclient_jobs = reclient.jobs.HIGH_JOBS_FOR_CI,
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
    os = os.LINUX_FOCAL,
    console_view_entry = consoles.console_view_entry(
        category = "msan",
        short_name = "crs",
    ),
    execution_timeout = 16 * time.hour,
    goma_jobs = goma.jobs.MANY_JOBS_FOR_CI,
)

fyi_mac_builder(
    name = "Mac Builder Next",
    cores = None,
    os = os.MAC_13,
    cpu = cpu.ARM64,
    console_view_entry = consoles.console_view_entry(
        category = "mac",
        short_name = "bld",
    ),
    goma_backend = None,
    reclient_instance = reclient.instance.DEFAULT_TRUSTED,
    reclient_jobs = reclient.jobs.DEFAULT,
)

fyi_mac_builder(
    name = "Mac deterministic",
    executable = "recipe:swarming/deterministic_build",
    cores = None,
    console_view_entry = consoles.console_view_entry(
        category = "deterministic|mac",
        short_name = "rel",
    ),
    execution_timeout = 6 * time.hour,
    goma_backend = None,
    reclient_instance = reclient.instance.DEFAULT_TRUSTED,
    reclient_jobs = reclient.jobs.DEFAULT,
)

fyi_mac_builder(
    name = "Mac deterministic (dbg)",
    executable = "recipe:swarming/deterministic_build",
    cores = None,
    os = os.MAC_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "deterministic|mac",
        short_name = "dbg",
    ),
    execution_timeout = 6 * time.hour,
    goma_backend = None,
    reclient_instance = reclient.instance.DEFAULT_TRUSTED,
    reclient_jobs = reclient.jobs.DEFAULT,
)

fyi_mac_builder(
    name = "mac-hermetic-upgrade-rel",
    cores = 12,
    console_view_entry = consoles.console_view_entry(
        category = "mac",
        short_name = "herm",
    ),
    goma_backend = None,
    reclient_instance = reclient.instance.DEFAULT_TRUSTED,
    reclient_jobs = reclient.jobs.DEFAULT,
)

ci.builder(
    name = "Win 10 Fast Ring",
    os = os.WINDOWS_10,
    console_view_entry = consoles.console_view_entry(
        category = "win10",
    ),
    goma_backend = None,
    notifies = ["Win 10 Fast Ring"],
    reclient_instance = reclient.instance.DEFAULT_TRUSTED,
    reclient_jobs = reclient.jobs.LOW_JOBS_FOR_CI,
)

ci.builder(
    name = "win10-wpt-content-shell-fyi-rel",
    schedule = "with 5h interval",
    triggered_by = [],
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = ["mb"],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
        ),
    ),
    builderless = True,
    os = os.WINDOWS_10,
    console_view_entry = consoles.console_view_entry(
        category = "win10",
    ),
    experimental = True,
    goma_backend = None,
    reclient_instance = reclient.instance.DEFAULT_TRUSTED,
    reclient_jobs = reclient.jobs.DEFAULT,
)

ci.builder(
    name = "win32-arm64-rel",
    cores = "8|16",
    os = os.WINDOWS_DEFAULT,
    cpu = cpu.X86,
    console_view_entry = consoles.console_view_entry(
        category = "win32|arm64",
    ),
    goma_backend = None,
    reclient_instance = reclient.instance.DEFAULT_TRUSTED,
    reclient_jobs = 150,
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
    os = os.WINDOWS_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "win",
    ),
    reclient_instance = reclient.instance.DEFAULT_TRUSTED,
    reclient_jobs = reclient.jobs.LOW_JOBS_FOR_CI,
)

ci.builder(
    name = "win-annotator-rel",
    builderless = True,
    os = os.WINDOWS_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "network|traffic|annotations",
        short_name = "win",
    ),
    execution_timeout = 16 * time.hour,
    goma_backend = None,
    notifies = ["annotator-rel"],
    reclient_instance = reclient.instance.DEFAULT_TRUSTED,
    reclient_jobs = reclient.jobs.LOW_JOBS_FOR_CI,
)

ci.builder(
    name = "Blink Unexpected Pass Finder",
    executable = "recipe:chromium_expectation_files/expectation_file_scripts",
    # This will eventually be set to run on a schedule, but only support
    # manual triggering for now until we get a successful build.
    schedule = "triggered",
    triggered_by = [],
    builderless = True,
    cores = "16",
    console_view_entry = consoles.console_view_entry(
        short_name = "upf",
    ),
    properties = {
        "scripts": [
            {
                "step_name": "remove_stale_blink_expectations",
                "script": "third_party/blink/tools/remove_stale_expectations.py",
                "script_type": "UNEXPECTED_PASS",
                "submit_type": "MANUAL",
                "reviewer_list": {
                    "reviewer": ["bsheedy@chromium.org"],
                },
                "cl_title": "Remove stale Blink expectations",
                "args": [
                    "--project",
                    "chrome-unexpected-pass-data",
                    "--no-include-internal-builders",
                    "--remove-stale-expectations",
                    "--num-samples",
                    "200",
                ],
            },
        ],
    },
    service_account = "chromium-automated-expectation@chops-service-accounts.iam.gserviceaccount.com",
)
