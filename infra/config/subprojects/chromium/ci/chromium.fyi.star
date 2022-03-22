# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Definitions of builders in the chromium.fuzz builder group."""

load("//lib/branches.star", "branches")
load("//lib/builder_config.star", "builder_config")
load("//lib/builders.star", "cpu", "goma", "os", "xcode")
load("//lib/ci.star", "ci", "rbe_instance", "rbe_jobs")
load("//lib/consoles.star", "consoles")

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
    console_view_entry = consoles.console_view_entry(
        category = "viz",
    ),
    goma_backend = None,
    os = os.LINUX_BIONIC_SWITCH_TO_DEFAULT,
    reclient_jobs = rbe_jobs.DEFAULT,
    reclient_instance = rbe_instance.DEFAULT,
)

ci.builder(
    name = "Site Isolation Android",
    console_view_entry = consoles.console_view_entry(
        category = "site_isolation",
    ),
    notifies = ["Site Isolation Android"],
    goma_backend = None,
    os = os.LINUX_BIONIC_SWITCH_TO_DEFAULT,
    reclient_jobs = rbe_jobs.DEFAULT,
    reclient_instance = rbe_instance.DEFAULT,
)

ci.builder(
    name = "VR Linux",
    branch_selector = branches.STANDARD_MILESTONE,
    console_view_entry = consoles.console_view_entry(
        category = "linux",
    ),
    cq_mirrors_console_view = "mirrors",
    os = os.LINUX_BIONIC_SWITCH_TO_DEFAULT,
    goma_backend = None,
    reclient_jobs = rbe_jobs.HIGH_JOBS_FOR_CI,
    reclient_instance = rbe_instance.DEFAULT,
)

ci.builder(
    name = "android-backuprefptr-arm-fyi-rel",
    builderless = True,
    console_view_entry = consoles.console_view_entry(
        category = "backuprefptr|android",
        short_name = "32rel",
    ),
    notifies = ["chrome-memory-safety"],
    os = os.LINUX_BIONIC_SWITCH_TO_DEFAULT,
    goma_backend = None,
    reclient_jobs = rbe_jobs.DEFAULT,
    reclient_instance = rbe_instance.DEFAULT,
)

ci.builder(
    name = "android-backuprefptr-arm64-fyi-rel",
    builderless = True,
    console_view_entry = consoles.console_view_entry(
        category = "backuprefptr|android",
        short_name = "64rel",
    ),
    notifies = ["chrome-memory-safety"],
    os = os.LINUX_BIONIC_SWITCH_TO_DEFAULT,
    goma_backend = None,
    reclient_jobs = rbe_jobs.DEFAULT,
    reclient_instance = rbe_instance.DEFAULT,
)

ci.builder(
    name = "fuchsia-fyi-arm64-dbg",
    console_view_entry = [
        consoles.console_view_entry(
            category = "fuchsia|a64",
            short_name = "dbg",
        ),
        consoles.console_view_entry(
            branch_selector = branches.MAIN,
            console_view = "sheriff.fuchsia",
            category = "fyi",
            short_name = "a64-dbg",
        ),
    ],
    notifies = ["cr-fuchsia"],
    os = os.LINUX_BIONIC_SWITCH_TO_DEFAULT,
)

ci.builder(
    name = "fuchsia-fyi-arm64-femu",
    console_view_entry = [
        consoles.console_view_entry(
            category = "fuchsia|a64",
            short_name = "femu",
        ),
    ],
    notifies = ["cr-fuchsia"],
    os = os.LINUX_BIONIC_SWITCH_TO_DEFAULT,
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
    os = os.LINUX_BIONIC_SWITCH_TO_DEFAULT,
)

