# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Definitions of builders in the chromium.android builder group."""

load("//lib/args.star", "args")
load("//lib/builders.star", "goma", "os", "sheriff_rotations")
load("//lib/branches.star", "branches")
load("//lib/ci.star", "ci", "rbe_instance", "rbe_jobs")
load("//lib/consoles.star", "consoles")

ci.defaults.set(
    builder_group = "chromium.android",
    cores = 8,
    executable = ci.DEFAULT_EXECUTABLE,
    execution_timeout = ci.DEFAULT_EXECUTION_TIMEOUT,
    goma_backend = goma.backend.RBE_PROD,
    # TODO(tandrii): migrate to this gradually (current value of
    # goma.jobs.MANY_JOBS_FOR_CI is 500).
    # goma_jobs=goma.jobs.MANY_JOBS_FOR_CI
    goma_jobs = goma.jobs.J150,
    os = os.LINUX_BIONIC_SWITCH_TO_DEFAULT,
    pool = ci.DEFAULT_POOL,
    service_account = ci.DEFAULT_SERVICE_ACCOUNT,
    sheriff_rotations = sheriff_rotations.ANDROID,
)

consoles.console_view(
    name = "chromium.android",
    branch_selector = branches.STANDARD_MILESTONE,
    ordering = {
        None: ["cronet", "builder", "tester"],
        "*cpu*": ["arm", "arm64", "x86"],
        "cronet": "*cpu*",
        "builder": "*cpu*",
        "builder|det": consoles.ordering(short_names = ["rel", "dbg"]),
        "tester": ["phone", "tablet"],
        "builder_tester|arm64": consoles.ordering(short_names = ["M proguard"]),
    },
)

ci.builder(
    name = "Android ASAN (dbg)",
    console_view_entry = consoles.console_view_entry(
        category = "builder|arm",
        short_name = "san",
    ),
    # Higher build timeout since dbg ASAN builds can take a while on a clobber
    # build.
    execution_timeout = 4 * time.hour,
    goma_backend = None,
    reclient_instance = rbe_instance.DEFAULT,
    reclient_jobs = rbe_jobs.HIGH_JOBS_FOR_CI,
    tree_closing = True,
)

ci.builder(
    name = "Android WebView M (dbg)",
    branch_selector = branches.STANDARD_MILESTONE,
    console_view_entry = consoles.console_view_entry(
        category = "tester|webview",
        short_name = "M",
    ),
    cq_mirrors_console_view = "mirrors",
    triggered_by = ["ci/Android arm64 Builder (dbg)"],
)

ci.builder(
    name = "Android WebView N (dbg)",
    branch_selector = branches.STANDARD_MILESTONE,
    console_view_entry = consoles.console_view_entry(
        category = "tester|webview",
        short_name = "N",
    ),
    cq_mirrors_console_view = "mirrors",
    triggered_by = ["ci/Android arm64 Builder (dbg)"],
)

ci.builder(
    name = "Android WebView O (dbg)",
    branch_selector = branches.STANDARD_MILESTONE,
    console_view_entry = consoles.console_view_entry(
        category = "tester|webview",
        short_name = "O",
    ),
    cq_mirrors_console_view = "mirrors",
    triggered_by = ["ci/Android arm64 Builder (dbg)"],
)

ci.builder(
    name = "Android WebView P (dbg)",
    branch_selector = branches.STANDARD_MILESTONE,
    console_view_entry = consoles.console_view_entry(
        category = "tester|webview",
        short_name = "P",
    ),
    cq_mirrors_console_view = "mirrors",
    triggered_by = ["ci/Android arm64 Builder (dbg)"],
)

ci.builder(
    name = "Android arm Builder (dbg)",
    branch_selector = branches.STANDARD_MILESTONE,
    console_view_entry = consoles.console_view_entry(
        category = "builder|arm",
        short_name = "32",
    ),
    cq_mirrors_console_view = "mirrors",
    execution_timeout = 4 * time.hour,
    goma_backend = None,
    reclient_instance = rbe_instance.DEFAULT,
    reclient_jobs = rbe_jobs.DEFAULT,
    tree_closing = True,
)

