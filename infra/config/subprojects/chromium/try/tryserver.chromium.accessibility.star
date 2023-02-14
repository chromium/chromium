# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Definitions of builders in the tryserver.chromium.accessibility builder group."""

load("//lib/builders.star", "goma", "os")
load("//lib/try.star", "try_")
load("//lib/consoles.star", "consoles")

try_.defaults.set(
    executable = try_.DEFAULT_EXECUTABLE,
    builder_group = "tryserver.chromium.accessibility",
    pool = try_.DEFAULT_POOL,
    cores = 8,
    os = os.LINUX_DEFAULT,
    compilator_cores = 16,
    compilator_goma_jobs = goma.jobs.J150,
    execution_timeout = try_.DEFAULT_EXECUTION_TIMEOUT,
    goma_backend = goma.backend.RBE_PROD,

    # TODO(crbug.com/1362440): remove this.
    omit_python2 = False,
    service_account = try_.DEFAULT_SERVICE_ACCOUNT,
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
