// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {HeapProfilerTestRunner} from 'heap_profiler_test_runner';

(async function() {
  TestRunner.addResult(`Tests search in Summary view of detailed heap snapshots.\n`);
  await TestRunner.showPanel('heap-profiler');

  var instanceCount = 25;
  function createHeapSnapshot() {
    return HeapProfilerTestRunner.createHeapSnapshot(instanceCount);
  }

  HeapProfilerTestRunner.runHeapSnapshotTestSuite([function testSearch(next) {
    HeapProfilerTestRunner.takeAndOpenSnapshot(createHeapSnapshot, step1a);
    function addSearchResultSniffer(step) {
      function jumpToSearchResult() {
        step(HeapProfilerTestRunner.currentProfileView().searchResults.length);
      }
      TestRunner.addSniffer(HeapProfilerTestRunner.currentProfileView(), 'jumpToSearchResult', jumpToSearchResult);
    }

    function addNodeSelectedSniffer(callback) {
      TestRunner.addSniffer(HeapProfilerTestRunner.currentProfileView(), 'selectRevealedNode', callback);
    }

    function step1a() {
      HeapProfilerTestRunner.switchToView('Summary', step1b);
    }

    function step1b() {
      var row = HeapProfilerTestRunner.findRow('Window');
      TestRunner.assertEquals(true, !!row, '"Window" class row');
      HeapProfilerTestRunner.expandRow(row, step1c);
    }

    function step1c(row) {
      TestRunner.assertEquals(1, row.children.length, 'single Window object');
      var windowRow = row.children[0];
      TestRunner.assertEquals(true, !!windowRow, '"Window" instance row');
      HeapProfilerTestRunner.expandRow(windowRow, step2);
    }

    function step2() {
      addSearchResultSniffer(step3);
      HeapProfilerTestRunner.currentProfileView().performSearch({query: 'window', caseSensitive: false});
    }

    function step3(resultCount) {
      TestRunner.assertEquals(1, resultCount, 'Search for existing node');
      addSearchResultSniffer(step4);
      HeapProfilerTestRunner.currentProfileView().performSearch({query: 'foo', caseSensitive: false});
    }

    function step4(resultCount) {
      TestRunner.assertEquals(0, resultCount, 'Search for not-existing node');
      addNodeSelectedSniffer(step5);
      HeapProfilerTestRunner.currentProfileView().performSearch({query: '@999', caseSensitive: false});
    }

    function step5(node) {
      TestRunner.assertEquals(false, !!node, 'Search for not-existing node by id');
      addNodeSelectedSniffer(step6);
      HeapProfilerTestRunner.currentProfileView().performSearch({query: '@83', caseSensitive: false});
    }

    function step6(node) {
      TestRunner.assertEquals(true, !!node, 'Search for existing node by id');
      next();
    }
  }]);
})();
