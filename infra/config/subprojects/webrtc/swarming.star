# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

load("//lib/swarming.star", "swarming")

# WebRTC pools have additional ACLs to allow LED for project-webrtc-admins.
swarming.pool_realm(
    name = "pools/webrtc",
    extends = "pools/ci",
)
swarming.task_triggerers(
    builder_realm = ["webrtc", "webrtc.fyi"],
    pool_realm = "pools/webrtc",
    groups = ["project-webrtc-admins"],
)
