# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

load("//console-header.star", "HEADER")
load("//lib/builder_config.star", "builder_config")
load("//lib/builders.star", "builders", "cpu", "os", "siso")
load("//lib/ci.star", "ci")
load("//lib/consoles.star", "consoles")
load("//lib/gn_args.star", "gn_args")
load("//lib/html.star", "linkify_builder")
load("//lib/structs.star", "structs")
load("//lib/xcode.star", "xcode")

luci.bucket(
    name = "reclient",
    acls = [
        acl.entry(
            roles = acl.BUILDBUCKET_READER,
            groups = "all",
        ),
        acl.entry(
            roles = acl.BUILDBUCKET_TRIGGERER,
            groups = [
                "project-chromium-ci-schedulers",
                "mdb/foundry-x-team",
            ],
        ),
        acl.entry(
            roles = acl.BUILDBUCKET_OWNER,
            groups = "project-chromium-admins",
        ),
    ],
)

ci.defaults.set(
    bucket = "reclient",
    executable = "recipe:chromium",
    triggered_by = ["chromium-gitiles-trigger"],
    builder_group = "chromium.reclient.fyi",
    pool = "luci.chromium.ci",
    cores = 8,
    cpu = cpu.X86_64,
    free_space = builders.free_space.standard,
    build_numbers = True,
    execution_timeout = 3 * time.hour,
    service_account = (
        "chromium-ci-builder@chops-service-accounts.iam.gserviceaccount.com"
    ),
    shadow_builderless = True,
    shadow_free_space = None,
    shadow_pool = "luci.chromium.try",
    shadow_service_account = "chromium-try-builder@chops-service-accounts.iam.gserviceaccount.com",
    siso_enabled = True,
)

luci.bucket(
    name = "reclient.shadow",
    shadows = "reclient",
    bindings = [
        luci.binding(
            roles = "role/buildbucket.creator",
            groups = ["mdb/foundry-x-team", "mdb/chrome-troopers"],
        ),
    ],
    dynamic = True,
)

consoles.console_view(
    name = "chromium.reclient.fyi",
    repo = "https://chromium.googlesource.com/chromium/src",
    header = HEADER,
    include_experimental_builds = True,
)

def fyi_reclient_staging_builder(
        *,
        name,
        console_view_category,
        siso_project = "rbe-chromium-%s",
        untrusted_service_account = (
            "chromium-cq-staging-builder@chops-service-accounts.iam.gserviceaccount.com"
        ),
        reclient_version = "staging",
        **kwargs):
    trusted_instance = siso_project % "trusted"
    unstrusted_instance = siso_project % "untrusted"
    reclient_bootstrap_env = kwargs.pop("reclient_bootstrap_env", {})

    reclient_bootstrap_env.update({
        # TODO(b/258210757) remove once long term breakpad plans are dertermined
        "GOMA_COMPILER_PROXY_ENABLE_CRASH_DUMP": "true",
        "GOMA_DEPS_CACHE_TABLE_THRESHOLD": "40000",
        "RBE_fast_log_collection": "true",
        "RBE_use_unified_uploads": "true",
    })

    reclient_rewrapper_env = kwargs.pop("reclient_rewrapper_env", {})
    reclient_rewrapper_env.update({
        "RBE_exec_timeout": "2m",
    })
    return [
        ci.builder(
            name = name,
            description_html = "Builds chromium using the %s version of reclient and the %s rbe instance." %
                               (reclient_version, trusted_instance),
            siso_project = trusted_instance,
            console_view_entry = consoles.console_view_entry(
                category = "rbe|" + console_view_category,
                short_name = "rcs",
            ),
            reclient_bootstrap_env = reclient_bootstrap_env,
            reclient_scandeps_server = True,
            reclient_rewrapper_env = reclient_rewrapper_env,
            **kwargs
        ),
        ci.builder(
            name = name + " untrusted",
            description_html = "Builds chromium using the %s version of reclient and the %s rbe instance." %
                               (reclient_version, unstrusted_instance),
            siso_project = unstrusted_instance,
            console_view_entry = consoles.console_view_entry(
                category = "rbecq|" + console_view_category,
                short_name = "rcs",
            ),
            service_account = untrusted_service_account,
            reclient_bootstrap_env = reclient_bootstrap_env,
            reclient_scandeps_server = True,
            reclient_rewrapper_env = reclient_rewrapper_env,
            **kwargs
        ),
    ]

