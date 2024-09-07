# Copyright 2020 The Chromium Authors
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
    cpu = cpu.X86_64,
    build_numbers = True,
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
            groups = [
                "project-chromium-tryjob-access",
                # Allow Pinpoint to trigger builds for bisection
                "service-account-chromeperf",
                "service-account-cq",
            ],
            users = [
                "dawn-automated-expectations@chops-service-accounts.iam.gserviceaccount.com",
                "findit-for-me@appspot.gserviceaccount.com",
                "tricium-prod@appspot.gserviceaccount.com",
            ],
            projects = [p for p in [
                branches.value(branch_selector = branches.selector.MAIN, value = "angle"),
                branches.value(branch_selector = branches.selector.DESKTOP_BRANCHES, value = "dawn"),
                branches.value(branch_selector = branches.selector.MAIN, value = "infra"),
                branches.value(branch_selector = branches.selector.MAIN, value = "skia"),
                branches.value(branch_selector = branches.selector.MAIN, value = "swiftshader"),
                branches.value(branch_selector = branches.selector.MAIN, value = "v8"),
            ] if p != None],
        ),
    ],
)

# Shadow bucket of `try`, for led builds.
luci.bucket(
    name = "try.shadow",
    shadows = "try",
    constraints = luci.bucket_constraints(
        pools = ["luci.chromium.try", "luci.chromium.try.orchestrator"],
        service_accounts = [
            "chromium-cipd-try-builder@chops-service-accounts.iam.gserviceaccount.com",
            "chromium-orchestrator@chops-service-accounts.iam.gserviceaccount.com",
            "chromium-try-builder@chops-service-accounts.iam.gserviceaccount.com",
            "chromium-try-gpu-builder@chops-service-accounts.iam.gserviceaccount.com",
        ],
    ),
    bindings = [
        luci.binding(
            roles = "role/buildbucket.creator",
            groups = [
                "mdb/chrome-build-access-sphinx",
                "mdb/chrome-troopers",
                "chromium-led-users",
            ],
            users = [
                "chromium-orchestrator@chops-service-accounts.iam.gserviceaccount.com",
                "chromium-try-builder@chops-service-accounts.iam.gserviceaccount.com",
                "infra-try-recipes-tester@chops-service-accounts.iam.gserviceaccount.com",
            ],
        ),
        luci.binding(
            roles = "role/buildbucket.triggerer",
            users = [
                "chromium-try-builder@chops-service-accounts.iam.gserviceaccount.com",
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
        # Allow try builders to create invocations in their own builds.
        luci.binding(
            roles = "role/resultdb.invocationCreator",
            groups = [
                "project-chromium-try-task-accounts",
                "project-chromium-tryjob-access",
            ],
        ),
    ],
    dynamic = True,
)

luci.cq_group(
    name = "cq",
    retry_config = cq.RETRY_ALL_FAILURES,
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
        cq.run_mode(
            name = try_.MEGA_CQ_DRY_RUN_NAME,
            cq_label_value = 1,
            triggering_label = "Mega-CQ",
            triggering_value = 1,
        ),
        cq.run_mode(
            name = try_.MEGA_CQ_FULL_RUN_NAME,
            cq_label_value = 2,
            triggering_label = "Mega-CQ",
            triggering_value = 1,
        ),
    ],
    tree_status_host = "chromium-status.appspot.com" if settings.is_main else None,
    user_limit_default = cq.user_limit(
        name = "default-limit",
        run = cq.run_limits(max_active = 10),
    ),
    user_limits = [
        cq.user_limit(
            name = "chromium-src-emergency-quota",
            groups = ["chromium-src-emergency-quota"],
            run = cq.run_limits(max_active = None),
        ),
        cq.user_limit(
            name = "bots",
            users = [
                "chromium-autoroll@skia-public.iam.gserviceaccount.com",
            ],
            run = cq.run_limits(max_active = None),
        ),
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
    branch_selector = branches.selector.ALL_BRANCHES,
    title = "{} CQ Console".format(settings.project_title),
)

consoles.list_view(
    name = "luci.chromium.try",
    branch_selector = branches.selector.ALL_BRANCHES,
)

exec("./try/presubmit.star")
exec("./try/tryserver.blink.star")
exec("./try/tryserver.chromium.star")
exec("./try/tryserver.chromium.accessibility.star")
exec("./try/tryserver.chromium.android.star")
exec("./try/tryserver.chromium.android.desktop.star")
exec("./try/tryserver.chromium.angle.star")
exec("./try/tryserver.chromium.chromiumos.star")
exec("./try/tryserver.chromium.cft.star")
exec("./try/tryserver.chromium.dawn.star")
exec("./try/tryserver.chromium.enterprise_companion.star")
exec("./try/tryserver.chromium.fuchsia.star")
exec("./try/tryserver.chromium.fuzz.star")
exec("./try/tryserver.chromium.infra.star")
exec("./try/tryserver.chromium.linux.star")
exec("./try/tryserver.chromium.mac.star")
exec("./try/tryserver.chromium.rust.star")
exec("./try/tryserver.chromium.tricium.star")
exec("./try/tryserver.chromium.updater.star")
exec("./try/tryserver.chromium.win.star")
