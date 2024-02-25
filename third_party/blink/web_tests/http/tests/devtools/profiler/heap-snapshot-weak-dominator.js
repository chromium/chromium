// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {HeapProfilerTestRunner} from 'heap_profiler_test_runner';

(async function() {
  TestRunner.addResult(
      `Tests that weak references are ignored when dominators are calculated and that weak references won't affect object's retained size.\n`);
  await TestRunner.showPanel('heap-profiler');

  HeapProfilerTestRunner.runHeapSnapshotTestSuite([function testWeakReferencesDoNotAffectRetainedSize(next) {
    function createHeapSnapshot() {
      // Mocking results of running the following code:
      // root = [new Uint8Array(1000), new Uint8Array(1000), new Uint8Array(1000)]
      var builder = new HeapProfilerTestRunner.HeapSnapshotBuilder();
      var rootNode = builder.rootNode;

      var gcRootsNode = new HeapProfilerTestRunner.HeapNode('(GC roots)');
      rootNode.linkNode(gcRootsNode, HeapProfilerTestRunner.HeapEdge.Type.element);

      var windowNode = new HeapProfilerTestRunner.HeapNode('Window', 20);
      rootNode.linkNode(windowNode, HeapProfilerTestRunner.HeapEdge.Type.shortcut);
      gcRootsNode.linkNode(windowNode, HeapProfilerTestRunner.HeapEdge.Type.element);

      var arrayNode = new HeapProfilerTestRunner.HeapNode('Array', 10);
      windowNode.linkNode(arrayNode, HeapProfilerTestRunner.HeapEdge.Type.property, 'root');
      var prevBufferNode = null;
      for (var i = 0; i < 3; i++) {
        var typedArrayNode = new HeapProfilerTestRunner.HeapNode('Uint8Array', 100);
        arrayNode.linkNode(typedArrayNode, HeapProfilerTestRunner.HeapEdge.Type.element);

        var bufferNode = new HeapProfilerTestRunner.HeapNode('ArrayBuffer', 1000);
        typedArrayNode.linkNode(bufferNode, HeapProfilerTestRunner.HeapEdge.Type.internal);
        if (prevBufferNode)
          prevBufferNode.linkNode(bufferNode, HeapProfilerTestRunner.HeapEdge.Type.weak, 'weak_next');
        prevBufferNode = bufferNode;
      }

      return builder.generateSnapshot();
    }

    HeapProfilerTestRunner.takeAndOpenSnapshot(createHeapSnapshot, step1);

    function step1() {
      HeapProfilerTestRunner.switchToView('Summary', step2);
    }

    function step2() {
      var row = HeapProfilerTestRunner.findRow('Array');
      TestRunner.assertEquals(true, !!row, '"Array" row');
      HeapProfilerTestRunner.expandRow(row, step3);
    }

    function step3(row) {
      TestRunner.assertEquals(1, row.count);
      TestRunner.assertEquals(3310, row.retainedSize);
      TestRunner.assertEquals(10, row.shallowSize);
      HeapProfilerTestRunner.expandRow(row.children[0], step4);
    }

    function step4(arrayInstanceRow) {
      TestRunner.assertEquals(2, arrayInstanceRow.distance);
      TestRunner.assertEquals(3310, arrayInstanceRow.retainedSize);
      TestRunner.assertEquals(10, arrayInstanceRow.shallowSize);

      var children = arrayInstanceRow.children;
      TestRunner.assertEquals(3, children.length);

      for (var i = 0; i < children.length; i++) {
        TestRunner.assertEquals('Uint8Array', children[i].name);
        TestRunner.assertEquals(100, children[i].shallowSize);
        TestRunner.assertEquals(1100, children[i].retainedSize);
      }
      setTimeout(next, 0);
    }
  }]);
})();
