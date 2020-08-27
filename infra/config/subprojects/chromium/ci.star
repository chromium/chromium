# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

load("//lib/branches.star", "branches")
load("//lib/builders.star", "builder_name", "goma", "os", "xcode_cache")
load("//lib/ci.star", "ci")
load("//project.star", "settings")

def main_console_if_on_branch():
    return None if settings.is_master else settings.main_console_name

ci.set_defaults(
    settings,
    add_to_console_view = True,
    bucketed_triggers = settings.is_master,
)

ci.declare_bucket(settings, branch_selector = branches.ALL_RELEASES)

# Automatically maintained consoles

ci.console_view(
    name = "chromium",
    branch_selector = branches.STANDARD_RELEASES,
    include_experimental_builds = True,
    ordering = {
        "*type*": ci.ordering(short_names = ["dbg", "rel", "off"]),
        "android": "*type*",
        "fuchsia": "*type*",
        "linux": "*type*",
        "mac": "*type*",
        "win": "*type*",
    },
)

ci.console_view(
    name = "chromium.android",
    branch_selector = branches.STANDARD_RELEASES,
    ordering = {
        None: ["cronet", "builder", "tester"],
        "*cpu*": ["arm", "arm64", "x86"],
        "cronet": "*cpu*",
        "builder": "*cpu*",
        "builder|det": ci.ordering(short_names = ["rel", "dbg"]),
        "tester": ["phone", "tablet"],
        "builder_tester|arm64": ci.ordering(short_names = ["M proguard"]),
    },
)

ci.console_view(
    name = "chromium.android.fyi",
    ordering = {
        None: ["android", "memory", "weblayer", "webview"],
    },
)

ci.console_view(
    name = "chromium.chromiumos",
    branch_selector = branches.STANDARD_RELEASES,
    ordering = {
        None: ["default"],
        "default": ci.ordering(short_names = ["ful", "rel"]),
        "simple": ["release", "debug"],
    },
)

ci.console_view(
    name = "chromium.clang",
    ordering = {
        None: [
            "ToT Linux",
            "ToT Android",
            "ToT Mac",
            "ToT Windows",
            "ToT Code Coverage",
        ],
        "ToT Linux": ci.ordering(
            short_names = ["rel", "ofi", "dbg", "asn", "fuz", "msn", "tsn"],
        ),
        "ToT Android": ci.ordering(short_names = ["rel", "dbg", "x64"]),
        "ToT Mac": ci.ordering(short_names = ["rel", "ofi", "dbg"]),
        "ToT Windows": ci.ordering(
            short_names = ["rel", "ofi"],
            categories = ["x64"],
        ),
        "ToT Windows|x64": ci.ordering(short_names = ["rel"]),
        "CFI|Win": ci.ordering(short_names = ["x86", "x64"]),
        "iOS": ["public"],
        "iOS|public": ci.ordering(short_names = ["sim", "dev"]),
    },
)

ci.console_view(
    name = "chromium.dawn",
    branch_selector = branches.STANDARD_RELEASES,
    ordering = {
        None: ["ToT"],
        "*builder*": ["Builder"],
        "*cpu*": ci.ordering(short_names = ["x86"]),
        "ToT|Mac": "*builder*",
        "ToT|Windows|Builder": "*cpu*",
        "ToT|Windows|Intel": "*cpu*",
        "ToT|Windows|Nvidia": "*cpu*",
        "DEPS|Mac": "*builder*",
        "DEPS|Windows|Builder": "*cpu*",
        "DEPS|Windows|Intel": "*cpu*",
        "DEPS|Windows|Nvidia": "*cpu*",
    },
)

ci.console_view(
    name = "chromium.fyi",
    branch_selector = branches.STANDARD_RELEASES,
    ordering = {
        None: [
            "closure_compilation",
            "code_coverage",
            "cronet",
            "mac",
            "deterministic",
            "fuchsia",
            "chromeos",
            "iOS",
            "linux",
            "mojo",
            "recipe",
            "remote_run",
            "site_isolation",
            "network",
            "viz",
            "win10",
            "win32",
        ],
        "code_coverage": ci.ordering(
            short_names = ["and", "ann", "lnx", "lcr", "mac"],
        ),
        "mac": ci.ordering(short_names = ["bld", "15", "herm"]),
        "deterministic|mac": ci.ordering(short_names = ["rel", "dbg"]),
        "iOS|iOS13": ci.ordering(short_names = ["dev", "sim"]),
        "linux|blink": ci.ordering(short_names = ["TD"]),
    },
)

ci.console_view(
    name = "chromium.fuzz",
    ordering = {
        None: [
            "afl",
            "win asan",
            "mac asan",
            "cros asan",
            "linux asan",
            "libfuzz",
            "linux msan",
            "linux tsan",
        ],
        "*config*": ci.ordering(short_names = ["dbg", "rel"]),
        "win asan": "*config*",
        "mac asan": "*config*",
        "linux asan": "*config*",
        "linux asan|x64 v8-ARM": "*config*",
        "libfuzz": ci.ordering(short_names = [
            "chromeos-asan",
            "linux32",
            "linux32-dbg",
            "linux",
            "linux-dbg",
            "linux-msan",
            "linux-ubsan",
            "mac-asan",
            "win-asan",
        ]),
    },
)

ci.console_view(
    name = "chromium.gpu",
    branch_selector = branches.STANDARD_RELEASES,
    ordering = {
        None: ["Windows", "Mac", "Linux"],
    },
)

