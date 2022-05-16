# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

load("//lib/builder_config.star", "builder_config")
load("//lib/builders.star", "builder", "cpu", "defaults", "goma", "os", "xcode")
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
    name = "Linux Builder Goma RBE Canary",
    builder_spec = builder_config.copy_from(
        "ci/Linux Builder",
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
)

fyi_goma_rbe_canary_builder(
    name = "Mac Builder (dbg) Goma RBE Canary (clobber)",
    builder_spec = builder_config.copy_from(
        "ci/Mac Builder (dbg)",
        lambda spec: structs.evolve(
            spec,
            chromium_config = structs.extend(
                spec.chromium_config,
                apply_configs = [
                    "goma_canary",
                    "clobber",
                ],
            ),
            build_gs_bucket = "chromium-fyi-archive",
        ),
    ),
    cores = 4,
    goma_jobs = goma.jobs.J80,
    os = os.MAC_DEFAULT,
)

fyi_goma_rbe_canary_builder(
    name = "Mac M1 Builder (dbg) Goma RBE Canary (clobber)",
    builder_spec = builder_config.copy_from(
        "ci/Mac Builder (dbg)",
        lambda spec: structs.evolve(
            spec,
            chromium_config = structs.extend(
                spec.chromium_config,
                apply_configs = [
                    "goma_canary",
                    "clobber",
                ],
            ),
            build_gs_bucket = "chromium-fyi-archive",
        ),
    ),
    cores = None,
    goma_jobs = goma.jobs.J80,
    os = os.MAC_11,
    cpu = cpu.ARM64,
)

fyi_goma_rbe_canary_builder(
    name = "android-archive-dbg-goma-rbe-ats-canary",
    goma_enable_ats = True,
)

fyi_goma_rbe_canary_builder(
    name = "android-archive-dbg-goma-rbe-canary",
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
    goma_enable_ats = True,
)

fyi_goma_rbe_canary_builder(
    name = "ios-device-goma-rbe-canary-clobber",
    builder_spec = builder_config.copy_from(
        "ci/ios-device",
        lambda spec: structs.evolve(
            spec,
            chromium_config = structs.extend(
                spec.chromium_config,
                apply_configs = [
                    "goma_canary",
                    "clobber",
                ],
            ),
            build_gs_bucket = "chromium-fyi-archive",
        ),
    ),
    cores = None,
    os = os.MAC_11,
    xcode = xcode.x13main,
)

