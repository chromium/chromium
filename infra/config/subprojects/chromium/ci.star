# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

load("//lib/branches.star", "branches")
load("//lib/builders.star", "builder_name", "cpu", "goma", "os", "xcode_cache")
load("//lib/ci.star", "ci")
load("//project.star", "settings")

def main_console_if_on_branch():
    return None if settings.is_master else settings.main_console_name

ci.set_defaults(
    settings,
    add_to_console_view = True,
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
    branch_selector = branches.ALL_RELEASES,
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
    name = "chromium.mojo",
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

# The chromium.clang console includes some entries for builders from the chrome project
[branches.console_view_entry(
    builder = "chrome:ci/{}".format(name),
    console_view = "chromium.clang",
    category = category,
    short_name = short_name,
) for name, category, short_name in (
    ("ToTLinuxOfficial", "ToT Linux", "ofi"),
    ("ToTMacOfficial", "ToT Mac", "ofi"),
    ("ToTWin", "ToT Windows", "rel"),
    ("ToTWin64", "ToT Windows|x64", "rel"),
    ("ToTWinOfficial", "ToT Windows", "ofi"),
    ("ToTWinThinLTO64", "ToT Windows|x64", "lto"),
    ("clang-tot-device", "iOS|internal", "dev"),
)]

# The main console includes some entries for builders from the chrome project
[branches.console_view_entry(
    builder = "chrome:ci/{}".format(name),
    console_view = "main",
    category = "chrome",
    short_name = short_name,
) for name, short_name in (
    ("linux-chromeos-chrome", "cro"),
    ("linux-chrome", "lnx"),
    ("mac-chrome", "mac"),
    ("win-chrome", "win"),
    ("win64-chrome", "win"),
)]

# Builders are sorted first lexicographically by the function used to define
# them, then lexicographically by their name

ci.builder(
    name = "android-avd-packager",
    executable = "recipe:android/avd_packager",
    properties = {
        "avd_configs": [
            "tools/android/avd/proto/creation/generic_android23.textpb",
            "tools/android/avd/proto/creation/generic_android28.textpb",
            "tools/android/avd/proto/creation/generic_android29.textpb",
            "tools/android/avd/proto/creation/generic_playstore_android28.textpb",
        ],
    },
    schedule = "0 7 * * 0 *",
    service_account = "chromium-cipd-builder@chops-service-accounts.iam.gserviceaccount.com",
    triggered_by = [],
)

ci.builder(
    name = "android-sdk-packager",
    executable = "recipe:android/sdk_packager",
    schedule = "0 7 * * 0 *",
    service_account = "chromium-cipd-builder@chops-service-accounts.iam.gserviceaccount.com",
    triggered_by = [],
    properties = {
        # We still package part of build-tools;25.0.2 to support
        # http://bit.ly/2KNUygZ
        "packages": [
            {
                "sdk_package_name": "build-tools;25.0.2",
                "cipd_yaml": "third_party/android_sdk/cipd/build-tools/25.0.2.yaml",
            },
            {
                "sdk_package_name": "build-tools;29.0.2",
                "cipd_yaml": "third_party/android_sdk/cipd/build-tools/29.0.2.yaml",
            },
            {
                "sdk_package_name": "build-tools;30.0.1",
                "cipd_yaml": "third_party/android_sdk/cipd/build-tools/30.0.1.yaml",
            },
            {
                "sdk_package_name": "cmdline-tools;latest",
                "cipd_yaml": "third_party/android_sdk/cipd/cmdline-tools.yaml",
            },
            {
                "sdk_package_name": "emulator",
                "cipd_yaml": "third_party/android_sdk/cipd/emulator.yaml",
            },
            {
                "sdk_package_name": "extras;google;gcm",
                "cipd_yaml": "third_party/android_sdk/cipd/extras/google/gcm.yaml",
            },
            {
                "sdk_package_name": "patcher;v4",
                "cipd_yaml": "third_party/android_sdk/cipd/patcher/v4.yaml",
            },
            {
                "sdk_package_name": "platforms;android-29",
                "cipd_yaml": "third_party/android_sdk/cipd/platforms/android-29.yaml",
            },
            {
                "sdk_package_name": "platforms;android-30",
                "cipd_yaml": "third_party/android_sdk/cipd/platforms/android-30.yaml",
            },
            {
                "sdk_package_name": "platform-tools",
                "cipd_yaml": "third_party/android_sdk/cipd/platform-tools.yaml",
            },
            {
                "sdk_package_name": "sources;android-29",
                "cipd_yaml": "third_party/android_sdk/cipd/sources/android-29.yaml",
            },
            # Not yet available as R is not released to AOSP.
            #{
            #    'sdk_package_name': 'sources;android-30',
            #    'cipd_yaml': 'third_party/android_sdk/cipd/sources/android-30.yaml'
            #},
            {
                "sdk_package_name": "system-images;android-29;google_apis;x86",
                "cipd_yaml": "third_party/android_sdk/cipd/system_images/android-29/google_apis/x86.yaml",
            },
            {
                "sdk_package_name": "system-images;android-29;google_apis_playstore;x86",
                "cipd_yaml": "third_party/android_sdk/cipd/system_images/android-29/google_apis_playstore/x86.yaml",
            },
            {
                "sdk_package_name": "system-images;android-30;google_apis;x86",
                "cipd_yaml": "third_party/android_sdk/cipd/system_images/android-30/google_apis/x86.yaml",
            },
            {
                "sdk_package_name": "system-images;android-30;google_apis_playstore;x86",
                "cipd_yaml": "third_party/android_sdk/cipd/system_images/android-30/google_apis_playstore/x86.yaml",
            },
        ],
    },
)

ci.android_builder(
    name = "Android ASAN (dbg)",
    console_view_entry = ci.console_view_entry(
        category = "builder|arm",
        short_name = "san",
    ),
    # Higher build timeout since dbg ASAN builds can take a while on a clobber
    # build.
    execution_timeout = 4 * time.hour,
    tree_closing = True,
)

ci.android_builder(
    name = "Android WebView L (dbg)",
    console_view_entry = ci.console_view_entry(
        category = "tester|webview",
        short_name = "L",
    ),
    triggered_by = ["ci/Android arm Builder (dbg)"],
)

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
    name = "Deterministic Android",
    console_view_entry = ci.console_view_entry(
        category = "builder|det",
        short_name = "rel",
    ),
    executable = "recipe:swarming/deterministic_build",
    execution_timeout = 6 * time.hour,
    notifies = ["Deterministic Android"],
    tree_closing = True,
)

ci.android_builder(
    name = "Deterministic Android (dbg)",
    console_view_entry = ci.console_view_entry(
        category = "builder|det",
        short_name = "dbg",
    ),
    executable = "recipe:swarming/deterministic_build",
    execution_timeout = 6 * time.hour,
    notifies = ["Deterministic Android"],
    tree_closing = True,
)

ci.android_builder(
    name = "Lollipop Phone Tester",
    console_view_entry = ci.console_view_entry(
        category = "tester|phone",
        short_name = "L",
    ),
    # We have limited phone capacity and thus limited ability to run
    # tests in parallel, hence the high timeout.
    execution_timeout = 6 * time.hour,
    triggered_by = ["ci/Android arm Builder (dbg)"],
)

ci.android_builder(
    name = "Lollipop Tablet Tester",
    console_view_entry = ci.console_view_entry(
        category = "tester|tablet",
        short_name = "L",
    ),
    # We have limited tablet capacity and thus limited ability to run
    # tests in parallel, hence the high timeout.
    execution_timeout = 20 * time.hour,
    triggered_by = ["ci/Android arm Builder (dbg)"],
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
    name = "Marshmallow Tablet Tester",
    console_view_entry = ci.console_view_entry(
        category = "tester|tablet",
        short_name = "M",
    ),
    # We have limited tablet capacity and thus limited ability to run
    # tests in parallel, hence the high timeout.
    execution_timeout = 12 * time.hour,
    triggered_by = ["ci/Android arm Builder (dbg)"],
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
    name = "android-10-arm64-rel",
    console_view_entry = ci.console_view_entry(
        category = "builder_tester|arm64",
        short_name = "10",
    ),
)

ci.android_builder(
    name = "android-arm64-proguard-rel",
    console_view_entry = ci.console_view_entry(
        category = "builder_tester|arm64",
        short_name = "M proguard",
    ),
    goma_jobs = goma.jobs.MANY_JOBS_FOR_CI,
    execution_timeout = 6 * time.hour,
)

ci.android_builder(
    name = "android-bfcache-rel",
    console_view_entry = ci.console_view_entry(
        category = "bfcache",
        short_name = "bfc",
    ),
)

ci.android_builder(
    name = "android-binary-size-generator",
    executable = "recipe:binary_size_generator_tot",
    console_view_entry = ci.console_view_entry(
        category = "builder|other",
        short_name = "size",
    ),
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
    name = "android-cronet-arm64-dbg",
    console_view_entry = ci.console_view_entry(
        category = "cronet|arm64",
        short_name = "dbg",
    ),
    notifies = ["cronet"],
)

ci.android_builder(
    name = "android-cronet-arm64-rel",
    console_view_entry = ci.console_view_entry(
        category = "cronet|arm64",
        short_name = "rel",
    ),
    notifies = ["cronet"],
)

ci.android_builder(
    name = "android-cronet-asan-arm-rel",
    console_view_entry = ci.console_view_entry(
        category = "cronet|asan",
    ),
    notifies = ["cronet"],
)