ci.builder(
    name = "fuchsia-fyi-x64-asan",
    console_view_entry = [
        consoles.console_view_entry(
            category = "fuchsia|x64",
            short_name = "asan",
        ),
        consoles.console_view_entry(
            branch_selector = branches.MAIN,
            console_view = "sheriff.fuchsia",
            category = "fyi",
            short_name = "asan",
        ),
    ],
    notifies = ["cr-fuchsia"],
    os = os.LINUX_BIONIC_SWITCH_TO_DEFAULT,
)

ci.builder(
    name = "fuchsia-fyi-x64-dbg",
    console_view_entry = [
        consoles.console_view_entry(
            category = "fuchsia|x64",
            short_name = "dbg",
        ),
        consoles.console_view_entry(
            branch_selector = branches.MAIN,
            console_view = "sheriff.fuchsia",
            category = "fyi",
            short_name = "x64-dbg",
        ),
    ],
    notifies = ["cr-fuchsia"],
    os = os.LINUX_BIONIC_SWITCH_TO_DEFAULT,
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
    os = os.LINUX_BIONIC_SWITCH_TO_DEFAULT,
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
    os = os.LINUX_BIONIC_SWITCH_TO_DEFAULT,
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
    os = os.LINUX_BIONIC_SWITCH_TO_DEFAULT,
)

ci.builder(
    name = "lacros-amd64-generic-rel-fyi",
    console_view_entry = consoles.console_view_entry(
        category = "lacros",
        short_name = "lcr",
    ),
    os = os.LINUX_BIONIC_SWITCH_TO_DEFAULT,
)

ci.builder(
    name = "lacros-amd64-generic-rel-skylab-fyi",
    console_view_entry = consoles.console_view_entry(
        category = "lacros",
        short_name = "lsf",
    ),
    os = os.LINUX_BIONIC_SWITCH_TO_DEFAULT,
)

ci.builder(
    name = "linux-annotator-rel",
    console_view_entry = consoles.console_view_entry(
        category = "network|traffic|annotations",
        short_name = "lnx",
    ),
    notifies = ["annotator-rel"],
    os = os.LINUX_BIONIC_SWITCH_TO_DEFAULT,
    goma_backend = None,
    reclient_jobs = rbe_jobs.HIGH_JOBS_FOR_CI,
    reclient_instance = rbe_instance.DEFAULT,
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
    os = os.LINUX_BIONIC_SWITCH_TO_DEFAULT,
    goma_enable_ats = True,
)

ci.builder(
    name = "linux-ash-chromium-builder-fyi-rel",
    console_view_entry = consoles.console_view_entry(
        category = "default",
        short_name = "lcr",
    ),
    os = os.LINUX_BIONIC_SWITCH_TO_DEFAULT,
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
    os = os.LINUX_BIONIC_SWITCH_TO_DEFAULT,
)

ci.builder(
    name = "linux-blink-animation-use-time-delta",
    console_view_entry = consoles.console_view_entry(
        category = "linux|blink",
        short_name = "TD",
    ),
    os = os.LINUX_BIONIC_SWITCH_TO_DEFAULT,
)

ci.builder(
    name = "linux-blink-heap-verification",
    console_view_entry = consoles.console_view_entry(
        category = "linux|blink",
        short_name = "VF",
    ),
    notifies = ["linux-blink-fyi-bots"],
    os = os.LINUX_BIONIC_SWITCH_TO_DEFAULT,
    goma_backend = None,
    reclient_jobs = rbe_jobs.HIGH_JOBS_FOR_CI,
    reclient_instance = rbe_instance.DEFAULT,
)

ci.builder(
    name = "linux-blink-v8-sandbox-future-dbg",
    console_view_entry = consoles.console_view_entry(
        category = "linux|blink",
        short_name = "SB",
    ),
    notifies = ["v8-sandbox-fyi-bots"],
    os = os.LINUX_BIONIC_SWITCH_TO_DEFAULT,
    goma_backend = None,
    reclient_jobs = rbe_jobs.DEFAULT,
    reclient_instance = rbe_instance.DEFAULT,
)

