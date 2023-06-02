# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Definitions of builders in the chromium.clang builder group."""

load("//lib/builders.star", "builders", "os", "reclient", "sheriff_rotations", "xcode")
load("//lib/branches.star", "branches")
load("//lib/ci.star", "ci")
load("//lib/consoles.star", "consoles")

ci.defaults.set(
    executable = ci.DEFAULT_EXECUTABLE,
    builder_group = "chromium.clang",
    pool = ci.DEFAULT_POOL,
    builderless = True,
    cores = 32,
    os = os.LINUX_DEFAULT,
    sheriff_rotations = sheriff_rotations.CHROMIUM_CLANG,
    # Because these run ToT Clang, goma is not used.
    # Naturally the runtime will be ~4-8h on average, depending on config.
    # CFI builds will take even longer - around 11h.
    execution_timeout = 14 * time.hour,
    properties = {
        "perf_dashboard_machine_group": "ChromiumClang",
    },
    service_account = ci.DEFAULT_SERVICE_ACCOUNT,
)

consoles.console_view(
    name = "chromium.clang",
    ordering = {
        None: [
            "ToT Linux",
            "ToT Android",
            "ToT Mac",
            "ToT Windows",
            "ToT Code Coverage",
        ],
        "ToT Linux": consoles.ordering(
            short_names = ["rel", "ofi", "dbg", "asn", "fuz", "msn", "tsn"],
        ),
        "ToT Android": consoles.ordering(short_names = ["rel", "dbg", "x64"]),
        "ToT Mac": consoles.ordering(short_names = ["rel", "ofi", "dbg"]),
        "ToT Windows": consoles.ordering(
            categories = ["x64"],
            short_names = ["rel", "ofi"],
        ),
        "ToT Windows|x64": consoles.ordering(short_names = ["rel"]),
        "CFI|Win": consoles.ordering(short_names = ["x86", "x64"]),
        "iOS": ["public"],
        "iOS|public": consoles.ordering(short_names = ["sim", "dev"]),
    },
)

[branches.console_view_entry(
    console_view = "chromium.clang",
    builder = "chrome:ci/{}".format(name),
    category = category,
    short_name = short_name,
) for name, category, short_name in (
    ("ToTLinuxOfficial", "ToT Linux", "ofi"),
    ("ToTMacOfficial", "ToT Mac", "ofi"),
    ("ToTWinOfficial", "ToT Windows", "ofi"),
    ("ToTWinOfficial64", "ToT Windows|x64", "ofi"),
    ("clang-tot-device", "iOS|internal", "dev"),
)]

def clang_mac_builder(*, name, cores = 24, **kwargs):
    return ci.builder(
        name = name,
        cores = cores,
        os = os.MAC_DEFAULT,
        ssd = True,
        properties = {
            # The Chromium build doesn't need system Xcode, but the ToT clang
            # bots also build clang and llvm and that build does need system
            # Xcode.
            "xcode_build_version": "13a233",
        },
        **kwargs
    )

def clang_tot_linux_builder(short_name, category = "ToT Linux", **kwargs):
    ci.builder(
        console_view_entry = consoles.console_view_entry(
            category = category,
            short_name = short_name,
        ),
        notifies = [luci.notifier(
            name = "ToT Linux notifier",
            notify_emails = ["thomasanderson@chromium.org"],
            on_new_status = ["FAILURE"],
        )],
        **kwargs
    )

ci.builder(
    name = "CFI Linux CF",
    console_view_entry = consoles.console_view_entry(
        category = "CFI|Linux",
        short_name = "CF",
    ),
    notifies = ["CFI Linux"],
    reclient_instance = reclient.instance.DEFAULT_TRUSTED,
    reclient_jobs = reclient.jobs.DEFAULT,
)

ci.builder(
    name = "CFI Linux ToT",
    console_view_entry = consoles.console_view_entry(
        category = "CFI|Linux",
        short_name = "ToT",
    ),
    notifies = ["CFI Linux"],
)