ci.console_view(
    name = "chromium.gpu.fyi",
    ordering = {
        None: ["Windows", "Mac", "Linux"],
        "*builder*": ["Builder"],
        "*type*": ci.ordering(short_names = ["rel", "dbg", "exp"]),
        "*cpu*": ci.ordering(short_names = ["x86"]),
        "Windows": "*builder*",
        "Windows|Builder": ["Release", "dEQP", "dx12vk", "Debug"],
        "Windows|Builder|Release": "*cpu*",
        "Windows|Builder|dEQP": "*cpu*",
        "Windows|Builder|dx12vk": "*type*",
        "Windows|Builder|Debug": "*cpu*",
        "Windows|10|x64|Intel": "*type*",
        "Windows|10|x64|Nvidia": "*type*",
        "Windows|10|x86|Nvidia": "*type*",
        "Windows|7|x64|Nvidia": "*type*",
        "Mac": "*builder*",
        "Mac|Builder": "*type*",
        "Mac|AMD|Retina": "*type*",
        "Mac|Intel": "*type*",
        "Mac|Nvidia": "*type*",
        "Linux": "*builder*",
        "Linux|Builder": "*type*",
        "Linux|Intel": "*type*",
        "Linux|Nvidia": "*type*",
        "Android": ["L32", "M64", "N64", "P32", "vk", "dqp", "skgl", "skv"],
        "Android|M64": ["QCOM"],
    },
)

ci.console_view(
    name = "chromium.linux",
    branch_selector = branches.STANDARD_RELEASES,
    ordering = {
        None: ["release", "debug"],
        "release": ci.ordering(short_names = ["bld", "tst", "nsl", "gcc"]),
        "cast": ci.ordering(short_names = ["vid", "aud"]),
    },
)

ci.console_view(
    name = "chromium.mac",
    branch_selector = branches.STANDARD_RELEASES,
    ordering = {
        None: ["release"],
        "release": ci.ordering(short_names = ["bld"]),
        "debug": ci.ordering(short_names = ["bld"]),
        "ios|default": ci.ordering(short_names = ["dev", "sim"]),
    },
)

ci.console_view(
    name = "chromium.memory",
    branch_selector = branches.STANDARD_RELEASES,
    ordering = {
        None: ["win", "mac", "linux", "cros"],
        "*build-or-test*": ci.ordering(short_names = ["bld", "tst"]),
        "linux|TSan v2": "*build-or-test*",
        "linux|asan lsan": "*build-or-test*",
        "linux|webkit": ci.ordering(short_names = ["asn", "msn"]),
    },
)

ci.console_view(
    name = "chromium.swangle",
    ordering = {
        None: ["DEPS", "ToT ANGLE", "ToT SwiftShader"],
        "*os*": ["Windows", "Mac"],
        "*cpu*": ci.ordering(short_names = ["x86", "x64"]),
        "DEPS": "*os*",
        "DEPS|Windows": "*cpu*",
        "DEPS|Linux": "*cpu*",
        "ToT ANGLE": "*os*",
        "ToT ANGLE|Windows": "*cpu*",
        "ToT ANGLE|Linux": "*cpu*",
        "ToT SwiftShader": "*os*",
        "ToT SwiftShader|Windows": "*cpu*",
        "ToT SwiftShader|Linux": "*cpu*",
        "Chromium": "*os*",
    },
)

ci.console_view(
    name = "chromium.win",
    branch_selector = branches.STANDARD_RELEASES,
    ordering = {
        None: ["release", "debug"],
        "debug|builder": ci.ordering(short_names = ["64", "32"]),
        "debug|tester": ci.ordering(short_names = ["7", "10"]),
    },
)

# Builders are sorted first lexicographically by the function used to define
# them, then lexicographically by their name

ci.android_builder(
    name = "Android WebView M (dbg)",
    branch_selector = branches.STANDARD_RELEASES,
    console_view_entry = ci.console_view_entry(
        category = "tester|webview",
        short_name = "M",
    ),
    cq_mirrors_console_view = settings.cq_mirrors_console_name,
    main_console_view = main_console_if_on_branch(),
    triggered_by = [builder_name("Android arm64 Builder (dbg)")],
)

ci.android_builder(
    name = "Android WebView N (dbg)",
    branch_selector = branches.STANDARD_RELEASES,
    console_view_entry = ci.console_view_entry(
        category = "tester|webview",
        short_name = "N",
    ),
    cq_mirrors_console_view = settings.cq_mirrors_console_name,
    main_console_view = main_console_if_on_branch(),
    triggered_by = [builder_name("Android arm64 Builder (dbg)")],
)

ci.android_builder(
    name = "Android WebView O (dbg)",
    branch_selector = branches.STANDARD_RELEASES,
    console_view_entry = ci.console_view_entry(
        category = "tester|webview",
        short_name = "O",
    ),
    cq_mirrors_console_view = settings.cq_mirrors_console_name,
    main_console_view = main_console_if_on_branch(),
    triggered_by = [builder_name("Android arm64 Builder (dbg)")],
)

ci.android_builder(
    name = "Android WebView P (dbg)",
    branch_selector = branches.STANDARD_RELEASES,
    console_view_entry = ci.console_view_entry(
        category = "tester|webview",
        short_name = "P",
    ),
    cq_mirrors_console_view = settings.cq_mirrors_console_name,
    main_console_view = main_console_if_on_branch(),
    triggered_by = [builder_name("Android arm64 Builder (dbg)")],
)

ci.android_builder(
    name = "Android arm Builder (dbg)",
    branch_selector = branches.STANDARD_RELEASES,
    console_view_entry = ci.console_view_entry(
        category = "builder|arm",
        short_name = "32",
    ),
    cq_mirrors_console_view = settings.cq_mirrors_console_name,
    execution_timeout = 4 * time.hour,
    main_console_view = main_console_if_on_branch(),
    tree_closing = True,
)

ci.android_builder(
    name = "Android arm64 Builder (dbg)",
    branch_selector = branches.STANDARD_RELEASES,
    console_view_entry = ci.console_view_entry(
        category = "builder|arm",
        short_name = "64",
    ),
    cq_mirrors_console_view = settings.cq_mirrors_console_name,
    goma_jobs = goma.jobs.MANY_JOBS_FOR_CI,
    execution_timeout = 5 * time.hour,
    main_console_view = main_console_if_on_branch(),
    tree_closing = True,
)

ci.android_builder(
    name = "Android x64 Builder (dbg)",
    branch_selector = branches.STANDARD_RELEASES,
    console_view_entry = ci.console_view_entry(
        category = "builder|x86",
        short_name = "64",
    ),
    cq_mirrors_console_view = settings.cq_mirrors_console_name,
    execution_timeout = 5 * time.hour,
    main_console_view = main_console_if_on_branch(),
)

