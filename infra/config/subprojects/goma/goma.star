# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

load("//lib/builder_config.star", "builder_config")
load("//lib/builders.star", "builder", "builders", "cpu", "defaults", "goma", "os")
load("//lib/gn_args.star", "gn_args")
load("//lib/structs.star", "structs")

luci.bucket(
    name = "goma",
    acls = [
        acl.entry(
            roles = acl.BUILDBUCKET_READER,
            groups = "all",
        ),
        acl.entry(
            roles = acl.BUILDBUCKET_TRIGGERER,
            groups = "project-chromium-ci-schedulers",
        ),
        acl.entry(
            roles = acl.BUILDBUCKET_OWNER,
            groups = "project-chromium-admins",
        ),
    ],
)

defaults.bucket.set("goma")
defaults.build_numbers.set(True)
defaults.cores.set(8)
defaults.cpu.set(cpu.X86_64)
defaults.executable.set("recipe:chromium")
defaults.execution_timeout.set(3 * time.hour)
defaults.os.set(os.LINUX_DEFAULT)
defaults.pool.set("luci.chromium.ci")
defaults.free_space.set(builders.free_space.standard)
defaults.service_account.set(
    "goma-release-testing@chops-service-accounts.iam.gserviceaccount.com",
)
defaults.triggered_by.set(["chromium-gitiles-trigger"])

# Builders appear after the function used to define them, with all builders
# defined using the same function ordered lexicographically by name
# Builder functions are defined in lexicographic order by name ignoring the
# '_builder' suffix

# Builder functions are defined for each builder group that goma builders appear
# in, with additional functions for specializing on OS or goma grouping (canary,
# latest client, etc.): XXX_YYY_builder where XXX is the part after the last dot
# in the builder group and YYY is the OS or goma grouping

def fyi_goma_rbe_canary_builder(
        *,
        name,
        goma_backend = goma.backend.RBE_PROD,
        os = os.LINUX_DEFAULT,
        **kwargs):
    return builder(
        name = name,
        builder_group = "chromium.goma.fyi",
        execution_timeout = 10 * time.hour,
        goma_backend = goma_backend,
        os = os,
        **kwargs
    )

fyi_goma_rbe_canary_builder(
    name = "chromeos-amd64-generic-rel-goma-rbe-canary",
    builder_spec = builder_config.copy_from(
        "ci/chromeos-amd64-generic-rel",
        lambda spec: structs.evolve(
            spec,
            chromium_config = structs.extend(
                spec.chromium_config,
                apply_configs = "goma_canary",
            ),
            build_gs_bucket = "chromium-fyi-archive",
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "chromeos_device",
            "ozone_headless",
            "dcheck_off",
            "amd64-generic-vm",
            "use_fake_dbus_clients",
            "also_build_lacros_chrome_for_architecture_amd64",
            "goma",
            "no_reclient",
        ],
    ),
    goma_enable_ats = True,
)

fyi_goma_rbe_canary_builder(
    name = "Win Builder (dbg) Goma RBE Canary",
    builder_spec = builder_config.copy_from(
        "ci/Win Builder (dbg)",
        lambda spec: structs.evolve(
            spec,
            chromium_config = structs.extend(
                spec.chromium_config,
                apply_configs = [
                    "goma_canary",
                ],
            ),
            build_gs_bucket = "chromium-fyi-archive",
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "gpu_tests",
            "debug_builder",
            "goma",
            "x86",
            "no_symbols",
        ],
    ),
    builderless = False,
    os = os.WINDOWS_DEFAULT,
    goma_enable_ats = False,
)

fyi_goma_rbe_canary_builder(
    name = "Win Builder Goma RBE Canary",
    builder_spec = builder_config.copy_from(
        "ci/Win Builder",
        lambda spec: structs.evolve(
            spec,
            chromium_config = structs.extend(
                spec.chromium_config,
                apply_configs = [
                    "goma_canary",
                    "goma_use_local",
                ],
            ),
            build_gs_bucket = "chromium-fyi-archive",
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "gpu_tests",
            "release_builder",
            "goma",
            "x86",
            "minimal_symbols",
        ],
    ),
    builderless = False,
    os = os.WINDOWS_DEFAULT,
    goma_enable_ats = False,
)

def goma_builder(
        *,
        name,
        builderless = False,
        os = os.LINUX_DEFAULT,
        **kwargs):
    return builder(
        name = name,
        builder_group = "chromium.goma",
        builderless = builderless,
        os = os,
        **kwargs
    )

goma_builder(
    name = "chromeos-amd64-generic-rel-goma-rbe-staging",
    builder_spec = builder_config.copy_from("ci/chromeos-amd64-generic-rel"),
    gn_args = gn_args.config(
        configs = [
            "chromeos_device",
            "ozone_headless",
            "dcheck_off",
            "amd64-generic-vm",
            "use_fake_dbus_clients",
            "also_build_lacros_chrome_for_architecture_amd64",
            "goma",
            "no_reclient",
        ],
    ),
    goma_backend = goma.backend.RBE_STAGING,
    goma_enable_ats = True,
)

def goma_windows_builder(
        *,
        name,
        goma_enable_ats = True,
        **kwargs):
    kwargs["execution_timeout"] = 4 * time.hour
    return goma_builder(
        name = name,
        goma_enable_ats = goma_enable_ats,
        os = os.WINDOWS_DEFAULT,
        cores = 16,
        **kwargs
    )

goma_windows_builder(
    name = "Chromium Win Goma RBE Staging",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(config = "chromium"),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "mb",
                "goma_failfast",
            ],
            target_bits = 64,
            target_platform = builder_config.target_platform.WIN,
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "release_builder",
            "goma",
            "x86",
            "minimal_symbols",
        ],
    ),
    goma_backend = goma.backend.RBE_STAGING,
    goma_enable_ats = False,
)
