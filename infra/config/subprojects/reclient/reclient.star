# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

load("//lib/builder_config.star", "builder_config")
load("//lib/builders.star", "builders", "cpu", "os")
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
    executable = "recipe:chromium",
    triggered_by = ["chromium-gitiles-trigger"],
    builder_group = "chromium.reclient.fyi",
    pool = "luci.chromium.ci",
    cores = 8,
    cpu = cpu.X86_64,
    free_space = builders.free_space.standard,
    build_numbers = True,
    execution_timeout = 3 * time.hour,
    goma_backend = None,
    service_account = (
        "chromium-ci-builder@chops-service-accounts.iam.gserviceaccount.com"
    ),
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
        reclient_instance = "rbe-chromium-trusted",
        **kwargs):
    return ci.builder(
        name = name,
        reclient_instance = reclient_instance,
        console_view_entry = consoles.console_view_entry(
            category = "rbe|" + console_view_category,
            short_name = "rcs",
        ),
        **kwargs
    )

def fyi_reclient_test_builder(
        *,
        name,
        console_view_category,
        **kwargs):
    return fyi_reclient_staging_builder(
        name = name,
        console_view_category = console_view_category,
        reclient_instance = "goma-foundry-experiments",
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
    os = os.LINUX_DEFAULT,
    console_view_category = "linux",
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
                    "enable_reclient",
                    "reclient_staging",
                ],
            ),
            build_gs_bucket = "chromium-fyi-archive",
        ),
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
                    "enable_reclient",
                    "reclient_test",
                ],
            ),
            build_gs_bucket = "chromium-fyi-archive",
        ),
    ),
    builderless = True,
    cores = 32,
    os = os.WINDOWS_ANY,
    console_view_category = "win",
    execution_timeout = 5 * time.hour,
)

fyi_reclient_staging_builder(
    name = "Simple Chrome Builder reclient staging",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = ["chromeos", "enable_reclient", "reclient_staging"],
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = ["mb"],
            build_config = builder_config.build_config.RELEASE,
            target_arch = builder_config.target_arch.INTEL,
            target_bits = 64,
            target_platform = builder_config.target_platform.CHROMEOS,
            cros_boards_with_qemu_images = "amd64-generic-vm",
        ),
    ),
    os = os.LINUX_DEFAULT,
    console_view_category = "linux",
)

fyi_reclient_test_builder(
    name = "Simple Chrome Builder reclient test",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = ["chromeos", "enable_reclient", "reclient_test"],
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = ["mb"],
            build_config = builder_config.build_config.RELEASE,
            target_arch = builder_config.target_arch.INTEL,
            target_bits = 64,
            target_platform = builder_config.target_platform.CHROMEOS,
            cros_boards_with_qemu_images = "amd64-generic-vm",
        ),
    ),
    os = os.LINUX_DEFAULT,
    console_view_category = "linux",
)