ci.android_builder(
    name = "Android x86 Builder (dbg)",
    branch_selector = branches.STANDARD_RELEASES,
    console_view_entry = ci.console_view_entry(
        category = "builder|x86",
        short_name = "32",
    ),
    cq_mirrors_console_view = settings.cq_mirrors_console_name,
    execution_timeout = 4 * time.hour,
    main_console_view = main_console_if_on_branch(),
)

ci.android_builder(
    name = "Cast Android (dbg)",
    branch_selector = branches.STANDARD_RELEASES,
    console_view_entry = ci.console_view_entry(
        category = "on_cq",
        short_name = "cst",
    ),
    cq_mirrors_console_view = settings.cq_mirrors_console_name,
    main_console_view = main_console_if_on_branch(),
    tree_closing = True,
)

ci.android_builder(
    name = "Marshmallow 64 bit Tester",
    branch_selector = branches.STANDARD_RELEASES,
    console_view_entry = ci.console_view_entry(
        category = "tester|phone",
        short_name = "M",
    ),
    cq_mirrors_console_view = settings.cq_mirrors_console_name,
    main_console_view = main_console_if_on_branch(),
    triggered_by = [builder_name("Android arm64 Builder (dbg)")],
)

ci.android_builder(
    name = "Nougat Phone Tester",
    branch_selector = branches.STANDARD_RELEASES,
    console_view_entry = ci.console_view_entry(
        category = "tester|phone",
        short_name = "N",
    ),
    cq_mirrors_console_view = settings.cq_mirrors_console_name,
    main_console_view = main_console_if_on_branch(),
    triggered_by = [builder_name("Android arm64 Builder (dbg)")],
)

ci.android_builder(
    name = "Oreo Phone Tester",
    branch_selector = branches.STANDARD_RELEASES,
    console_view_entry = ci.console_view_entry(
        category = "tester|phone",
        short_name = "O",
    ),
    cq_mirrors_console_view = settings.cq_mirrors_console_name,
    main_console_view = main_console_if_on_branch(),
    triggered_by = [builder_name("Android arm64 Builder (dbg)")],
)

ci.android_builder(
    name = "android-cronet-arm-dbg",
    branch_selector = branches.STANDARD_RELEASES,
    console_view_entry = ci.console_view_entry(
        category = "cronet|arm",
        short_name = "dbg",
    ),
    cq_mirrors_console_view = settings.cq_mirrors_console_name,
    main_console_view = main_console_if_on_branch(),
    notifies = ["cronet"],
)

ci.android_builder(
    name = "android-cronet-arm-rel",
    branch_selector = branches.STANDARD_RELEASES,
    console_view_entry = ci.console_view_entry(
        category = "cronet|arm",
        short_name = "rel",
    ),
    cq_mirrors_console_view = settings.cq_mirrors_console_name,
    main_console_view = main_console_if_on_branch(),
    notifies = ["cronet"],
)

ci.android_builder(
    name = "android-cronet-kitkat-arm-rel",
    branch_selector = branches.STANDARD_RELEASES,
    console_view_entry = ci.console_view_entry(
        category = "cronet|test",
        short_name = "k",
    ),
    cq_mirrors_console_view = settings.cq_mirrors_console_name,
    main_console_view = main_console_if_on_branch(),
    notifies = ["cronet"],
    triggered_by = [builder_name("android-cronet-arm-rel")],
)

ci.android_builder(
    name = "android-cronet-lollipop-arm-rel",
    branch_selector = branches.STANDARD_RELEASES,
    console_view_entry = ci.console_view_entry(
        category = "cronet|test",
        short_name = "l",
    ),
    cq_mirrors_console_view = settings.cq_mirrors_console_name,
    main_console_view = main_console_if_on_branch(),
    notifies = ["cronet"],
    triggered_by = [builder_name("android-cronet-arm-rel")],
)

ci.android_builder(
    name = "android-lollipop-arm-rel",
    branch_selector = branches.STANDARD_RELEASES,
    console_view_entry = ci.console_view_entry(
        category = "on_cq",
        short_name = "L",
    ),
    cq_mirrors_console_view = settings.cq_mirrors_console_name,
    main_console_view = main_console_if_on_branch(),
    tree_closing = True,
)

ci.android_builder(
    name = "android-marshmallow-arm64-rel",
    branch_selector = branches.STANDARD_RELEASES,
    console_view_entry = ci.console_view_entry(
        category = "on_cq",
        short_name = "M",
    ),
    cq_mirrors_console_view = settings.cq_mirrors_console_name,
    main_console_view = main_console_if_on_branch(),
    tree_closing = True,
)

ci.android_builder(
    name = "android-marshmallow-x86-rel",
    branch_selector = branches.STANDARD_RELEASES,
    console_view_entry = ci.console_view_entry(
        category = "builder_tester|x86",
        short_name = "M",
    ),
    cq_mirrors_console_view = settings.cq_mirrors_console_name,
    main_console_view = main_console_if_on_branch(),
)

ci.android_builder(
    name = "android-nougat-arm64-rel",
    branch_selector = branches.STANDARD_RELEASES,
    console_view_entry = ci.console_view_entry(
        category = "builder_tester|arm64",
        short_name = "N",
    ),
    cq_mirrors_console_view = settings.cq_mirrors_console_name,
    main_console_view = main_console_if_on_branch(),
)

ci.android_builder(
    name = "android-pie-arm64-dbg",
    branch_selector = branches.STANDARD_RELEASES,
    console_view_entry = ci.console_view_entry(
        category = "tester|phone",
        short_name = "P",
    ),
    cq_mirrors_console_view = settings.cq_mirrors_console_name,
    main_console_view = main_console_if_on_branch(),
    triggered_by = [builder_name("Android arm64 Builder (dbg)")],
)

ci.android_builder(
    name = "android-pie-arm64-rel",
    branch_selector = branches.STANDARD_RELEASES,
    console_view_entry = ci.console_view_entry(
        category = "on_cq",
        short_name = "P",
    ),
    cq_mirrors_console_view = settings.cq_mirrors_console_name,
    main_console_view = main_console_if_on_branch(),
    tree_closing = True,
)