def fyi_reclient_test_builder(
        *,
        name,
        console_view_category,
        **kwargs):
    reclient_bootstrap_env = kwargs.pop("reclient_bootstrap_env", {})
    reclient_bootstrap_env.update({
        "RBE_fast_log_collection": "true",
    })
    reclient_rewrapper_env = kwargs.pop("reclient_rewrapper_env", {})

    reclient_rewrapper_env.update({
        "RBE_exec_timeout": "15m",
    })
    return fyi_reclient_staging_builder(
        name = name,
        console_view_category = console_view_category,
        siso_project = "rbe-chromium-%s-test",
        reclient_version = "test",
        untrusted_service_account = ci.DEFAULT_SERVICE_ACCOUNT,
        reclient_bootstrap_env = reclient_bootstrap_env,
        reclient_rewrapper_env = reclient_rewrapper_env,
        **kwargs
    )

fyi_reclient_staging_builder(
    name = "Linux Builder reclient staging",
    builder_spec = builder_config.copy_from(
        "ci/Linux Builder",
        lambda spec: structs.evolve(
            spec,
            gclient_config = structs.extend(
                spec.gclient_config,
                apply_configs = [
                    "reclient_staging",
                ],
            ),
            build_gs_bucket = "chromium-fyi-archive",
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "gpu_tests",
            "release_builder",
            "remoteexec",
            "linux",
            "x64",
        ],
    ),
    os = os.LINUX_DEFAULT,
    console_view_category = "linux",
)

fyi_reclient_test_builder(
    name = "Linux Builder reclient test",
    builder_spec = builder_config.copy_from(
        "ci/Linux Builder",
        lambda spec: structs.evolve(
            spec,
            gclient_config = structs.extend(
                spec.gclient_config,
                apply_configs = [
                    "reclient_test",
                ],
            ),
            build_gs_bucket = "chromium-fyi-archive",
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "gpu_tests",
            "release_builder",
            "remoteexec",
            "linux",
            "x64",
        ],
    ),
    os = os.LINUX_DEFAULT,
    console_view_category = "linux",
    reclient_rewrapper_env = {
        "RBE_compression_threshold": "0",
    },
)

fyi_reclient_test_builder(
    name = "Linux Builder reclient test (unified uploads)",
    builder_spec = builder_config.copy_from(
        "ci/Linux Builder",
        lambda spec: structs.evolve(
            spec,
            gclient_config = structs.extend(
                spec.gclient_config,
                apply_configs = [
                    "reclient_test",
                ],
            ),
            build_gs_bucket = "chromium-fyi-archive",
        ),
    ),
    gn_args = "reclient/Linux Builder reclient test",
    os = os.LINUX_DEFAULT,
    console_view_category = "linux",
    reclient_bootstrap_env = {
        "GLOG_use_unified_uploads": "true",
    },
)

fyi_reclient_test_builder(
    name = "Linux Builder reclient test (casng)",
    # Trigger manually when testing is needed.
    schedule = "triggered",
    triggered_by = [],
    builder_spec = builder_config.copy_from(
        "ci/Linux Builder",
        lambda spec: structs.evolve(
            spec,
            gclient_config = structs.extend(
                spec.gclient_config,
                apply_configs = [
                    "reclient_test",
                ],
            ),
            build_gs_bucket = "chromium-fyi-archive",
        ),
    ),
    gn_args = "reclient/Linux Builder reclient test",
    os = os.LINUX_DEFAULT,
    console_view_category = "linux",
    reclient_bootstrap_env = {
        "RBE_use_casng": "true",
    },
)

