# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

load("//lib/branches.star", "branches")

luci.notifier(
    name = "chromesec-lkgr-failures",
    on_status_change = True,
    notify_emails = [
        "chromesec-lkgr-failures@google.com",
    ],
)

luci.notifier(
    name = "chrome-memory-sheriffs",
    on_status_change = True,
    notify_emails = [
        "chrome-memory-sheriffs+bots@google.com",
    ],
)

luci.notifier(
    name = "cr-fuchsia",
    on_status_change = True,
    notify_emails = [
        "cr-fuchsia+bot@chromium.org",
        "chrome-fuchsia-gardener@grotations.appspotmail.com",
    ],
)

luci.notifier(
    name = "cronet",
    on_status_change = True,
    notify_emails = [
        "cronet-bots-observer@google.com",
    ],
)

luci.notifier(
    name = "metadata-mapping",
    on_new_status = ["FAILURE"],
    notify_emails = ["chromium-component-mapping@google.com"],
)

TREE_CLOSING_STEPS = [
    "bot_update",
    "compile",
    "gclient runhooks",
    "runhooks",
    "update",
]

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
    if branches.matches(branches.MAIN_ONLY):
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
    failed_step_regexp = TREE_CLOSING_STEPS,
)

tree_closer(
    name = "close-on-any-step-failure",
    tree_status_host = "chromium-status.appspot.com",
)

def tree_closure_notifier(*, name, **kwargs):
    if branches.matches(branches.MAIN_ONLY):
        luci.notifier(
            name = name,
            on_occurrence = ["FAILURE"],
            failed_step_regexp = TREE_CLOSING_STEPS,
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
        "https://rota-ng.appspot.com/legacy/sheriff_gpu.json",
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

tree_closure_notifier(
    name = "linux-ozone-rel",
    notify_emails = [
        "fwang@chromium.org",
        "maksim.sisov@chromium.org",
        "rjkroege@chromium.org",
        "thomasanderson@chromium.org",
        "timbrown@chromium.org",
        "tonikitoo@chromium.org",
    ],
)

luci.notifier(
    name = "Closure Compilation Linux",
    notify_emails = [
        "dbeam+closure-bots@chromium.org",
        "fukino+closure-bots@chromium.org",
        "hirono+closure-bots@chromium.org",
        "vitalyp@chromium.org",
    ],
    on_occurrence = ["FAILURE"],
    failed_step_regexp = [
        "update_scripts",
        "setup_build",
        "bot_update",
        "generate_gyp_files",
        "compile",
        "generate_v2_gyp_files",
        "compile_v2",
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
    name = "linux-blink-heap-verification",
    notify_emails = [
        "mlippautz+fyi-bots@chromium.org",
    ],
    on_new_status = ["FAILURE"],
)

luci.notifier(
    name = "annotator-rel",
    notify_emails = [
        "pastarmovj@chromium.org",
        "nicolaso@chromium.org",
    ],
    on_new_status = ["FAILURE"],
)

tree_closure_notifier(
    name = "chromium.linux",
    notify_emails = [
        "thomasanderson@chromium.org",
    ],
)
