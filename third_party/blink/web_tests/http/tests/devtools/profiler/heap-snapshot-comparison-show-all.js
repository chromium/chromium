// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {HeapProfilerTestRunner} from 'heap_profiler_test_runner';

(async function() {
  TestRunner.addResult(
      `Tests Comparison view of detailed heap snapshots. The "Show All" button must show all nodes.\n`);
  await TestRunner.showPanel('heap-profiler');

  var instanceCount = 24;
  var firstId = 100;
  function createHeapSnapshotA() {
    return HeapProfilerTestRunner.createHeapSnapshot(instanceCount, firstId);
  }
  function createHeapSnapshotB() {
    return HeapProfilerTestRunner.createHeapSnapshot(instanceCount + 1, firstId + instanceCount * 2);
  }

  HeapProfilerTestRunner.runHeapSnapshotTestSuite([function testShowAll(next) {
    HeapProfilerTestRunner.takeAndOpenSnapshot(createHeapSnapshotA, createSnapshotB);
    function createSnapshotB() {
      HeapProfilerTestRunner.takeAndOpenSnapshot(createHeapSnapshotB, step1);
    }

    function step1() {
      HeapProfilerTestRunner.switchToView('Comparison', step2);
    }

    function step2() {
      var row = HeapProfilerTestRunner.findRow('A');
      TestRunner.assertEquals(true, !!row, '"A" row');
      HeapProfilerTestRunner.expandRow(row, step3);
    }

    var countA;
    var countB;
    function step3(row) {
      countA = row.addedCount;
      TestRunner.assertEquals(true, countA > 0, 'countA > 0');
      countB = row.removedCount;
      TestRunner.assertEquals(true, countB > 0, 'countB > 0');

      var buttonsNode = HeapProfilerTestRunner.findButtonsNode(row);
      TestRunner.assertEquals(true, !!buttonsNode, 'buttons node (added)');
      var words = buttonsNode.showAll.textContent.split(' ');
      for (var i = 0; i < words.length; ++i) {
        var maybeNumber = parseInt(words[i], 10);
        if (!isNaN(maybeNumber))
          TestRunner.assertEquals(
              countA + countB - row.dataGrid.defaultPopulateCount(), maybeNumber, buttonsNode.showAll.textContent);
      }
      HeapProfilerTestRunner.clickShowMoreButton('showAll', buttonsNode, step4);
    }

    function step4(row) {
      var rowsShown = HeapProfilerTestRunner.countDataRows(row, function(node) {
        return node.data.addedCount;
      });
      TestRunner.assertEquals(countA, rowsShown, 'after showAll click 1');

      countB = row.removedCount;
      TestRunner.assertEquals(true, countB > 0, 'countB > 0');
      var buttonsNode = HeapProfilerTestRunner.findButtonsNode(row);
      TestRunner.assertEquals(false, !!buttonsNode, 'buttons node (deleted)');

      var deletedRowsShown = HeapProfilerTestRunner.countDataRows(row, function(node) {
        return node.data.removedCount;
      });
      TestRunner.assertEquals(countB, deletedRowsShown, 'deleted rows shown');
      setTimeout(next, 0);
    }
  }]);
})();