fyi_reclient_staging_builder(
    name = "Mac Builder reclient staging",
    builder_spec = builder_config.copy_from(
        "ci/Mac Builder",
        lambda spec: structs.evolve(
            spec,
            gclient_config = structs.extend(
                spec.gclient_config,
                apply_configs = [
                    "reclient_staging",
                ],
            ),
            build_gs_bucket = "chromium-fyi-archive",
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "gpu_tests",
            "release_builder",
            "remoteexec",
            "minimal_symbols",
            "mac",
            "x64",
        ],
    ),
    builderless = True,
    cores = None,
    os = os.MAC_DEFAULT,
    cpu = cpu.ARM64,
    console_view_category = "mac",
    priority = 35,
    reclient_bootstrap_env = {
        "GLOG_vmodule": "depsscannerclient.go=2,main.go=2",
    },
)

fyi_reclient_staging_builder(
    name = "Win x64 Builder reclient staging",
    builder_spec = builder_config.copy_from(
        "ci/Win x64 Builder",
        lambda spec: structs.evolve(
            spec,
            gclient_config = structs.extend(
                spec.gclient_config,
                apply_configs = [
                    "reclient_staging",
                ],
            ),
            build_gs_bucket = "chromium-fyi-archive",
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "gpu_tests",
            "release_builder",
            "remoteexec",
            "minimal_symbols",
            "win",
            "x64",
        ],
    ),
    builderless = True,
    cores = 32,
    os = os.WINDOWS_ANY,
    console_view_category = "win",
    execution_timeout = 5 * time.hour,
)

fyi_reclient_test_builder(
    name = "Win x64 Builder reclient test",
    builder_spec = builder_config.copy_from(
        "ci/Win x64 Builder",
        lambda spec: structs.evolve(
            spec,
            gclient_config = structs.extend(
                spec.gclient_config,
                apply_configs = [
                    "reclient_test",
                ],
            ),
            build_gs_bucket = "chromium-fyi-archive",
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "gpu_tests",
            "release_builder",
            "remoteexec",
            "minimal_symbols",
            "win",
            "x64",
        ],
    ),
    builderless = True,
    cores = 32,
    os = os.WINDOWS_ANY,
    console_view_category = "win",
    execution_timeout = 5 * time.hour,
    reclient_rewrapper_env = {
        "RBE_compression_threshold": "0",
    },
)

fyi_reclient_staging_builder(
    name = "Simple Chrome Builder reclient staging",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = ["chromeos", "reclient_staging"],
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = ["mb"],
            build_config = builder_config.build_config.RELEASE,
            target_arch = builder_config.target_arch.INTEL,
            target_bits = 64,
            target_platform = builder_config.target_platform.CHROMEOS,
            target_cros_boards = ["amd64-generic"],
            cros_boards_with_qemu_images = "amd64-generic-vm",
        ),
    ),
    gn_args = "reclient/Simple Chrome Builder reclient test",
    os = os.LINUX_DEFAULT,
    console_view_category = "linux",
)

fyi_reclient_test_builder(
    name = "Simple Chrome Builder reclient test",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = ["chromeos", "reclient_test"],
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = ["mb"],
            build_config = builder_config.build_config.RELEASE,
            target_arch = builder_config.target_arch.INTEL,
            target_bits = 64,
            target_platform = builder_config.target_platform.CHROMEOS,
            target_cros_boards = ["amd64-generic"],
            cros_boards_with_qemu_images = "amd64-generic-vm",
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "chromeos_device",
            "dcheck_off",
            "remoteexec",
            "amd64-generic-vm",
            "ozone_headless",
            "use_fake_dbus_clients",
            "x64",
        ],
    ),
    os = os.LINUX_DEFAULT,
    console_view_category = "linux",
    execution_timeout = 4 * time.hour,
)

fyi_reclient_staging_builder(
    name = "ios-simulator reclient staging",
    builder_spec = builder_config.copy_from(
        "ci/ios-simulator",
        lambda spec: structs.evolve(
            spec,
            gclient_config = structs.extend(
                spec.gclient_config,
                apply_configs = [
                    "reclient_staging",
                ],
            ),
            build_gs_bucket = "chromium-fyi-archive",
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "debug",
            "static",
            "minimal_symbols",
            "remoteexec",
            "ios_simulator",
            "x64",
            "xctest",
        ],
    ),
    builderless = True,
    cores = 12,
    os = os.MAC_DEFAULT,
    console_view_category = "ios",
    priority = 35,
    reclient_bootstrap_env = {
        "GLOG_vmodule": "bridge*=2",
    },
    xcode = xcode.xcode_default,
)

