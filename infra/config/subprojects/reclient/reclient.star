# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

load("//lib/builder_config.star", "builder_config")
load("//lib/builders.star", "builders", "cpu", "os", "xcode")
load("//lib/ci.star", "ci")
load("//lib/consoles.star", "consoles")
load("//lib/structs.star", "structs")
load("//console-header.star", "HEADER")

luci.bucket(
    name = "reclient",
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

ci.defaults.set(
    bucket = "reclient",
    build_numbers = True,
    builder_group = "chromium.reclient.fyi",
    cores = 8,
    cpu = cpu.X86_64,
    executable = "recipe:chromium",
    execution_timeout = 3 * time.hour,
    goma_backend = None,
    pool = "luci.chromium.ci",
    service_account = (
        "chromium-ci-builder@chops-service-accounts.iam.gserviceaccount.com"
    ),
    triggered_by = ["chromium-gitiles-trigger"],
    free_space = builders.free_space.standard,

    # TODO(crbug.com/1362440): remove this.
    omit_python2 = False,
)

consoles.console_view(
    name = "chromium.reclient.fyi",
    header = HEADER,
    include_experimental_builds = True,
    repo = "https://chromium.googlesource.com/chromium/src",
)

def fyi_reclient_staging_builder(
        *,
        name,
        console_view_category,
        reclient_instance = "rbe-chromium-%s",
        untrusted_service_account = (
            "chromium-cq-staging-builder@chops-service-accounts.iam.gserviceaccount.com"
        ),
        reclient_version = "staging",
        **kwargs):
    trusted_instance = reclient_instance % "trusted"
    unstrusted_instance = reclient_instance % "untrusted"
    return [
        ci.builder(
            name = name,
            description_html = "Builds chromium using the %s version of reclient and the %s rbe instance." %
                               (reclient_version, trusted_instance),
            reclient_instance = trusted_instance,
            console_view_entry = consoles.console_view_entry(
                category = "rbe|" + console_view_category,
                short_name = "rcs",
            ),
            **kwargs
        ),
        ci.builder(
            name = name + " untrusted",
            description_html = "Builds chromium using the %s version of reclient and the %s rbe instance." %
                               (reclient_version, unstrusted_instance),
            reclient_instance = unstrusted_instance,
            console_view_entry = consoles.console_view_entry(
                category = "rbecq|" + console_view_category,
                short_name = "rcs",
            ),
            service_account = untrusted_service_account,
            **kwargs
        ),
    ]

def fyi_reclient_test_builder(
        *,
        name,
        console_view_category,
        **kwargs):
    return fyi_reclient_staging_builder(
        name = name,
        console_view_category = console_view_category,
        reclient_instance = "rbe-chromium-%s-test",
        reclient_version = "test",
        untrusted_service_account = ci.DEFAULT_SERVICE_ACCOUNT,
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
    console_view_category = "linux",
    os = os.LINUX_DEFAULT,
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
    console_view_category = "linux",
    os = os.LINUX_DEFAULT,
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
    console_view_category = "mac",
    os = os.MAC_DEFAULT,
    builderless = True,
    cores = None,
    priority = 35,
    reclient_bootstrap_env = {
        "GLOG_vmodule": "bridge*=2",
    },
)

fyi_reclient_test_builder(
    name = "Mac Builder reclient test",
    builder_spec = builder_config.copy_from(
        "ci/Mac Builder",
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
    console_view_category = "mac",
    os = os.MAC_DEFAULT,
    builderless = True,
    cores = None,
    priority = 35,
    reclient_bootstrap_env = {
        "RBE_ip_timeout": "-1s",
        "GLOG_vmodule": "bridge*=2",
    },
    reclient_profiler_service = "reclient-mac",
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
    builderless = True,
    console_view_category = "win",
    cores = 32,
    execution_timeout = 5 * time.hour,
    os = os.WINDOWS_ANY,
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
    builderless = True,
    console_view_category = "win",
    cores = 32,
    execution_timeout = 5 * time.hour,
    os = os.WINDOWS_ANY,
)

fyi_reclient_staging_builder(
    name = "Simple Chrome Builder reclient staging",
    console_view_category = "linux",
    os = os.LINUX_DEFAULT,
    builder_spec = builder_config.builder_spec(
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = ["mb"],
            build_config = builder_config.build_config.RELEASE,
            target_arch = builder_config.target_arch.INTEL,
            target_bits = 64,
            target_platform = builder_config.target_platform.CHROMEOS,
            cros_boards_with_qemu_images = "amd64-generic-vm",
        ),
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = ["chromeos", "reclient_staging"],
        ),
    ),
)

fyi_reclient_test_builder(
    name = "Simple Chrome Builder reclient test",
    console_view_category = "linux",
    os = os.LINUX_DEFAULT,
    builder_spec = builder_config.builder_spec(
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = ["mb"],
            build_config = builder_config.build_config.RELEASE,
            target_arch = builder_config.target_arch.INTEL,
            target_bits = 64,
            target_platform = builder_config.target_platform.CHROMEOS,
            cros_boards_with_qemu_images = "amd64-generic-vm",
        ),
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = ["chromeos", "reclient_test"],
        ),
    ),
)

fyi_reclient_test_builder(
    name = "ios-simulator reclient test",
    builder_spec = builder_config.copy_from(
        "ci/ios-simulator",
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
    console_view_category = "ios",
    os = os.MAC_DEFAULT,
    builderless = True,
    cores = None,
    xcode = xcode.x13main,
    priority = 35,
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
    console_view_category = "ios",
    os = os.MAC_DEFAULT,
    builderless = True,
    cores = None,
    xcode = xcode.x13main,
    priority = 35,
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
    console_view_category = "mac",
    os = os.MAC_DEFAULT,
    builderless = True,
    cores = None,
    priority = 35,
)

fyi_reclient_test_builder(
    name = "mac-arm64-rel reclient test",
    builder_spec = builder_config.copy_from(
        "ci/mac-arm64-rel",
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
    console_view_category = "mac",
    os = os.MAC_DEFAULT,
    builderless = True,
    cores = None,
    priority = 35,
)