ci.builder(
    name = "linux-example-builder",
    console_view_entry = consoles.console_view_entry(
        category = "linux",
    ),
    os = os.LINUX_BIONIC_SWITCH_TO_DEFAULT,
    schedule = "with 12h interval",
    triggered_by = [],
    goma_backend = None,
    reclient_jobs = rbe_jobs.DEFAULT,
    reclient_instance = rbe_instance.DEFAULT,
)

ci.builder(
    name = "linux-fieldtrial-rel",
    console_view_entry = consoles.console_view_entry(
        category = "linux",
    ),
    os = os.LINUX_BIONIC_SWITCH_TO_DEFAULT,
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
    name = "linux-lacros-builder-fyi-rel",
    console_view_entry = consoles.console_view_entry(
        category = "linux",
    ),
    os = os.LINUX_BIONIC_SWITCH_TO_DEFAULT,
)

ci.builder(
    name = "linux-lacros-tester-fyi-rel",
    console_view_entry = consoles.console_view_entry(
        category = "linux",
    ),
    os = os.LINUX_BIONIC_SWITCH_TO_DEFAULT,
    triggered_by = ["linux-lacros-builder-fyi-rel"],
)

ci.builder(
    name = "linux-lacros-dbg-fyi",
    console_view_entry = consoles.console_view_entry(
        category = "linux",
    ),
    os = os.LINUX_BIONIC_SWITCH_TO_DEFAULT,
)

ci.builder(
    name = "linux-lacros-dbg-tests-fyi",
    console_view_entry = consoles.console_view_entry(
        category = "linux",
    ),
    os = os.LINUX_BIONIC_SWITCH_TO_DEFAULT,
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
    os = os.LINUX_BIONIC_SWITCH_TO_DEFAULT,
    goma_backend = None,
    reclient_jobs = rbe_jobs.DEFAULT,
    reclient_instance = rbe_instance.DEFAULT,
)

ci.builder(
    name = "linux-perfetto-rel",
    console_view_entry = consoles.console_view_entry(
        category = "linux",
    ),
    os = os.LINUX_BIONIC_SWITCH_TO_DEFAULT,
    goma_backend = None,
    reclient_jobs = rbe_jobs.DEFAULT,
    reclient_instance = rbe_instance.DEFAULT,
)

ci.builder(
    name = "linux-wpt-fyi-rel",
    console_view_entry = consoles.console_view_entry(
        category = "linux",
    ),
    experimental = True,
    os = os.LINUX_BIONIC_SWITCH_TO_DEFAULT,
    goma_backend = None,
    reclient_jobs = rbe_jobs.DEFAULT,
    reclient_instance = rbe_instance.DEFAULT,
)

ci.builder(
    name = "linux-wpt-identity-fyi-rel",
    console_view_entry = consoles.console_view_entry(
        category = "linux",
    ),
    experimental = True,
    os = os.LINUX_BIONIC_SWITCH_TO_DEFAULT,
    goma_backend = None,
    reclient_jobs = rbe_jobs.DEFAULT,
    reclient_instance = rbe_instance.DEFAULT,
)

ci.builder(
    name = "linux-wpt-input-fyi-rel",
    console_view_entry = consoles.console_view_entry(
        category = "linux",
    ),
    experimental = True,
    os = os.LINUX_BIONIC_SWITCH_TO_DEFAULT,
    goma_backend = None,
    reclient_jobs = rbe_jobs.DEFAULT,
    reclient_instance = rbe_instance.DEFAULT,
)

# This is launching & collecting entirely isolated tests.
# OS shouldn't matter.
ci.builder(
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
    goma_backend = goma.backend.RBE_PROD,
    main_console_view = None,
    os = os.LINUX_BIONIC_SWITCH_TO_DEFAULT,
    triggered_by = ["ci/Mac Builder"],
)

