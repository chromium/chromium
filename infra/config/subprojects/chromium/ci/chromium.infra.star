# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Definitions of builders in the chromium.infra builder group."""

load("//lib/branches.star", "branches")
load("//lib/builder_health_indicators.star", "health_spec")
load("//lib/builders.star", "gardener_rotations", "os")
load("//lib/ci.star", "ci")
load("//lib/consoles.star", "consoles")

ci.defaults.set(
    builder_group = "chromium.infra",
    pool = ci.DEFAULT_POOL,
    cores = 8,
    os = os.LINUX_DEFAULT,
    execution_timeout = ci.DEFAULT_EXECUTION_TIMEOUT,
    health_spec = health_spec.DEFAULT,
    service_account = ci.DEFAULT_SERVICE_ACCOUNT,
    shadow_service_account = ci.DEFAULT_SHADOW_SERVICE_ACCOUNT,
)

consoles.console_view(
    name = "chromium.infra",
)

# Builders monitored by go/clank-autoroll
consoles.list_view(
    name = "android.autoroll",
    title = "Android Autoroll Gardening",
)
[branches.list_view_entry(
    list_view = "android.autoroll",
    builder = "chromium:ci/{}".format(name),
) for name in (
    "android-androidx-packager",
    "android-sdk-packager",
    "3pp-linux-amd64-packager",
)]

def packager_builder(**kwargs):
    return ci.builder(
        service_account = "chromium-cipd-builder@chops-service-accounts.iam.gserviceaccount.com",
        shadow_service_account = "chromium-cipd-try-builder@chops-service-accounts.iam.gserviceaccount.com",
        **kwargs
    )

packager_builder(
    name = "3pp-linux-amd64-packager",
    executable = "recipe:chromium_3pp",
    # Every 6 hours starting at 5am UTC.
    schedule = "0 5/6 * * * *",
    triggered_by = [],
    builderless = False,
    console_view_entry = consoles.console_view_entry(
        category = "packager|3pp|linux",
        short_name = "amd64",
    ),
    execution_timeout = 4 * time.hour,
    notifies = ["chromium-infra"],
    properties = {
        "$build/chromium_3pp": {
            "platform": "linux-amd64",
            "preprocess": [{
                "name": "third_party/android_deps",
                "cmd": [
                    "{CHECKOUT}/src/third_party/android_deps/fetch_all.py",
                    "-v",
                    "--ignore-vulnerabilities",
                ],
            }],
            "gclient_config": "chromium",
            "gclient_apply_config": ["android"],
        },
    },
)

packager_builder(
    name = "3pp-mac-amd64-packager",
    executable = "recipe:chromium_3pp",
    # TODO(crbug.com/40204454): Trigger builds routinely once works fine.
    schedule = "triggered",
    triggered_by = [],
    builderless = True,
    cores = None,
    os = os.MAC_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "packager|3pp|mac",
        short_name = "amd64",
    ),
    notifies = ["chromium-infra"],
    properties = {
        "$build/chromium_3pp": {
            "platform": "mac-amd64",
            "gclient_config": "chromium",
        },
    },
)

packager_builder(
    name = "3pp-windows-amd64-packager",
    description_html = "3PP Packager for Windows",
    executable = "recipe:chromium_3pp",
    # Every 6 hours starting at 5am UTC.
    schedule = "0 5/6 * * * *",
    triggered_by = [],
    builderless = True,
    cores = None,
    os = os.WINDOWS_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "packager|3pp|win",
        short_name = "amd64",
    ),
    contact_team_email = "chrome-browser-infra-team@google.com",
    notifies = ["chromium-infra"],
    properties = {
        "$build/chromium_3pp": {
            "platform": "windows-amd64",
            "gclient_config": "chromium",
        },
    },
)

