# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

load("//lib/builders.star", "cpu", "goma", "os")
load("//lib/consoles.star", "consoles")
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
try_.defaults.goma_backend.set(goma.backend.RBE_PROD)
try_.defaults.os.set(os.LINUX_DEFAULT)
try_.defaults.pool.set("luci.chromium.try")
try_.defaults.service_account.set("chromium-try-builder@chops-service-accounts.iam.gserviceaccount.com")

try_.defaults.caches.set([
    swarming.cache(
        name = "win_toolchain",
        path = "win_toolchain",
    ),
])

consoles.list_view(
    name = "tryserver.chromium.codesearch",
)

try_.builder(
    name = "gen-android-try",
)

try_.builder(
    name = "gen-chromiumos-try",
)

try_.builder(
    name = "gen-fuchsia-try",
)

try_.builder(
    name = "gen-lacros-try",
)

try_.builder(
    name = "gen-linux-try",
)

try_.builder(
    name = "gen-mac-try",
    os = os.MAC_10_15,
)

try_.builder(
    name = "gen-webview-try",
)

try_.builder(
    name = "gen-win-try",
    os = os.WINDOWS_10,
    properties = {
        "recipe_properties": {
            "platform": "win",
        },
    },
)
