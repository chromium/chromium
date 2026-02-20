<!-- Copyright 2025 The Chromium Authors
     Use of this source code is governed by a BSD-style license that can be
     found in the LICENSE file.
-->

# CBB: Information on Browsers Being Tested

JSON files in this directory contain browser information, such as their
versions. This data is utilized by CBB to select the browsers to test against,
and to generate metadata for performance test results, which are then uploaded
to Cloud Storage. These JSON files are automatically updated by the CBB new
release detector workflow. **Please do not update them manually.**

You can find the source code for CBB new release detector in the [Skia repo](
https://source.corp.google.com/h/skia/buildbot/+/main:pinpoint/go/workflows/internal/cbb_new_release_detector.go).
This workflow runs once per day on the Chrome perf Temporal server. Googlers
can monitor the workflow on the [Temporal dashboard](
https://skia-temporal-ui.corp.goog/namespaces/perf-internal/workflows?query=WorkflowType%3D%22perf.cbb_new_release_detector%22).
