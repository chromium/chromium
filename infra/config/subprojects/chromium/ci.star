# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

load("//lib/branches.star", "branches")
load("//lib/builders.star", "builders", "cpu")
load("//lib/ci.star", "ci")
load("//lib/consoles.star", "consoles")
load("//project.star", "settings")

# Bucket-wide defaults
ci.defaults.set(
    bucket = "ci",
    build_numbers = True,
    cpu = cpu.X86_64,
    triggered_by = ["chromium-gitiles-trigger"],
    free_space = builders.free_space.standard,
)

luci.bucket(
    name = "ci",
    acls = [
        acl.entry(
            roles = acl.BUILDBUCKET_READER,
            groups = "all",
        ),
        acl.entry(
            roles = acl.BUILDBUCKET_TRIGGERER,
            users = [
                # Allow chrome-release/branch builders on luci.chrome.official.infra
                # to schedule builds
                "chrome-official-brancher@chops-service-accounts.iam.gserviceaccount.com",
            ],
            groups = "project-chromium-ci-schedulers",
        ),
        acl.entry(
            roles = acl.BUILDBUCKET_OWNER,
            groups = "project-chromium-admins",
        ),
        acl.entry(
            roles = acl.SCHEDULER_TRIGGERER,
            groups = "project-chromium-scheduler-triggerers",
        ),
    ],
)

luci.gitiles_poller(
    name = "chromium-gitiles-trigger",
    bucket = "ci",
    repo = "https://chromium.googlesource.com/chromium/src",
    refs = [settings.ref],
)

[consoles.overview_console_view(
    name = name,
    repo = "https://chromium.googlesource.com/chromium/src",
    refs = [settings.ref],
    title = title,
    top_level_ordering = [
        "chromium",
        "chromium.win",
        "chromium.mac",
        "chromium.linux",
        "chromium.chromiumos",
        "chromium.android",
        "chromium.fuchsia",
        "chromium.angle",
        "chrome",
        "chromium.memory",
        "chromium.dawn",
        "chromium.gpu",
        "chromium.fyi",
        "chromium.android.fyi",
        "chromium.clang",
        "chromium.fuzz",
        "chromium.gpu.fyi",
        "chromium.swangle",
        "chromium.updater",
    ],
) for name, title in (
    ("main", "{} Main Console".format(settings.project_title)),
    ("mirrors", "{} CQ Mirrors Console".format(settings.project_title)),
)]

# The main console includes some entries for builders from the chrome project
[branches.console_view_entry(
    builder = "chrome:ci/{}".format(name),
    console_view = "main",
    category = "chrome",
    short_name = short_name,
) for name, short_name in (
    ("lacros-amd64-generic-chrome", "lcr"),
    ("lacros-arm-generic-chrome", "lcr"),
    ("linux-chromeos-chrome", "cro"),
    ("linux-chrome", "lnx"),
    ("mac-chrome", "mac"),
    ("win-chrome", "win"),
    ("win64-chrome", "win"),
)]

consoles.console_view(
    name = "sheriff.fuchsia",
    title = "Fuchsia Sheriff Console",
    ordering = {
        "*type*": consoles.ordering(short_names = ["a64", "x64"]),
        None: ["ci", "fuchsia ci", "hardware", "fyi"],
        "chromium.fyi|13": "*type*",
    },
)

# The sheriff.fuchsia console includes some entries for builders from the chrome project
[branches.console_view_entry(
    builder = "chrome:ci/{}".format(name),
    console_view = "sheriff.fuchsia",
    category = category,
    short_name = short_name,
) for name, category, short_name in (
    ("fuchsia-fyi-arm64-size", "fuchsia ci", "a64-size"),
    ("fuchsia-fyi-astro", "hardware", "ast"),
    ("fuchsia-fyi-atlas", "hardware", "atl"),
    ("fuchsia-fyi-sherlock", "hardware", "slk"),
    ("fuchsia-builder-perf-fyi", "fuchsia ci", "builder-perf"),
    ("fuchsia-builder-perf-x64", "fuchsia ci", "builder-perf-x64"),
    ("fuchsia-perf-fyi", "hardware", "ast-perf"),
    ("fuchsia-perf-atlas-fyi", "hardware", "atl-perf"),
    ("fuchsia-perf-sherlock-fyi", "hardware", "slk-perf"),
    ("fuchsia-x64", "fuchsia ci", "x64-chrome"),
)]

exec("./ci/chromium.star")
exec("./ci/chromium.accessibility.star")
exec("./ci/chromium.android.star")
exec("./ci/chromium.android.fyi.star")
exec("./ci/chromium.angle.star")
exec("./ci/chromium.chromiumos.star")
exec("./ci/chromium.clang.star")
exec("./ci/chromium.dawn.star")
exec("./ci/chromium.fuchsia.star")
exec("./ci/chromium.fuchsia.fyi.star")
exec("./ci/chromium.fuzz.star")
exec("./ci/chromium.fyi.star")
exec("./ci/chromium.gpu.star")
exec("./ci/chromium.gpu.experimental.star")
exec("./ci/chromium.gpu.fyi.star")
exec("./ci/chromium.linux.star")
exec("./ci/chromium.mac.star")
exec("./ci/chromium.memory.star")
exec("./ci/chromium.packager.star")
exec("./ci/chromium.rust.star")
exec("./ci/chromium.swangle.star")
exec("./ci/chromium.updater.star")
exec("./ci/chromium.win.star")
exec("./ci/metadata.exporter.star")
