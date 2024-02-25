// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {HeapProfilerTestRunner} from 'heap_profiler_test_runner';

(async function() {
  TestRunner.addResult('Tests that sampling heap profiling works.\n');
  await TestRunner.showPanel('heap-profiler');

  HeapProfilerTestRunner.runHeapSnapshotTestSuite([async function testProfiling(next) {

    await HeapProfilerTestRunner.startSamplingHeapProfiler();
    await TestRunner.evaluateInPagePromise(`
        function pageFunction() {
          (function () {
            window.holder = [];
            // Allocate few MBs of data.
            for (var i = 0; i < 1000; ++i)
              window.holder.push(new Array(1000).fill(42));
          })();
        }
        pageFunction();`);
    HeapProfilerTestRunner.stopSamplingHeapProfiler();

    const view = await HeapProfilerTestRunner.showProfileWhenAdded('Profile 1');
    const tree = view.profileDataGridTree;
    if (!tree)
      TestRunner.addResult('no tree');
    await checkFunction(tree, 'pageFunction');
    await checkFunction(tree, '(anonymous)');
    next();

    async function checkFunction(tree, name) {
      let node = tree.children[0];
      if (!node)
        TestRunner.addResult('no node');
      while (node) {
        const element = node.element();
        // Ordering is important here, as accessing the element the first time around
        // triggers live location creation and updates which we need to await properly.
        await TestRunner.waitForPendingLiveLocationUpdates();
        const url = element.children[2].lastChild.textContent;
        if (node.functionName === name) {
          TestRunner.addResult(`found ${name} ${url}`);
          return;
        }
        node = node.traverseNextNode(true, null, true);
      }
      TestRunner.addResult(`${name} is not found.`);
    }
  }]);

})();
