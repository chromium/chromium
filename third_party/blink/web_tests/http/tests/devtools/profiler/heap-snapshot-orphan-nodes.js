// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {HeapProfilerTestRunner} from 'heap_profiler_test_runner';

import * as ProfilerModule from 'devtools/panels/profiler/profiler.js';

(async function() {
  TestRunner.addResult(
      `Tests that weak references are ignored when dominators are calculated and that weak references won't affect object's retained size.\n`);
  await TestRunner.showPanel('heap-profiler');
  await TestRunner.loadHTML(`
      <pre></pre>
    `);

  HeapProfilerTestRunner.runHeapSnapshotTestSuite([function testWeakReferencesDoNotAffectRetainedSize(next) {
    function createHeapSnapshot() {
      // Mocking a heap snapshot with a node that retained by weak references only.
      var builder = new HeapProfilerTestRunner.HeapSnapshotBuilder();
      var rootNode = builder.rootNode;

      var gcRootsNode = new HeapProfilerTestRunner.HeapNode('(GC roots)');
      rootNode.linkNode(gcRootsNode, HeapProfilerTestRunner.HeapEdge.Type.element);

      var windowNode = new HeapProfilerTestRunner.HeapNode('Window', 10);
      rootNode.linkNode(windowNode, HeapProfilerTestRunner.HeapEdge.Type.shortcut);
      gcRootsNode.linkNode(windowNode, HeapProfilerTestRunner.HeapEdge.Type.element);

      var orphanNode = new HeapProfilerTestRunner.HeapNode('Orphan', 2000);
      windowNode.linkNode(orphanNode, HeapProfilerTestRunner.HeapEdge.Type.weak, 'weak_ref');

      var orphanChildNode = new HeapProfilerTestRunner.HeapNode('OrphanChild', 2000);
      orphanNode.linkNode(orphanChildNode, HeapProfilerTestRunner.HeapEdge.Type.property, 'child');

      var aNode = new HeapProfilerTestRunner.HeapNode('A', 300);
      windowNode.linkNode(aNode, HeapProfilerTestRunner.HeapEdge.Type.property, 'a');

      var bNode = new HeapProfilerTestRunner.HeapNode('B', 300);
      aNode.linkNode(bNode, HeapProfilerTestRunner.HeapEdge.Type.property, 'b');
      orphanChildNode.linkNode(bNode, HeapProfilerTestRunner.HeapEdge.Type.property, 'b');

      // Shortcut links should not affect retained sizes.
      rootNode.linkNode(windowNode, HeapProfilerTestRunner.HeapEdge.Type.shortcut, 'w');
      rootNode.linkNode(aNode, HeapProfilerTestRunner.HeapEdge.Type.shortcut, 'a');
      rootNode.linkNode(bNode, HeapProfilerTestRunner.HeapEdge.Type.shortcut, 'b');
      rootNode.linkNode(orphanNode, HeapProfilerTestRunner.HeapEdge.Type.shortcut, 'o');

      return builder.generateSnapshot();
    }

    TestRunner.addSniffer(ProfilerModule.HeapSnapshotView.HeapSnapshotView.prototype, 'retrieveStatistics', checkStatistics);
    HeapProfilerTestRunner.takeAndOpenSnapshot(createHeapSnapshot, step1);

    async function checkStatistics(arg, result) {
      var statistics = await result;
      TestRunner.assertEquals(4610, statistics.total);
      TestRunner.assertEquals(4610, statistics.v8heap);
      TestRunner.addResult('SUCCESS: total size is correct.');
    }

    function step1() {
      HeapProfilerTestRunner.switchToView('Summary', step2);
    }

    function step2() {
      var row = HeapProfilerTestRunner.findRow('A');
      TestRunner.assertEquals(true, !!row, '"A" row');
      TestRunner.assertEquals(1, row.count);
      TestRunner.assertEquals(300, row.retainedSize);
      TestRunner.assertEquals(300, row.shallowSize);


      row = HeapProfilerTestRunner.findRow('B');
      TestRunner.assertEquals(true, !!row, '"B" row');
      TestRunner.assertEquals(1, row.count);
      TestRunner.assertEquals(300, row.retainedSize);
      TestRunner.assertEquals(300, row.shallowSize);

      row = HeapProfilerTestRunner.findRow('Orphan');
      TestRunner.assertEquals(true, !!row, '"Orphan" row');
      TestRunner.assertEquals(1, row.count);
      TestRunner.assertEquals(4000, row.retainedSize);
      TestRunner.assertEquals(2000, row.shallowSize);


      row = HeapProfilerTestRunner.findRow('OrphanChild');
      TestRunner.assertEquals(true, !!row, '"OrphanChild" row');
      TestRunner.assertEquals(1, row.count);
      TestRunner.assertEquals(2000, row.retainedSize);
      TestRunner.assertEquals(2000, row.shallowSize);

      TestRunner.addResult('SUCCESS: all nodes have expected retained sizes.');
      setTimeout(next, 0);
    }
  }]);
})();