ci.builder(
    name = "Android arm64 Builder (dbg)",
    branch_selector = branches.STANDARD_MILESTONE,
    console_view_entry = consoles.console_view_entry(
        category = "builder|arm",
        short_name = "64",
    ),
    cq_mirrors_console_view = "mirrors",
    execution_timeout = 7 * time.hour,
    goma_backend = None,
    reclient_instance = rbe_instance.DEFAULT,
    reclient_jobs = rbe_jobs.HIGH_JOBS_FOR_CI,
    tree_closing = True,
)

ci.builder(
    name = "Android x64 Builder (dbg)",
    branch_selector = branches.STANDARD_MILESTONE,
    console_view_entry = consoles.console_view_entry(
        category = "builder|x86",
        short_name = "64",
    ),
    cq_mirrors_console_view = "mirrors",
    execution_timeout = 7 * time.hour,
    goma_backend = None,
    reclient_instance = rbe_instance.DEFAULT,
    reclient_jobs = rbe_jobs.HIGH_JOBS_FOR_CI,
)

ci.builder(
    name = "Android x86 Builder (dbg)",
    branch_selector = branches.STANDARD_MILESTONE,
    console_view_entry = consoles.console_view_entry(
        category = "builder|x86",
        short_name = "32",
    ),
    cq_mirrors_console_view = "mirrors",
    execution_timeout = 6 * time.hour,
    goma_backend = None,
    reclient_instance = rbe_instance.DEFAULT,
    reclient_jobs = rbe_jobs.HIGH_JOBS_FOR_CI,
)

ci.builder(
    name = "Cast Android (dbg)",
    branch_selector = branches.STANDARD_MILESTONE,
    console_view_entry = consoles.console_view_entry(
        category = "on_cq",
        short_name = "cst",
    ),
    cq_mirrors_console_view = "mirrors",
    tree_closing = True,
    goma_backend = None,
    reclient_jobs = rbe_jobs.HIGH_JOBS_FOR_CI,
    reclient_instance = rbe_instance.DEFAULT,
)

ci.builder(
    name = "Deterministic Android",
    console_view_entry = consoles.console_view_entry(
        category = "builder|det",
        short_name = "rel",
    ),
    cores = 32,
    executable = "recipe:swarming/deterministic_build",
    execution_timeout = 7 * time.hour,
    goma_jobs = goma.jobs.MANY_JOBS_FOR_CI,
    notifies = ["Deterministic Android"],
    tree_closing = True,
)

ci.builder(
    name = "Deterministic Android (dbg)",
    console_view_entry = consoles.console_view_entry(
        category = "builder|det",
        short_name = "dbg",
    ),
    executable = "recipe:swarming/deterministic_build",
    execution_timeout = 6 * time.hour,
    notifies = ["Deterministic Android"],
    tree_closing = True,
)

ci.builder(
    name = "Marshmallow 64 bit Tester",
    branch_selector = branches.STANDARD_MILESTONE,
    console_view_entry = consoles.console_view_entry(
        category = "tester|phone",
        short_name = "M",
    ),
    cq_mirrors_console_view = "mirrors",
    triggered_by = ["ci/Android arm64 Builder (dbg)"],
)

ci.builder(
    name = "Marshmallow Tablet Tester",
    console_view_entry = consoles.console_view_entry(
        category = "tester|tablet",
        short_name = "M",
    ),
    # We have limited tablet capacity and thus limited ability to run
    # tests in parallel, hence the high timeout.
    execution_timeout = 12 * time.hour,
    triggered_by = ["ci/Android arm Builder (dbg)"],
)

ci.builder(
    name = "Nougat Phone Tester",
    branch_selector = branches.STANDARD_MILESTONE,
    console_view_entry = consoles.console_view_entry(
        category = "tester|phone",
        short_name = "N",
    ),
    cq_mirrors_console_view = "mirrors",
    triggered_by = ["ci/Android arm64 Builder (dbg)"],
)

ci.builder(
    name = "Oreo Phone Tester",
    branch_selector = branches.STANDARD_MILESTONE,
    console_view_entry = consoles.console_view_entry(
        category = "tester|phone",
        short_name = "O",
    ),
    cq_mirrors_console_view = "mirrors",
    sheriff_rotations = args.ignore_default(None),
    triggered_by = ["ci/Android arm64 Builder (dbg)"],
)