ci.chromium_builder(
    name = "android-official",
    branch_selector = branches.STANDARD_RELEASES,
    main_console_view = settings.main_console_name,
    console_view_entry = ci.console_view_entry(
        category = "android",
        short_name = "off",
    ),
    cores = 32,
    tree_closing = False,
)

ci.chromium_builder(
    name = "fuchsia-official",
    branch_selector = branches.STANDARD_RELEASES,
    main_console_view = settings.main_console_name,
    console_view_entry = ci.console_view_entry(
        category = "fuchsia",
        short_name = "off",
    ),
    cores = 32,
    # TODO: Change this back down to something reasonable once these builders
    # have populated their cached by getting through the compile step
    execution_timeout = 10 * time.hour,
    tree_closing = False,
)

ci.chromium_builder(
    name = "linux-official",
    branch_selector = branches.STANDARD_RELEASES,
    builderless = False,
    # TODO(https://crbug.com/1072012) Use the default console view and add
    # main_console_view = settings.main_console_name once the build is green
    console_view = "chromium.fyi",
    console_view_entry = ci.console_view_entry(
        category = "linux",
        short_name = "off",
    ),
    cores = 32,
    # TODO: Change this back down to something reasonable once these builders
    # have populated their cached by getting through the compile step
    execution_timeout = 10 * time.hour,
    main_console_view = main_console_if_on_branch(),
    tree_closing = False,
)

ci.chromium_builder(
    name = "win-official",
    branch_selector = branches.STANDARD_RELEASES,
    main_console_view = settings.main_console_name,
    console_view_entry = ci.console_view_entry(
        category = "win|off",
        short_name = "64",
    ),
    cores = 32,
    os = os.WINDOWS_DEFAULT,
    execution_timeout = 6 * time.hour,
    tree_closing = False,
)

ci.chromium_builder(
    name = "win32-official",
    branch_selector = branches.STANDARD_RELEASES,
    main_console_view = settings.main_console_name,
    console_view_entry = ci.console_view_entry(
        category = "win|off",
        short_name = "32",
    ),
    cores = 32,
    os = os.WINDOWS_DEFAULT,
    execution_timeout = 6 * time.hour,
    tree_closing = False,
)

ci.chromiumos_builder(
    name = "chromeos-amd64-generic-dbg",
    branch_selector = branches.STANDARD_RELEASES,
    console_view_entry = ci.console_view_entry(
        category = "simple|debug|x64",
        short_name = "dbg",
    ),
    cq_mirrors_console_view = settings.cq_mirrors_console_name,
    main_console_view = settings.main_console_name,
)

ci.chromiumos_builder(
    name = "chromeos-amd64-generic-rel",
    branch_selector = branches.STANDARD_RELEASES,
    console_view_entry = ci.console_view_entry(
        category = "simple|release|x64",
        short_name = "rel",
    ),
    cq_mirrors_console_view = settings.cq_mirrors_console_name,
    main_console_view = settings.main_console_name,
)

ci.chromiumos_builder(
    name = "chromeos-arm-generic-rel",
    branch_selector = branches.STANDARD_RELEASES,
    console_view_entry = ci.console_view_entry(
        category = "simple|release",
        short_name = "arm",
    ),
    cq_mirrors_console_view = settings.cq_mirrors_console_name,
    main_console_view = settings.main_console_name,
)

ci.chromiumos_builder(
    name = "linux-chromeos-dbg",
    branch_selector = branches.STANDARD_RELEASES,
    console_view_entry = ci.console_view_entry(
        category = "default",
        short_name = "dbg",
    ),
    cq_mirrors_console_view = settings.cq_mirrors_console_name,
    main_console_view = settings.main_console_name,
)

ci.chromiumos_builder(
    name = "linux-chromeos-rel",
    branch_selector = branches.STANDARD_RELEASES,
    console_view_entry = ci.console_view_entry(
        category = "default",
        short_name = "rel",
    ),
    cq_mirrors_console_view = settings.cq_mirrors_console_name,
    main_console_view = settings.main_console_name,
)

ci.chromiumos_builder(
    name = "linux-lacros-builder-rel",
    branch_selector = branches.STANDARD_RELEASES,
    console_view_entry = ci.console_view_entry(
        category = "default",
        short_name = "lcr",
    ),
    cq_mirrors_console_view = settings.cq_mirrors_console_name,
    main_console_view = settings.main_console_name,
    # TODO(crbug.com/1104291): Enable tree closing.
    tree_closing = False,
)

ci.chromiumos_builder(
    name = "linux-lacros-tester-rel",
    branch_selector = branches.STANDARD_RELEASES,
    console_view_entry = ci.console_view_entry(
        category = "default",
        short_name = "lcr",
    ),
    main_console_view = settings.main_console_name,
    cq_mirrors_console_view = settings.cq_mirrors_console_name,
    triggered_by = ["linux-lacros-builder-rel"],
    # TODO(crbug.com/1104291): Enable tree closing.
    tree_closing = False,
)

ci.dawn_builder(
    name = "Dawn Linux x64 DEPS Builder",
    branch_selector = branches.STANDARD_RELEASES,
    console_view_entry = ci.console_view_entry(
        category = "DEPS|Linux|Builder",
        short_name = "x64",
    ),
    cq_mirrors_console_view = settings.cq_mirrors_console_name,
    main_console_view = main_console_if_on_branch(),
)

ci.dawn_builder(
    name = "Dawn Linux x64 DEPS Release (Intel HD 630)",
    branch_selector = branches.STANDARD_RELEASES,
    console_view_entry = ci.console_view_entry(
        category = "DEPS|Linux|Intel",
        short_name = "x64",
    ),
    cores = 2,
    cq_mirrors_console_view = settings.cq_mirrors_console_name,
    main_console_view = main_console_if_on_branch(),
    os = os.LINUX_DEFAULT,
    triggered_by = [builder_name("Dawn Linux x64 DEPS Builder")],
)