ci.builder(
    name = "linux-headless-shell-rel",
    console_view_entry = consoles.console_view_entry(
        category = "linux",
        short_name = "hdls",
    ),
    notifies = ["headless-owners"],
    os = os.LINUX_BIONIC_SWITCH_TO_DEFAULT,
    goma_backend = None,
    reclient_jobs = rbe_jobs.DEFAULT,
    reclient_instance = rbe_instance.DEFAULT,
)

ci.builder(
    name = "mac-paeverywhere-x64-fyi-dbg",
    builderless = True,
    console_view_entry = consoles.console_view_entry(
        category = "paeverywhere|mac",
        short_name = "64dbg",
    ),
    cores = None,
    notifies = ["chrome-memory-safety"],
    os = os.MAC_ANY,
)

ci.builder(
    name = "mac-paeverywhere-x64-fyi-rel",
    builderless = True,
    console_view_entry = consoles.console_view_entry(
        category = "paeverywhere|mac",
        short_name = "64rel",
    ),
    cores = None,
    notifies = ["chrome-memory-safety"],
    os = os.MAC_ANY,
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
)

ci.builder(
    name = "win-pixel-builder-rel",
    console_view_entry = consoles.console_view_entry(
        category = "win10",
    ),
    os = os.WINDOWS_10,
)

ci.builder(
    name = "win-pixel-tester-rel",
    console_view_entry = consoles.console_view_entry(
        category = "win10",
    ),
    os = os.LINUX_BIONIC_SWITCH_TO_DEFAULT,
    triggered_by = ["win-pixel-builder-rel"],
)

ci.builder(
    name = "linux-upload-perfetto",
    console_view_entry = consoles.console_view_entry(
        category = "perfetto",
        short_name = "lnx",
    ),
    os = os.LINUX_BIONIC_SWITCH_TO_DEFAULT,
    goma_backend = None,
    reclient_jobs = rbe_jobs.DEFAULT,
    reclient_instance = rbe_instance.DEFAULT,
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
)

ci.builder(
    name = "Comparison Android (reclient)",
    console_view_entry = consoles.console_view_entry(
        category = "android",
        short_name = "cmp",
    ),
    goma_jobs = 250,
    executable = "recipe:reclient_goma_comparison",
    execution_timeout = 10 * time.hour,
    reclient_cache_silo = "Comparison Android - cache siloed",
    reclient_instance = rbe_instance.DEFAULT,
    reclient_jobs = 250,
    os = os.LINUX_BIONIC_SWITCH_TO_DEFAULT,
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
    reclient_instance = rbe_instance.DEFAULT,
    reclient_jobs = 250,
    os = os.LINUX_BIONIC_SWITCH_TO_DEFAULT,
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
    reclient_instance = rbe_instance.DEFAULT,
    reclient_jobs = 80,
    os = os.WINDOWS_DEFAULT,
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
    reclient_instance = rbe_instance.DEFAULT,
    reclient_jobs = 250,
    os = os.WINDOWS_DEFAULT,
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
    reclient_instance = rbe_instance.DEFAULT,
    reclient_jobs = 250,
    os = os.LINUX_BIONIC_SWITCH_TO_DEFAULT,
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
    reclient_instance = rbe_instance.DEFAULT,
    reclient_jobs = 500,
    os = os.LINUX_BIONIC_SWITCH_TO_DEFAULT,
    schedule = "triggered",
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
    os = os.LINUX_BIONIC_SWITCH_TO_DEFAULT,
    reclient_jobs = 400,
    reclient_instance = rbe_instance.DEFAULT,
)
# End - Reclient migration, phase 2, block 1 shadow builders

ci.builder(
    name = "Win ASan Release (reclient shadow)",
    builderless = True,
    console_view_entry = consoles.console_view_entry(
        category = "win asan",
        short_name = "rel",
    ),
    os = os.WINDOWS_DEFAULT,
    goma_backend = None,
    reclient_jobs = 80,
    reclient_instance = rbe_instance.DEFAULT,
)

