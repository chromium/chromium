# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

load("//lib/builders.star", "cpu", "os", "siso")
load("//lib/consoles.star", "consoles")
load("//lib/gn_args.star", "gn_args")
load("//lib/try.star", "try_")

luci.bucket(
    name = "codesearch",
    acls = [
        acl.entry(
            roles = acl.BUILDBUCKET_READER,
            groups = "all",
        ),
        acl.entry(
            roles = acl.BUILDBUCKET_TRIGGERER,
            groups = "project-chromium-tryjob-access",
        ),
    ],
)

try_.defaults.set(
    bucket = "codesearch",
    executable = "recipe:chromium_codesearch",
    builder_group = "tryserver.chromium.codesearch",
    pool = "luci.chromium.try",
    builderless = True,
    cores = 8,
    os = os.LINUX_DEFAULT,
    cpu = cpu.X86_64,
    build_numbers = True,
    cq_group = "cq",
    execution_timeout = 9 * time.hour,
    expiration_timeout = 2 * time.hour,
    service_account = "chromium-try-builder@chops-service-accounts.iam.gserviceaccount.com",
    siso_enabled = True,
    siso_project = siso.project.DEFAULT_UNTRUSTED,
    siso_remote_jobs = siso.remote_jobs.LOW_JOBS_FOR_CQ,
)

consoles.list_view(
    name = "tryserver.chromium.codesearch",
)

try_.builder(
    name = "gen-android-try",
    gn_args = gn_args.config(
        configs = [
            "codesearch_builder",
            "clang",
            "debug_builder",
            "minimal_symbols",
            "remoteexec",
            "android_builder_without_codecs",
            "static",
            "arm",
        ],
    ),
    properties = {
        "recipe_properties": {
            "build_config": "android",
            "platform": "android",
        },
    },
)

try_.builder(
    name = "gen-chromiumos-try",
    gn_args = gn_args.config(
        configs = [
            "codesearch_builder",
            "clang",
            "debug_builder",
            "minimal_symbols",
            "remoteexec",
            "chromeos",
            "use_cups",
            "x64",
        ],
    ),
    properties = {
        "recipe_properties": {
            "build_config": "chromeos",
            "platform": "chromeos",
        },
    },
)

try_.builder(
    name = "gen-fuchsia-try",
    gn_args = gn_args.config(
        configs = [
            "codesearch_builder",
            "release_builder",
            "remoteexec",
            "fuchsia",
            "cast_receiver",
            "x64",
        ],
    ),
    properties = {
        "recipe_properties": {
            "build_config": "fuchsia",
            "platform": "fuchsia",
        },
    },
)

try_.builder(
    name = "gen-ios-try",
    gn_args = gn_args.config(
        configs = [
            "codesearch_builder",
            "remoteexec",
            "clang",
            "debug",
            "minimal_symbols",
            "ios",
            "ios_disable_code_signing",
            "arm64",
        ],
    ),
    cores = None,
    os = os.MAC_DEFAULT,
    cpu = cpu.ARM64,
    properties = {
        "recipe_properties": {
            "build_config": "ios",
            "platform": "ios",
        },
        "xcode_build_version": "15a240d",
    },
)

try_.builder(
    name = "gen-linux-try",
    gn_args = gn_args.config(
        configs = [
            "codesearch_builder",
            "clang",
            "debug_builder",
            "minimal_symbols",
            "remoteexec",
            "linux",
            "x64",
        ],
    ),
)

try_.builder(
    name = "gen-mac-try",
    gn_args = gn_args.config(
        configs = [
            "codesearch_builder",
            "clang",
            "debug_builder",
            "minimal_symbols",
            "remoteexec",
            "mac",
            "arm64",
        ],
    ),
    cores = None,
    os = os.MAC_DEFAULT,
    cpu = cpu.ARM64,
    properties = {
        "recipe_properties": {
            "build_config": "mac",
            "platform": "mac",
        },
    },
)

try_.builder(
    name = "gen-webview-try",
    gn_args = gn_args.config(
        configs = [
            "codesearch_builder",
            "clang",
            "debug_builder",
            "remoteexec",
            "android_builder_without_codecs",
            "static",
            "arm",
        ],
    ),
    properties = {
        "recipe_properties": {
            "build_config": "webview",
            "platform": "webview",
        },
    },
)

try_.builder(
    name = "gen-win-try",
    gn_args = gn_args.config(
        configs = [
            "codesearch_builder",
            "clang",
            "debug_builder",
            "minimal_symbols",
            "remoteexec",
            "win",
            "x64",
        ],
    ),
    os = os.WINDOWS_10,
    properties = {
        "recipe_properties": {
            "platform": "win",
        },
    },
)
