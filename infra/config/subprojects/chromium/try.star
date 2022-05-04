# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

load("//lib/branches.star", "branches")
load("//lib/builders.star", "cpu")
load("//lib/consoles.star", "consoles")
load("//lib/try.star", "try_")
load("//project.star", "ACTIVE_MILESTONES", "settings")
load("./fallback-cq.star", "fallback_cq")

try_.defaults.set(
    bucket = "try",
    build_numbers = True,
    caches = [
        swarming.cache(
            name = "win_toolchain",
            path = "win_toolchain",
        ),
    ],
    cpu = cpu.X86_64,
    cq_group = "cq",
    # Max. pending time for builds. CQ considers builds pending >2h as timed
    # out: http://shortn/_8PaHsdYmlq. Keep this in sync.
    expiration_timeout = 2 * time.hour,
    grace_period = 2 * time.minute,
    subproject_list_view = "luci.chromium.try",
    task_template_canary_percentage = 5,
)

luci.bucket(
    name = "try",
    acls = [
        acl.entry(
            roles = acl.BUILDBUCKET_READER,
            groups = "all",
        ),
        acl.entry(
            roles = acl.BUILDBUCKET_TRIGGERER,
            users = [
                "findit-for-me@appspot.gserviceaccount.com",
                "tricium-prod@appspot.gserviceaccount.com",
            ],
            groups = [
                "project-chromium-tryjob-access",
                # Allow Pinpoint to trigger builds for bisection
                "service-account-chromeperf",
                "service-account-cq",
            ],
            projects = [
                "angle",
                "dawn",
                "skia",
                "swiftshader",
                "v8",
            ] if settings.is_main else None,
        ),
        acl.entry(
            roles = acl.BUILDBUCKET_OWNER,
            groups = "service-account-chromium-tryserver",
        ),
    ],
)

luci.cq_group(
    name = "cq",
    retry_config = cq.RETRY_ALL_FAILURES,
    tree_status_host = "chromium-status.appspot.com" if settings.is_main else None,
    watch = cq.refset(
        repo = "https://chromium.googlesource.com/chromium/src",
        # The chromium project's CQ covers all of the refs under refs/heads,
        # which includes refs/heads/main, for projects running out of a branch
        # the CQ only runs for that ref
        refs = ["refs/heads/.+" if settings.is_main else settings.ref],
    ),
    acls = [
        acl.entry(
            acl.CQ_COMMITTER,
            groups = "project-chromium-committers",
        ),
        acl.entry(
            acl.CQ_DRY_RUNNER,
            groups = "project-chromium-tryjob-access",
        ),
    ],
    additional_modes = [
        cq.run_mode(cq.MODE_QUICK_DRY_RUN, 1, "Quick-Run", 1),
    ],
)

# Declare a CQ group that watches all branch heads, excluding the active
# branches. SUBMIT TO CQ fails if there is no CQ group watching a branch, so
# this allows SUBMIT TO CQ to wok regardless of the branch. The CQ group will
# only have specific builders added to ensure that changes to non-active
# branches makes sense (e.g. fail CLs that require testing if there isn't a
# proper CQ group set up for the ref).
branches.cq_group(
    name = fallback_cq.GROUP,
    retry_config = cq.RETRY_ALL_FAILURES,
    watch = cq.refset(
        repo = "https://chromium.googlesource.com/chromium/src",
        refs = ["refs/branch-heads/.*"],
        refs_exclude = [
            details.ref
            for details in ACTIVE_MILESTONES.values()
        ],
    ),
    acls = [
        acl.entry(
            acl.CQ_COMMITTER,
            groups = "project-chromium-committers",
        ),
        acl.entry(
            acl.CQ_DRY_RUNNER,
            groups = "project-chromium-tryjob-access",
        ),
    ],
)

consoles.list_view(
    name = "try",
    branch_selector = branches.ALL_BRANCHES,
    title = "{} CQ Console".format(settings.project_title),
)

consoles.list_view(
    name = "luci.chromium.try",
    branch_selector = branches.ALL_BRANCHES,
)

exec("./try/presubmit.star")
exec("./try/tryserver.blink.star")
exec("./try/tryserver.chromium.star")
exec("./try/tryserver.chromium.android.star")
exec("./try/tryserver.chromium.angle.star")
exec("./try/tryserver.chromium.chromiumos.star")
exec("./try/tryserver.chromium.dawn.star")
exec("./try/tryserver.chromium.fuchsia.star")
exec("./try/tryserver.chromium.linux.star")
exec("./try/tryserver.chromium.mac.star")
exec("./try/tryserver.chromium.packager.star")
exec("./try/tryserver.chromium.rust.star")
exec("./try/tryserver.chromium.updater.star")
exec("./try/tryserver.chromium.win.star")

