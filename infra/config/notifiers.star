# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

load("@chromium-luci//notifiers.star", "notifiers")

luci.notifier(
    name = "chrome-fuzzing-core",
    on_status_change = True,
    notify_emails = [
        "chrome-fuzzing-core+bots@google.com",
    ],
)

luci.notifier(
    name = "chrome-rust-experiments",
    on_new_status = ["FAILURE", "INFRA_FAILURE"],
    notify_emails = [
        "chrome-rust-experiments+bots@google.com",
    ],
)

# Notifier for "package rust" step on *_upload_clang bots.
luci.notifier(
    name = "chrome-rust-toolchain",
    # Watch for Rust failure regardless of the overall build status.
    on_occurrence = ["SUCCESS", "FAILURE", "INFRA_FAILURE"],
    failed_step_regexp = "package rust",
    notify_emails = [
        "chrome-rust-experiments+toolchain@google.com",
    ],
)

luci.notifier(
    name = "chromium-android-device-flasher",
    # android-device-flasher runs only once a week. So have it sends
    # notifications whenever there is an infra failure.
    on_occurrence = ["INFRA_FAILURE"],
    notify_emails = [
        "chromium-infra+failures@google.com",
    ],
)

luci.notifier(
    name = "chromium-androidx-packager",
    on_new_status = ["FAILURE"],
    notify_emails = [
        "clank-library-failures+androidx@google.com",
    ],
)

luci.notifier(
    name = "chromium-infra",
    on_new_status = ["FAILURE", "INFRA_FAILURE"],
    notify_emails = [
        "chromium-infra+failures@google.com",
    ],
)

luci.notifier(
    name = "codeql-infra",
    on_status_change = True,
    notify_emails = [
        "flowerhack@google.com",
    ],
)

luci.notifier(
    name = "cr-fuchsia",
    on_status_change = True,
    notify_emails = [
        "chrome-fuchsia-engprod+builder-notification@grotations.appspotmail.com",
    ],
)

luci.notifier(
    name = "cronet",
    on_new_status = ["FAILURE", "INFRA_FAILURE", "SUCCESS"],
    notify_emails = [
        "cronet-sheriff@grotations.appspotmail.com",
    ],
)

luci.notifier(
    name = "metadata-mapping",
    on_new_status = ["FAILURE"],
    notify_emails = [
        "chromium-component-mapping@google.com",
        "chanli@google.com",
    ],
)

TREE_CLOSING_STEPS_REGEXP = "\\b({})\\b".format("|".join([
    "bot_update",
    "compile",
    "gclient runhooks",
    "generate_build_files",
    "runhooks",
    "update",
    "\\w*nocompile_test",
]))

# This results in a notifier with no recipients, so nothing will actually be
# notified. This still creates a "notifiable" that can be passed to the notifies
# argument of a builder, so conditional logic doesn't need to be used when
# setting the argument and erroneous tree closure notifications won't be sent
# for failures on branches.
def _empty_notifier(*, name):
    luci.notifier(
        name = name,
        on_new_status = ["INFRA_FAILURE"],
    )

def tree_closer(*, name, tree_status_host, **kwargs):
    if notifiers.tree_closer_branch():
        luci.tree_closer(
            name = name,
            tree_status_host = tree_status_host,
            **kwargs
        )
    else:
        _empty_notifier(name = name)

tree_closer(
    name = "chromium-tree-closer",
    tree_status_host = "chromium-status.appspot.com",
    failed_step_regexp = TREE_CLOSING_STEPS_REGEXP,
)

tree_closer(
    name = "close-on-any-step-failure",
    tree_status_host = "chromium-status.appspot.com",
)

def tree_closure_notifier(*, name, **kwargs):
    if notifiers.tree_closer_branch():
        luci.notifier(
            name = name,
            on_occurrence = ["FAILURE"],
            failed_step_regexp = TREE_CLOSING_STEPS_REGEXP,
            **kwargs
        )
    else:
        _empty_notifier(name = name)

