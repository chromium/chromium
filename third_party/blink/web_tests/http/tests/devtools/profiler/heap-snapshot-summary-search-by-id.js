// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {HeapProfilerTestRunner} from 'heap_profiler_test_runner';

(async function() {
  TestRunner.addResult(`Tests search in Summary view of detailed heap snapshots.\n`);
  await TestRunner.showPanel('heap-profiler');

  var instanceCount = 200;
  function createHeapSnapshot() {
    return HeapProfilerTestRunner.createHeapSnapshot(instanceCount, 100);
  }

  HeapProfilerTestRunner.runHeapSnapshotTestSuite([function testSearch(next) {
    HeapProfilerTestRunner.takeAndOpenSnapshot(createHeapSnapshot, step1);

    function step0() {
      HeapProfilerTestRunner.switchToView('Summary', step1);
    }

    var view;
    function addNodeSelectSniffer(constructorName, nodeId, onSuccess) {
      TestRunner.addSniffer(
          view, 'selectRevealedNode', checkNodeIsSelected.bind(view, constructorName, nodeId, onSuccess));
    }

    function step1() {
      view = HeapProfilerTestRunner.currentProfileView();
      TestRunner.addSniffer(view, 'selectRevealedNode', step2);
      view.performSearch({query: '@1', caseSensitive: false});
    }

    function step2(callback, node) {
      if (node) {
        TestRunner.addResult('FAIL: hidden node @1 was found');
        return next();
      }
      TestRunner.addResult('PASS: hidden node @1 was not found.');
      view = HeapProfilerTestRunner.currentProfileView();
      addNodeSelectSniffer('A', '101', step3);
      view.performSearch({query: '@101', caseSensitive: false});
    }

    function step3() {
      view.performSearch({query: '@a', caseSensitive: false});
      if (view.searchResults.length !== 0) {
        TestRunner.addResult('FAIL: node @a found');
        return next();
      }
      TestRunner.addResult('PASS: node @a was not found');
      TestRunner.addSniffer(view, 'selectRevealedNode', step5);
      view.performSearch({query: '@999', caseSensitive: false}, step5);
    }

    function step5(node) {
      if (node) {
        TestRunner.addResult('FAIL: found node @999');
        return next();
      }
      TestRunner.addResult('PASS: node @999 was not found');
      addNodeSelectSniffer('B', '100', step6);
      view.performSearch({query: '@100', caseSensitive: false});
    }

    function step6() {
      addNodeSelectSniffer('B', '400', step7);
      view.performSearch({query: '@400', caseSensitive: false});
    }

    function step7() {
      addNodeSelectSniffer('A', '401', next);
      view.performSearch({query: '@401', caseSensitive: false});
    }

    function checkNodeIsSelected(constructorName, nodeId, onSuccess, node) {
      if (!node) {
        TestRunner.addResult('FAIL: node @' + nodeId + ' not found');
        return next();
      }
      if (constructorName !== node.parent.name) {
        TestRunner.addResult('FAIL: constructor name doesn\'t match. ' + constructorName + ' !== ' + node.parent.name);
        next();
      }
      if (nodeId != node.snapshotNodeId) {
        TestRunner.addResult('FAIL: snapshot node id doesn\'t match. ' + nodeId + ' !== ' + node.snapshotNodeId);
        next();
      }
      TestRunner.addResult('PASS: found node @' + nodeId + ' with class \'' + constructorName + '\'');
      return onSuccess();
    }
  }]);
})();
