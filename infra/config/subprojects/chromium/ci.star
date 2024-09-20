# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

load("//lib/branches.star", "branches")
load("//lib/builders.star", "builders", "cpu", "siso")
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
    shadow_builderless = True,
    shadow_free_space = None,
    shadow_pool = "luci.chromium.try",
    shadow_siso_project = siso.project.DEFAULT_UNTRUSTED,
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
                # Allow currently-oncall gardeners to cancel builds. Useful when
                # a tree-closer is behind and hasn't picked up a needed revert
                # or fix yet.
                "mdb/chrome-active-sheriffs",
                "mdb/chrome-gpu",
                "mdb/bling-engprod",
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
        acl.entry(
            roles = acl.SCHEDULER_OWNER,
            groups = [
                # Allow currently-oncall gardeners to pause schedulers.
                "mdb/chrome-active-sheriffs",
                "mdb/chrome-gpu",
                "mdb/bling-engprod",
            ],
        ),
    ],
)

# Shadow bucket of `ci`, for led builds.
luci.bucket(
    name = "ci.shadow",
    shadows = "ci",
    bindings = [
        luci.binding(
            roles = "role/buildbucket.creator",
            groups = [
                "mdb/chrome-build-access-sphinx",
                "mdb/chrome-troopers",
                "chromium-led-users",
            ],
            users = [
                ci.DEFAULT_SHADOW_SERVICE_ACCOUNT,
                ci.gpu.SHADOW_SERVICE_ACCOUNT,
            ],
        ),
        luci.binding(
            roles = "role/buildbucket.triggerer",
            users = [
                ci.DEFAULT_SHADOW_SERVICE_ACCOUNT,
                ci.gpu.SHADOW_SERVICE_ACCOUNT,
            ],
        ),
        # TODO(crbug.com/40941662): Remove this binding after shadow bucket
        # could inherit the view permission from the actual bucket.
        luci.binding(
            roles = "role/buildbucket.reader",
            groups = [
                "all",
            ],
        ),
        # Allow ci builders to create invocations in their own builds.
        luci.binding(
            roles = "role/resultdb.invocationCreator",
            users = [
                ci.DEFAULT_SHADOW_SERVICE_ACCOUNT,
                ci.gpu.SHADOW_SERVICE_ACCOUNT,
            ],
        ),
    ],
    dynamic = True,
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
        "chromium.enterprise_companion",
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
    ("fuchsia-arm64-rel-ready", "gardener|p/chrome|arm64", "rel-ready"),
    ("fuchsia-arm64-nest-sd", "gardener|p/chrome|arm64", "nest-arm"),
    ("fuchsia-ava-nelson", "gardener|hardware|ava", "nsn"),
    ("fuchsia-builder-perf-arm64", "gardener|p/chrome|arm64", "perf-arm"),
    ("fuchsia-cast-astro", "gardener|hardware|cast", "ast"),
    ("fuchsia-cast-nelson", "gardener|hardware|cast", "nsn"),
    ("fuchsia-cast-sherlock", "gardener|hardware|cast", "sher"),
    ("fuchsia-fyi-arm64-size", "gardener|p/chrome|arm64", "size"),
    ("fuchsia-fyi-astro", "gardener|hardware", "ast"),
    ("fuchsia-fyi-nelson", "gardener|hardware", "nsn"),
    ("fuchsia-fyi-sherlock", "gardener|hardware", "sher"),
    ("fuchsia-fyi-sherlock-qemu", "gardener|hardware|emu", "sher"),
    ("fuchsia-smoke-astro", "gardener|hardware|smoke", "ast"),
    ("fuchsia-smoke-nelson", "gardener|hardware|smoke", "nsn"),
    ("fuchsia-smoke-sherlock", "gardener|hardware|smoke", "sher"),
    ("fuchsia-smoke-sherlock-roller", "gardener|hardware|smoke", "roll"),
    ("fuchsia-perf-nsn", "gardener|hardware|perf", "nsn"),
    ("fuchsia-perf-shk", "gardener|hardware|perf", "sher"),
    ("fuchsia-x64", "gardener|p/chrome|x64", "rel"),
    ("fuchsia-x64-nest-sd", "gardener|p/chrome|x64", "nest-x64"),
)]

exec("./ci/blink.infra.star")
exec("./ci/checks.star")
exec("./ci/chromium.star")
exec("./ci/chromium.accessibility.star")
exec("./ci/chromium.android.star")
exec("./ci/chromium.android.desktop.star")
exec("./ci/chromium.android.fyi.star")
exec("./ci/chromium.angle.star")
exec("./ci/chromium.cft.star")
exec("./ci/chromium.chromiumos.star")
exec("./ci/chromium.clang.star")
exec("./ci/chromium.coverage.star")
exec("./ci/chromium.dawn.star")
exec("./ci/chromium.enterprise_companion.star")
exec("./ci/chromium.fuchsia.star")
exec("./ci/chromium.fuchsia.fyi.star")
exec("./ci/chromium.fuzz.star")
exec("./ci/chromium.fyi.star")
exec("./ci/chromium.gpu.star")
exec("./ci/chromium.gpu.experimental.star")
exec("./ci/chromium.gpu.fyi.star")
exec("./ci/chromium.infra.star")
exec("./ci/chromium.linux.star")
exec("./ci/chromium.mac.star")
exec("./ci/chromium.memory.star")
exec("./ci/chromium.memory.fyi.star")
exec("./ci/chromium.rust.star")
exec("./ci/chromium.swangle.star")
exec("./ci/chromium.updater.star")
exec("./ci/chromium.win.star")
exec("./ci/metadata.exporter.star")
