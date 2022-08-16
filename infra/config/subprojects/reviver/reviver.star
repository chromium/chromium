# Copyright 2022 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

load("//lib/builders.star", "builder", "cpu", "defaults", "free_space", "os")
load("//lib/ci.star", "ci")
load("//lib/consoles.star", "consoles")

luci.bucket(
    name = "reviver",
    acls = [
        acl.entry(
            roles = acl.BUILDBUCKET_READER,
            groups = "all",
        ),
        acl.entry(
            roles = acl.BUILDBUCKET_TRIGGERER,
            # TODO(crbug/1346396) Switch this to something more sensible once
            # the builders are verified
            users = [
                "gbeaty@google.com",
                "reviver-builder@chops-service-accounts.iam.gserviceaccount.com",
            ],
        ),
        acl.entry(
            roles = acl.BUILDBUCKET_OWNER,
            groups = "project-chromium-admins",
        ),
    ],
)

consoles.list_view(
    name = "reviver",
)

defaults.set(
    bucket = "reviver",
    list_view = "reviver",
    service_account = "reviver-builder@chops-service-accounts.iam.gserviceaccount.com",
)

def target_builder(*, name, dimensions):
    return {
        "builder_id": {
            "project": "chromium",
            "bucket": "ci",
            "builder": name,
        },
        "dimensions": {k: str(v) for k, v in dimensions.items()},
    }

builder(
    name = "coordinator",
    executable = "recipe:chromium_polymorphic/launcher",
    # TODO(crbug/1346396) Figure out what machines the coordinator should run on
    os = os.LINUX_DEFAULT,
    pool = "luci.chromium.ci",
    properties = {
        "runner_builder": {
            "project": "chromium",
            "bucket": "reviver",
            "builder": "runner",
        },
        # TODO(crbug/1346396) Figure out what machines the runnner should run on
        "target_builders": [
            target_builder(
                name = "android-marshmallow-x86-rel",
                dimensions = {
                    "builderless": 1,
                    "cpu": cpu.X86_64,
                    "free_space": free_space.standard,
                    "os": os.LINUX_DEFAULT.dimension,
                    "ssd": "0",
                },
            ),
        ],
    },
    # TODO(crbug/1346396) Switch this to an appropriate schedule once the
    # builders are verified
    schedule = "triggered",
)

builder(
    name = "runner",
    executable = "recipe:reviver/chromium/runner",
    # TODO(crbug/1346396) Figure out what machines the runnner should run on
    pool = ci.DEFAULT_POOL,
)
