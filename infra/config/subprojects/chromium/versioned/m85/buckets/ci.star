# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

load("//lib/builders.star", "builder_name", "goma", "os")
load("//lib/ci.star", "ci")

# Load this using relative path so that the load statement doesn't
# need to be changed when making a new milestone
load("../vars.star", "vars")

ci.set_defaults(
    vars,
    bucketed_triggers = True,
    main_console_view = vars.main_console_name,
    cq_mirrors_console_view = vars.cq_mirrors_console_name,
)

ci.declare_bucket(vars)

# Builders are sorted first lexicographically by the function used to define
# them, then lexicographically by their name

ci.android_builder(
    name = "Android WebView M (dbg)",
    console_view_entry = ci.console_view_entry(
        category = "tester|webview",
        short_name = "M",
    ),
    triggered_by = [builder_name("Android arm64 Builder (dbg)")],
)

ci.android_builder(
    name = "Android WebView N (dbg)",
    console_view_entry = ci.console_view_entry(
        category = "tester|webview",
        short_name = "N",
    ),
    triggered_by = [builder_name("Android arm64 Builder (dbg)")],
)

ci.android_builder(
    name = "Android WebView O (dbg)",
    console_view_entry = ci.console_view_entry(
        category = "tester|webview",
        short_name = "O",
    ),
    triggered_by = [builder_name("Android arm64 Builder (dbg)")],
)

ci.android_builder(
    name = "Android WebView P (dbg)",
    console_view_entry = ci.console_view_entry(
        category = "tester|webview",
        short_name = "P",
    ),
    triggered_by = [builder_name("Android arm64 Builder (dbg)")],
)

ci.android_builder(
    name = "Android arm Builder (dbg)",
    console_view_entry = ci.console_view_entry(
        category = "builder|arm",
        short_name = "32",
    ),
    execution_timeout = 4 * time.hour,
)

ci.android_builder(
    name = "Android arm64 Builder (dbg)",
    console_view_entry = ci.console_view_entry(
        category = "builder|arm",
        short_name = "64",
    ),
    goma_jobs = goma.jobs.MANY_JOBS_FOR_CI,
    execution_timeout = 5 * time.hour,
)

ci.android_builder(
    name = "Android x64 Builder (dbg)",
    console_view_entry = ci.console_view_entry(
        category = "builder|x86",
        short_name = "64",
    ),
    execution_timeout = 4 * time.hour,
)

ci.android_builder(
    name = "Android x86 Builder (dbg)",
    console_view_entry = ci.console_view_entry(
        category = "builder|x86",
        short_name = "32",
    ),
)

ci.android_builder(
    name = "Cast Android (dbg)",
    console_view_entry = ci.console_view_entry(
        category = "on_cq",
        short_name = "cst",
    ),
)

ci.android_builder(
    name = "Marshmallow 64 bit Tester",
    console_view_entry = ci.console_view_entry(
        category = "tester|phone",
        short_name = "M",
    ),
    triggered_by = [builder_name("Android arm64 Builder (dbg)")],
)

ci.android_builder(
    name = "Nougat Phone Tester",
    console_view_entry = ci.console_view_entry(
        category = "tester|phone",
        short_name = "N",
    ),
    triggered_by = [builder_name("Android arm64 Builder (dbg)")],
)

ci.android_builder(
    name = "Oreo Phone Tester",
    console_view_entry = ci.console_view_entry(
        category = "tester|phone",
        short_name = "O",
    ),
    triggered_by = [builder_name("Android arm64 Builder (dbg)")],
)

ci.android_builder(
    name = "android-cronet-arm-dbg",
    console_view_entry = ci.console_view_entry(
        category = "cronet|arm",
        short_name = "dbg",
    ),
    notifies = ["cronet"],
)

ci.android_builder(
    name = "android-cronet-arm-rel",
    console_view_entry = ci.console_view_entry(
        category = "cronet|arm",
        short_name = "rel",
    ),
    notifies = ["cronet"],
)

ci.android_builder(
    name = "android-cronet-arm-rel-kitkat-tests",
    console_view_entry = ci.console_view_entry(
        category = "cronet|test",
        short_name = "k",
    ),
    notifies = ["cronet"],
    triggered_by = [builder_name("android-cronet-arm-rel")],
)

