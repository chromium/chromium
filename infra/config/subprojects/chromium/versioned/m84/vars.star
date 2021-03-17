# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

vars = struct(
    is_main = False,
    ref = "refs/branch-heads/4147",
    ci_bucket = "ci-m84",
    ci_poller = "m84-gitiles-trigger",
    main_console_name = "main-m84",
    main_console_title = "Chromium M84 Console",
    cq_mirrors_console_name = "mirrors-m84",
    cq_mirrors_console_title = "Chromium M84 CQ Mirrors Console",
    try_bucket = "try-m84",
    try_triggering_projects = [],
    cq_group = "cq-m84",
    cq_ref_regexp = "refs/branch-heads/4147",
    main_list_view_name = "try-m84",
    main_list_view_title = "Chromium M84 CQ console",
    tree_status_host = None,
)
