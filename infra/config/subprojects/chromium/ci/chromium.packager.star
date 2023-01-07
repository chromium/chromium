# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Definitions of builders in the chromium.packager builder group."""

load("//lib/builders.star", "os", "sheriff_rotations")
load("//lib/ci.star", "ci")
load("//lib/consoles.star", "consoles")

ci.defaults.set(
    builder_group = "chromium.packager",
    cores = 8,
    execution_timeout = ci.DEFAULT_EXECUTION_TIMEOUT,
    os = os.LINUX_DEFAULT,
    pool = ci.DEFAULT_POOL,
    service_account = "chromium-cipd-builder@chops-service-accounts.iam.gserviceaccount.com",

    # TODO(crbug.com/1362440): remove this.
    omit_python2 = False,
)

consoles.console_view(
    name = "chromium.packager",
)

ci.builder(
    name = "3pp-linux-amd64-packager",
    builderless = False,
    console_view_entry = consoles.console_view_entry(
        category = "3pp|linux",
        short_name = "amd64",
    ),
    executable = "recipe:chromium_3pp",
    notifies = ["chromium-3pp-packager"],
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
    # Every 6 hours starting at 5am UTC.
    schedule = "0 5/6 * * * *",
    triggered_by = [],
)

ci.builder(
    name = "3pp-mac-amd64-packager",
    os = os.MAC_DEFAULT,
    builderless = True,
    console_view_entry = consoles.console_view_entry(
        category = "3pp|mac",
        short_name = "amd64",
    ),
    cores = None,
    executable = "recipe:chromium_3pp",
    notifies = ["chromium-3pp-packager"],
    properties = {
        "$build/chromium_3pp": {
            "platform": "mac-amd64",
            "gclient_config": "chromium",
        },
    },
    # TODO(crbug.com/1267449): Trigger builds routinely once works fine.
    schedule = "triggered",
    triggered_by = [],
)

ci.builder(
    name = "android-androidx-packager",
    console_view_entry = consoles.console_view_entry(
        category = "android",
        short_name = "androidx",
    ),
    notifies = ["chromium-androidx-packager"],
    executable = "recipe:android/androidx_packager",
    schedule = "0 7,14,22 * * * *",
    sheriff_rotations = sheriff_rotations.ANDROID,
    triggered_by = [],
)

ci.builder(
    name = "android-avd-packager",
    console_view_entry = consoles.console_view_entry(
        category = "android",
        short_name = "avd",
    ),
    executable = "recipe:android/avd_packager",
    properties = {
        "avd_configs": [
            "tools/android/avd/proto/creation/generic_android19.textpb",
            "tools/android/avd/proto/creation/generic_android22.textpb",
            "tools/android/avd/proto/creation/generic_android23.textpb",
            "tools/android/avd/proto/creation/generic_android24.textpb",
            "tools/android/avd/proto/creation/generic_playstore_android24.textpb",
            "tools/android/avd/proto/creation/generic_android25.textpb",
            "tools/android/avd/proto/creation/generic_playstore_android25.textpb",
            "tools/android/avd/proto/creation/generic_android27.textpb",
            "tools/android/avd/proto/creation/generic_playstore_android27.textpb",
            "tools/android/avd/proto/creation/generic_android28.textpb",
            "tools/android/avd/proto/creation/generic_playstore_android28.textpb",
            "tools/android/avd/proto/creation/generic_android29.textpb",
            "tools/android/avd/proto/creation/generic_android30.textpb",
            "tools/android/avd/proto/creation/generic_playstore_android30.textpb",
            "tools/android/avd/proto/creation/generic_android31.textpb",
            "tools/android/avd/proto/creation/generic_playstore_android31.textpb",
            "tools/android/avd/proto/creation/generic_android32_foldable.textpb",
            "tools/android/avd/proto/creation/generic_playstore_android32_foldable.textpb",
            "tools/android/avd/proto/creation/generic_android33.textpb",
            "tools/android/avd/proto/creation/generic_playstore_android33.textpb",
        ],
    },
    # Triggered manually through the scheduler UI
    # https://luci-scheduler.appspot.com/jobs/chromium/android-avd-packager
    schedule = "triggered",
    triggered_by = [],
)