ci.builder(
    name = "android-10-arm64-rel",
    console_view_entry = consoles.console_view_entry(
        category = "builder_tester|arm64",
        short_name = "10",
    ),
    goma_backend = None,
    reclient_instance = rbe_instance.DEFAULT,
    reclient_jobs = rbe_jobs.HIGH_JOBS_FOR_CI,
)

ci.builder(
    name = "android-arm64-proguard-rel",
    console_view_entry = consoles.console_view_entry(
        category = "builder_tester|arm64",
        short_name = "M proguard",
    ),
    execution_timeout = 6 * time.hour,
    goma_backend = None,
    reclient_instance = rbe_instance.DEFAULT,
    reclient_jobs = rbe_jobs.HIGH_JOBS_FOR_CI,
)

ci.builder(
    name = "android-bfcache-rel",
    console_view_entry = consoles.console_view_entry(
        category = "bfcache",
        short_name = "bfc",
    ),
    goma_backend = None,
    reclient_instance = rbe_instance.DEFAULT,
    reclient_jobs = rbe_jobs.HIGH_JOBS_FOR_CI,
)

ci.builder(
    name = "android-binary-size-generator",
    builderless = False,
    cores = 32,
    console_view_entry = consoles.console_view_entry(
        category = "builder|other",
        short_name = "size",
    ),
    executable = "recipe:binary_size_generator_tot",
    ssd = True,
    goma_backend = None,
    reclient_instance = rbe_instance.DEFAULT,
    reclient_jobs = rbe_jobs.DEFAULT,
)

ci.builder(
    name = "android-cronet-arm-dbg",
    branch_selector = branches.STANDARD_MILESTONE,
    console_view_entry = consoles.console_view_entry(
        category = "cronet|arm",
        short_name = "dbg",
    ),
    cq_mirrors_console_view = "mirrors",
    notifies = ["cronet"],
    goma_backend = None,
    reclient_instance = rbe_instance.DEFAULT,
    reclient_jobs = rbe_jobs.HIGH_JOBS_FOR_CI,
)

ci.builder(
    name = "android-cronet-arm-rel",
    branch_selector = branches.STANDARD_MILESTONE,
    console_view_entry = consoles.console_view_entry(
        category = "cronet|arm",
        short_name = "rel",
    ),
    cq_mirrors_console_view = "mirrors",
    notifies = ["cronet"],
    goma_backend = None,
    reclient_instance = rbe_instance.DEFAULT,
    reclient_jobs = rbe_jobs.HIGH_JOBS_FOR_CI,
)

ci.builder(
    name = "android-cronet-arm64-dbg",
    console_view_entry = consoles.console_view_entry(
        category = "cronet|arm64",
        short_name = "dbg",
    ),
    notifies = ["cronet"],
    goma_backend = None,
    reclient_instance = rbe_instance.DEFAULT,
    reclient_jobs = rbe_jobs.HIGH_JOBS_FOR_CI,
)

ci.builder(
    name = "android-cronet-arm64-rel",
    console_view_entry = consoles.console_view_entry(
        category = "cronet|arm64",
        short_name = "rel",
    ),
    notifies = ["cronet"],
    goma_backend = None,
    reclient_instance = rbe_instance.DEFAULT,
    reclient_jobs = rbe_jobs.HIGH_JOBS_FOR_CI,
)

ci.builder(
    name = "android-cronet-asan-arm-rel",
    console_view_entry = consoles.console_view_entry(
        category = "cronet|asan",
    ),
    notifies = ["cronet"],
    goma_backend = None,
    reclient_instance = rbe_instance.DEFAULT,
    reclient_jobs = rbe_jobs.HIGH_JOBS_FOR_CI,
)

ci.builder(
    name = "android-cronet-arm-rel-kitkat-tests",
    branch_selector = branches.STANDARD_MILESTONE,
    console_view_entry = consoles.console_view_entry(
        category = "cronet|test",
        short_name = "k",
    ),
    cq_mirrors_console_view = "mirrors",
    notifies = ["cronet"],
    triggered_by = ["ci/android-cronet-arm-rel"],
)

