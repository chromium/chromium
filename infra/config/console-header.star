# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

load("//lib/branches.star", "branches")
load(".//project.star", "ACTIVE_MILESTONES", "settings")

def _remove_none(l):
    return [e for e in l if e != None]

def _remove_none_values(d):
    return {k: v for k, v in d.items() if v != None}

def _oncall(*, name, url, show_primary_secondary_labels = None, branch_selector = branches.MAIN):
    if not branches.matches(branch_selector):
        return None
    return _remove_none_values(dict(
        name = name,
        url = url,
        show_primary_secondary_labels = show_primary_secondary_labels,
    ))

def _link(*, url, text, alt, branch_selector = branches.MAIN):
    if not branches.matches(branch_selector):
        return None
    return _remove_none_values(dict(
        url = url,
        text = text,
        alt = alt,
    ))

def _link_group(*, name, links):
    links = _remove_none(links)
    if not links:
        return None
    return _remove_none_values(dict(
        name = name,
        links = links,
    ))

def _console_group_title(*, text, url):
    return _remove_none_values(dict(
        text = text,
        url = url,
    ))

def _console_group(*, console_ids, title = None, branch_selector = branches.MAIN):
    if not branches.matches(branch_selector):
        return None
    console_ids = _remove_none(console_ids)
    if not console_ids:
        return None
    return _remove_none_values(dict(
        title = title,
        console_ids = console_ids,
    ))

def _header(*, oncalls, link_groups, console_groups, tree_status_host):
    return _remove_none_values(dict(
        oncalls = _remove_none(oncalls),
        links = _remove_none(link_groups),
        console_groups = _remove_none(console_groups),
        tree_status_host = tree_status_host,
    ))