fyi_reclient_staging_builder(
    name = "mac-arm64-rel reclient staging",
    builder_spec = builder_config.copy_from(
        "ci/mac-arm64-rel",
        lambda spec: structs.evolve(
            spec,
            gclient_config = structs.extend(
                spec.gclient_config,
                apply_configs = [
                    "reclient_staging",
                ],
            ),
            build_gs_bucket = "chromium-fyi-archive",
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "arm64",
            "gpu_tests",
            "release_builder",
            "remoteexec",
            "minimal_symbols",
            "mac",
        ],
    ),
    builderless = True,
    cores = None,
    os = os.MAC_DEFAULT,
    cpu = cpu.ARM64,
    console_view_category = "mac",
    priority = 35,
    reclient_bootstrap_env = {
        "GLOG_vmodule": "depsscannerclient.go=2,main.go=2",
    },
)

ci.builder(
    name = "Comparison Linux (reclient vs reclient remote links)",
    executable = "recipe:reclient_reclient_comparison",
    gn_args = {
        "build1": gn_args.config(
            configs = [
                "gpu_tests",
                "release_builder",
                "remoteexec",
                "linux",
                "x64",
            ],
        ),
        "build2": gn_args.config(
            configs = [
                "gpu_tests",
                "release_builder",
                "reclient_with_remoteexec_links",
                "linux",
                "x64",
            ],
        ),
    },
    os = os.LINUX_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "linux",
        short_name = "cmp",
    ),
    execution_timeout = 6 * time.hour,
    reclient_bootstrap_env = {
        "GOMA_DEPS_CACHE_TABLE_THRESHOLD": "40000",
        "RBE_ip_reset_min_delay": "-1s",
        "RBE_clang_depscan_archive": "true",
        "RBE_fast_log_collection": "true",
    },
    reclient_cache_silo = "Comparison Linux remote links - cache siloed",
    siso_project = siso.project.TEST_TRUSTED,
    siso_remote_jobs = siso.remote_jobs.DEFAULT,
)

# The following 2 builders use the untrusted RBE instance because each instance has its own
# rewrapper configs and the trusted instance uses native windows rewrapper configs but the
# untrusted instance uses cross compile windows rewrapper configs.
# TODO(b/260228493) Remove once CI backend is switched
ci.builder(
    name = "Windows Cross deterministic",
    description_html = "verify artifacts. should be removed after the migration. b/260228493",
    executable = "recipe:swarming/deterministic_build",
    gn_args = {
        "local": gn_args.config(
            configs = ["release_builder", "x86", "minimal_symbols", "win"],
        ),
        "reclient": gn_args.config(
            configs = ["release_builder", "remoteexec", "x86", "minimal_symbols", "win"],
        ),
    },
    builderless = True,
    cores = 32,
    os = os.WINDOWS_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "win",
        short_name = "detcross",
    ),
    execution_timeout = 12 * time.hour,
    reclient_bootstrap_env = {
        "GOMA_DEPS_CACHE_TABLE_THRESHOLD": "40000",
        "RBE_fast_log_collection": "true",
    },
    service_account = "chromium-cq-staging-builder@chops-service-accounts.iam.gserviceaccount.com",
    siso_project = siso.project.DEFAULT_UNTRUSTED,
)

# TODO(b/260228493) Remove once CI backend is switched
ci.builder(
    name = "Win x64 Cross Builder (reclient compare)",
    description_html = "verify artifacts. should be removed after the migration. b/260228493",
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
            target_platform = builder_config.target_platform.WIN,
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "gpu_tests",
            "release_builder",
            "remoteexec",
            "minimal_symbols",
            "win",
            "x64",
        ],
    ),
    builderless = True,
    cores = 32,
    os = os.WINDOWS_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "win",
        short_name = "compcross",
    ),
    execution_timeout = 12 * time.hour,
    reclient_bootstrap_env = {
        "GOMA_DEPS_CACHE_TABLE_THRESHOLD": "40000",
        "RBE_fast_log_collection": "true",
    },
    reclient_disable_bq_upload = True,
    reclient_ensure_verified = True,
    reclient_rewrapper_env = {
        "RBE_compare": "true",
        "RBE_num_local_reruns": "1",
        "RBE_num_remote_reruns": "1",
    },
    service_account = "chromium-cq-staging-builder@chops-service-accounts.iam.gserviceaccount.com",
    siso_project = siso.project.DEFAULT_UNTRUSTED,
    siso_remote_jobs = None,
)

