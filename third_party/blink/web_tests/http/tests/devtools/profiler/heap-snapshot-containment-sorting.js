// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {HeapProfilerTestRunner} from 'heap_profiler_test_runner';

(async function() {
  TestRunner.addResult(`Tests sorting in Containment view of detailed heap snapshots.\n`);
  await TestRunner.showPanel('heap-profiler');

  var instanceCount = 25;
  function createHeapSnapshot() {
    return HeapProfilerTestRunner.createHeapSnapshot(instanceCount);
  }

  HeapProfilerTestRunner.runHeapSnapshotTestSuite([function testSorting(next) {
    HeapProfilerTestRunner.takeAndOpenSnapshot(createHeapSnapshot, step1);

    function step1() {
      HeapProfilerTestRunner.switchToView('Containment', step2);
    }

    var gcRoots;
    var columns;
    var currentColumn;
    var currentColumnOrder;

    function step2() {
      HeapProfilerTestRunner.findAndExpandGCRoots(step3);
    }

    function step3(gcRootsRow) {
      gcRoots = gcRootsRow;
      columns = HeapProfilerTestRunner.viewColumns();
      currentColumn = 0;
      currentColumnOrder = false;
      setTimeout(step4, 0);
    }

    function step4() {
      if (currentColumn >= columns.length) {
        setTimeout(next, 0);
        return;
      }

      HeapProfilerTestRunner.clickColumn(columns[currentColumn], step5);
    }

    function step5(newColumnState) {
      columns[currentColumn] = newColumnState;
      var contents = HeapProfilerTestRunner.columnContents(columns[currentColumn], gcRoots);
      TestRunner.assertEquals(true, !!contents.length, 'column contents');
      var sortTypes = {object: 'name', distance: 'number', shallowSize: 'size', retainedSize: 'size'};
      TestRunner.assertEquals(true, !!sortTypes[columns[currentColumn].id], 'sort by id');
      HeapProfilerTestRunner.checkArrayIsSorted(
          contents, sortTypes[columns[currentColumn].id], columns[currentColumn].sort);

      if (!currentColumnOrder)
        currentColumnOrder = true;
      else {
        currentColumnOrder = false;
        ++currentColumn;
      }
      setTimeout(step4, 0);
    }
  }]);
})();
