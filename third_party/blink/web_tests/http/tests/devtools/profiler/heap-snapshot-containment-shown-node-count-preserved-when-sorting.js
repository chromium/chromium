// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {HeapProfilerTestRunner} from 'heap_profiler_test_runner';

(async function() {
  TestRunner.addResult(
      `Tests Containment view of detailed heap snapshots. Shown node count must be preserved after sorting.\n`);
  await TestRunner.showPanel('heap-profiler');

  var instanceCount = 25;
  function createHeapSnapshot() {
    return HeapProfilerTestRunner.createHeapSnapshot(instanceCount);
  }

  HeapProfilerTestRunner.runHeapSnapshotTestSuite([function testShownNodeCountPreservedWhenSorting(next) {
    HeapProfilerTestRunner.takeAndOpenSnapshot(createHeapSnapshot, step1);

    function step1() {
      HeapProfilerTestRunner.switchToView('Containment', step2);
    }

    var columns;
    function step2() {
      columns = HeapProfilerTestRunner.viewColumns();
      HeapProfilerTestRunner.clickColumn(columns[0], step3);
    }

    function step3() {
      HeapProfilerTestRunner.findAndExpandWindow(step4);
    }

    function step4(row) {
      var buttonsNode = HeapProfilerTestRunner.findButtonsNode(row);
      TestRunner.assertEquals(true, !!buttonsNode, 'no buttons node found!');
      HeapProfilerTestRunner.clickShowMoreButton('showNext', buttonsNode, step5);
    }

    var nodeCount;
    function step5(row) {
      // There must be enough nodes to have some unrevealed.
      var buttonsNode = HeapProfilerTestRunner.findButtonsNode(row);
      TestRunner.assertEquals(true, !!buttonsNode, 'no buttons node found!');

      nodeCount = HeapProfilerTestRunner.columnContents(columns[0]).length;
      TestRunner.assertEquals(true, nodeCount > 0, 'nodeCount > 0');

      HeapProfilerTestRunner.clickColumn(columns[0], clickTwice);
      function clickTwice() {
        HeapProfilerTestRunner.clickColumn(columns[0], step6);
      }
    }

    function step6() {
      var newNodeCount = HeapProfilerTestRunner.columnContents(columns[0]).length;
      TestRunner.assertEquals(nodeCount, newNodeCount);
      setTimeout(next, 0);
    }
  }]);
})();