ci.builder(
    name = "CrWinAsan",
    os = os.WINDOWS_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "ToT Windows|Asan",
        short_name = "asn",
    ),
)

ci.builder(
    name = "CrWinAsan(dll)",
    os = os.WINDOWS_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "ToT Windows|Asan",
        short_name = "dll",
    ),
)

ci.builder(
    name = "ToTAndroid",
    console_view_entry = consoles.console_view_entry(
        category = "ToT Android",
        short_name = "rel",
    ),
)

ci.builder(
    name = "ToTAndroid (dbg)",
    console_view_entry = consoles.console_view_entry(
        category = "ToT Android",
        short_name = "dbg",
    ),
)

ci.builder(
    name = "ToTAndroid x64",
    console_view_entry = consoles.console_view_entry(
        category = "ToT Android",
        short_name = "x64",
    ),
)

ci.builder(
    name = "ToTAndroid x86",
    console_view_entry = consoles.console_view_entry(
        category = "ToT Android",
        short_name = "x86",
    ),
)

ci.builder(
    name = "ToTAndroidCoverage x86",
    console_view_entry = consoles.console_view_entry(
        category = "ToT Code Coverage",
        short_name = "and",
    ),
)

ci.builder(
    name = "ToTAndroid64",
    console_view_entry = consoles.console_view_entry(
        category = "ToT Android",
        short_name = "a64",
    ),
)

ci.builder(
    name = "ToTAndroidASan",
    console_view_entry = consoles.console_view_entry(
        category = "ToT Android",
        short_name = "asn",
    ),
)

ci.builder(
    name = "ToTAndroidOfficial",
    console_view_entry = consoles.console_view_entry(
        category = "ToT Android",
        short_name = "off",
    ),
)

ci.builder(
    name = "ToTChromeOS",
    console_view_entry = consoles.console_view_entry(
        category = "ToT ChromeOS",
        short_name = "rel",
    ),
)

ci.builder(
    name = "ToTChromeOS (dbg)",
    console_view_entry = consoles.console_view_entry(
        category = "ToT ChromeOS",
        short_name = "dbg",
    ),
)

ci.builder(
    name = "ToTFuchsia x64",
    console_view_entry = [
        consoles.console_view_entry(
            category = "ToT Fuchsia",
            short_name = "x64",
        ),
        consoles.console_view_entry(
            branch_selector = branches.selector.MAIN,
            console_view = "sheriff.fuchsia",
            category = "fyi|clang",
            short_name = "x64",
        ),
    ],
)

ci.builder(
    name = "ToTFuchsiaOfficial arm64",
    console_view_entry = [
        consoles.console_view_entry(
            category = "ToT Fuchsia",
            short_name = "off",
        ),
        consoles.console_view_entry(
            branch_selector = branches.selector.MAIN,
            console_view = "sheriff.fuchsia",
            category = "fyi|clang",
            short_name = "arm64-off",
        ),
    ],
)

clang_tot_linux_builder(
    name = "ToTLinux",
    short_name = "rel",
)

clang_tot_linux_builder(
    name = "ToTLinux (dbg)",
    short_name = "dbg",
)

clang_tot_linux_builder(
    name = "ToTLinuxASan",
    short_name = "asn",
)

clang_tot_linux_builder(
    name = "ToTLinuxASanLibfuzzer",
    # Requires a large disk, so has a machine specifically devoted to it
    builderless = False,
    short_name = "fuz",
)

clang_tot_linux_builder(
    name = "ToTLinuxCoverage",
    executable = "recipe:chromium_clang_coverage_tot",
    category = "ToT Code Coverage",
    short_name = "linux",
)

clang_tot_linux_builder(
    name = "ToTLinuxMSan",
    os = os.LINUX_FOCAL,
    short_name = "msn",
)

clang_tot_linux_builder(
    name = "ToTLinuxPGO",
    short_name = "pgo",
)