ci.builder(
    name = "Win ASan Release Media (reclient shadow)",
    builderless = True,
    console_view_entry = consoles.console_view_entry(
        category = "win asan",
        short_name = "med",
    ),
    os = os.WINDOWS_DEFAULT,
    goma_backend = None,
    reclient_jobs = 80,
    reclient_instance = rbe_instance.DEFAULT,
)

ci.builder(
    name = "Win x64 Builder (dbg) (goma cache silo)",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "goma_enable_cache_silo",
                "mb",
            ],
            build_config = builder_config.build_config.DEBUG,
            target_bits = 64,
        ),
    ),
    builderless = True,
    console_view_entry = consoles.console_view_entry(
        category = "debug|builder",
        short_name = "64",
    ),
    cores = 32,
    os = os.WINDOWS_ANY,
)

ci.builder(
    name = "Win x64 Builder (dbg) (reclient shadow)",
    builderless = True,
    console_view_entry = consoles.console_view_entry(
        category = "debug|builder",
        short_name = "64",
    ),
    cores = 32,
    os = os.WINDOWS_ANY,
    goma_backend = None,
    reclient_jobs = rbe_jobs.DEFAULT,
    reclient_instance = rbe_instance.DEFAULT,
)

ci.builder(
    name = "win-archive-dbg (goma cache silo)",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "clobber",
                "goma_enable_cache_silo",
                "mb",
            ],
            build_config = builder_config.build_config.DEBUG,
            target_bits = 64,
        ),
    ),
    builderless = True,
    console_view_entry = consoles.console_view_entry(
        category = "win|dbg",
        short_name = "64",
    ),
    cores = 32,
    os = os.WINDOWS_DEFAULT,
)

ci.builder(
    name = "win-archive-dbg (reclient shadow)",
    builderless = True,
    console_view_entry = consoles.console_view_entry(
        category = "win|dbg",
        short_name = "64",
    ),
    cores = 32,
    os = os.WINDOWS_DEFAULT,
    goma_backend = None,
    reclient_jobs = rbe_jobs.DEFAULT,
    reclient_instance = rbe_instance.DEFAULT,
)

ci.builder(
    name = "win-archive-rel (goma cache silo)",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "clobber",
                "goma_enable_cache_silo",
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
        ),
    ),
    builderless = True,
    console_view_entry = consoles.console_view_entry(
        category = "win|rel",
        short_name = "64",
    ),
    cores = 32,
    os = os.WINDOWS_DEFAULT,
)

ci.builder(
    name = "win-archive-rel (reclient shadow)",
    builderless = True,
    console_view_entry = consoles.console_view_entry(
        category = "win|rel",
        short_name = "64",
    ),
    cores = 32,
    os = os.WINDOWS_DEFAULT,
    goma_backend = None,
    reclient_jobs = rbe_jobs.DEFAULT,
    reclient_instance = rbe_instance.DEFAULT,
)

ci.builder(
    name = "Win x64 Builder (reclient)",
    builderless = True,
    console_view_entry = consoles.console_view_entry(
        category = "win",
        short_name = "re",
    ),
    cores = 32,
    goma_backend = None,
    reclient_instance = rbe_instance.DEFAULT,
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
    reclient_instance = rbe_instance.DEFAULT,
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
    reclient_instance = rbe_instance.DEFAULT,
    reclient_profiler_service = "reclient-win",
    reclient_publish_trace = True,
    os = os.WINDOWS_DEFAULT,
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
    reclient_instance = rbe_instance.DEFAULT,
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
    reclient_instance = rbe_instance.DEFAULT,
    reclient_rewrapper_env = {"RBE_compare": "true"},
    reclient_ensure_verified = True,
    description_html = "verify artifacts. should be removed after the migration. crbug.com/1260232",
)