packager_builder(
    name = "android-androidx-packager",
    executable = "recipe:android/androidx_packager",
    schedule = "0 7,14,22 * * * *",
    triggered_by = [],
    gardener_rotations = gardener_rotations.ANDROID,
    console_view_entry = consoles.console_view_entry(
        category = "packager|android",
        short_name = "androidx",
    ),
    contact_team_email = "clank-build@google.com",
    notifies = ["chromium-androidx-packager"],
)

packager_builder(
    name = "android-avd-packager",
    executable = "recipe:android/avd_packager",
    # Triggered manually through the scheduler UI
    # https://luci-scheduler.appspot.com/jobs/chromium/android-avd-packager
    schedule = "triggered",
    triggered_by = [],
    console_view_entry = consoles.console_view_entry(
        category = "packager|android",
        short_name = "avd",
    ),
    properties = {
        "$build/avd_packager": {
            "avd_configs": [
                # google_apis system images
                "tools/android/avd/proto_creation/android_28_google_apis_x86.textpb",
                "tools/android/avd/proto_creation/android_29_google_apis_x86.textpb",
                "tools/android/avd/proto_creation/android_30_google_apis_x86.textpb",
                "tools/android/avd/proto_creation/android_31_google_apis_x64.textpb",
                "tools/android/avd/proto_creation/android_32_google_apis_x64_foldable.textpb",
                "tools/android/avd/proto_creation/android_32_google_apis_x64_foldable_landscape.textpb",
                "tools/android/avd/proto_creation/android_33_google_apis_x64.textpb",
                "tools/android/avd/proto_creation/android_34_google_apis_x64.textpb",
                "tools/android/avd/proto_creation/android_35_google_apis_x64.textpb",

                # google_atd system images
                "tools/android/avd/proto_creation/android_30_google_atd_x86.textpb",
                "tools/android/avd/proto_creation/android_30_google_atd_x64.textpb",
                "tools/android/avd/proto_creation/android_31_google_atd_x64.textpb",
                "tools/android/avd/proto_creation/android_32_google_atd_x64_foldable.textpb",
                "tools/android/avd/proto_creation/android_33_google_atd_x64.textpb",

                # Desktop system images
                "tools/android/avd/proto_creation/android_34_desktop_x64.textpb",

                # TODO(hypan): Using more specific names for the configs below.
                "tools/android/avd/proto_creation/generic_android19.textpb",
                "tools/android/avd/proto_creation/generic_android22.textpb",
                "tools/android/avd/proto_creation/generic_android23.textpb",
                "tools/android/avd/proto_creation/generic_android24.textpb",
                "tools/android/avd/proto_creation/generic_android25.textpb",
                "tools/android/avd/proto_creation/generic_android26.textpb",
                "tools/android/avd/proto_creation/generic_android27.textpb",
            ],
            "gclient_config": "chromium",
            "gclient_apply_config": ["android"],
        },
    },
)

