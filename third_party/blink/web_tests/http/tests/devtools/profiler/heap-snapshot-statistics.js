// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {HeapProfilerTestRunner} from 'heap_profiler_test_runner';

import * as ProfilerModule from 'devtools/panels/profiler/profiler.js';

(async function() {
  'use strict';
  TestRunner.addResult(`Tests Statistics view of detailed heap snapshots.\n`);
  await TestRunner.showPanel('heap-profiler');

  function createHeapSnapshot() {
    var builder = new HeapProfilerTestRunner.HeapSnapshotBuilder();
    var index = 0;
    for (let type in HeapProfilerTestRunner.HeapNode.Type) {
      if (!HeapProfilerTestRunner.HeapNode.Type.hasOwnProperty(type))
        continue;
      if (type === HeapProfilerTestRunner.HeapNode.Type.synthetic)
        continue;
      if (type === HeapProfilerTestRunner.HeapNode.Type.number)
        continue;
      ++index;
      var size = index * Math.pow(10, index - 1);
      var node = new HeapProfilerTestRunner.HeapNode(type, size, HeapProfilerTestRunner.HeapNode.Type[type]);
      TestRunner.addResult(type + ' node size: ' + size);
      builder.rootNode.linkNode(node, HeapProfilerTestRunner.HeapEdge.Type.internal, type + 'Link');
    }
    var jsArrayNode = new HeapProfilerTestRunner.HeapNode('Array', 8, HeapProfilerTestRunner.HeapNode.Type.object);
    builder.rootNode.linkNode(jsArrayNode, HeapProfilerTestRunner.HeapEdge.Type.internal, 'JSArrayLink');
    var jsArrayContentsNode = new HeapProfilerTestRunner.HeapNode('', 12, HeapProfilerTestRunner.HeapNode.Type.array);
    jsArrayNode.linkNode(jsArrayContentsNode, HeapProfilerTestRunner.HeapEdge.Type.internal, 'elements');
    var gcRootsNode =
        new HeapProfilerTestRunner.HeapNode('(GC roots)', 0, HeapProfilerTestRunner.HeapNode.Type.synthetic);
    builder.rootNode.linkNode(gcRootsNode, HeapProfilerTestRunner.HeapEdge.Type.internal, '0');
    var strongRoots =
        new HeapProfilerTestRunner.HeapNode('(Strong roots)', 0, HeapProfilerTestRunner.HeapNode.Type.synthetic);
    gcRootsNode.linkNode(strongRoots, HeapProfilerTestRunner.HeapEdge.Type.internal, '0');
    var systemObj =
        new HeapProfilerTestRunner.HeapNode('SystemObject', 900000000, HeapProfilerTestRunner.HeapNode.Type.object);
    strongRoots.linkNode(systemObj, HeapProfilerTestRunner.HeapEdge.Type.internal, '0');
    return builder.generateSnapshot();
  }

  HeapProfilerTestRunner.runHeapSnapshotTestSuite([function testStatistics(next) {
    TestRunner.addSniffer(ProfilerModule.HeapSnapshotView.HeapSnapshotView.prototype, 'retrieveStatistics', step1);
    HeapProfilerTestRunner.takeAndOpenSnapshot(createHeapSnapshot, () => {});

    async function step1(arg, result) {
      var statistics = await result;
      TestRunner.addResult(JSON.stringify(statistics));
      setTimeout(next, 0);
    }
  }]);
})();
