# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

load("//lib/builders.star", "builder", "cpu", "defaults", "free_space", "os")
load("//lib/ci.star", "ci")
load("//lib/consoles.star", "consoles")
load("//lib/dimensions.star", "dimensions")
load("//lib/polymorphic.star", "polymorphic")

luci.bucket(
    name = "reviver",
    acls = [
        acl.entry(
            roles = acl.BUILDBUCKET_READER,
            groups = "all",
        ),
        acl.entry(
            roles = acl.BUILDBUCKET_TRIGGERER,
            # TODO(crbug.com/40232487) Switch this to something more sensible once
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
    pool = ci.DEFAULT_POOL,
    cores = 8,
    os = os.LINUX_DEFAULT,
    list_view = "reviver",
    service_account = "reviver-builder@chops-service-accounts.iam.gserviceaccount.com",
)

polymorphic.launcher(
    name = "android-launcher",
    # To avoid peak hours, we run it at 2 AM, 5 AM, 8 AM, 11AM, 2 PM UTC.
    schedule = "0 2,5,8,11,14 * * *",
    pool = ci.DEFAULT_POOL,
    cores = 8,
    os = os.LINUX_DEFAULT,
    runner = "reviver/runner",
    target_builders = [
        "ci/android-oreo-x86-rel",
        "ci/android-pie-x86-rel",
        "ci/android-12-x64-rel",
        "ci/android-13-x64-rel",
        "ci/android-15-x64-rel",
    ],
)

polymorphic.launcher(
    name = "android-coverage-launcher",
    # Match the replicated builders' schedule for comparable data
    schedule = "0 4 * * *",
    pool = ci.DEFAULT_POOL,
    os = os.LINUX_DEFAULT,
    runner = "reviver/coverage-runner",
    target_builders = [
        "ci/android-code-coverage",
        "ci/android-code-coverage-native",
    ],
)

polymorphic.launcher(
    name = "android-device-launcher",
    # To avoid peak hours, we run it at 5 AM, 8 AM, 11AM UTC.
    schedule = "0 5,8,11 * * *",
    pool = ci.DEFAULT_POOL,
    os = os.LINUX_DEFAULT,
    runner = "reviver/runner",
    target_builders = [
        "ci/android-pie-arm64-rel",
    ],
)

polymorphic.launcher(
    name = "android-x64-launcher",
    # To avoid peak hours, we run it at 2 AM, 5 AM, 8 AM, 11AM, 2 PM UTC.
    schedule = "0 2,5,8,11,14 * * *",
    pool = ci.DEFAULT_POOL,
    cores = 8,
    os = os.LINUX_DEFAULT,
    runner = "reviver/runner",
    target_builders = [
        polymorphic.target_builder(
            builder = "ci/Android x64 Builder (dbg)",
            dimensions = dimensions.dimensions(
                builderless = "",
                cores = "",
                os = "Ubuntu-22.04",
                ssd = "",
                free_space = "",
                builder = "Android x64 Builder (dbg)",
            ),
            testers = [
                "ci/android-12l-x64-dbg-tests",
            ],
        ),
    ],
)

polymorphic.launcher(
    name = "linux-launcher",
    # To avoid peak hours, we run it at 5~11 UTC, 21~27 PST.
    schedule = "0 5-11/3 * * *",
    runner = "reviver/runner",
    target_builders = [
        polymorphic.target_builder(
            builder = "ci/Linux Builder",
            testers = [
                "ci/Linux Tests",
            ],
        ),
    ],
)

polymorphic.launcher(
    name = "win-launcher",
    # To avoid peak hours, we run it at 5~11 UTC, 21~27 PST.
    schedule = "0 5-11/3 * * *",
    runner = "reviver/runner",
    target_builders = [
        polymorphic.target_builder(
            builder = "ci/Win x64 Builder",
            dimensions = dimensions.dimensions(
                builderless = True,
                os = os.WINDOWS_DEFAULT,
                cpu = cpu.X86_64,
                free_space = free_space.standard,
            ),
            testers = [
                "ci/Win10 Tests x64",
            ],
        ),
    ],
)

polymorphic.launcher(
    name = "mac-launcher",
    # To avoid peak hours, we run it at 5~11 UTC, 21~27 PST.
    schedule = "0 5-11/3 * * *",
    runner = "reviver/runner",
    target_builders = [
        polymorphic.target_builder(
            builder = "ci/Mac Builder",
            dimensions = dimensions.dimensions(
                builderless = True,
                os = os.MAC_DEFAULT,
                cpu = cpu.X86_64,
                ssd = True,
                free_space = free_space.standard,
            ),
            testers = [
                "ci/Mac12 Tests",
            ],
        ),
    ],
)

# A coordinator of slightly aggressive scheduling with effectively unlimited
# test bot capacity for fuchsia.
polymorphic.launcher(
    name = "fuchsia-coordinator",
    # Avoid peak hours.
    schedule = "0 2,4,6,8,10,12,14 * * *",
    pool = ci.DEFAULT_POOL,
    os = os.LINUX_DEFAULT,
    runner = "reviver/runner",
    target_builders = [
        "ci/fuchsia-arm64-cast-receiver-rel",
        "ci/fuchsia-x64-cast-receiver-dbg",
        "ci/fuchsia-x64-cast-receiver-rel",
    ],
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
    # TODO(crbug.com/40232487) Remove this once the reviver service account has
    # necessary permissions
    service_account = ci.DEFAULT_SERVICE_ACCOUNT,
)

builder(
    name = "coverage-runner",
    executable = "recipe:reviver/chromium/runner",
    pool = ci.DEFAULT_POOL,
    builderless = 1,
    cores = 32,
    os = os.LINUX_DEFAULT,
    cpu = cpu.X86_64,
    ssd = True,
    free_space = free_space.standard,
    auto_builder_dimension = False,
    execution_timeout = 6 * time.hour,
    resultdb_bigquery_exports = [
        resultdb.export_test_results(
            bq_table = "chrome-luci-data.chromium.reviver_test_results",
        ),
    ],
    # TODO(crbug.com/40232487) Remove this once the reviver service account has
    # necessary permissions
    service_account = ci.DEFAULT_SERVICE_ACCOUNT,
)