ci.dawn_builder(
    name = "Dawn Linux x64 DEPS Release (NVIDIA)",
    branch_selector = branches.STANDARD_RELEASES,
    console_view_entry = ci.console_view_entry(
        category = "DEPS|Linux|Nvidia",
        short_name = "x64",
    ),
    cores = 2,
    cq_mirrors_console_view = settings.cq_mirrors_console_name,
    main_console_view = main_console_if_on_branch(),
    os = os.LINUX_DEFAULT,
    triggered_by = [builder_name("Dawn Linux x64 DEPS Builder")],
)

ci.dawn_builder(
    name = "Dawn Mac x64 DEPS Builder",
    branch_selector = branches.STANDARD_RELEASES,
    builderless = False,
    console_view_entry = ci.console_view_entry(
        category = "DEPS|Mac|Builder",
        short_name = "x64",
    ),
    cores = None,
    cq_mirrors_console_view = settings.cq_mirrors_console_name,
    main_console_view = main_console_if_on_branch(),
    os = os.MAC_ANY,
)

# Note that the Mac testers are all thin Linux VMs, triggering jobs on the
# physical Mac hardware in the Swarming pool which is why they run on linux
ci.dawn_builder(
    name = "Dawn Mac x64 DEPS Release (AMD)",
    branch_selector = branches.STANDARD_RELEASES,
    console_view_entry = ci.console_view_entry(
        category = "DEPS|Mac|AMD",
        short_name = "x64",
    ),
    cores = 2,
    cq_mirrors_console_view = settings.cq_mirrors_console_name,
    main_console_view = main_console_if_on_branch(),
    os = os.LINUX_DEFAULT,
    triggered_by = [builder_name("Dawn Mac x64 DEPS Builder")],
)

ci.dawn_builder(
    name = "Dawn Mac x64 DEPS Release (Intel)",
    branch_selector = branches.STANDARD_RELEASES,
    console_view_entry = ci.console_view_entry(
        category = "DEPS|Mac|Intel",
        short_name = "x64",
    ),
    cores = 2,
    cq_mirrors_console_view = settings.cq_mirrors_console_name,
    main_console_view = main_console_if_on_branch(),
    os = os.LINUX_DEFAULT,
    triggered_by = [builder_name("Dawn Mac x64 DEPS Builder")],
)

ci.dawn_builder(
    name = "Dawn Win10 x64 DEPS Builder",
    branch_selector = branches.STANDARD_RELEASES,
    console_view_entry = ci.console_view_entry(
        category = "DEPS|Windows|Builder",
        short_name = "x64",
    ),
    cq_mirrors_console_view = settings.cq_mirrors_console_name,
    main_console_view = main_console_if_on_branch(),
    os = os.WINDOWS_ANY,
)

ci.dawn_builder(
    name = "Dawn Win10 x64 DEPS Release (Intel HD 630)",
    branch_selector = branches.STANDARD_RELEASES,
    console_view_entry = ci.console_view_entry(
        category = "DEPS|Windows|Intel",
        short_name = "x64",
    ),
    cores = 2,
    cq_mirrors_console_view = settings.cq_mirrors_console_name,
    main_console_view = main_console_if_on_branch(),
    os = os.LINUX_DEFAULT,
    triggered_by = [builder_name("Dawn Win10 x64 DEPS Builder")],
)

ci.dawn_builder(
    name = "Dawn Win10 x64 DEPS Release (NVIDIA)",
    branch_selector = branches.STANDARD_RELEASES,
    console_view_entry = ci.console_view_entry(
        category = "DEPS|Windows|Nvidia",
        short_name = "x64",
    ),
    cores = 2,
    cq_mirrors_console_view = settings.cq_mirrors_console_name,
    main_console_view = main_console_if_on_branch(),
    os = os.LINUX_DEFAULT,
    triggered_by = [builder_name("Dawn Win10 x64 DEPS Builder")],
)

ci.dawn_builder(
    name = "Dawn Win10 x86 DEPS Builder",
    branch_selector = branches.STANDARD_RELEASES,
    console_view_entry = ci.console_view_entry(
        category = "DEPS|Windows|Builder",
        short_name = "x86",
    ),
    cq_mirrors_console_view = settings.cq_mirrors_console_name,
    main_console_view = main_console_if_on_branch(),
    os = os.WINDOWS_ANY,
)

ci.dawn_builder(
    name = "Dawn Win10 x86 DEPS Release (Intel HD 630)",
    branch_selector = branches.STANDARD_RELEASES,
    console_view_entry = ci.console_view_entry(
        category = "DEPS|Windows|Intel",
        short_name = "x86",
    ),
    cores = 2,
    cq_mirrors_console_view = settings.cq_mirrors_console_name,
    main_console_view = main_console_if_on_branch(),
    os = os.LINUX_DEFAULT,
    triggered_by = [builder_name("Dawn Win10 x86 DEPS Builder")],
)

ci.dawn_builder(
    name = "Dawn Win10 x86 DEPS Release (NVIDIA)",
    branch_selector = branches.STANDARD_RELEASES,
    console_view_entry = ci.console_view_entry(
        category = "DEPS|Windows|Nvidia",
        short_name = "x86",
    ),
    cores = 2,
    cq_mirrors_console_view = settings.cq_mirrors_console_name,
    main_console_view = main_console_if_on_branch(),
    os = os.LINUX_DEFAULT,
    triggered_by = [builder_name("Dawn Win10 x86 DEPS Builder")],
)

ci.fyi_builder(
    name = "VR Linux",
    branch_selector = branches.STANDARD_RELEASES,
    console_view_entry = ci.console_view_entry(
        category = "linux",
    ),
    cq_mirrors_console_view = settings.cq_mirrors_console_name,
    main_console_view = main_console_if_on_branch(),
)

ci.fyi_ios_builder(
    name = "ios-simulator-cronet",
    branch_selector = branches.STANDARD_RELEASES,
    caches = [xcode_cache.x11e146],
    console_view_entry = ci.console_view_entry(
        category = "cronet",
    ),
    cq_mirrors_console_view = settings.cq_mirrors_console_name,
    executable = "recipe:chromium",
    main_console_view = main_console_if_on_branch(),
    notifies = ["cronet"],
    properties = {
        "xcode_build_version": "11e146",
    },
)

