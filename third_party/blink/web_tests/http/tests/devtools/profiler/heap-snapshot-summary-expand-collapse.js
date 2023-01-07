// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(
      `https://crbug.com/738932 Tests the snapshot view is not empty on repeatitive expand-collapse.\n`);
  await TestRunner.loadTestModule('heap_profiler_test_runner');
  await TestRunner.showPanel('heap_profiler');

  var instanceCount = 25;
  function createHeapSnapshot() {
    return HeapProfilerTestRunner.createHeapSnapshot(instanceCount);
  }

  HeapProfilerTestRunner.runHeapSnapshotTestSuite([function testShowAll(next) {
    HeapProfilerTestRunner.takeAndOpenSnapshot(createHeapSnapshot, step1);

    function step1() {
      HeapProfilerTestRunner.switchToView('Summary', step2);
    }

    function step2() {
      HeapProfilerTestRunner.findAndExpandRow('A', step3);
    }

    function step3(row) {
      row.collapse();
      row.expand();
      var visibleChildren = row.children.filter(c => c.element().classList.contains('revealed'));
      TestRunner.assertEquals(11, visibleChildren.length);
      next();
    }
  }]);
})();
