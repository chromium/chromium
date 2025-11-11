# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

load("@chromium-luci//builder_config.star", "builder_config")
load("@chromium-luci//builder_health_indicators.star", "health_spec")
load("@chromium-luci//builders.star", "os")
load("@chromium-luci//ci.star", "ci")
load("@chromium-luci//consoles.star", "consoles")
load("//lib/ci_constants.star", "ci_constants")
load("//project.star", "settings")

luci.gitiles_poller(
    name = "history-rag-gitiles-trigger",
    bucket = "ci",
    repo = "https://chromium.googlesource.com/chromium/src",
    refs = [settings.ref],
    # Trigger once a day at 12 am PST
    schedule = "0 8 * * *",
)

consoles.console_view(
    name = "chromium.history_rag",
    repo = "https://chromium.googlesource.com/chromium/src/",
    refs = ["refs/heads/main"],
    title = "Chromium History RAG CI Builders",
)

ci.defaults.set(
    builder_group = "chromium.history_rag",
    pool = "linux-builder-perf",
    cores = 32,
    os = os.LINUX_DEFAULT,
    execution_timeout = 3 * time.hour,
    health_spec = health_spec.default(),
    priority = ci_constants.DEFAULT_FYI_PRIORITY,
    service_account = ci_constants.DEFAULT_SERVICE_ACCOUNT,
    shadow_service_account = ci_constants.DEFAULT_SHADOW_SERVICE_ACCOUNT,
)

def history_rag_builder(**kwargs):
    return ci.builder(
        schedule = "triggered",
        triggered_by = ["history-rag-gitiles-trigger"],
        # TODO(crbug.com/455899383): Change this to history_rag recipe
        executable = ci_constants.DEFAULT_EXECUTABLE,
        triggering_policy = scheduler.greedy_batching(
            max_concurrent_invocations = 1,
        ),
        **kwargs
    )

history_rag_builder(
    name = "linux-history-rag",
    description_html = "Generates index for Chromium History RAG",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = ["mb"],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
            target_platform = builder_config.target_platform.LINUX,
        ),
    ),
    os = os.LINUX_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "linux",
        short_name = "lnx",
    ),
    contact_team_email = "chrome-mlsearch@google.com",
)
