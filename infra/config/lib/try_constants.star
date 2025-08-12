# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

try_constants = struct(
    DEFAULT_EXECUTABLE = "recipe:chromium_trybot",
    DEFAULT_EXECUTION_TIMEOUT = 4 * time.hour,
    DEFAULT_POOL = "luci.chromium.try",
    DEFAULT_SERVICE_ACCOUNT = "chromium-try-builder@chops-service-accounts.iam.gserviceaccount.com",
)
