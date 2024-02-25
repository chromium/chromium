// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {HeapProfilerTestRunner} from 'heap_profiler_test_runner';

(async function() {
  TestRunner.addResult(
      `Tests Comparison view of detailed heap snapshots. Repeated clicks on "Show Next" button must show all nodes.\n`);
  await TestRunner.showPanel('heap-profiler');

  var instanceCount = 24;
  function createHeapSnapshotA() {
    return HeapProfilerTestRunner.createHeapSnapshot(instanceCount, 5);
  }
  function createHeapSnapshotB() {
    return HeapProfilerTestRunner.createHeapSnapshot(instanceCount + 1, 5 + instanceCount);
  }

  HeapProfilerTestRunner.runHeapSnapshotTestSuite([function testShowNext(next) {
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

    function step3(row) {
      var expectedRowCountA = parseInt(row.data['addedCount']);
      var rowsShown = HeapProfilerTestRunner.countDataRows(row, function(node) {
        return node.data.addedCount;
      });
      TestRunner.assertEquals(
          true, rowsShown <= expectedRowCountA,
          'shown more instances than created: ' + rowsShown + ' > ' + expectedRowCountA);
      if (rowsShown < expectedRowCountA) {
        var buttonsNode = HeapProfilerTestRunner.findButtonsNode(row);
        TestRunner.assertEquals(true, !!buttonsNode, 'buttons node');
        HeapProfilerTestRunner.clickShowMoreButton('showNext', buttonsNode, step3);
      } else if (rowsShown === expectedRowCountA)
        setTimeout(step4.bind(null, row), 0);
    }

    function step4(row) {
      var expectedRowCountB = parseInt(row.data['removedCount']);
      var rowsShown = HeapProfilerTestRunner.countDataRows(row, function(node) {
        return node.data.removedCount;
      });
      TestRunner.assertEquals(
          true, rowsShown <= expectedRowCountB,
          'shown more instances than created: ' + rowsShown + ' > ' + expectedRowCountB);
      if (rowsShown < expectedRowCountB) {
        var buttonsNode = HeapProfilerTestRunner.findButtonsNode(row);
        TestRunner.assertEquals(true, !!buttonsNode, 'buttons node');
        HeapProfilerTestRunner.clickShowMoreButton('showNext', buttonsNode, step4);
      } else if (rowsShown === expectedRowCountB) {
        var buttonsNode = HeapProfilerTestRunner.findButtonsNode(row);
        TestRunner.assertEquals(false, !!buttonsNode, 'buttons node found after all rows shown');
        setTimeout(next, 0);
      }
    }
  }]);
})();
