// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {HeapProfilerTestRunner} from 'heap_profiler_test_runner';

(async function() {
  TestRunner.addResult(`Tests showing several node ranges in the Summary view of detailed heap snapshot.\n`);
  await TestRunner.showPanel('heap-profiler');

  var instanceCount = 50;
  function createHeapSnapshot() {
    return HeapProfilerTestRunner.createHeapSnapshot(instanceCount);
  }

  HeapProfilerTestRunner.runHeapSnapshotTestSuite([function testShowAll(next) {
    HeapProfilerTestRunner.takeAndOpenSnapshot(createHeapSnapshot, step1);
    var row;

    function dumpAndPopulate(step, from, to) {
      TestRunner.addResult('');
      TestRunner.addResult(step);
      TestRunner.addResult('Retrieved ranges: ' + JSON.stringify(row.retrievedChildrenRanges));
      for (var i = 0; i < row.children.length; ++i)
        TestRunner.addResult('[' + i + '] ' + row.children[i].element().textContent.replace(/[^\w\d]/mg, ' '));
      return row.populateChildren(from, to);
    }

    function step1() {
      HeapProfilerTestRunner.switchToView('Summary', step2);
    }

    function step2() {
      row = HeapProfilerTestRunner.findRow('A');
      TestRunner.addResult('Row found: ' + !!row);
      HeapProfilerTestRunner.expandRow(row, step3);
    }

    async function step3() {
      await dumpAndPopulate('Step 3', 30, 40);
      await dumpAndPopulate('Step 4', 20, 25);
      await dumpAndPopulate('Step 5', 28, 35);
      await dumpAndPopulate('Step 6', 18, 26);
      await dumpAndPopulate('Step 7', 15, 45);
      await dumpAndPopulate('Step 8', 10, 15);
      await dumpAndPopulate('Step 9');
      setTimeout(next, 0);
    }
  }]);
})();
