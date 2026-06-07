# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This file is used to list trybots in Chromium's commit-queue.cfg that
# are defined in src-internal. Note that the recipe these builders run have
# certain ACL restrictions. For more info, see
# http://go/chromium-cq#internal-builders-on-the-cq.

load("@chromium-luci//branches.star", "branches")
load("@chromium-luci//try.star", "default_location_filters", "default_owner_whitelist_group_for_cq_bots", "try_")
load("//project.star", "settings")

def chrome_internal_verifier(
        *,
        builder,
        cq_settings = None,
        **kwargs):
    """Registers an internal Chrome trybot in Chromium's CQ config

    Args:
      builder: Name of builder in the internal chrome project.
      cq_settings - A struct containing the details of the tryjob verifier for the
        builder, obtained by calling the `try.cq_settings()` function.
    """
    if cq_settings != None:
        location_filters = cq_settings.location_filters
        if cq_settings.add_default_filters:
            location_filters = (location_filters or []) + default_location_filters()

        branches.cq_tryjob_verifier(
            builder = "{}:try/{}".format(settings.chrome_project, builder),
            cancel_stale = cq_settings.cancel_stale,
            cq_group = "cq",
            disable_reuse = cq_settings.disable_reuse,
            experiment_percentage = cq_settings.experiment_percentage,
            location_filters = location_filters,
            mode_allowlist = cq_settings.custom_cq_run_modes,
            result_visibility = cq.COMMENT_LEVEL_RESTRICTED,
            equivalent_builder = cq_settings.equivalent_builder,
            equivalent_builder_percentage = cq_settings.equivalent_builder_percentage,
            equivalent_builder_whitelist = cq_settings.equivalent_builder_whitelist,
            **kwargs
        )
    else:
        branches.cq_tryjob_verifier(
            builder = "{}:try/{}".format(settings.chrome_project, builder),
            cq_group = "cq",
            includable_only = True,
            owner_whitelist = default_owner_whitelist_group_for_cq_bots(settings.chrome_project),
            result_visibility = cq.COMMENT_LEVEL_RESTRICTED,
            **kwargs
        )

### Mandatory builders ###

chrome_internal_verifier(
    builder = "internal-cq-builder-verifier",
    cq_settings = try_.cq_settings(
        add_default_filters = False,
        location_filters = ["infra/config/generated/cq-usage/full.cfg"],
    ),
)

chrome_internal_verifier(
    builder = "android-internal-desktop-x64-rel",
    cq_settings = try_.cq_settings(
        experiment_percentage = 25,
        on_default_cq = True,
    ),
    owner_whitelist = ["google/chrome-al-eng@google.com"],
)

chrome_internal_verifier(
    builder = "linux-chromeos-compile-chrome",
    cq_settings = try_.cq_settings(
        on_default_cq = True,
    ),
)

chrome_internal_verifier(
    builder = "win-branded-compile-rel",
    cq_settings = try_.cq_settings(
        on_default_cq = True,
    ),
)

chrome_internal_verifier(
    builder = "mega-cq-launcher",
    cq_settings = try_.cq_settings(
        custom_cq_run_modes = [try_.MEGA_CQ_DRY_RUN_NAME, try_.MEGA_CQ_FULL_RUN_NAME],
        on_default_cq = True,
    ),
)

### Optional builders ###

chrome_internal_verifier(
    # TODO(https://crbug.com/400712231): Turn on branches for this bot.
    #branch_selector = branches.selector.ANDROID_BRANCHES,
    builder = "android-arm32-orderfile",
)

chrome_internal_verifier(
    # TODO(https://crbug.com/400712231): Turn on branches for this bot.
    #branch_selector = branches.selector.ANDROID_BRANCHES,
    builder = "android-arm64-orderfile",
)

chrome_internal_verifier(
    branch_selector = branches.selector.ANDROID_BRANCHES,
    builder = "android-arm32-pgo",
)

chrome_internal_verifier(
    branch_selector = branches.selector.ANDROID_BRANCHES,
    builder = "android-arm64-pgo",
)

chrome_internal_verifier(
    # TODO(https://crbug.com/400712231): Turn on branches for this bot.
    #branch_selector = branches.selector.ANDROID_BRANCHES,
    builder = "webview-arm-orderfile",
)

chrome_internal_verifier(
    # TODO(https://crbug.com/400712231): Turn on branches for this bot.
    #branch_selector = branches.selector.ANDROID_BRANCHES,
    builder = "webview-arm64-orderfile",
)

chrome_internal_verifier(
    builder = "android-internal-binary-size",
)

chrome_internal_verifier(
    builder = "android-internal-dbg",
)

chrome_internal_verifier(
    builder = "android-internal-rel",
)

chrome_internal_verifier(
    branch_selector = branches.selector.ANDROID_BRANCHES,
    builder = "android-arm-rel-ready",
)

chrome_internal_verifier(
    branch_selector = branches.selector.ANDROID_BRANCHES,
    builder = "android-x64-rel-ready",
)

chrome_internal_verifier(
    builder = "chromeos-arm64-generic-cfi-thin-lto-chrome",
)