# Used for listing chrome trybots in chromium's commit-queue.cfg without also
# adding them to chromium's cr-buildbucket.cfg. Note that the recipe these
# builders run allow only known roller accounts when triggered via the CQ.
def chrome_internal_verifier(
        *,
        builder,
        **kwargs):
    branches.cq_tryjob_verifier(
        builder = "{}:try/{}".format(settings.chrome_project, builder),
        cq_group = "cq",
        includable_only = True,
        owner_whitelist = [
            "googlers",
            "project-chromium-robot-committers",
        ],
        **kwargs
    )

chrome_internal_verifier(
    builder = "android-internal-binary-size",
)

chrome_internal_verifier(
    builder = "android-internal-rel",
)

chrome_internal_verifier(
    builder = "chromeos-betty-chrome",
)

chrome_internal_verifier(
    builder = "chromeos-betty-pi-arc-chrome",
)

chrome_internal_verifier(
    builder = "chromeos-eve-chrome",
)

chrome_internal_verifier(
    builder = "chromeos-eve-compile-chrome",
)

# TODO(crbug.com/1295085): Migrate to gitfooter based trigger
# During Nearby Connection library autoroller uprev, we want
# chromeos-jacuzzi-nearby-chrome-fyi to run as an experimental builder
# and not block the auto-submission of the CL.
# Currently there is no support for gitfooter based trigger like
# "Cq-Include-Trybots" for experimental builders, we are using the following
# workaround until the support is available.
# Autoroller generated CL keeps an additional githash bookkeeping in
# third_party/nearby/README.chromium. This file serves as a unique marker for
# Nearby uprev and is used to trigger the Nearby builder.
branches.cq_tryjob_verifier(
    builder = "{}:try/{}".format(settings.chrome_project, "chromeos-jacuzzi-nearby-chrome-fyi"),
    cq_group = "cq",
    experiment_percentage = 100,
    includable_only = False,
    location_regexp = [".+/[+]/third_party/nearby/README.chromium"],
    owner_whitelist = [
        "googlers",
        "project-chromium-robot-committers",
    ],
)

chrome_internal_verifier(
    builder = "chromeos-kevin-chrome",
)

chrome_internal_verifier(
    builder = "chromeos-kevin-compile-chrome",
)

chrome_internal_verifier(
    builder = "chromeos-octopus-chrome",
)

chrome_internal_verifier(
    builder = "chromeos-octopus-compile-chrome",
)

chrome_internal_verifier(
    builder = "chromeos-reven-chrome",
)

chrome_internal_verifier(
    builder = "fuchsia-fyi-astro",
)

chrome_internal_verifier(
    builder = "ipad-device",
)

chrome_internal_verifier(
    builder = "iphone-device",
)

chrome_internal_verifier(
    builder = "lacros-amd64-generic-chrome",
)

chrome_internal_verifier(
    builder = "lacros-amd64-generic-chrome-skylab",
    branch_selector = branches.STANDARD_MILESTONE,
)

chrome_internal_verifier(
    builder = "lacros-arm-generic-chrome",
)

chrome_internal_verifier(
    builder = "lacros-arm-generic-chrome-skylab",
)

chrome_internal_verifier(
    builder = "linux-chrome",
    branch_selector = branches.STANDARD_MILESTONE,
)

chrome_internal_verifier(
    builder = "linux-chrome-stable",
    branch_selector = branches.STANDARD_MILESTONE,
)

chrome_internal_verifier(
    builder = "linux-chromeos-chrome",
)

chrome_internal_verifier(
    builder = "linux-nearby-chrome-fyi",
)

chrome_internal_verifier(
    builder = "linux-pgo",
    branch_selector = branches.STANDARD_MILESTONE,
)

chrome_internal_verifier(
    builder = "mac-chrome",
    branch_selector = branches.DESKTOP_EXTENDED_STABLE_MILESTONE,
)

chrome_internal_verifier(
    builder = "mac-chrome-stable",
    branch_selector = branches.DESKTOP_EXTENDED_STABLE_MILESTONE,
)

chrome_internal_verifier(
    builder = "mac-arm-pgo",
    branch_selector = branches.STANDARD_MILESTONE,
)

chrome_internal_verifier(
    builder = "mac-pgo",
    branch_selector = branches.STANDARD_MILESTONE,
)

chrome_internal_verifier(
    builder = "test-o-emulator",
)

chrome_internal_verifier(
    builder = "win-chrome",
    branch_selector = branches.DESKTOP_EXTENDED_STABLE_MILESTONE,
)

chrome_internal_verifier(
    builder = "win-chrome-stable",
    branch_selector = branches.DESKTOP_EXTENDED_STABLE_MILESTONE,
)

chrome_internal_verifier(
    builder = "win32-pgo",
    branch_selector = branches.STANDARD_MILESTONE,
)

chrome_internal_verifier(
    builder = "win64-chrome",
    branch_selector = branches.DESKTOP_EXTENDED_STABLE_MILESTONE,
)

chrome_internal_verifier(
    builder = "win64-chrome-stable",
    branch_selector = branches.DESKTOP_EXTENDED_STABLE_MILESTONE,
)

chrome_internal_verifier(
    builder = "win64-pgo",
    branch_selector = branches.STANDARD_MILESTONE,
)
