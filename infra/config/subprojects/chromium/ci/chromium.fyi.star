# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Definitions of builders in the chromium.fuzz builder group."""

load("//lib/branches.star", "branches")
load("//lib/builder_config.star", "builder_config")
load("//lib/builders.star", "cpu", "goma", "os", "reclient", "xcode")
load("//lib/ci.star", "ci")
load("//lib/consoles.star", "consoles")

ci.defaults.set(
    executable = ci.DEFAULT_EXECUTABLE,
    builder_group = "chromium.fyi",
    pool = ci.DEFAULT_POOL,
    cores = 8,
    execution_timeout = 10 * time.hour,
    goma_backend = goma.backend.RBE_PROD,
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
            "paeverywhere",
            "backuprefptr",
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
    kwargs.setdefault("os", os.MAC_11)
    kwargs.setdefault("xcode", xcode.x13main)
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
    name = "fuchsia-fyi-arm64-dbg",
    os = os.LINUX_DEFAULT,
    console_view_entry = [
        consoles.console_view_entry(
            category = "fuchsia|a64",
            short_name = "dbg",
        ),
        consoles.console_view_entry(
            branch_selector = branches.selector.MAIN,
            console_view = "sheriff.fuchsia",
            category = "fyi",
            short_name = "a64-dbg",
        ),
    ],
    notifies = ["cr-fuchsia"],
)

ci.builder(
    name = "fuchsia-fyi-arm64-femu",
    os = os.LINUX_DEFAULT,
    console_view_entry = [
        consoles.console_view_entry(
            category = "fuchsia|a64",
            short_name = "femu",
        ),
    ],
    notifies = ["cr-fuchsia"],
)

ci.builder(
    name = "fuchsia-fyi-arm64-rel",
    os = os.LINUX_DEFAULT,
    console_view_entry = [
        consoles.console_view_entry(
            category = "fuchsia|a64",
            short_name = "rel",
        ),
        consoles.console_view_entry(
            branch_selector = branches.selector.MAIN,
            console_view = "sheriff.fuchsia",
            category = "fyi",
            short_name = "a64",
        ),
    ],
    notifies = ["cr-fuchsia"],
)

ci.builder(
    name = "fuchsia-fyi-x64-asan",
    os = os.LINUX_DEFAULT,
    console_view_entry = [
        consoles.console_view_entry(
            category = "fuchsia|x64",
            short_name = "asan",
        ),
        consoles.console_view_entry(
            branch_selector = branches.selector.MAIN,
            console_view = "sheriff.fuchsia",
            category = "fyi",
            short_name = "asan",
        ),
    ],
    notifies = ["cr-fuchsia"],
)

ci.builder(
    name = "fuchsia-fyi-x64-dbg",
    os = os.LINUX_DEFAULT,
    console_view_entry = [
        consoles.console_view_entry(
            category = "fuchsia|x64",
            short_name = "dbg",
        ),
        consoles.console_view_entry(
            branch_selector = branches.selector.MAIN,
            console_view = "sheriff.fuchsia",
            category = "fyi",
            short_name = "x64-dbg",
        ),
    ],
    notifies = ["cr-fuchsia"],
)

ci.builder(
    name = "fuchsia-fyi-x64-rel",
    os = os.LINUX_DEFAULT,
    console_view_entry = [
        consoles.console_view_entry(
            category = "fuchsia|x64",
            short_name = "rel",
        ),
        consoles.console_view_entry(
            branch_selector = branches.selector.MAIN,
            console_view = "sheriff.fuchsia",
            category = "fyi",
            short_name = "x64",
        ),
    ],
    notifies = ["cr-fuchsia"],
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
    os = os.LINUX_DEFAULT,
    console_view_entry = [
        consoles.console_view_entry(
            category = "fuchsia|x64",
            short_name = "rev",
        ),
        consoles.console_view_entry(
            branch_selector = branches.selector.MAIN,
            console_view = "sheriff.fuchsia",
            category = "fyi",
            short_name = "rev",
        ),
    ],
    notifies = ["cr-fuchsia"],
)

