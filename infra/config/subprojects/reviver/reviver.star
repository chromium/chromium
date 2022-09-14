# Copyright 2022 The Chromium Authors
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
    name = "android-launcher",
    executable = "recipe:chromium_polymorphic/launcher",
    os = os.LINUX_DEFAULT,
    pool = "luci.chromium.ci",
    properties = {
        "runner_builder": {
            "project": "chromium",
            "bucket": "reviver",
            "builder": "runner",
        },
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
    # To avoid peak hours, we run it at 1 AM, 4 AM, 7 AM, 10AM, 1 PM UTC.
    schedule = "0 1,4,7,10,13 * * *",
)

builder(
    name = "runner",
    executable = "recipe:reviver/chromium/runner",
    auto_builder_dimension = False,
    execution_timeout = 6 * time.hour,
    pool = ci.DEFAULT_POOL,
    # TODO(crbug/1346396) Remove this once the reviver service account has
    # necessary permissions
    service_account = ci.DEFAULT_SERVICE_ACCOUNT,
    resultdb_bigquery_exports = [
        resultdb.export_test_results(
            bq_table = "chrome-luci-data.chromium.reviver_test_results",
        ),
    ],
)
