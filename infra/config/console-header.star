# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

load("//lib/branches.star", "branches")
load("//lib/headers.star", "headers")
load(".//project.star", "ACTIVE_MILESTONES", "settings")

HEADER = headers.header(
    oncalls = [
        headers.oncall(
            name = "Chromium",
            url = "https://chrome-ops-rotation-proxy.appspot.com/current/oncallator:chrome-build-sheriff",
        ),
        headers.oncall(
            name = "Chromium Branches",
            branch_selector = [branches.STANDARD_BRANCHES, branches.DESKTOP_EXTENDED_STABLE_BRANCHES],
            url = "https://chrome-ops-rotation-proxy.appspot.com/current/oncallator:chrome-branch-sheriff",
        ),
        headers.oncall(
            name = "Android",
            url = "https://chrome-ops-rotation-proxy.appspot.com/current/oncallator:chrome-android-sheriff",
        ),
        headers.oncall(
            name = "iOS",
            url = "https://chrome-ops-rotation-proxy.appspot.com/current/oncallator:chrome-ios",
        ),
        headers.oncall(
            name = "ChromeOS",
            url = "https://chrome-ops-rotation-proxy.appspot.com/current/oncallator:chromeos-gardeners",
        ),
        headers.oncall(
            name = "Fuchsia",
            url = "https://chrome-ops-rotation-proxy.appspot.com/current/grotation:chrome-fuchsia-gardener",
        ),
        headers.oncall(
            name = "GPU",
            url = "https://chrome-ops-rotation-proxy.appspot.com/current/oncallator:chrome-gpu-pixel-wrangler-weekly",
        ),
        headers.oncall(
            name = "ANGLE",
            url = "https://chrome-ops-rotation-proxy.appspot.com/current/grotation:angle-wrangler",
        ),
        headers.oncall(
            name = "Perf",
            url = "https://chrome-ops-rotation-proxy.appspot.com/current/grotation:chromium-perf-regression-sheriff",
        ),
        headers.oncall(
            name = "Perfbot",
            url = "https://chrome-ops-rotation-proxy.appspot.com/current/grotation:chromium-perf-bot-sheriff",
        ),
        headers.oncall(
            name = "Trooper",
            branch_selector = branches.ALL_BRANCHES,
            url = "https://chrome-ops-rotation-proxy.appspot.com/current/oncallator:chrome-ops-client-infra",
            show_primary_secondary_labels = True,
        ),
    ],
    link_groups = [
        headers.link_group(
            name = "Builds",
            links = [
                headers.link(
                    text = "continuous",
                    branch_selector = branches.ALL_BRANCHES,
                    url = "https://commondatastorage.googleapis.com/chromium-browser-snapshots/index.html",
                    alt = "Continuous browser snapshots",
                ),
                headers.link(
                    text = "symbols",
                    branch_selector = branches.ALL_BRANCHES,
                    url = "https://www.chromium.org/developers/how-tos/debugging-on-windows",
                    alt = "Windows Symbols",
                ),
                headers.link(
                    text = "status",
                    url = "https://chromium-status.appspot.com/",
                    alt = "Current tree status",
                ),
            ],
        ),
        headers.link_group(
            name = "Dashboards",
            links = [
                headers.link(
                    text = "perf",
                    branch_selector = branches.ALL_BRANCHES,
                    url = "https://chromeperf.appspot.com/",
                    alt = "Chrome perf dashboard",
                ),
                headers.link(
                    text = "flake-portal",
                    branch_selector = branches.ALL_BRANCHES,
                    url = "https://analysis.chromium.org/p/chromium/flake-portal",
                    alt = "New flake portal",
                ),
                headers.link(
                    text = "legacy-flakiness",
                    branch_selector = branches.ALL_BRANCHES,
                    url = "https://test-results.appspot.com/dashboards/flakiness_dashboard.html",
                    alt = "Legacy flakiness dashboard",
                ),
            ],
        ),
        headers.link_group(
            name = "Chromium",
            links = [
                headers.link(
                    text = "source",
                    branch_selector = branches.ALL_BRANCHES,
                    url = branches.value(
                        {
                            branches.MAIN: "https://chromium.googlesource.com/chromium/src",
                        },
                        default = "https://chromium.googlesource.com/chromium/src/+/{}".format(settings.ref),
                    ),
                    alt = "Chromium source code repository",
                ),
                headers.link(
                    text = "reviews",
                    branch_selector = branches.ALL_BRANCHES,
                    url = "https://chromium-review.googlesource.com",
                    alt = "Chromium code review tool",
                ),
                headers.link(
                    text = "bugs",
                    branch_selector = branches.ALL_BRANCHES,
                    url = "https://crbug.com",
                    alt = "Chromium bug tracker",
                ),
                headers.link(
                    text = "coverage",
                    branch_selector = branches.ALL_BRANCHES,
                    url = "https://analysis.chromium.org/coverage/p/chromium",
                    alt = "Chromium code coverage dashboard",
                ),
                headers.link(
                    text = "dev",
                    branch_selector = branches.ALL_BRANCHES,
                    url = "https://dev.chromium.org/Home",
                    alt = "Chromium developer home page",
                ),
                headers.link(
                    text = "support",
                    branch_selector = branches.ALL_BRANCHES,
                    url = "https://support.google.com/chrome/#topic=7438008",
                    alt = "Google Chrome help center",
                ),
            ],
        ),
        headers.link_group(
            name = "Consoles",
            links = [
                headers.link(
                    text = "android",
                    branch_selector = branches.STANDARD_MILESTONE,
                    url = "/p/{}/g/chromium.android".format(settings.project),
                    alt = "Chromium Android console",
                ),
                headers.link(
                    text = "clang",
                    url = "/p/{}/g/chromium.clang".format(settings.project),
                    alt = "Chromium Clang console",
                ),
                headers.link(
                    text = "dawn",
                    branch_selector = branches.STANDARD_MILESTONE,
                    url = "/p/{}/g/chromium.dawn".format(settings.project),
                    alt = "Chromium Dawn console",
                ),
                headers.link(
                    text = "fuzz",
                    url = "/p/{}/g/chromium.fuzz".format(settings.project),
                    alt = "Chromium Fuzz console",
                ),
                headers.link(
                    text = "fuchsia",
                    branch_selector = branches.FUCHSIA_LTS_MILESTONE,
                    url = "/p/{}/g/chromium.fuchsia".format(settings.project),
                    alt = "Chromium Fuchsia console",
                ),
                headers.link(
                    text = "fyi",
                    branch_selector = branches.STANDARD_MILESTONE,
                    url = "/p/{}/g/chromium.fyi".format(settings.project),
                    alt = "Chromium FYI console",
                ),
                headers.link(
                    text = "gpu",
                    branch_selector = branches.STANDARD_MILESTONE,
                    url = "/p/{}/g/chromium.gpu".format(settings.project),
                    alt = "Chromium GPU console",
                ),
                headers.link(
                    text = "packager",
                    url = "/p/{}/g/chromium.packager".format(settings.project),
                    alt = "Chromium Packager console",
                ),
                headers.link(
                    text = "perf",
                    url = "/p/chrome/g/chrome.perf/console",
                    alt = "Chromium Perf console",
                ),
                headers.link(
                    text = "perf.fyi",
                    url = "/p/chrome/g/chrome.perf.fyi/console",
                    alt = "Chromium Perf FYI console",
                ),
                headers.link(
                    text = "angle",
                    url = "/p/{}/g/chromium.angle".format(settings.project),
                    alt = "Chromium ANGLE console",
                ),
                headers.link(
                    text = "swangle",
                    url = "/p/{}/g/chromium.swangle".format(settings.project),
                    alt = "Chromium SWANGLE console",
                ),
                headers.link(
                    text = "updater",
                    url = "/p/{}/g/chromium.updater".format(settings.project),
                    alt = "Chromium Updater console",
                ),
                headers.link(
                    text = "webrtc",
                    url = "/p/{}/g/chromium.webrtc".format(settings.project),
                    alt = "Chromium WebRTC console",
                ),
                headers.link(
                    text = "chromiumos",
                    branch_selector = branches.CROS_LTS_MILESTONE,
                    url = "/p/{}/g/chromium.chromiumos".format(settings.project),
                    alt = "ChromiumOS console",
                ),
                headers.link(
                    text = "flakiness",
                    url = "/p/{}/g/chromium.flakiness".format(settings.project),
                    alt = "Chromium Flakiness console",
                ),
            ],
        ),
        headers.link_group(
            name = "Branch Consoles",
            links = [
                headers.link(
                    text = milestone,
                    url = "/p/{}/g/main/console".format(details.project),
                )
                for milestone, details in ACTIVE_MILESTONES.items()
            ] + [
                headers.link(
                    text = "trunk",
                    branch_selector = branches.ALL_BRANCHES,
                    url = "/p/chromium/g/main/console",
                    alt = "Trunk (ToT) console",
                ),
            ],
        ),
        headers.link_group(
            name = "Tryservers",
            links = [
                headers.link(
                    text = "android",
                    branch_selector = branches.STANDARD_MILESTONE,
                    url = "/p/{}/g/tryserver.chromium.android/builders".format(settings.project),
                    alt = "Android",
                ),
                headers.link(
                    text = "angle",
                    url = "/p/{}/g/tryserver.chromium.angle/builders".format(settings.project),
                    alt = "Angle",
                ),
                headers.link(
                    text = "blink",
                    branch_selector = branches.STANDARD_MILESTONE,
                    url = "/p/{}/g/tryserver.blink/builders".format(settings.project),
                    alt = "Blink",
                ),
                headers.link(
                    text = "chrome",
                    url = "/p/chrome/g/tryserver.chrome/builders",
                    alt = "Chrome",
                ),
                headers.link(
                    text = "chromiumos",
                    branch_selector = branches.CROS_LTS_MILESTONE,
                    url = "/p/{}/g/tryserver.chromium.chromiumos/builders".format(settings.project),
                    alt = "ChromiumOS",
                ),
                headers.link(
                    text = "fuchsia",
                    branch_selector = branches.FUCHSIA_LTS_MILESTONE,
                    url = "/p/{}/g/tryserver.chromium.fuchsia/builders".format(settings.project),
                    alt = "Fuchsia",
                ),
                headers.link(
                    text = "linux",
                    branch_selector = branches.STANDARD_MILESTONE,
                    url = "/p/{}/g/tryserver.chromium.linux/builders".format(settings.project),
                    alt = "Linux",
                ),
                headers.link(
                    text = "mac",
                    branch_selector = branches.STANDARD_MILESTONE,
                    url = "/p/{}/g/tryserver.chromium.mac/builders".format(settings.project),
                    alt = "Mac",
                ),
                headers.link(
                    text = "swangle",
                    url = "/p/{}/g/tryserver.chromium.swangle/builders".format(settings.project),
                    alt = "SWANGLE",
                ),
                headers.link(
                    text = "tricium",
                    url = "/p/{}/g/tryserver.chromium.tricium/builders".format(settings.project),
                    alt = "Tricium",
                ),
                headers.link(
                    text = "win",
                    branch_selector = branches.STANDARD_MILESTONE,
                    url = "/p/{}/g/tryserver.chromium.win/builders".format(settings.project),
                    alt = "Win",
                ),
            ],
        ),
        headers.link_group(
            name = "Navigate",
            links = [
                headers.link(
                    text = "about",
                    branch_selector = branches.ALL_BRANCHES,
                    url = "http://dev.chromium.org/developers/testing/chromium-build-infrastructure/tour-of-the-chromium-buildbot",
                    alt = "Tour of the console",
                ),
                headers.link(
                    text = "customize",
                    branch_selector = branches.ALL_BRANCHES,
                    url = "https://chromium.googlesource.com/chromium/src/+/{}/infra/config/generated/luci/luci-milo.cfg".format(settings.ref),
                    alt = "Customize this console",
                ),
            ],
        ),
    ],
    console_groups = [
        headers.console_group(
            title = headers.link(
                text = "Tree Closers",
                url = "https://chromium-status.appspot.com/",
            ),
            console_ids = [
                "chromium/chromium",
                "chromium/chromium.win",
                "chromium/chromium.mac",
                "chromium/chromium.linux",
                "chromium/chromium.chromiumos",
                "chromium/chromium.fuchsia",
                "chrome/chrome",
                "chromium/chromium.memory",
                "chromium/chromium.gpu",
            ],
        ),
        headers.console_group(
            console_ids = [
                "chromium/chromium.android",
                "chrome/chrome.perf",
                "chromium/chromium.fuchsia.fyi",
                "chromium/chromium.gpu.fyi",
                "chromium/chromium.angle",
                "chromium/chromium.swangle",
                "chromium/chromium.fuzz",
            ],
        ),
        headers.console_group(
            branch_selector = branches.STANDARD_BRANCHES,
            console_ids = ["{}/{}".format(settings.project, c) for c in [
                "chromium",
                "chromium.win",
                "chromium.mac",
                "chromium.linux",
                "chromium.chromiumos",
                "chromium.memory",
                "chromium.gpu",
                "chromium.android",
            ]],
        ),
        headers.console_group(
            branch_selector = branches.CROS_LTS_BRANCHES,
            console_ids = ["{}/{}".format(settings.project, c) for c in [
                "chromium.chromiumos",
            ]],
        ),
    ],
    tree_status_host = "chromium-status.appspot.com" if settings.is_main else None,
)