ci.builder(
    name = "fuchsia-fyi-x64-wst",
    os = os.LINUX_DEFAULT,
    console_view_entry = [
        consoles.console_view_entry(
            category = "fuchsia|x64",
            short_name = "wst",
        ),
        consoles.console_view_entry(
            branch_selector = branches.selector.MAIN,
            console_view = "sheriff.fuchsia",
            category = "fyi",
            short_name = "wst",
        ),
    ],
    notifies = ["cr-fuchsia"],
)

ci.builder(
    name = "lacros-amd64-generic-rel-fyi",
    os = os.LINUX_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "lacros",
        short_name = "lcr",
    ),
)

ci.builder(
    name = "lacros-amd64-generic-rel-skylab-fyi",
    os = os.LINUX_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "lacros",
        short_name = "lsf",
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
    branch_selector = branches.selector.LINUX_BRANCHES,
    builderless = True,
    os = os.LINUX_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "release",
        short_name = "rel",
    ),
    execution_timeout = 3 * time.hour,
    goma_enable_ats = True,
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
    name = "linux-lacros-version-skew-fyi",
    os = os.LINUX_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "default",
    ),
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
    name = "linux-blink-v8-sandbox-future-dbg",
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
)

ci.builder(
    name = "linux-lacros-tester-fyi-rel",
    triggered_by = ["linux-lacros-builder-fyi-rel"],
    os = os.LINUX_DEFAULT,
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
)

ci.builder(
    name = "linux-lacros-dbg-tests-fyi",
    triggered_by = ["linux-lacros-dbg-fyi"],
    os = os.LINUX_DEFAULT,
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

# This is launching & collecting entirely isolated tests.
# OS shouldn't matter.
ci.builder(
    name = "mac-osxbeta-rel",
    triggered_by = ["ci/Mac Builder"],
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
    os = os.LINUX_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "mac",
        short_name = "beta",
    ),
    main_console_view = None,
    goma_backend = goma.backend.RBE_PROD,
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

ci.builder(
    name = "mac-paeverywhere-x64-fyi-dbg",
    builderless = True,
    cores = None,
    os = os.MAC_ANY,
    console_view_entry = consoles.console_view_entry(
        category = "paeverywhere|mac",
        short_name = "64dbg",
    ),
    notifies = ["chrome-memory-safety"],
)

ci.builder(
    name = "mac-paeverywhere-x64-fyi-rel",
    builderless = True,
    cores = None,
    os = os.MAC_ANY,
    console_view_entry = consoles.console_view_entry(
        category = "paeverywhere|mac",
        short_name = "64rel",
    ),
    notifies = ["chrome-memory-safety"],
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
    name = "win-pixel-builder-rel",
    os = os.WINDOWS_10,
    console_view_entry = consoles.console_view_entry(
        category = "win10",
    ),
    goma_backend = None,
    reclient_instance = reclient.instance.DEFAULT_TRUSTED,
    reclient_jobs = reclient.jobs.LOW_JOBS_FOR_CI,
)

ci.builder(
    name = "win-pixel-tester-rel",
    triggered_by = ["win-pixel-builder-rel"],
    os = os.LINUX_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "win10",
    ),
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
    executable = "recipe:reclient_goma_comparison",
    os = os.LINUX_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "android",
        short_name = "cmp",
    ),
    execution_timeout = 10 * time.hour,
    goma_jobs = 250,
    reclient_cache_silo = "Comparison Android - cache siloed",
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
    name = "Comparison Windows (8 cores) (reclient)",
    executable = "recipe:reclient_goma_comparison",
    builderless = True,
    cores = 8,
    os = os.WINDOWS_DEFAULT,
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
    name = "Win ASan Release (reclient shadow)",
    builderless = True,
    os = os.WINDOWS_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "win asan",
        short_name = "rel",
    ),
    goma_backend = None,
    reclient_instance = reclient.instance.DEFAULT_TRUSTED,
    reclient_jobs = 80,
)