# Runs on a specific machine with an attached phone
ci.builder(
    name = "android-cronet-marshmallow-arm64-perf-rel",
    console_view_entry = consoles.console_view_entry(
        category = "cronet|test|perf",
        short_name = "m",
    ),
    cores = None,
    cpu = None,
    executable = "recipe:cronet",
    notifies = ["cronet"],
    os = os.ANDROID,
    goma_backend = None,
    reclient_instance = rbe_instance.DEFAULT,
    reclient_jobs = rbe_jobs.DEFAULT,
)

ci.builder(
    name = "android-cronet-x86-dbg",
    console_view_entry = consoles.console_view_entry(
        category = "cronet|x86",
        short_name = "dbg",
    ),
    notifies = ["cronet"],
    goma_backend = None,
    reclient_instance = rbe_instance.DEFAULT,
    reclient_jobs = rbe_jobs.HIGH_JOBS_FOR_CI,
)

ci.builder(
    name = "android-cronet-x86-dbg-oreo-tests",
    console_view_entry = consoles.console_view_entry(
        category = "cronet|test",
        short_name = "o",
    ),
    notifies = ["cronet"],
    triggered_by = ["ci/android-cronet-x86-dbg"],
)

ci.builder(
    name = "android-cronet-x86-dbg-pie-tests",
    console_view_entry = consoles.console_view_entry(
        category = "cronet|test",
        short_name = "p",
    ),
    notifies = ["cronet"],
    triggered_by = ["ci/android-cronet-x86-dbg"],
)

ci.builder(
    name = "android-cronet-x86-dbg-10-tests",
    console_view_entry = consoles.console_view_entry(
        category = "cronet|test",
        short_name = "10",
    ),
    notifies = ["cronet"],
    triggered_by = ["ci/android-cronet-x86-dbg"],
)

ci.builder(
    name = "android-cronet-x86-dbg-11-tests",
    console_view_entry = consoles.console_view_entry(
        category = "cronet|test",
        short_name = "11",
    ),
    notifies = ["cronet"],
    triggered_by = ["ci/android-cronet-x86-dbg"],
)

ci.builder(
    name = "android-cronet-x86-rel",
    console_view_entry = consoles.console_view_entry(
        category = "cronet|x86",
        short_name = "rel",
    ),
    notifies = ["cronet"],
    goma_backend = None,
    reclient_instance = rbe_instance.DEFAULT,
    reclient_jobs = rbe_jobs.HIGH_JOBS_FOR_CI,
)

ci.builder(
    name = "android-marshmallow-arm64-rel",
    branch_selector = branches.STANDARD_MILESTONE,
    console_view_entry = consoles.console_view_entry(
        category = "on_cq",
        short_name = "M",
    ),
    cq_mirrors_console_view = "mirrors",
    execution_timeout = 4 * time.hour,
    goma_backend = None,
    reclient_instance = rbe_instance.DEFAULT,
    reclient_jobs = rbe_jobs.HIGH_JOBS_FOR_CI,
    tree_closing = True,
)

ci.builder(
    name = "android-marshmallow-x86-rel",
    branch_selector = branches.STANDARD_MILESTONE,
    console_view_entry = consoles.console_view_entry(
        category = "on_cq|x86",
        short_name = "M",
    ),
    cq_mirrors_console_view = "mirrors",
    tree_closing = True,
    goma_backend = None,
    reclient_instance = rbe_instance.DEFAULT,
    reclient_jobs = rbe_jobs.HIGH_JOBS_FOR_CI,
)

ci.builder(
    name = "android-marshmallow-x86-rel-non-cq",
    console_view_entry = consoles.console_view_entry(
        category = "builder_tester|x86",
        short_name = "M_non-cq",
    ),
    goma_backend = None,
    reclient_instance = rbe_instance.DEFAULT,
    reclient_jobs = rbe_jobs.HIGH_JOBS_FOR_CI,
)

ci.builder(
    name = "android-pie-arm64-dbg",
    branch_selector = branches.STANDARD_MILESTONE,
    console_view_entry = consoles.console_view_entry(
        category = "tester|phone",
        short_name = "P",
    ),
    cq_mirrors_console_view = "mirrors",
    triggered_by = ["ci/Android arm64 Builder (dbg)"],
    goma_backend = None,
    reclient_instance = rbe_instance.DEFAULT,
    reclient_jobs = rbe_jobs.HIGH_JOBS_FOR_CI,
)

