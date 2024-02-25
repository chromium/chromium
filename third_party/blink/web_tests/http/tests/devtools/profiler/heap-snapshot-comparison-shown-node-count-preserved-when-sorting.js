// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {HeapProfilerTestRunner} from 'heap_profiler_test_runner';

(async function() {
  TestRunner.addResult(
      `Tests Comparison view of detailed heap snapshots. Shown node count must be preserved after sorting.\n`);
  await TestRunner.showPanel('heap-profiler');

  var instanceCount = 24;
  function createHeapSnapshotA() {
    return HeapProfilerTestRunner.createHeapSnapshot(instanceCount, 5);
  }
  function createHeapSnapshotB() {
    return HeapProfilerTestRunner.createHeapSnapshot(instanceCount + 1, 5 + instanceCount);
  }

  HeapProfilerTestRunner.runHeapSnapshotTestSuite([function testExpansionPreservedWhenSorting(next) {
    HeapProfilerTestRunner.takeAndOpenSnapshot(createHeapSnapshotA, createSnapshotB);
    function createSnapshotB() {
      HeapProfilerTestRunner.takeAndOpenSnapshot(createHeapSnapshotB, step1);
    }

    function step1() {
      HeapProfilerTestRunner.switchToView('Comparison', step2);
    }

    var columns;
    function step2() {
      columns = HeapProfilerTestRunner.viewColumns();
      HeapProfilerTestRunner.clickColumn(columns[0], step3);
    }

    function step3() {
      var row = HeapProfilerTestRunner.findRow('B');
      TestRunner.assertEquals(true, !!row, '"B" row');
      HeapProfilerTestRunner.expandRow(row, showNext);
      function showNext(row) {
        var buttonsNode = HeapProfilerTestRunner.findButtonsNode(row);
        TestRunner.assertEquals(true, !!buttonsNode, 'no buttons node found!');
        HeapProfilerTestRunner.clickShowMoreButton('showAll', buttonsNode, step4);
      }
    }

    function step4() {
      var row = HeapProfilerTestRunner.findRow('B');
      TestRunner.assertEquals(true, !!row, '"B" row');
      function deletedNodeMatcher(data) {
        return data.isDeletedNode && data.name.charAt(0) === 'B';
      }
      var bInstanceRow = HeapProfilerTestRunner.findMatchingRow(deletedNodeMatcher, row);
      TestRunner.assertEquals(true, !!bInstanceRow, '"B" instance row');
      var buttonsNode = HeapProfilerTestRunner.findButtonsNode(row, bInstanceRow);
      TestRunner.assertEquals(false, !!buttonsNode, 'buttons node found!');
      step5();
    }

    var nodeCount;
    function step5() {
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
