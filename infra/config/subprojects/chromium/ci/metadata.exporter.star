# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Definitions of builders in the metadata.exporter builder group."""

load("//lib/builders.star", "os")
load("//lib/ci.star", "ci")
load("//lib/consoles.star", "consoles")

ci.defaults.set(
    cores = 8,
    execution_timeout = ci.DEFAULT_EXECUTION_TIMEOUT,
    os = os.LINUX_DEFAULT,
    pool = ci.DEFAULT_POOL,
)

consoles.console_view(
    name = "metadata.exporter",
    header = None,
)

description = """
This builder exports info contained in all the DIR_METADATA files throughout
Chromium. It gets exported to Google Storage and BigQuery and is then consumed
by various services.<br/>For more info, see documentation on the
<a href="https://chromium.googlesource.com/infra/infra/+/HEAD/go/src/infra/tools/dirmd">dirmd</a>
tool.
"""

ci.builder(
    name = "metadata-exporter",
    console_view_entry = consoles.console_view_entry(
        console_view = "metadata.exporter",
    ),
    executable = "recipe:chromium_export_metadata",
    notifies = "metadata-mapping",
    service_account = "component-mapping-updater@chops-service-accounts.iam.gserviceaccount.com",
    description_html = description,
)
