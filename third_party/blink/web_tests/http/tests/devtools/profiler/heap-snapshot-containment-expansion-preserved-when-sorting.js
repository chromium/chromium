// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {HeapProfilerTestRunner} from 'heap_profiler_test_runner';

(async function() {
  TestRunner.addResult(
      `Tests Containment view of detailed heap snapshots. Expanded nodes must be preserved after sorting.\n`);
  await TestRunner.showPanel('heap-profiler');

  var instanceCount = 25;
  function createHeapSnapshot() {
    return HeapProfilerTestRunner.createHeapSnapshot(instanceCount);
  }

  HeapProfilerTestRunner.runHeapSnapshotTestSuite([function testExpansionPreservedWhenSorting(next) {
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
      HeapProfilerTestRunner.clickShowMoreButton('showAll', buttonsNode, step5);
    }

    function step5(row) {
      var row = row.children[0];
      TestRunner.assertEquals(true, !!row, '"B" instance row');
      HeapProfilerTestRunner.expandRow(row, expandA);
      function expandA(row) {
        function propertyMatcher(data) {
          return data.referenceName === 'a' && data.name.charAt(0) === 'A';
        }
        var aRow = HeapProfilerTestRunner.findMatchingRow(propertyMatcher, row);
        TestRunner.assertEquals(true, !!aRow, '"a: A" row');
        HeapProfilerTestRunner.expandRow(aRow, step6);
      }
    }

    var columnContents;
    function step6() {
      columnContents = HeapProfilerTestRunner.columnContents(columns[0]);
      HeapProfilerTestRunner.clickColumn(columns[0], clickTwice);
      function clickTwice() {
        HeapProfilerTestRunner.clickColumn(columns[0], step7);
      }
    }

    function step7() {
      var newColumnContents = HeapProfilerTestRunner.columnContents(columns[0]);
      HeapProfilerTestRunner.assertColumnContentsEqual(columnContents, newColumnContents);
      setTimeout(next, 0);
    }
  }]);
})();