ci.android_builder(
    name = "android-cronet-arm-rel-lollipop-tests",
    console_view_entry = ci.console_view_entry(
        category = "cronet|test",
        short_name = "l",
    ),
    notifies = ["cronet"],
    triggered_by = [builder_name("android-cronet-arm-rel")],
)

ci.android_builder(
    name = "android-lollipop-arm-rel",
    console_view_entry = ci.console_view_entry(
        category = "on_cq",
        short_name = "L",
    ),
)

ci.android_builder(
    name = "android-marshmallow-arm64-rel",
    console_view_entry = ci.console_view_entry(
        category = "on_cq",
        short_name = "M",
    ),
)

ci.android_builder(
    name = "android-marshmallow-x86-rel",
    console_view_entry = ci.console_view_entry(
        category = "builder_tester|x86",
        short_name = "M",
    ),
)

ci.android_builder(
    name = "android-nougat-arm64-rel",
    console_view_entry = ci.console_view_entry(
        category = "builder_tester|arm64",
        short_name = "N",
    ),
)

ci.android_builder(
    name = "android-pie-arm64-dbg",
    console_view_entry = ci.console_view_entry(
        category = "tester|phone",
        short_name = "P",
    ),
    triggered_by = [builder_name("Android arm64 Builder (dbg)")],
)

ci.android_builder(
    name = "android-pie-arm64-rel",
    console_view_entry = ci.console_view_entry(
        category = "on_cq",
        short_name = "P",
    ),
)

ci.chromium_builder(
    name = "android-official",
    # TODO(https://crbug.com/1072012) Use the default console view and add
    # main_console_view = settings.main_console_name once the build is green
    console_view = "chromium.fyi",
    console_view_entry = ci.console_view_entry(
        category = "android",
        short_name = "off",
    ),
    cores = 32,
    # TODO: Change this back down to something reasonable once these builders
    # have populated their cached by getting through the compile step
    execution_timeout = 6 * time.hour,
)

ci.chromium_builder(
    name = "fuchsia-official",
    # TODO(https://crbug.com/1072012) Use the default console view and add
    # main_console_view = settings.main_console_name once the build is green
    console_view = "chromium.fyi",
    console_view_entry = ci.console_view_entry(
        category = "fuchsia",
        short_name = "off",
    ),
    cores = 32,
    # TODO: Change this back down to something reasonable once these builders
    # have populated their cached by getting through the compile step
    execution_timeout = 10 * time.hour,
)

ci.chromium_builder(
    name = "linux-official",
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
)

ci.chromiumos_builder(
    name = "chromeos-amd64-generic-dbg",
    console_view_entry = ci.console_view_entry(
        category = "simple|debug|x64",
        short_name = "dbg",
    ),
)

ci.chromiumos_builder(
    name = "chromeos-amd64-generic-rel",
    console_view_entry = ci.console_view_entry(
        category = "simple|release|x64",
        short_name = "rel",
    ),
)

ci.chromiumos_builder(
    name = "chromeos-arm-generic-rel",
    console_view_entry = ci.console_view_entry(
        category = "simple|release",
        short_name = "arm",
    ),
)

ci.chromiumos_builder(
    name = "linux-chromeos-dbg",
    console_view_entry = ci.console_view_entry(
        category = "default",
        short_name = "dbg",
    ),
)

ci.chromiumos_builder(
    name = "linux-chromeos-rel",
    console_view_entry = ci.console_view_entry(
        category = "default",
        short_name = "rel",
    ),
)

ci.dawn_linux_builder(
    name = "Dawn Linux x64 DEPS Builder",
    console_view_entry = ci.console_view_entry(
        category = "DEPS|Linux|Builder",
        short_name = "x64",
    ),
)

ci.dawn_thin_tester(
    name = "Dawn Linux x64 DEPS Release (Intel HD 630)",
    console_view_entry = ci.console_view_entry(
        category = "DEPS|Linux|Intel",
        short_name = "x64",
    ),
    triggered_by = [builder_name("Dawn Linux x64 DEPS Builder")],
)

ci.dawn_thin_tester(
    name = "Dawn Linux x64 DEPS Release (NVIDIA)",
    console_view_entry = ci.console_view_entry(
        category = "DEPS|Linux|Nvidia",
        short_name = "x64",
    ),
    triggered_by = [builder_name("Dawn Linux x64 DEPS Builder")],
)

