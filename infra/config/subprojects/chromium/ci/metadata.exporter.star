# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Definitions of builders in the metadata.exporter builder group."""

load("@chromium-luci//builder_health_indicators.star", "health_spec")
load("@chromium-luci//builders.star", "os")
load("@chromium-luci//ci.star", "ci")
load("@chromium-luci//consoles.star", "consoles")
load("@chromium-luci//html.star", "linkify")
load("//lib/ci_constants.star", "ci_constants")

ci.defaults.set(
    pool = ci_constants.DEFAULT_POOL,
    cores = 8,
    os = os.LINUX_DEFAULT,
    execution_timeout = ci_constants.DEFAULT_EXECUTION_TIMEOUT,
    experiments = {
        "chromium_tests.resultdb_module": 100,
    },
    health_spec = health_spec.default(),
)

consoles.console_view(
    name = "metadata.exporter",
    header = None,
)

description = """
This builder exports info contained in all the DIR_METADATA files throughout
Chromium. It gets exported to Google Storage and BigQuery and is then consumed
by various services.<br/>For more info, see documentation on the {} tool.
""".format(linkify("https://chromium.googlesource.com/infra/infra/+/HEAD/go/src/infra/tools/dirmd", "dirmd"))

ci.builder(
    name = "metadata-exporter",
    description_html = description,
    executable = "recipe:chromium_export_metadata",
    console_view_entry = consoles.console_view_entry(
        console_view = "metadata.exporter",
    ),
    notifies = "metadata-mapping",
    service_account = "component-mapping-updater@chops-service-accounts.iam.gserviceaccount.com",
    shadow_service_account = "chromium-try-builder@chops-service-accounts.iam.gserviceaccount.com",
)