fyi_goma_rbe_canary_builder(
    name = "linux-archive-rel-goma-rbe-ats-canary",
    builder_spec = builder_config.copy_from(
        "ci/linux-archive-rel",
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
    goma_enable_ats = True,
)

fyi_goma_rbe_canary_builder(
    name = "linux-archive-rel-goma-rbe-canary",
    builder_spec = builder_config.copy_from(
        "ci/linux-archive-rel",
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
)

fyi_goma_rbe_canary_builder(
    name = "mac-archive-rel-goma-rbe-canary",
    builder_spec = builder_config.copy_from(
        "ci/mac-archive-rel",
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
    cores = 4,
    goma_jobs = goma.jobs.J80,
    os = os.MAC_DEFAULT,
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
    goma_enable_ats = False,
    os = os.WINDOWS_DEFAULT,
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
    goma_enable_ats = False,
    os = os.WINDOWS_DEFAULT,
)

fyi_goma_rbe_canary_builder(
    name = "Win Builder Goma RBE Canary (clobber)",
    builder_spec = builder_config.copy_from(
        "ci/Win Builder",
        lambda spec: structs.evolve(
            spec,
            chromium_config = structs.extend(
                spec.chromium_config,
                apply_configs = [
                    "goma_canary",
                    "goma_use_local",
                    "clobber",
                ],
            ),
            build_gs_bucket = "chromium-fyi-archive",
        ),
    ),
    goma_enable_ats = False,
    os = os.WINDOWS_DEFAULT,
)

fyi_goma_rbe_canary_builder(
    name = "Win Builder (dbg) Goma RBE ATS Canary",
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
    goma_enable_ats = True,
    os = os.WINDOWS_DEFAULT,
)

fyi_goma_rbe_canary_builder(
    name = "Win Builder Goma RBE ATS Canary",
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
    goma_enable_ats = True,
    os = os.WINDOWS_DEFAULT,
)

def fyi_goma_rbe_latest_client_builder(
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

fyi_goma_rbe_latest_client_builder(
    name = "Linux Builder Goma RBE Latest Client",
    builder_spec = builder_config.copy_from(
        "ci/Linux Builder",
        lambda spec: structs.evolve(
            spec,
            chromium_config = structs.extend(
                spec.chromium_config,
                apply_configs = [
                    "goma_latest_client",
                    "goma_use_local",
                ],
            ),
            build_gs_bucket = "chromium-fyi-archive",
        ),
    ),
)

fyi_goma_rbe_latest_client_builder(
    name = "Mac Builder (dbg) Goma RBE Latest Client (clobber)",
    builder_spec = builder_config.copy_from(
        "ci/Mac Builder (dbg)",
        lambda spec: structs.evolve(
            spec,
            chromium_config = structs.extend(
                spec.chromium_config,
                apply_configs = [
                    "goma_latest_client",
                    "clobber",
                ],
            ),
            build_gs_bucket = "chromium-fyi-archive",
        ),
    ),
    cores = 4,
    goma_jobs = goma.jobs.J80,
    os = os.MAC_DEFAULT,
)

fyi_goma_rbe_latest_client_builder(
    name = "Win Builder (dbg) Goma RBE Latest Client",
    builder_spec = builder_config.copy_from(
        "ci/Win Builder (dbg)",
        lambda spec: structs.evolve(
            spec,
            chromium_config = structs.extend(
                spec.chromium_config,
                apply_configs = [
                    "goma_latest_client",
                ],
            ),
            build_gs_bucket = "chromium-fyi-archive",
        ),
    ),
    goma_enable_ats = False,
    os = os.WINDOWS_DEFAULT,
)

fyi_goma_rbe_latest_client_builder(
    name = "Win Builder Goma RBE Latest Client",
    builder_spec = builder_config.copy_from(
        "ci/Win Builder",
        lambda spec: structs.evolve(
            spec,
            chromium_config = structs.extend(
                spec.chromium_config,
                apply_configs = [
                    "goma_latest_client",
                    "goma_use_local",
                ],
            ),
            build_gs_bucket = "chromium-fyi-archive",
        ),
    ),
    goma_enable_ats = False,
    os = os.WINDOWS_DEFAULT,
)

fyi_goma_rbe_latest_client_builder(
    name = "Win Builder (dbg) Goma RBE ATS Latest Client",
    builder_spec = builder_config.copy_from(
        "ci/Win Builder (dbg)",
        lambda spec: structs.evolve(
            spec,
            chromium_config = structs.extend(
                spec.chromium_config,
                apply_configs = [
                    "goma_latest_client",
                ],
            ),
            build_gs_bucket = "chromium-fyi-archive",
        ),
    ),
    goma_enable_ats = True,
    os = os.WINDOWS_DEFAULT,
)

fyi_goma_rbe_latest_client_builder(
    name = "Win Builder Goma RBE ATS Latest Client",
    builder_spec = builder_config.copy_from(
        "ci/Win Builder",
        lambda spec: structs.evolve(
            spec,
            chromium_config = structs.extend(
                spec.chromium_config,
                apply_configs = [
                    "goma_latest_client",
                    "goma_use_local",
                ],
            ),
            build_gs_bucket = "chromium-fyi-archive",
        ),
    ),
    goma_enable_ats = True,
    os = os.WINDOWS_DEFAULT,
)

fyi_goma_rbe_latest_client_builder(
    name = "android-archive-dbg-goma-rbe-ats-latest",
    goma_enable_ats = True,
)

fyi_goma_rbe_latest_client_builder(
    name = "android-archive-dbg-goma-rbe-latest",
)

fyi_goma_rbe_latest_client_builder(
    name = "chromeos-amd64-generic-rel-goma-rbe-latest",
    builder_spec = builder_config.copy_from(
        "ci/chromeos-amd64-generic-rel",
        lambda spec: structs.evolve(
            spec,
            chromium_config = structs.extend(
                spec.chromium_config,
                apply_configs = "goma_latest_client",
            ),
            build_gs_bucket = "chromium-fyi-archive",
        ),
    ),
    goma_enable_ats = True,
)

fyi_goma_rbe_latest_client_builder(
    name = "ios-device-goma-rbe-latest-clobber",
    builder_spec = builder_config.copy_from(
        "ci/ios-device",
        lambda spec: structs.evolve(
            spec,
            chromium_config = structs.extend(
                spec.chromium_config,
                apply_configs = [
                    "goma_latest_client",
                    "clobber",
                ],
            ),
            build_gs_bucket = "chromium-fyi-archive",
        ),
    ),
    cores = None,
    os = os.MAC_11,
    xcode = xcode.x13main,
)

fyi_goma_rbe_latest_client_builder(
    name = "linux-archive-rel-goma-rbe-ats-latest",
    builder_spec = builder_config.copy_from(
        "ci/linux-archive-rel",
        lambda spec: structs.evolve(
            spec,
            chromium_config = structs.extend(
                spec.chromium_config,
                apply_configs = [
                    "goma_latest_client",
                ],
            ),
            build_gs_bucket = "chromium-fyi-archive",
        ),
    ),
    goma_enable_ats = True,
)

fyi_goma_rbe_latest_client_builder(
    name = "linux-archive-rel-goma-rbe-latest",
    builder_spec = builder_config.copy_from(
        "ci/linux-archive-rel",
        lambda spec: structs.evolve(
            spec,
            chromium_config = structs.extend(
                spec.chromium_config,
                apply_configs = [
                    "goma_latest_client",
                ],
            ),
            build_gs_bucket = "chromium-fyi-archive",
        ),
    ),
)

fyi_goma_rbe_latest_client_builder(
    name = "mac-archive-rel-goma-rbe-latest",
    builder_spec = builder_config.copy_from(
        "ci/mac-archive-rel",
        lambda spec: structs.evolve(
            spec,
            chromium_config = structs.extend(
                spec.chromium_config,
                apply_configs = [
                    "goma_latest_client",
                ],
            ),
            build_gs_bucket = "chromium-fyi-archive",
        ),
    ),
    cores = 4,
    goma_jobs = goma.jobs.J80,
    os = os.MAC_DEFAULT,
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
    name = "Chromium Android ARM 32-bit Goma RBE Staging",
    goma_backend = goma.backend.RBE_STAGING,
    execution_timeout = 4 * time.hour,
)

goma_builder(
    name = "Chromium Android ARM 32-bit Goma RBE ToT",
    goma_backend = goma.backend.RBE_TOT,
    goma_enable_ats = False,
    execution_timeout = 4 * time.hour,
)

goma_builder(
    name = "Chromium Android ARM 32-bit Goma RBE ToT (ATS)",
    goma_backend = goma.backend.RBE_TOT,
    goma_enable_ats = True,
    execution_timeout = 4 * time.hour,
)

goma_builder(
    name = "Chromium Linux Goma RBE Staging",
    goma_backend = goma.backend.RBE_STAGING,
)

goma_builder(
    name = "Chromium Linux Goma RBE Staging (clobber)",
    goma_backend = goma.backend.RBE_STAGING,
)

goma_builder(
    name = "Chromium Linux Goma RBE Staging (dbg)",
    goma_backend = goma.backend.RBE_STAGING,
)

goma_builder(
    name = "Chromium Linux Goma RBE Staging (dbg) (clobber)",
    goma_backend = goma.backend.RBE_STAGING,
)

goma_builder(
    name = "chromeos-amd64-generic-rel-goma-rbe-staging",
    builder_spec = builder_config.copy_from("ci/chromeos-amd64-generic-rel"),
    goma_backend = goma.backend.RBE_STAGING,
    goma_enable_ats = True,
)

goma_builder(
    name = "Chromium Linux Goma RBE ToT",
    goma_backend = goma.backend.RBE_TOT,
    goma_enable_ats = False,
)

goma_builder(
    name = "Chromium Linux Goma RBE ToT (ATS)",
    goma_backend = goma.backend.RBE_TOT,
    goma_enable_ats = True,
)

goma_builder(
    name = "chromeos-amd64-generic-rel-goma-rbe-tot",
    builder_spec = builder_config.copy_from(
        "ci/chromeos-amd64-generic-rel",
        lambda spec: structs.evolve(
            spec,
            chromium_config = structs.extend(
                spec.chromium_config,
                apply_configs = ["goma_client_candidate"],
            ),
        ),
    ),
    goma_backend = goma.backend.RBE_TOT,
    goma_enable_ats = True,
)

def goma_mac_builder(
        *,
        name,
        os = os.MAC_DEFAULT,
        **kwargs):
    return goma_builder(
        name = name,
        cores = 4,
        goma_jobs = goma.jobs.J80,
        os = os,
        **kwargs
    )

goma_mac_builder(
    name = "Chromium iOS Goma RBE ToT",
    goma_backend = goma.backend.RBE_TOT,
    os = os.MAC_11,
    xcode = xcode.x13main,
)

goma_mac_builder(
    name = "Chromium Mac Goma RBE Staging",
    goma_backend = goma.backend.RBE_STAGING,
)

goma_mac_builder(
    name = "Chromium Mac Goma RBE Staging (clobber)",
    goma_backend = goma.backend.RBE_STAGING,
)

goma_mac_builder(
    name = "Chromium Mac Goma RBE Staging (dbg)",
    goma_backend = goma.backend.RBE_STAGING,
)

goma_mac_builder(
    name = "Chromium Mac Goma RBE ToT",
    goma_backend = goma.backend.RBE_TOT,
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
        **kwargs
    )

goma_windows_builder(
    name = "Chromium Win Goma RBE Staging",
    goma_backend = goma.backend.RBE_STAGING,
    goma_enable_ats = False,
)

goma_windows_builder(
    name = "Chromium Win Goma RBE Staging (clobber)",
    goma_backend = goma.backend.RBE_STAGING,
    goma_enable_ats = False,
)

goma_windows_builder(
    name = "Chromium Win Goma RBE ToT",
    goma_backend = goma.backend.RBE_TOT,
    goma_enable_ats = False,
)

goma_windows_builder(
    name = "Chromium Win Goma RBE ATS Staging",
    goma_backend = goma.backend.RBE_STAGING,
    goma_enable_ats = True,
)

goma_windows_builder(
    name = "Chromium Win Goma RBE ATS Staging (clobber)",
    goma_backend = goma.backend.RBE_STAGING,
    goma_enable_ats = True,
)

goma_windows_builder(
    name = "Chromium Win Goma RBE ATS ToT",
    goma_backend = goma.backend.RBE_TOT,
    goma_enable_ats = True,
)
