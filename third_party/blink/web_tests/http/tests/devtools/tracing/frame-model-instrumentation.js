// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  await TestRunner.loadLegacyModule('timeline'); await TestRunner.loadTestModule('performance_test_runner');
  await TestRunner.showPanel('timeline');
  await TestRunner.evaluateInPagePromise(`
  function doActions() {
    return generateFrames(3);
  }`);

  UI.panels.timeline.captureLayersAndPicturesSetting.set(true);
  await PerformanceTestRunner.invokeAsyncWithTimeline('doActions');
  const frames = PerformanceTestRunner.timelineFrameModel().getFrames();
  const lastFrame = frames[frames.length - 1];
  if (lastFrame) {
    TestRunner.addResult('layerTree: ' + typeof lastFrame.layerTree);
    TestRunner.addResult('mainFrameId: ' + typeof lastFrame.mainFrameId);
    const paints = lastFrame.layerTree.paints();
    TestRunner.addResult('paints: ' + (paints && paints.length ? 'present' : 'absent'));
  } else {
    TestRunner.addResult('FAIL: there was no frame');
  }
  TestRunner.completeTest();
})();
