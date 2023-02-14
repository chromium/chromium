# Copyright 2020 The Chromium Authors
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
    triggered_by = ["chromium-gitiles-trigger"],
    cpu = cpu.X86_64,
    free_space = builders.free_space.standard,
    build_numbers = True,
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
            groups = [
                "project-chromium-ci-schedulers",
                # Allow currently-oncall sheriffs to cancel builds. Useful when
                # a tree-closer is behind and hasn't picked up a needed revert
                # or fix yet.
                "mdb/chrome-active-sheriffs",
            ],
            users = [
                # Allow chrome-release/branch builders on luci.chrome.official.infra
                # to schedule builds
                "chrome-official-brancher@chops-service-accounts.iam.gserviceaccount.com",
            ],
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
        "chromium.cft",
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
    console_view = "main",
    builder = "chrome:ci/{}".format(name),
    category = "chrome",
    short_name = short_name,
) for name, short_name in (
    ("lacros-amd64-generic-chrome", "lcr"),
    ("lacros-arm-generic-chrome", "lcr"),
    ("lacros-arm64-generic-chrome", "lcr"),
    ("linux-chromeos-chrome", "cro"),
    ("linux-chrome", "lnx"),
    ("mac-chrome", "mac"),
    ("win-chrome", "win"),
    ("win64-chrome", "win"),
)]

# Any builders that should be monitored by the Chrome-Fuchsia Gardener
# should be in the "gardener" group.
consoles.console_view(
    name = "sheriff.fuchsia",
    title = "Fuchsia Sheriff Console",
    ordering = {
        None: ["gardener", "fyi"],
        "gardener": ["ci", "fuchsia ci", "p/chrome", "hardware"],
        "fyi": ["arm64", "x64", "clang", "hardware"],
    },
)

# The sheriff.fuchsia console includes some entries for builders from the chrome project
[branches.console_view_entry(
    console_view = "sheriff.fuchsia",
    builder = "chrome:ci/{}".format(name),
    category = category,
    short_name = short_name,
) for name, category, short_name in (
    ("fuchsia-builder-perf-arm64", "gardener|p/chrome|arm64", "perf-bld"),
    ("fuchsia-builder-perf-x64", "gardener|p/chrome|x64", "perf-bld"),
    ("fuchsia-fyi-arm64-size", "gardener|p/chrome|arm64", "size"),
    ("fuchsia-fyi-astro", "gardener|hardware", "ast"),
    ("fuchsia-fyi-atlas", "fyi|hardware", "atl"),
    ("fuchsia-fyi-sherlock", "fyi|hardware", "sher"),
    ("fuchsia-perf-ast", "gardener|hardware|perf", "ast"),
    ("fuchsia-perf-atlas-fyi", "fyi|hardware|perf", "atl"),
    ("fuchsia-perf-fyi", "fyi|hardware|perf", "ast"),
    ("fuchsia-perf-sherlock-fyi", "fyi|hardware|perf", "sher"),
    ("fuchsia-perf-shk", "gardener|hardware|perf", "sher"),
    ("fuchsia-x64", "gardener|p/chrome|x64", "rel"),
)]

exec("./ci/blink.infra.star")
exec("./ci/checks.star")
exec("./ci/chromium.star")
exec("./ci/chromium.accessibility.star")
exec("./ci/chromium.android.star")
exec("./ci/chromium.android.fyi.star")
exec("./ci/chromium.angle.star")
exec("./ci/chromium.cft.star")
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
exec("./ci/chromium.memory.fyi.star")
exec("./ci/chromium.packager.star")
exec("./ci/chromium.rust.star")
exec("./ci/chromium.swangle.star")
exec("./ci/chromium.updater.star")
exec("./ci/chromium.win.star")
exec("./ci/metadata.exporter.star")
