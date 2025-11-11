# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Definitions of builders in the tryserver.chromium.fuzz builder group."""

load("@chromium-luci//builders.star", "cpu", "os")
load("@chromium-luci//consoles.star", "consoles")
load("@chromium-luci//gn_args.star", "gn_args")
load("@chromium-luci//try.star", "try_")
load("//lib/siso.star", "siso")
load("//lib/try_constants.star", "try_constants")

try_.defaults.set(
    executable = try_constants.DEFAULT_EXECUTABLE,
    builder_group = "tryserver.chromium.fuzz",
    pool = try_constants.DEFAULT_POOL,
    builderless = True,
    cores = 8,
    os = os.LINUX_DEFAULT,
    execution_timeout = try_constants.DEFAULT_EXECUTION_TIMEOUT,
    experiments = {
        "chromium_tests.resultdb_module": 100,
    },
    service_account = try_constants.DEFAULT_SERVICE_ACCOUNT,
    siso_project = siso.project.DEFAULT_UNTRUSTED,
    siso_remote_jobs = siso.remote_jobs.HIGH_JOBS_FOR_CQ,
)

consoles.list_view(
    name = "tryserver.chromium.fuzz",
)

def _builder(mirror_of = None, **kwargs):
    try_.builder(
        mirrors = [mirror_of],
        gn_args = mirror_of,
        **kwargs
    )

def _mirror_builder(name = None, **kwargs):
    """Defines a builder that mirrors the CI builder of the same name."""
    _builder(
        name = name,
        mirror_of = "ci/" + name,
        contact_team_email = "chrome-fuzzing-core@google.com",
        **kwargs
    )

_builder(
    name = "linux-asan-dbg",
    mirror_of = "ci/ASAN Debug",
)

_builder(
    name = "linux-asan-rel",
    mirror_of = "ci/ASAN Release",
)

_builder(
    name = "linux-asan-media-rel",
    mirror_of = "ci/ASAN Release Media",
)

_builder(
    name = "linux-asan-v8-arm-dbg",
    mirror_of = "ci/ASan Debug (32-bit x86 with V8-ARM)",
)

_builder(
    name = "linux-asan-v8-arm-rel",
    mirror_of = "ci/ASan Release (32-bit x86 with V8-ARM)",
)

_builder(
    name = "linux-asan-media-v8-arm-rel",
    mirror_of = "ci/ASan Release Media (32-bit x86 with V8-ARM)",
)

_builder(
    name = "linux-asan-v8-sandbox-testing",
    contact_team_email = "v8-infra@google.com",
    mirror_of = "ci/ASAN Release V8 Sandbox Testing",
)

_builder(
    name = "linux-chromeos-asan-rel",
    mirror_of = "ci/ChromiumOS ASAN Release",
)

_builder(
    name = "linux-msan-chained-origins-rel",
    mirror_of = "ci/MSAN Release (chained origins)",
)

_builder(
    name = "linux-msan-no-origins-rel",
    mirror_of = "ci/MSAN Release (no origins)",
)

_builder(
    name = "linux-tsan-dbg",
    mirror_of = "ci/TSAN Debug",
)

_builder(
    name = "linux-tsan-rel",
    mirror_of = "ci/TSAN Release",
)

_builder(
    name = "linux-ubsan-rel",
    mirror_of = "ci/UBSan Release",
)

_builder(
    name = "linux-ubsan-vptr-rel",
    mirror_of = "ci/UBSan vptr Release",
)

_mirror_builder(name = "android-desktop-x64-libfuzzer-asan", executable = "recipe:chromium/fuzz")

_mirror_builder(name = "android-arm64-libfuzzer-hwasan", executable = "recipe:chromium/fuzz")

_builder(
    name = "mac-asan-rel",
    cores = None,
    os = os.MAC_DEFAULT,
    cpu = cpu.ARM64,
    mirror_of = "ci/Mac ASAN Release",
)

_builder(
    name = "mac-arm64-asan-rel",
    cores = None,
    os = os.MAC_DEFAULT,
    cpu = cpu.ARM64,
    contact_team_email = "chrome-sanitizer-builder-owners@google.com",
    mirror_of = "ci/Mac ARM64 ASAN Release",
)

_builder(
    name = "mac-asan-media-rel",
    cores = None,
    os = os.MAC_DEFAULT,
    mirror_of = "ci/Mac ASAN Release Media",
)

_builder(
    name = "win-asan-rel",
    os = os.WINDOWS_DEFAULT,
    mirror_of = "ci/Win ASan Release",
)

_builder(
    name = "win-asan-media-rel",
    os = os.WINDOWS_DEFAULT,
    mirror_of = "ci/Win ASan Release Media",
)

try_.builder(
    name = "linux-centipede-high-end-asan-dcheck",
    mirrors = ["ci/Centipede High End Upload Linux ASan DCheck"],
    gn_args = gn_args.config(
        configs = [
            "ci/Centipede High End Upload Linux ASan DCheck",
            "no_symbols",
            "skip_generate_fuzzer_owners",
        ],
    ),
    contact_team_email = "chrome-fuzzing-core@google.com",
)

try_.builder(
    name = "linux-libfuzzer-high-end-asan-rel",
    mirrors = ["ci/Libfuzzer High End Upload Linux ASan"],
    gn_args = gn_args.config(
        configs = [
            "ci/Libfuzzer High End Upload Linux ASan",
            "no_symbols",
            "skip_generate_fuzzer_owners",
        ],
    ),
    contact_team_email = "chrome-fuzzing-core@google.com",
)

try_.builder(
    name = "linux-libfuzzer-high-end-asan-dbg",
    mirrors = ["ci/Libfuzzer High End Upload Linux ASan Debug"],
    gn_args = gn_args.config(
        configs = [
            "ci/Libfuzzer High End Upload Linux ASan Debug",
            "no_symbols",
            "skip_generate_fuzzer_owners",
        ],
    ),
    contact_team_email = "chrome-fuzzing-core@google.com",
)

# Libfuzzer test bots.

_mirror_builder(name = "chromeos-x64-libfuzzer-asan-rel-tests")

_mirror_builder(name = "linux-x64-libfuzzer-asan-dbg-tests")

_mirror_builder(name = "linux-x64-libfuzzer-asan-rel-tests")

_mirror_builder(name = "linux-x64-libfuzzer-msan-rel-tests")

_mirror_builder(name = "linux-x64-libfuzzer-ubsan-rel-tests")

_mirror_builder(name = "linux-x86-libfuzzer-asan-rel-tests")

_mirror_builder(
    name = "mac-arm64-libfuzzer-asan-rel-tests",
    cores = None,
    os = os.MAC_DEFAULT,
    cpu = cpu.ARM64,
)

_mirror_builder(
    name = "win-x64-libfuzzer-asan-rel-tests",
    os = os.WINDOWS_DEFAULT,
)

_mirror_builder(name = "linux-x64-centipede-asan-rel-tests")