# TODO(b/276727069) Remove once developer rollout is done
ci.builder(
    name = "Linux Builder (canonical wd) (reclient compare)",
    description_html = "verify artifacts with canonicalize_working_dir enabled. should be removed after developer rollout. b/276727069",
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
    gn_args = gn_args.config(
        configs = [
            "gpu_tests",
            "debug_builder",
            "remoteexec",
            "linux",
            "x64",
        ],
    ),
    cores = 32,
    os = os.LINUX_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "linux",
        short_name = "compwd",
    ),
    execution_timeout = 14 * time.hour,
    reclient_bootstrap_env = {
        "GOMA_DEPS_CACHE_TABLE_THRESHOLD": "40000",
        "RBE_fast_log_collection": "true",
    },
    reclient_ensure_verified = True,
    reclient_rewrapper_env = {
        "RBE_compare": "true",
        "RBE_num_local_reruns": "1",
        "RBE_num_remote_reruns": "1",
        "RBE_compression_threshold": "4000000",
        "RBE_canonicalize_working_dir": "true",
        "RBE_cache_silo": "Linux Builder (canonical wd) (reclient compare)",
    },
    siso_project = siso.project.TEST_TRUSTED,
    siso_remote_jobs = None,
)

ci.builder(
    name = "Comparison Linux (reclient)",
    executable = "recipe:reclient_reclient_comparison",
    gn_args = {
        "build1": gn_args.config(
            configs = ["gpu_tests", "release_builder", "remoteexec", "linux", "x64"],
        ),
        "build2": gn_args.config(
            configs = ["gpu_tests", "release_builder", "remoteexec", "linux", "x64"],
        ),
    },
    os = os.LINUX_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "linux",
        short_name = "cmp",
    ),
    execution_timeout = 6 * time.hour,
    reclient_bootstrap_env = {
        "RBE_ip_reset_min_delay": "-1s",
        "RBE_fast_log_collection": "true",
    },
    reclient_cache_silo = "Comparison Linux - cache siloed",
    shadow_siso_project = siso.project.TEST_UNTRUSTED,
    siso_project = siso.project.TEST_TRUSTED,
    siso_remote_jobs = siso.remote_jobs.DEFAULT,
)

ci.builder(
    name = "Comparison Linux (reclient)(CQ)",
    description_html = """\
This builder measures Linux build performance with reclient prod vs test in cq configuration.<br/>\
The bot specs should be in sync with {}.\
""".format(linkify_builder("try", "linux-rel-compilator")),
    executable = "recipe:reclient_reclient_comparison",
    gn_args = {
        "build1": gn_args.config(
            configs = ["gpu_tests", "release_builder", "remoteexec", "linux", "x64"],
        ),
        "build2": gn_args.config(
            configs = ["gpu_tests", "release_builder", "remoteexec", "linux", "x64"],
        ),
    },
    cores = 16,
    os = os.LINUX_DEFAULT,
    ssd = True,
    console_view_entry = consoles.console_view_entry(
        category = "linux|cq",
        short_name = "cmp",
    ),
    execution_timeout = 6 * time.hour,
    reclient_bootstrap_env = {
        "RBE_ip_reset_min_delay": "-1s",
        "RBE_fast_log_collection": "true",
    },
    reclient_cache_silo = "Comparison Linux CQ - cache siloed",
    shadow_siso_project = siso.project.TEST_UNTRUSTED,
    siso_project = siso.project.TEST_UNTRUSTED,
    siso_remote_jobs = siso.remote_jobs.HIGH_JOBS_FOR_CQ,
)