fyi_mac_builder(
    name = "mac-arm64-on-arm64-rel-reclient",

    # same with mac-arm64-on-arm64-rel
    cores = None,  # crbug.com/1245114
    cpu = cpu.ARM64,
    os = os.MAC_11,
    console_view_entry = consoles.console_view_entry(
        category = "mac",
        short_name = "re",
    ),
    goma_backend = None,
    reclient_instance = rbe_instance.DEFAULT,
    description_html = "experiment reclient on mac-arm. should be removed after the migration. crbug.com/1252626",
)

ci.builder(
    name = "chromeos-amd64-generic-rel (goma cache silo)",
    console_view_entry = consoles.console_view_entry(
        category = "cros x64",
        short_name = "cgc",
    ),
    os = os.LINUX_BIONIC_SWITCH_TO_DEFAULT,
)

ci.builder(
    name = "chromeos-amd64-generic-rel (reclient)",
    console_view_entry = consoles.console_view_entry(
        category = "cros x64",
    ),
    goma_backend = None,
    reclient_instance = rbe_instance.DEFAULT,
    os = os.LINUX_BIONIC_SWITCH_TO_DEFAULT,
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
    reclient_instance = rbe_instance.DEFAULT,
    os = os.LINUX_BIONIC_SWITCH_TO_DEFAULT,
    reclient_rewrapper_env = {"RBE_compare": "true"},
    reclient_ensure_verified = True,
    description_html = "verify artifacts. should be removed after the migration. crbug.com/1235218",
)

ci.builder(
    name = "lacros-amd64-generic-rel (goma cache silo)",
    console_view_entry = consoles.console_view_entry(
        category = "lacros x64",
        short_name = "cgc",
    ),
    os = os.LINUX_BIONIC_SWITCH_TO_DEFAULT,
)

ci.builder(
    name = "lacros-amd64-generic-rel (reclient)",
    console_view_entry = consoles.console_view_entry(
        category = "lacros x64",
    ),
    goma_backend = None,
    reclient_instance = rbe_instance.DEFAULT,
    os = os.LINUX_BIONIC_SWITCH_TO_DEFAULT,
    reclient_rewrapper_env = {"RBE_cache_silo": "lacros-amd64-generic-rel (reclient)"},
)

ci.builder(
    name = "linux-lacros-builder-rel (goma cache silo)",
    console_view_entry = consoles.console_view_entry(
        category = "lacros rel",
        short_name = "cgc",
    ),
    os = os.LINUX_BIONIC_SWITCH_TO_DEFAULT,
)

ci.builder(
    name = "linux-lacros-builder-rel (reclient)",
    console_view_entry = consoles.console_view_entry(
        category = "lacros rel",
    ),
    goma_backend = None,
    reclient_instance = rbe_instance.DEFAULT,
    os = os.LINUX_BIONIC_SWITCH_TO_DEFAULT,
    reclient_rewrapper_env = {"RBE_cache_silo": "linux-lacros-builder-rel (reclient)"},
)

fyi_celab_builder(
    name = "win-celab-builder-rel",
    console_view_entry = consoles.console_view_entry(
        category = "celab",
    ),
    schedule = "0 0,6,12,18 * * *",
    triggered_by = [],
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
    os = os.LINUX_BIONIC_SWITCH_TO_DEFAULT,
    coverage_test_types = ["overall", "unit"],
    schedule = "triggered",
    triggered_by = [],
    use_java_coverage = True,
    goma_backend = None,
    reclient_jobs = rbe_jobs.HIGH_JOBS_FOR_CI,
    reclient_instance = rbe_instance.DEFAULT,
)