ci.gpu_builder(
    name = "Android Release (Nexus 5X)",
    branch_selector = branches.STANDARD_RELEASES,
    console_view_entry = ci.console_view_entry(
        category = "Android",
    ),
    cq_mirrors_console_view = settings.cq_mirrors_console_name,
    main_console_view = main_console_if_on_branch(),
)

ci.gpu_builder(
    name = "GPU Linux Builder",
    branch_selector = branches.STANDARD_RELEASES,
    # TODO(https://crbug.com/1109276) Once support for mastername is removed, do
    # not explicitly set
    builder_group = "chromium.gpu",
    console_view_entry = ci.console_view_entry(
        category = "Linux",
    ),
    cq_mirrors_console_view = settings.cq_mirrors_console_name,
    main_console_view = main_console_if_on_branch(),
)

ci.gpu_builder(
    name = "GPU Mac Builder",
    branch_selector = branches.STANDARD_RELEASES,
    console_view_entry = ci.console_view_entry(
        category = "Mac",
    ),
    cores = None,
    cq_mirrors_console_view = settings.cq_mirrors_console_name,
    main_console_view = main_console_if_on_branch(),
    os = os.MAC_ANY,
)

ci.gpu_builder(
    name = "GPU Win x64 Builder",
    branch_selector = branches.STANDARD_RELEASES,
    builderless = True,
    console_view_entry = ci.console_view_entry(
        category = "Windows",
    ),
    cq_mirrors_console_view = settings.cq_mirrors_console_name,
    main_console_view = main_console_if_on_branch(),
    os = os.WINDOWS_ANY,
)

ci.gpu_thin_tester(
    name = "Linux Release (NVIDIA)",
    branch_selector = branches.STANDARD_RELEASES,
    # TODO(https://crbug.com/1109276) Once support for mastername is removed, do
    # not explicitly set
    builder_group = "chromium.gpu",
    cq_mirrors_console_view = settings.cq_mirrors_console_name,
    console_view_entry = ci.console_view_entry(
        category = "Linux",
    ),
    main_console_view = main_console_if_on_branch(),
    triggered_by = [builder_name("GPU Linux Builder")],
)

ci.gpu_thin_tester(
    name = "Mac Release (Intel)",
    branch_selector = branches.STANDARD_RELEASES,
    console_view_entry = ci.console_view_entry(
        category = "Mac",
    ),
    cq_mirrors_console_view = settings.cq_mirrors_console_name,
    main_console_view = main_console_if_on_branch(),
    triggered_by = [builder_name("GPU Mac Builder")],
)

ci.gpu_thin_tester(
    name = "Mac Retina Release (AMD)",
    branch_selector = branches.STANDARD_RELEASES,
    console_view_entry = ci.console_view_entry(
        category = "Mac",
    ),
    cq_mirrors_console_view = settings.cq_mirrors_console_name,
    main_console_view = main_console_if_on_branch(),
    triggered_by = [builder_name("GPU Mac Builder")],
)

ci.gpu_thin_tester(
    name = "Win10 x64 Release (NVIDIA)",
    branch_selector = branches.STANDARD_RELEASES,
    console_view_entry = ci.console_view_entry(
        category = "Windows",
    ),
    cq_mirrors_console_view = settings.cq_mirrors_console_name,
    main_console_view = main_console_if_on_branch(),
    triggered_by = [builder_name("GPU Win x64 Builder")],
)

ci.linux_builder(
    name = "Cast Linux",
    branch_selector = branches.STANDARD_RELEASES,
    console_view_entry = ci.console_view_entry(
        category = "cast",
        short_name = "vid",
    ),
    cq_mirrors_console_view = settings.cq_mirrors_console_name,
    goma_jobs = goma.jobs.J50,
    main_console_view = settings.main_console_name,
)

ci.linux_builder(
    name = "Fuchsia ARM64",
    branch_selector = branches.STANDARD_RELEASES,
    console_view_entry = ci.console_view_entry(
        category = "fuchsia|a64",
        short_name = "rel",
    ),
    cq_mirrors_console_view = settings.cq_mirrors_console_name,
    main_console_view = settings.main_console_name,
    extra_notifies = ["cr-fuchsia"],
)

ci.linux_builder(
    name = "Fuchsia x64",
    branch_selector = branches.STANDARD_RELEASES,
    console_view_entry = ci.console_view_entry(
        category = "fuchsia|x64",
        short_name = "rel",
    ),
    cq_mirrors_console_view = settings.cq_mirrors_console_name,
    main_console_view = settings.main_console_name,
    extra_notifies = ["cr-fuchsia"],
)

ci.linux_builder(
    name = "Linux Builder",
    branch_selector = branches.STANDARD_RELEASES,
    # TODO(https://crbug.com/1109276) Once support for mastername is removed, do
    # not explicitly set
    builder_group = "chromium.linux",
    console_view_entry = ci.console_view_entry(
        category = "release",
        short_name = "bld",
    ),
    cq_mirrors_console_view = settings.cq_mirrors_console_name,
    main_console_view = settings.main_console_name,
)

ci.linux_builder(
    name = "Linux Builder (dbg)",
    branch_selector = branches.STANDARD_RELEASES,
    console_view_entry = ci.console_view_entry(
        category = "debug|builder",
        short_name = "64",
    ),
    cq_mirrors_console_view = settings.cq_mirrors_console_name,
    main_console_view = settings.main_console_name,
)

ci.linux_builder(
    name = "Linux Tests",
    branch_selector = branches.STANDARD_RELEASES,
    # TODO(https://crbug.com/1109276) Once support for mastername is removed, do
    # not explicitly set
    builder_group = "chromium.linux",
    console_view_entry = ci.console_view_entry(
        category = "release",
        short_name = "tst",
    ),
    cq_mirrors_console_view = settings.cq_mirrors_console_name,
    goma_backend = None,
    main_console_view = settings.main_console_name,
    triggered_by = [builder_name("Linux Builder")],
)

ci.linux_builder(
    name = "Linux Tests (dbg)(1)",
    branch_selector = branches.STANDARD_RELEASES,
    console_view_entry = ci.console_view_entry(
        category = "debug|tester",
        short_name = "64",
    ),
    cq_mirrors_console_view = settings.cq_mirrors_console_name,
    main_console_view = settings.main_console_name,
    triggered_by = [builder_name("Linux Builder (dbg)")],
)