tree_closure_notifier(
    name = "chromium-tree-closer-email",
    notify_rotation_urls = [
        "https://chrome-ops-rotation-proxy.appspot.com/current/oncallator:chrome-build-sheriff",
    ],
    template = luci.notifier_template(
        name = "tree_closure_email_template",
        body = io.read_file("templates/tree_closure_email.template"),
    ),
)

tree_closure_notifier(
    name = "gpu-tree-closer-email",
    notify_emails = ["chrome-gpu-build-failures@google.com"],
    notify_rotation_urls = [
        "https://chrome-ops-rotation-proxy.appspot.com/current/oncallator:chrome-gpu-pixel-wrangler-weekly",
    ],
)

tree_closure_notifier(
    name = "linux-memory",
    notify_emails = ["thomasanderson@chromium.org"],
)

tree_closure_notifier(
    name = "linux-archive-rel",
    notify_emails = ["thomasanderson@chromium.org"],
)

tree_closure_notifier(
    name = "Deterministic Android",
    notify_emails = ["agrieve@chromium.org"],
)

tree_closure_notifier(
    name = "Deterministic Linux",
    notify_emails = [
        "tikuta@chromium.org",
        "ukai@chromium.org",
        "yyanagisawa@chromium.org",
    ],
)

luci.notifier(
    name = "Site Isolation Android",
    notify_emails = [
        "nasko+fyi-bots@chromium.org",
        "creis+fyi-bots@chromium.org",
        "lukasza+fyi-bots@chromium.org",
        "alexmos+fyi-bots@chromium.org",
    ],
    on_new_status = ["FAILURE"],
)

luci.notifier(
    name = "CFI Linux",
    notify_emails = [
        "pcc@chromium.org",
    ],
    on_new_status = ["FAILURE"],
)

luci.notifier(
    name = "Win 10 Fast Ring",
    notify_emails = [
        "wfh@chromium.org",
    ],
    on_new_status = ["FAILURE"],
)

luci.notifier(
    name = "linux-blink-fyi-bots",
    notify_emails = [
        "mlippautz+fyi-bots@chromium.org",
    ],
    on_new_status = ["FAILURE"],
)

luci.notifier(
    name = "annotator-rel",
    notify_emails = [
        "crmullins@chromium.org",
        "nicolaso@chromium.org",
        "pastarmovj@chromium.org",
    ],
    on_new_status = ["FAILURE"],
)

luci.notifier(
    name = "headless-owners",
    notify_emails = [
        "headless-owners@chromium.org",
    ],
    on_new_status = ["FAILURE"],
)

tree_closure_notifier(
    name = "chromium.linux",
    notify_emails = [
        "thomasanderson@chromium.org",
    ],
)

luci.notifier(
    name = "cr-accessibility",
    notify_emails = [
        "chrome-a11y-alerts@google.com",
    ],
    on_new_status = ["FAILURE"],
)

luci.notifier(
    name = "chrometto-sheriff",
    notify_emails = [
        "chrometto-sheriff-oncall@google.com",
    ],
    on_new_status = ["FAILURE"],
)

luci.notifier(
    name = "peeps-security-core-ssci",
    notify_emails = [
        "chops-security-core+ssci-alert@google.com",
    ],
    on_occurrence = ["FAILURE"],
    on_new_status = ["SUCCESS", "INFRA_FAILURE"],
    template = luci.notifier_template(
        name = "build_with_step_summary_template",
        body = io.read_file("templates/build_with_step_summary.template"),
    ),
)

luci.notifier(
    name = "Chromium Build Time Watcher",
    notify_emails = [
        "jwata@google.com",
        "pasthana@google.com",
        "thakis@google.com",
    ],
    on_new_status = ["INFRA_FAILURE"],
)

luci.notifier(
    name = "chrome-fake-vaapi-test",
    on_occurrence = ["SUCCESS", "FAILURE", "INFRA_FAILURE"],
    failed_step_regexp = "video_decode_accelerator_tests_fake_vaapi.*",
    notify_emails = [
        "bchoobineh@google.com",
    ],
)

luci.notifier(
    name = "chrome-v4l2-visl-test",
    on_occurrence = ["SUCCESS", "FAILURE", "INFRA_FAILURE"],
    failed_step_regexp = "video_decode_accelerator_tests_v4l2*",
    notify_emails = [
        "stevecho@google.com",
    ],
)