ci.builder(
    name = "android-sdk-packager",
    console_view_entry = consoles.console_view_entry(
        category = "android",
        short_name = "sdk",
    ),
    executable = "recipe:android/sdk_packager",
    properties = {
        # We still package part of build-tools;25.0.2 to support
        # http://bit.ly/2KNUygZ
        "packages": [
            {
                "sdk_package_name": "build-tools;25.0.2",
                "cipd_yaml": "third_party/android_sdk/cipd/build-tools/25.0.2.yaml",
            },
            {
                "sdk_package_name": "build-tools;31.0.0",
                "cipd_yaml": "third_party/android_sdk/cipd/build-tools/31.0.0.yaml",
            },
            {
                "sdk_package_name": "build-tools;33.0.0",
                "cipd_yaml": "third_party/android_sdk/cipd/build-tools/33.0.0.yaml",
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
                "sdk_package_name": "patcher;v4",
                "cipd_yaml": "third_party/android_sdk/cipd/patcher/v4.yaml",
            },
            {
                "sdk_package_name": "platforms;android-31",
                "cipd_yaml": "third_party/android_sdk/cipd/platforms/android-31.yaml",
            },
            {
                "sdk_package_name": "platforms;android-33",
                "cipd_yaml": "third_party/android_sdk/cipd/platforms/android-33.yaml",
            },
            {
                "sdk_package_name": "platform-tools",
                "cipd_yaml": "third_party/android_sdk/cipd/platform-tools.yaml",
            },
            {
                "sdk_package_name": "sources;android-31",
                "cipd_yaml": "third_party/android_sdk/cipd/sources/android-31.yaml",
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
                "sdk_package_name": "system-images;android-24;google_apis_playstore;x86",
                "cipd_yaml": "third_party/android_sdk/cipd/system_images/android-24/google_apis_playstore/x86.yaml",
            },
            {
                "sdk_package_name": "system-images;android-25;google_apis;x86",
                "cipd_yaml": "third_party/android_sdk/cipd/system_images/android-25/google_apis/x86.yaml",
            },
            {
                "sdk_package_name": "system-images;android-25;google_apis_playstore;x86",
                "cipd_yaml": "third_party/android_sdk/cipd/system_images/android-25/google_apis_playstore/x86.yaml",
            },
            {
                "sdk_package_name": "system-images;android-27;google_apis;x86",
                "cipd_yaml": "third_party/android_sdk/cipd/system_images/android-27/google_apis/x86.yaml",
            },
            {
                "sdk_package_name": "system-images;android-27;google_apis_playstore;x86",
                "cipd_yaml": "third_party/android_sdk/cipd/system_images/android-27/google_apis_playstore/x86.yaml",
            },
            {
                "sdk_package_name": "system-images;android-28;google_apis;x86",
                "cipd_yaml": "third_party/android_sdk/cipd/system_images/android-28/google_apis/x86.yaml",
            },
            {
                "sdk_package_name": "system-images;android-28;google_apis_playstore;x86",
                "cipd_yaml": "third_party/android_sdk/cipd/system_images/android-28/google_apis_playstore/x86.yaml",
            },
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
                "sdk_package_name": "system-images;android-31;google_apis_playstore;x86_64",
                "cipd_yaml": "third_party/android_sdk/cipd/system_images/android-31/google_apis_playstore/x86_64.yaml",
            },
            {
                "sdk_package_name": "system-images;android-32;google_apis;x86_64",
                "cipd_yaml": "third_party/android_sdk/cipd/system_images/android-32/google_apis/x86_64.yaml",
            },
            {
                "sdk_package_name": "system-images;android-32;google_apis_playstore;x86_64",
                "cipd_yaml": "third_party/android_sdk/cipd/system_images/android-32/google_apis_playstore/x86_64.yaml",
            },
            {
                "sdk_package_name": "system-images;android-33;google_apis;x86_64",
                "cipd_yaml": "third_party/android_sdk/cipd/system_images/android-33/google_apis/x86_64.yaml",
            },
            {
                "sdk_package_name": "system-images;android-33;google_apis_playstore;x86_64",
                "cipd_yaml": "third_party/android_sdk/cipd/system_images/android-33/google_apis_playstore/x86_64.yaml",
            },
        ],
    },
    schedule = "0 7 * * *",
    triggered_by = [],
)

ci.builder(
    name = "rts-model-packager",
    builderless = False,
    console_view_entry = consoles.console_view_entry(
        category = "rts",
        short_name = "create-model",
    ),
    cores = None,
    executable = "recipe:chromium_rts/create_model",
    execution_timeout = 8 * time.hour,
    notifies = [
        luci.notifier(
            name = "rts-model-packager-notifier",
            on_occurrence = ["FAILURE", "INFRA_FAILURE"],
            notify_emails = ["chrome-browser-infra-team@google.com"],
        ),
    ],
    schedule = "0 9 * * *",  # at 1AM or 2AM PT (depending on DST), once a day.
    triggered_by = [],
)