packager_builder(
    name = "android-sdk-packager",
    executable = "recipe:android/sdk_packager",
    schedule = "0 7 * * *",
    triggered_by = [],
    console_view_entry = consoles.console_view_entry(
        category = "packager|android",
        short_name = "sdk",
    ),
    properties = {
        "packages": [
            {
                "sdk_package_name": "build-tools;34.0.0",
                "cipd_yaml": "third_party/android_sdk/cipd/build-tools/34.0.0.yaml",
            },
            {
                "sdk_package_name": "build-tools;35.0.0",
                "cipd_yaml": "third_party/android_sdk/cipd/build-tools/35.0.0.yaml",
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
                "sdk_package_name": "emulator",
                "cipd_yaml": "third_party/android_sdk/cipd/emulator.yaml",
                "sdk_channel": "BETA",
            },
            {
                "sdk_package_name": "emulator",
                "cipd_yaml": "third_party/android_sdk/cipd/emulator.yaml",
                "sdk_channel": "CANARY",
            },
            {
                "sdk_package_name": "platforms;android-34",
                "cipd_yaml": "third_party/android_sdk/cipd/platforms/android-34.yaml",
            },
            {
                "sdk_package_name": "platforms;android-35",
                "cipd_yaml": "third_party/android_sdk/cipd/platforms/android-35.yaml",
            },
            {
                "sdk_package_name": "platform-tools",
                "cipd_yaml": "third_party/android_sdk/cipd/platform-tools.yaml",
            },
            {
                "sdk_package_name": "system-images;android-19;google_apis;x86",
                "cipd_yaml": "third_party/android_sdk/cipd/system_images/android-19/google_apis/x86.yaml",
            },
            {
                "sdk_package_name": "system-images;android-22;google_apis;x86",
                "cipd_yaml": "third_party/android_sdk/cipd/system_images/android-22/google_apis/x86.yaml",
            },
            {
                "sdk_package_name": "system-images;android-23;google_apis;x86",
                "cipd_yaml": "third_party/android_sdk/cipd/system_images/android-23/google_apis/x86.yaml",
            },
            {
                "sdk_package_name": "system-images;android-24;google_apis;x86",
                "cipd_yaml": "third_party/android_sdk/cipd/system_images/android-24/google_apis/x86.yaml",
            },
            {
                "sdk_package_name": "system-images;android-25;google_apis;x86",
                "cipd_yaml": "third_party/android_sdk/cipd/system_images/android-25/google_apis/x86.yaml",
            },
            {
                "sdk_package_name": "system-images;android-26;google_apis;x86",
                "cipd_yaml": "third_party/android_sdk/cipd/system_images/android-26/google_apis/x86.yaml",
            },
            {
                "sdk_package_name": "system-images;android-27;google_apis;x86",
                "cipd_yaml": "third_party/android_sdk/cipd/system_images/android-27/google_apis/x86.yaml",
            },
            {
                "sdk_package_name": "system-images;android-28;google_apis;x86",
                "cipd_yaml": "third_party/android_sdk/cipd/system_images/android-28/google_apis/x86.yaml",
            },
            {
                "sdk_package_name": "system-images;android-29;google_apis;x86",
                "cipd_yaml": "third_party/android_sdk/cipd/system_images/android-29/google_apis/x86.yaml",
            },
            {
                "sdk_package_name": "system-images;android-30;google_apis;x86",
                "cipd_yaml": "third_party/android_sdk/cipd/system_images/android-30/google_apis/x86.yaml",
            },
            {
                "sdk_package_name": "system-images;android-30;google_atd;x86",
                "cipd_yaml": "third_party/android_sdk/cipd/system_images/android-30/google_atd/x86.yaml",
            },
            {
                "sdk_package_name": "system-images;android-30;google_atd;x86_64",
                "cipd_yaml": "third_party/android_sdk/cipd/system_images/android-30/google_atd/x86_64.yaml",
            },
            # use x86_64 since sdkmanager don't ship x86 for android-31 and above.
            {
                "sdk_package_name": "system-images;android-31;google_apis;arm64-v8a",
                "cipd_yaml": "third_party/android_sdk/cipd/system_images/android-31/google_apis/arm64.yaml",
            },
            {
                "sdk_package_name": "system-images;android-31;google_apis;x86_64",
                "cipd_yaml": "third_party/android_sdk/cipd/system_images/android-31/google_apis/x86_64.yaml",
            },
            {
                "sdk_package_name": "system-images;android-31;google_atd;x86_64",
                "cipd_yaml": "third_party/android_sdk/cipd/system_images/android-31/google_atd/x86_64.yaml",
            },
            {
                "sdk_package_name": "system-images;android-32;google_apis;x86_64",
                "cipd_yaml": "third_party/android_sdk/cipd/system_images/android-32/google_apis/x86_64.yaml",
            },
            {
                "sdk_package_name": "system-images;android-32;google_atd;x86_64",
                "cipd_yaml": "third_party/android_sdk/cipd/system_images/android-32/google_atd/x86_64.yaml",
            },
            {
                "sdk_package_name": "system-images;android-33;google_apis;x86_64",
                "cipd_yaml": "third_party/android_sdk/cipd/system_images/android-33/google_apis/x86_64.yaml",
            },
            {
                "sdk_package_name": "system-images;android-33;google_atd;x86_64",
                "cipd_yaml": "third_party/android_sdk/cipd/system_images/android-33/google_atd/x86_64.yaml",
            },
            {
                "sdk_package_name": "system-images;android-34;google_apis;x86_64",
                "cipd_yaml": "third_party/android_sdk/cipd/system_images/android-34/google_apis/x86_64.yaml",
            },
            {
                "sdk_package_name": "system-images;android-35;google_apis;x86_64",
                "cipd_yaml": "third_party/android_sdk/cipd/system_images/android-35/google_apis/x86_64.yaml",
            },
            {
                "sdk_package_name": "system-images;android-34;android-desktop;x86_64",
                "cipd_yaml": "third_party/android_sdk/cipd/system_images/android-34/android-desktop/x86_64.yaml",
            },
        ],
    },
)

