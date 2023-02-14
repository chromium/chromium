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

    # TODO(crbug.com/1362440): remove this.
    omit_python2 = False,
    service_account = "reviver-builder@chops-service-accounts.iam.gserviceaccount.com",
)

def target_builder(*, name):
    return {
        "builder_id": {
            "project": "chromium",
            "bucket": "ci",
            "builder": name,
        },
    }

builder(
    name = "android-launcher",
    executable = "recipe:chromium_polymorphic/launcher",
    # To avoid peak hours, we run it at 1 AM, 4 AM, 7 AM, 10AM, 1 PM UTC.
    schedule = "0 1,4,7,10,13 * * *",
    pool = "luci.chromium.ci",
    os = os.LINUX_DEFAULT,
    properties = {
        "runner_builder": {
            "project": "chromium",
            "bucket": "reviver",
            "builder": "runner",
        },
        "target_builders": [
            target_builder(
                name = "android-nougat-x86-rel",
            ),
            target_builder(
                name = "android-12-x64-rel",
            ),
        ],
    },
)

# A coordinator of slightly aggressive scheduling with effectively unlimited
# test bot capacity for fuchsia.
builder(
    name = "fuchsia-coordinator",
    executable = "recipe:chromium_polymorphic/launcher",
    # Avoid peak hours.
    schedule = "0 1,3,5,7,9,11,13 * * *",
    pool = "luci.chromium.ci",
    os = os.LINUX_DEFAULT,
    properties = {
        "runner_builder": {
            "project": "chromium",
            "bucket": "reviver",
            "builder": "runner",
        },
        "target_builders": [
            target_builder(
                name = "fuchsia-fyi-x64-rel",
            ),
            target_builder(
                name = "fuchsia-fyi-arm64-dbg",
            ),
            target_builder(
                name = "fuchsia-fyi-x64-asan",
            ),
        ],
    },
)

builder(
    name = "runner",
    executable = "recipe:reviver/chromium/runner",
    pool = ci.DEFAULT_POOL,
    builderless = 1,
    os = os.LINUX_DEFAULT,
    cpu = cpu.X86_64,
    ssd = False,
    free_space = free_space.standard,
    auto_builder_dimension = False,
    execution_timeout = 6 * time.hour,
    resultdb_bigquery_exports = [
        resultdb.export_test_results(
            bq_table = "chrome-luci-data.chromium.reviver_test_results",
        ),
    ],
    # TODO(crbug/1346396) Remove this once the reviver service account has
    # necessary permissions
    service_account = ci.DEFAULT_SERVICE_ACCOUNT,
)