ci.linux_builder(
    name = "fuchsia-arm64-cast",
    branch_selector = branches.STANDARD_RELEASES,
    console_view_entry = ci.console_view_entry(
        category = "fuchsia|cast",
        short_name = "a64",
    ),
    cq_mirrors_console_view = settings.cq_mirrors_console_name,
    main_console_view = settings.main_console_name,
    # Set tree_closing to false to disable the defaualt tree closer, which
    # filters by step name, and instead enable tree closing for any step
    # failure.
    tree_closing = False,
    extra_notifies = ["cr-fuchsia", "close-on-any-step-failure"],
)

ci.linux_builder(
    name = "fuchsia-x64-cast",
    branch_selector = branches.STANDARD_RELEASES,
    console_view_entry = ci.console_view_entry(
        category = "fuchsia|cast",
        short_name = "x64",
    ),
    cq_mirrors_console_view = settings.cq_mirrors_console_name,
    main_console_view = settings.main_console_name,
    # Set tree_closing to false to disable the defaualt tree closer, which
    # filters by step name, and instead enable tree closing for any step
    # failure.
    tree_closing = False,
    extra_notifies = ["cr-fuchsia", "close-on-any-step-failure"],
)

ci.linux_builder(
    name = "linux-ozone-rel",
    branch_selector = branches.STANDARD_RELEASES,
    console_view_entry = ci.console_view_entry(
        category = "release",
        short_name = "ozo",
    ),
    cq_mirrors_console_view = settings.cq_mirrors_console_name,
    main_console_view = settings.main_console_name,
    # Set tree_closing to false to disable the defaualt tree closer, which
    # filters by step name, and instead enable tree closing for any step
    # failure.
    tree_closing = False,
    extra_notifies = ["linux-ozone-rel", "close-on-any-step-failure"],
)

ci.linux_builder(
    name = "Linux Ozone Tester (Headless)",
    branch_selector = branches.STANDARD_RELEASES,
    console_view = "chromium.fyi",
    console_view_entry = ci.console_view_entry(
        category = "linux",
        short_name = "loh",
    ),
    cq_mirrors_console_view = settings.cq_mirrors_console_name,
    main_console_view = main_console_if_on_branch(),
    triggered_by = [builder_name("linux-ozone-rel")],
)

ci.linux_builder(
    name = "Linux Ozone Tester (Wayland)",
    branch_selector = branches.STANDARD_RELEASES,
    console_view = "chromium.fyi",
    console_view_entry = ci.console_view_entry(
        category = "linux",
        short_name = "low",
    ),
    cq_mirrors_console_view = settings.cq_mirrors_console_name,
    main_console_view = main_console_if_on_branch(),
    triggered_by = [builder_name("linux-ozone-rel")],
)

ci.linux_builder(
    name = "Linux Ozone Tester (X11)",
    branch_selector = branches.STANDARD_RELEASES,
    console_view = "chromium.fyi",
    console_view_entry = ci.console_view_entry(
        category = "linux",
        short_name = "lox",
    ),
    cq_mirrors_console_view = settings.cq_mirrors_console_name,
    main_console_view = main_console_if_on_branch(),
    triggered_by = [builder_name("linux-ozone-rel")],
)

ci.mac_builder(
    name = "Mac Builder",
    branch_selector = branches.STANDARD_RELEASES,
    console_view_entry = ci.console_view_entry(
        category = "release",
        short_name = "bld",
    ),
    cq_mirrors_console_view = settings.cq_mirrors_console_name,
    main_console_view = settings.main_console_name,
    os = os.MAC_10_15,
)

ci.mac_builder(
    name = "Mac Builder (dbg)",
    branch_selector = branches.STANDARD_RELEASES,
    console_view_entry = ci.console_view_entry(
        category = "debug",
        short_name = "bld",
    ),
    cq_mirrors_console_view = settings.cq_mirrors_console_name,
    main_console_view = settings.main_console_name,
    os = os.MAC_ANY,
)

ci.mac_builder(
    name = "mac-arm64-rel",
    branch_selector = branches.STANDARD_RELEASES,
    console_view_entry = ci.console_view_entry(
        category = "release",
        short_name = "a64",
    ),
    main_console_view = settings.main_console_name,
    cores = None,
    os = os.MAC_ANY,
)

ci.thin_tester(
    name = "Mac10.10 Tests",
    branch_selector = branches.STANDARD_RELEASES,
    mastername = "chromium.mac",
    console_view_entry = ci.console_view_entry(
        category = "release",
        short_name = "10",
    ),
    cq_mirrors_console_view = settings.cq_mirrors_console_name,
    main_console_view = settings.main_console_name,
    triggered_by = [builder_name("Mac Builder")],
)

ci.thin_tester(
    name = "Mac10.11 Tests",
    branch_selector = branches.STANDARD_RELEASES,
    mastername = "chromium.mac",
    console_view_entry = ci.console_view_entry(
        category = "release",
        short_name = "11",
    ),
    cq_mirrors_console_view = settings.cq_mirrors_console_name,
    main_console_view = settings.main_console_name,
    triggered_by = [builder_name("Mac Builder")],
)

ci.thin_tester(
    name = "Mac10.12 Tests",
    branch_selector = branches.STANDARD_RELEASES,
    mastername = "chromium.mac",
    console_view_entry = ci.console_view_entry(
        category = "release",
        short_name = "12",
    ),
    cq_mirrors_console_view = settings.cq_mirrors_console_name,
    main_console_view = settings.main_console_name,
    triggered_by = [builder_name("Mac Builder")],
)

ci.thin_tester(
    name = "Mac10.13 Tests",
    branch_selector = branches.STANDARD_RELEASES,
    mastername = "chromium.mac",
    console_view_entry = ci.console_view_entry(
        category = "release",
        short_name = "13",
    ),
    cq_mirrors_console_view = settings.cq_mirrors_console_name,
    main_console_view = settings.main_console_name,
    triggered_by = [builder_name("Mac Builder")],
)

