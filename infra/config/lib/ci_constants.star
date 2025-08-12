# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

ci_constants = struct(
    DEFAULT_EXECUTABLE = "recipe:chromium",
    DEFAULT_EXECUTION_TIMEOUT = 3 * time.hour,
    DEFAULT_FYI_PRIORITY = 35,
    DEFAULT_POOL = "luci.chromium.ci",
    DEFAULT_SHADOW_POOL = "luci.chromium.try",
    DEFAULT_SERVICE_ACCOUNT = "chromium-ci-builder@chops-service-accounts.iam.gserviceaccount.com",
    DEFAULT_SHADOW_SERVICE_ACCOUNT = "chromium-try-builder@chops-service-accounts.iam.gserviceaccount.com",
    DEFAULT_TREE_CLOSING_NOTIFIERS = ["chromium-tree-closer", "chromium-tree-closer-email"],
)
