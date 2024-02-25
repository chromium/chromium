// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {HeapProfilerTestRunner} from 'heap_profiler_test_runner';

(async function() {
  TestRunner.addResult(
      `Tests Comparison view of detailed heap snapshots. Expanded nodes must be preserved after sorting.\n`);
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

    function step2() {
      var row = HeapProfilerTestRunner.findRow('B');
      TestRunner.assertEquals(true, !!row, '"B" row');
      HeapProfilerTestRunner.expandRow(row, expandB);
      function expandB() {
        var buttonsNode = HeapProfilerTestRunner.findButtonsNode(row);
        TestRunner.assertEquals(true, !!buttonsNode, 'no buttons node found!');
        HeapProfilerTestRunner.clickShowMoreButton('showAll', buttonsNode, step4);
      }
    }

    var columns;
    function step4() {
      columns = HeapProfilerTestRunner.viewColumns();
      HeapProfilerTestRunner.clickColumn(columns[0], step5);
    }

    function step5() {
      var row = HeapProfilerTestRunner.findRow('B');
      TestRunner.assertEquals(true, !!row, '"B" row');
      var bInstanceRow = row.children[0];
      TestRunner.assertEquals(true, !!bInstanceRow, '"B" instance row');
      HeapProfilerTestRunner.expandRow(bInstanceRow, expandA);
      function expandA(row) {
        function propertyMatcher(node) {
          return node.referenceName === 'a' && node.name.charAt(0) === 'A';
        }
        var aRow = HeapProfilerTestRunner.findMatchingRow(propertyMatcher, row);
        TestRunner.assertEquals(true, !!aRow, '"a: A" row');
        HeapProfilerTestRunner.expandRow(aRow, step6);
      }
    }

    function step6() {
      var row = HeapProfilerTestRunner.findRow('B');
      TestRunner.assertEquals(true, !!row, '"B" row');
      function deletedNodeMatcher(data) {
        return data.isDeletedNode && data.name.charAt(0) === 'B';
      }
      var bInstanceRow = HeapProfilerTestRunner.findMatchingRow(deletedNodeMatcher, row);
      TestRunner.assertEquals(true, !!bInstanceRow, '"B" instance row');
      HeapProfilerTestRunner.expandRow(bInstanceRow, expandA);
      function expandA(row) {
        function propertyMatcher(data) {
          return data.referenceName === 'a' && data.name.charAt(0) === 'A';
        }
        var aRow = HeapProfilerTestRunner.findMatchingRow(propertyMatcher, row);
        TestRunner.assertEquals(true, !!aRow, '"a: A" row');
        HeapProfilerTestRunner.expandRow(aRow, step7);
      }
    }

    var columnContents;
    function step7() {
      columnContents = HeapProfilerTestRunner.columnContents(columns[0]);
      HeapProfilerTestRunner.clickColumn(columns[0], clickTwice);
      function clickTwice() {
        HeapProfilerTestRunner.clickColumn(columns[0], step8);
      }
    }

    function step8() {
      var newColumnContents = HeapProfilerTestRunner.columnContents(columns[0]);
      HeapProfilerTestRunner.assertColumnContentsEqual(columnContents, newColumnContents);
      setTimeout(next, 0);
    }
  }]);
})();
