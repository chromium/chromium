# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
luci.realm(
    name = "build",
    bindings = [
        # Allow FYI builders to create invocations in their own builds.
        luci.binding(
            roles = "role/resultdb.invocationCreator",
            users = "chromium-build-perf-ci-builder@chops-service-accounts.iam.gserviceaccount.com",
        ),
    ],
)

exec("./build.star")
exec("./build.fyi.star")