clang_tot_linux_builder(
    name = "ToTLinuxTSan",
    short_name = "tsn",
)

clang_tot_linux_builder(
    name = "ToTLinuxUBSanVptr",
    short_name = "usn",
)

ci.builder(
    name = "ToTWin",
    os = os.WINDOWS_DEFAULT,
    free_space = builders.free_space.high,
    console_view_entry = consoles.console_view_entry(
        category = "ToT Windows",
        short_name = "rel",
    ),
)

ci.builder(
    name = "ToTWin(dbg)",
    builderless = False,
    os = os.WINDOWS_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "ToT Windows",
        short_name = "dbg",
    ),
)

ci.builder(
    name = "ToTWin(dll)",
    os = os.WINDOWS_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "ToT Windows",
        short_name = "dll",
    ),
)

ci.builder(
    name = "ToTWin64",
    os = os.WINDOWS_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "ToT Windows|x64",
        short_name = "rel",
    ),
)

ci.builder(
    name = "ToTWin64(dbg)",
    os = os.WINDOWS_DEFAULT,
    free_space = builders.free_space.high,
    console_view_entry = consoles.console_view_entry(
        category = "ToT Windows|x64",
        short_name = "dbg",
    ),
)

ci.builder(
    name = "ToTWin64(dll)",
    os = os.WINDOWS_DEFAULT,
    free_space = builders.free_space.high,
    console_view_entry = consoles.console_view_entry(
        category = "ToT Windows|x64",
        short_name = "dll",
    ),
)

ci.builder(
    name = "ToTWinASanLibfuzzer",
    builderless = False,
    os = os.WINDOWS_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "ToT Windows|Asan",
        short_name = "fuz",
    ),
)

ci.builder(
    name = "ToTWindowsCoverage",
    executable = "recipe:chromium_clang_coverage_tot",
    os = os.WINDOWS_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "ToT Code Coverage",
        short_name = "win",
    ),
)

ci.builder(
    name = "ToTWin64PGO",
    os = os.WINDOWS_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "ToT Windows|x64",
        short_name = "pgo",
    ),
)

ci.builder(
    name = "linux-win_cross-rel",
    console_view_entry = consoles.console_view_entry(
        category = "ToT Windows",
        short_name = "lxw",
    ),
)

ci.builder(
    name = "ToTiOS",
    builderless = False,
    cores = None,
    os = os.MAC_DEFAULT,
    ssd = True,
    console_view_entry = consoles.console_view_entry(
        category = "iOS|public",
        short_name = "sim",
    ),
    xcode = xcode.x14main,
)

ci.builder(
    name = "ToTiOSDevice",
    builderless = False,
    cores = None,
    os = os.MAC_DEFAULT,
    ssd = True,
    console_view_entry = consoles.console_view_entry(
        category = "iOS|public",
        short_name = "dev",
    ),
    xcode = xcode.x14main,
)

clang_mac_builder(
    name = "ToTMac",
    cores = None,
    console_view_entry = consoles.console_view_entry(
        category = "ToT Mac",
        short_name = "rel",
    ),
    execution_timeout = 20 * time.hour,
)

clang_mac_builder(
    name = "ToTMac (dbg)",
    cores = None,
    console_view_entry = consoles.console_view_entry(
        category = "ToT Mac",
        short_name = "dbg",
    ),
    execution_timeout = 20 * time.hour,
)

clang_mac_builder(
    name = "ToTMacASan",
    cores = None,
    console_view_entry = consoles.console_view_entry(
        category = "ToT Mac",
        short_name = "asn",
    ),
    execution_timeout = 20 * time.hour,
)

clang_mac_builder(
    name = "ToTMacCoverage",
    executable = "recipe:chromium_clang_coverage_tot",
    cores = None,
    console_view_entry = consoles.console_view_entry(
        category = "ToT Code Coverage",
        short_name = "mac",
    ),
)