ci.dawn_mac_builder(
    name = "Dawn Mac x64 DEPS Builder",
    console_view_entry = ci.console_view_entry(
        category = "DEPS|Mac|Builder",
        short_name = "x64",
    ),
)

# Note that the Mac testers are all thin Linux VMs, triggering jobs on the
# physical Mac hardware in the Swarming pool which is why they run on linux
ci.dawn_thin_tester(
    name = "Dawn Mac x64 DEPS Release (AMD)",
    console_view_entry = ci.console_view_entry(
        category = "DEPS|Mac|AMD",
        short_name = "x64",
    ),
    triggered_by = [builder_name("Dawn Mac x64 DEPS Builder")],
)

ci.dawn_thin_tester(
    name = "Dawn Mac x64 DEPS Release (Intel)",
    console_view_entry = ci.console_view_entry(
        category = "DEPS|Mac|Intel",
        short_name = "x64",
    ),
    triggered_by = [builder_name("Dawn Mac x64 DEPS Builder")],
)

ci.dawn_windows_builder(
    name = "Dawn Win10 x64 DEPS Builder",
    console_view_entry = ci.console_view_entry(
        category = "DEPS|Windows|Builder",
        short_name = "x64",
    ),
)

ci.dawn_thin_tester(
    name = "Dawn Win10 x64 DEPS Release (Intel HD 630)",
    console_view_entry = ci.console_view_entry(
        category = "DEPS|Windows|Intel",
        short_name = "x64",
    ),
    triggered_by = [builder_name("Dawn Win10 x64 DEPS Builder")],
)

ci.dawn_thin_tester(
    name = "Dawn Win10 x64 DEPS Release (NVIDIA)",
    console_view_entry = ci.console_view_entry(
        category = "DEPS|Windows|Nvidia",
        short_name = "x64",
    ),
    triggered_by = [builder_name("Dawn Win10 x64 DEPS Builder")],
)

ci.dawn_windows_builder(
    name = "Dawn Win10 x86 DEPS Builder",
    console_view_entry = ci.console_view_entry(
        category = "DEPS|Windows|Builder",
        short_name = "x86",
    ),
)

ci.dawn_thin_tester(
    name = "Dawn Win10 x86 DEPS Release (Intel HD 630)",
    console_view_entry = ci.console_view_entry(
        category = "DEPS|Windows|Intel",
        short_name = "x86",
    ),
    triggered_by = [builder_name("Dawn Win10 x86 DEPS Builder")],
)

ci.dawn_thin_tester(
    name = "Dawn Win10 x86 DEPS Release (NVIDIA)",
    console_view_entry = ci.console_view_entry(
        category = "DEPS|Windows|Nvidia",
        short_name = "x86",
    ),
    triggered_by = [builder_name("Dawn Win10 x86 DEPS Builder")],
)

ci.fyi_builder(
    name = "VR Linux",
    console_view_entry = ci.console_view_entry(
        category = "linux",
    ),
)

ci.fyi_ios_builder(
    name = "ios-simulator-cronet",
    console_view_entry = ci.console_view_entry(
        category = "cronet",
    ),
    fully_qualified_builder_dimension = True,
    executable = "recipe:chromium",
    notifies = ["cronet"],
    properties = {
        "xcode_build_version": "11e146",
    },
)

ci.gpu_linux_builder(
    name = "Android Release (Nexus 5X)",
    console_view_entry = ci.console_view_entry(
        category = "Android",
    ),
)

ci.gpu_linux_builder(
    name = "GPU Linux Builder",
    console_view_entry = ci.console_view_entry(
        category = "Linux",
    ),
)

ci.gpu_mac_builder(
    name = "GPU Mac Builder",
    console_view_entry = ci.console_view_entry(
        category = "Mac",
    ),
)

ci.gpu_windows_builder(
    name = "GPU Win x64 Builder",
    console_view_entry = ci.console_view_entry(
        category = "Windows",
    ),
)

ci.gpu_thin_tester(
    name = "Linux Release (NVIDIA)",
    console_view_entry = ci.console_view_entry(
        category = "Linux",
    ),
    triggered_by = [builder_name("GPU Linux Builder")],
)

ci.gpu_thin_tester(
    name = "Mac Release (Intel)",
    console_view_entry = ci.console_view_entry(
        category = "Mac",
    ),
    triggered_by = [builder_name("GPU Mac Builder")],
)

