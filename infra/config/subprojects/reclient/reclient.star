# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

load("//lib/builders.star", "cpu", "os")
load("//lib/ci.star", "ci")
load("//lib/consoles.star", "consoles")
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
    os = os.LINUX_DEFAULT,
    pool = "luci.chromium.ci",
    service_account = (
        "chromium-ci-builder@chops-service-accounts.iam.gserviceaccount.com"
    ),
    triggered_by = ["chromium-gitiles-trigger"],
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
        reclient_instance = "rbe-chromium-trusted",
        **kwargs):
    return ci.builder(
        name = name,
        reclient_instance = reclient_instance,
        console_view_entry = consoles.console_view_entry(
            category = "rbe|linux",
            short_name = "rcs",
        ),
        **kwargs
    )

def fyi_reclient_test_builder(
        *,
        name,
        **kwargs):
    return fyi_reclient_staging_builder(
        name = name,
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
)