ci.android_builder(
    name = "android-cronet-arm-rel-kitkat-tests",
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
    name = "android-cronet-arm-rel-lollipop-tests",
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

# Runs on a specific machine with an attached phone
ci.android_builder(
    name = "android-cronet-marshmallow-arm64-perf-rel",
    console_view_entry = ci.console_view_entry(
        category = "cronet|test|perf",
        short_name = "m",
    ),
    cores = None,
    cpu = None,
    executable = "recipe:cronet",
    notifies = ["cronet"],
    os = os.ANDROID,
)

ci.android_builder(
    name = "android-cronet-arm64-rel-marshmallow-tests",
    console_view_entry = ci.console_view_entry(
        category = "cronet|test",
        short_name = "m",
    ),
    notifies = ["cronet"],
    triggered_by = ["android-cronet-arm64-rel"],
)

ci.android_builder(
    name = "android-cronet-x86-dbg",
    console_view_entry = ci.console_view_entry(
        category = "cronet|x86",
        short_name = "dbg",
    ),
    notifies = ["cronet"],
)

ci.android_builder(
    name = "android-cronet-x86-rel",
    console_view_entry = ci.console_view_entry(
        category = "cronet|x86",
        short_name = "rel",
    ),
    notifies = ["cronet"],
)

ci.android_builder(
    name = "android-incremental-dbg",
    console_view_entry = ci.console_view_entry(
        category = "tester|incremental",
    ),
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
    name = "android-marshmallow-x86-rel-non-cq",
    console_view_entry = ci.console_view_entry(
        category = "builder_tester|x86",
        short_name = "M_non-cq",
    ),
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

ci.android_fyi_builder(
    name = "android-pie-arm64-wpt-rel-non-cq",
    console_view_entry = ci.console_view_entry(
        category = "builder_tester|arm64",
        short_name = "P-WPT",
    ),
)

ci.android_builder(
    name = "android-pie-x86-rel",
    console_view_entry = ci.console_view_entry(
        category = "builder_tester|x86",
        short_name = "P",
    ),
)

ci.android_fyi_builder(
    name = "android-weblayer-pie-x86-fyi-rel",
    console_view_entry = ci.console_view_entry(
        category = "weblayer",
        short_name = "p-x86-rel",
    ),
)

ci.android_fyi_builder(
    name = "Android WebView P FYI (rel)",
    console_view_entry = ci.console_view_entry(
        category = "webview",
        short_name = "p-rel",
    ),
)

# TODO(hypan): remove this once there is no associated disabled tests
ci.android_fyi_builder(
    name = "android-pie-x86-fyi-rel",
    console_view_entry = ci.console_view_entry(
        category = "emulator|P|x86",
        short_name = "rel",
    ),
    goma_jobs = goma.jobs.J150,
    schedule = "triggered",  # triggered manually via Scheduler UI
)

ci.chromium_builder(
    name = "android-archive-dbg",
    # Bump to 32 if needed.
    console_view_entry = ci.console_view_entry(
        category = "android",
        short_name = "dbg",
    ),
    cores = 8,
    main_console_view = "main",
)

ci.chromium_builder(
    name = "android-archive-rel",
    console_view_entry = ci.console_view_entry(
        category = "android",
        short_name = "rel",
    ),
    cores = 32,
    main_console_view = "main",
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
    name = "linux-archive-dbg",
    console_view_entry = ci.console_view_entry(
        category = "linux",
        short_name = "dbg",
    ),
    # Bump to 32 if needed.
    cores = 8,
    main_console_view = "main",
)

ci.chromium_builder(
    name = "linux-archive-rel",
    console_view_entry = ci.console_view_entry(
        category = "linux",
        short_name = "rel",
    ),
    cores = 32,
    main_console_view = "main",
    notifies = ["linux-archive-rel"],
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
    name = "mac-archive-dbg",
    console_view_entry = ci.console_view_entry(
        category = "mac",
        short_name = "dbg",
    ),
    # Bump to 8 cores if needed.
    cores = 4,
    main_console_view = "main",
    os = os.MAC_DEFAULT,
)

ci.chromium_builder(
    name = "mac-archive-rel",
    console_view_entry = ci.console_view_entry(
        category = "mac",
        short_name = "rel",
    ),
    main_console_view = "main",
    os = os.MAC_DEFAULT,
)

ci.chromium_builder(
    name = "mac-official",
    builderless = False,
    # TODO(https://crbug.com/1072012) Use the default console view and add
    # main_console_view = settings.main_console_name once the build is green
    console_view = "chromium.fyi",
    console_view_entry = ci.console_view_entry(
        category = "mac",
        short_name = "off",
    ),
    # TODO: Change this back down to something reasonable once these builders
    # have populated their cached by getting through the compile step
    execution_timeout = 10 * time.hour,
    main_console_view = main_console_if_on_branch(),
    tree_closing = False,
    os = os.MAC_ANY,
    cores = None,
)

ci.chromium_builder(
    name = "win-archive-dbg",
    console_view_entry = ci.console_view_entry(
        category = "win|dbg",
        short_name = "64",
    ),
    cores = 32,
    main_console_view = "main",
    os = os.WINDOWS_DEFAULT,
    tree_closing = False,
)

ci.chromium_builder(
    name = "win-archive-rel",
    console_view_entry = ci.console_view_entry(
        category = "win|rel",
        short_name = "64",
    ),
    cores = 32,
    main_console_view = "main",
    os = os.WINDOWS_DEFAULT,
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
    name = "win32-archive-dbg",
    console_view_entry = ci.console_view_entry(
        category = "win|dbg",
        short_name = "32",
    ),
    cores = 32,
    main_console_view = "main",
    os = os.WINDOWS_DEFAULT,
    tree_closing = False,
)

ci.chromium_builder(
    name = "win32-archive-rel",
    console_view_entry = ci.console_view_entry(
        category = "win|rel",
        short_name = "32",
    ),
    cores = 32,
    main_console_view = "main",
    os = os.WINDOWS_DEFAULT,
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
    name = "Linux ChromiumOS Full",
    console_view_entry = ci.console_view_entry(
        category = "default",
        short_name = "ful",
    ),
    main_console_view = "main",
)

ci.chromiumos_builder(
    name = "chromeos-amd64-generic-asan-rel",
    console_view_entry = ci.console_view_entry(
        category = "simple|release|x64",
        short_name = "asn",
    ),
    main_console_view = "main",
)

ci.chromiumos_builder(
    name = "chromeos-amd64-generic-cfi-thin-lto-rel",
    console_view_entry = ci.console_view_entry(
        category = "simple|release|x64",
        short_name = "cfi",
    ),
    main_console_view = "main",
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
    name = "chromeos-amd64-generic-lacros-dbg",
    branch_selector = branches.STANDARD_RELEASES,
    console_view_entry = ci.console_view_entry(
        category = "lacros|x64",
        short_name = "dbg",
    ),
    cq_mirrors_console_view = settings.cq_mirrors_console_name,
    main_console_view = settings.main_console_name,
)

ci.chromiumos_builder(
    name = "chromeos-amd64-generic-rel",
    branch_selector = branches.ALL_RELEASES,
    console_view_entry = ci.console_view_entry(
        category = "simple|release|x64",
        short_name = "rel",
    ),
    cq_mirrors_console_view = settings.cq_mirrors_console_name,
    main_console_view = settings.main_console_name,
)

ci.chromiumos_builder(
    name = "chromeos-arm-generic-dbg",
    console_view_entry = ci.console_view_entry(
        category = "simple|debug",
        short_name = "arm",
    ),
    main_console_view = "main",
)

ci.chromiumos_builder(
    name = "chromeos-arm-generic-rel",
    branch_selector = branches.ALL_RELEASES,
    console_view_entry = ci.console_view_entry(
        category = "simple|release",
        short_name = "arm",
    ),
    cq_mirrors_console_view = settings.cq_mirrors_console_name,
    main_console_view = settings.main_console_name,
)

ci.chromiumos_builder(
    name = "chromeos-kevin-rel",
    branch_selector = branches.ALL_RELEASES,
    console_view_entry = ci.console_view_entry(
        category = "simple|release",
        short_name = "kvn",
    ),
    main_console_view = "main",
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
    branch_selector = branches.ALL_RELEASES,
    console_view_entry = ci.console_view_entry(
        category = "default",
        short_name = "rel",
    ),
    cq_mirrors_console_view = settings.cq_mirrors_console_name,
    main_console_view = settings.main_console_name,
)

ci.clang_builder(
    name = "CFI Linux CF",
    goma_backend = goma.backend.RBE_PROD,
    console_view_entry = ci.console_view_entry(
        category = "CFI|Linux",
        short_name = "CF",
    ),
    notifies = ["CFI Linux"],
)

ci.clang_builder(
    name = "CFI Linux ToT",
    console_view_entry = ci.console_view_entry(
        category = "CFI|Linux",
        short_name = "ToT",
    ),
    notifies = ["CFI Linux"],
)

ci.clang_builder(
    name = "CrWinAsan",
    console_view_entry = ci.console_view_entry(
        category = "ToT Windows|Asan",
        short_name = "asn",
    ),
    os = os.WINDOWS_ANY,
)

ci.clang_builder(
    name = "CrWinAsan(dll)",
    console_view_entry = ci.console_view_entry(
        category = "ToT Windows|Asan",
        short_name = "dll",
    ),
    os = os.WINDOWS_ANY,
)

ci.clang_builder(
    name = "ToTAndroid",
    console_view_entry = ci.console_view_entry(
        category = "ToT Android",
        short_name = "rel",
    ),
)

ci.clang_builder(
    name = "ToTAndroid (dbg)",
    console_view_entry = ci.console_view_entry(
        category = "ToT Android",
        short_name = "dbg",
    ),
)

ci.clang_builder(
    name = "ToTAndroid x64",
    console_view_entry = ci.console_view_entry(
        category = "ToT Android",
        short_name = "x64",
    ),
)

ci.clang_builder(
    name = "ToTAndroid64",
    console_view_entry = ci.console_view_entry(
        category = "ToT Android",
        short_name = "a64",
    ),
)

ci.clang_builder(
    name = "ToTAndroidASan",
    console_view_entry = ci.console_view_entry(
        category = "ToT Android",
        short_name = "asn",
    ),
)

ci.clang_builder(
    name = "ToTAndroidCFI",
    console_view_entry = ci.console_view_entry(
        category = "ToT Android",
        short_name = "cfi",
    ),
)

ci.clang_builder(
    name = "ToTAndroidOfficial",
    console_view_entry = ci.console_view_entry(
        category = "ToT Android",
        short_name = "off",
    ),
)

def clang_tot_linux_builder(short_name, category = "ToT Linux", **kwargs):
    ci.clang_builder(
        console_view_entry = ci.console_view_entry(
            category = category,
            short_name = short_name,
        ),
        notifies = [luci.notifier(
            name = "ToT Linux notifier",
            on_new_status = ["FAILURE"],
            notify_emails = ["thomasanderson@chromium.org"],
        )],
        **kwargs
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
    category = "ToT Code Coverage",
    short_name = "linux",
    executable = "recipe:chromium_clang_coverage_tot",
)

clang_tot_linux_builder(
    name = "ToTLinuxMSan",
    short_name = "msn",
)

clang_tot_linux_builder(
    name = "ToTLinuxTSan",
    short_name = "tsn",
)

clang_tot_linux_builder(
    name = "ToTLinuxThinLTO",
    short_name = "lto",
)

clang_tot_linux_builder(
    name = "ToTLinuxUBSanVptr",
    short_name = "usn",
)

ci.clang_builder(
    name = "ToTWin(dbg)",
    console_view_entry = ci.console_view_entry(
        category = "ToT Windows",
        short_name = "dbg",
    ),
    os = os.WINDOWS_ANY,
)

ci.clang_builder(
    name = "ToTWin(dll)",
    console_view_entry = ci.console_view_entry(
        category = "ToT Windows",
        short_name = "dll",
    ),
    os = os.WINDOWS_ANY,
)

ci.clang_builder(
    name = "ToTWin64(dbg)",
    console_view_entry = ci.console_view_entry(
        category = "ToT Windows|x64",
        short_name = "dbg",
    ),
    os = os.WINDOWS_ANY,
)

ci.clang_builder(
    name = "ToTWin64(dll)",
    console_view_entry = ci.console_view_entry(
        category = "ToT Windows|x64",
        short_name = "dll",
    ),
    os = os.WINDOWS_ANY,
)

ci.clang_builder(
    name = "ToTWinASanLibfuzzer",
    console_view_entry = ci.console_view_entry(
        category = "ToT Windows|Asan",
        short_name = "fuz",
    ),
    os = os.WINDOWS_ANY,
)

ci.clang_builder(
    name = "ToTWinCFI",
    console_view_entry = ci.console_view_entry(
        category = "CFI|Win",
        short_name = "x86",
    ),
    os = os.WINDOWS_ANY,
)

ci.clang_builder(
    name = "ToTWinCFI64",
    console_view_entry = ci.console_view_entry(
        category = "CFI|Win",
        short_name = "x64",
    ),
    os = os.WINDOWS_ANY,
)

ci.clang_builder(
    name = "UBSanVptr Linux",
    console_view_entry = ci.console_view_entry(
        short_name = "usn",
    ),
    goma_backend = goma.backend.RBE_PROD,
)

ci.clang_builder(
    name = "linux-win_cross-rel",
    console_view_entry = ci.console_view_entry(
        category = "ToT Windows",
        short_name = "lxw",
    ),
)

ci.clang_builder(
    name = "ToTiOS",
    builderless = False,
    caches = [xcode_cache.x12a7209],
    console_view_entry = ci.console_view_entry(
        category = "iOS|public",
        short_name = "sim",
    ),
    cores = None,
    os = os.MAC_10_15,
    properties = {
        "xcode_build_version": "12a7209",
    },
    ssd = True,
)

ci.clang_builder(
    name = "ToTiOSDevice",
    builderless = False,
    caches = [xcode_cache.x12a7209],
    console_view_entry = ci.console_view_entry(
        category = "iOS|public",
        short_name = "dev",
    ),
    cores = None,
    os = os.MAC_10_15,
    properties = {
        "xcode_build_version": "12a7209",
    },
    ssd = True,
)

ci.clang_mac_builder(
    name = "ToTMac",
    console_view_entry = ci.console_view_entry(
        category = "ToT Mac",
        short_name = "rel",
    ),
)

ci.clang_mac_builder(
    name = "ToTMac (dbg)",
    console_view_entry = ci.console_view_entry(
        category = "ToT Mac",
        short_name = "dbg",
    ),
)

ci.clang_mac_builder(
    name = "ToTMacASan",
    console_view_entry = ci.console_view_entry(
        category = "ToT Mac",
        short_name = "asn",
    ),
)

ci.clang_mac_builder(
    name = "ToTMacCoverage",
    console_view_entry = ci.console_view_entry(
        category = "ToT Code Coverage",
        short_name = "mac",
    ),
    executable = "recipe:chromium_clang_coverage_tot",
)

ci.dawn_linux_builder(
    name = "Dawn Linux x64 Builder",
    console_view_entry = ci.console_view_entry(
        category = "ToT|Linux|Builder",
        short_name = "x64",
    ),
)

ci.dawn_linux_builder(
    name = "Dawn Linux x64 DEPS Builder",
    branch_selector = branches.STANDARD_RELEASES,
    console_view_entry = ci.console_view_entry(
        category = "DEPS|Linux|Builder",
        short_name = "x64",
    ),
    cq_mirrors_console_view = settings.cq_mirrors_console_name,
    main_console_view = main_console_if_on_branch(),
)

ci.dawn_thin_tester(
    name = "Dawn Linux x64 DEPS Release (Intel HD 630)",
    branch_selector = branches.STANDARD_RELEASES,
    console_view_entry = ci.console_view_entry(
        category = "DEPS|Linux|Intel",
        short_name = "x64",
    ),
    cq_mirrors_console_view = settings.cq_mirrors_console_name,
    main_console_view = main_console_if_on_branch(),
    triggered_by = [builder_name("Dawn Linux x64 DEPS Builder")],
)

ci.dawn_thin_tester(
    name = "Dawn Linux x64 DEPS Release (NVIDIA)",
    branch_selector = branches.STANDARD_RELEASES,
    console_view_entry = ci.console_view_entry(
        category = "DEPS|Linux|Nvidia",
        short_name = "x64",
    ),
    cq_mirrors_console_view = settings.cq_mirrors_console_name,
    main_console_view = main_console_if_on_branch(),
    triggered_by = [builder_name("Dawn Linux x64 DEPS Builder")],
)

ci.dawn_thin_tester(
    name = "Dawn Linux x64 Release (Intel HD 630)",
    console_view_entry = ci.console_view_entry(
        category = "ToT|Linux|Intel",
        short_name = "x64",
    ),
    triggered_by = ["Dawn Linux x64 Builder"],
)

ci.dawn_thin_tester(
    name = "Dawn Linux x64 Release (NVIDIA)",
    console_view_entry = ci.console_view_entry(
        category = "ToT|Linux|Nvidia",
        short_name = "x64",
    ),
    triggered_by = ["Dawn Linux x64 Builder"],
)

ci.dawn_mac_builder(
    name = "Dawn Mac x64 Builder",
    console_view_entry = ci.console_view_entry(
        category = "ToT|Mac|Builder",
        short_name = "x64",
    ),
)

ci.dawn_mac_builder(
    name = "Dawn Mac x64 DEPS Builder",
    branch_selector = branches.STANDARD_RELEASES,
    console_view_entry = ci.console_view_entry(
        category = "DEPS|Mac|Builder",
        short_name = "x64",
    ),
    cq_mirrors_console_view = settings.cq_mirrors_console_name,
    main_console_view = main_console_if_on_branch(),
)

# Note that the Mac testers are all thin Linux VMs, triggering jobs on the
# physical Mac hardware in the Swarming pool which is why they run on linux
ci.dawn_thin_tester(
    name = "Dawn Mac x64 DEPS Release (AMD)",
    branch_selector = branches.STANDARD_RELEASES,
    console_view_entry = ci.console_view_entry(
        category = "DEPS|Mac|AMD",
        short_name = "x64",
    ),
    cq_mirrors_console_view = settings.cq_mirrors_console_name,
    main_console_view = main_console_if_on_branch(),
    triggered_by = [builder_name("Dawn Mac x64 DEPS Builder")],
)

ci.dawn_thin_tester(
    name = "Dawn Mac x64 DEPS Release (Intel)",
    branch_selector = branches.STANDARD_RELEASES,
    console_view_entry = ci.console_view_entry(
        category = "DEPS|Mac|Intel",
        short_name = "x64",
    ),
    cq_mirrors_console_view = settings.cq_mirrors_console_name,
    main_console_view = main_console_if_on_branch(),
    triggered_by = [builder_name("Dawn Mac x64 DEPS Builder")],
)

ci.dawn_thin_tester(
    name = "Dawn Mac x64 Release (AMD)",
    console_view_entry = ci.console_view_entry(
        category = "ToT|Mac|AMD",
        short_name = "x64",
    ),
    triggered_by = ["Dawn Mac x64 Builder"],
)

ci.dawn_thin_tester(
    name = "Dawn Mac x64 Release (Intel)",
    console_view_entry = ci.console_view_entry(
        category = "ToT|Mac|Intel",
        short_name = "x64",
    ),
    triggered_by = ["Dawn Mac x64 Builder"],
)

ci.dawn_windows_builder(
    name = "Dawn Win10 x64 ASAN Release",
    console_view_entry = ci.console_view_entry(
        category = "ToT|Windows|ASAN",
        short_name = "x64",
    ),
)

ci.dawn_windows_builder(
    name = "Dawn Win10 x64 Builder",
    console_view_entry = ci.console_view_entry(
        category = "ToT|Windows|Builder",
        short_name = "x64",
    ),
)

ci.dawn_windows_builder(
    name = "Dawn Win10 x64 DEPS Builder",
    branch_selector = branches.STANDARD_RELEASES,
    console_view_entry = ci.console_view_entry(
        category = "DEPS|Windows|Builder",
        short_name = "x64",
    ),
    cq_mirrors_console_view = settings.cq_mirrors_console_name,
    main_console_view = main_console_if_on_branch(),
)

# Note that the Win testers are all thin Linux VMs, triggering jobs on the
# physical Win hardware in the Swarming pool, which is why they run on linux
ci.dawn_thin_tester(
    name = "Dawn Win10 x64 DEPS Release (Intel HD 630)",
    branch_selector = branches.STANDARD_RELEASES,
    console_view_entry = ci.console_view_entry(
        category = "DEPS|Windows|Intel",
        short_name = "x64",
    ),
    cq_mirrors_console_view = settings.cq_mirrors_console_name,
    main_console_view = main_console_if_on_branch(),
    triggered_by = [builder_name("Dawn Win10 x64 DEPS Builder")],
)

ci.dawn_thin_tester(
    name = "Dawn Win10 x64 DEPS Release (NVIDIA)",
    branch_selector = branches.STANDARD_RELEASES,
    console_view_entry = ci.console_view_entry(
        category = "DEPS|Windows|Nvidia",
        short_name = "x64",
    ),
    cq_mirrors_console_view = settings.cq_mirrors_console_name,
    main_console_view = main_console_if_on_branch(),
    triggered_by = [builder_name("Dawn Win10 x64 DEPS Builder")],
)

ci.dawn_thin_tester(
    name = "Dawn Win10 x64 Release (Intel HD 630)",
    console_view_entry = ci.console_view_entry(
        category = "ToT|Windows|Intel",
        short_name = "x64",
    ),
    triggered_by = ["Dawn Win10 x64 Builder"],
)

ci.dawn_thin_tester(
    name = "Dawn Win10 x64 Release (NVIDIA)",
    console_view_entry = ci.console_view_entry(
        category = "ToT|Windows|Nvidia",
        short_name = "x64",
    ),
    triggered_by = ["Dawn Win10 x64 Builder"],
)

ci.dawn_windows_builder(
    name = "Dawn Win10 x86 Builder",
    console_view_entry = ci.console_view_entry(
        category = "ToT|Windows|Builder",
        short_name = "x86",
    ),
)

ci.dawn_windows_builder(
    name = "Dawn Win10 x86 DEPS Builder",
    branch_selector = branches.STANDARD_RELEASES,
    console_view_entry = ci.console_view_entry(
        category = "DEPS|Windows|Builder",
        short_name = "x86",
    ),
    cq_mirrors_console_view = settings.cq_mirrors_console_name,
    main_console_view = main_console_if_on_branch(),
)

# Note that the Win testers are all thin Linux VMs, triggering jobs on the
# physical Win hardware in the Swarming pool, which is why they run on linux
ci.dawn_thin_tester(
    name = "Dawn Win10 x86 DEPS Release (Intel HD 630)",
    branch_selector = branches.STANDARD_RELEASES,
    console_view_entry = ci.console_view_entry(
        category = "DEPS|Windows|Intel",
        short_name = "x86",
    ),
    cq_mirrors_console_view = settings.cq_mirrors_console_name,
    main_console_view = main_console_if_on_branch(),
    triggered_by = [builder_name("Dawn Win10 x86 DEPS Builder")],
)

ci.dawn_thin_tester(
    name = "Dawn Win10 x86 DEPS Release (NVIDIA)",
    branch_selector = branches.STANDARD_RELEASES,
    console_view_entry = ci.console_view_entry(
        category = "DEPS|Windows|Nvidia",
        short_name = "x86",
    ),
    cq_mirrors_console_view = settings.cq_mirrors_console_name,
    main_console_view = main_console_if_on_branch(),
    triggered_by = [builder_name("Dawn Win10 x86 DEPS Builder")],
)

ci.dawn_thin_tester(
    name = "Dawn Win10 x86 Release (Intel HD 630)",
    console_view_entry = ci.console_view_entry(
        category = "ToT|Windows|Intel",
        short_name = "x86",
    ),
    triggered_by = ["Dawn Win10 x86 Builder"],
)

ci.dawn_thin_tester(
    name = "Dawn Win10 x86 Release (NVIDIA)",
    console_view_entry = ci.console_view_entry(
        category = "ToT|Windows|Nvidia",
        short_name = "x86",
    ),
    triggered_by = ["Dawn Win10 x86 Builder"],
)

ci.fuzz_builder(
    name = "ASAN Debug",
    console_view_entry = ci.console_view_entry(
        category = "linux asan",
        short_name = "dbg",
    ),
    triggering_policy = scheduler.greedy_batching(
        max_concurrent_invocations = 4,
    ),
)

ci.fuzz_builder(
    name = "ASan Debug (32-bit x86 with V8-ARM)",
    console_view_entry = ci.console_view_entry(
        category = "linux asan|x64 v8-ARM",
        short_name = "dbg",
    ),
    triggering_policy = scheduler.greedy_batching(
        max_concurrent_invocations = 4,
    ),
)

ci.fuzz_builder(
    name = "ASAN Release",
    console_view_entry = ci.console_view_entry(
        category = "linux asan",
        short_name = "rel",
    ),
    triggering_policy = scheduler.greedy_batching(
        max_concurrent_invocations = 5,
    ),
)

ci.fuzz_builder(
    name = "ASan Release (32-bit x86 with V8-ARM)",
    console_view_entry = ci.console_view_entry(
        category = "linux asan|x64 v8-ARM",
        short_name = "rel",
    ),
    triggering_policy = scheduler.greedy_batching(
        max_concurrent_invocations = 4,
    ),
)

ci.fuzz_builder(
    name = "ASAN Release Media",
    console_view_entry = ci.console_view_entry(
        category = "linux asan",
        short_name = "med",
    ),
    triggering_policy = scheduler.greedy_batching(
        max_concurrent_invocations = 4,
    ),
)

ci.fuzz_builder(
    name = "Afl Upload Linux ASan",
    console_view_entry = ci.console_view_entry(
        category = "afl",
        short_name = "afl",
    ),
    executable = "recipe:chromium_afl",
    triggering_policy = scheduler.greedy_batching(
        max_concurrent_invocations = 4,
    ),
)

ci.fuzz_builder(
    name = "ASan Release Media (32-bit x86 with V8-ARM)",
    console_view_entry = ci.console_view_entry(
        category = "linux asan|x64 v8-ARM",
        short_name = "med",
    ),
    triggering_policy = scheduler.greedy_batching(
        max_concurrent_invocations = 4,
    ),
)

ci.fuzz_builder(
    name = "ChromiumOS ASAN Release",
    console_view_entry = ci.console_view_entry(
        category = "cros asan",
    ),
    triggering_policy = scheduler.greedy_batching(
        max_concurrent_invocations = 6,
    ),
)

ci.fuzz_builder(
    name = "MSAN Release (chained origins)",
    console_view_entry = ci.console_view_entry(
        category = "linux msan",
        short_name = "org",
    ),
    triggering_policy = scheduler.greedy_batching(
        max_concurrent_invocations = 4,
    ),
)

ci.fuzz_builder(
    name = "MSAN Release (no origins)",
    console_view_entry = ci.console_view_entry(
        category = "linux msan",
        short_name = "rel",
    ),
    triggering_policy = scheduler.greedy_batching(
        max_concurrent_invocations = 4,
    ),
)

ci.fuzz_builder(
    name = "Mac ASAN Release",
    builderless = False,
    console_view_entry = ci.console_view_entry(
        category = "mac asan",
        short_name = "rel",
    ),
    cores = 4,
    os = os.MAC_DEFAULT,
    triggering_policy = scheduler.greedy_batching(
        max_concurrent_invocations = 2,
    ),
)

ci.fuzz_builder(
    name = "Mac ASAN Release Media",
    builderless = False,
    console_view_entry = ci.console_view_entry(
        category = "mac asan",
        short_name = "med",
    ),
    cores = 4,
    os = os.MAC_DEFAULT,
    triggering_policy = scheduler.greedy_batching(
        max_concurrent_invocations = 2,
    ),
)

ci.fuzz_builder(
    name = "TSAN Debug",
    console_view_entry = ci.console_view_entry(
        category = "linux tsan",
        short_name = "dbg",
    ),
    triggering_policy = scheduler.greedy_batching(
        max_concurrent_invocations = 4,
    ),
)

ci.fuzz_builder(
    name = "TSAN Release",
    console_view_entry = ci.console_view_entry(
        category = "linux tsan",
        short_name = "rel",
    ),
    triggering_policy = scheduler.greedy_batching(
        max_concurrent_invocations = 3,
    ),
)

ci.fuzz_builder(
    name = "UBSan Release",
    console_view_entry = ci.console_view_entry(
        category = "linux UBSan",
        short_name = "rel",
    ),
    triggering_policy = scheduler.greedy_batching(
        max_concurrent_invocations = 4,
    ),
)

ci.fuzz_builder(
    name = "UBSan vptr Release",
    console_view_entry = ci.console_view_entry(
        category = "linux UBSan",
        short_name = "vpt",
    ),
    triggering_policy = scheduler.greedy_batching(
        max_concurrent_invocations = 4,
    ),
)

ci.fuzz_builder(
    name = "Win ASan Release",
    builderless = False,
    console_view_entry = ci.console_view_entry(
        category = "win asan",
        short_name = "rel",
    ),
    os = os.WINDOWS_DEFAULT,
    triggering_policy = scheduler.greedy_batching(
        max_concurrent_invocations = 7,
    ),
)

ci.fuzz_builder(
    name = "Win ASan Release Media",
    builderless = False,
    console_view_entry = ci.console_view_entry(
        category = "win asan",
        short_name = "med",
    ),
    os = os.WINDOWS_DEFAULT,
    triggering_policy = scheduler.greedy_batching(
        max_concurrent_invocations = 6,
    ),
)

ci.fuzz_libfuzzer_builder(
    name = "Libfuzzer Upload Chrome OS ASan",
    console_view_entry = ci.console_view_entry(
        category = "libfuzz",
        short_name = "chromeos-asan",
    ),
    triggering_policy = scheduler.greedy_batching(
        max_concurrent_invocations = 3,
    ),
)

ci.fuzz_libfuzzer_builder(
    name = "Libfuzzer Upload Linux ASan",
    console_view_entry = ci.console_view_entry(
        category = "libfuzz",
        short_name = "linux",
    ),
    triggering_policy = scheduler.greedy_batching(
        max_concurrent_invocations = 5,
    ),
)

ci.fuzz_libfuzzer_builder(
    name = "Libfuzzer Upload Linux ASan Debug",
    console_view_entry = ci.console_view_entry(
        category = "libfuzz",
        short_name = "linux-dbg",
    ),
    triggering_policy = scheduler.greedy_batching(
        max_concurrent_invocations = 5,
    ),
)

ci.fuzz_libfuzzer_builder(
    name = "Libfuzzer Upload Linux MSan",
    console_view_entry = ci.console_view_entry(
        category = "libfuzz",
        short_name = "linux-msan",
    ),
    triggering_policy = scheduler.greedy_batching(
        max_concurrent_invocations = 5,
    ),
)

ci.fuzz_libfuzzer_builder(
    name = "Libfuzzer Upload Linux UBSan",
    # Do not use builderless for this (crbug.com/980080).
    builderless = False,
    console_view_entry = ci.console_view_entry(
        category = "libfuzz",
        short_name = "linux-ubsan",
    ),
    execution_timeout = 3 * time.hour + 30 * time.minute,
    triggering_policy = scheduler.greedy_batching(
        max_concurrent_invocations = 5,
    ),
)

ci.fuzz_libfuzzer_builder(
    name = "Libfuzzer Upload Linux V8-ARM64 ASan",
    console_view_entry = ci.console_view_entry(
        category = "libfuzz",
        short_name = "arm64",
    ),
    triggering_policy = scheduler.greedy_batching(
        max_concurrent_invocations = 1,
    ),
)

ci.fuzz_libfuzzer_builder(
    name = "Libfuzzer Upload Linux V8-ARM64 ASan Debug",
    console_view_entry = ci.console_view_entry(
        category = "libfuzz",
        short_name = "arm64-dbg",
    ),
    triggering_policy = scheduler.greedy_batching(
        max_concurrent_invocations = 1,
    ),
)

ci.fuzz_libfuzzer_builder(
    name = "Libfuzzer Upload Linux32 ASan",
    console_view_entry = ci.console_view_entry(
        category = "libfuzz",
        short_name = "linux32",
    ),
    triggering_policy = scheduler.greedy_batching(
        max_concurrent_invocations = 3,
    ),
)

ci.fuzz_libfuzzer_builder(
    name = "Libfuzzer Upload Linux32 ASan Debug",
    console_view_entry = ci.console_view_entry(
        category = "libfuzz",
        short_name = "linux32-dbg",
    ),
    triggering_policy = scheduler.greedy_batching(
        max_concurrent_invocations = 3,
    ),
)

ci.fuzz_libfuzzer_builder(
    name = "Libfuzzer Upload Linux32 V8-ARM ASan",
    console_view_entry = ci.console_view_entry(
        category = "libfuzz",
        short_name = "arm",
    ),
    triggering_policy = scheduler.greedy_batching(
        max_concurrent_invocations = 1,
    ),
)

ci.fuzz_libfuzzer_builder(
    name = "Libfuzzer Upload Linux32 V8-ARM ASan Debug",
    console_view_entry = ci.console_view_entry(
        category = "libfuzz",
        short_name = "arm-dbg",
    ),
    triggering_policy = scheduler.greedy_batching(
        max_concurrent_invocations = 1,
    ),
)

ci.fuzz_libfuzzer_builder(
    name = "Libfuzzer Upload Mac ASan",
    console_view_entry = ci.console_view_entry(
        category = "libfuzz",
        short_name = "mac-asan",
    ),
    cores = 24,
    execution_timeout = 4 * time.hour,
    os = os.MAC_DEFAULT,
)

ci.fuzz_libfuzzer_builder(
    name = "Libfuzzer Upload Windows ASan",
    console_view_entry = ci.console_view_entry(
        category = "libfuzz",
        short_name = "win-asan",
    ),
    os = os.WINDOWS_DEFAULT,
    triggering_policy = scheduler.greedy_batching(
        max_concurrent_invocations = 3,
    ),
)

ci.fyi_builder(
    name = "Closure Compilation Linux",
    console_view_entry = ci.console_view_entry(
        category = "closure_compilation",
    ),
    executable = "recipe:closure_compilation",
    notifies = ["Closure Compilation Linux"],
)

ci.fyi_builder(
    name = "Linux Viz",
    console_view_entry = ci.console_view_entry(
        category = "viz",
    ),
)

ci.fyi_builder(
    name = "Linux remote_run Builder",
    console_view_entry = ci.console_view_entry(
        category = "remote_run",
    ),
)

ci.fyi_builder(
    name = "Linux remote_run Tester",
    console_view_entry = ci.console_view_entry(
        category = "remote_run",
    ),
    triggered_by = ["Linux remote_run Builder"],
)

ci.fyi_builder(
    name = "Site Isolation Android",
    console_view_entry = ci.console_view_entry(
        category = "site_isolation",
    ),
    notifies = ["Site Isolation Android"],
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

ci.fyi_builder(
    name = "chromeos-amd64-generic-lacros-rel",
    console_view_entry = ci.console_view_entry(
        category = "chromeos",
    ),
    properties = {
        # The format of these properties is defined at archive/properties.proto
        "$build/archive": {
            "archive_datas": [
                # The list of files and dirs should be synched with
                # _TRACKED_ITEMS in //build/lacros/lacros_resource_sizes.py.
                {
                    "files": [
                        "chrome",
                        "chrome_100_percent.pak",
                        "chrome_200_percent.pak",
                        "crashpad_handler",
                        "headless_lib.pak",
                        "icudtl.dat",
                        "nacl_helper",
                        "nacl_irt_x86_64.nexe",
                        "resources.pak",
                        "snapshot_blob.bin",
                    ],
                    "dirs": ["locales", "swiftshader"],
                    "gcs_bucket": "chromium-lacros-fishfood",
                    "gcs_path": "x86_64/{%position%}/lacros.zip",
                    "archive_type": "ARCHIVE_TYPE_ZIP",
                },
            ],
        },
    },
)

ci.fyi_builder(
    name = "fuchsia-fyi-arm64-dbg",
    console_view_entry = ci.console_view_entry(
        category = "fuchsia|a64",
        short_name = "dbg",
    ),
    notifies = ["cr-fuchsia"],
)

ci.fyi_builder(
    name = "fuchsia-fyi-arm64-rel",
    console_view_entry = ci.console_view_entry(
        category = "fuchsia|a64",
        short_name = "rel",
    ),
    notifies = ["cr-fuchsia"],
)

ci.fyi_builder(
    name = "fuchsia-fyi-x64-dbg",
    console_view_entry = ci.console_view_entry(
        category = "fuchsia|x64",
        short_name = "dbg",
    ),
    notifies = ["cr-fuchsia"],
)

ci.fyi_builder(
    name = "fuchsia-fyi-x64-rel",
    console_view_entry = ci.console_view_entry(
        category = "fuchsia|x64",
        short_name = "rel",
    ),
    notifies = ["cr-fuchsia"],
)

ci.fyi_builder(
    name = "linux-annotator-rel",
    console_view_entry = ci.console_view_entry(
        category = "network|traffic|annotations",
        short_name = "lnx",
    ),
    notifies = ["annotator-rel"],
)

ci.fyi_builder(
    name = "linux-ash-chromium-builder-fyi-rel",
    console_view_entry = ci.console_view_entry(
        category = "default",
        short_name = "lcr",
    ),
    properties = {
        # The format of these properties is defined at archive/properties.proto
        "$build/archive": {
            "archive_datas": [
                {
                    "files": [
                        "chrome",
                        "chrome_100_percent.pak",
                        "chrome_200_percent.pak",
                        "crashpad_handler",
                        "headless_lib.pak",
                        "icudtl.dat",
                        "libminigbm.so",
                        "nacl_helper",
                        "nacl_irt_x86_64.nexe",
                        "resources.pak",
                        "snapshot_blob.bin",
                    ],
                    "dirs": ["locales", "swiftshader"],
                    "gcs_bucket": "ash-chromium-on-linux-prebuilts",
                    "gcs_path": "x86_64/{%position%}/ash-chromium.zip",
                    "archive_type": "ARCHIVE_TYPE_ZIP",
                    "latest_upload": {
                        "gcs_path": "x86_64/latest/ash-chromium.txt",
                        "gcs_file_content": "{%position%}",
                    },
                },
            ],
        },
    },
)

ci.fyi_builder(
    name = "linux-blink-animation-use-time-delta",
    console_view_entry = ci.console_view_entry(
        category = "linux|blink",
        short_name = "TD",
    ),
)

ci.fyi_builder(
    name = "linux-blink-heap-concurrent-marking-tsan-rel",
    console_view_entry = ci.console_view_entry(
        category = "linux|blink",
        short_name = "CM",
    ),
)

ci.fyi_builder(
    name = "linux-blink-heap-verification",
    console_view_entry = ci.console_view_entry(
        category = "linux|blink",
        short_name = "VF",
    ),
    notifies = ["linux-blink-heap-verification"],
)

# For testing impact of builderful: https://crbug.com/1123673
# remove by 2020-10-05 gatong
ci.fyi_builder(
    name = "linux-builderful-fast-fyi-rel",
    console_view_entry = ci.console_view_entry(
        category = "build test",
        short_name = "bff",
    ),
    builderless = False,
)

ci.fyi_builder(
    name = "linux-builderful-slow-fyi-rel",
    console_view_entry = ci.console_view_entry(
        category = "build test",
        short_name = "bfs",
    ),
    builderless = False,
    schedule = "with 2h interval",
    triggered_by = [],
)

ci.fyi_builder(
    name = "linux-builderless-fast-fyi-rel",
    console_view_entry = ci.console_view_entry(
        category = "build test",
        short_name = "blf",
    ),
)

ci.fyi_builder(
    name = "linux-builderless-slow-fyi-rel",
    console_view_entry = ci.console_view_entry(
        category = "build test",
        short_name = "bls",
    ),
    schedule = "with 2h interval",
    triggered_by = [],
)

ci.fyi_builder(
    name = "linux-chromium-tests-staging-builder",
    console_view_entry = ci.console_view_entry(
        category = "recipe|staging|linux",
        short_name = "bld",
    ),
)

ci.fyi_builder(
    name = "linux-chromium-tests-staging-tests",
    console_view_entry = ci.console_view_entry(
        category = "recipe|staging|linux",
        short_name = "tst",
    ),
    triggered_by = ["linux-chromium-tests-staging-builder"],
)

ci.fyi_builder(
    name = "linux-inverse-fieldtrials-fyi-rel",
    console_view_entry = ci.console_view_entry(
        category = "linux",
    ),
)

ci.fyi_builder(
    name = "linux-fieldtrial-rel",
    console_view_entry = ci.console_view_entry(
        category = "linux",
    ),
)

ci.fyi_builder(
    name = "linux-lacros-builder-fyi-rel",
    console_view_entry = ci.console_view_entry(
        category = "linux",
    ),
)

ci.fyi_builder(
    name = "linux-lacros-tester-fyi-rel",
    console_view_entry = ci.console_view_entry(
        category = "linux",
    ),
    triggered_by = ["linux-lacros-builder-fyi-rel"],
)

ci.fyi_builder(
    name = "linux-perfetto-rel",
    console_view_entry = ci.console_view_entry(
        category = "linux",
    ),
)

ci.fyi_builder(
    name = "linux-wpt-fyi-rel",
    console_view_entry = ci.console_view_entry(
        category = "linux",
    ),
    experimental = True,
    goma_backend = goma.backend.RBE_PROD,
)

ci.fyi_builder(
    name = "linux-wpt-identity-fyi-rel",
    console_view_entry = ci.console_view_entry(
        category = "linux",
    ),
    experimental = True,
    goma_backend = goma.backend.RBE_PROD,
)

ci.fyi_builder(
    name = "linux-wpt-payments-fyi-rel",
    console_view_entry = ci.console_view_entry(
        category = "linux",
    ),
    experimental = True,
    goma_backend = goma.backend.RBE_PROD,
)

# This is launching & collecting entirely isolated tests.
# OS shouldn't matter.
ci.fyi_builder(
    name = "mac-osxbeta-rel",
    console_view_entry = ci.console_view_entry(
        category = "mac",
        short_name = "beta",
    ),
    goma_backend = goma.backend.RBE_PROD,
    main_console_view = None,
    triggered_by = ["ci/Mac Builder"],
)

ci.fyi_builder(
    name = "mac-omaha-builder-rel",
    console_view_entry = ci.console_view_entry(
        category = "updater|mac",
        short_name = "bld",
    ),
    os = os.MAC_ANY,
    cpu = cpu.X86_64,
    builderless = True,
    cores = None,
)

ci.fyi_builder(
    name = "mac10.10-omaha-tester-rel",
    console_view_entry = ci.console_view_entry(
        category = "updater|mac",
        short_name = "10.10",
    ),
    triggered_by = ["mac-omaha-builder-rel"],
)

ci.fyi_builder(
    name = "mac10.11-omaha-tester-rel",
    console_view_entry = ci.console_view_entry(
        category = "updater|mac",
        short_name = "10.11",
    ),
    triggered_by = ["mac-omaha-builder-rel"],
)

ci.fyi_builder(
    name = "mac10.12-omaha-tester-rel",
    console_view_entry = ci.console_view_entry(
        category = "updater|mac",
        short_name = "10.12",
    ),
    triggered_by = ["mac-omaha-builder-rel"],
)

ci.fyi_builder(
    name = "mac10.13-omaha-tester-rel",
    console_view_entry = ci.console_view_entry(
        category = "updater|mac",
        short_name = "10.13",
    ),
    triggered_by = ["mac-omaha-builder-rel"],
)

ci.fyi_builder(
    name = "mac10.14-omaha-tester-rel",
    console_view_entry = ci.console_view_entry(
        category = "updater|mac",
        short_name = "10.14",
    ),
    triggered_by = ["mac-omaha-builder-rel"],
)

ci.fyi_builder(
    name = "mac10.15-omaha-tester-rel",
    console_view_entry = ci.console_view_entry(
        category = "updater|mac",
        short_name = "10.15",
    ),
    triggered_by = ["mac-omaha-builder-rel"],
)

ci.fyi_builder(
    name = "win-omaha-builder-rel",
    console_view_entry = ci.console_view_entry(
        category = "updater|win",
        short_name = "bld",
    ),
    os = os.WINDOWS_DEFAULT,
    cpu = cpu.X86,
)

ci.fyi_builder(
    name = "win7-omaha-tester-rel",
    console_view_entry = ci.console_view_entry(
        category = "updater|win",
        short_name = "7",
    ),
    triggered_by = ["win-omaha-builder-rel"],
)

ci.fyi_builder(
    name = "win10-omaha-tester-rel",
    console_view_entry = ci.console_view_entry(
        category = "updater|win",
        short_name = "10",
    ),
    os = os.WINDOWS_10,
    triggered_by = ["win-omaha-builder-rel"],
)

ci.fyi_builder(
    name = "win-pixel-builder-rel",
    console_view_entry = ci.console_view_entry(
        category = "win10",
    ),
    os = os.WINDOWS_10,
)

ci.fyi_builder(
    name = "win-pixel-tester-rel",
    console_view_entry = ci.console_view_entry(
        category = "win10",
    ),
    os = None,
    triggered_by = ["win-pixel-builder-rel"],
)

ci.fyi_builder(
    name = "linux-upload-perfetto",
    console_view_entry = ci.console_view_entry(
        category = "perfetto",
        short_name = "lnx",
    ),
    os = os.LINUX_DEFAULT,
)

ci.fyi_builder(
    name = "mac-upload-perfetto",
    builderless = True,
    console_view_entry = ci.console_view_entry(
        category = "perfetto",
        short_name = "mac",
    ),
    os = os.MAC_DEFAULT,
    schedule = "with 3h interval",
    triggered_by = [],
)

ci.fyi_builder(
    name = "win-upload-perfetto",
    builderless = True,
    console_view_entry = ci.console_view_entry(
        category = "perfetto",
        short_name = "win",
    ),
    os = os.WINDOWS_DEFAULT,
    schedule = "with 3h interval",
    triggered_by = [],
)

ci.fyi_celab_builder(
    name = "win-celab-builder-rel",
    console_view_entry = ci.console_view_entry(
        category = "celab",
    ),
    schedule = "0 0,6,12,18 * * *",
    triggered_by = [],
)

ci.fyi_celab_builder(
    name = "win-celab-tester-rel",
    console_view_entry = ci.console_view_entry(
        category = "celab",
    ),
    triggered_by = ["win-celab-builder-rel"],
)

ci.fyi_coverage_builder(
    name = "android-code-coverage",
    console_view_entry = ci.console_view_entry(
        category = "code_coverage",
        short_name = "and",
    ),
    use_java_coverage = True,
    schedule = "triggered",
    triggered_by = [],
)

ci.fyi_coverage_builder(
    name = "android-code-coverage-native",
    console_view_entry = ci.console_view_entry(
        category = "code_coverage",
        short_name = "ann",
    ),
    use_clang_coverage = True,
)

ci.fyi_coverage_builder(
    name = "ios-simulator-code-coverage",
    caches = [xcode_cache.x12a7209],
    console_view_entry = ci.console_view_entry(
        category = "code_coverage",
        short_name = "ios",
    ),
    cores = None,
    os = os.MAC_10_15,
    use_clang_coverage = True,
    coverage_exclude_sources = "ios_test_files_and_test_utils",
    coverage_test_types = ["overall", "unit"],
    properties = {
        "xcode_build_version": "12a7209",
    },
)

ci.fyi_coverage_builder(
    name = "linux-chromeos-code-coverage",
    console_view_entry = ci.console_view_entry(
        category = "code_coverage",
        short_name = "lcr",
    ),
    use_clang_coverage = True,
    schedule = "triggered",
    triggered_by = [],
)

ci.fyi_coverage_builder(
    name = "linux-code-coverage",
    console_view_entry = ci.console_view_entry(
        category = "code_coverage",
        short_name = "lnx",
    ),
    use_clang_coverage = True,
    triggered_by = [],
)

ci.fyi_coverage_builder(
    name = "mac-code-coverage",
    builderless = True,
    console_view_entry = ci.console_view_entry(
        category = "code_coverage",
        short_name = "mac",
    ),
    cores = 24,
    os = os.MAC_ANY,
    use_clang_coverage = True,
)

ci.fyi_coverage_builder(
    name = "win10-code-coverage",
    builderless = True,
    console_view_entry = ci.console_view_entry(
        category = "code_coverage",
        short_name = "win",
    ),
    os = os.WINDOWS_DEFAULT,
    use_clang_coverage = True,
)

ci.fyi_ios_builder(
    name = "ios-asan",
    console_view_entry = ci.console_view_entry(
        category = "iOS",
        short_name = "asan",
    ),
)

ci.fyi_ios_builder(
    name = "ios-simulator-cr-recipe",
    console_view_entry = ci.console_view_entry(
        category = "iOS",
        short_name = "chr",
    ),
)

ci.fyi_ios_builder(
    name = "ios-simulator-cronet",
    branch_selector = branches.STANDARD_RELEASES,
    caches = [xcode_cache.x11e146],
    console_view_entry = ci.console_view_entry(
        category = "cronet",
    ),
    cq_mirrors_console_view = settings.cq_mirrors_console_name,
    main_console_view = main_console_if_on_branch(),
    notifies = ["cronet"],
    properties = {
        "xcode_build_version": "11e146",
    },
)

ci.fyi_ios_builder(
    name = "ios-simulator-multi-window",
    console_view_entry = ci.console_view_entry(
        category = "iOS",
        short_name = "mwd",
    ),
)

ci.fyi_ios_builder(
    name = "ios-webkit-tot",
    caches = [xcode_cache.x11e608cwk],
    console_view_entry = ci.console_view_entry(
        category = "iOS",
        short_name = "wk",
    ),
    properties = {
        "xcode_build_version": "11e608cwk",
    },
    schedule = "0 1-23/6 * * *",
    triggered_by = [],
)

ci.fyi_ios_builder(
    name = "ios13-beta-simulator",
    console_view_entry = ci.console_view_entry(
        category = "iOS|iOS13",
        short_name = "ios13",
    ),
    schedule = "0 0,12 * * *",
    triggered_by = [],
)

ci.fyi_ios_builder(
    name = "ios13-sdk-device",
    console_view_entry = ci.console_view_entry(
        category = "iOS|iOS13",
        short_name = "dev",
    ),
)

ci.fyi_ios_builder(
    name = "ios13-sdk-simulator",
    console_view_entry = ci.console_view_entry(
        category = "iOS|iOS13",
        short_name = "sdk13",
    ),
    schedule = "0 6,18 * * *",
    triggered_by = [],
)

ci.fyi_ios_builder(
    name = "ios14-beta-simulator",
    console_view_entry = ci.console_view_entry(
        category = "iOS|iOS14",
        short_name = "ios14",
    ),
)

ci.fyi_ios_builder(
    name = "ios14-sdk-simulator",
    console_view_entry = ci.console_view_entry(
        category = "iOS|iOS14",
        short_name = "sdk14",
    ),
    caches = [xcode_cache.x12b5018i],
    properties = {
        "xcode_build_version": "12b5018i",
    },
)

ci.fyi_mac_builder(
    name = "Mac Builder Next",
    console_view_entry = ci.console_view_entry(
        category = "mac",
        short_name = "bld",
    ),
    cores = None,
    os = None,
)

ci.thin_tester(
    name = "Mac11.0 Tests",
    builder_group = "chromium.fyi",
    console_view_entry = ci.console_view_entry(
        category = "mac",
        short_name = "11.0",
    ),
    triggered_by = ["Mac Builder Next"],
)

ci.fyi_mac_builder(
    name = "Mac deterministic",
    console_view_entry = ci.console_view_entry(
        category = "deterministic|mac",
        short_name = "rel",
    ),
    cores = None,
    executable = "recipe:swarming/deterministic_build",
    execution_timeout = 6 * time.hour,
)

ci.fyi_mac_builder(
    name = "Mac deterministic (dbg)",
    console_view_entry = ci.console_view_entry(
        category = "deterministic|mac",
        short_name = "dbg",
    ),
    cores = None,
    executable = "recipe:swarming/deterministic_build",
    execution_timeout = 6 * time.hour,
    os = os.MAC_10_15,
)

ci.fyi_mac_builder(
    name = "mac-hermetic-upgrade-rel",
    console_view_entry = ci.console_view_entry(
        category = "mac",
        short_name = "herm",
    ),
    cores = 8,
)

ci.fyi_windows_builder(
    name = "Win10 Tests x64 1803",
    console_view_entry = ci.console_view_entry(
        category = "win10|1803",
    ),
    goma_backend = None,
    main_console_view = None,
    os = os.WINDOWS_10,
    triggered_by = ["ci/Win x64 Builder"],
)

ci.fyi_windows_builder(
    name = "Win10 Tests x64 1909",
    console_view_entry = ci.console_view_entry(
        category = "win10|1909",
    ),
    goma_backend = None,
    main_console_view = None,
    os = os.WINDOWS_10,
    triggered_by = ["ci/Win x64 Builder"],
)

ci.fyi_windows_builder(
    name = "Win 10 Fast Ring",
    console_view_entry = ci.console_view_entry(
        category = "win10",
    ),
    os = os.WINDOWS_10,
    notifies = ["Win 10 Fast Ring"],
)

ci.fyi_windows_builder(
    name = "win32-arm64-rel",
    console_view_entry = ci.console_view_entry(
        category = "win32|arm64",
    ),
    cpu = cpu.X86,
    goma_jobs = goma.jobs.J150,
)

ci.fyi_windows_builder(
    name = "win-annotator-rel",
    builderless = True,
    console_view_entry = ci.console_view_entry(
        category = "network|traffic|annotations",
        short_name = "win",
    ),
    execution_timeout = 16 * time.hour,
    notifies = ["annotator-rel"],
)

ci.gpu_linux_builder(
    name = "Android Release (Nexus 5X)",
    branch_selector = branches.STANDARD_RELEASES,
    console_view_entry = ci.console_view_entry(
        category = "Android",
    ),
    cq_mirrors_console_view = settings.cq_mirrors_console_name,
    main_console_view = main_console_if_on_branch(),
)

ci.gpu_linux_builder(
    name = "GPU Linux Builder",
    branch_selector = branches.STANDARD_RELEASES,
    console_view_entry = ci.console_view_entry(
        category = "Linux",
    ),
    cq_mirrors_console_view = settings.cq_mirrors_console_name,
    main_console_view = main_console_if_on_branch(),
)

ci.gpu_linux_builder(
    name = "GPU Linux Builder (dbg)",
    console_view_entry = ci.console_view_entry(
        category = "Linux",
    ),
    tree_closing = False,
)

ci.gpu_mac_builder(
    name = "GPU Mac Builder",
    branch_selector = branches.STANDARD_RELEASES,
    console_view_entry = ci.console_view_entry(
        category = "Mac",
    ),
    cq_mirrors_console_view = settings.cq_mirrors_console_name,
    main_console_view = main_console_if_on_branch(),
)

ci.gpu_mac_builder(
    name = "GPU Mac Builder (dbg)",
    console_view_entry = ci.console_view_entry(
        category = "Mac",
    ),
    tree_closing = False,
)

ci.gpu_windows_builder(
    name = "GPU Win x64 Builder",
    branch_selector = branches.STANDARD_RELEASES,
    console_view_entry = ci.console_view_entry(
        category = "Windows",
    ),
    cq_mirrors_console_view = settings.cq_mirrors_console_name,
    main_console_view = main_console_if_on_branch(),
)

ci.gpu_windows_builder(
    name = "GPU Win x64 Builder (dbg)",
    console_view_entry = ci.console_view_entry(
        category = "Windows",
    ),
    tree_closing = False,
)

ci.gpu_thin_tester(
    name = "Linux Debug (NVIDIA)",
    console_view_entry = ci.console_view_entry(
        category = "Linux",
    ),
    triggered_by = ["GPU Linux Builder (dbg)"],
    tree_closing = False,
)

ci.gpu_thin_tester(
    name = "Linux Release (NVIDIA)",
    branch_selector = branches.STANDARD_RELEASES,
    cq_mirrors_console_view = settings.cq_mirrors_console_name,
    console_view_entry = ci.console_view_entry(
        category = "Linux",
    ),
    main_console_view = main_console_if_on_branch(),
    triggered_by = [builder_name("GPU Linux Builder")],
)

ci.gpu_thin_tester(
    name = "Mac Debug (Intel)",
    console_view_entry = ci.console_view_entry(
        category = "Mac",
    ),
    triggered_by = ["GPU Mac Builder (dbg)"],
    tree_closing = False,
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
    name = "Mac Retina Debug (AMD)",
    console_view_entry = ci.console_view_entry(
        category = "Mac",
    ),
    triggered_by = ["GPU Mac Builder (dbg)"],
    tree_closing = False,
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
    name = "Win10 x64 Debug (NVIDIA)",
    console_view_entry = ci.console_view_entry(
        category = "Windows",
    ),
    triggered_by = ["GPU Win x64 Builder (dbg)"],
    tree_closing = False,
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

ci.gpu_fyi_linux_builder(
    name = "Android FYI 32 Vk Release (Pixel 2)",
    console_view_entry = ci.console_view_entry(
        category = "Android|vk|Q32",
        short_name = "P2",
    ),
)

ci.gpu_fyi_linux_builder(
    name = "Android FYI 32 dEQP Vk Release (Pixel 2)",
    console_view_entry = ci.console_view_entry(
        category = "Android|dqp|vk|Q32",
        short_name = "P2",
    ),
)

ci.gpu_fyi_thin_tester(
    name = "Android FYI 64 Perf (Pixel 2)",
    console_view_entry = ci.console_view_entry(
        category = "Android|Perf|Q64",
        short_name = "P2",
    ),
    triggered_by = ["GPU FYI Perf Android 64 Builder"],
)

ci.gpu_fyi_linux_builder(
    name = "Android FYI 64 Vk Release (Pixel 2)",
    console_view_entry = ci.console_view_entry(
        category = "Android|vk|Q64",
        short_name = "P2",
    ),
)

ci.gpu_fyi_linux_builder(
    name = "Android FYI 64 dEQP Vk Release (Pixel 2)",
    console_view_entry = ci.console_view_entry(
        category = "Android|dqp|vk|Q64",
        short_name = "P2",
    ),
)

ci.gpu_fyi_linux_builder(
    name = "Android FYI Release (NVIDIA Shield TV)",
    console_view_entry = ci.console_view_entry(
        category = "Android|N64|NVDA",
        short_name = "STV",
    ),
)

ci.gpu_fyi_linux_builder(
    name = "Android FYI Release (Nexus 5)",
    console_view_entry = ci.console_view_entry(
        category = "Android|L32",
        short_name = "N5",
    ),
)

ci.gpu_fyi_linux_builder(
    name = "Android FYI Release (Nexus 5X)",
    console_view_entry = ci.console_view_entry(
        category = "Android|M64|QCOM",
        short_name = "N5X",
    ),
)

ci.gpu_fyi_linux_builder(
    name = "Android FYI Release (Nexus 6)",
    console_view_entry = ci.console_view_entry(
        category = "Android|L32",
        short_name = "N6",
    ),
)

ci.gpu_fyi_linux_builder(
    name = "Android FYI Release (Nexus 6P)",
    console_view_entry = ci.console_view_entry(
        category = "Android|M64|QCOM",
        short_name = "N6P",
    ),
)

ci.gpu_fyi_linux_builder(
    name = "Android FYI Release (Nexus 9)",
    console_view_entry = ci.console_view_entry(
        category = "Android|M64|NVDA",
        short_name = "N9",
    ),
)

ci.gpu_fyi_linux_builder(
    name = "Android FYI Release (Pixel 2)",
    console_view_entry = ci.console_view_entry(
        category = "Android|P32|QCOM",
        short_name = "P2",
    ),
)

ci.gpu_fyi_linux_builder(
    name = "Android FYI SkiaRenderer GL (Nexus 5X)",
    console_view_entry = ci.console_view_entry(
        category = "Android|skgl|M64",
        short_name = "N5X",
    ),
)

ci.gpu_fyi_linux_builder(
    name = "Android FYI SkiaRenderer Vulkan (Pixel 2)",
    console_view_entry = ci.console_view_entry(
        category = "Android|skv|P32",
        short_name = "P2",
    ),
)

ci.gpu_fyi_linux_builder(
    name = "Android FYI dEQP Release (Nexus 5X)",
    console_view_entry = ci.console_view_entry(
        category = "Android|dqp|M64",
        short_name = "N5X",
    ),
)

ci.gpu_fyi_linux_builder(
    name = "ChromeOS FYI Release (amd64-generic)",
    console_view_entry = ci.console_view_entry(
        category = "ChromeOS|amd64|generic",
        short_name = "x64",
    ),
)

ci.gpu_fyi_linux_builder(
    name = "ChromeOS FYI Release (kevin)",
    console_view_entry = ci.console_view_entry(
        category = "ChromeOS|arm|kevin",
        short_name = "kvn",
    ),
)

ci.gpu_fyi_linux_builder(
    name = "GPU FYI Linux Builder",
    console_view_entry = ci.console_view_entry(
        category = "Linux|Builder",
        short_name = "rel",
    ),
)

ci.gpu_fyi_linux_builder(
    name = "GPU FYI Linux Builder (dbg)",
    console_view_entry = ci.console_view_entry(
        category = "Linux|Builder",
        short_name = "dbg",
    ),
)

ci.gpu_fyi_linux_builder(
    name = "GPU FYI Linux Ozone Builder",
    console_view_entry = ci.console_view_entry(
        category = "Linux|Builder",
        short_name = "ozn",
    ),
)

ci.gpu_fyi_linux_builder(
    name = "GPU FYI Linux dEQP Builder",
    console_view_entry = ci.console_view_entry(
        category = "Linux|Builder",
        short_name = "dqp",
    ),
)

ci.gpu_fyi_linux_builder(
    name = "GPU FYI Perf Android 64 Builder",
    console_view_entry = ci.console_view_entry(
        category = "Android|Perf|Builder",
        short_name = "64",
    ),
)

ci.gpu_fyi_linux_builder(
    name = "Linux FYI GPU TSAN Release",
    console_view_entry = ci.console_view_entry(
        category = "Linux",
        short_name = "tsn",
    ),
)

# Builder + tester.
ci.gpu_fyi_linux_builder(
    name = "Linux FYI SkiaRenderer Dawn Release (Intel HD 630)",
    console_view_entry = ci.console_view_entry(
        category = "Linux|Intel",
        short_name = "skd",
    ),
)

ci.gpu_fyi_mac_builder(
    name = "Mac FYI arm64 Release (Apple DTK)",
    console_view_entry = ci.console_view_entry(
        category = "Mac",
        short_name = "dtk",
    ),
)

ci.gpu_fyi_mac_builder(
    name = "Mac FYI GPU ASAN Release",
    console_view_entry = ci.console_view_entry(
        category = "Mac",
        short_name = "asn",
    ),
)

ci.gpu_fyi_mac_builder(
    name = "GPU FYI Mac Builder",
    console_view_entry = ci.console_view_entry(
        category = "Mac|Builder",
        short_name = "rel",
    ),
)

ci.gpu_fyi_mac_builder(
    name = "GPU FYI Mac Builder (dbg)",
    console_view_entry = ci.console_view_entry(
        category = "Mac|Builder",
        short_name = "dbg",
    ),
)

ci.gpu_fyi_mac_builder(
    name = "GPU FYI Mac dEQP Builder",
    console_view_entry = ci.console_view_entry(
        category = "Mac|Builder",
        short_name = "dqp",
    ),
)

ci.gpu_fyi_thin_tester(
    name = "Linux FYI Debug (NVIDIA)",
    console_view_entry = ci.console_view_entry(
        category = "Linux|Nvidia",
        short_name = "dbg",
    ),
    triggered_by = ["GPU FYI Linux Builder (dbg)"],
)

ci.gpu_fyi_thin_tester(
    name = "Linux FYI Experimental Release (Intel HD 630)",
    console_view_entry = ci.console_view_entry(
        category = "Linux|Intel",
        short_name = "exp",
    ),
    triggered_by = ["GPU FYI Linux Builder"],
)

ci.gpu_fyi_thin_tester(
    name = "Linux FYI Experimental Release (NVIDIA)",
    console_view_entry = ci.console_view_entry(
        category = "Linux|Nvidia",
        short_name = "exp",
    ),
    triggered_by = ["GPU FYI Linux Builder"],
)

ci.gpu_fyi_thin_tester(
    name = "Linux FYI Ozone (Intel)",
    console_view_entry = ci.console_view_entry(
        category = "Linux|Intel",
        short_name = "ozn",
    ),
    triggered_by = ["GPU FYI Linux Ozone Builder"],
)

ci.gpu_fyi_thin_tester(
    name = "Linux FYI Release (NVIDIA)",
    console_view_entry = ci.console_view_entry(
        category = "Linux|Nvidia",
        short_name = "rel",
    ),
    triggered_by = ["GPU FYI Linux Builder"],
)

ci.gpu_fyi_thin_tester(
    name = "Linux FYI Release (AMD R7 240)",
    console_view_entry = ci.console_view_entry(
        category = "Linux|AMD",
        short_name = "rel",
    ),
    triggered_by = ["GPU FYI Linux Builder"],
)

ci.gpu_fyi_thin_tester(
    name = "Linux FYI Release (Intel HD 630)",
    console_view_entry = ci.console_view_entry(
        category = "Linux|Intel",
        short_name = "rel",
    ),
    triggered_by = ["GPU FYI Linux Builder"],
)

ci.gpu_fyi_thin_tester(
    name = "Linux FYI Release (Intel UHD 630)",
    console_view_entry = ci.console_view_entry(
        category = "Linux|Intel",
        short_name = "uhd",
    ),
    # TODO(https://crbug.com/986939): Remove this increased timeout once more
    # devices are added.
    execution_timeout = 18 * time.hour,
    triggered_by = ["GPU FYI Linux Builder"],
)

ci.gpu_fyi_thin_tester(
    name = "Linux FYI SkiaRenderer Vulkan (Intel HD 630)",
    console_view_entry = ci.console_view_entry(
        category = "Linux|Intel",
        short_name = "skv",
    ),
    triggered_by = ["GPU FYI Linux Builder"],
)

ci.gpu_fyi_thin_tester(
    name = "Linux FYI SkiaRenderer Vulkan (NVIDIA)",
    console_view_entry = ci.console_view_entry(
        category = "Linux|Nvidia",
        short_name = "skv",
    ),
    triggered_by = ["GPU FYI Linux Builder"],
)

ci.gpu_fyi_thin_tester(
    name = "Linux FYI dEQP Release (Intel HD 630)",
    console_view_entry = ci.console_view_entry(
        category = "Linux|Intel",
        short_name = "dqp",
    ),
    triggered_by = ["GPU FYI Linux dEQP Builder"],
)

ci.gpu_fyi_thin_tester(
    name = "Linux FYI dEQP Release (NVIDIA)",
    console_view_entry = ci.console_view_entry(
        category = "Linux|Nvidia",
        short_name = "dqp",
    ),
    triggered_by = ["GPU FYI Linux dEQP Builder"],
)

ci.gpu_fyi_thin_tester(
    name = "Mac FYI Debug (Intel)",
    console_view_entry = ci.console_view_entry(
        category = "Mac|Intel",
        short_name = "dbg",
    ),
    triggered_by = ["GPU FYI Mac Builder (dbg)"],
)

ci.gpu_fyi_thin_tester(
    name = "Mac FYI Experimental Release (Intel)",
    console_view_entry = ci.console_view_entry(
        category = "Mac|Intel",
        short_name = "exp",
    ),
    triggered_by = ["GPU FYI Mac Builder"],
)

ci.gpu_fyi_thin_tester(
    name = "Mac FYI Experimental Retina Release (AMD)",
    console_view_entry = ci.console_view_entry(
        category = "Mac|AMD|Retina",
        short_name = "exp",
    ),
    triggered_by = ["GPU FYI Mac Builder"],
)

ci.gpu_fyi_thin_tester(
    name = "Mac FYI Experimental Retina Release (NVIDIA)",
    console_view_entry = ci.console_view_entry(
        category = "Mac|Nvidia",
        short_name = "exp",
    ),
    # This bot has one machine backing its tests at the moment.
    # If it gets more, this can be removed.
    # See crbug.com/853307 for more context.
    execution_timeout = 12 * time.hour,
    triggered_by = ["GPU FYI Mac Builder"],
)

ci.gpu_fyi_thin_tester(
    name = "Mac FYI Release (Intel)",
    console_view_entry = ci.console_view_entry(
        category = "Mac|Intel",
        short_name = "rel",
    ),
    triggered_by = ["GPU FYI Mac Builder"],
)

ci.gpu_fyi_thin_tester(
    name = "Mac FYI Retina Debug (AMD)",
    console_view_entry = ci.console_view_entry(
        category = "Mac|AMD|Retina",
        short_name = "dbg",
    ),
    triggered_by = ["GPU FYI Mac Builder (dbg)"],
)

ci.gpu_fyi_thin_tester(
    name = "Mac FYI Retina Debug (NVIDIA)",
    console_view_entry = ci.console_view_entry(
        category = "Mac|Nvidia",
        short_name = "dbg",
    ),
    triggered_by = ["GPU FYI Mac Builder (dbg)"],
)

ci.gpu_fyi_thin_tester(
    name = "Mac FYI Retina Release (AMD)",
    console_view_entry = ci.console_view_entry(
        category = "Mac|AMD|Retina",
        short_name = "rel",
    ),
    triggered_by = ["GPU FYI Mac Builder"],
)

ci.gpu_fyi_thin_tester(
    name = "Mac FYI Retina Release (NVIDIA)",
    console_view_entry = ci.console_view_entry(
        category = "Mac|Nvidia",
        short_name = "rel",
    ),
    triggered_by = ["GPU FYI Mac Builder"],
)

ci.gpu_fyi_thin_tester(
    name = "Mac FYI dEQP Release AMD",
    console_view_entry = ci.console_view_entry(
        category = "Mac|AMD",
        short_name = "dqp",
    ),
    triggered_by = ["GPU FYI Mac dEQP Builder"],
)

ci.gpu_fyi_thin_tester(
    name = "Mac FYI dEQP Release Intel",
    console_view_entry = ci.console_view_entry(
        category = "Mac|Intel",
        short_name = "dqp",
    ),
    triggered_by = ["GPU FYI Mac dEQP Builder"],
)

ci.gpu_fyi_thin_tester(
    name = "Mac Pro FYI Release (AMD)",
    console_view_entry = ci.console_view_entry(
        category = "Mac|AMD|Pro",
        short_name = "rel",
    ),
    triggered_by = ["GPU FYI Mac Builder"],
)

ci.gpu_fyi_thin_tester(
    name = "Win10 FYI x64 Debug (NVIDIA)",
    console_view_entry = ci.console_view_entry(
        category = "Windows|10|x64|Nvidia",
        short_name = "dbg",
    ),
    triggered_by = ["GPU FYI Win x64 Builder (dbg)"],
)

ci.gpu_fyi_thin_tester(
    name = "Win10 FYI x64 DX12 Vulkan Debug (NVIDIA)",
    console_view_entry = ci.console_view_entry(
        category = "Windows|10|x64|Nvidia|dx12vk",
        short_name = "dbg",
    ),
    triggered_by = ["GPU FYI Win x64 DX12 Vulkan Builder (dbg)"],
)

ci.gpu_fyi_thin_tester(
    name = "Win10 FYI x64 DX12 Vulkan Release (NVIDIA)",
    console_view_entry = ci.console_view_entry(
        category = "Windows|10|x64|Nvidia|dx12vk",
        short_name = "rel",
    ),
    triggered_by = ["GPU FYI Win x64 DX12 Vulkan Builder"],
)

ci.gpu_fyi_thin_tester(
    name = "Win10 FYI x64 Exp Release (Intel HD 630)",
    console_view_entry = ci.console_view_entry(
        category = "Windows|10|x64|Intel",
        short_name = "exp",
    ),
    triggered_by = ["GPU FYI Win x64 Builder"],
)

ci.gpu_fyi_thin_tester(
    name = "Win10 FYI x64 Exp Release (NVIDIA)",
    console_view_entry = ci.console_view_entry(
        category = "Windows|10|x64|Nvidia",
        short_name = "exp",
    ),
    triggered_by = ["GPU FYI Win x64 Builder"],
)

ci.gpu_fyi_thin_tester(
    name = "Win10 FYI x64 Release (AMD RX 550)",
    console_view_entry = ci.console_view_entry(
        category = "Windows|10|x64|AMD",
        short_name = "rel",
    ),
    triggered_by = ["GPU FYI Win x64 Builder"],
)

ci.gpu_fyi_thin_tester(
    name = "Win10 FYI x64 Release (Intel HD 630)",
    console_view_entry = ci.console_view_entry(
        category = "Windows|10|x64|Intel",
        short_name = "rel",
    ),
    triggered_by = ["GPU FYI Win x64 Builder"],
)

ci.gpu_fyi_thin_tester(
    name = "Win10 FYI x64 Release (Intel UHD 630)",
    console_view_entry = ci.console_view_entry(
        category = "Windows|10|x64|Intel",
        short_name = "uhd",
    ),
    # TODO(https://crbug.com/986939): Remove this increased timeout once
    # more devices are added.
    execution_timeout = 18 * time.hour,
    triggered_by = ["GPU FYI Win x64 Builder"],
)

ci.gpu_fyi_thin_tester(
    name = "Win10 FYI x64 Release (NVIDIA GeForce GTX 1660)",
    console_view_entry = ci.console_view_entry(
        category = "Windows|10|x64|Nvidia",
        short_name = "gtx",
    ),
    execution_timeout = 18 * time.hour,
    triggered_by = ["GPU FYI Win x64 Builder"],
)

ci.gpu_fyi_thin_tester(
    name = "Win10 FYI x64 Release (NVIDIA)",
    console_view_entry = ci.console_view_entry(
        category = "Windows|10|x64|Nvidia",
        short_name = "rel",
    ),
    triggered_by = ["GPU FYI Win x64 Builder"],
)

ci.gpu_fyi_thin_tester(
    name = "Win10 FYI x64 Release XR Perf (NVIDIA)",
    console_view_entry = ci.console_view_entry(
        category = "Windows|10|x64|Nvidia",
        short_name = "xr",
    ),
    triggered_by = ["GPU FYI XR Win x64 Builder"],
)

# Builder + tester.
ci.gpu_fyi_windows_builder(
    name = "Win10 FYI x64 SkiaRenderer Dawn Release (NVIDIA)",
    console_view_entry = ci.console_view_entry(
        category = "Windows|10|x64|Nvidia",
        short_name = "skd",
    ),
)

ci.gpu_fyi_thin_tester(
    name = "Win10 FYI x64 SkiaRenderer GL (NVIDIA)",
    console_view_entry = ci.console_view_entry(
        category = "Windows|10|x64|Nvidia",
        short_name = "skgl",
    ),
    triggered_by = ["GPU FYI Win x64 Builder"],
)

ci.gpu_fyi_thin_tester(
    name = "Win10 FYI x64 dEQP Release (Intel HD 630)",
    console_view_entry = ci.console_view_entry(
        category = "Windows|10|x64|Intel",
        short_name = "dqp",
    ),
    triggered_by = ["GPU FYI Win x64 dEQP Builder"],
)

ci.gpu_fyi_thin_tester(
    name = "Win10 FYI x64 dEQP Release (NVIDIA)",
    console_view_entry = ci.console_view_entry(
        category = "Windows|10|x64|Nvidia",
        short_name = "dqp",
    ),
    triggered_by = ["GPU FYI Win x64 dEQP Builder"],
)

ci.gpu_fyi_thin_tester(
    name = "Win10 FYI x86 Release (NVIDIA)",
    console_view_entry = ci.console_view_entry(
        category = "Windows|10|x86|Nvidia",
        short_name = "rel",
    ),
    triggered_by = ["GPU FYI Win Builder"],
)

ci.gpu_fyi_thin_tester(
    name = "Win7 FYI Debug (AMD)",
    console_view_entry = ci.console_view_entry(
        category = "Windows|7|x86|AMD",
        short_name = "dbg",
    ),
    triggered_by = ["GPU FYI Win Builder (dbg)"],
)

ci.gpu_fyi_thin_tester(
    name = "Win7 FYI Release (AMD)",
    console_view_entry = ci.console_view_entry(
        category = "Windows|7|x86|AMD",
        short_name = "rel",
    ),
    triggered_by = ["GPU FYI Win Builder"],
)

ci.gpu_fyi_thin_tester(
    name = "Win7 FYI Release (NVIDIA)",
    console_view_entry = ci.console_view_entry(
        category = "Windows|7|x86|Nvidia",
        short_name = "rel",
    ),
    triggered_by = ["GPU FYI Win Builder"],
)

ci.gpu_fyi_thin_tester(
    name = "Win7 FYI dEQP Release (AMD)",
    console_view_entry = ci.console_view_entry(
        category = "Windows|7|x86|AMD",
        short_name = "dqp",
    ),
    triggered_by = ["GPU FYI Win dEQP Builder"],
)

ci.gpu_fyi_thin_tester(
    name = "Win7 FYI x64 Release (NVIDIA)",
    console_view_entry = ci.console_view_entry(
        category = "Windows|7|x64|Nvidia",
        short_name = "rel",
    ),
    triggered_by = ["GPU FYI Win x64 Builder"],
)

ci.gpu_fyi_thin_tester(
    name = "Win7 FYI x64 dEQP Release (NVIDIA)",
    console_view_entry = ci.console_view_entry(
        category = "Windows|7|x64|Nvidia",
        short_name = "dqp",
    ),
    triggered_by = ["GPU FYI Win x64 dEQP Builder"],
)

ci.gpu_fyi_windows_builder(
    name = "GPU FYI Win Builder",
    console_view_entry = ci.console_view_entry(
        category = "Windows|Builder|Release",
        short_name = "x86",
    ),
)

ci.gpu_fyi_windows_builder(
    name = "GPU FYI Win Builder (dbg)",
    console_view_entry = ci.console_view_entry(
        category = "Windows|Builder|Debug",
        short_name = "x86",
    ),
)

ci.gpu_fyi_windows_builder(
    name = "GPU FYI Win dEQP Builder",
    console_view_entry = ci.console_view_entry(
        category = "Windows|Builder|dEQP",
        short_name = "x86",
    ),
)

ci.gpu_fyi_windows_builder(
    name = "GPU FYI Win x64 Builder",
    console_view_entry = ci.console_view_entry(
        category = "Windows|Builder|Release",
        short_name = "x64",
    ),
)

ci.gpu_fyi_windows_builder(
    name = "GPU FYI Win x64 Builder (dbg)",
    console_view_entry = ci.console_view_entry(
        category = "Windows|Builder|Debug",
        short_name = "x64",
    ),
)

ci.gpu_fyi_windows_builder(
    name = "GPU FYI Win x64 dEQP Builder",
    console_view_entry = ci.console_view_entry(
        category = "Windows|Builder|dEQP",
        short_name = "x64",
    ),
)

ci.gpu_fyi_windows_builder(
    name = "GPU FYI Win x64 DX12 Vulkan Builder",
    console_view_entry = ci.console_view_entry(
        category = "Windows|Builder|dx12vk",
        short_name = "rel",
    ),
)

ci.gpu_fyi_windows_builder(
    name = "GPU FYI Win x64 DX12 Vulkan Builder (dbg)",
    console_view_entry = ci.console_view_entry(
        category = "Windows|Builder|dx12vk",
        short_name = "dbg",
    ),
)

ci.gpu_fyi_windows_builder(
    name = "GPU FYI XR Win x64 Builder",
    console_view_entry = ci.console_view_entry(
        category = "Windows|Builder|XR",
        short_name = "x64",
    ),
)

ci.linux_builder(
    name = "Cast Audio Linux",
    console_view_entry = ci.console_view_entry(
        category = "cast",
        short_name = "aud",
    ),
    main_console_view = "main",
    ssd = True,
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
    name = "Deterministic Fuchsia (dbg)",
    console_view_entry = ci.console_view_entry(
        category = "fuchsia|x64",
        short_name = "det",
    ),
    executable = "recipe:swarming/deterministic_build",
    execution_timeout = 6 * time.hour,
    goma_jobs = None,
    main_console_view = "main",
)

ci.linux_builder(
    name = "Deterministic Linux",
    console_view_entry = ci.console_view_entry(
        category = "release",
        short_name = "det",
    ),
    executable = "recipe:swarming/deterministic_build",
    execution_timeout = 6 * time.hour,
    main_console_view = "main",
    # Set tree_closing to false to disable the defaualt tree closer, which
    # filters by step name, and instead enable tree closing for any step
    # failure.
    tree_closing = False,
    extra_notifies = ["Deterministic Linux", "close-on-any-step-failure"],
)

ci.linux_builder(
    name = "Deterministic Linux (dbg)",
    console_view_entry = ci.console_view_entry(
        category = "debug|builder",
        short_name = "det",
    ),
    cores = 32,
    executable = "recipe:swarming/deterministic_build",
    execution_timeout = 6 * time.hour,
    main_console_view = "main",
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
    name = "Leak Detection Linux",
    console_view = "chromium.fyi",
    console_view_entry = ci.console_view_entry(
        category = "linux",
        short_name = "lk",
    ),
    notifies = [],
    tree_closing = False,
)

ci.linux_builder(
    name = "Linux Builder",
    branch_selector = branches.STANDARD_RELEASES,
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
    name = "Linux Builder (dbg)(32)",
    console_view_entry = ci.console_view_entry(
        category = "debug|builder",
        short_name = "32",
    ),
    main_console_view = "main",
)

ci.linux_builder(
    name = "Linux Tests",
    branch_selector = branches.STANDARD_RELEASES,
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
    name = "Network Service Linux",
    console_view_entry = ci.console_view_entry(
        category = "release",
        short_name = "nsl",
    ),
    main_console_view = "main",
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
    name = "fuchsia-x64-dbg",
    console_view_entry = ci.console_view_entry(
        category = "fuchsia|x64",
        short_name = "dbg",
    ),
    main_console_view = "main",
    extra_notifies = ["cr-fuchsia"],
)

ci.linux_builder(
    name = "linux-bfcache-rel",
    console_view_entry = ci.console_view_entry(
        category = "bfcache",
        short_name = "bfc",
    ),
    main_console_view = "main",
)

ci.linux_builder(
    name = "linux-gcc-rel",
    console_view_entry = ci.console_view_entry(
        category = "release",
        short_name = "gcc",
    ),
    goma_backend = None,
    main_console_view = "main",
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

ci.linux_builder(
    # CI tester for Ozone/Headless
    name = "Linux Tester (Ozone Headless)",
    branch_selector = branches.STANDARD_RELEASES,
    console_view_entry = ci.console_view_entry(
        category = "release|ozone",
        short_name = "ltoh",
    ),
    main_console_view = "main",
    cq_mirrors_console_view = "mirrors",
    triggered_by = [builder_name("linux-ozone-rel")],
    tree_closing = False,
)

ci.linux_builder(
    # CI tester for Ozone/Wayland
    name = "Linux Tester (Ozone Wayland)",
    branch_selector = branches.STANDARD_RELEASES,
    console_view_entry = ci.console_view_entry(
        category = "release|ozone",
        short_name = "ltow",
    ),
    main_console_view = "main",
    cq_mirrors_console_view = "mirrors",
    triggered_by = [builder_name("linux-ozone-rel")],
    tree_closing = False,
)

ci.linux_builder(
    # CI tester for Ozone/X11
    name = "Linux Tester (Ozone X11)",
    branch_selector = branches.STANDARD_RELEASES,
    console_view_entry = ci.console_view_entry(
        category = "release|ozone",
        short_name = "ltox",
    ),
    main_console_view = "main",
    cq_mirrors_console_view = "mirrors",
    triggered_by = [builder_name("linux-ozone-rel")],
    tree_closing = False,
)

ci.linux_builder(
    name = "linux-trusty-rel",
    console_view_entry = ci.console_view_entry(
        category = "release",
        short_name = "tru",
    ),
    main_console_view = "main",
    os = os.LINUX_TRUSTY,
)

ci.linux_builder(
    name = "metadata-exporter",
    executable = "recipe:chromium_export_metadata",
    service_account = "component-mapping-updater@chops-service-accounts.iam.gserviceaccount.com",
    notifies = ["metadata-mapping"],
    tree_closing = False,
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
        category = "release|arm64",
        short_name = "bld",
    ),
    main_console_view = settings.main_console_name,
    cores = None,
    os = os.MAC_ANY,
)

# TODO(estaab) When promoting out of FYI, make tree_closing True and make
# branch_selector branches.STANDARD_RELEASES, then remove the entry for this
# builder from //generators/scheduler-noop-jobs.star
ci.thin_tester(
    name = "mac-arm64-rel-tests",
    builder_group = "chromium.fyi",
    console_view_entry = ci.console_view_entry(
        category = "mac",
        short_name = "a64",
    ),
    tree_closing = False,
    triggered_by = [builder_name("mac-arm64-rel")],
)

ci.thin_tester(
    name = "Mac10.10 Tests",
    branch_selector = branches.STANDARD_RELEASES,
    builder_group = "chromium.mac",
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
    builder_group = "chromium.mac",
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
    builder_group = "chromium.mac",
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
    builder_group = "chromium.mac",
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
    builder_group = "chromium.mac",
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
    builder_group = "chromium.mac",
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
    builder_group = "chromium.mac",
    console_view_entry = ci.console_view_entry(
        category = "debug",
        short_name = "13",
    ),
    cq_mirrors_console_view = settings.cq_mirrors_console_name,
    main_console_view = settings.main_console_name,
    triggered_by = [builder_name("Mac Builder (dbg)")],
)

ci.mac_ios_builder(
    name = "ios-device",
    console_view_entry = ci.console_view_entry(
        category = "ios|default",
        short_name = "dev",
    ),
    # We don't have necessary capacity to run this configuration in CQ, but it
    # is part of the main waterfall
    main_console_view = "main",
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

ci.mac_ios_builder(
    name = "ios-simulator-noncq",
    console_view_entry = ci.console_view_entry(
        category = "ios|default",
        short_name = "non",
    ),
    # We don't have necessary capacity to run this configuration in CQ, but it
    # is part of the main waterfall
    main_console_view = "main",
)

ci.memory_builder(
    name = "Android CFI",
    # TODO(https://crbug.com/1008094) When this builder is not consistently
    # failing, remove the console_view value
    console_view = "chromium.android.fyi",
    console_view_entry = ci.console_view_entry(
        category = "memory",
        short_name = "cfi",
    ),
    cores = 32,
    # TODO(https://crbug.com/919430) Remove the larger timeout once compile
    # times have been brought down to reasonable level
    execution_timeout = 4 * time.hour + 30 * time.minute,
    tree_closing = False,
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
    name = "Linux CFI",
    console_view_entry = ci.console_view_entry(
        category = "cfi",
        short_name = "lnx",
    ),
    cores = 32,
    # TODO(thakis): Remove once https://crbug.com/927738 is resolved.
    execution_timeout = 4 * time.hour,
    goma_jobs = goma.jobs.MANY_JOBS_FOR_CI,
    main_console_view = "main",
)

ci.memory_builder(
    name = "Linux Chromium OS ASan LSan Builder",
    console_view_entry = ci.console_view_entry(
        category = "cros|asan",
        short_name = "bld",
    ),
    # TODO(crbug.com/1030593): Builds take more than 3 hours sometimes. Remove
    # once the builds are faster.
    execution_timeout = 6 * time.hour,
    main_console_view = "main",
)

ci.memory_builder(
    name = "Linux Chromium OS ASan LSan Tests (1)",
    console_view_entry = ci.console_view_entry(
        category = "cros|asan",
        short_name = "tst",
    ),
    triggered_by = ["Linux Chromium OS ASan LSan Builder"],
    main_console_view = "main",
)

ci.memory_builder(
    name = "Linux ChromiumOS MSan Builder",
    console_view_entry = ci.console_view_entry(
        category = "cros|msan",
        short_name = "bld",
    ),
    main_console_view = "main",
)

ci.memory_builder(
    name = "Linux ChromiumOS MSan Tests",
    console_view_entry = ci.console_view_entry(
        category = "cros|msan",
        short_name = "tst",
    ),
    triggered_by = ["Linux ChromiumOS MSan Builder"],
    main_console_view = "main",
)

ci.memory_builder(
    name = "Linux MSan Builder",
    console_view_entry = ci.console_view_entry(
        category = "linux|msan",
        short_name = "bld",
    ),
    goma_jobs = goma.jobs.MANY_JOBS_FOR_CI,
    main_console_view = "main",
)

ci.memory_builder(
    name = "Linux MSan Tests",
    console_view_entry = ci.console_view_entry(
        category = "linux|msan",
        short_name = "tst",
    ),
    triggered_by = ["Linux MSan Builder"],
    main_console_view = "main",
)

ci.memory_builder(
    name = "Mac ASan 64 Builder",
    builderless = False,
    console_view_entry = ci.console_view_entry(
        category = "mac",
        short_name = "bld",
    ),
    goma_debug = True,  # TODO(hinoka): Remove this after debugging.
    goma_jobs = None,
    cores = None,  # Swapping between 8 and 24
    main_console_view = "main",
    os = os.MAC_DEFAULT,
    triggering_policy = scheduler.greedy_batching(
        max_concurrent_invocations = 2,
    ),
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

ci.memory_builder(
    name = "Mac ASan 64 Tests (1)",
    builderless = False,
    console_view_entry = ci.console_view_entry(
        category = "mac",
        short_name = "tst",
    ),
    main_console_view = "main",
    os = os.MAC_DEFAULT,
    triggered_by = ["Mac ASan 64 Builder"],
)

ci.memory_builder(
    name = "WebKit Linux ASAN",
    console_view_entry = ci.console_view_entry(
        category = "linux|webkit",
        short_name = "asn",
    ),
    main_console_view = "main",
)

ci.memory_builder(
    name = "WebKit Linux Leak",
    console_view_entry = ci.console_view_entry(
        category = "linux|webkit",
        short_name = "lk",
    ),
    main_console_view = "main",
)

ci.memory_builder(
    name = "WebKit Linux MSAN",
    console_view_entry = ci.console_view_entry(
        category = "linux|webkit",
        short_name = "msn",
    ),
    main_console_view = "main",
)

ci.memory_builder(
    name = "android-asan",
    console_view_entry = ci.console_view_entry(
        category = "android",
        short_name = "asn",
    ),
    main_console_view = "main",
    tree_closing = False,
)

ci.memory_builder(
    name = "win-asan",
    console_view_entry = ci.console_view_entry(
        category = "win",
        short_name = "asn",
    ),
    cores = 32,
    builderless = True,
    main_console_view = "main",
    os = os.WINDOWS_DEFAULT,
)

ci.mojo_builder(
    name = "Mojo Android",
    console_view_entry = ci.console_view_entry(
        short_name = "and",
    ),
)

ci.mojo_builder(
    name = "Mojo ChromiumOS",
    console_view_entry = ci.console_view_entry(
        short_name = "cr",
    ),
)

ci.mojo_builder(
    name = "Mojo Linux",
    console_view_entry = ci.console_view_entry(
        short_name = "lnx",
    ),
)

ci.mojo_builder(
    name = "Mojo Windows",
    builderless = False,
    console_view_entry = ci.console_view_entry(
        short_name = "win",
    ),
    os = os.WINDOWS_DEFAULT,
)

ci.mojo_builder(
    name = "android-mojo-webview-rel",
    console_view_entry = ci.console_view_entry(
        short_name = "aw",
    ),
)

ci.mojo_builder(
    name = "mac-mojo-rel",
    console_view_entry = ci.console_view_entry(
        short_name = "mac",
    ),
    cores = 4,
    os = os.MAC_ANY,
)

ci.swangle_linux_builder(
    name = "linux-swangle-chromium-x64",
    console_view_entry = ci.console_view_entry(
        category = "Chromium|Linux",
        short_name = "x64",
    ),
    pinned = False,
)

ci.swangle_linux_builder(
    name = "linux-swangle-tot-angle-x64",
    console_view_entry = ci.console_view_entry(
        category = "ToT ANGLE|Linux",
        short_name = "x64",
    ),
)

ci.swangle_linux_builder(
    name = "linux-swangle-tot-angle-x86",
    console_view_entry = ci.console_view_entry(
        category = "ToT ANGLE|Linux",
        short_name = "x86",
    ),
)

ci.swangle_linux_builder(
    name = "linux-swangle-tot-swiftshader-x64",
    console_view_entry = ci.console_view_entry(
        category = "ToT SwiftShader|Linux",
        short_name = "x64",
    ),
)

ci.swangle_linux_builder(
    name = "linux-swangle-tot-swiftshader-x86",
    console_view_entry = ci.console_view_entry(
        category = "ToT SwiftShader|Linux",
        short_name = "x86",
    ),
)

ci.swangle_linux_builder(
    name = "linux-swangle-x64",
    console_view_entry = ci.console_view_entry(
        category = "DEPS|Linux",
        short_name = "x64",
    ),
    pinned = False,
)

ci.swangle_linux_builder(
    name = "linux-swangle-x86",
    console_view_entry = ci.console_view_entry(
        category = "DEPS|Linux",
        short_name = "x86",
    ),
    pinned = False,
)

ci.swangle_mac_builder(
    name = "mac-swangle-chromium-x64",
    console_view_entry = ci.console_view_entry(
        category = "Chromium|Mac",
        short_name = "x64",
    ),
    pinned = False,
)

ci.swangle_windows_builder(
    name = "win-swangle-chromium-x86",
    console_view_entry = ci.console_view_entry(
        category = "Chromium|Windows",
        short_name = "x86",
    ),
    pinned = False,
)

ci.swangle_windows_builder(
    name = "win-swangle-tot-angle-x64",
    console_view_entry = ci.console_view_entry(
        category = "ToT ANGLE|Windows",
        short_name = "x64",
    ),
)

ci.swangle_windows_builder(
    name = "win-swangle-tot-angle-x86",
    console_view_entry = ci.console_view_entry(
        category = "ToT ANGLE|Windows",
        short_name = "x86",
    ),
)

ci.swangle_windows_builder(
    name = "win-swangle-tot-swiftshader-x64",
    console_view_entry = ci.console_view_entry(
        category = "ToT SwiftShader|Windows",
        short_name = "x64",
    ),
)

ci.swangle_windows_builder(
    name = "win-swangle-tot-swiftshader-x86",
    console_view_entry = ci.console_view_entry(
        category = "ToT SwiftShader|Windows",
        short_name = "x86",
    ),
)

ci.swangle_windows_builder(
    name = "win-swangle-x64",
    console_view_entry = ci.console_view_entry(
        category = "DEPS|Windows",
        short_name = "x64",
    ),
    pinned = False,
)

ci.swangle_windows_builder(
    name = "win-swangle-x86",
    console_view_entry = ci.console_view_entry(
        category = "DEPS|Windows",
        short_name = "x86",
    ),
    pinned = False,
)

ci.win_builder(
    name = "WebKit Win10",
    console_view_entry = ci.console_view_entry(
        category = "misc",
        short_name = "wbk",
    ),
    main_console_view = "main",
    triggered_by = ["Win Builder"],
)

ci.win_builder(
    name = "Win Builder",
    console_view_entry = ci.console_view_entry(
        category = "release|builder",
        short_name = "32",
    ),
    cores = 32,
    main_console_view = "main",
    os = os.WINDOWS_ANY,
)

ci.win_builder(
    name = "Win x64 Builder (dbg)",
    console_view_entry = ci.console_view_entry(
        category = "debug|builder",
        short_name = "64",
    ),
    cores = 32,
    builderless = True,
    main_console_view = "main",
    os = os.WINDOWS_ANY,
)

ci.win_builder(
    name = "Win10 Tests x64 (dbg)",
    console_view_entry = ci.console_view_entry(
        category = "debug|tester",
        short_name = "10",
    ),
    main_console_view = "main",
    triggered_by = ["Win x64 Builder (dbg)"],
    # Too flaky. See crbug.com/876224 for more details.
    tree_closing = False,
)

ci.win_builder(
    name = "Win7 (32) Tests",
    console_view_entry = ci.console_view_entry(
        category = "release|tester",
        short_name = "32",
    ),
    main_console_view = "main",
    os = os.WINDOWS_7,
    triggered_by = ["Win Builder"],
)

ci.win_builder(
    name = "Win7 Tests (1)",
    console_view_entry = ci.console_view_entry(
        category = "release|tester",
        short_name = "32",
    ),
    main_console_view = "main",
    os = os.WINDOWS_7,
    triggered_by = ["Win Builder"],
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

ci.win_builder(
    name = "Windows deterministic",
    console_view_entry = ci.console_view_entry(
        category = "misc",
        short_name = "det",
    ),
    executable = "recipe:swarming/deterministic_build",
    execution_timeout = 6 * time.hour,
    goma_jobs = goma.jobs.J150,
    main_console_view = "main",
)