ci.gpu_thin_tester(
    name = "Mac Retina Release (AMD)",
    console_view_entry = ci.console_view_entry(
        category = "Mac",
    ),
    triggered_by = [builder_name("GPU Mac Builder")],
)

ci.gpu_thin_tester(
    name = "Win10 x64 Release (NVIDIA)",
    console_view_entry = ci.console_view_entry(
        category = "Windows",
    ),
    triggered_by = [builder_name("GPU Win x64 Builder")],
)

ci.linux_builder(
    name = "Cast Linux",
    console_view_entry = ci.console_view_entry(
        category = "cast",
        short_name = "vid",
    ),
    goma_jobs = goma.jobs.J50,
)

ci.linux_builder(
    name = "Fuchsia ARM64",
    console_view_entry = ci.console_view_entry(
        category = "fuchsia|a64",
        short_name = "rel",
    ),
    extra_notifies = ["cr-fuchsia"],
)

ci.linux_builder(
    name = "Fuchsia x64",
    console_view_entry = ci.console_view_entry(
        category = "fuchsia|x64",
        short_name = "rel",
    ),
    extra_notifies = ["cr-fuchsia"],
)

ci.linux_builder(
    name = "Linux Builder",
    console_view_entry = ci.console_view_entry(
        category = "release",
        short_name = "bld",
    ),
)

ci.linux_builder(
    name = "Linux Builder (dbg)",
    console_view_entry = ci.console_view_entry(
        category = "debug|builder",
        short_name = "64",
    ),
)

ci.linux_builder(
    name = "Linux Tests",
    console_view_entry = ci.console_view_entry(
        category = "release",
        short_name = "tst",
    ),
    goma_backend = None,
    triggered_by = [builder_name("Linux Builder")],
)

ci.linux_builder(
    name = "Linux Tests (dbg)(1)",
    console_view_entry = ci.console_view_entry(
        category = "debug|tester",
        short_name = "64",
    ),
    triggered_by = [builder_name("Linux Builder (dbg)")],
)

ci.linux_builder(
    name = "fuchsia-arm64-cast",
    console_view_entry = ci.console_view_entry(
        category = "fuchsia|cast",
        short_name = "a64",
    ),
    extra_notifies = ["cr-fuchsia"],
)

ci.linux_builder(
    name = "fuchsia-x64-cast",
    console_view_entry = ci.console_view_entry(
        category = "fuchsia|cast",
        short_name = "x64",
    ),
    extra_notifies = ["cr-fuchsia"],
)

ci.linux_builder(
    name = "linux-ozone-rel",
    console_view_entry = ci.console_view_entry(
        category = "release",
        short_name = "ozo",
    ),
)

ci.linux_builder(
    name = "Linux Ozone Tester (Headless)",
    console_view = "chromium.fyi",
    console_view_entry = ci.console_view_entry(
        category = "linux",
        short_name = "loh",
    ),
    triggered_by = [builder_name("linux-ozone-rel")],
)

ci.linux_builder(
    name = "Linux Ozone Tester (Wayland)",
    console_view = "chromium.fyi",
    console_view_entry = ci.console_view_entry(
        category = "linux",
        short_name = "low",
    ),
    triggered_by = [builder_name("linux-ozone-rel")],
)

ci.linux_builder(
    name = "Linux Ozone Tester (X11)",
    console_view = "chromium.fyi",
    console_view_entry = ci.console_view_entry(
        category = "linux",
        short_name = "lox",
    ),
    triggered_by = [builder_name("linux-ozone-rel")],
)

ci.mac_builder(
    name = "Mac Builder",
    console_view_entry = ci.console_view_entry(
        category = "release",
        short_name = "bld",
    ),
    os = os.MAC_10_15,
)

ci.mac_builder(
    name = "Mac Builder (dbg)",
    console_view_entry = ci.console_view_entry(
        category = "debug",
        short_name = "bld",
    ),
    os = os.MAC_ANY,
)

ci.thin_tester(
    name = "Mac10.10 Tests",
    builder_group = "chromium.mac",
    console_view_entry = ci.console_view_entry(
        category = "release",
        short_name = "10",
    ),
    triggered_by = [builder_name("Mac Builder")],
)

ci.thin_tester(
    name = "Mac10.11 Tests",
    builder_group = "chromium.mac",
    console_view_entry = ci.console_view_entry(
        category = "release",
        short_name = "11",
    ),
    triggered_by = [builder_name("Mac Builder")],
)

