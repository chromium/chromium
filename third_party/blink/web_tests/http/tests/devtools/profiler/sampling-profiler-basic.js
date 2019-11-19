// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests that the sampling heap profiler works and supports nesting.\n`);
  await TestRunner.loadModule('profiler');

  const profiler = SDK.targetManager.mainTarget().model(SDK.HeapProfilerModel);
  await profiler.startSampling();
  await profiler.startSampling();
  await TestRunner.evaluateInPagePromise(`let dump = new Array(5e4).fill(42.42)`);
  const profile1 = await profiler.stopSampling();
  await TestRunner.evaluateInPagePromise(`let dump2 = new Array(5e4).fill(42.42)`);
  const profile2 = await profiler.stopSampling();

  const totalSize = node => node.children.reduce((acc, c) => acc + totalSize(c), node.selfSize);
  const checkValue = (expected, value) => expected * 0.8 < value && value < expected * 1.2;
  TestRunner.addResult(`profile1 size is ok: ${checkValue(400e3, totalSize(profile1.head))}`);
  TestRunner.addResult(`profile2 size is ok: ${checkValue(800e3, totalSize(profile2.head))}`);

  try {
    await profiler.stopSampling();
  } catch (e) {
    TestRunner.addResult(`Expected error: ${e}`);
  }

  TestRunner.completeTest();
})();
