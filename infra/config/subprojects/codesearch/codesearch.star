# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

load("//lib/builders.star", "cpu", "os", "reclient")
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
        acl.entry(
            roles = acl.BUILDBUCKET_OWNER,
            groups = "service-account-chromium-tryserver",
        ),
    ],
)

try_.defaults.bucket.set("codesearch")
try_.defaults.build_numbers.set(True)
try_.defaults.builder_group.set("tryserver.chromium.codesearch")
try_.defaults.builderless.set(True)
try_.defaults.cores.set(8)
try_.defaults.cpu.set(cpu.X86_64)
try_.defaults.cq_group.set("cq")
try_.defaults.executable.set("recipe:chromium_codesearch")
try_.defaults.execution_timeout.set(9 * time.hour)
try_.defaults.expiration_timeout.set(2 * time.hour)
try_.defaults.os.set(os.LINUX_DEFAULT)
try_.defaults.pool.set("luci.chromium.try")
try_.defaults.reclient_instance.set(reclient.instance.DEFAULT_UNTRUSTED)
try_.defaults.reclient_jobs.set(reclient.jobs.LOW_JOBS_FOR_CQ)
try_.defaults.service_account.set("chromium-try-builder@chops-service-accounts.iam.gserviceaccount.com")

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
            "reclient",
            "android_builder_without_codecs",
            "static",
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
            "reclient",
            "chromeos",
            "use_cups",
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
            "reclient",
            "fuchsia",
            "cast_receiver",
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
            "reclient",
            "clang",
            "debug",
            "minimal_symbols",
            "ios",
            "ios_disable_code_signing",
        ],
    ),
    os = os.MAC_DEFAULT,
    cpu = cpu.ARM64,
    properties = {
        "recipe_properties": {
            "build_config": "ios",
            "platform": "ios",
        },
    },
)

try_.builder(
    name = "gen-lacros-try",
    gn_args = gn_args.config(
        configs = [
            "codesearch_builder",
            "clang",
            "debug_builder",
            "minimal_symbols",
            "reclient",
            "lacros_on_linux",
            "use_cups",
        ],
    ),
    properties = {
        "recipe_properties": {
            "build_config": "lacros",
            "platform": "lacros",
        },
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
            "reclient",
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
            "reclient",
            "mac",
        ],
    ),
    os = os.MAC_10_15,
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
            "reclient",
            "android_builder_without_codecs",
            "static",
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
            "reclient",
        ],
    ),
    os = os.WINDOWS_10,
    properties = {
        "recipe_properties": {
            "platform": "win",
        },
    },
)