# TODO(crbug/1182468) Remove android coverage bots after coverage is
# running on CQ.
ci.builder(
    name = "android-pie-arm64-coverage-experimental-rel",
    console_view_entry = consoles.console_view_entry(
        category = "builder_tester|arm64",
        short_name = "p-cov",
    ),
    goma_backend = None,
    reclient_instance = rbe_instance.DEFAULT,
    reclient_jobs = rbe_jobs.HIGH_JOBS_FOR_CI,
    sheriff_rotations = args.ignore_default(None),
)

ci.builder(
    name = "android-pie-arm64-rel",
    branch_selector = branches.STANDARD_MILESTONE,
    console_view_entry = consoles.console_view_entry(
        category = "on_cq",
        short_name = "P",
    ),
    cq_mirrors_console_view = "mirrors",
    execution_timeout = 4 * time.hour,
    tree_closing = True,
    goma_backend = None,
    reclient_instance = rbe_instance.DEFAULT,
    reclient_jobs = rbe_jobs.HIGH_JOBS_FOR_CI,
)

ci.builder(
    name = "android-pie-x86-rel",
    console_view_entry = consoles.console_view_entry(
        category = "builder_tester|x86",
        short_name = "P",
    ),
    goma_backend = None,
    reclient_instance = rbe_instance.DEFAULT,
    reclient_jobs = rbe_jobs.HIGH_JOBS_FOR_CI,
)

# TODO(crbug.com/1137474): Update the console view config once on CQ
ci.builder(
    name = "android-11-x86-rel",
    console_view_entry = consoles.console_view_entry(
        category = "builder_tester|x86",
        short_name = "11",
    ),
    tree_closing = True,
    goma_backend = None,
    reclient_instance = rbe_instance.DEFAULT,
    reclient_jobs = rbe_jobs.HIGH_JOBS_FOR_CI,
)

ci.builder(
    name = "android-12-x64-rel",
    console_view_entry = consoles.console_view_entry(
        category = "builder_tester|x64",
        short_name = "12",
    ),
    execution_timeout = 4 * time.hour,
    goma_backend = None,
    reclient_instance = rbe_instance.DEFAULT,
    reclient_jobs = rbe_jobs.HIGH_JOBS_FOR_CI,
)

ci.builder(
    name = "android-weblayer-10-x86-rel-tests",
    console_view_entry = consoles.console_view_entry(
        category = "tester|weblayer",
        short_name = "10",
    ),
    triggered_by = ["android-weblayer-with-aosp-webview-x86-rel"],
    notifies = ["weblayer-sheriff"],
)

ci.builder(
    name = "android-weblayer-marshmallow-x86-rel-tests",
    console_view_entry = consoles.console_view_entry(
        category = "tester|weblayer",
        short_name = "M",
    ),
    triggered_by = ["android-weblayer-with-aosp-webview-x86-rel"],
    notifies = ["weblayer-sheriff"],
)

ci.builder(
    name = "android-weblayer-oreo-x86-rel-tests",
    console_view_entry = consoles.console_view_entry(
        category = "tester|weblayer",
        short_name = "O",
    ),
    triggered_by = ["android-weblayer-x86-rel"],
    notifies = ["weblayer-sheriff"],
)

ci.builder(
    name = "android-weblayer-pie-x86-rel-tests",
    console_view_entry = consoles.console_view_entry(
        category = "tester|weblayer",
        short_name = "P",
    ),
    triggered_by = ["android-weblayer-x86-rel"],
    notifies = ["weblayer-sheriff"],
)

ci.builder(
    name = "android-weblayer-with-aosp-webview-x86-rel",
    console_view_entry = consoles.console_view_entry(
        category = "builder|weblayer_with_aosp_webview",
        short_name = "x86",
    ),
)

ci.builder(
    name = "android-weblayer-x86-rel",
    console_view_entry = consoles.console_view_entry(
        category = "builder|weblayer",
        short_name = "x86",
    ),
)
