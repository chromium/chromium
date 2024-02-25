// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {HeapProfilerTestRunner} from 'heap_profiler_test_runner';

(async function() {
  TestRunner.addResult(`Tests Summary view of detailed heap snapshots. The "Show All" button must show all nodes.\n`);
  await TestRunner.showPanel('heap-profiler');

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
      var row = HeapProfilerTestRunner.findRow('A');
      TestRunner.assertEquals(true, !!row, '"A" row');
      HeapProfilerTestRunner.expandRow(row, step3);
    }

    function step3(row) {
      var count = row.data['count'];
      TestRunner.assertEquals(instanceCount.toString(), count);
      var buttonsNode = HeapProfilerTestRunner.findButtonsNode(row);
      TestRunner.assertEquals(true, !!buttonsNode, 'buttons node');
      var words = buttonsNode.showAll.textContent.split(' ');
      for (var i = 0; i < words.length; ++i) {
        var maybeNumber = parseInt(words[i], 10);
        if (!isNaN(maybeNumber))
          TestRunner.assertEquals(
              instanceCount - row.dataGrid.defaultPopulateCount(), maybeNumber, buttonsNode.showAll.textContent);
      }
      HeapProfilerTestRunner.clickShowMoreButton('showAll', buttonsNode, step4);
    }

    function step4(row) {
      var rowsShown = HeapProfilerTestRunner.countDataRows(row);
      TestRunner.assertEquals(instanceCount, rowsShown, 'after showAll click');
      var buttonsNode = HeapProfilerTestRunner.findButtonsNode(row);
      TestRunner.assertEquals(false, !!buttonsNode, 'buttons node found when all instances are shown!');
      setTimeout(next, 0);
    }
  }]);
})();