ci.thin_tester(
    name = "Mac10.14 Tests",
    branch_selector = branches.STANDARD_RELEASES,
    mastername = "chromium.mac",
    console_view_entry = ci.console_view_entry(
        category = "release",
        short_name = "14",
    ),
    cq_mirrors_console_view = settings.cq_mirrors_console_name,
    main_console_view = settings.main_console_name,
    triggered_by = [builder_name("Mac Builder")],
)

ci.thin_tester(
    name = "Mac10.15 Tests",
    branch_selector = branches.STANDARD_RELEASES,
    mastername = "chromium.mac",
    console_view_entry = ci.console_view_entry(
        category = "release",
        short_name = "15",
    ),
    cq_mirrors_console_view = settings.cq_mirrors_console_name,
    main_console_view = settings.main_console_name,
    triggered_by = [builder_name("Mac Builder")],
)

ci.thin_tester(
    name = "Mac10.13 Tests (dbg)",
    branch_selector = branches.STANDARD_RELEASES,
    mastername = "chromium.mac",
    console_view_entry = ci.console_view_entry(
        category = "debug",
        short_name = "13",
    ),
    cq_mirrors_console_view = settings.cq_mirrors_console_name,
    main_console_view = settings.main_console_name,
    triggered_by = [builder_name("Mac Builder (dbg)")],
)

ci.mac_ios_builder(
    name = "ios-simulator",
    branch_selector = branches.STANDARD_RELEASES,
    console_view_entry = ci.console_view_entry(
        category = "ios|default",
        short_name = "sim",
    ),
    cq_mirrors_console_view = settings.cq_mirrors_console_name,
    main_console_view = settings.main_console_name,
)

ci.mac_ios_builder(
    name = "ios-simulator-full-configs",
    branch_selector = branches.STANDARD_RELEASES,
    console_view_entry = ci.console_view_entry(
        category = "ios|default",
        short_name = "ful",
    ),
    cq_mirrors_console_view = settings.cq_mirrors_console_name,
    main_console_view = settings.main_console_name,
)

ci.memory_builder(
    name = "Linux ASan LSan Builder",
    branch_selector = branches.STANDARD_RELEASES,
    console_view_entry = ci.console_view_entry(
        category = "linux|asan lsan",
        short_name = "bld",
    ),
    cq_mirrors_console_view = settings.cq_mirrors_console_name,
    main_console_view = settings.main_console_name,
    ssd = True,
)

ci.memory_builder(
    name = "Linux ASan LSan Tests (1)",
    branch_selector = branches.STANDARD_RELEASES,
    console_view_entry = ci.console_view_entry(
        category = "linux|asan lsan",
        short_name = "tst",
    ),
    cq_mirrors_console_view = settings.cq_mirrors_console_name,
    main_console_view = settings.main_console_name,
    triggered_by = [builder_name("Linux ASan LSan Builder")],
)

ci.memory_builder(
    name = "Linux ASan Tests (sandboxed)",
    branch_selector = branches.STANDARD_RELEASES,
    console_view_entry = ci.console_view_entry(
        category = "linux|asan lsan",
        short_name = "sbx",
    ),
    cq_mirrors_console_view = settings.cq_mirrors_console_name,
    main_console_view = settings.main_console_name,
    triggered_by = [builder_name("Linux ASan LSan Builder")],
)

ci.memory_builder(
    name = "Linux TSan Builder",
    branch_selector = branches.STANDARD_RELEASES,
    console_view_entry = ci.console_view_entry(
        category = "linux|TSan v2",
        short_name = "bld",
    ),
    cq_mirrors_console_view = settings.cq_mirrors_console_name,
    main_console_view = settings.main_console_name,
)

ci.memory_builder(
    name = "Linux TSan Tests",
    branch_selector = branches.STANDARD_RELEASES,
    console_view_entry = ci.console_view_entry(
        category = "linux|TSan v2",
        short_name = "tst",
    ),
    cq_mirrors_console_view = settings.cq_mirrors_console_name,
    triggered_by = [builder_name("Linux TSan Builder")],
    main_console_view = settings.main_console_name,
)

ci.win_builder(
    name = "Win7 Tests (dbg)(1)",
    branch_selector = branches.STANDARD_RELEASES,
    console_view_entry = ci.console_view_entry(
        category = "debug|tester",
        short_name = "7",
    ),
    cq_mirrors_console_view = settings.cq_mirrors_console_name,
    os = os.WINDOWS_7,
    main_console_view = settings.main_console_name,
    triggered_by = [builder_name("Win Builder (dbg)")],
)

ci.win_builder(
    name = "Win 7 Tests x64 (1)",
    branch_selector = branches.STANDARD_RELEASES,
    console_view_entry = ci.console_view_entry(
        category = "release|tester",
        short_name = "64",
    ),
    cq_mirrors_console_view = settings.cq_mirrors_console_name,
    os = os.WINDOWS_7,
    main_console_view = settings.main_console_name,
    triggered_by = [builder_name("Win x64 Builder")],
)

ci.win_builder(
    name = "Win Builder (dbg)",
    branch_selector = branches.STANDARD_RELEASES,
    console_view_entry = ci.console_view_entry(
        category = "debug|builder",
        short_name = "32",
    ),
    cores = 32,
    cq_mirrors_console_view = settings.cq_mirrors_console_name,
    main_console_view = settings.main_console_name,
    os = os.WINDOWS_ANY,
)

ci.win_builder(
    name = "Win x64 Builder",
    branch_selector = branches.STANDARD_RELEASES,
    console_view_entry = ci.console_view_entry(
        category = "release|builder",
        short_name = "64",
    ),
    cores = 32,
    cq_mirrors_console_view = settings.cq_mirrors_console_name,
    main_console_view = settings.main_console_name,
    os = os.WINDOWS_ANY,
)

ci.win_builder(
    name = "Win10 Tests x64",
    branch_selector = branches.STANDARD_RELEASES,
    console_view_entry = ci.console_view_entry(
        category = "release|tester",
        short_name = "w10",
    ),
    cq_mirrors_console_view = settings.cq_mirrors_console_name,
    main_console_view = settings.main_console_name,
    triggered_by = [builder_name("Win x64 Builder")],
)