luci.notifier(
    name = "multiscreen-owners",
    on_new_status = ["FAILURE"],
    notify_emails = [
        "web-windowing-team@google.com",
    ],
)

luci.builder_health_notifier(
    owner_email = "chrome-browser-infra-team@google.com",
    ignore_buckets = ["try"],
    notify_all_healthy = True,
    gardened_builders_only = True,
)

luci.builder_health_notifier(
    owner_email = "chrome-fuchsia-engprod@google.com",
    ignore_buckets = ["try"],
    notify_all_healthy = False,
    gardened_builders_only = True,
)

luci.builder_health_notifier(
    owner_email = "clank-build@google.com",
    ignore_buckets = ["try"],
    notify_all_healthy = False,
    gardened_builders_only = True,
)

luci.builder_health_notifier(
    owner_email = "clank-engprod@google.com",
    ignore_buckets = ["try"],
    notify_all_healthy = False,
    gardened_builders_only = True,
)

luci.builder_health_notifier(
    owner_email = "chrome-gpu-infra@google.com",
    ignore_buckets = ["try"],
    notify_all_healthy = False,
    gardened_builders_only = True,
)

luci.builder_health_notifier(
    owner_email = "angle-team@google.com",
    ignore_buckets = ["try"],
    notify_all_healthy = False,
    gardened_builders_only = True,
)

luci.builder_health_notifier(
    owner_email = "browser-automation-staff@google.com",
    ignore_buckets = ["try"],
    notify_all_healthy = False,
    gardened_builders_only = True,
)

luci.builder_health_notifier(
    owner_email = "cronet-team@google.com",
    ignore_buckets = ["try"],
    notify_all_healthy = False,
    gardened_builders_only = True,
)

luci.builder_health_notifier(
    owner_email = "cronet-sheriff@google.com",
    ignore_buckets = ["try"],
    notify_all_healthy = False,
    gardened_builders_only = True,
)

luci.builder_health_notifier(
    owner_email = "bling-engprod@google.com",
    ignore_buckets = ["try"],
    notify_all_healthy = False,
    gardened_builders_only = True,
)

luci.builder_health_notifier(
    owner_email = "chromeos-chrome-build@google.com",
    ignore_buckets = ["try"],
    notify_all_healthy = False,
    gardened_builders_only = True,
)

luci.builder_health_notifier(
    owner_email = "core-devices-eng@google.com",
    ignore_buckets = ["try"],
    notify_all_healthy = False,
    gardened_builders_only = True,
)

luci.builder_health_notifier(
    owner_email = "chrome-fuzzing-core@google.com",
    ignore_buckets = ["try"],
    notify_all_healthy = False,
    gardened_builders_only = True,
)

luci.builder_health_notifier(
    owner_email = "v8-infra@google.com",
    ignore_buckets = ["try"],
    notify_all_healthy = False,
    gardened_builders_only = True,
)

luci.builder_health_notifier(
    owner_email = "chrome-desktop-engprod@google.com",
    ignore_buckets = ["try"],
    notify_all_healthy = False,
    gardened_builders_only = True,
)

luci.builder_health_notifier(
    owner_email = "chrome-build-team@google.com",
    ignore_buckets = ["try"],
    notify_all_healthy = False,
    gardened_builders_only = True,
)

luci.builder_health_notifier(
    owner_email = "cast-eng@google.com",
    ignore_buckets = ["try"],
    notify_all_healthy = False,
    gardened_builders_only = True,
)

luci.builder_health_notifier(
    owner_email = "chrome-linux-engprod@google.com",
    ignore_buckets = ["try"],
    notify_all_healthy = False,
    gardened_builders_only = True,
)

luci.builder_health_notifier(
    owner_email = "chrome-security-architecture@google.com",
    ignore_buckets = ["try"],
    notify_all_healthy = False,
    gardened_builders_only = True,
)

luci.builder_health_notifier(
    owner_email = "omaha-core@google.com",
    ignore_buckets = ["try"],
    notify_all_healthy = False,
)