HEADER = _header(
    oncalls = [
        _oncall(
            name = "Chromium",
            url = "https://chrome-ops-rotation-proxy.appspot.com/current/oncallator:chrome-build-sheriff",
        ),
        _oncall(
            name = "Chromium Branches",
            branch_selector = branches.NOT_MAIN,
            url = "https://chrome-ops-rotation-proxy.appspot.com/current/oncallator:chrome-branch-sheriff",
        ),
        _oncall(
            name = "Android",
            url = "https://chrome-ops-rotation-proxy.appspot.com/current/oncallator:chrome-android-sheriff",
        ),
        _oncall(
            name = "iOS",
            url = "https://rota-ng.appspot.com/legacy/sheriff_ios.json",
        ),
        _oncall(
            name = "ChromeOS",
            url = "https://chrome-ops-rotation-proxy.appspot.com/current/oncallator:chromeos-gardeners",
        ),
        _oncall(
            name = "GPU",
            url = "https://chrome-ops-rotation-proxy.appspot.com/current/grotation:chrome-gpu-pixel-wrangling",
        ),
        _oncall(
            name = "ANGLE",
            url = "https://chrome-ops-rotation-proxy.appspot.com/current/grotation:angle-wrangler",
        ),
        _oncall(
            name = "Perf",
            url = "https://rota-ng.appspot.com/legacy/sheriff_perf.json",
        ),
        _oncall(
            name = "Perfbot",
            url = "https://rota-ng.appspot.com/legacy/sheriff_perfbot.json",
        ),
        _oncall(
            name = "Trooper",
            branch_selector = branches.ALL_BRANCHES,
            url = "https://chrome-ops-rotation-proxy.appspot.com/current/oncallator:chrome-ops-client-infra",
            show_primary_secondary_labels = True,
        ),
    ],
    link_groups = [
        _link_group(
            name = "Builds",
            links = [
                _link(
                    text = "continuous",
                    branch_selector = branches.ALL_BRANCHES,
                    url = "https://commondatastorage.googleapis.com/chromium-browser-snapshots/index.html",
                    alt = "Continuous browser snapshots",
                ),
                _link(
                    text = "symbols",
                    branch_selector = branches.ALL_BRANCHES,
                    url = "https://www.chromium.org/developers/how-tos/debugging-on-windows",
                    alt = "Windows Symbols",
                ),
                _link(
                    text = "status",
                    url = "https://chromium-status.appspot.com/",
                    alt = "Current tree status",
                ),
            ],
        ),
        _link_group(
            name = "Dashboards",
            links = [
                _link(
                    text = "perf",
                    branch_selector = branches.ALL_BRANCHES,
                    url = "https://chromeperf.appspot.com/",
                    alt = "Chrome perf dashboard",
                ),
                _link(
                    text = "flake-portal",
                    branch_selector = branches.ALL_BRANCHES,
                    url = "https://analysis.chromium.org/p/chromium/flake-portal",
                    alt = "New flake portal",
                ),
                _link(
                    text = "legacy-flakiness",
                    branch_selector = branches.ALL_BRANCHES,
                    url = "https://test-results.appspot.com/dashboards/flakiness_dashboard.html",
                    alt = "Legacy flakiness dashboard",
                ),
            ],
        ),
        _link_group(
            name = "Chromium",
            links = [
                _link(
                    text = "source",
                    branch_selector = branches.ALL_BRANCHES,
                    url = branches.value(
                        for_main = "https://chromium.googlesource.com/chromium/src",
                        for_branches = "https://chromium.googlesource.com/chromium/src/+/{}".format(settings.ref),
                    ),
                    alt = "Chromium source code repository",
                ),
                _link(
                    text = "reviews",
                    branch_selector = branches.ALL_BRANCHES,
                    url = "https://chromium-review.googlesource.com",
                    alt = "Chromium code review tool",
                ),
                _link(
                    text = "bugs",
                    branch_selector = branches.ALL_BRANCHES,
                    url = "https://crbug.com",
                    alt = "Chromium bug tracker",
                ),
                _link(
                    text = "coverage",
                    branch_selector = branches.ALL_BRANCHES,
                    url = "https://analysis.chromium.org/p/chromium/coverage",
                    alt = "Chromium code coverage dashboard",
                ),
                _link(
                    text = "dev",
                    branch_selector = branches.ALL_BRANCHES,
                    url = "https://dev.chromium.org/Home",
                    alt = "Chromium developer home page",
                ),
                _link(
                    text = "support",
                    branch_selector = branches.ALL_BRANCHES,
                    url = "https://support.google.com/chrome/#topic=7438008",
                    alt = "Google Chrome help center",
                ),
            ],
        ),
        _link_group(
            name = "Consoles",
            links = [
                _link(
                    text = "android",
                    branch_selector = branches.ALL_BRANCHES,
                    url = "/p/{}/g/chromium.android".format(settings.project),
                    alt = "Chromium Android console",
                ),
                _link(
                    text = "clang",
                    url = "/p/{}/g/chromium.clang".format(settings.project),
                    alt = "Chromium Clang console",
                ),
                _link(
                    text = "dawn",
                    branch_selector = branches.ALL_BRANCHES,
                    url = "/p/{}/g/chromium.dawn".format(settings.project),
                    alt = "Chromium Dawn console",
                ),
                _link(
                    text = "fuzz",
                    url = "/p/{}/g/chromium.fuzz".format(settings.project),
                    alt = "Chromium Fuzz console",
                ),
                _link(
                    text = "fyi",
                    branch_selector = branches.ALL_BRANCHES,
                    url = "/p/{}/g/chromium.fyi".format(settings.project),
                    alt = "Chromium FYI console",
                ),
                _link(
                    text = "gpu",
                    branch_selector = branches.ALL_BRANCHES,
                    url = "/p/{}/g/chromium.gpu".format(settings.project),
                    alt = "Chromium GPU console",
                ),
                _link(
                    text = "perf",
                    url = "/p/chrome/g/chrome.perf/console",
                    alt = "Chromium Perf console",
                ),
                _link(
                    text = "perf.fyi",
                    url = "/p/chrome/g/chrome.perf.fyi/console",
                    alt = "Chromium Perf FYI console",
                ),
                _link(
                    text = "swangle",
                    url = "/p/{}/g/chromium.swangle".format(settings.project),
                    alt = "Chromium SWANGLE console",
                ),
                _link(
                    text = "webrtc",
                    url = "/p/{}/g/chromium.webrtc".format(settings.project),
                    alt = "Chromium WebRTC console",
                ),
                _link(
                    text = "chromiumos",
                    branch_selector = branches.ALL_BRANCHES,
                    url = "/p/{}/g/chromium.chromiumos".format(settings.project),
                    alt = "ChromiumOS console",
                ),
            ],
        ),
        _link_group(
            name = "Branch Consoles",
            links = [
                _link(
                    text = milestone,
                    url = "/p/{}/g/main/console".format(details.project),
                    alt = "{} branch console".format(details.channel),
                )
                for milestone, details in sorted(ACTIVE_MILESTONES.items())
            ] + [
                _link(
                    text = "trunk",
                    branch_selector = branches.ALL_BRANCHES,
                    url = "/p/chromium/g/main/console",
                    alt = "Trunk (ToT) console",
                ),
            ],
        ),
        _link_group(
            name = "Tryservers",
            links = [
                _link(
                    text = "android",
                    branch_selector = branches.ALL_BRANCHES,
                    url = "/p/{}/g/tryserver.chromium.android/builders".format(settings.project),
                    alt = "Android",
                ),
                _link(
                    text = "angle",
                    url = "/p/{}/g/tryserver.chromium.angle/builders".format(settings.project),
                    alt = "Angle",
                ),
                _link(
                    text = "blink",
                    branch_selector = branches.ALL_BRANCHES,
                    url = "/p/{}/g/tryserver.blink/builders".format(settings.project),
                    alt = "Blink",
                ),
                _link(
                    text = "chrome",
                    url = "/p/chrome/g/tryserver.chrome/builders",
                    alt = "Chrome",
                ),
                _link(
                    text = "chromiumos",
                    branch_selector = branches.ALL_BRANCHES,
                    url = "/p/{}/g/tryserver.chromium.chromiumos/builders".format(settings.project),
                    alt = "ChromiumOS",
                ),
                _link(
                    text = "linux",
                    branch_selector = branches.ALL_BRANCHES,
                    url = "/p/{}/g/tryserver.chromium.linux/builders".format(settings.project),
                    alt = "Linux",
                ),
                _link(
                    text = "mac",
                    branch_selector = branches.ALL_BRANCHES,
                    url = "/p/{}/g/tryserver.chromium.mac/builders".format(settings.project),
                    alt = "Mac",
                ),
                _link(
                    text = "swangle",
                    url = "/p/{}/g/tryserver.chromium.swangle/builders".format(settings.project),
                    alt = "SWANGLE",
                ),
                _link(
                    text = "win",
                    branch_selector = branches.ALL_BRANCHES,
                    url = "/p/{}/g/tryserver.chromium.win/builders".format(settings.project),
                    alt = "Win",
                ),
            ],
        ),
        _link_group(
            name = "Navigate",
            links = [
                _link(
                    text = "about",
                    branch_selector = branches.ALL_BRANCHES,
                    url = "http://dev.chromium.org/developers/testing/chromium-build-infrastructure/tour-of-the-chromium-buildbot",
                    alt = "Tour of the console",
                ),
                _link(
                    text = "customize",
                    branch_selector = branches.ALL_BRANCHES,
                    url = "https://chromium.googlesource.com/chromium/src/+/{}/infra/config/generated/luci-milo.cfg".format(settings.ref),
                    alt = "Customize this console",
                ),
            ],
        ),
    ],
    console_groups = [
        _console_group(
            title = _console_group_title(
                text = "Tree Closers",
                url = "https://chromium-status.appspot.com/",
            ),
            console_ids = [
                "chromium/chromium",
                "chromium/chromium.win",
                "chromium/chromium.mac",
                "chromium/chromium.linux",
                "chromium/chromium.chromiumos",
                "chrome/chrome",
                "chromium/chromium.memory",
                "chromium/chromium.gpu",
            ],
        ),
        _console_group(
            console_ids = [
                "chromium/chromium.android",
                "chrome/chrome.perf",
                "chromium/chromium.gpu.fyi",
                "chromium/chromium.swangle",
                "chromium/chromium.fuzz",
            ],
        ),
        _console_group(
            branch_selector = branches.NOT_MAIN,
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
    ],
    tree_status_host = settings.tree_status_host,
)