chrome_internal_verifier(
    builder = "chromeos-betty-chrome",
)

chrome_internal_verifier(
    builder = "chromeos-betty-compile-chrome",
    cq_settings = try_.cq_settings(
        equivalent_builder = "chrome:try/chromeos-betty-chrome-noop",
        equivalent_builder_percentage = 100,
        equivalent_builder_whitelist = "googlers",
        on_default_cq = True,
    ),
)

chrome_internal_verifier(
    builder = "chromeos-betty-chrome-gtest",
    cq_settings = try_.cq_settings(
        equivalent_builder = "chrome:try/chromeos-betty-chrome-gtest-and-cqtast",
        equivalent_builder_percentage = 100,
        equivalent_builder_whitelist = "google/chromeos-pa",
        on_default_cq = True,
    ),
    owner_whitelist = ["googlers", "project-chromium-robot-committers"],
)

chrome_internal_verifier(
    builder = "chromeos-betty-chrome-gtest-and-tast",
)

chrome_internal_verifier(
    builder = "chromeos-betty-chrome-dchecks",
)

chrome_internal_verifier(
    builder = "chromeos-betty-cfi-thin-lto-chrome",
)

chrome_internal_verifier(
    builder = "chromeos-brya-chrome",
)

chrome_internal_verifier(
    builder = "chromeos-eve-chrome",
)

chrome_internal_verifier(
    builder = "chromeos-eve-compile-chrome",
)

chrome_internal_verifier(
    builder = "chromeos-jacuzzi-chrome",
)

chrome_internal_verifier(
    builder = "chromeos-jacuzzi-compile-chrome",
)

chrome_internal_verifier(
    builder = "chromeos-octopus-chrome",
)

chrome_internal_verifier(
    builder = "chromeos-octopus-compile-chrome",
)

chrome_internal_verifier(
    builder = "chromeos-trogdor-chrome",
)

chrome_internal_verifier(
    builder = "chromeos-volteer-chrome",
)

chrome_internal_verifier(
    branch_selector = branches.selector.ANDROID_BRANCHES,
    builder = "cronet-arm64-gn2bp-debug",
    cq_settings = try_.cq_settings(
        location_filters = [
            "components/cronet/gn2bp/.+",
        ],
    ),
    # The limited traffic to the location_filters specified below makes this
    # use of owner_whitelist acceptable (see
    # https://crrev.com/c/6429907/4..6/infra/config/subprojects/chrome/try.star#b182).
    owner_whitelist = ["googlers"],
)

chrome_internal_verifier(
    builder = "chromeos-reven-chrome",
)

chrome_internal_verifier(
    builder = "fuchsia-ava-astro",
)

chrome_internal_verifier(
    builder = "fuchsia-ava-nelson",
)

chrome_internal_verifier(
    builder = "fuchsia-ava-sherlock",
)

chrome_internal_verifier(
    builder = "fuchsia-fyi-astro",
)

chrome_internal_verifier(
    builder = "fuchsia-fyi-astro-qemu",
)

chrome_internal_verifier(
    builder = "fuchsia-fyi-nelson",
)

chrome_internal_verifier(
    builder = "fuchsia-fyi-sherlock",
)

chrome_internal_verifier(
    builder = "fuchsia-fyi-sherlock-qemu",
)

chrome_internal_verifier(
    builder = "fuchsia-internal-images-roller",
)

chrome_internal_verifier(
    builder = "fuchsia-smoke-astro",
)

chrome_internal_verifier(
    builder = "fuchsia-smoke-nelson",
)

chrome_internal_verifier(
    builder = "fuchsia-smoke-sherlock",
)

chrome_internal_verifier(
    builder = "fuchsia-smoke-sherlock-roller",
)

chrome_internal_verifier(
    builder = "fuchsia-webgl-astro",
)

chrome_internal_verifier(
    builder = "fuchsia-webgl-astro-qemu",
)

chrome_internal_verifier(
    builder = "fuchsia-webgl-nelson",
)

chrome_internal_verifier(
    builder = "fuchsia-webgl-sherlock",
)

chrome_internal_verifier(
    builder = "fuchsia-webgl-sherlock-qemu",
)

chrome_internal_verifier(
    branch_selector = branches.selector.IOS_BRANCHES,
    builder = "ios-rel-ready",
)

chrome_internal_verifier(
    builder = "ios-simulator",
)

chrome_internal_verifier(
    builder = "ipad-device",
)

chrome_internal_verifier(
    builder = "iphone-device",
)

chrome_internal_verifier(
    builder = "linux-autofill-captured-sites-rel",
)

chrome_internal_verifier(
    branch_selector = branches.selector.LINUX_BRANCHES,
    builder = "linux-chrome",
)

chrome_internal_verifier(
    builder = "linux-cft",
)

chrome_internal_verifier(
    builder = "linux-chromeos-chrome",
)

chrome_internal_verifier(
    builder = "linux-nearby-chrome-fyi",
)

chrome_internal_verifier(
    builder = "linux-password-manager-captured-sites-rel",
)