ci.thin_tester(
    name = "Mac10.12 Tests",
    builder_group = "chromium.mac",
    console_view_entry = ci.console_view_entry(
        category = "release",
        short_name = "12",
    ),
    triggered_by = [builder_name("Mac Builder")],
)

ci.thin_tester(
    name = "Mac10.13 Tests",
    builder_group = "chromium.mac",
    console_view_entry = ci.console_view_entry(
        category = "release",
        short_name = "13",
    ),
    triggered_by = [builder_name("Mac Builder")],
)

ci.thin_tester(
    name = "Mac10.14 Tests",
    builder_group = "chromium.mac",
    console_view_entry = ci.console_view_entry(
        category = "release",
        short_name = "14",
    ),
    triggered_by = [builder_name("Mac Builder")],
)

ci.thin_tester(
    name = "Mac10.15 Tests",
    builder_group = "chromium.mac",
    console_view_entry = ci.console_view_entry(
        category = "release",
        short_name = "15",
    ),
    triggered_by = [builder_name("Mac Builder")],
)

ci.thin_tester(
    name = "Mac10.13 Tests (dbg)",
    builder_group = "chromium.mac",
    console_view_entry = ci.console_view_entry(
        category = "debug",
        short_name = "13",
    ),
    triggered_by = [builder_name("Mac Builder (dbg)")],
)

ci.mac_ios_builder(
    name = "ios-simulator",
    console_view_entry = ci.console_view_entry(
        category = "ios|default",
        short_name = "sim",
    ),
    fully_qualified_builder_dimension = True,
    properties = {
        "xcode_build_version": "11e146",
    },
)

ci.mac_ios_builder(
    name = "ios-simulator-full-configs",
    console_view_entry = ci.console_view_entry(
        category = "ios|default",
        short_name = "ful",
    ),
    fully_qualified_builder_dimension = True,
    properties = {
        "xcode_build_version": "11e146",
    },
)

ci.memory_builder(
    name = "Linux ASan LSan Builder",
    console_view_entry = ci.console_view_entry(
        category = "linux|asan lsan",
        short_name = "bld",
    ),
    ssd = True,
)

ci.memory_builder(
    name = "Linux ASan LSan Tests (1)",
    console_view_entry = ci.console_view_entry(
        category = "linux|asan lsan",
        short_name = "tst",
    ),
    triggered_by = [builder_name("Linux ASan LSan Builder")],
)

ci.memory_builder(
    name = "Linux ASan Tests (sandboxed)",
    console_view_entry = ci.console_view_entry(
        category = "linux|asan lsan",
        short_name = "sbx",
    ),
    triggered_by = [builder_name("Linux ASan LSan Builder")],
)

ci.memory_builder(
    name = "Linux TSan Builder",
    console_view_entry = ci.console_view_entry(
        category = "linux|TSan v2",
        short_name = "bld",
    ),
)

ci.memory_builder(
    name = "Linux TSan Tests",
    console_view_entry = ci.console_view_entry(
        category = "linux|TSan v2",
        short_name = "tst",
    ),
    triggered_by = [builder_name("Linux TSan Builder")],
)

ci.win_builder(
    name = "Win7 Tests (dbg)(1)",
    console_view_entry = ci.console_view_entry(
        category = "debug|tester",
        short_name = "7",
    ),
    os = os.WINDOWS_7,
    triggered_by = [builder_name("Win Builder (dbg)")],
)

ci.win_builder(
    name = "Win 7 Tests x64 (1)",
    console_view_entry = ci.console_view_entry(
        category = "release|tester",
        short_name = "64",
    ),
    os = os.WINDOWS_7,
    triggered_by = [builder_name("Win x64 Builder")],
)

ci.win_builder(
    name = "Win Builder (dbg)",
    console_view_entry = ci.console_view_entry(
        category = "debug|builder",
        short_name = "32",
    ),
    cores = 32,
    os = os.WINDOWS_ANY,
)

ci.win_builder(
    name = "Win x64 Builder",
    console_view_entry = ci.console_view_entry(
        category = "release|builder",
        short_name = "64",
    ),
    cores = 32,
    os = os.WINDOWS_ANY,
)

ci.win_builder(
    name = "Win10 Tests x64",
    console_view_entry = ci.console_view_entry(
        category = "release|tester",
        short_name = "w10",
    ),
    triggered_by = [builder_name("Win x64 Builder")],
)
