# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Definitions of builders in the metadata.exporter builder group."""

load("//lib/builders.star", "goma", "os", "sheriff_rotations")
load("//lib/ci.star", "ci")
load("//lib/consoles.star", "consoles")

ci.defaults.set(
    cores = 8,
    execution_timeout = ci.DEFAULT_EXECUTION_TIMEOUT,
    os = os.LINUX_BIONIC_SWITCH_TO_DEFAULT,
    pool = ci.DEFAULT_POOL,
)

consoles.console_view(
    name = "metadata.exporter",
    header = None,
)

ci.builder(
    name = "metadata-exporter",
    # TODO(gbeaty) Remove this after prod freeze, the recipe doesn't require a
    # builder group
    builder_group = "chromium.linux",
    console_view_entry = consoles.console_view_entry(
        console_view = "metadata.exporter",
    ),
    executable = "recipe:chromium_export_metadata",
    # TODO(gbeaty) Remove the goma values after prod freeze, the recipe doesn't
    # do builds
    goma_backend = goma.backend.RBE_PROD,
    goma_jobs = goma.jobs.MANY_JOBS_FOR_CI,
    notifies = "metadata-mapping",
    service_account = "component-mapping-updater@chops-service-accounts.iam.gserviceaccount.com",
    sheriff_rotations = sheriff_rotations.CHROMIUM,
)
