# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

load("//lib/builders.star", "builders", "cpu", "goma")
load("//lib/consoles.star", "consoles")

builders.defaults.set(
    bucket = "infra",
    build_numbers = True,
    cores = 8,
    cpu = cpu.X86_64,
)

consoles.console_view(
    name = "infra",
)

luci.bucket(
    name = "infra",
    acls = [
        acl.entry(
            roles = acl.BUILDBUCKET_READER,
            groups = "all",
        ),
        acl.entry(
            roles = acl.BUILDBUCKET_TRIGGERER,
            groups = "project-chromium-infra-schedulers",
        ),
    ],
)

builders.builder(
    name = "linux-rel-warmed-compilator-warmer",
    console_view_entry = consoles.console_view_entry(
        console_view = "infra",
        category = "warmer",
    ),
    override_builder_dimension = "linux-rel-warmed-compilator",
    caches = [
        swarming.cache(
            name = "linux_rel_warmed_compilator_warmed_cache",
            path = "builder",
            wait_for_warm_cache = 4 * time.minute,
        ),
    ],
    cores = None,
    executable = "recipe:chromium/builder_cache_prewarmer",
    goma_jobs = goma.jobs.J150,
    pool = "luci.chromium.try",
    properties = {
        "builder_to_warm": {
            "builder_name": "linux-rel-warmed",
            "builder_group": "tryserver.chromium.linux",
        },
    },
    service_account = "chromium-warmer@chops-service-accounts.iam.gserviceaccount.com",
)
