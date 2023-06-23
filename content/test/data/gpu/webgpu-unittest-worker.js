// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import { webGpuUnitTests } from './webgpu-unittest-utils.js';

onmessage = async (e) => {
  postMessage(await webGpuUnitTests.runTest(e.data.testId));
}