fyi_coverage_builder(
    name = "android-code-coverage-native",
    console_view_entry = consoles.console_view_entry(
        category = "code_coverage",
        short_name = "ann",
    ),
    os = os.LINUX_BIONIC_SWITCH_TO_DEFAULT,
    use_clang_coverage = True,
    coverage_test_types = ["overall", "unit"],
    goma_backend = None,
    reclient_jobs = rbe_jobs.HIGH_JOBS_FOR_CI,
    reclient_instance = rbe_instance.DEFAULT,
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
    os = os.LINUX_BIONIC_SWITCH_TO_DEFAULT,
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
    os = os.MAC_11,
    use_clang_coverage = True,
    coverage_exclude_sources = "ios_test_files_and_test_utils",
    coverage_test_types = ["overall", "unit"],
    xcode = xcode.x13main,
)

fyi_coverage_builder(
    name = "linux-chromeos-code-coverage",
    console_view_entry = consoles.console_view_entry(
        category = "code_coverage",
        short_name = "lcr",
    ),
    os = os.LINUX_BIONIC_SWITCH_TO_DEFAULT,
    use_clang_coverage = True,
    coverage_test_types = ["overall", "unit"],
    schedule = "triggered",
    triggered_by = [],
)

fyi_coverage_builder(
    name = "linux-chromeos-js-code-coverage",
    console_view_entry = consoles.console_view_entry(
        category = "code_coverage",
        short_name = "jcr",
    ),
    os = os.LINUX_BIONIC_SWITCH_TO_DEFAULT,
    use_javascript_coverage = True,
    schedule = "triggered",
    triggered_by = [],
)

fyi_coverage_builder(
    name = "linux-code-coverage",
    console_view_entry = consoles.console_view_entry(
        category = "code_coverage",
        short_name = "lnx",
    ),
    os = os.LINUX_BIONIC_SWITCH_TO_DEFAULT,
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
    os = os.LINUX_BIONIC_SWITCH_TO_DEFAULT,
    use_clang_coverage = True,
    coverage_test_types = ["overall", "unit"],
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
)

fyi_ios_builder(
    name = "ios-asan",
    console_view_entry = consoles.console_view_entry(
        category = "iOS",
        short_name = "asan",
    ),
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
    reclient_instance = rbe_instance.DEFAULT,
    description_html = "experiment reclient for ios. remove after the migration. crbug.com/1254986",
)

fyi_ios_builder(
    name = "ios-simulator-cronet",
    branch_selector = branches.STANDARD_MILESTONE,
    console_view_entry = consoles.console_view_entry(
        category = "cronet",
    ),
    cq_mirrors_console_view = "mirrors",
    notifies = ["cronet"],
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
    name = "ios14-beta-simulator",
    console_view_entry = consoles.console_view_entry(
        category = "iOS|iOS14",
        short_name = "ios14",
    ),
    os = os.MAC_11,
    schedule = "0 0,4,8,12,16,20 * * *",
    triggered_by = [],
)

fyi_ios_builder(
    name = "ios14-sdk-simulator",
    console_view_entry = consoles.console_view_entry(
        category = "iOS|iOS14",
        short_name = "sdk14",
    ),
    os = os.MAC_11,
    cpu = cpu.ARM64,
    schedule = "0 2,6,10,14,18,22 * * *",
    triggered_by = [],
)

fyi_ios_builder(
    name = "ios15-beta-simulator",
    console_view_entry = [
        consoles.console_view_entry(
            category = "iOS|iOS15",
            short_name = "ios15",
        ),
    ],
    os = os.MAC_11,
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
    xcode = xcode.x13betabots,
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
    xcode = xcode.x13betabots,
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
)

ci.builder(
    name = "Win11 Tests x64",
    builderless = True,
    console_view_entry = consoles.console_view_entry(
        category = "win11",
    ),
    goma_backend = None,
    main_console_view = None,
    os = os.WINDOWS_10,
    triggered_by = ["ci/Win x64 Builder"],
)

ci.builder(
    name = "win32-arm64-rel",
    console_view_entry = consoles.console_view_entry(
        category = "win32|arm64",
    ),
    cpu = cpu.X86,
    goma_jobs = goma.jobs.J150,
    os = os.WINDOWS_DEFAULT,
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
)