packager_builder(
    name = "rts-model-packager",
    executable = "recipe:chromium_rts/create_model",
    schedule = "0 9 * * *",  # at 1AM or 2AM PT (depending on DST), once a day.
    triggered_by = [],
    builderless = False,
    cores = None,
    console_view_entry = consoles.console_view_entry(
        category = "packager|rts",
        short_name = "create-model",
    ),
    execution_timeout = 10 * time.hour,
    notifies = [
        luci.notifier(
            name = "rts-model-packager-notifier",
            notify_emails = ["chrome-browser-infra-team@google.com"],
            on_occurrence = ["FAILURE", "INFRA_FAILURE"],
        ),
    ],
)

ci.builder(
    name = "android-device-flasher",
    executable = "recipe:android/device_flasher",
    # TODO(crbug.com/40201767): Find the sweet spot for the frequency.
    schedule = "0 9 * * 1",  # at 9am UTC every Monday.
    triggered_by = [],
    console_view_entry = consoles.console_view_entry(
        short_name = "flash",
    ),
    notifies = ["chromium-infra"],
    properties = {
        "flash_criteria": [
            # Used by ci/Android Release (Nexus 5X)
            # This is mirrored by the CQ builder android-arm64-rel
            {
                "pool": "chromium.tests",
                "device_type": "bullhead",
                "device_os": "N2G48C",
                "max_uid_threshold": 18000,
            },
            {
                "pool": "chromium.tests",
                "device_type": "walleye",
                "device_os": "OPM4.171019.021.P2",
                "max_uid_threshold": 18000,
            },
            # Used by ci/android-pie-arm64-rel
            # This is mirrored by the CQ builder android-arm64-rel
            {
                "pool": "chromium.tests",
                "device_type": "walleye",
                "device_os": "PQ3A.190801.002",
                "max_uid_threshold": 18000,
            },
            # Used by ci/android-pie-arm64-rel
            # This is mirrored by the CQ builder android-arm64-rel
            {
                "pool": "chromium.tests",
                "device_type": "sailfish",
                "device_os": "PQ3A.190801.002",
                "max_uid_threshold": 18000,
            },
            {
                "pool": "chromium.tests",
                "device_type": "walleye",
                "device_os": "QQ1A.191205.008",
                "max_uid_threshold": 18000,
            },
            # Used by GPU team
            {
                "pool": "chromium.tests.gpu",
                "device_type": "oriole",
                "device_os": "TP1A.220624.021",
                "max_uid_threshold": 18000,
            },
        ],
    },
)

ci.builder(
    name = "rts-suite-analysis",
    executable = "recipe:chromium_rts/rts_analyze",
    schedule = "0 9 * * *",  # at 1AM or 2AM PT (depending on DST), once a day.
    triggered_by = [],
    builderless = False,
    cores = None,
    console_view_entry = consoles.console_view_entry(
        category = "analysis|rts",
        short_name = "suite-analysis",
    ),
    execution_timeout = 10 * time.hour,
    service_account = "chromium-cipd-builder@chops-service-accounts.iam.gserviceaccount.com",
)
