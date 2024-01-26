// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {HeapProfilerTestRunner} from 'heap_profiler_test_runner';

(async function() {
  TestRunner.addResult(
      `Tests Containment view of detailed heap snapshots. The "Show All" button must show all nodes. Test object distances calculation.\n`);
  await TestRunner.showPanel('heap-profiler');

  var instanceCount = 25;
  function createHeapSnapshot() {
    return HeapProfilerTestRunner.createHeapSnapshot(instanceCount);
  }

  HeapProfilerTestRunner.runHeapSnapshotTestSuite([
    function testShowAll(next) {
      HeapProfilerTestRunner.takeAndOpenSnapshot(createHeapSnapshot, step1);

      function step1() {
        HeapProfilerTestRunner.switchToView('Containment', step2);
      }

      function step2() {
        HeapProfilerTestRunner.findAndExpandWindow(step3);
      }

      function step3(row) {
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
    },

    function testDistances(next) {
      function createHeapSnapshot() {
        // Mocking results of running the following code:
        //
        // function Tail() {}
        // next = new Tail();
        // for (var i = 0; i < 5; ++i)
        //   next = { next: next };

        var builder = new HeapProfilerTestRunner.HeapSnapshotBuilder();
        var rootNode = builder.rootNode;

        var gcRootsNode =
            new HeapProfilerTestRunner.HeapNode('(GC roots)', 0, HeapProfilerTestRunner.HeapNode.Type.synthetic);
        rootNode.linkNode(gcRootsNode, HeapProfilerTestRunner.HeapEdge.Type.element);

        var globalHandlesNode = new HeapProfilerTestRunner.HeapNode('(Global Handles)');
        gcRootsNode.linkNode(globalHandlesNode, HeapProfilerTestRunner.HeapEdge.Type.element);

        var nativeContextNode = new HeapProfilerTestRunner.HeapNode('system / NativeContext', 32);
        globalHandlesNode.linkNode(nativeContextNode, HeapProfilerTestRunner.HeapEdge.Type.element);

        var windowNode = new HeapProfilerTestRunner.HeapNode('Window', 20);
        rootNode.linkNode(windowNode, HeapProfilerTestRunner.HeapEdge.Type.shortcut);
        nativeContextNode.linkNode(windowNode, HeapProfilerTestRunner.HeapEdge.Type.element);
        windowNode.linkNode(nativeContextNode, HeapProfilerTestRunner.HeapEdge.Type.internal, 'native_context');

        var headNode = new HeapProfilerTestRunner.HeapNode('Body', 32);
        windowNode.linkNode(headNode, HeapProfilerTestRunner.HeapEdge.Type.property, 'next');
        for (var i = 1; i < 5; ++i) {
          var nextNode = new HeapProfilerTestRunner.HeapNode('Body', 32);
          headNode.linkNode(nextNode, HeapProfilerTestRunner.HeapEdge.Type.property, 'next');
          headNode = nextNode;
        }
        var tailNode = new HeapProfilerTestRunner.HeapNode('Tail', 32);
        headNode.linkNode(tailNode, HeapProfilerTestRunner.HeapEdge.Type.property, 'next');
        return builder.generateSnapshot();
      }

      var distance = 1;
      HeapProfilerTestRunner.takeAndOpenSnapshot(createHeapSnapshot, step1);

      function step1() {
        HeapProfilerTestRunner.switchToView('Containment', step2);
      }

      function step2() {
        HeapProfilerTestRunner.findAndExpandGCRoots(step3);
      }

      function step3(row) {
        TestRunner.assertEquals('(GC roots)', row.name, '(GC roots) object');
        TestRunner.assertEquals('\u2212', row.data.distance, '(GC roots) distance should be zero');
        HeapProfilerTestRunner.findAndExpandWindow(step4);
      }

      function step4(row) {
        TestRunner.assertEquals('Window', row.name, 'Window object');
        TestRunner.assertEquals(distance, row.distance, 'Window distance should be 1');
        var child = HeapProfilerTestRunner.findMatchingRow(function(obj) {
          return obj.referenceName === 'next';
        }, row);
        TestRunner.assertEquals(true, !!child, 'next found');
        HeapProfilerTestRunner.expandRow(child, step5);
      }

      function step5(row) {
        ++distance;
        TestRunner.assertEquals(distance, row.distance, 'Check distance of objects chain');
        if (row.name === 'Tail') {
          TestRunner.assertEquals(7, distance, 'Tail distance');
          setTimeout(next, 0);
          return;
        }
        TestRunner.assertEquals('Body', row.name, 'Body');
        var child = HeapProfilerTestRunner.findMatchingRow(function(obj) {
          return obj.referenceName === 'next';
        }, row);
        TestRunner.assertEquals(true, !!child, 'next found');
        if (child.name !== 'Tail')
          HeapProfilerTestRunner.expandRow(child, step5);
        else
          step5(child);
      }
    }
  ]);
})();