chrome_internal_verifier(
    builder = "linux-perf-trigger",
    cq_settings = try_.cq_settings(
        # TODO(b/457822464) Keep it running for now and with new devices in Q1
        # 2026. By the end of Q1, we will decide whether remove it or promote
        # it to CQ.
        experiment_percentage = 100,
        on_default_cq = True,
    ),
    # The current whitelist includes:
    #  Googlers: internal users are always welcome
    #  project-chromium-robot-committers: this list includes autoroll bots,
    #       rubber stamper for reverts, etc.
    #       We definitely want to have autoroll bots here because we have no
    #       Perf tests on those sub repos, and we want to catch the regressions
    #       during rollout.
    owner_whitelist = ["googlers", "project-chromium-robot-committers"],
)

chrome_internal_verifier(
    branch_selector = branches.selector.LINUX_BRANCHES,
    builder = "linux-pgo",
)

chrome_internal_verifier(
    branch_selector = branches.selector.LINUX_BRANCHES,
    builder = "linux64-rel-ready",
)

chrome_internal_verifier(
    builder = "mac-arm64-bluebird-rel",
)

chrome_internal_verifier(
    branch_selector = branches.selector.MAC_BRANCHES,
    builder = "mac-chrome",
)

chrome_internal_verifier(
    builder = "mac-cft",
)

chrome_internal_verifier(
    branch_selector = branches.selector.MAC_BRANCHES,
    builder = "mac-arm-pgo",
)

chrome_internal_verifier(
    branch_selector = branches.selector.MAC_BRANCHES,
    builder = "mac-pgo",
)

chrome_internal_verifier(
    branch_selector = branches.selector.MAC_BRANCHES,
    builder = "mac-rel-ready",
)

chrome_internal_verifier(
    builder = "optimization_guide-ios-device",
)

chrome_internal_verifier(
    builder = "optimization_guide-ios-simulator",
)

chrome_internal_verifier(
    builder = "optimization_guide-linux",
    cq_settings = try_.cq_settings(
        location_filters = [
            "chrome/browser/ai/.+",
            "components/optimization_guide/.+",
            "services/on_device_model/.+",
        ],
    ),
    owner_whitelist = [
        "google/optimization-guide-try-opt-in@google.com",
    ],
)

chrome_internal_verifier(
    builder = "optimization_guide-mac-arm64",
    cq_settings = try_.cq_settings(
        location_filters = [
            "chrome/browser/ai/.+",
            "components/optimization_guide/.+",
            "services/on_device_model/.+",
        ],
    ),
    owner_whitelist = [
        "google/optimization-guide-try-opt-in@google.com",
    ],
)

chrome_internal_verifier(
    builder = "optimization_guide-mac-x64",
)

chrome_internal_verifier(
    builder = "optimization_guide-win32",
)

chrome_internal_verifier(
    builder = "optimization_guide-win64",
    cq_settings = try_.cq_settings(
        location_filters = [
            "chrome/browser/ai/.+",
            "components/optimization_guide/.+",
            "services/on_device_model/.+",
        ],
    ),
    owner_whitelist = [
        "google/optimization-guide-try-opt-in@google.com",
    ],
)

chrome_internal_verifier(
    builder = "test-emulator",
)

chrome_internal_verifier(
    builder = "test-tablet",
)

chrome_internal_verifier(
    builder = "win-arm64-bluebird-rel",
)

chrome_internal_verifier(
    branch_selector = branches.selector.WINDOWS_BRANCHES,
    builder = "win-arm64-pgo",
)

chrome_internal_verifier(
    branch_selector = branches.selector.WINDOWS_BRANCHES,
    builder = "win-arm64-rel-ready",
)

chrome_internal_verifier(
    builder = "win-bluebird-rel",
)

chrome_internal_verifier(
    branch_selector = branches.selector.WINDOWS_BRANCHES,
    builder = "win-chrome",
)

chrome_internal_verifier(
    builder = "win-cft",
)

chrome_internal_verifier(
    builder = "win-perf-trigger",
    cq_settings = try_.cq_settings(
        experiment_percentage = 100,
        on_default_cq = True,
    ),
    # The current whitelist includes:
    #  Googlers: internal users are always welcome
    #  project-chromium-robot-committers: this list includes autoroll bots,
    #       rubber stamper for reverts, etc.
    #       We definitely want to have autoroll bots here because we have no
    #       Perf tests on those sub repos, and we want to catch the regressions
    #       during rollout.
    # setting to 50 on new builder for win-11.
    owner_whitelist = ["googlers", "project-chromium-robot-committers"],
)

chrome_internal_verifier(
    branch_selector = branches.selector.WINDOWS_BRANCHES,
    builder = "win-rel-ready",
)

chrome_internal_verifier(
    branch_selector = branches.selector.WINDOWS_BRANCHES,
    builder = "win32-pgo",
)

chrome_internal_verifier(
    branch_selector = branches.selector.WINDOWS_BRANCHES,
    builder = "win64-chrome",
)

chrome_internal_verifier(
    branch_selector = branches.selector.WINDOWS_BRANCHES,
    builder = "win64-pgo",
)

chrome_internal_verifier(
    branch_selector = branches.selector.WINDOWS_BRANCHES,
    builder = "win64-rel-ready",
)
