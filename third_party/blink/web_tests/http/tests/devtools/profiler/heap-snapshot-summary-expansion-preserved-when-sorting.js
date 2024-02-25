// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {HeapProfilerTestRunner} from 'heap_profiler_test_runner';

(async function() {
  TestRunner.addResult(
      `Tests Summary view of detailed heap snapshots. Expanded nodes must be preserved after sorting.\n`);
  await TestRunner.showPanel('heap-profiler');

  var instanceCount = 25;
  function createHeapSnapshot() {
    return HeapProfilerTestRunner.createHeapSnapshot(instanceCount);
  }

  HeapProfilerTestRunner.runHeapSnapshotTestSuite([function testExpansionPreservedWhenSorting(next) {
    HeapProfilerTestRunner.takeAndOpenSnapshot(createHeapSnapshot, step1);

    function step1() {
      HeapProfilerTestRunner.switchToView('Summary', step2);
    }

    function step2() {
      var row = HeapProfilerTestRunner.findRow('B');
      TestRunner.assertEquals(true, !!row, '"B" row');
      HeapProfilerTestRunner.expandRow(row, expandB);
      function expandB() {
        var buttonsNode = HeapProfilerTestRunner.findButtonsNode(row);
        TestRunner.assertEquals(true, !!buttonsNode, 'no buttons node found!');
        HeapProfilerTestRunner.clickShowMoreButton('showAll', buttonsNode, step3);
      }
    }

    var columns;
    function step3() {
      columns = HeapProfilerTestRunner.viewColumns();
      HeapProfilerTestRunner.clickColumn(columns[0], step4);
    }

    function step4() {
      var row = HeapProfilerTestRunner.findRow('B');
      TestRunner.assertEquals(true, !!row, '"B" row');
      var bInstanceRow = row.children[0];
      TestRunner.assertEquals(true, !!bInstanceRow, '"B" instance row');
      HeapProfilerTestRunner.expandRow(bInstanceRow, expandA);
      function expandA(row) {
        function propertyMatcher(data) {
          return data.referenceName === 'a' && data.name.charAt(0) === 'A';
        }
        var aRow = HeapProfilerTestRunner.findMatchingRow(propertyMatcher, row);
        TestRunner.assertEquals(true, !!aRow, '"a: A" row');
        HeapProfilerTestRunner.expandRow(aRow, step5);
      }
    }

    var columnContents;
    function step5() {
      columnContents = HeapProfilerTestRunner.columnContents(columns[0]);
      HeapProfilerTestRunner.clickColumn(columns[0], clickTwice);
      function clickTwice() {
        HeapProfilerTestRunner.clickColumn(columns[0], step6);
      }
    }

    function step6() {
      var newColumnContents = HeapProfilerTestRunner.columnContents(columns[0]);
      HeapProfilerTestRunner.assertColumnContentsEqual(columnContents, newColumnContents);
      setTimeout(next, 0);
    }
  }]);
})();
