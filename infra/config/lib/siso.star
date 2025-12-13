# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

siso = struct(
    project = struct(
        DEFAULT_TRUSTED = "rbe-chromium-trusted",
        TEST_TRUSTED = "rbe-chromium-trusted-test",
        DEFAULT_UNTRUSTED = "rbe-chromium-untrusted",
        TEST_UNTRUSTED = "rbe-chromium-untrusted-test",
    ),
    remote_jobs = struct(
        DEFAULT = 250,
        LOW_JOBS_FOR_CI = 80,
        HIGH_JOBS_FOR_CI = 500,
        LOW_JOBS_FOR_CQ = 150,
        # Calculated based on the number of CPUs inside Siso.
        HIGH_JOBS_FOR_CQ = -1,
    ),
)
