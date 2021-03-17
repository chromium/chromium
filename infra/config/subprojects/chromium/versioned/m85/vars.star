# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

vars = struct(
    is_main = False,
    ref = "refs/branch-heads/4183",
    ci_bucket = "ci-m85",
    ci_poller = "m85-gitiles-trigger",
    main_console_name = "main-m85",
    main_console_title = "Chromium M85 Console",
    cq_mirrors_console_name = "mirrors-m85",
    cq_mirrors_console_title = "Chromium M85 CQ Mirrors Console",
    try_bucket = "try-m85",
    try_triggering_projects = [],
    cq_group = "cq-m85",
    cq_ref_regexp = "refs/branch-heads/4183",
    main_list_view_name = "try-m85",
    main_list_view_title = "Chromium M85 CQ console",
    tree_status_host = None,
)
