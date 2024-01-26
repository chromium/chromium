// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {HeapProfilerTestRunner} from 'heap_profiler_test_runner';

(async function() {
  TestRunner.addResult(
      `Tests Containment view of detailed heap snapshots. Repeated clicks on "Show Next" button must show all nodes.\n`);
  await TestRunner.showPanel('heap-profiler');

  var instanceCount = 25;
  function createHeapSnapshot() {
    return HeapProfilerTestRunner.createHeapSnapshot(instanceCount);
  }

  HeapProfilerTestRunner.runHeapSnapshotTestSuite([function testShowNext(next) {
    HeapProfilerTestRunner.takeAndOpenSnapshot(createHeapSnapshot, step1);

    function step1() {
      HeapProfilerTestRunner.switchToView('Containment', step2);
    }

    function step2() {
      HeapProfilerTestRunner.findAndExpandWindow(step3);
    }

    function step3(row) {
      var rowsShown = HeapProfilerTestRunner.countDataRows(row);
      TestRunner.assertEquals(
          true, rowsShown <= instanceCount, 'shown more instances than created: ' + rowsShown + ' > ' + instanceCount);
      if (rowsShown < instanceCount) {
        var buttonsNode = HeapProfilerTestRunner.findButtonsNode(row);
        TestRunner.assertEquals(true, !!buttonsNode, 'buttons node');
        HeapProfilerTestRunner.clickShowMoreButton('showNext', buttonsNode, step3);
      } else if (rowsShown === instanceCount) {
        var buttonsNode = HeapProfilerTestRunner.findButtonsNode(row);
        TestRunner.assertEquals(false, !!buttonsNode, 'buttons node found when all instances are shown!');
        setTimeout(next, 0);
      }
    }
  }]);
})();
