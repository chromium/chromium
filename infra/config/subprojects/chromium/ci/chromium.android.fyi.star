# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Definitions of builders in the chromium.android.fyi builder group."""

load("//lib/builders.star", "goma", "os")
load("//lib/ci.star", "ci", "rbe_instance")
load("//lib/consoles.star", "consoles")

ci.defaults.set(
    builder_group = "chromium.android.fyi",
    cores = 8,
    executable = ci.DEFAULT_EXECUTABLE,
    execution_timeout = ci.DEFAULT_EXECUTION_TIMEOUT,
    goma_backend = goma.backend.RBE_PROD,
    os = os.LINUX_BIONIC_SWITCH_TO_DEFAULT,
    pool = ci.DEFAULT_POOL,
    service_account = ci.DEFAULT_SERVICE_ACCOUNT,
)

consoles.console_view(
    name = "chromium.android.fyi",
    ordering = {
        None: ["android", "memory", "weblayer", "webview"],
    },
)

ci.builder(
    name = "Android ASAN (dbg) (reclient)",
    console_view_entry = consoles.console_view_entry(
        category = "builder|arm",
        short_name = "san",
    ),
    # Higher build timeout since dbg ASAN builds can take a while on a clobber
    # build.
    execution_timeout = 4 * time.hour,
    reclient_instance = rbe_instance.DEFAULT,
    reclient_jobs = 150,
    schedule = "triggered",  # triggered manually via Scheduler UI
)

ci.builder(
    name = "android-pie-arm64-wpt-rel-non-cq",
    console_view_entry = consoles.console_view_entry(
        category = "builder_tester|arm64",
        short_name = "P-WPT",
    ),
)

ci.builder(
    name = "android-web-platform-pie-x86-fyi-rel",
    console_view_entry = consoles.console_view_entry(
        category = "builder_tester|web-platform",
        short_name = "P",
    ),
)

ci.builder(
    name = "android-weblayer-pie-x86-wpt-fyi-rel",
    console_view_entry = consoles.console_view_entry(
        category = "builder_tester|weblayer",
        short_name = "P",
    ),
)

ci.builder(
    name = "android-weblayer-pie-x86-wpt-smoketest",
    console_view_entry = consoles.console_view_entry(
        category = "builder_tester|weblayer",
        short_name = "P",
    ),
)

ci.builder(
    name = "android-webview-pie-x86-wpt-fyi-rel",
    console_view_entry = consoles.console_view_entry(
        category = "builder_tester|webview",
        short_name = "P",
    ),
)

ci.builder(
    name = "android-weblayer-10-x86-rel-tests",
    console_view_entry = consoles.console_view_entry(
        category = "tester|weblayer",
        short_name = "10",
    ),
    triggered_by = ["android-weblayer-with-aosp-webview-x86-fyi-rel"],
    notifies = ["weblayer-sheriff"],
)

ci.builder(
    name = "android-weblayer-with-aosp-webview-x86-fyi-rel",
    console_view_entry = consoles.console_view_entry(
        category = "builder|weblayer_with_aosp_webview",
        short_name = "x86",
    ),
)

ci.builder(
    name = "Android WebView P FYI (rel)",
    console_view_entry = consoles.console_view_entry(
        category = "webview",
        short_name = "p-rel",
    ),
)

# TODO(crbug.com/1022533#c40): Remove this builder once there are no associated
# disabled tests.
ci.builder(
    name = "android-pie-x86-fyi-rel",
    console_view_entry = consoles.console_view_entry(
        category = "emulator|P|x86",
        short_name = "rel",
    ),
    goma_jobs = goma.jobs.J150,
    # Set to an empty list to avoid chromium-gitiles-trigger triggering new
    # builds. Also we don't set any `schedule` since this builder is for
    # reference only and should not run any new builds.
    triggered_by = [],
)

ci.builder(
    name = "android-10-x86-fyi-rel-tests",
    console_view_entry = consoles.console_view_entry(
        category = "tester|10",
        short_name = "10",
    ),
    triggered_by = ["android-x86-fyi-rel"],
)

# TODO(crbug.com/1137474): Remove this builder once there are no associated
# disabled tests.
ci.builder(
    name = "android-11-x86-fyi-rel",
    console_view_entry = consoles.console_view_entry(
        category = "emulator|11|x86",
        short_name = "rel",
    ),
    # Set to an empty list to avoid chromium-gitiles-trigger triggering new
    # builds. Also we don't set any `schedule` since this builder is for
    # reference only and should not run any new builds.
    triggered_by = [],
)

ci.builder(
    name = "android-12-x64-fyi-rel",
    console_view_entry = consoles.console_view_entry(
        category = "emulator|12|x64",
        short_name = "rel",
    ),
    # Bump to 6h for now since compile on x64 seems slower than x86. It could
    # take 3h on Android-12 (For example ci.chromium.org/b/8841892751541698720)
    # vs 1h on Android-11 (For example ci.chromium.org/b/8841899947736889024)
    # TODO(crbug.com/1229245): Look into ways to improve the compile time.
    execution_timeout = 6 * time.hour,
)

ci.builder(
    name = "android-annotator-rel",
    console_view_entry = consoles.console_view_entry(
        category = "network|traffic|annotations",
        short_name = "and",
    ),
    notifies = ["annotator-rel"],
)

ci.builder(
    name = "android-x86-fyi-rel",
    console_view_entry = consoles.console_view_entry(
        category = "builder|x86",
        short_name = "x86",
    ),
)
