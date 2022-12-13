# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Definitions of builders in the tryserver.chromium.accessibility builder group."""

load("//lib/builders.star", "os", "reclient")
load("//lib/try.star", "try_")
load("//lib/consoles.star", "consoles")

try_.defaults.set(
    builder_group = "tryserver.chromium.accessibility",
    executable = try_.DEFAULT_EXECUTABLE,
    cores = 8,
    os = os.LINUX_DEFAULT,
    pool = try_.DEFAULT_POOL,
    service_account = try_.DEFAULT_SERVICE_ACCOUNT,
    compilator_cores = 16,
    execution_timeout = try_.DEFAULT_EXECUTION_TIMEOUT,
    reclient_instance = reclient.instance.DEFAULT_UNTRUSTED,
    reclient_jobs = 150,
)

consoles.list_view(
    name = "tryserver.chromium.accessibility",
)

try_.builder(
    name = "linux-blink-web-tests-force-accessibility-rel",
    mirrors = ["ci/linux-blink-web-tests-force-accessibility-rel"],
    tryjob = try_.job(
        location_filters = [
            "third_party/blink/renderer/modules/accessibility/.+",
            "content/renderer/accessibility/.+",
            "content/browser/accessibility/.+",
            "ui/accessibility/.+",
        ],
    ),
)