ci.builder(
    name = "Win ASan Release Media (reclient shadow)",
    builderless = True,
    os = os.WINDOWS_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "win asan",
        short_name = "med",
    ),
    goma_backend = None,
    reclient_instance = reclient.instance.DEFAULT_TRUSTED,
    reclient_jobs = 80,
)

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
            apply_configs = ["use_clang_coverage", "enable_reclient", "reclient_test"],
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
    os = os.MAC_11,
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
            short_name = "fsa",
        ),
        consoles.console_view_entry(
            branch_selector = branches.selector.MAIN,
            console_view = "sheriff.fuchsia",
            category = "misc",
            short_name = "cov",
        ),
    ],
    coverage_test_types = ["overall", "unit"],
    use_clang_coverage = True,
)

fyi_coverage_builder(
    name = "ios-simulator-code-coverage",
    cores = None,
    os = os.MAC_11,
    console_view_entry = consoles.console_view_entry(
        category = "code_coverage",
        short_name = "ios",
    ),
    coverage_exclude_sources = "ios_test_files_and_test_utils",
    coverage_test_types = ["overall", "unit"],
    use_clang_coverage = True,
    xcode = xcode.x13main,
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
    use_clang_coverage = True,
)

fyi_coverage_builder(
    name = "linux-chromeos-js-code-coverage",
    schedule = "triggered",
    triggered_by = [],
    os = os.LINUX_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "code_coverage",
        short_name = "jcr",
    ),
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
    use_clang_coverage = True,
)

fyi_ios_builder(
    name = "ios-asan",
    console_view_entry = consoles.console_view_entry(
        category = "iOS",
        short_name = "asan",
    ),
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
    os = os.MAC_11,
    cpu = cpu.ARM64,
    console_view_entry = consoles.console_view_entry(
        category = "iOS|iOSM1",
        short_name = "iosM1",
    ),
)

fyi_ios_builder(
    name = "ios-reclient",
    description_html = "experiment reclient for ios. remove after the migration. crbug.com/1254986",
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
    notifies = ["cronet"],
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
    os = os.MAC_11,
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
    xcode = xcode.x13wk,
)

fyi_ios_builder(
    name = "ios14-beta-simulator",
    schedule = "0 0,4,8,12,16,20 * * *",
    triggered_by = [],
    os = os.MAC_11,
    console_view_entry = consoles.console_view_entry(
        category = "iOS|iOS14",
        short_name = "ios14",
    ),
)

fyi_ios_builder(
    name = "ios14-sdk-simulator",
    schedule = "0 2,6,10,14,18,22 * * *",
    triggered_by = [],
    os = os.MAC_11,
    console_view_entry = consoles.console_view_entry(
        category = "iOS|iOS14",
        short_name = "sdk14",
    ),
)

fyi_ios_builder(
    name = "ios15-beta-simulator",
    os = os.MAC_11,
    console_view_entry = [
        consoles.console_view_entry(
            category = "iOS|iOS15",
            short_name = "ios15",
        ),
    ],
)

fyi_ios_builder(
    name = "ios15-sdk-device",
    os = os.MAC_12,
    console_view_entry = [
        consoles.console_view_entry(
            category = "iOS|iOS15",
            short_name = "dev",
        ),
    ],
    xcode = xcode.x13betabots,
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
    xcode = xcode.x13betabots,
)

fyi_mac_builder(
    name = "Mac Builder Next",
    cores = None,
    os = None,
    console_view_entry = consoles.console_view_entry(
        category = "mac",
        short_name = "bld",
    ),
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
)

fyi_mac_builder(
    name = "mac-hermetic-upgrade-rel",
    cores = 12,
    console_view_entry = consoles.console_view_entry(
        category = "mac",
        short_name = "herm",
    ),
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
    name = "Win11 Tests x64",
    triggered_by = ["ci/Win x64 Builder"],
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "use_clang_coverage",
            ],
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
    builderless = True,
    os = os.WINDOWS_10,
    console_view_entry = consoles.console_view_entry(
        category = "win11",
    ),
    main_console_view = None,
    goma_backend = None,
)

ci.builder(
    name = "win32-arm64-rel",
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
