// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
(async function(testRunner) {
  const {dp} = await testRunner.startBlank(
      'Tests getDOMCountersForLeakDetection output.');

  const {result} = await dp.Memory.getDOMCountersForLeakDetection();

  for (const counter of result.counters) {
    testRunner.log(`${counter.name}=${counter.count}`);
  }

  testRunner.completeTest();
})
